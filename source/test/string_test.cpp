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

#include "gtest/gtest.h"

using namespace Star;

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
  EXPECT_EQ(String("\n\raa\n\raa\r\n\r\naa\r\n\r\n\r\n").splitAny("\r\n"), StringList(3, "aa"));
  EXPECT_EQ(String("\n\r\n\r\r\n\r\n\r\n\r\n\r\n").splitAny("\r\n"), StringList());
  EXPECT_EQ(String("").splitAny("\r\n"), StringList());
  EXPECT_EQ(String("xyxFoo").splitAny("x", 1).transform([](auto const& parts) { return parts.join("_"); }), "y_Foo");
  EXPECT_EQ(String("xyxFoo").rsplitAny("x", 1).transform([](auto const& parts) { return parts.join("_"); }), "xy_Foo");
  EXPECT_EQ(String("xyxFooxFoox").rsplitAny("x", 1).transform([](auto const& parts) { return parts.join("_"); }), "xyxFoo_Foo");
  EXPECT_EQ(String("x").rsplitAny("x"), StringList());
  EXPECT_EQ(String("x").splitAny("x", 1), StringList());
  EXPECT_EQ(String("").splitAny("x", 1), StringList());
  EXPECT_EQ(String(std::string_view("asdf\0asdf", 9)).splitAny(String(std::string_view("\0", 1))), StringList(2, "asdf"));
  EXPECT_EQ(String(std::string_view("asdf\0asdf", 9)).splitAny("a"), StringList({String(std::string_view("sdf\0", 4)), String("sdf")}));
}

TEST(StringTest, replace) {
  EXPECT_EQ(String("x").replace("cdc", "foo"), "x");
  EXPECT_EQ(String("cdcdcdc").replace("cdc", "foo"), "foodfoo");
  EXPECT_EQ(String("").replace("cdc", "foo"), String());
  EXPECT_EQ(String("xxx").replace("x", "xx"), "xxxxxx");
  EXPECT_EQ(String(std::string_view("/bin/bash\0aaa:123123:123", 24)).replace(String(std::string_view("\0", 1)), ""), String("/bin/bashaaa:123123:123"));
  EXPECT_EQ(String(std::string_view("/bin/bash\0aaa:123123:123", 24)).replace(String(), ""), String(std::string_view("/bin/bash\0aaa:123123:123", 24)));
}

TEST(StringTest, endsWith) {
  EXPECT_EQ(String("something.com").endsWith(".com"), true);
  EXPECT_EQ(String("something.com").endsWith("fsomething.com"), false);
  EXPECT_EQ(String("something.com").endsWith(""), true);
  EXPECT_EQ(String("something.com").endsWith("SOMETHING.COMF", String::CaseInsensitive), false);
  EXPECT_EQ(String("something.com").endsWith('M', String::CaseInsensitive), true);
  EXPECT_EQ(String("something.com").endsWith('F', String::CaseInsensitive), false);
  EXPECT_EQ(String("").endsWith('f'), false);
  EXPECT_EQ(String(std::string_view("something.com\0", 14)).endsWith("m"), false);
  EXPECT_EQ(String(std::string_view("s\0omething.com", 14)).endsWith("m"), true);
  EXPECT_EQ(String(std::string_view("s\0omething.com", 14)).endsWith("s"), false);
}

TEST(StringTest, beginsWith) {
  EXPECT_EQ(String("something.com").beginsWith("something"), true);
  EXPECT_EQ(String("something.com").beginsWith("something.comf"), false);
  EXPECT_EQ(String("something.com").beginsWith(""), true);
  EXPECT_EQ(String("something.com").beginsWith("FSOMETHING.COM", String::CaseInsensitive), false);
  EXPECT_EQ(String("something.com").beginsWith('S', String::CaseInsensitive), true);
  EXPECT_EQ(String("something.com").beginsWith('F', String::CaseInsensitive), false);
  EXPECT_EQ(String("").beginsWith('s'), false);
  EXPECT_EQ(String(std::string_view("\0something.com", 14)).beginsWith(String(std::string_view("\0", 1))), true);
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
  EXPECT_EQ(String("Foo Bar Foo").toLower(), String("foo bar foo"));
  EXPECT_EQ(String("Foo Bar Foo").toUpper(), String("FOO BAR FOO"));
}

TEST(StringTest, format) {
  EXPECT_EQ(strf("({}, {}, {})", 1, "foo", 3.2), "(1, foo, 3.2)");
  EXPECT_EQ(strf("{} ({}, {}, {})", String(std::string_view("asdf\0", 5)), 1, "foo", 3.2), String(std::string_view("asdf\0 (1, foo, 3.2)", 19)));
}

