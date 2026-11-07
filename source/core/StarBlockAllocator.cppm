module;

import std;
import star.format;
import star.type_identity;

namespace Star {

// Constant size only allocator using fixed size blocks of memory.  much faster
// than general purpose allocators, but not thread safe.  Useful as the
// allocator for containers that mostly allocate one element at a time, such as
// std::list, std::map, std::set etc.
template <typename T, std::size_t BlockSize>
class BlockAllocator {
  static_assert(BlockSize > 0);
public:
  using value_type = T;

  using pointer = T*;
  using const_pointer = T const*;

  using reference = T&;
  using const_reference = T const&;

  // Allocator can be shared, but since it is NOT thread safe this should not
  // be done by default.
  using propagate_on_container_copy_assignment = std::false_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;

  template <class U>
  struct rebind {
    using other = BlockAllocator<U, BlockSize>;
  };

  BlockAllocator();
  // Copy constructed BlockAllocators of the same type share underlying
  // resources.
  BlockAllocator(BlockAllocator const& other) = default;
  BlockAllocator(BlockAllocator&& other) = default;
  // Copy constructed BlockAllocators of different type share no resources
  template <class U>
  BlockAllocator(BlockAllocator<U, BlockSize> const& other);

  BlockAllocator& operator=(BlockAllocator const& rhs) = default;
  BlockAllocator& operator=(BlockAllocator&& rhs) = default;

  // If n is != 1, will fall back on std::allocator<T>
  T* allocate(std::size_t n);
  void deallocate(T* p, std::size_t n);

  template <typename... Args>
  void construct(pointer p, Args&&... args) const;
  void destroy(pointer p) const;

  // BlockAllocator will always be != to any other BlockAllocator instance
  template <class U>
  bool operator==(BlockAllocator<U, BlockSize> const& rhs) const;
  template <class U>
  bool operator!=(BlockAllocator<U, BlockSize> const& rhs) const;

private:
  template <typename OtherT, std::size_t OtherBlockSize>
  friend class BlockAllocator;

  using ChunkIndex =
    std::conditional_t<BlockSize <= std::numeric_limits<std::uint8_t>::max(), std::uint8_t,
      std::conditional_t<BlockSize <= std::numeric_limits<std::uint16_t>::max(), std::uint16_t,
        std::conditional_t<BlockSize <= std::numeric_limits<std::uint32_t>::max(), std::uint32_t,
          std::conditional_t<BlockSize <= std::numeric_limits<std::uint64_t>::max(), std::uint64_t, std::uintmax_t>>>>;

  static ChunkIndex const NullChunkIndex = std::numeric_limits<ChunkIndex>::max();

  struct Unallocated {
    ChunkIndex prev;
    ChunkIndex next;
  };

  struct alignas(T) alignas(Unallocated) Chunk {
    std::byte bytes[std::max(sizeof(T), sizeof(Unallocated))];

    Unallocated* unallocated() {
      return std::launder(reinterpret_cast<Unallocated*>(bytes));
    }
  };

  struct Block {
    T* allocate();
    void deallocate(T* ptr);

    bool full() const;
    bool empty() const;

    Chunk* chunkPointer(ChunkIndex chunkIndex);

    std::array<Chunk, BlockSize> chunks;
    ChunkIndex firstUnallocated = NullChunkIndex;
    ChunkIndex allocationCount = 0;
  };

  struct Data {
    std::vector<std::unique_ptr<Block>> blocks;
    Block* unfilledBlock;
    std::allocator<T> multiAllocator;
  };

  using BlockAllocatorFamily = std::unordered_map<TypeKey, std::shared_ptr<void>>;

  static Data* getAllocatorData(BlockAllocatorFamily& family);

  std::shared_ptr<BlockAllocatorFamily> m_family;
  Data* m_data;
};

template <typename T, std::size_t BlockSize>
BlockAllocator<T, BlockSize>::BlockAllocator() {
  m_family = std::make_shared<BlockAllocatorFamily>();
  m_data = getAllocatorData(*m_family);
  m_data->blocks.reserve(32);
  m_data->unfilledBlock = nullptr;
}

template <typename T, std::size_t BlockSize>
template <class U>
BlockAllocator<T, BlockSize>::BlockAllocator(BlockAllocator<U, BlockSize> const& other)
  : m_family(other.m_family) {
  m_data = getAllocatorData(*m_family);
}

template <typename T, std::size_t BlockSize>
T* BlockAllocator<T, BlockSize>::allocate(std::size_t n) {
  if (n == 1) {
    if (m_data->unfilledBlock == nullptr) {
      for (auto const& p : m_data->blocks) {
        if (!p->full()) {
          m_data->unfilledBlock = p.get();
          break;
        }
      }

      if (!m_data->unfilledBlock) {
        auto block = std::make_unique<Block>();
        auto sortedPosition = std::lower_bound(m_data->blocks.begin(), m_data->blocks.end(), block.get(), [](std::unique_ptr<Block> const& a, Block* b) {
            return std::less<void const*>{}(a->chunkPointer(0), b->chunkPointer(0));
          });
        auto inserted = m_data->blocks.insert(sortedPosition, std::move(block));
        m_data->unfilledBlock = inserted->get();
      }
    }

    auto allocated = m_data->unfilledBlock->allocate();
    if (m_data->unfilledBlock->full())
      m_data->unfilledBlock = nullptr;
    return allocated;
  } else {
    return m_data->multiAllocator.allocate(n);
  }
}

template <typename T, std::size_t BlockSize>
void BlockAllocator<T, BlockSize>::deallocate(T* p, std::size_t n) {
  if (n == 1) {
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((p));
    }

    auto i = std::upper_bound(m_data->blocks.begin(), m_data->blocks.end(), p, [](T* a, std::unique_ptr<Block> const& b) {
        return std::less<void const*>{}(a, b->chunkPointer(0));
      });

    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((i != m_data->blocks.begin()));
    }
    --i;

