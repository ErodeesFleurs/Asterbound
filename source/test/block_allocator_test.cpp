import star.block_allocator;
import std;
import star.map;

#include "gtest/gtest.h"

using namespace Star;

TEST(BlockAllocatorTest, All) {
  HashMap<int, int, std::hash<int>, std::equal_to<int>, BlockAllocator<std::pair<int const, int>, 4096>> testMap;
  testMap.reserve(32768);
  for (std::size_t i = 0; i < 65536; ++i)
    testMap[i] = i;

  for (auto const& p : testMap)
    EXPECT_EQ(p.first, p.second);
}

namespace {
template <typename T>
concept block_test_value = allocator_value<T> && requires(unsigned char value, T const& object) {
  T{value};
  { object.value } -> std::convertible_to<unsigned char>;
};

template <block_test_value T, std::size_t BlockSize = 4>
  requires allocator_block_size<BlockSize>
void exercise_block_reuse() {
  using allocator_type = BlockAllocator<T, BlockSize>;
  using traits = std::allocator_traits<allocator_type>;
  allocator_type allocator;
  auto shared_allocator = allocator;
  auto release = [&shared_allocator](T* allocation) {
    traits::destroy(shared_allocator, allocation);
    traits::deallocate(shared_allocator, allocation, 1);
  };
  using owner = std::unique_ptr<T, decltype(release)>;
  std::vector<owner> values;
  auto count = std::max(std::size_t(160), BlockSize + 2);
  values.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    owner allocation(traits::allocate(allocator, 1), release);
    traits::construct(allocator, allocation.get(), T{static_cast<unsigned char>(i)});
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(allocation.get()) % alignof(T), 0u);
    values.push_back(std::move(allocation));
  }
  for (std::size_t i = 1; i < count; i += 2)
    values[i].reset();
  for (std::size_t i = 1; i < count; i += 2) {
    owner allocation(traits::allocate(allocator, 1), release);
    traits::construct(allocator, allocation.get(), T{static_cast<unsigned char>(i)});
    values[i].reset(allocation.release());
  }
  for (std::size_t i = 0; i < count; ++i)
    EXPECT_EQ(values[i]->value, static_cast<unsigned char>(i));

  std::vector<std::size_t> order(count);
  std::iota(order.begin(), order.end(), 0);
  std::uint32_t random = 0x31415926;
  for (std::size_t i = count; i > 1; --i) {
    random ^= random << 13;
    random ^= random >> 17;
    random ^= random << 5;
    std::swap(order[i - 1], order[random % i]);
  }
  for (auto index : order)
    values[index].reset();
  owner reused(traits::allocate(allocator, 1), release);
  traits::construct(allocator, reused.get(), T{42});
  EXPECT_EQ(reused->value, 42);
}
}// namespace

TEST(BlockAllocatorTest, ChunkStrideAndBlockLookup) {
  struct Tiny {
    unsigned char value;
  };
  struct Odd {
    unsigned char value;
    std::array<unsigned char, 2> padding{};
  };
  struct alignas(64) Aligned {
    unsigned char value;
  };
  exercise_block_reuse<Tiny>();
  exercise_block_reuse<Odd>();
  exercise_block_reuse<Aligned>();
  exercise_block_reuse<Tiny, 255>();
  exercise_block_reuse<Tiny, 256>();
}

TEST(BlockAllocatorTest, MultipleElementFallback) {
  using allocator_type = BlockAllocator<int, 4>;
  using traits = std::allocator_traits<allocator_type>;
  allocator_type allocator;
  auto release = [&allocator](int* allocation) {
    std::destroy_n(allocation, 3);
    traits::deallocate(allocator, allocation, 3);
  };
  std::unique_ptr<int[], decltype(release)> elements(traits::allocate(allocator, 3), release);
  for (int i = 0; i < 3; ++i)
    traits::construct(allocator, elements.get() + i, i + 10);
  for (int i = 0; i < 3; ++i)
    EXPECT_EQ(elements[i], i + 10);
}

