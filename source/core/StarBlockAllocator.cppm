module;

import std;
import star.format;
import star.type_identity;

namespace Star {

// Keep incomplete node types valid during allocator rebinding.
template <typename T>
concept allocator_value = std::is_object_v<T> && !std::is_array_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T>;

template <std::size_t size>
concept allocator_block_size = size > 0;

namespace block_allocator_detail {
struct PoolBase {};
}

// Copies and rebinds share a non-thread-safe family, with one pool per type.
// Only single-element requests use the pool; arrays use std::allocator.
template <allocator_value T, std::size_t block_size>
  requires allocator_block_size<block_size>
class BlockAllocator {
public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using propagate_on_container_copy_assignment = std::false_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;

  template <allocator_value U> struct Rebind {
    using other = BlockAllocator<U, block_size>;
  };
  template <allocator_value U> using rebind = Rebind<U>;

  BlockAllocator() : m_family(std::make_shared<family_type>()) {}
  BlockAllocator(BlockAllocator const& other) noexcept : m_family(other.get_family()), m_data(other.m_data) {
    if (!m_data)
      inherit_data(other);
  }
  BlockAllocator& operator=(BlockAllocator const& other) noexcept {
    if (this != &other)
      *this = BlockAllocator(other);
    return *this;
  }

  // Reset the borrowed cache too; a moved source lazily obtains a fresh family.
  BlockAllocator(BlockAllocator&& other) noexcept
      : m_family(std::move(other.m_family)), m_data(std::exchange(other.m_data, std::nullopt)) {}
  BlockAllocator& operator=(BlockAllocator&& other) noexcept {
    if (this != &other) {
      m_family = std::move(other.m_family);
      m_data = std::exchange(other.m_data, std::nullopt);
    }
    return *this;
  }

  template <allocator_value U>
  BlockAllocator(BlockAllocator<U, block_size> const& other) noexcept : m_family(other.get_family()) {}

  [[nodiscard]] value_type* allocate(size_type count) {
    if (count != 1)
      return std::allocator<value_type>{}.allocate(count);

    auto& data = get_data();
    if (!data.available)
      find_available(data);

    auto& selected = data.available->get();
    auto index = selected.allocate();
    if (selected.full())
      data.available.reset();
    return reinterpret_cast<value_type*>(std::addressof(selected.at(index)));
  }

  void deallocate(value_type* address, size_type count) {
    if (count != 1) {
      std::allocator<value_type>{}.deallocate(address, count);
      return;
    }

    auto& data = get_data();
    if constexpr (debug_enabled)
      star_assert_failed(address);
    auto position = std::upper_bound(data.blocks.begin(), data.blocks.end(), address,
                                     [](value_type* allocated, std::unique_ptr<Block> const& owner) {
                                       return std::less<void const*>{}(allocated, std::addressof(owner->at(0)));
                                     });
    if constexpr (debug_enabled)
      star_assert_failed(position != data.blocks.begin());
    --position;
    auto& owner = **position;
    // Slots contain storage, not an array of T; use their byte stride.
    auto offset =
      reinterpret_cast<std::uintptr_t>(address) - reinterpret_cast<std::uintptr_t>(std::addressof(owner.at(0)));
    if constexpr (debug_enabled)
      star_assert_failed(offset < sizeof(owner.slots) && offset % sizeof(Slot) == 0);
    owner.deallocate(static_cast<index_type>(offset / sizeof(Slot)));

    if (!data.available)
      data.available = std::ref(owner);
    else if (owner.empty() && std::addressof(data.available->get()) != std::addressof(owner))
      data.blocks.erase(position);
  }

  template <allocator_value U> bool operator==(BlockAllocator<U, block_size> const& other) const noexcept {
    return get_family() == other.get_family();
  }

private:
  template <allocator_value U, std::size_t size>
    requires allocator_block_size<size>
  friend class BlockAllocator;

