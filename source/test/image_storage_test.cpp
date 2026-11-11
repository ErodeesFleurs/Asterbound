import std;
import star.image;
import star.image_scaling;
import star.image_processing;
import star.buffer;
import star.logging;
import star.rect;
import star.vector;
import star.format;

#include "gtest/gtest.h"
#include <climits>

using namespace Star;

namespace {
Image patterned_image(Vec2U size, PixelFormat format) {
  Image image(size, format);
  for (std::size_t i = 0; i < image.data().size(); ++i)
    image.data()[i] = static_cast<std::uint8_t>(i * 37 + 19);
  return image;
}
}// namespace

TEST(ImageStorageTest, LayoutUsesFullWidthArithmeticAndRejectsUnaddressableStorage) {
  auto large = Image::size_bytes({65536, 65536}, PixelFormat::RGBA32);
  if constexpr (sizeof(std::ptrdiff_t) > 4) {
    ASSERT_TRUE(large);
    EXPECT_EQ(*large, 17179869184ULL);
  } else {
    EXPECT_FALSE(large);
  }
  auto invalid = Image::size_bytes({UINT_MAX, UINT_MAX}, PixelFormat::RGBA_F);
  ASSERT_FALSE(invalid);
  EXPECT_TRUE(invalid.error().is<ImageException>());
  EXPECT_DEATH((Image(Vec2U(UINT_MAX, UINT_MAX), PixelFormat::RGBA_F)), "");
  Image empty(Vec2U(0, UINT_MAX), PixelFormat::RGBA_F);
  EXPECT_TRUE(empty.empty());
  EXPECT_EQ(empty.size(), Vec2U(0, UINT_MAX));
  EXPECT_TRUE(empty.data().empty());
}

TEST(ImageStorageTest, RectangleClippingDoesNotWrapUnsignedEndpoints) {
  Image image = Image::filled({4, 3}, {11, 22, 33, 44});
  ASSERT_TRUE(image.fill_rect({1, 0}, {UINT_MAX, UINT_MAX}, Vec4B(7, 8, 9, 10)));
  for (unsigned y = 0; y < 3; ++y)
    for (unsigned x = 0; x < 4; ++x)
      EXPECT_EQ(image.get32(x, y), x == 0 ? Vec4B(11, 22, 33, 44) : Vec4B(7, 8, 9, 10));
  auto invalid = image.sub_image({UINT_MAX, 0}, {2, 1});
  ASSERT_FALSE(invalid);
  EXPECT_TRUE(invalid.error().is<ImageException>());
  auto empty = image.sub_image({4, 1}, {0, 2});
  ASSERT_TRUE(empty);
  EXPECT_EQ(empty->size(), Vec2U(0, 2));
  EXPECT_EQ(empty->pixel_format(), image.pixel_format());
}

TEST(ImageStorageTest, CropsAndClippedCopiesPreserveRawChannelsAcrossByteFormats) {
  // One case per 3/4-byte width path, including raw BGR ordering.
  constexpr std::array formats{
    std::pair{PixelFormat::RGB24, PixelFormat::BGR24}, std::pair{PixelFormat::BGR24, PixelFormat::RGBA32},
    std::pair{PixelFormat::RGBA32, PixelFormat::RGB24}, std::pair{PixelFormat::BGRA32, PixelFormat::RGBA32}};
  for (auto [source_format, target_format] : formats) {
    Image source = patterned_image({5, 3}, source_format);
    auto original = std::vector<std::uint8_t>(source.data().begin(), source.data().end());
    unsigned from = source.bytes_per_pixel();
    auto cropped = source.sub_image({2, 1}, {3, 2});
    ASSERT_TRUE(cropped);
    EXPECT_EQ(cropped->pixel_format(), source_format);
    for (unsigned y = 0; y < 2; ++y)
      for (unsigned x = 0; x < 3; ++x)
        for (unsigned c = 0; c < from; ++c)
          EXPECT_EQ(cropped->data()[(y * 3 + x) * from + c], original[((y + 1) * 5 + x + 2) * from + c]);
    Image target = Image::filled({4, 3}, {91, 92, 93, 94}, target_format);
    auto expected = std::vector<std::uint8_t>(target.data().begin(), target.data().end());
    unsigned to = target.bytes_per_pixel();
    for (unsigned y = 0; y < 2; ++y)
      for (unsigned x = 0; x < 3; ++x)
        for (unsigned c = 0; c < to; ++c)
          expected[((y + 1) * 4 + x + 1) * to + c] = c < from ? original[(y * 5 + x) * from + c] : 255;
    ASSERT_TRUE(target.copy_into({1, 1}, source));
    EXPECT_TRUE(std::ranges::equal(target.data(), expected));
    Image converted = source.convert(target_format);
    EXPECT_EQ(converted.pixel_format(), target_format);
    for (std::size_t p = 0; p < 15; ++p)
      for (unsigned c = 0; c < to; ++c)
        EXPECT_EQ(converted.data()[p * to + c], c < from ? original[p * from + c] : 255);
    EXPECT_TRUE(std::ranges::equal(source.data(), original));
  }
}

