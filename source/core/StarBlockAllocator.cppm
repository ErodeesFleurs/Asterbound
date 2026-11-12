module;

import std;
import star.format;
import star.type_identity;

namespace Star {

// Keep incomplete node types valid during allocator rebinding.
template <typename T>
concept allocator_value = std::is_object_v<T> && !std::is_array_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T>;

template <std::size_t Size>
concept allocator_block_size = Size > 0;

// Copies and rebinds share a non-thread-safe family, with one pool per type.
// Only single-element requests use the pool; arrays use std::allocator.
template <allocator_value T, std::size_t BlockSize>
  requires allocator_block_size<BlockSize>
class BlockAllocator {
public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using propagate_on_container_copy_assignment = std::false_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;

  template <allocator_value U> struct rebind {
    using other = BlockAllocator<U, BlockSize>;
  };

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
  BlockAllocator(BlockAllocator<U, BlockSize> const& other) noexcept : m_family(other.get_family()) {}

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
    if constexpr (DebugEnabled)
      star_assert_failed(address);
    auto position = std::upper_bound(data.blocks.begin(), data.blocks.end(), address,
                                     [](value_type* allocated, std::unique_ptr<block> const& owner) {
                                       return std::less<void const*>{}(allocated, std::addressof(owner->at(0)));
                                     });
    if constexpr (DebugEnabled)
      star_assert_failed(position != data.blocks.begin());
    --position;
    auto& owner = **position;
    // Slots contain storage, not an array of T; use their byte stride.
    auto offset =
      reinterpret_cast<std::uintptr_t>(address) - reinterpret_cast<std::uintptr_t>(std::addressof(owner.at(0)));
    if constexpr (DebugEnabled)
      star_assert_failed(offset < sizeof(owner.slots) && offset % sizeof(slot) == 0);
    owner.deallocate(static_cast<index_type>(offset / sizeof(slot)));

    if (!data.available)
      data.available = std::ref(owner);
    else if (owner.empty() && std::addressof(data.available->get()) != std::addressof(owner))
      data.blocks.erase(position);
  }

  template <allocator_value U> bool operator==(BlockAllocator<U, BlockSize> const& other) const noexcept {
    return get_family() == other.get_family();
  }

private:
  template <allocator_value U, std::size_t Size>
    requires allocator_block_size<Size>
  friend class BlockAllocator;

  using index_type =
    std::conditional_t<BlockSize <= std::numeric_limits<std::uint8_t>::max(), std::uint8_t,
                       std::conditional_t<BlockSize <= std::numeric_limits<std::uint16_t>::max(), std::uint16_t,
                                          std::conditional_t<BlockSize <= std::numeric_limits<std::uint32_t>::max(),
                                                             std::uint32_t, std::uintmax_t>>>;
  static constexpr index_type null_index = std::numeric_limits<index_type>::max();

  struct free_node {
    index_type next;
  };

  struct alignas(value_type) alignas(free_node) slot {
    std::array<std::byte, std::max(sizeof(value_type), sizeof(free_node))> bytes;

    free_node& free_entry() { return *std::launder(reinterpret_cast<free_node*>(bytes.data())); }
  };

  struct block {
    index_type allocate() {
      if constexpr (DebugEnabled)
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
      if constexpr (DebugEnabled)
        star_assert_failed(!empty());
      std::construct_at(reinterpret_cast<free_node*>(std::addressof(at(index))), free_node{first_free});
      first_free = index;
      --allocated;
    }

    bool full() const { return allocated == BlockSize; }
    bool empty() const { return allocated == 0; }
    slot& at(index_type index) {
      if constexpr (DebugEnabled)
        star_assert_failed(index < BlockSize);
      return slots.data()[index];
    }

    std::array<slot, BlockSize> slots;
    index_type first_free = null_index;
    index_type allocated = 0;
  };

  struct pool_data {
    std::vector<std::unique_ptr<block>> blocks;
    std::optional<std::reference_wrapper<block>> available;
  };
  using family_type = std::unordered_map<TypeKey, std::shared_ptr<void>>;

  [[gnu::noinline]] void find_available(pool_data& data) {
    auto found = std::ranges::find_if(data.blocks, [](std::unique_ptr<block> const& owner) { return !owner->full(); });
    if (found != data.blocks.end()) {
      data.available = std::ref(**found);
      return;
    }
    auto owner = std::make_unique_for_overwrite<block>();
    auto& added = *owner;
    auto position = std::lower_bound(
      data.blocks.begin(), data.blocks.end(), std::ref(added),
      [](std::unique_ptr<block> const& existing, std::reference_wrapper<block> incoming) {
        return std::less<void const*>{}(std::addressof(existing->at(0)), std::addressof(incoming.get().at(0)));
      });
    data.blocks.insert(position, std::move(owner));
    data.available = std::ref(added);
  }

  pool_data& get_data() {
    if (!m_data)
      initialize_data();
    return m_data->get();
  }
  [[gnu::noinline]] void initialize_data() {
    auto& owner = (*get_family())[typeKey<pool_data>()];
    if (!owner)
      owner = std::make_shared<pool_data>();
    // The family owns this type-erased state; the cached handle only borrows.
    m_data = std::ref(*static_cast<pool_data*>(owner.get()));
  }

  std::shared_ptr<family_type> const& get_family() const noexcept {
    if (!m_family)
      initialize_family();
    return m_family;
  }
  [[gnu::noinline]] void initialize_family() const noexcept { m_family = std::make_shared<family_type>(); }
  [[gnu::noinline]] void inherit_data(BlockAllocator const& other) noexcept {
    auto found = m_family->find(typeKey<pool_data>());
    if (found != m_family->end())
      m_data = other.m_data = std::ref(*static_cast<pool_data*>(found->second.get()));
  }

  mutable std::shared_ptr<family_type> m_family;
  mutable std::optional<std::reference_wrapper<pool_data>> m_data;
};

}// namespace Star

export module star.block_allocator;

export namespace Star {
using ::Star::allocator_block_size;
using ::Star::allocator_value;
using ::Star::BlockAllocator;
}// namespace Star
