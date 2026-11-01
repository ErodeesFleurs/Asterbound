import std;
import star.memory;

#include "gtest/gtest.h"

TEST(MemoryTest, FailedReallocationPreservesOwnedBytes) {
  constexpr std::size_t size = 513;
  auto data = static_cast<unsigned char*>(Star::malloc(size));
  ASSERT_NE(data, nullptr);
  for (std::size_t i = 0; i < size; ++i)
    data[i] = static_cast<unsigned char>(i);

  auto replacement = Star::realloc(data, std::numeric_limits<std::size_t>::max());
  EXPECT_EQ(replacement, nullptr);
  if (replacement) {
    Star::free(replacement);
    return;
  }
  for (std::size_t i = 0; i < size; ++i)
    EXPECT_EQ(data[i], static_cast<unsigned char>(i));
  Star::free(data, size);
}

TEST(MemoryTest, FirstAllocationOnFreshThreadCanShrinkTransferredBytes) {
  constexpr std::size_t size = 1024;
  constexpr std::size_t resizedSize = 768;
  auto data = static_cast<unsigned char*>(Star::malloc(size));
  ASSERT_NE(data, nullptr);
  for (std::size_t i = 0; i < size; ++i)
    data[i] = static_cast<unsigned char>(i);

  unsigned char* resized = nullptr;
  std::thread consumer([&] {
    resized = static_cast<unsigned char*>(Star::realloc(data, resizedSize));
  });
  consumer.join();
  EXPECT_NE(resized, nullptr);
  if (!resized) {
    Star::free(data);
    return;
  }
  for (std::size_t i = 0; i < resizedSize; ++i)
    EXPECT_EQ(resized[i], static_cast<unsigned char>(i));
  Star::free(resized);
}

TEST(MemoryTest, LargeAlignedObjectsTransferAcrossThreads) {
  // Keep type alignment within the Windows x64 ABI limit; test larger allocations below.
  struct alignas(8192) Value {
    std::array<unsigned char, 257> bytes{};
  };
  auto values = std::make_unique<Value[]>(2);
  for (std::size_t i = 0; i < values[0].bytes.size(); ++i) {
    values[0].bytes[i] = static_cast<unsigned char>(i);
    values[1].bytes[i] = static_cast<unsigned char>(i + 17);
  }
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(values.get()) % alignof(Value), 0u);

  bool intact = true;
  std::thread consumer([owned = std::move(values), &intact] {
    for (std::size_t i = 0; i < owned[0].bytes.size(); ++i) {
      intact &= owned[0].bytes[i] == static_cast<unsigned char>(i);
      intact &= owned[1].bytes[i] == static_cast<unsigned char>(i + 17);
    }
  });
  consumer.join();
  EXPECT_TRUE(intact);
}

TEST(MemoryTest, LargeDynamicAlignmentTransfersAcrossThreads) {
  constexpr std::size_t alignment = 131072;
  constexpr std::size_t payloadSize = 257;
  auto release = [](unsigned char* ptr) { ::operator delete[](ptr, std::align_val_t{alignment}); };
  std::unique_ptr<unsigned char[], decltype(release)> bytes(
      static_cast<unsigned char*>(::operator new[](2 * alignment, std::align_val_t{alignment})), release);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(bytes.get()) % alignment, 0u);
  for (std::size_t i = 0; i < payloadSize; ++i) {
    bytes[i] = static_cast<unsigned char>(i);
    bytes[alignment + i] = static_cast<unsigned char>(i + 17);
  }

  bool intact = true;
  std::thread consumer([owned = std::move(bytes), &intact] {
    for (std::size_t i = 0; i < payloadSize; ++i) {
      intact &= owned[i] == static_cast<unsigned char>(i);
      intact &= owned[alignment + i] == static_cast<unsigned char>(i + 17);
    }
  });
  consumer.join();
  EXPECT_TRUE(intact);
}

TEST(MemoryTest, ImpossibleNothrowAllocationsReturnNull) {
  auto size = std::numeric_limits<std::size_t>::max();
  auto plain = ::operator new(size, std::nothrow);
  auto aligned = ::operator new(size, std::align_val_t{128}, std::nothrow);
  EXPECT_EQ(plain, nullptr);
  EXPECT_EQ(aligned, nullptr);
  ::operator delete(plain, std::nothrow);
  ::operator delete(aligned, std::align_val_t{128}, std::nothrow);
}

#if GTEST_HAS_DEATH_TEST
TEST(MemoryTest, ImpossibleThrowingAllocationsTerminate) {
  auto oldStyle = testing::GTEST_FLAG(death_test_style);
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
  EXPECT_EXIT(
    {
      std::set_terminate([] { std::_Exit(73); });
      auto ptr = ::operator new(std::numeric_limits<std::size_t>::max());
      ::operator delete(ptr);
      std::_Exit(1);
    },
    testing::ExitedWithCode(73), "");
  EXPECT_EXIT(
    {
      std::set_terminate([] { std::_Exit(73); });
      auto ptr = ::operator new(std::numeric_limits<std::size_t>::max(), std::align_val_t{128});
      ::operator delete(ptr, std::align_val_t{128});
      std::_Exit(1);
    },
    testing::ExitedWithCode(73), "");
  testing::GTEST_FLAG(death_test_style) = oldStyle;
}
#endif