TEST(ImageStorageTest, AliasedPasteAndSourceOverRetainForwardFeedback) {
  Image paste(3, 1);
  for (unsigned x = 0; x < 3; ++x)
    paste.set32(x, 0, {static_cast<std::uint8_t>(x + 1), 0, 0, 255});
  ASSERT_TRUE(paste.copy_into({1, 0}, paste));
  EXPECT_EQ(paste.get32(1, 0), Vec4B(1, 0, 0, 255));
  EXPECT_EQ(paste.get32(2, 0), Vec4B(1, 0, 0, 255));
  Image vertical = patterned_image({2, 3}, PixelFormat::RGB24);
  std::array<std::uint8_t, 6> first_row;
  std::copy_n(vertical.data().begin(), first_row.size(), first_row.begin());
  ASSERT_TRUE(vertical.copy_into({0, 1}, vertical));
  for (unsigned y = 1; y < 3; ++y)
    EXPECT_TRUE(std::ranges::equal(vertical.data().subspan(y * first_row.size(), first_row.size()), first_row));
  Image draw(3, 1);
  draw.set32(0, 0, {50, 0, 0, 128});
  draw.set32(1, 0, {100, 0, 0, 64});
  draw.set32(2, 0, {150, 0, 0, 255});
  ASSERT_TRUE(draw.draw_into({1, 0}, draw));
  EXPECT_EQ(draw.get32(1, 0), Vec4B(37, 0, 0, 159));
  EXPECT_EQ(draw.get32(2, 0), Vec4B(79, 0, 0, 255));
  Image hidden = Image::filled({1, 1}, {90, 80, 70, 0});
  ASSERT_TRUE(hidden.draw_into({0, 0}, hidden));
  EXPECT_EQ(hidden.get32(0, 0), Vec4B(0, 0, 0, 0));
}

TEST(ImageStorageTest, ResetAndFloatOwnershipPreserveTheirStorageContracts) {
  Image image = patterned_image({3, 2}, PixelFormat::RGB24);
  Image snapshot = image;
  image.reset({3, 2});
  EXPECT_TRUE(std::ranges::equal(image.data(), snapshot.data()));
  image.reset({2, 3});
  EXPECT_TRUE(std::ranges::all_of(image.data(), [](auto byte) { return byte == 0; }));
  Image floating({2, 1}, PixelFormat::RGBA_F);
  constexpr std::array<std::uint32_t, 8> bits{0x7fc00019, 0x80000000, 0x3f800000, 0,
                                              0xffc00123, 1,          0x7f800000, 0xff800000};
  std::memcpy(floating.data().data(), bits.data(), sizeof(bits));
  Image copy = floating;
  Image assigned;
  assigned = floating;
  floating.data()[0] ^= 0xff;
  EXPECT_EQ(std::memcmp(copy.data().data(), bits.data(), sizeof(bits)), 0);
  EXPECT_EQ(std::memcmp(assigned.data().data(), bits.data(), sizeof(bits)), 0);
}

