#include <stdio.h>
import std;
import star.memory;
import star.string;
import star.string_view;
import star.list;
import star.unicode;
import star.format;
import star.json;
import star.set;
import star.line;
import star.matrix3;
import star.vector;
import star.bi_map;
import star.input_event;
import star.logging;
import star.map;
import star.text;
import star.text_formatter;

#include "gtest/gtest.h"

using namespace Star;

// Importing domain writers must not change primitive dispatch through implicit
// Json/String constructors. These are the types consumed by nested diagnostics.
static_assert(nonfallible_text_writable<bool>);
static_assert(nonfallible_text_writable<int>);
static_assert(nonfallible_text_writable<double>);
static_assert(nonfallible_text_writable<char>);
static_assert(nonfallible_text_writable<std::string>);
static_assert(nonfallible_text_writable<std::string_view>);
static_assert(std::same_as<decltype(append_text(std::declval<TextBuffer&>(), true)), void>);
static_assert(std::same_as<decltype(write_text(std::declval<TextBuffer&>(), std::declval<Json const&>())),
    std::expected<void, StarException>>);

TEST(StringTest, PointerKeyDiagnosticsIdentifyThePointeeRatherThanTheWrapper) {
  Map<std::shared_ptr<int>, int> values;
  auto owner = std::make_shared<int>(42);
  auto missing = values.get(owner);
  ASSERT_FALSE(missing);
  auto identity = std::format("{}", static_cast<void const*>(owner.get()));
  EXPECT_NE(std::string_view(missing.error().what()).find(identity), std::string_view::npos);
  EXPECT_EQ(std::format("{}", List<std::nullptr_t>{nullptr}), "(nullptr)");
  EXPECT_EQ(std::format("{}", List<void*>{nullptr}), "(0)");
}

TEST(StringTest, SortedComparatorPreservesInputAndDuplicates) {
  StringList input{"b", "a", "b"};
  auto descending = input.sorted([](String const& a, String const& b) { return a > b; });
  EXPECT_EQ(descending, (StringList{"b", "b", "a"}));
  EXPECT_EQ(input, (StringList{"b", "a", "b"}));
}

TEST(StringTest, FilteredPreservesDerivedTypeAndMutableCopyIsolation) {
  StringList const input{"alpha", "beta", "alpha"};
  auto filtered = input.filtered([](String& value) {
    if (value == "beta")
      return false;
    value.append("!");
    return true;
  });
  static_assert(std::same_as<decltype(filtered), StringList>);
  EXPECT_EQ(filtered, (StringList{"alpha!", "alpha!"}));
  EXPECT_EQ(input, (StringList{"alpha", "beta", "alpha"}));

  auto accepted = input.filtered([](String& value) -> std::expected<bool, StarException> {
    if (value == "beta")
      return false;
    value.append("!");
    return true;
  });
  static_assert(std::same_as<decltype(accepted), std::expected<StringList, StarException>>);
  ASSERT_TRUE(accepted);
  EXPECT_EQ(*accepted, (StringList{"alpha!", "alpha!"}));
  EXPECT_EQ(input, (StringList{"alpha", "beta", "alpha"}));
}

TEST(StringTest, FilteredPropagatesFirstPredicateErrorWithoutChangingInput) {
  StringList const input{"alpha", "beta", "alpha"};
  auto filtered = input.filtered([](String& value) -> std::expected<bool, StarException> {
    if (value == "beta")
      return std::unexpected(OutOfRangeException("StringList filtered predicate error", false));
    value = "changed copy";
    return false;
  });
  static_assert(std::same_as<decltype(filtered), std::expected<StringList, StarException>>);
  ASSERT_FALSE(filtered);
  EXPECT_TRUE(filtered.error().is<OutOfRangeException>());
  EXPECT_EQ(input, (StringList{"alpha", "beta", "alpha"}));
}

TEST(StringTest, substr) {
  EXPECT_EQ(String("barbazbaffoo").substr(4, 4), String("azba"));
  EXPECT_EQ(String(std::string_view("\0asdf", 5)).substr(1, 2), String("as"));
}

TEST(StringTest, find) {
  EXPECT_EQ(String("xxFooxx").find("Foo"), 2u);
  EXPECT_EQ(String("xxFooxx").find("foo"), NPos);
  EXPECT_EQ(String("xxFooxx").find("foo", 0, String::CaseInsensitive), 2u);
  EXPECT_EQ(String("xxFooxx").find("bar", 0, String::CaseInsensitive), NPos);
  EXPECT_EQ(String("BAR baz baf BAR").find("bar", 1, String::CaseInsensitive), 12u);
  EXPECT_EQ(String(std::string_view("\0asdf", 5)).find("df"), 3u);
}

TEST(StringTest, split_join) {
  EXPECT_EQ(String("語語日語語日語語").split("日").transform([](auto const& parts) { return parts.join("_"); }), "語語_語語_語語");
  EXPECT_EQ(String("日語語日語語日語語日").split("日").transform([](auto const& parts) { return parts.join("_"); }), "_語語_語語_語語_");
  EXPECT_EQ(String("aabaabaa").split('b').transform([](auto const& parts) { return parts.join("_"); }), "aa_aa_aa");
  EXPECT_EQ(String("baabaabaab").split('b').transform([](auto const& parts) { return parts.join("_"); }), "_aa_aa_aa_");
  EXPECT_EQ(String("a").split("bcd"), StringList{"a"});
  EXPECT_EQ(String("").split("bcd"), StringList{""});
  EXPECT_EQ(String("\n\raa\n\raa\r\n\r\naa\r\n\r\n\r\n").split_any("\r\n"), StringList(3, "aa"));
  EXPECT_EQ(String("\n\r\n\r\r\n\r\n\r\n\r\n\r\n").split_any("\r\n"), StringList());
  EXPECT_EQ(String("").split_any("\r\n"), StringList());
  EXPECT_EQ(String("xyxFoo").split_any("x", 1).transform([](auto const& parts) { return parts.join("_"); }), "y_Foo");
  EXPECT_EQ(String("xyxFoo").rsplit_any("x", 1).transform([](auto const& parts) { return parts.join("_"); }), "xy_Foo");
  EXPECT_EQ(String("xyxFooxFoox").rsplit_any("x", 1).transform([](auto const& parts) { return parts.join("_"); }), "xyxFoo_Foo");
  EXPECT_EQ(String("x").rsplit_any("x"), StringList());
  EXPECT_EQ(String("x").split_any("x", 1), StringList());
  EXPECT_EQ(String("").split_any("x", 1), StringList());
  EXPECT_EQ(String(std::string_view("asdf\0asdf", 9)).split_any(String(std::string_view("\0", 1))), StringList(2, "asdf"));
  EXPECT_EQ(String(std::string_view("asdf\0asdf", 9)).split_any("a"), StringList({String(std::string_view("sdf\0", 4)), String("sdf")}));
}

