#ifdef STAR_SYSTEM_FAMILY_WINDOWS
#include <windows.h>
#endif

import std;
import star.format;
import star.string;
import star.unicode;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
import star.algorithm;
import star.file;
import star.list;
import star.random;
#endif

#include "gtest/gtest.h"

using namespace Star;

TEST(StringWideTest, BoundedNativeTextRoundTripsUnicodeScalarsAndNuls) {
  constexpr wchar_t units[] = L"A\0\u6c34\U00010000\U0001f600\U0010ffff\0";
  constexpr char bytes[] = "A\0\xe6\xb0\xb4\xf0\x90\x80\x80\xf0\x9f\x98\x80\xf4\x8f\xbf\xbf\0";
  std::wstring_view wide(units, std::size(units) - 1);
  std::string_view utf8(bytes, std::size(bytes) - 1);
  auto text = String::from_wstring(wide);
  ASSERT_TRUE(text);
  EXPECT_EQ(text->utf8(), utf8);
  auto converted = text->to_wstring();
  ASSERT_TRUE(converted);
  EXPECT_EQ(*converted, wide);
  auto empty = String::from_wstring({});
  ASSERT_TRUE(empty);
  auto empty_wide = empty->to_wstring();
  ASSERT_TRUE(empty_wide);
  EXPECT_TRUE(empty_wide->empty());
}

TEST(StringWideTest, NativeConversionsRejectMalformedInputWithoutNarrowingTheStringCodec) {
  for (auto const& wide : {std::wstring(1, wchar_t(0xd800)), std::wstring(1, wchar_t(0xdc00)),
           std::wstring{wchar_t(0xd800), L'x'}, std::wstring{L'\0', wchar_t(0xd800)}}) {
    auto invalid = String::from_wstring(wide);
    ASSERT_FALSE(invalid);
    EXPECT_TRUE(invalid.error().is<UnicodeException>());
  }
  if constexpr (sizeof(wchar_t) == 4) {
    auto invalid = String::from_wstring(std::wstring(1, static_cast<wchar_t>(0x110000)));
    ASSERT_FALSE(invalid);
    EXPECT_TRUE(invalid.error().is<UnicodeException>());
  }
  for (auto surrogate : {char32_t(0xd800), char32_t(0xdfff)}) {
    auto text = String::create(surrogate);
    ASSERT_TRUE(text);  // String itself deliberately permits encoded surrogate values.
    auto invalid = text->to_wstring();
    ASSERT_FALSE(invalid);
    EXPECT_TRUE(invalid.error().is<UnicodeException>());
  }
  auto malformed = String(std::string("\0\xff", 2)).to_wstring();
  ASSERT_FALSE(malformed);
  EXPECT_TRUE(malformed.error().is<UnicodeException>());
}

#ifdef STAR_SYSTEM_FAMILY_WINDOWS
TEST(StringWideTest, WindowsSupplementaryFilenameReachesWin32AndDirectoryEnumeration) {
  static_assert(sizeof(wchar_t) == 2);
  auto random = RandomService::create();
  ASSERT_TRUE(random);
  auto directory = File::temporary_directory(**random);
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  String filename("orbit-\xf0\x9f\x9a\x80.txt");
  auto path = File::relative_to(*directory, filename);
  ASSERT_TRUE(File::write_file(String("probe"), path));

  auto native_directory = directory->to_wstring();
  ASSERT_TRUE(native_directory);
  auto native_path = std::filesystem::path(std::move(*native_directory)) / L"orbit-\U0001f680.txt";
  EXPECT_NE(GetFileAttributesW(native_path.c_str()), INVALID_FILE_ATTRIBUTES);
  auto entries = File::directory_entries(*directory, true);
  ASSERT_TRUE(entries);
  ASSERT_EQ(entries->size(), 1u);
  EXPECT_EQ((*entries)[0].first, filename);
}
#endif