TEST(BlockAllocatorTest, RebindingPreservesFamilyAndInterchangeableDeallocation) {
  using allocator_type = BlockAllocator<int, 4>;
  using traits = std::allocator_traits<allocator_type>;
  allocator_type original;
  auto copied_before_allocation = original;
  BlockAllocator<std::byte, 4> rebound(original);
  allocator_type round_trip(rebound);
  allocator_type separate;
  EXPECT_EQ(original, copied_before_allocation);
  EXPECT_EQ(original, round_trip);
  EXPECT_NE(original, separate);
  auto release = [&round_trip](int* allocation) {
    traits::destroy(round_trip, allocation);
    traits::deallocate(round_trip, allocation, 1);
  };
  std::unique_ptr<int, decltype(release)> value(traits::allocate(copied_before_allocation, 1), release);
  traits::construct(original, value.get(), 73);
  EXPECT_EQ(*value, 73);
  auto address = reinterpret_cast<std::uintptr_t>(value.get());
  value.reset();
  value.reset(traits::allocate(original, 1));
  traits::construct(round_trip, value.get(), 91);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(value.get()), address);
  EXPECT_EQ(*value, 91);
}

TEST(BlockAllocatorTest, MovedListSourceRemainsUsableAfterDestinationDestruction) {
  using list_type = std::list<int, BlockAllocator<int, 4>>;
  list_type source{1, 2, 3, 4, 5, 6};
  {
    list_type destination(std::move(source));
    EXPECT_TRUE(source.empty());
    source.push_back(71);
    EXPECT_EQ(std::vector<int>(destination.begin(), destination.end()), (std::vector<int>{1, 2, 3, 4, 5, 6}));
  }
  source.push_back(72);
  EXPECT_EQ(std::vector<int>(source.begin(), source.end()), (std::vector<int>{71, 72}));
  source.clear();
  source.push_back(73);
  EXPECT_EQ(source.front(), 73);
}

TEST(BlockAllocatorTest, MoveAssignmentAndSwapPreserveAllocationOwnership) {
  using list_type = std::list<int, BlockAllocator<int, 4>>;
  list_type source{1, 2, 3, 4, 5};
  {
    list_type destination{91, 92};
    destination = std::move(source);
    EXPECT_TRUE(source.empty());
    source.push_back(61);
    list_type separate{81, 82};
    destination.swap(separate);
    EXPECT_EQ(std::vector<int>(destination.begin(), destination.end()), (std::vector<int>{81, 82}));
    EXPECT_EQ(std::vector<int>(separate.begin(), separate.end()), (std::vector<int>{1, 2, 3, 4, 5}));
  }
  source.push_back(62);
  EXPECT_EQ(std::vector<int>(source.begin(), source.end()), (std::vector<int>{61, 62}));
}

TEST(BlockAllocatorTest, CopiedMovedSourceSharesFreshFamilyBeforeAllocation) {
  using allocator_type = BlockAllocator<int, 4>;
  using traits = std::allocator_traits<allocator_type>;
  allocator_type source;
  {
    allocator_type destination(std::move(source));
    auto release = [&destination](int* allocation) {
      traits::destroy(destination, allocation);
      traits::deallocate(destination, allocation, 1);
    };
    std::unique_ptr<int, decltype(release)> value(traits::allocate(destination, 1), release);
    traits::construct(destination, value.get(), 13);
    EXPECT_EQ(*value, 13);
  }
  allocator_type copied(source);
  EXPECT_EQ(source, copied);
  auto release = [&source](int* allocation) {
    traits::destroy(source, allocation);
    traits::deallocate(source, allocation, 1);
  };
  std::unique_ptr<int, decltype(release)> value(traits::allocate(copied, 1), release);
  traits::construct(copied, value.get(), 97);
  EXPECT_EQ(*value, 97);
  value.reset();
  value.reset(traits::allocate(source, 1));
  traits::construct(copied, value.get(), 101);
  EXPECT_EQ(*value, 101);
}