TEST(StringTest, replace) {
  EXPECT_EQ(String("x").replace("cdc", "foo"), "x");
  EXPECT_EQ(String("cdcdcdc").replace("cdc", "foo"), "foodfoo");
  EXPECT_EQ(String("").replace("cdc", "foo"), String());
  EXPECT_EQ(String("xxx").replace("x", "xx"), "xxxxxx");
  EXPECT_EQ(String(std::string_view("/bin/bash\0aaa:123123:123", 24)).replace(String(std::string_view("\0", 1)), ""), String("/bin/bashaaa:123123:123"));
  EXPECT_EQ(String(std::string_view("/bin/bash\0aaa:123123:123", 24)).replace(String(), ""), String(std::string_view("/bin/bash\0aaa:123123:123", 24)));
}

TEST(StringTest, ends_with) {
  EXPECT_EQ(String("something.com").ends_with(".com"), true);
  EXPECT_EQ(String("something.com").ends_with("fsomething.com"), false);
  EXPECT_EQ(String("something.com").ends_with(""), true);
  EXPECT_EQ(String("something.com").ends_with("SOMETHING.COMF", String::CaseInsensitive), false);
  EXPECT_EQ(String("something.com").ends_with('M', String::CaseInsensitive), true);
  EXPECT_EQ(String("something.com").ends_with('F', String::CaseInsensitive), false);
  EXPECT_EQ(String("").ends_with('f'), false);
  EXPECT_EQ(String(std::string_view("something.com\0", 14)).ends_with("m"), false);
  EXPECT_EQ(String(std::string_view("s\0omething.com", 14)).ends_with("m"), true);
  EXPECT_EQ(String(std::string_view("s\0omething.com", 14)).ends_with("s"), false);
}

TEST(StringTest, SuffixByteComparisonRetainsCodepointsAndEagerErrors) {
  EXPECT_EQ(String("a🙂").ends_with("🙂"), true);
  EXPECT_EQ(String("abc").ends_with("🙂"), false);
  EXPECT_EQ(String("éA").ends_with("éa", String::CaseInsensitive), true);
  EXPECT_EQ(String("éA").ends_with("Éa", String::CaseInsensitive), false);
  EXPECT_EQ(String(std::string_view("x\0a", 3)).ends_with(
      String(std::string_view("\0A", 2)), String::CaseInsensitive), true);
  String invalid_source(std::string("a\xff", 2));
  EXPECT_EQ(invalid_source.ends_with(""), true);
  auto invalid = invalid_source.ends_with("z");
  ASSERT_FALSE(invalid);
  EXPECT_TRUE(invalid.error().is<UnicodeException>());
  String invalid_suffix(std::string("z\xc2", 2));
  auto expected = invalid_suffix.size();
  auto suffix_first = invalid_source.ends_with(invalid_suffix);
  ASSERT_FALSE(expected);
  ASSERT_FALSE(suffix_first);
  EXPECT_STREQ(suffix_first.error().what(), expected.error().what());
}

TEST(StringTest, starts_with) {
  EXPECT_EQ(String("something.com").starts_with("something"), true);
  EXPECT_EQ(String("something.com").starts_with("something.comf"), false);
  EXPECT_EQ(String("something.com").starts_with(""), true);
  EXPECT_EQ(String("something.com").starts_with("FSOMETHING.COM", String::CaseInsensitive), false);
  EXPECT_EQ(String("something.com").starts_with('S', String::CaseInsensitive), true);
  EXPECT_EQ(String("something.com").starts_with('F', String::CaseInsensitive), false);
  EXPECT_EQ(String("").starts_with('s'), false);
  EXPECT_EQ(String(std::string_view("\0something.com", 14)).starts_with(String(std::string_view("\0", 1))), true);
}

TEST(StringTest, trim) {
  EXPECT_EQ(String("").trim(), String());
  EXPECT_EQ(String("   ").trim(), String());
  EXPECT_EQ(String(" \t ").trim(), String());
  EXPECT_EQ(String("   something   ").trim(), "something");
  EXPECT_EQ(String("something").trim(), "something");
  EXPECT_EQ(String("\tsomething\t\t  \t").trim(), "something");

  EXPECT_EQ(String("thththsomethingthththt").trim("th"), "something");
  EXPECT_EQ(String("mmmmmmsomethingmmmmmmm").trim("m"), "something");
  EXPECT_EQ(String("\tsomething\t\t\t").trim("\t"), "something");
  EXPECT_EQ(String(std::string_view("\0something\0\0\0", 13)).trim(String(std::string_view("\0", 1))), "something");
}

TEST(StringTest, extract) {
  String test("xxxfooxxxfooxxxfooxxxbarxxx");
  EXPECT_EQ(test.rextract("x"), "bar");
  EXPECT_EQ(test, "xxxfooxxxfooxxxfoo");
  EXPECT_EQ(test.rextract("x"), "foo");
  EXPECT_EQ(test, "xxxfooxxxfoo");
  EXPECT_EQ(test.rextract("x"), "foo");
  EXPECT_EQ(test, "xxxfoo");
  EXPECT_EQ(test.rextract("x"), "foo");
  EXPECT_EQ(test, "");
}

TEST(StringTest, reverse) {
  EXPECT_EQ(String("FooBar").reverse(), "raBooF");
  EXPECT_EQ(String("").reverse(), "");
}

TEST(StringTest, contains) {
  EXPECT_EQ(String("Foo Bar Foo").contains("foo", String::CaseInsensitive), true);
  EXPECT_EQ(String("Foo Bar Foo").contains("bar foo", String::CaseInsensitive), true);
  EXPECT_EQ(String("Foo Bar Foo").contains("foo"), false);
  EXPECT_EQ(String("Foo Bar Foo").to_lower(), String("foo bar foo"));
  EXPECT_EQ(String("Foo Bar Foo").to_upper(), String("FOO BAR FOO"));
}

TEST(StringTest, format) {
  EXPECT_EQ(Star::format("({}, {}, {})", 1, "foo", 3.2), "(1, foo, 3.2)");
  EXPECT_EQ(Star::format("{} ({}, {}, {})", String(std::string_view("asdf\0", 5)), 1, "foo", 3.2), String(std::string_view("asdf\0 (1, foo, 3.2)", 19)));
}

TEST(StringTest, CompileTimeFormattingPreservesByteLengths) {
  EXPECT_EQ(Star::format("{1}:{0:04x}", 42, String("日")), "日:002a");
  constexpr std::string_view withNull("x\0{}", 4);
  EXPECT_EQ(Star::format(withNull, "y"), std::string("x\0y", 3));
}