TEST(StringTest, CompileTimeFormattingPreservesByteLengths) {
  EXPECT_EQ(strf("{1}:{0:04x}", 42, String("日")), "日:002a");
  constexpr std::string_view withNull("x\0{}", 4);
  EXPECT_EQ(strf(withNull, "y"), std::string("x\0y", 3));
}

TEST(StringTest, StreamFormattingPreservesBinaryTextAndFieldPadding) {
  std::ostringstream output;
  Star::format(output, "{}:{:04x}", String(std::string_view("日\0x", 5)), 42);
  EXPECT_EQ(output.str(), std::string("日\0x:002a", 10));
  output.str("");
  output << std::setfill('_') << std::setw(8) << std::left;
  Star::format(output, "{}:{}", "日", 42);
  output << '!';
  EXPECT_EQ(output.str(), "日:42__!");
  EXPECT_EQ(output.width(), 0);
}

TEST(StringTest, StreamFormattingReportsShortWrites) {
  class LimitedBuffer : public std::streambuf {
  public:
    LimitedBuffer() { setp(m_bytes.data(), m_bytes.data() + m_bytes.size()); }
    std::string_view written() const { return {pbase(), static_cast<std::size_t>(pptr() - pbase())}; }

  private:
    std::array<char, 4> m_bytes;
  } buffer;
  std::ostream output(&buffer);
  Star::format(output, "{}", "abcdef");
  EXPECT_TRUE(output.bad());
  EXPECT_EQ(buffer.written(), "abcd");
}

TEST(StringTest, StreamFormattingHonorsTiedStreamsAndUnitbuf) {
  class SyncBuffer : public std::stringbuf {
  public:
    int sync() override {
      flushed = str();
      return 0;
    }
    std::string flushed;
  } tiedBuffer, outputBuffer;
  std::ostream tied(&tiedBuffer);
  std::ostream output(&outputBuffer);
  tied << "pending";
  output.tie(&tied);
  output << std::unitbuf;
  Star::format(output, "{}:{}", "日", 42);
  EXPECT_EQ(tiedBuffer.flushed, "pending");
  EXPECT_EQ(outputBuffer.flushed, "日:42");
}

TEST(StringTest, TagTemplatesPreserveByteLengthsAndRejectInvalidTags) {
  std::array<std::pair<std::string_view, std::string_view>, 3> bindings{{
      {"name", "日"}, {"value", "002a"}, {"name", "ignored"}}};
  auto formatted = formatTags("<name>:<value>", bindings);
  ASSERT_TRUE(formatted);
  EXPECT_EQ(*formatted, "日:002a");
  std::string_view withNull("x\0<name>", 8);
  auto binary = formatTags(withNull, bindings);
  ASSERT_TRUE(binary);
  EXPECT_EQ(*binary, std::string("x\0日", 5));
  std::array<std::pair<std::string_view, std::string_view>, 1> binaryBindings{{
      {"bytes", std::string_view("\xff\0z", 3)}}};
  auto binaryReplacement = formatTags("a<bytes>b", binaryBindings);
  ASSERT_TRUE(binaryReplacement);
  EXPECT_EQ(*binaryReplacement, std::string("a\xff\0zb", 5));
  auto escaped = formatTags("<<name> <name> {not:std-format}", bindings);
  ASSERT_TRUE(escaped);
  EXPECT_EQ(*escaped, "<name> 日 {not:std-format}");
  auto plain = formatTags("unchanged", bindings);
  ASSERT_TRUE(plain);
  EXPECT_EQ(*plain, "unchanged");
  for (std::string_view invalid : {"<missing>", "<>", "<name"}) {
    auto result = formatTags(invalid, bindings);
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().is<FormatException>());
  }
  // Invalid dynamic text must not poison subsequent valid substitutions.
  auto recovered = formatTags("<value>", bindings);
  ASSERT_TRUE(recovered);
  EXPECT_EQ(*recovered, "002a");
}