  using index_type =
    std::conditional_t<block_size <= std::numeric_limits<std::uint8_t>::max(), std::uint8_t,
                       std::conditional_t<block_size <= std::numeric_limits<std::uint16_t>::max(), std::uint16_t,
                                          std::conditional_t<block_size <= std::numeric_limits<std::uint32_t>::max(),
                                                             std::uint32_t, std::uintmax_t>>>;
  static constexpr index_type null_index = std::numeric_limits<index_type>::max();

  struct FreeNode {
    index_type next;
  };

  struct alignas(value_type) alignas(FreeNode) Slot {
    std::array<std::byte, std::max(sizeof(value_type), sizeof(FreeNode))> bytes;

    FreeNode& free_entry() { return *std::launder(reinterpret_cast<FreeNode*>(bytes.data())); }
  };

  struct Block {
    index_type allocate() {
      if constexpr (debug_enabled)
        star_assert_failed(!full());
      auto index = first_free == null_index ? allocated : first_free;
      if (first_free != null_index) {
        auto& entry = at(index).free_entry();
        first_free = entry.next;
        std::destroy_at(std::addressof(entry));
      }
      ++allocated;
      return index;
    }

    void deallocate(index_type index) {
      if constexpr (debug_enabled)
        star_assert_failed(!empty());
      std::construct_at(reinterpret_cast<FreeNode*>(std::addressof(at(index))), FreeNode{first_free});
      first_free = index;
      --allocated;
    }

    bool full() const { return allocated == block_size; }
    bool empty() const { return allocated == 0; }
    Slot& at(index_type index) {
      if constexpr (debug_enabled)
        star_assert_failed(index < block_size);
      return slots.data()[index];
    }

    std::array<Slot, block_size> slots;
    index_type first_free = null_index;
    index_type allocated = 0;
  };

  struct PoolData : block_allocator_detail::PoolBase {
    std::vector<std::unique_ptr<Block>> blocks;
    std::optional<std::reference_wrapper<Block>> available;
  };
  using family_type = std::unordered_map<TypeKey, std::shared_ptr<block_allocator_detail::PoolBase>, TypeKey::Hash>;

  [[gnu::noinline]] void find_available(PoolData& data) {
    auto found = std::ranges::find_if(data.blocks, [](std::unique_ptr<Block> const& owner) { return !owner->full(); });
    if (found != data.blocks.end()) {
      data.available = std::ref(**found);
      return;
    }
    auto owner = std::make_unique_for_overwrite<Block>();
    auto& added = *owner;
    auto position = std::lower_bound(
      data.blocks.begin(), data.blocks.end(), std::ref(added),
      [](std::unique_ptr<Block> const& existing, std::reference_wrapper<Block> incoming) {
        return std::less<void const*>{}(std::addressof(existing->at(0)), std::addressof(incoming.get().at(0)));
      });
    data.blocks.insert(position, std::move(owner));
    data.available = std::ref(added);
  }

  PoolData& get_data() {
    if (!m_data)
      initialize_data();
    return m_data->get();
  }
  [[gnu::noinline]] void initialize_data() {
    auto& owner = (*get_family())[type_key<PoolData>()];
    if (!owner)
      owner = std::make_shared<PoolData>();
    // The family owns this type-erased state; the cached handle only borrows.
    m_data = std::ref(static_cast<PoolData&>(*owner));
  }

  std::shared_ptr<family_type> const& get_family() const noexcept {
    if (!m_family)
      initialize_family();
    return m_family;
  }
  [[gnu::noinline]] void initialize_family() const noexcept { m_family = std::make_shared<family_type>(); }
  [[gnu::noinline]] void inherit_data(BlockAllocator const& other) noexcept {
    auto found = m_family->find(type_key<PoolData>());
    if (found != m_family->end())
      m_data = other.m_data = std::ref(static_cast<PoolData&>(*found->second));
  }

  mutable std::shared_ptr<family_type> m_family;
  mutable std::optional<std::reference_wrapper<PoolData>> m_data;
};

}// namespace Star

export module star.block_allocator;

export namespace Star {
using ::Star::allocator_block_size;
using ::Star::allocator_value;
using ::Star::BlockAllocator;
}// namespace Star