TEST(StringTest, CheckedConsoleOutputPreservesBinaryTextAndNewlines) {
  std::unique_ptr<std::FILE, decltype(&std::fclose)> file(std::tmpfile(), &std::fclose);
  ASSERT_TRUE(file);
  ASSERT_TRUE(Star::print(file.get(), "{}:{:04x}", String(std::string_view("水\0x", 5)), 42));
  ASSERT_TRUE(Star::println(file.get(), ":{}", 7));
  ASSERT_EQ(std::fseek(file.get(), 0, SEEK_SET), 0);
  std::array<char, 32> bytes;
  auto count = ::fread(bytes.data(), 1, bytes.size(), file.get());
  EXPECT_EQ(std::string_view(bytes.data(), count), std::string_view("水\0x:002a:7\n", 13));
  EXPECT_EQ(std::ferror(file.get()), 0);
}

#if defined(STAR_SYSTEM_LINUX)
TEST(StringTest, CheckedConsoleOutputReportsFlushFailureWithoutPoisoningLaterOutput) {
  std::unique_ptr<std::FILE, decltype(&std::fclose)> full(std::fopen("/dev/full", "w"), &std::fclose);
  ASSERT_TRUE(full);
  auto failed = Star::print(full.get(), "{}", "buffered bytes");
  ASSERT_FALSE(failed);
  EXPECT_TRUE(failed.error().is<IOException>());
  std::unique_ptr<std::FILE, decltype(&std::fclose)> valid(std::tmpfile(), &std::fclose);
  ASSERT_TRUE(valid);
  ASSERT_TRUE(Star::print(valid.get(), "{}", "recovered"));
  ASSERT_EQ(std::fseek(valid.get(), 0, SEEK_SET), 0);
  std::array<char, 16> bytes;
  auto count = ::fread(bytes.data(), 1, bytes.size(), valid.get());
  EXPECT_EQ(std::string_view(bytes.data(), count), "recovered");
}
#endif

TEST(StringTest, NestedFormattingRetainsBinaryBytesAcrossShortAndLongRepresentations) {
  for (std::size_t count : {0u, 1u, 56u, 57u, 256u}) {
    StringList values;
    std::string expected = "(";
    for (std::size_t i = 0; i != count; ++i) {
      values.push_back(String(std::string_view("水\0x", 5)));
      if (i)
        expected += ", ";
      expected += '\'';
      expected.append("水\0x", 5);
      expected += '\'';
    }
    expected += ')';
    EXPECT_EQ(std::format("{}", values), expected);
    EXPECT_EQ(std::format("{:>80.40}", values), std::format("{:>80.40}", std::string_view(expected)));
  }
}


TEST(StringTest, TagTemplatesPreserveByteLengthsAndRejectInvalidTags) {
  std::array<std::pair<std::string_view, std::string_view>, 3> bindings{{
      {"name", "日"}, {"value", "002a"}, {"name", "ignored"}}};
  auto formatted = Star::format_tags("<name>:<value>", bindings);
  ASSERT_TRUE(formatted);
  EXPECT_EQ(*formatted, "日:002a");
  std::string_view withNull("x\0<name>", 8);
  auto binary = Star::format_tags(withNull, bindings);
  ASSERT_TRUE(binary);
  EXPECT_EQ(*binary, std::string("x\0日", 5));
  std::array<std::pair<std::string_view, std::string_view>, 1> binaryBindings{{
      {"bytes", std::string_view("\xff\0z", 3)}}};
  auto binaryReplacement = Star::format_tags("a<bytes>b", binaryBindings);
  ASSERT_TRUE(binaryReplacement);
  EXPECT_EQ(*binaryReplacement, std::string("a\xff\0zb", 5));
  auto escaped = Star::format_tags("<<name> <name> {not:std-format}", bindings);
  ASSERT_TRUE(escaped);
  EXPECT_EQ(*escaped, "<name> 日 {not:std-format}");
  auto plain = Star::format_tags("unchanged", bindings);
  ASSERT_TRUE(plain);
  EXPECT_EQ(*plain, "unchanged");
  for (std::string_view invalid : {"<missing>", "<>", "<name"}) {
    auto result = Star::format_tags(invalid, bindings);
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().is<FormatException>());
  }
  // Invalid dynamic text must not poison subsequent valid substitutions.
  auto recovered = Star::format_tags("<value>", bindings);
  ASSERT_TRUE(recovered);
  EXPECT_EQ(*recovered, "002a");
}

TEST(StringTest, StandardFormattersPreserveProjectRepresentations) {
  EXPECT_EQ(std::format("{:>6.2}", String("abcd")), "    ab");
  EXPECT_EQ(std::format("{}", Line2I(Vec2I(1, 2), Vec2I(3, 4))), "[(1, 2), (3, 4)]");
  EXPECT_EQ(std::format("{}", Mat3F(1, 2, 3, 4, 5, 6, 7, 8, 9)), "1 2 3\n4 5 6\n7 8 9");
  EXPECT_EQ(std::format("{}", List<bool>{false, true, false}), "(0, 1, 0)");
}

TEST(StringTest, StreamFormattingPreservesLongEscapedJson) {
  String text(std::string(700, 'x') + "\n\"");
  auto json = Json(text);
  std::string expected = "\"" + std::string(700, 'x') + "\\n\\\"\"";
  auto serialized = json.repr();
  ASSERT_TRUE(serialized);
  EXPECT_EQ(std::format("{}", *serialized), expected);
  EXPECT_EQ(std::format("{:.5}", *serialized), "\"xxxx");
}

TEST(StringTest, append) {
  String s = "foo";
  s.append(String("bar"));
  EXPECT_EQ(s, "foobar");

  s = "foo";
  s.append("bar");
  EXPECT_EQ(s, "foobar");

  s = "foo";
  ASSERT_TRUE(s.append('b'));
  EXPECT_EQ(s, "foob");
}

TEST(StringTest, prepend) {
  String s = "foo";
  s.prepend(String("bar"));
  EXPECT_EQ(s, "barfoo");

  s = "foo";
  s.prepend("bar");
  EXPECT_EQ(s, "barfoo");

  s = "foo";
  ASSERT_TRUE(s.prepend('b'));
  EXPECT_EQ(s, "bfoo");
}