    (*i)->deallocate(p);

    if (!m_data->unfilledBlock) {
      m_data->unfilledBlock = i->get();
    } else if ((*i)->empty()) {
      if (m_data->unfilledBlock != i->get())
        m_data->blocks.erase(i);
    }
  } else {
    m_data->multiAllocator.deallocate(p, n);
  }
}

template <typename T, std::size_t BlockSize>
template <typename... Args>
void BlockAllocator<T, BlockSize>::construct(pointer p, Args&&... args) const {
  std::construct_at(p, std::forward<Args>(args)...);
}

template <typename T, std::size_t BlockSize>
void BlockAllocator<T, BlockSize>::destroy(pointer p) const {
  std::destroy_at(p);
}

template <typename T, std::size_t BlockSize>
template <class U>
bool BlockAllocator<T, BlockSize>::operator==(BlockAllocator<U, BlockSize> const& rhs) const {
  return m_family == rhs.m_family;
}

template <typename T, std::size_t BlockSize>
template <class U>
bool BlockAllocator<T, BlockSize>::operator!=(BlockAllocator<U, BlockSize> const& rhs) const {
  return m_family != rhs.m_family;
}

template <typename T, std::size_t BlockSize>
T* BlockAllocator<T, BlockSize>::Block::allocate() {
  if constexpr (Star::DebugEnabled) {
    Star::starAssertFailed((allocationCount < BlockSize));
  }

  T* allocated;
  if (firstUnallocated == NullChunkIndex) {
    allocated = reinterpret_cast<T*>(chunkPointer(allocationCount));
  } else {
    auto chunk = chunkPointer(firstUnallocated);
    auto unallocated = chunk->unallocated();
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((unallocated->prev == NullChunkIndex));
    }
    firstUnallocated = unallocated->next;
    std::destroy_at(unallocated);
    if (firstUnallocated != NullChunkIndex)
      chunkPointer(firstUnallocated)->unallocated()->prev = NullChunkIndex;
    allocated = reinterpret_cast<T*>(chunk);
  }

  ++allocationCount;
  return allocated;
}

template <typename T, std::size_t BlockSize>
void BlockAllocator<T, BlockSize>::Block::deallocate(T* ptr) {
  if constexpr (Star::DebugEnabled) {
    Star::starAssertFailed((allocationCount > 0));
  }

  // The stride is sizeof(Chunk), not sizeof(T), and the T objects are not an
  // array.  Use byte addresses rather than subtracting unrelated T pointers.
  auto offset = reinterpret_cast<std::uintptr_t>(ptr) - reinterpret_cast<std::uintptr_t>(chunks.data());
  if constexpr (Star::DebugEnabled) {
    Star::starAssertFailed((offset < sizeof(chunks) && offset % sizeof(Chunk) == 0));
  }
  auto chunkIndex = static_cast<ChunkIndex>(offset / sizeof(Chunk));
  std::construct_at(reinterpret_cast<Unallocated*>(chunkPointer(chunkIndex)),
    Unallocated{NullChunkIndex, firstUnallocated});
  if (firstUnallocated != NullChunkIndex)
    chunkPointer(firstUnallocated)->unallocated()->prev = chunkIndex;
  firstUnallocated = chunkIndex;
  --allocationCount;
}

template <typename T, std::size_t BlockSize>
bool BlockAllocator<T, BlockSize>::Block::full() const {
  return allocationCount == BlockSize;
}

template <typename T, std::size_t BlockSize>
bool BlockAllocator<T, BlockSize>::Block::empty() const {
  return allocationCount == 0;
}

template <typename T, std::size_t BlockSize>
auto BlockAllocator<T, BlockSize>::Block::chunkPointer(ChunkIndex chunkIndex) -> Chunk* {
  if constexpr (Star::DebugEnabled) {
    Star::starAssertFailed((chunkIndex < BlockSize));
  }
  return &chunks[chunkIndex];
}

template <typename T, std::size_t BlockSize>
typename BlockAllocator<T, BlockSize>::Data* BlockAllocator<T, BlockSize>::getAllocatorData(BlockAllocatorFamily& family) {
  // Keep each complete Data specialization distinct within the shared family.
  auto& dataptr = family[typeKey<Data>()];
  if (!dataptr)
    dataptr = std::make_shared<Data>();
  return static_cast<Data*>(dataptr.get());
}

}

export module star.block_allocator;

export namespace Star {
  using ::Star::BlockAllocator;
}