TEST(ImageStorageTest, BorrowedMoveOnlyCallbacksCommitOnlySuccessfulPixels) {
  Image image = Image::filled({3, 1}, {1, 2, 3, 4});
  auto callback = [count = std::make_unique<unsigned>(0)](unsigned, unsigned,
                                                          Vec4B& pixel) mutable -> std::expected<void, StarException> {
    pixel = {9, 8, 7, 6};
    if (++*count == 2)
      return std::unexpected(ImageException("injected pixel failure"));
    return {};
  };
  static_assert(pixel_writer<decltype(callback)>);
  static_assert(!pixel_reader<decltype(callback)>);
  static_assert(!pixel_result<int>);
  auto result = image.for_each_pixel(callback);
  ASSERT_FALSE(result);
  EXPECT_TRUE(result.error().is<ImageException>());
  EXPECT_EQ(image.get32(0, 0), Vec4B(9, 8, 7, 6));
  EXPECT_EQ(image.get32(1, 0), Vec4B(1, 2, 3, 4));
  EXPECT_EQ(image.get32(2, 0), Vec4B(1, 2, 3, 4));
  Image const& reader = image;
  unsigned visits = 0;
  auto read = reader.for_each_pixel([&](unsigned, unsigned, Vec4B const&) -> std::expected<void, StarException> {
    if (++visits == 2)
      return std::unexpected(ImageException("injected reader failure"));
    return {};
  });
  EXPECT_FALSE(read);
  EXPECT_EQ(visits, 2u);
  EXPECT_EQ(image.get32(1, 0), Vec4B(1, 2, 3, 4));
}

TEST(ImageStorageTest, ScalingRejectsInvalidExtentsButAllowsSmallWindowsOfHugeOutputs) {
  Image source = Image::filled({2, 2}, {12, 34, 56, 78});
  for (auto scale : {Vec2F(-1, 1), Vec2F(std::numeric_limits<float>::infinity(), 1),
                     Vec2F(std::numeric_limits<float>::quiet_NaN(), 1), Vec2F(1e30f, 1)}) {
    EXPECT_FALSE(scale_nearest(source, scale));
    EXPECT_FALSE(scale_bilinear(source, scale));
    EXPECT_FALSE(scale_bicubic(source, scale));
  }
  auto scale = Vec2F(1e9f, 1e9f);
  RectI window({0, 0}, {2, 2});
  ASSERT_TRUE(scaled_crop_fits(source, scale, window));
  auto nearest = scale_nearest_cropped(source, scale, window);
  auto linear = scale_bilinear_cropped(source, scale, window);
  ASSERT_TRUE(nearest);
  ASSERT_TRUE(linear);
  EXPECT_EQ(nearest->size(), Vec2U(2, 2));
  EXPECT_EQ(linear->size(), Vec2U(2, 2));
  EXPECT_EQ(nearest->get32(1, 1), source.get32(0, 0));
  EXPECT_EQ(linear->get32(0, 0), source.get32(0, 0));
}

TEST(ImageStorageTest, OverflowingBorderReportsAnOperationErrorWithoutChangingPixels) {
  Image source = Image::filled({2, 2}, {12, 34, 56, 78});
  auto before = std::vector<std::uint8_t>(source.data().begin(), source.data().end());
  auto result =
    process_image_operation(BorderImageOperation{UINT_MAX, {1, 2, 3, 4}, {5, 6, 7, 8}, false, true}, source);
  ASSERT_FALSE(result);
  EXPECT_TRUE(result.error().is<ImageOperationException>());
  EXPECT_EQ(source.size(), Vec2U(2, 2));
  EXPECT_TRUE(std::ranges::equal(source.data(), before));
}

TEST(ImageStorageTest, PngUsesThreeRawChannelsForBgrStorageAndRejectsEmptyImages) {
  Logger logger;
  logger.remove_stdout_sink();
  Image source = patterned_image({3, 2}, PixelFormat::BGR24);
  auto encoded = std::make_shared<Buffer>();
  ASSERT_TRUE(source.write_png(logger, encoded));
  ASSERT_TRUE(encoded->seek(0));
  auto decoded = Image::read_png(logger, encoded);
  ASSERT_TRUE(decoded);
  EXPECT_EQ(decoded->pixel_format(), PixelFormat::RGB24);
  EXPECT_EQ(decoded->size(), source.size());
  EXPECT_TRUE(std::ranges::equal(decoded->data(), source.data()));
  Image empty(Vec2U(0, UINT_MAX));
  EXPECT_FALSE(empty.write_png(logger, std::make_shared<Buffer>()));
}