TEST(StringTest, utf8) {
  std::string_view utf8String = "This is a [日本語] Unicode String. (日本語)";
  EXPECT_EQ(utf8_length(utf8String.data(), utf8String.size()), 37u);

  String s1 = utf8String;
  EXPECT_EQ(s1.utf8(), std::string(utf8String));
  EXPECT_EQ(s1, utf8String);
  EXPECT_EQ(s1, "This is a [日本語] Unicode String. (日本語)");
  EXPECT_EQ("This is a [日本語] Unicode String. (日本語)", s1);
  EXPECT_EQ(s1.size(), 37u);
  EXPECT_EQ(s1.utf8().size(), 49u);
  EXPECT_EQ(String(std::string_view(s1.utf8_data())), String(utf8String));

  EXPECT_EQ(String("abcdefghijkl").slice(1, 6, 2), String("bdf"));
  auto shorter = String("aa").compare("aaaa");
  ASSERT_TRUE(shorter);
  EXPECT_LT(*shorter, 0);
  auto greater = String("bb").compare("aaaa");
  ASSERT_TRUE(greater);
  EXPECT_GT(*greater, 0);
  EXPECT_EQ(String("[日本語]").compare("[日本語]"), 0);
  auto shorterInsensitive = String("Aa").compare("aAaA", String::CaseInsensitive);
  ASSERT_TRUE(shorterInsensitive);
  EXPECT_LT(*shorterInsensitive, 0);
  auto greaterInsensitive = String("bB").compare("AaAa", String::CaseInsensitive);
  ASSERT_TRUE(greaterInsensitive);
  EXPECT_GT(*greaterInsensitive, 0);
  EXPECT_EQ(String("[日本語]").compare("[日本語]", String::CaseInsensitive), 0);
  EXPECT_EQ(String("[日本語]").find(']', 1), 4u);

  EXPECT_EQ(String("日本語日本語日本語日本語").substr(6, 3), String("日本語"));
  String s2 = "日本語日本語日本語日本語";
  ASSERT_TRUE(s2.erase(6, 3));
  EXPECT_EQ(s2, String("日本語日本語日本語"));
  EXPECT_EQ(String("日本語日本語日本語").reverse(), String("語本日語本日語本日"));

  EXPECT_EQ(String("foo_bar_baz_baf").regex_match("foo.*baf"), true);
  EXPECT_EQ(String("日本語日本語日本語").regex_match("日.*本語"), true);
  EXPECT_EQ(String("12345678").regex_match("[[:digit:]]{1,8}"), true);
  EXPECT_EQ(String("81234567").regex_match("[[:digit:]]{1,7}"), false);
  EXPECT_EQ(String("12345678").regex_match("[[:digit:]]{1,7}"), false);
  EXPECT_EQ(String("12345678").regex_match("[[:digit:]]{1,8}", false), true);
  EXPECT_EQ(String("81234567").regex_match("[[:digit:]]{1,7}", false), true);
  EXPECT_EQ(String("12345678").regex_match("[[:digit:]]{1,7}", false), true);

  EXPECT_EQ(String("𠜎𠜱𠝹𠱓𠱸𠲖𠳏𠳕𠴕𠵼𠵿𠸎𠸏𠹷𠺝𠺢𠻗𠻹𠻺𠼭𠼮𠽌𠾴𠾼𠿪𡁜𡁯𡁵𡁶𡁻𡃁𡃉𡇙𢃇𢞵𢫕𢭃𢯊𢱑𢱕𢳂𢴈𢵌𢵧𢺳𣲷𤓓𤶸𤷪"
                   "𥄫𦉘𦟌𦧲𦧺𧨾𨅝𨈇𨋢𨳊𨳍𨳒𩶘")
                .size(),
      62u);
}

TEST(StringTest, tags) {
  String testString = "<foo>:<bar>";
  StringMap<String> tags = {{"foo", "hello"}, {"bar", "there"}};

  EXPECT_EQ(testString.replace_tags(tags), "hello:there");
}

TEST(StringTest, OwningTagLookupPreservesBinaryFragmentsAndFirstFailure) {
  constexpr char inputBytes[] = "前\0<one><two>後\0<unfinished";
  constexpr char outputBytes[] = "前\0x\0y後\0<unfinished";
  String source(std::string_view(inputBytes, sizeof(inputBytes) - 1));
  StringList visited;
  auto result = source.lookup_tags([&](String const& key) -> std::expected<String, StarException> {
    visited.push_back(key);
    if (key == "one")
      return String(std::string_view("x\0y", 3));
    if (key == "two")
      return String();
    return std::unexpected(StarException("Unexpected lookup of an unfinished tag", false));
  });
  ASSERT_TRUE(result) << result.error().what();
  EXPECT_EQ(result->utf8(), std::string_view(outputBytes, sizeof(outputBytes) - 1));
  EXPECT_EQ(visited, (StringList{"one", "two"}));

  visited.clear();
  auto failed = String("<one><bad><later>").lookup_tags([&](String const& key) -> std::expected<String, StarException> {
    visited.push_back(key);
    if (key == "bad")
      return std::unexpected(OutOfRangeException("Tag lookup failure", false));
    return key;
  });
  ASSERT_FALSE(failed);
  EXPECT_TRUE(failed.error().is<OutOfRangeException>());
  EXPECT_EQ(visited, (StringList{"one", "bad"}));
}

TEST(StringTest, TagViewsPreserveOwningAndBorrowedReplacementBytes) {
  String source("a<x>b<x>");
  String replacement(std::string(4096, 'R') + std::string("\0中", 4));
  String expected = String("a") + replacement + "b" + replacement;
  auto owning = source.maybe_lookup_tags_view([&](StringView) { return replacement; });
  auto borrowed = source.maybe_lookup_tags_view([&](StringView) { return StringView(replacement); });
  ASSERT_TRUE(owning);
  ASSERT_TRUE(borrowed);
  EXPECT_EQ(*owning, expected);
  EXPECT_EQ(*borrowed, expected);
  EXPECT_FALSE(String("plain").maybe_lookup_tags_view([](StringView) { return String("unused"); }));
  auto unmatched = String("a<x").maybe_lookup_tags_view([](StringView) { return String("unused"); });
  ASSERT_TRUE(unmatched);
  EXPECT_EQ(*unmatched, "a<x");
}