TEST(StringTest, StandardFormattersPreserveProjectRepresentations) {
  EXPECT_EQ(std::format("{:>6.2}", String("abcd")), "    ab");
  EXPECT_EQ(std::format("{}", Line2I(Vec2I(1, 2), Vec2I(3, 4))), "[(1, 2), (3, 4)]");
  EXPECT_EQ(std::format("{}", Mat3F(1, 2, 3, 4, 5, 6, 7, 8, 9)), "1 2 3\n4 5 6\n7 8 9");
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
  EXPECT_EQ(utf8Length(utf8String.data(), utf8String.size()), 37u);

  String s1 = utf8String;
  EXPECT_EQ(s1.utf8(), std::string(utf8String));
  EXPECT_EQ(s1, utf8String);
  EXPECT_EQ(s1, "This is a [日本語] Unicode String. (日本語)");
  EXPECT_EQ("This is a [日本語] Unicode String. (日本語)", s1);
  EXPECT_EQ(s1.size(), 37u);
  EXPECT_EQ(s1.utf8().size(), 49u);
  EXPECT_EQ(String(std::string_view(s1.utf8Ptr())), String(utf8String));

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

  EXPECT_EQ(String("foo_bar_baz_baf").regexMatch("foo.*baf"), true);
  EXPECT_EQ(String("日本語日本語日本語").regexMatch("日.*本語"), true);
  EXPECT_EQ(String("12345678").regexMatch("[[:digit:]]{1,8}"), true);
  EXPECT_EQ(String("81234567").regexMatch("[[:digit:]]{1,7}"), false);
  EXPECT_EQ(String("12345678").regexMatch("[[:digit:]]{1,7}"), false);
  EXPECT_EQ(String("12345678").regexMatch("[[:digit:]]{1,8}", false), true);
  EXPECT_EQ(String("81234567").regexMatch("[[:digit:]]{1,7}", false), true);
  EXPECT_EQ(String("12345678").regexMatch("[[:digit:]]{1,7}", false), true);

  EXPECT_EQ(String("𠜎𠜱𠝹𠱓𠱸𠲖𠳏𠳕𠴕𠵼𠵿𠸎𠸏𠹷𠺝𠺢𠻗𠻹𠻺𠼭𠼮𠽌𠾴𠾼𠿪𡁜𡁯𡁵𡁶𡁻𡃁𡃉𡇙𢃇𢞵𢫕𢭃𢯊𢱑𢱕𢳂𢴈𢵌𢵧𢺳𣲷𤓓𤶸𤷪"
                   "𥄫𦉘𦟌𦧲𦧺𧨾𨅝𨈇𨋢𨳊𨳍𨳒𩶘")
                .size(),
      62u);
}

TEST(StringTest, tags) {
  String testString = "<foo>:<bar>";
  StringMap<String> tags = {{"foo", "hello"}, {"bar", "there"}};

  EXPECT_EQ(testString.replaceTags(tags), "hello:there");
}

TEST(StringTest, OwningTagLookupPreservesBinaryFragmentsAndFirstFailure) {
  constexpr char inputBytes[] = "前\0<one><two>後\0<unfinished";
  constexpr char outputBytes[] = "前\0x\0y後\0<unfinished";
  String source(std::string_view(inputBytes, sizeof(inputBytes) - 1));
  StringList visited;
  auto result = source.lookupTags([&](String const& key) -> std::expected<String, StarException> {
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
  auto failed = String("<one><bad><later>").lookupTags([&](String const& key) -> std::expected<String, StarException> {
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
  auto owning = source.maybeLookupTagsView([&](StringView) { return replacement; });
  auto borrowed = source.maybeLookupTagsView([&](StringView) { return StringView(replacement); });
  ASSERT_TRUE(owning);
  ASSERT_TRUE(borrowed);
  EXPECT_EQ(*owning, expected);
  EXPECT_EQ(*borrowed, expected);
  EXPECT_FALSE(String("plain").maybeLookupTagsView([](StringView) { return String("unused"); }));
  auto unmatched = String("a<x").maybeLookupTagsView([](StringView) { return String("unused"); });
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
      auto lower = String(spelling).toLower();
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
  EXPECT_EQ(String("foo").regexMatch("foo", true, true), true);
  EXPECT_EQ(String("foo bar").regexMatch("foo", true, true), false);
  EXPECT_EQ(String("foo bar").regexMatch("foo", false, true), true);
  EXPECT_EQ(String("foo bar").regexMatch("FOO", false, false), true);
  EXPECT_EQ(String("foo bar").regexMatch("FOO", false, true), false);
  EXPECT_EQ(String("foo bar").regexMatch("^fo*", false, true), true);
  EXPECT_EQ(String("foo bar").regexMatch("^fo*", true, true), false);
  EXPECT_EQ(String("0123456").regexMatch("\\d{0,9}", true, true), true);
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