TEST(StringTest, CaseInsensitive) {
  auto foo = CaseInsensitiveString::create("foo");
  auto upperFoo = CaseInsensitiveString::create("FOO");
  auto paddedFoo = CaseInsensitiveString::create("foo ");
  auto paddedUpperFoo = CaseInsensitiveString::create("FOO ");
  ASSERT_TRUE(foo);
  ASSERT_TRUE(upperFoo);
  ASSERT_TRUE(paddedFoo);
  ASSERT_TRUE(paddedUpperFoo);
  EXPECT_TRUE(CaseInsensitiveStringCompare()(*foo, *upperFoo));
  EXPECT_FALSE(CaseInsensitiveStringCompare()(*upperFoo, *paddedFoo));
  EXPECT_FALSE(CaseInsensitiveStringCompare()(*paddedFoo, *upperFoo));
  EXPECT_TRUE(CaseInsensitiveStringCompare()(*paddedUpperFoo, *paddedFoo));

  EXPECT_EQ(CaseInsensitiveStringHash()(*foo), CaseInsensitiveStringHash()(*upperFoo));
  EXPECT_NE(CaseInsensitiveStringHash()(*upperFoo), CaseInsensitiveStringHash()(*paddedFoo));
  EXPECT_NE(CaseInsensitiveStringHash()(*paddedFoo), CaseInsensitiveStringHash()(*upperFoo));
  EXPECT_EQ(CaseInsensitiveStringHash()(*paddedUpperFoo), CaseInsensitiveStringHash()(*paddedFoo));
  auto upperView = CaseInsensitiveStringView::create(upperFoo->string());
  ASSERT_TRUE(upperView);
  EXPECT_EQ(upperView->utf8().data(), upperFoo->string().utf8().data());
  EXPECT_EQ(CaseInsensitiveStringHash()(*foo), CaseInsensitiveStringHash()(*upperView));
  EXPECT_EQ(CaseInsensitiveStringHash()(*foo), CaseInsensitiveStringHash()(foo->view()));
  EXPECT_TRUE(CaseInsensitiveStringCompare()(*foo, *upperView));
  EXPECT_TRUE(CaseInsensitiveStringCompare()(*upperView, *foo));
  EXPECT_TRUE(CaseInsensitiveStringCompare()(foo->view(), *upperView));

  auto one = CaseInsensitiveString::create("One");
  auto three = CaseInsensitiveString::create("Three");
  auto mixedOne = CaseInsensitiveString::create("OnE");
  auto lowerOne = CaseInsensitiveStringView::create(std::string_view("one"));
  auto lowerThree = CaseInsensitiveStringView::create(std::string_view("three"));
  auto two = CaseInsensitiveStringView::create(std::string_view("two"));
  ASSERT_TRUE(one);
  ASSERT_TRUE(three);
  ASSERT_TRUE(mixedOne);
  ASSERT_TRUE(lowerOne);
  ASSERT_TRUE(lowerThree);
  ASSERT_TRUE(two);
  CaseInsensitiveStringMap<int> map;
  map[*one] = 1;
  map[*three] = 3;
  map[*mixedOne] = 2;

  EXPECT_TRUE(map.contains(*lowerOne));
  EXPECT_TRUE(map.contains(*lowerThree));
  EXPECT_FALSE(map.contains(*two));
  auto found = map.get(*lowerOne);
  ASSERT_TRUE(found);
  EXPECT_EQ(found->get(), 2);

  StringSet keys;
  for (auto const& p : map)
    keys.add(p.first.string());

  StringSet keyCmp = {"One", "Three"};
  EXPECT_EQ(keys, keyCmp);
}

TEST(StringTest, FixedEnumNamesPreserveCaseInsensitiveLookupAndCanonicalNames) {
  auto roundTrip = []<auto const& Names>() {
    for (auto const& [value, spelling] : Names) {
      auto lower = String(spelling).to_lower();
      ASSERT_TRUE(lower);
      auto parsed = enumValue<Names>(*lower);
      ASSERT_TRUE(parsed);
      EXPECT_EQ(*parsed, value);
      auto canonical = enumName<Names>(*parsed);
      ASSERT_TRUE(canonical);
      EXPECT_EQ(*canonical, spelling);
    }
  };
  roundTrip.operator()<LogLevelNames>();
  roundTrip.operator()<KeyNames>();
  roundTrip.operator()<KeyModNames>();
  roundTrip.operator()<MouseButtonNames>();
  roundTrip.operator()<MouseWheelNames>();
  roundTrip.operator()<ControllerAxisNames>();
  roundTrip.operator()<ControllerButtonNames>();
  EXPECT_EQ(enumValue<KeyNames>("kEyPaD eNtEr"), Key::KeypadEnter);
  EXPECT_EQ(enumValue<KeyModNames>("lMeTa"), KeyMod::LGui);
  EXPECT_EQ(enumName<KeyNames>(Key::Escape), "Esc");
  EXPECT_EQ(enumName<KeyNames>(Key::Delete), "Del");
  EXPECT_EQ(enumName<ControllerButtonNames>(ControllerButton::Invalid), "Invalid");
}

TEST(StringTest, FixedEnumNamesDistinguishUnknownNamesInvalidUtf8AndSparseValues) {
  for (std::string_view text : {"", "Escape", "LMeta", "日本語", "K", "A "}) {
    auto optional = maybeEnumValue<KeyNames>(text);
    ASSERT_TRUE(optional);
    EXPECT_FALSE(*optional);
    auto required = enumValue<KeyNames>(text);
    ASSERT_FALSE(required);
    EXPECT_TRUE(required.error().is<MapException>());
  }
  auto embeddedNull = enumValue<KeyNames>(std::string_view("A\0", 2));
  ASSERT_FALSE(embeddedNull);
  EXPECT_TRUE(embeddedNull.error().is<MapException>());
  for (std::string_view bytes : {std::string_view("\xff", 1), std::string_view("A\xff", 2),
      std::string_view("zzzz\xc3", 5), std::string_view("\xc3\xa9\xff", 3)}) {
    auto optional = maybeEnumValue<KeyNames>(bytes);
    ASSERT_FALSE(optional);
    EXPECT_TRUE(optional.error().is<UnicodeException>());
    auto required = enumValue<LogLevelNames>(bytes);
    ASSERT_FALSE(required);
    EXPECT_TRUE(required.error().is<UnicodeException>());
  }
  auto unknownKey = enumName<KeyNames>(static_cast<Key>(65535));
  ASSERT_FALSE(unknownKey);
  EXPECT_TRUE(unknownKey.error().is<MapException>());
  auto unnamedModifier = enumName<KeyModNames>(KeyMod::Scroll);
  ASSERT_FALSE(unnamedModifier);
  EXPECT_TRUE(unnamedModifier.error().is<MapException>());
  auto unnamedAxis = enumName<ControllerAxisNames>(ControllerAxis::Invalid);
  ASSERT_FALSE(unnamedAxis);
  EXPECT_TRUE(unnamedAxis.error().is<MapException>());
  auto unknownLevel = enumValue<LogLevelNames>("warning");
  ASSERT_FALSE(unknownLevel);
  EXPECT_TRUE(unknownLevel.error().is<MapException>());
}

TEST(StringTest, FixedEnumNamesPreserveEnumSortedIterationAndText) {
  StringList modifiers;
  for (auto const& [value, name] : KeyModNames)
    modifiers.push_back(String(name));
  EXPECT_EQ(modifiers, (StringList{"NoMod", "LShift", "RShift", "LCtrl", "RCtrl",
      "LAlt", "RAlt", "LMeta", "RMeta", "Num", "Caps", "AltGr"}));
  EXPECT_EQ(modifiers.join(" + "),
      "NoMod + LShift + RShift + LCtrl + RCtrl + LAlt + RAlt + LMeta + RMeta + Num + Caps + AltGr");
  StringList tail;
  for (auto const& [value, name] : KeyNames)
    if (value >= Key::NumLock)
      tail.push_back(String(name));
  EXPECT_EQ(tail, (StringList{"NumLock", "CapsLock", "ScrollLock", "RShift", "LShift",
      "RCtrl", "LCtrl", "RAlt", "LAlt", "RGui", "LGui", "AltGr", "Compose", "Help",
      "PrintScreen", "SysReq", "Pause", "Menu", "Power", "F16", "F17", "F18", "F19",
      "F20", "F21", "F22", "F23", "F24"}));
}

TEST(StringTest, RegexSearch) {
  EXPECT_EQ(String("foo").regex_match("foo", true, true), true);
  EXPECT_EQ(String("foo bar").regex_match("foo", true, true), false);
  EXPECT_EQ(String("foo bar").regex_match("foo", false, true), true);
  EXPECT_EQ(String("foo bar").regex_match("FOO", false, false), true);
  EXPECT_EQ(String("foo bar").regex_match("FOO", false, true), false);
  EXPECT_EQ(String("foo bar").regex_match("^fo*", false, true), true);
  EXPECT_EQ(String("foo bar").regex_match("^fo*", true, true), false);
  EXPECT_EQ(String("0123456").regex_match("\\d{0,9}", true, true), true);
}

TEST(StringTest, InvalidUnicodeIsRecoverableAndDoesNotModifyTheSource) {
  String malformed(std::string_view("\xff", 1));
  auto length = malformed.size();
  ASSERT_FALSE(length);
  EXPECT_TRUE(length.error().is<UnicodeException>());
  auto substring = malformed.substr(0, 1);
  ASSERT_FALSE(substring);
  EXPECT_TRUE(substring.error().is<UnicodeException>());
  auto invalidKey = CaseInsensitiveString::create(malformed);
  ASSERT_FALSE(invalidKey);
  EXPECT_TRUE(invalidKey.error().is<UnicodeException>());
  auto invalidView = CaseInsensitiveStringView::create(malformed);
  ASSERT_FALSE(invalidView);
  EXPECT_TRUE(invalidView.error().is<UnicodeException>());
  EXPECT_EQ(malformed.utf8(), std::string("\xff", 1));
  EXPECT_EQ(String("日本語").size(), 3u);
}

TEST(StringTest, OwningUtf8ExtractionRetainsBinaryBytesAndOwnership) {
  static_assert(std::same_as<decltype(std::declval<String const&>().utf8()), std::string const&>);
  static_assert(std::same_as<decltype(std::declval<String&&>().utf8()), std::string>);
  static_assert(std::same_as<decltype(std::declval<String const&&>().utf8()), std::string>);
  static_assert(noexcept(std::declval<String&&>().utf8()));
  static_assert(std::is_nothrow_move_constructible_v<StringList>);
  static_assert(std::is_nothrow_move_assignable_v<StringList>);

  std::string bytes(256, 'x');
  bytes[3] = '\0';
  bytes.append("日本語");
  String source(bytes);
  auto storage = source.utf8().data();
  std::string moved = std::move(source).utf8();
  EXPECT_EQ(moved, bytes);
  EXPECT_EQ(moved.data(), storage);
  source = "new owner";
  EXPECT_EQ(moved, bytes);

  String const constant(bytes);
  std::string copied = std::move(constant).utf8();
  EXPECT_EQ(copied, bytes);
  EXPECT_NE(copied.data(), constant.utf8().data());
  copied[0] = 'y';
  EXPECT_EQ(constant.utf8(), bytes);

  String taken(bytes);
  EXPECT_EQ(taken.take_utf8(), bytes);
  EXPECT_TRUE(taken.empty());
}

TEST(StringTest, EagerOwnerBoundariesRejectMalformedSuffixBeforeRangeErrors) {
  String source(std::string_view("a\xff", 2));
  auto expectUnicode = [](auto result) {
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().template is<UnicodeException>());
  };
  expectUnicode(source.at(0));
  expectUnicode(source.at(NPos));
  expectUnicode(source.substr(0, 0));
  expectUnicode(source.substr(NPos, 1));
  expectUnicode(source.erase(0, 0));
  expectUnicode(source.erase(NPos, 1));
  expectUnicode(source.begin());
  expectUnicode(source.starts_with("a"));
  expectUnicode(source.starts_with('a'));
  expectUnicode(source.to_upper());
  expectUnicode(source.to_lower());
  expectUnicode(source.rot13());
  EXPECT_EQ(source.utf8(), std::string("a\xff", 2));

  // These checked views/searches intentionally stop before the invalid suffix.
  EXPECT_EQ(source[0], U'a');
  EXPECT_EQ(StringView(source).at(0), U'a');
  EXPECT_EQ(StringView(source).substr(0, 1), StringView("a"));
  EXPECT_EQ(source.find('a'), 0u);
  EXPECT_EQ(source.starts_with(""), true);
  EXPECT_EQ(StringView(source).starts_with('a'), true);
}

TEST(StringTest, ValidatingBoundariesKeepCodepointsBinaryBytesAndClamping) {
  String source(std::string_view("a日\0🙂z", 10));
  EXPECT_EQ(source.at(2), U'\0');
  EXPECT_EQ(source.at(3), U'🙂');
  EXPECT_EQ(source.substr(1, 3), String(std::string_view("日\0🙂", 8)));
  EXPECT_EQ(source.substr(2, NPos), String(std::string_view("\0🙂z", 6)));
  EXPECT_EQ(source.substr(5, NPos), String());
  EXPECT_EQ(source.substr(1, 0), String());
  auto outside = source.at(5);
  ASSERT_FALSE(outside);
  EXPECT_TRUE(outside.error().is<OutOfRangeException>());
  String erased = source;
  ASSERT_TRUE(erased.erase(1, 3));
  EXPECT_EQ(erased, "az");
  ASSERT_TRUE(erased.erase(2, NPos));
  EXPECT_EQ(erased, "az");
  ASSERT_TRUE(source.erase(1, NPos));
  EXPECT_EQ(source, "a");
}

TEST(StringTest, ForwardSplitSpansKeepLimitsAndLazyDelimiterErrors) {
  String source(",,a,,,b,,");
  EXPECT_EQ(source.split_any(",", 0), (StringList{"a,,,b,,"}));
  EXPECT_EQ(source.split_any(",", 1), (StringList{"a", "b,,"}));
  EXPECT_EQ(source.split_any(",", 2), (StringList{"a", "b"}));
  EXPECT_EQ(String("日,,語,").split_any(",", 1), (StringList{"日", "語,"}));
  EXPECT_EQ(String(std::string_view("\0a\0\0b\0", 6)).split_any(String(std::string_view("\0", 1)), 1),
      (StringList{"a", String(std::string_view("b\0", 2))}));
  String invalid_pattern(std::string(",\xff", 2));
  EXPECT_EQ(String(",,,").split_any(invalid_pattern), StringList());
  EXPECT_FALSE(String("a").split_any(invalid_pattern, 0));
  auto invalid_remainder = String(std::string("a,\xff", 3)).split_any(",", 1);
  ASSERT_FALSE(invalid_remainder);
  EXPECT_TRUE(invalid_remainder.error().is<UnicodeException>());
}

TEST(StringTest, ReplaceDenseMultibyteBinaryPatternsWithoutRescanningReplacement) {
  String source;
  std::string expected;
  String raw(std::string_view("\xff\0", 2));
  for (unsigned i = 0; i != 128; ++i) {
    source.append(std::string_view("日a\0", 5));
    expected.append("\xff\0", 2);
  }
  String sensitivePattern(std::string_view("日a\0", 5));
  String insensitivePattern(std::string_view("日A\0", 5));
  auto sensitive = source.replace(sensitivePattern, raw);
  ASSERT_TRUE(sensitive);
  EXPECT_EQ(sensitive->utf8(), expected);
  auto insensitive = source.replace(insensitivePattern, raw, String::CaseInsensitive);
  ASSERT_TRUE(insensitive);
  EXPECT_EQ(insensitive->utf8(), expected);
  EXPECT_EQ(source.utf8_size(), 640u);
  EXPECT_EQ(source.replace(insensitivePattern, raw), source);
  EXPECT_EQ(String("日日日日日").replace("日日", "x"), "xx日");
  EXPECT_EQ(String("AaAaA").replace("aa", "aaa", String::CaseInsensitive), "aaaaaaA");
  EXPECT_EQ(String("Ää").replace("ä", "x", String::CaseInsensitive), "Äx");
}

TEST(StringTest, ReplaceValidatesSourceThenPatternButKeepsReplacementRaw) {
  String source(std::string_view("a\xff", 2));
  String pattern(std::string_view("a\xc3", 2));
  auto sourceError = source.size();
  auto patternError = pattern.size();
  ASSERT_FALSE(sourceError);
  ASSERT_FALSE(patternError);
  for (auto sensitivity : {String::CaseSensitive, String::CaseInsensitive}) {
    auto emptyPattern = source.replace("", "x", sensitivity);
    ASSERT_FALSE(emptyPattern);
    EXPECT_STREQ(emptyPattern.error().what(), sourceError.error().what());
    auto bothInvalid = source.replace(pattern, "x", sensitivity);
    ASSERT_FALSE(bothInvalid);
    EXPECT_STREQ(bothInvalid.error().what(), sourceError.error().what());
    auto invalidPattern = String("z").replace(pattern, "x", sensitivity);
    ASSERT_FALSE(invalidPattern);
    EXPECT_STREQ(invalidPattern.error().what(), patternError.error().what());
    EXPECT_EQ(String("valid").replace("", source, sensitivity), "valid");
    EXPECT_EQ(String("aaa").replace("a", source, sensitivity),
        String(std::string_view("a\xff" "a\xff" "a\xff", 6)));
  }
}

TEST(StringTest, BeginsWithValidatesNonemptyPatternBeforeTheCompleteSource) {
  String source(std::string_view("a\xff", 2));
  String prefix(std::string_view("a\xc3", 2));
  auto prefixError = prefix.size();
  ASSERT_FALSE(prefixError);
  for (auto sensitivity : {String::CaseSensitive, String::CaseInsensitive}) {
    auto result = source.starts_with(prefix, sensitivity);
    ASSERT_FALSE(result);
    EXPECT_STREQ(result.error().what(), prefixError.error().what());
    EXPECT_EQ(source.starts_with("", sensitivity), true);
    EXPECT_FALSE(source.starts_with("a", sensitivity));
    EXPECT_EQ(String(std::string_view("日A\0z", 6)).starts_with("日a", sensitivity),
        sensitivity == String::CaseInsensitive);
  }
}

TEST(StringTest, RightSplitCharRetainsEmptyFieldsLikeStringPattern) {
  EXPECT_EQ(String("a,,b").rsplit(','), (StringList{"a", "", "b"}));
  EXPECT_EQ(String(",a,,b,").rsplit(','), (StringList{"", "a", "", "b", ""}));
  EXPECT_EQ(String(",a,,b,").rsplit(',', 0), (StringList{",a,,b,"}));
  EXPECT_EQ(String(",a,,b,").rsplit(',', 1), (StringList{",a,,b", ""}));
  EXPECT_EQ(String(",a,,b,").rsplit(',', 2), (StringList{",a,", "b", ""}));
  EXPECT_EQ(String("").rsplit(','), (StringList{""}));
  EXPECT_EQ(String(",,,").rsplit(','), (StringList{"", "", "", ""}));
  EXPECT_EQ(String("日日日日日").rsplit("日日"), (StringList{"日", "", ""}));
  EXPECT_EQ(String("a").rsplit("long"), (StringList{"a"}));
  EXPECT_EQ(String("a").rsplit(""), (StringList{"a"}));
  String binary(std::string_view("a\0日\0", 6));
  EXPECT_EQ(binary.rsplit(U'\0'), (StringList{"a", "日", ""}));
}

TEST(StringTest, RightSplitAnyCompressesOnlyBeforeTheLimitIsReached) {
  String source(",,a,,,b,,");
  EXPECT_EQ(source.rsplit_any(","), (StringList{"a", "b"}));
  EXPECT_EQ(source.rsplit_any(",", 0), (StringList{",,a,,,b"}));
  EXPECT_EQ(source.rsplit_any(",", 1), (StringList{",,a", "b"}));
  EXPECT_EQ(source.rsplit_any(",", 2), (StringList{"a", "b"}));
  EXPECT_EQ(String(",,,").rsplit_any(",", 0), StringList());
  EXPECT_EQ(String("").rsplit_any(","), StringList());
  EXPECT_EQ(String(" \t日 語 \r\n").rsplit_any("", 1), (StringList{" \t日", "語"}));
  EXPECT_EQ(String("日🙂🙂語🙂").rsplit_any("🙂", 1), (StringList{"日", "語"}));
  EXPECT_EQ(String(std::string_view("\0a\0\0b\0", 6)).rsplit_any(String(std::string_view("\0", 1)), 1),
      (StringList{String(std::string_view("\0a", 2)), "b"}));
}

TEST(StringTest, RightSplitPreservesValidationDirectionAndLazyDelimiterErrors) {
  String badSource(std::string_view("\xff\xc3", 2));
  String badPattern(std::string_view(",\xff", 2));
  auto sourceError = badSource.reverse();
  auto patternError = badPattern.reverse();
  ASSERT_FALSE(sourceError);
  ASSERT_FALSE(patternError);
  for (auto limit : {std::size_t(0), std::size_t(1), NPos}) {
    auto sourceFirst = badSource.rsplit(badPattern, limit);
    ASSERT_FALSE(sourceFirst);
    EXPECT_STREQ(sourceFirst.error().what(), sourceError.error().what());
    auto patternSecond = String("").rsplit(badPattern, limit);
    ASSERT_FALSE(patternSecond);
    EXPECT_STREQ(patternSecond.error().what(), patternError.error().what());
    auto anySourceFirst = badSource.rsplit_any(badPattern, limit);
    ASSERT_FALSE(anySourceFirst);
    EXPECT_STREQ(anySourceFirst.error().what(), sourceError.error().what());
  }
  // Empty input never queries the delimiter set; successful membership also
  // avoids observing the set's malformed suffix.
  EXPECT_EQ(String("").rsplit_any(badPattern), StringList());
  EXPECT_EQ(String(",,,").rsplit_any(badPattern), StringList());
  EXPECT_FALSE(String("a").rsplit_any(badPattern, 0));
  EXPECT_FALSE(String("a\xff").rsplit("", 0));
}

TEST(StringTest, AsciiTransformsPreserveMultibyteSurrogatesAndEmbeddedNulls) {
  String source(std::string_view("aZ\0日\xc3\xa4\xed\xa0\x80", 11));
  EXPECT_EQ(source.to_upper(), String(std::string_view("AZ\0日\xc3\xa4\xed\xa0\x80", 11)));
  EXPECT_EQ(source.to_lower(), String(std::string_view("az\0日\xc3\xa4\xed\xa0\x80", 11)));
  EXPECT_EQ(source.rot13(), String(std::string_view("nM\0日\xc3\xa4\xed\xa0\x80", 11)));
  auto rotated = source.rot13();
  ASSERT_TRUE(rotated);
  EXPECT_EQ(rotated->rot13(), source);
}

TEST(StringTest, JoinAndKnownCountListsRetainOwnedBinaryOutput) {
  std::array<std::string_view, 3> views{std::string_view("a\0", 2), "日", ""};
  StringList strings{std::span<std::string_view const>(views)};
  String separator(std::string_view("\0:", 2));
  String joined = strings.join(separator);
  EXPECT_EQ(joined.utf8(), std::string("a\0\0:日\0:", 9));
  strings[0] = "changed";
  EXPECT_EQ(joined.utf8(), std::string("a\0\0:日\0:", 9));
  std::array<std::u32string_view, 2> wide{U" a ", U" 日 "};
  auto created = StringList::create(std::span<std::u32string_view const>(wide));
  ASSERT_TRUE(created);
  auto trimmed = created->trim_all();
  ASSERT_TRUE(trimmed);
  EXPECT_EQ(*trimmed, (StringList{"a", "日"}));
  EXPECT_EQ(*created, (StringList{" a ", " 日 "}));
  StringList moved(std::move(*trimmed));
  EXPECT_EQ(moved, (StringList{"a", "日"}));
  StringList assigned;
  assigned = std::move(moved);
  EXPECT_EQ(assigned, (StringList{"a", "日"}));
  EXPECT_EQ(StringList{}.join(separator), "");
  EXPECT_EQ(StringList{String(std::string_view("\xff", 1))}.join(separator),
      String(std::string_view("\xff", 1)));
  EXPECT_EQ(String::join_with("/", "a/", "/b", "", "c"), "a/b/c");
  EXPECT_EQ(String::join_with(String(std::string_view("\0", 1)), "a", "b", "c"),
      String(std::string_view("a\0b\0c", 5)));
}

TEST(StringTest, JoinedAccumulatorRetainsExistingEmptyAndInvalidByteBoundaries) {
  String malformed(std::string_view("a\xff", 2));
  EXPECT_EQ(String::join_with("/", "", malformed), malformed);
  EXPECT_EQ(String::join_with("/", malformed, ""), malformed);
  EXPECT_EQ(String::join_with("", malformed, "b"), String(std::string_view("a\xff" "b", 3)));
  EXPECT_FALSE(String::join_with("", "b", malformed));
  EXPECT_FALSE(String::join_with("/", malformed, "b"));
  EXPECT_FALSE(String::join_with("/", "b", malformed));
}

TEST(StringTest, EscapeProcessingPreservesBinaryFragmentsAndFirstCallbackError) {
  constexpr char input[] = "a;\0^red;b\x1breset;c";
  StringView text(std::string_view(input, sizeof(input) - 1));
  std::vector<std::string> fragments;
  std::vector<std::string> commands;
  auto processed = Text::processText(
    text,
    [&](StringView part) -> std::expected<bool, StarException> {
      fragments.emplace_back(part.utf8());
      return true;
    },
    [&](StringView command) -> std::expected<bool, StarException> {
      commands.emplace_back(command.utf8());
      return true;
    });
  ASSERT_TRUE(processed);
  EXPECT_TRUE(*processed);
  EXPECT_EQ(fragments, (std::vector<std::string>{std::string("a;\0", 3), "b", "c"}));
  EXPECT_EQ(commands, (std::vector<std::string>{"red", "reset"}));

  fragments.clear();
  commands.clear();
  auto failed = Text::processText(
    text,
    [&](StringView part) -> std::expected<bool, StarException> {
      fragments.emplace_back(part.utf8());
      if (part.utf8() == "b")
        return std::unexpected(IOException("Text callback failure", false));
      return true;
    },
    [&](StringView command) -> std::expected<bool, StarException> {
      commands.emplace_back(command.utf8());
      return true;
    });
  ASSERT_FALSE(failed);
  EXPECT_TRUE(failed.error().is<IOException>());
  EXPECT_EQ(fragments, (std::vector<std::string>{std::string("a;\0", 3), "b"}));
  EXPECT_EQ(commands, (std::vector<std::string>{"red"}));
}
