import std;
import star.format;
import star.json;
import star.string;
import star.unicode;
import star.file;
import star.lexical_cast;
import star.json_path;
import star.json_builder;
import star.json_parser;
import star.random;

#include "gtest/gtest.h"
import star.json_patch;

import star.algorithm;
using namespace Star;

TEST(JsonTest, ImplicitSharing) {
  Json map1 = JsonObject{{"foo", 1}, {"bar", 10}};

  Json map2 = JsonObject{{"foo", 5}, {"bar", 50}};

  std::swap(map1, map2);
  Json map3 = map1;
  map1 = map2;
  map2 = map3;

  auto result1 = map1.get("foo");
  ASSERT_TRUE(result1);
  EXPECT_EQ((*result1), 1);
  auto result2 = map2.get("bar");
  ASSERT_TRUE(result2);
  EXPECT_EQ((*result2), 50);
}

TEST(JsonTest, Defaults) {
  Json obj = JsonObject{{"null", Json()}};
  Json arr = JsonArray{"array", JsonArray{Json(), Json()}};

  auto result3 = obj.getInt("null", 5);
  ASSERT_TRUE(result3);
  EXPECT_EQ((*result3), 5);
  auto result4 = arr.getInt(2, 5);
  ASSERT_TRUE(result4);
  EXPECT_EQ((*result4), 5);
  auto result5 = arr.getInt(3, 5);
  ASSERT_TRUE(result5);
  EXPECT_EQ((*result5), 5);
  auto error6 = arr.getInt(2);
  ASSERT_FALSE(error6);
  EXPECT_TRUE(error6.error().is<JsonException>());
}

TEST(JsonTest, Merging) {
  JsonObject a{{"I", "feel"}, {"friendly", "now"}};
  JsonObject b{{"hello", "there"}, {"leg", "friend"}};
  JsonObject c{{"hello", "you"}, {"leg", "fiend"}};
  JsonObject d{{"goodbye", "you"}, {"friendly", "leg"}};

  Json merged = jsonMerge(a, b, c, d);

  auto result7 = merged.get("I");
  ASSERT_TRUE(result7);
  EXPECT_EQ((*result7), "feel");
  auto result8 = merged.get("hello");
  ASSERT_TRUE(result8);
  EXPECT_EQ((*result8), "you");
  auto result9 = merged.get("friendly");
  ASSERT_TRUE(result9);
  EXPECT_EQ((*result9), "leg");
  auto result10 = merged.get("leg");
  ASSERT_TRUE(result10);
  EXPECT_EQ((*result10), "fiend");

  Json e = JsonObject();
  auto result11 = e.set("1", 2);
  ASSERT_TRUE(result11);
  e = (*result11);
  auto result12 = e.setAll({{"a", "b"}, {"c", "d"}});
  ASSERT_TRUE(result12);
  e = (*result12);
  Json f = JsonObject{{"1", 2}, {"a", "b"}, {"c", "d"}};

  EXPECT_EQ(e, f);

  Json g = JsonObject{{"a", "a"}, {"sub", JsonObject()}};
  auto result13 = g.setPath("sub.field", 1);
  ASSERT_TRUE(result13);
  g = (*result13);
  auto result14 = g.setPath("sub.field2", 2);
  ASSERT_TRUE(result14);
  g = (*result14);
  auto result15 = g.erasePath("sub.field2");
  ASSERT_TRUE(result15);
  g = (*result15);
  Json h = JsonObject{{"a", "a"}, {"sub", JsonObject{{"field", 1}}}};
  EXPECT_EQ(g, h);
}

TEST(JsonTest, Unicode) {
  auto result16 = Json::parse("{ \"first\" : \"日本語\", \"second\" : \"foobar\\u0019\" }");
  ASSERT_TRUE(result16);
  Json v = (*result16);
  auto result17 = v.getString("first");
  ASSERT_TRUE(result17);
  EXPECT_EQ((*result17), String("日本語"));
  auto result18 = v.get("second");
  ASSERT_TRUE(result18);
  auto result19 = (*result18).repr();
  ASSERT_TRUE(result19);
  EXPECT_EQ((*result19), String("\"foobar\\u0019\""));

  auto result20 = v.printJson();
  ASSERT_TRUE(result20);
  String json = (*result20);
  auto result21 = Json::parseJson(json);
  ASSERT_TRUE(result21);
  Json v2 = (*result21);
  auto result22 = v2.getString("first");
  ASSERT_TRUE(result22);
  EXPECT_EQ((*result22), String("日本語"));

  EXPECT_EQ(v, v2);

  auto result23 = Json::parse("\"\\ud83d\\ude00\"");
  ASSERT_TRUE(result23);
  EXPECT_EQ(Json("😀"), (*result23));
  auto result25 = (*result23).toString();
  ASSERT_TRUE(result25);
  auto result26 = (*result25).size();
  ASSERT_TRUE(result26);
  EXPECT_EQ((*result26), 1u);
}

TEST(JsonTest, UnicodeFile) {
  auto result27 = Json::parse("{ \"first\" : \"日本語\", \"second\" : \"foobar\\u0019\" }");
  ASSERT_TRUE(result27);
  Json v = (*result27);
  RandomService random(0);
  auto result31 = File::temporaryFileName(random);
  ASSERT_TRUE(result31);
  String file = (*result31);
  auto finallyGuard = finally([&file]() { EXPECT_TRUE(File::remove(file)); });

  auto result32 = v.printJson();
  ASSERT_TRUE(result32);
  auto result33 = File::writeFile((*result32), file);
  ASSERT_TRUE(result33);
  auto result34 = File::readFileString(file);
  ASSERT_TRUE(result34);
  auto result35 = Json::parseJson((*result34));
  ASSERT_TRUE(result35);
  Json v2 = (*result35);
  EXPECT_EQ(v, v2);
}

TEST(JsonTest, JsonParsingEdge) {
  auto isValidFragment = [](String const& json) -> bool {
    auto result = Json::parse(json);
    if (!result)
      EXPECT_TRUE(result.error().is<JsonParsingException>());
    return result.has_value();
  };

  auto isValidJson = [](String const& json) -> bool {
    auto result = Json::parseJson(json);
    if (!result)
      EXPECT_TRUE(result.error().is<JsonParsingException>());
    return result.has_value();
  };

  EXPECT_TRUE(isValidFragment(" \t 0.0 "));
  EXPECT_TRUE(isValidFragment("-0.0\t "));
  EXPECT_FALSE(isValidFragment("-.0"));
  EXPECT_FALSE(isValidFragment("00.0"));

  EXPECT_FALSE(isValidJson(" 0.0"));
  EXPECT_FALSE(isValidJson("true"));
  EXPECT_TRUE(isValidJson("\t[]"));
  EXPECT_TRUE(isValidJson(" {} "));
}

TEST(JsonTest, CommentHeavyParsingRetainsValuesAndUnicodeValidation) {
  std::string text = "\xef\xbb\xbf";
  for (unsigned i = 0; i < 256; ++i)
    text += " \t/* 水😀\n ** nested-looking /* text */ // é\n";
  text += R"JSON({
    /* before key */ "values" /* before colon */ : /* before value */
    [1 /* before comma */, // before next value
     -2.5e+1, /* before null */ null],
    "text": "水😀" // before closing object
  } /* after document */)JSON";
  auto parsed = Json::parseJson(String(text));
  ASSERT_TRUE(parsed);
  EXPECT_EQ(*parsed, (Json{JsonObject{{"values", JsonArray{1, -25.0, Json()}}, {"text", "水😀"}}}));

  // Discarding comment text must still decode every visited codepoint.
  auto invalid = Json::parseJson(String(std::string_view("[0] /*\xff*/", 9)));
  ASSERT_FALSE(invalid);
  EXPECT_TRUE(invalid.error().is<UnicodeException>());
}

TEST(JsonTest, TokenFailuresRetainCodepointPositions) {
  struct Failure {
    std::string_view input;
    std::string_view position;
  };
  for (auto const& failure : std::array<Failure, 5>{
         Failure{"[\n /*水\n*/ 1,\n /x]", "4:3"}, Failure{"[\n /*水*/ 1,\n ]", "3:2"}, Failure{"/*水\nabc", "2:4"},
         Failure{"[\n 1e+,\n 2]", "2:5"}, Failure{"[\n \"水😀\", /x]", "2:9"}}) {
    auto parsed = Json::parse(String(failure.input));
    ASSERT_FALSE(parsed);
    EXPECT_TRUE(parsed.error().is<JsonParsingException>());
    auto diagnostic = printException(parsed.error(), false);
    EXPECT_NE(diagnostic.find(strf("Error parsing json at {}", failure.position)), std::string::npos);
  }
}

TEST(JsonTest, NumericParsingPreservesFullTokenRangeSemantics) {
  // The existing lexical policy accepts fully consumed range errors: an
  // overflowed integer keeps its value-initialized zero, not a clamped value.
  for (std::string_view text :
       std::array<std::string_view, 3>{"9223372036854775808", "-9223372036854775809",
                                       "999999999999999999999999999999999999999999999999999999999999999999"}) {
    auto parsed = Json::parse(String(text));
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->type(), Json::Type::Int);
    auto value = parsed->toInt();
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 0);
  }

  // Pinned libc++ writes signed infinity/zero on floating range errors.
  // Inspect the representation, not fast-math-sensitive floating predicates.
  for (auto const& [text, bits] :
       std::array<std::pair<std::string_view, std::uint64_t>, 6>{{{"1e99999", 0x7ff0000000000000},
                                                                  {"-1e99999", 0xfff0000000000000},
                                                                  {"1e-99999", 0x0000000000000000},
                                                                  {"-1e-99999", 0x8000000000000000},
                                                                  {"1.", 0x3ff0000000000000},
                                                                  {"-0.", 0x8000000000000000}}}) {
    auto parsed = Json::parse(String(text));
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->type(), Json::Type::Float);
    auto value = parsed->toDouble();
    ASSERT_TRUE(value);
    std::uint64_t actual;
    std::memcpy(&actual, &*value, sizeof(actual));
    EXPECT_EQ(actual, bits);
  }

  for (std::string_view text : std::array<std::string_view, 5>{"1e", "1e+", "1.e-", "-", "-.0"}) {
    auto parsed = Json::parse(String(text));
    ASSERT_FALSE(parsed);
    EXPECT_TRUE(parsed.error().is<JsonParsingException>());
  }
}

TEST(JsonTest, LazyUtf8PreservesFirstError) {
  auto syntaxError = Json::parse(String(std::string_view("! \xff", 3)));
  ASSERT_FALSE(syntaxError);
  EXPECT_TRUE(syntaxError.error().is<JsonParsingException>());
  EXPECT_FALSE(syntaxError.error().is<UnicodeException>());

  auto decodeError = Json::parse(String(std::string_view("1 \xff", 3)));
  ASSERT_FALSE(decodeError);
  EXPECT_TRUE(decodeError.error().is<UnicodeException>());
  EXPECT_FALSE(decodeError.error().is<JsonParsingException>());

  for (auto text : {std::string_view("/x \xff", 4), std::string_view("[1e+ \xff]", 7)}) {
    auto firstError = Json::parse(String(text));
    ASSERT_FALSE(firstError);
    EXPECT_TRUE(firstError.error().is<JsonParsingException>());
    EXPECT_FALSE(firstError.error().is<UnicodeException>());
  }
  // A malformed codepoint needed to finish the numeric token is visited
  // before the builder can diagnose its incomplete exponent.
  auto numericDecodeError = Json::parse(String(std::string_view("[1e+\xff]", 6)));
  ASSERT_FALSE(numericDecodeError);
  EXPECT_TRUE(numericDecodeError.error().is<UnicodeException>());

  auto valid = Json::parse(R"({"valid":true})");
  ASSERT_TRUE(valid);
  EXPECT_EQ(*valid, (Json{JsonObject{{"valid", true}}}));
}

TEST(JsonTest, Utf8TokensMatchGenericInputAcrossEscapesAndSequence) {
  struct Input {
    std::string_view text;
    JsonParseType type;
    Json expected;
  };
  for (auto const& input : std::array<Input, 3>{
         Input{R"({"ASCII":"plain","水😀":"é水😀","escaped\u006b":"水\n\uD83D\uDE00"})", JsonParseType::Value,
               JsonObject{{"ASCII", "plain"}, {"水😀", "é水😀"}, {"escapedk", "水\n😀"}}},
         Input{R"(["","ASCII","é水😀","水\\尾","\u0000水"])", JsonParseType::Top,
               JsonArray{"", "ASCII", "é水😀", "水\\尾", String(std::string_view("\0水", 4))}},
         Input{R"("ASCII" "水😀" "水\n\uD83D\uDE00" tail)", JsonParseType::Sequence,
               JsonArray{"ASCII", "水😀", "水\n😀", "tail"}}}) {
    auto contiguous = inputUtf8Json(input.text.begin(), input.text.end(), input.type);
    std::list<char> bytes(input.text.begin(), input.text.end());
    auto generic = inputUtf8Json(bytes.begin(), bytes.end(), input.type);
    ASSERT_TRUE(contiguous);
    ASSERT_TRUE(generic);
    EXPECT_EQ(*contiguous, input.expected);
    EXPECT_EQ(*generic, input.expected);
  }
}

TEST(JsonTest, Utf8StringBoundariesPreserveFirstError) {
  struct RejectStrings : JsonBuilderStream {
    std::expected<void, StarException> putString(std::span<char32_t const>) override {
      return std::unexpected(JsonException("rejected string"));
    }
    std::expected<void, StarException> putStringUtf8(std::string_view bytes) override {
      return JsonStream::putStringUtf8(bytes);
    }
  };
  for (auto const& [text, unicode] : std::array<std::pair<std::string_view, bool>, 3>{
         {{"\"水\"\xff", true}, {"[\"水\", \xff]", false}, {"[\"水\\n\", \xff]", false}}}) {
    auto parsed =
      inputUtf8Json<std::string_view::const_iterator, RejectStrings>(text.begin(), text.end(), JsonParseType::Value);
    ASSERT_FALSE(parsed);
    EXPECT_EQ(parsed.error().is<UnicodeException>(), unicode);
    EXPECT_EQ(parsed.error().is<JsonException>(), !unicode);
  }
  // Invalid escape syntax wins before an unvisited malformed UTF-8 suffix.
  auto escape = Json::parse(String(std::string_view("\"水\\q \xff\"")));
  ASSERT_FALSE(escape);
  EXPECT_TRUE(escape.error().is<JsonParsingException>());
  EXPECT_FALSE(escape.error().is<UnicodeException>());
}

TEST(JsonTest, Types) {
  Json v;
  EXPECT_EQ(v.type(), Json::Type::Null);
  v = 0;
  EXPECT_EQ(v.type(), Json::Type::Int);
  v = 0.0;
  EXPECT_EQ(v.type(), Json::Type::Float);
  v = true;
  EXPECT_EQ(v.type(), Json::Type::Bool);
  v = "";
  EXPECT_EQ(v.type(), Json::Type::String);
  v = JsonArray();
  EXPECT_EQ(v.type(), Json::Type::Array);
  v = JsonObject();
  EXPECT_EQ(v.type(), Json::Type::Object);
}

TEST(JsonTest, NumericOutputPreservesIntegerExtrema) {
  Json numbers = JsonArray{std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max(), 0, -1};
  auto text = numbers.repr();
  ASSERT_TRUE(text);
  EXPECT_EQ(*text, "[-9223372036854775808,9223372036854775807,0,-1]");

  auto parsed = Json::parseJson(*text);
  ASSERT_TRUE(parsed);
  EXPECT_EQ(*parsed, numbers);
}

TEST(JsonTest, NumericOutputPreservesFloatingBoundaries) {
  Json numbers = JsonArray{
      0.0, std::bit_cast<double>(std::uint64_t{0x8000000000000000}), 1.5, 1e100, 1e-100,
      std::numeric_limits<double>::max(), std::numeric_limits<double>::lowest(),
      std::numeric_limits<double>::min(), std::numeric_limits<double>::denorm_min()};
  auto text = numbers.repr();
  ASSERT_TRUE(text);
  EXPECT_EQ(*text, "[0,-0,1.5,1e+100,1e-100,1.7976931348623157e+308,-1.7976931348623157e+308,"
      "2.2250738585072014e-308,5e-324]");
}

TEST(JsonTest, NumericOutputPreservesNonfiniteSpellings) {
  Json numbers = JsonArray{
      std::bit_cast<double>(std::uint64_t{0x7ff0000000000000}),
      std::bit_cast<double>(std::uint64_t{0xfff0000000000000}),
      std::bit_cast<double>(std::uint64_t{0x7ff8000000000000})};
  auto text = numbers.repr();
  ASSERT_TRUE(text);
  // Preserve the existing spellings, even though strict JSON cannot parse them.
  EXPECT_EQ(*text, "[inf,-inf,nan]");
}

TEST(JsonTest, NumericStreamPreservesKindsAndSignedZero) {
  JsonArray numbers{
      std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max(), 0,
      0.0, std::bit_cast<double>(std::uint64_t{0x8000000000000000}),
      std::numeric_limits<double>::max(), std::numeric_limits<double>::denorm_min()};
  for (auto const& number : numbers) {
    JsonBuilderStream sink;
    auto written = JsonStreamer<Json>::toJsonStream(number, sink, false);
    ASSERT_TRUE(written);
    auto rebuilt = sink.takeTop();
    ASSERT_TRUE(rebuilt);
    EXPECT_EQ(rebuilt->type(), number.type());
    if (number.type() == Json::Type::Int) {
      auto original = number.toInt();
      auto actual = rebuilt->toInt();
      ASSERT_TRUE(original);
      ASSERT_TRUE(actual);
      EXPECT_EQ(*actual, *original);
    } else {
      auto original = number.toDouble();
      auto actual = rebuilt->toDouble();
      ASSERT_TRUE(original);
      ASSERT_TRUE(actual);
      EXPECT_EQ(std::bit_cast<std::uint64_t>(*actual), std::bit_cast<std::uint64_t>(*original));
    }
  }
}

TEST(JsonTest, NumericOutputStopsAtFirstSinkError) {
  struct Sink {
    std::u32string& prefix;
    std::size_t& calls;
    std::size_t limit;

    std::expected<void, StarException> push(char32_t c) {
      ++calls;
      if (prefix.size() == limit)
        return std::unexpected(JsonException("numeric sink failure"));
      prefix.push_back(c);
      return {};
    }
  };

  JsonArray numbers{std::numeric_limits<std::int64_t>::min(), std::numeric_limits<double>::max()};
  for (auto const& number : numbers) {
    for (std::size_t limit : {std::size_t{1}, std::size_t{6}}) {
      std::u32string prefix;
      std::size_t calls = 0;
      // The invalid string must not replace the earlier numeric sink failure.
      Json input = JsonArray{number, String(std::string_view("\xff", 1))};
      auto written = outputUtf32Json(input, Sink{prefix, calls, limit}, 0, false);
      ASSERT_FALSE(written);
      EXPECT_TRUE(written.error().is<JsonException>());
      EXPECT_FALSE(written.error().is<UnicodeException>());
      EXPECT_EQ(calls, limit + 1);
      EXPECT_EQ(prefix.size(), limit);
      if (limit == 1)
        EXPECT_EQ(prefix, U"[");
      else if (number.type() == Json::Type::Int)
        EXPECT_EQ(prefix, U"[-9223");
      else
        EXPECT_EQ(prefix, U"[1.797");
    }
  }
}

TEST(JsonTest, Utf8OutputPreservesExactEscapesUnicodeAndFormatting) {
  String controls(std::string_view("A\0\1\b\f\n\r\t\"\\", 10));
  Json input = JsonObject{{"é", 1}, {"a", JsonArray{controls, "é水😀", Json(), true}}};
  std::string escaped = "\"A\\u0000\\u0001\\b\\f\\n\\r\\t\\\"\\\\\"";
  std::string compact = "{\"a\":[" + escaped + ",\"é水😀\",null,true],\"é\":1}";
  std::string pretty = "{\n  \"a\" : [" + escaped + ", \"é水😀\", null, true],\n  \"é\" : 1\n}";
  JsonBuilderStream builder;
  auto streamed = JsonStreamer<Json>::toJsonStream(input, builder, true);
  ASSERT_TRUE(streamed);
  auto rebuilt = builder.takeTop();
  ASSERT_TRUE(rebuilt);
  EXPECT_EQ(*rebuilt, input);
  for (int indentation : {0, 2}) {
    auto repr = input.repr(indentation, true);
    ASSERT_TRUE(repr);
    EXPECT_EQ(repr->utf8(), indentation == 0 ? compact : pretty);
    auto parsed = Json::parseJson(*repr);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(*parsed, input);
    for (bool sort : {false, true}) {
      String wideOutput;
      auto wide = outputUtf32Json(input, JsonStringOutputIterator(wideOutput), indentation, sort);
      ASSERT_TRUE(wide);
      std::string byteOutput;
      auto bytes = outputUtf8Json(input, std::back_inserter(byteOutput), indentation, sort);
      ASSERT_TRUE(bytes);
      EXPECT_EQ(byteOutput, wideOutput.utf8());
      std::string appendedOutput;
      auto appended = outputUtf8Json(input, JsonUtf8StringOutputIterator(appendedOutput), indentation, sort);
      ASSERT_TRUE(appended);
      EXPECT_EQ(appendedOutput, wideOutput.utf8());
    }
  }
}

TEST(JsonTest, Utf8OutputValidatesCurrentStringBeforeWriting) {
  struct Sink {
    std::string& prefix;
    std::size_t& calls;
    std::expected<void, StarException> push(char c) {
      ++calls;
      prefix.push_back(c);
      return std::unexpected(JsonException("unexpected output callback"));
    }
  };
  for (auto const& invalid : {String(std::string_view("valid\xff", 6)), String(std::string_view("valid\0\xff", 7)),
           String(std::string_view("valid\xe2\x82", 7)), String(std::string_view("valid\xc0\x80", 7)), String(std::string_view("valid\xf4\x90\x80\x80", 9))}) {
    std::string prefix;
    std::size_t calls = 0;
    auto written = outputUtf8Json(Json(invalid), Sink{prefix, calls}, 2, false);
    ASSERT_FALSE(written);
    EXPECT_TRUE(written.error().is<UnicodeException>());
    EXPECT_EQ(calls, 0u);
    EXPECT_TRUE(prefix.empty());

    // A genuine UTF-32 builder must also receive no partial string callback.
    struct Builder : JsonBuilderStream {
      std::size_t strings = 0;
      std::expected<void, StarException> putString(std::span<char32_t const> text) override {
        ++strings;
        return JsonBuilderStream::putString(text);
      }
      std::expected<void, StarException> putStringUtf8(std::string_view bytes) override {
        return JsonStream::putStringUtf8(bytes);
      }
    } builder;
    auto built = JsonStreamer<Json>::toJsonStream(Json(invalid), builder, false);
    ASSERT_FALSE(built);
    EXPECT_TRUE(built.error().is<UnicodeException>());
    EXPECT_EQ(builder.strings, 0u);
  }
}

TEST(JsonTest, Utf8OutputPreservesFirstSinkErrorBeforeInvalidLaterValues) {
  struct Sink {
    std::string& prefix;
    std::size_t& calls;
    std::size_t limit;
    std::expected<void, StarException> push(char c) {
      ++calls;
      if (prefix.size() == limit)
        return std::unexpected(JsonException("UTF-8 sink failure"));
      prefix.push_back(c);
      return {};
    }
  };
  Json input = JsonArray{"é水😀", String(std::string_view("bad\xff", 4))};
  std::string validPrefix = "[\"é水😀\",";
  for (std::size_t limit = 0; limit < validPrefix.size(); ++limit) {
    std::string prefix;
    std::size_t calls = 0;
    auto written = outputUtf8Json(input, Sink{prefix, calls, limit}, 0, false);
    ASSERT_FALSE(written);
    EXPECT_TRUE(written.error().is<JsonException>());
    EXPECT_FALSE(written.error().is<UnicodeException>());
    EXPECT_EQ(calls, limit + 1);
    EXPECT_EQ(prefix, validPrefix.substr(0, limit));
  }
  for (Json const& invalid : {input, Json(JsonObject{{String(std::string_view("key\0\xff", 5)), 1}})}) {
    std::string prefix;
    std::size_t calls = 0;
    auto written = outputUtf8Json(invalid, Sink{prefix, calls, 100}, 2, true);
    ASSERT_FALSE(written);
    EXPECT_TRUE(written.error().is<UnicodeException>());
    // Invalid keys are checked after '{', but before indentation or quotes.
    if (invalid.type() == Json::Type::Object) {
      EXPECT_EQ(prefix, "{");
      EXPECT_EQ(calls, 1u);
    } else {
      EXPECT_EQ(prefix, validPrefix);
    }
  }
}

TEST(JsonTest, Utf8WhitespaceValidatesBeforeOutputAndPreservesBytes) {
  std::string output = "prefix";
  JsonWriter<JsonUtf8StringOutputIterator, true> writer{JsonUtf8StringOutputIterator(output)};
  auto invalid = writer.putWhitespaceUtf8(std::string_view(" /*水*/\xff"));
  ASSERT_FALSE(invalid);
  EXPECT_TRUE(invalid.error().is<UnicodeException>());
  EXPECT_EQ(output, "prefix");
  auto valid = writer.putWhitespaceUtf8(" /*水😀*/\n\t");
  ASSERT_TRUE(valid);
  EXPECT_EQ(output, "prefix /*水😀*/\n\t");

  // The default UTF-32 adapter also validates completely before its callback.
  struct Builder : JsonBuilderStream {
    std::u32string whitespace;
    std::expected<void, StarException> putWhitespace(std::span<char32_t const> text) override {
      whitespace.append(text.data(), text.size());
      return {};
    }
  } builder;
  auto rejected = builder.putWhitespaceUtf8(std::string_view(" /*水*/\xff"));
  ASSERT_FALSE(rejected);
  EXPECT_TRUE(rejected.error().is<UnicodeException>());
  EXPECT_TRUE(builder.whitespace.empty());
  auto accepted = builder.putWhitespaceUtf8(" /*水😀*/\n\t");
  ASSERT_TRUE(accepted);
  EXPECT_EQ(builder.whitespace, U" /*水😀*/\n\t");
}

TEST(JsonTest, Query) {
  auto result37 = Json::parse(R"JSON(
      {
        "foo" : "bar",
        "baz" : {
          "baf" : [1, 2],
          "bal" : 2
        },

        "baf" : null
      }
    )JSON");
  ASSERT_TRUE(result37);
  Json v = (*result37);

  auto result38 = v.query("foo");
  ASSERT_TRUE(result38);
  EXPECT_EQ((*result38), Json("bar"));
  auto result39 = v.query("baz.baf[1]");
  ASSERT_TRUE(result39);
  EXPECT_EQ((*result39), Json(2));
  auto result40 = v.query("baz.bal");
  ASSERT_TRUE(result40);
  EXPECT_EQ((*result40), Json(2));
  auto result41 = v.query("blargh", Json("default"));
  ASSERT_TRUE(result41);
  EXPECT_EQ((*result41), Json("default"));
  auto result42 = v.query("blargh", Json("default"));
  ASSERT_TRUE(result42);
  EXPECT_EQ((*result42), Json("default"));
  auto result43 = v.query("baz.baf[3]", Json("default"));
  ASSERT_TRUE(result43);
  EXPECT_EQ((*result43), Json("default"));
  auto result44 = v.query("baz.bal[0]", Json("default"));
  ASSERT_TRUE(result44);
  EXPECT_EQ((*result44), Json("default"));
  auto result45 = v.query("baz[1]", Json("default"));
  ASSERT_TRUE(result45);
  EXPECT_EQ((*result45), Json("default"));
  auto result46 = v.query("baz.bal.a", Json("default"));
  ASSERT_TRUE(result46);
  EXPECT_EQ((*result46), Json("default"));
  auto result47 = v.query("baz[0]", Json("default"));
  ASSERT_TRUE(result47);
  EXPECT_EQ((*result47), Json("default"));
  auto result48 = v.query("baz.baf.a", Json("default"));
  ASSERT_TRUE(result48);
  EXPECT_EQ((*result48), Json("default"));
  auto error49 = v.query("blargh");
  ASSERT_FALSE(error49);
  EXPECT_TRUE(error49.error().is<JsonPath::TraversalException>());
  auto error50 = v.query("baz.funk");
  ASSERT_FALSE(error50);
  EXPECT_TRUE(error50.error().is<JsonPath::TraversalException>());
  auto error51 = v.query("baz.baf[3]");
  ASSERT_FALSE(error51);
  EXPECT_TRUE(error51.error().is<JsonPath::TraversalException>());
  auto error52 = v.query("baz.baf[whee]", Json());
  ASSERT_FALSE(error52);
  EXPECT_TRUE(error52.error().is<JsonPath::ParsingException>());
  auto error53 = v.query("baz.baf[[]", Json());
  ASSERT_FALSE(error53);
  EXPECT_TRUE(error53.error().is<JsonPath::ParsingException>());
  auto error54 = v.query("baz..baf", Json());
  ASSERT_FALSE(error54);
  EXPECT_TRUE(error54.error().is<JsonPath::ParsingException>());
  auto error55 = v.query("baf.nothing");
  ASSERT_FALSE(error55);
  EXPECT_TRUE(error55.error().is<JsonException>());
}

TEST(JsonTest, PatchingAdd) {
  auto result56 = Json::parse(R"JSON(
      {
        "foo" : "bar",
        "baz" : {
          "baf" : 1,
          "bal" : 2
        },
        "rab" : [0, 1, 2, "foo", false]
      }
    )JSON");
  ASSERT_TRUE(result56);
  Json before = (*result56);

  auto result57 = Json::parse(R"JSON(
      {
        "foo" : "xyzzy",
        "bar" : "foo",
        "baz" : {
          "baf" : 1,
          "bal" : 2,
          "0" : "derp",
          "rebar" : {
            "after" : "party"
          }
        },
        "rab" : [0, 0, 1, 2, "foo", false, true, { "baz" : "bar"} ]
      }
    )JSON");
  ASSERT_TRUE(result57);
  Json after = (*result57);

  auto result58 = Json::parse(R"JSON(
      [
        {"op" : "add", "path" : "/foo", "value" : "xyzzy"},
        {"op" : "add", "path" : "/bar", "value" : "foo"},
        {"op" : "add", "path" : "/baz/rebar", "value" : {}},
        {"op" : "add", "path" : "/baz/rebar/after", "value" : "party"},
        {"op" : "add", "path" : "/baz/0", "value" : "derp"},
        {"op" : "add", "path" : "/rab/0", "value" : 0},
        {"op" : "add", "path" : "/rab/6", "value" : true},
        {"op" : "add", "path" : "/rab/-", "value" : {"baz" : "bar"} }
      ]
  )JSON");
  ASSERT_TRUE(result58);
  Json patch = (*result58);

  // Past end of list
  auto result59 = Json::parse(R"JSON(
      [
        {"op" : "add", "path" : "/rab/6", "value" : {"baz" : "bar"} }
      ]
  )JSON");
  ASSERT_TRUE(result59);
  Json badPatch1 = (*result59);

  // Parent does not exist, map
  auto result60 = Json::parse(R"JSON(
      [
        {"op" : "add", "path" : "/bar/baz", "value" : {"baz" : "bar"} }
      ]
  )JSON");
  ASSERT_TRUE(result60);
  Json badPatch2 = (*result60);

  // Parent does not exist, list
  auto result61 = Json::parse(R"JSON(
      [
        {"op" : "add", "path" : "/bar/0", "value" : {"baz" : "bar"} }
      ]
  )JSON");
  ASSERT_TRUE(result61);
  Json badPatch3 = (*result61);

  auto result62 = patch.toArray();
  ASSERT_TRUE(result62);
  auto result63 = jsonPatch(before, (*result62));
  ASSERT_TRUE(result63);
  EXPECT_EQ((*result63), after);
  auto result64 = badPatch1.toArray();
  ASSERT_TRUE(result64);
  auto error65 = jsonPatch(before, (*result64));
  ASSERT_FALSE(error65);
  EXPECT_TRUE(error65.error().is<JsonPatchException>());
  auto result66 = badPatch2.toArray();
  ASSERT_TRUE(result66);
  auto error67 = jsonPatch(before, (*result66));
  ASSERT_FALSE(error67);
  EXPECT_TRUE(error67.error().is<JsonPatchException>());
  auto result68 = badPatch3.toArray();
  ASSERT_TRUE(result68);
  auto error69 = jsonPatch(before, (*result68));
  ASSERT_FALSE(error69);
  EXPECT_TRUE(error69.error().is<JsonPatchException>());
}

TEST(JsonTest, PatchingRemove) {
  auto result70 = Json::parse(R"JSON(
      {
        "foo" : "xyzzy",
        "bar" : "foo",
        "baz" : {
          "baf" : 1,
          "bal" : 2,
          "rebar" : true
        },
        "rab" : [0, 0, 1, 2, "foo", false, {"baz" : "bar"} ]
      }
    )JSON");
  ASSERT_TRUE(result70);
  Json before = (*result70);

  auto result71 = Json::parse(R"JSON(
      {
        "bar" : "foo",
        "baz" : {
          "baf" : 1,
          "bal" : 2
        },
        "rab" : [0, 1, 2, "foo", false]
      }
    )JSON");
  ASSERT_TRUE(result71);
  Json after = (*result71);

  auto result72 = Json::parse(R"JSON(
      [
        {"op" : "remove", "path" : "/foo"},
        {"op" : "remove", "path" : "/baz/rebar"},
        {"op" : "remove", "path" : "/rab/0"},
        {"op" : "remove", "path" : "/rab/5"}
      ]
  )JSON");
  ASSERT_TRUE(result72);
  Json patch = (*result72);

  // Removing end of list
  auto result73 = Json::parse(R"JSON(
      [
        {"op" : "remove", "path" : "/rab/-"}
      ]
  )JSON");
  ASSERT_TRUE(result73);
  Json badPatch1 = (*result73);

  // Removing past end of list
  auto result74 = Json::parse(R"JSON(
      [
        {"op" : "add", "path" : "/rab/7"}
      ]
  )JSON");
  ASSERT_TRUE(result74);
  Json badPatch2 = (*result74);

  // Path wrong type
  auto result75 = Json::parse(R"JSON(
      [
        {"op" : "remove", "path" : "/bar/baz"}
      ]
  )JSON");
  ASSERT_TRUE(result75);
  Json badPatch3 = (*result75);

  auto result76 = patch.toArray();
  ASSERT_TRUE(result76);
  auto result77 = jsonPatch(before, (*result76));
  ASSERT_TRUE(result77);
  EXPECT_EQ((*result77), after);
  auto result78 = badPatch1.toArray();
  ASSERT_TRUE(result78);
  auto error79 = jsonPatch(before, (*result78));
  ASSERT_FALSE(error79);
  EXPECT_TRUE(error79.error().is<JsonPatchException>());
  auto result80 = badPatch2.toArray();
  ASSERT_TRUE(result80);
  auto error81 = jsonPatch(before, (*result80));
  ASSERT_FALSE(error81);
  EXPECT_TRUE(error81.error().is<JsonPatchException>());
  auto result82 = badPatch3.toArray();
  ASSERT_TRUE(result82);
  auto error83 = jsonPatch(before, (*result82));
  ASSERT_FALSE(error83);
  EXPECT_TRUE(error83.error().is<JsonPatchException>());
}

TEST(JsonTest, PatchingReplace) {
  auto result84 = Json::parse(R"JSON(
      {
        "foo" : "bar",
        "bar" : {
          "baf" : 1,
          "bal" : 2
        },
        "baz" : {
          "baf" : 1,
          "bal" : 2
        },
        "rab" : [0, 1, 2, "foo", false],
        "rabby" : [0, 1, 2, "foo", false]
      }
    )JSON");
  ASSERT_TRUE(result84);
  Json before = (*result84);

  auto result85 = Json::parse(R"JSON(
      {
        "foo" : "xyzzy",
        "bar" : [3, 2, 1, "contact"],
        "baz" : {
          "baf" : 1,
          "bal" : "touched"
        },
        "rab" : [{"omg" : "no"}, 1, 2, "foo", false],
        "rabby" : false
      }
    )JSON");
  ASSERT_TRUE(result85);
  Json after = (*result85);

  auto result86 = Json::parse(R"JSON(
      [
        {"op" : "replace", "path" : "/foo", "value" : "xyzzy"},
        {"op" : "replace", "path" : "/bar", "value" : [3, 2, 1, "contact"]},
        {"op" : "replace", "path" : "/baz/bal", "value" : "touched"},
        {"op" : "replace", "path" : "/rab/0", "value" : {"omg" : "yes"}},
        {"op" : "replace", "path" : "/rab/0/omg", "value" : "no"},
        {"op" : "replace", "path" : "/rab/2", "value" : 2},
        {"op" : "replace", "path" : "/rabby", "value" : false}
      ]
  )JSON");
  ASSERT_TRUE(result86);
  Json patch = (*result86);

  // End of list
  auto result87 = Json::parse(R"JSON(
      [
        {"op" : "replace", "path" : "/rab/-", "value" : {"baz" : "bar"} }
      ]
  )JSON");
  ASSERT_TRUE(result87);
  Json badPatch1 = (*result87);

  // Past end of list
  auto result88 = Json::parse(R"JSON(
      [
        {"op" : "replace", "path" : "/rab/5", "value" : {"baz" : "bar"} }
      ]
  )JSON");
  ASSERT_TRUE(result88);
  Json badPatch2 = (*result88);

  // Key does not exist
  auto result89 = Json::parse(R"JSON(
      [
        {"op" : "replace", "path" : "/bar/baz", "value" : {"baz" : "bar"} }
      ]
  )JSON");
  ASSERT_TRUE(result89);
  Json badPatch3 = (*result89);

  auto result90 = patch.toArray();
  ASSERT_TRUE(result90);
  auto result91 = jsonPatch(before, (*result90));
  ASSERT_TRUE(result91);
  EXPECT_EQ((*result91), after);
  auto result92 = badPatch1.toArray();
  ASSERT_TRUE(result92);
  auto error93 = jsonPatch(before, (*result92));
  ASSERT_FALSE(error93);
  EXPECT_TRUE(error93.error().is<JsonPatchException>());
  auto result94 = badPatch2.toArray();
  ASSERT_TRUE(result94);
  auto error95 = jsonPatch(before, (*result94));
  ASSERT_FALSE(error95);
  EXPECT_TRUE(error95.error().is<JsonPatchException>());
  auto result96 = badPatch3.toArray();
  ASSERT_TRUE(result96);
  auto error97 = jsonPatch(before, (*result96));
  ASSERT_FALSE(error97);
  EXPECT_TRUE(error97.error().is<JsonPatchException>());
}

TEST(JsonTest, PatchingMove) {
  auto result98 = Json::parse(R"JSON(
      {
        "foo" : "bar",
        "bar" : [1, 2, 3, "contact"],
        "baz" : {
          "baf" : 1,
          "bar" : 2
        },
        "rab" : [0, 1, 2, "foo", false],
        "rabby" : [0, 1, 2, "foo", true]
      }
    )JSON");
  ASSERT_TRUE(result98);
  Json before = (*result98);

  auto result99 = Json::parse(R"JSON(
      {
        "foot" : "bar",
        "baz" : {
          "baf" : 1,
          "bar" : [3, 2, 1, "contact"]
        },
        "bar" : 2,
        "rab" : [0, 1, 2, true, "foo"]
      }
    )JSON");
  ASSERT_TRUE(result99);
  Json after = (*result99);

  auto result100 = Json::parse(R"JSON(
      [
        {"op" : "move", "from" : "/foo", "path" : "/foot"},
        {"op" : "move", "from" : "/bar", "path" : "/baz/bal"},
        {"op" : "move", "from" : "/baz/bar", "path" : "/bar"},
        {"op" : "move", "from" : "/baz/bal", "path" : "/baz/bar"},
        {"op" : "move", "from" : "/baz/bar/0", "path" : "/baz/bar/1"},
        {"op" : "move", "from" : "/baz/bar/2", "path" : "/baz/bar/0"},
        {"op" : "move", "from" : "/rabby", "path" : "/rab"},
        {"op" : "move", "from" : "/rab/3", "path" : "/rab/-"}
      ]
  )JSON");
  ASSERT_TRUE(result100);
  Json patch = (*result100);

  // From end of list
  auto result101 = Json::parse(R"JSON(
      [
        {"op" : "move", "from" : "/rab/-", "path" : "/doesnotmatter"}
      ]
  )JSON");
  ASSERT_TRUE(result101);
  Json badPatch1 = (*result101);

  // From past end of list
  auto result102 = Json::parse(R"JSON(
      [
        {"op" : "move", "from" : "/rab/5", "path" : "/doesnotmatter"}
      ]
  )JSON");
  ASSERT_TRUE(result102);
  Json badPatch2 = (*result102);

  // To past end of list
  auto result103 = Json::parse(R"JSON(
      [
        {"op" : "move", "from" : "/rab/0", "path" : "/rab/5"}
      ]
  )JSON");
  ASSERT_TRUE(result103);
  Json badPatch3 = (*result103);

  // Source path does not exist
  auto result104 = Json::parse(R"JSON(
      [
        {"op" : "move", "from" : "/omgomg", "path" : "/doesntmatter"}
      ]
  )JSON");
  ASSERT_TRUE(result104);
  Json badPatch4 = (*result104);

  // Dest path wrong type
  auto result105 = Json::parse(R"JSON(
      [
        {"op" : "move", "from" : "/baz/bar", "path" : "/rabby/bar"}
      ]
  )JSON");
  ASSERT_TRUE(result105);
  Json badPatch5 = (*result105);

  auto result106 = patch.toArray();
  ASSERT_TRUE(result106);
  auto result107 = jsonPatch(before, (*result106));
  ASSERT_TRUE(result107);
  EXPECT_EQ((*result107), after);
  auto result108 = badPatch1.toArray();
  ASSERT_TRUE(result108);
  auto error109 = jsonPatch(before, (*result108));
  ASSERT_FALSE(error109);
  EXPECT_TRUE(error109.error().is<JsonPatchException>());
  auto result110 = badPatch2.toArray();
  ASSERT_TRUE(result110);
  auto error111 = jsonPatch(before, (*result110));
  ASSERT_FALSE(error111);
  EXPECT_TRUE(error111.error().is<JsonPatchException>());
  auto result112 = badPatch3.toArray();
  ASSERT_TRUE(result112);
  auto error113 = jsonPatch(before, (*result112));
  ASSERT_FALSE(error113);
  EXPECT_TRUE(error113.error().is<JsonPatchException>());
  auto result114 = badPatch4.toArray();
  ASSERT_TRUE(result114);
  auto error115 = jsonPatch(before, (*result114));
  ASSERT_FALSE(error115);
  EXPECT_TRUE(error115.error().is<JsonPatchException>());
  auto result116 = badPatch5.toArray();
  ASSERT_TRUE(result116);
  auto error117 = jsonPatch(before, (*result116));
  ASSERT_FALSE(error117);
  EXPECT_TRUE(error117.error().is<JsonPatchException>());
}

TEST(JsonTest, PatchingCopy) {
  auto result118 = Json::parse(R"JSON(
      {
        "foo" : "bar",
        "foot" : "bar",
        "bar" : [1, 2, 3, "contact"],
        "baz" : {
          "baf" : 1,
          "bar" : 2
        },
        "rab" : [0, 1, 2, "foo", false],
        "rabby" : [0, 1, 2, "foo", true]
      }
    )JSON");
  ASSERT_TRUE(result118);
  Json before = (*result118);

  auto result119 = Json::parse(R"JSON(
      {
        "foo" : "bar",
        "foot" : "bar",
        "baz" : {
          "baf" : 1,
          "bar" : [2, 1, 1, 2, 3, "contact"],
          "bal" : [1, 2, 3, "contact"]
        },
        "bar" : 2,
        "rab" : [0, 1, 2, "foo", true, "foo"],
        "rabby" : [0, 1, 2, "foo", true]
      }
    )JSON");
  ASSERT_TRUE(result119);
  Json after = (*result119);

  auto result120 = Json::parse(R"JSON(
      [
        {"op" : "copy", "from" : "/foo", "path" : "/foot"},
        {"op" : "copy", "from" : "/bar", "path" : "/baz/bal"},
        {"op" : "copy", "from" : "/baz/bar", "path" : "/bar"},
        {"op" : "copy", "from" : "/baz/bal", "path" : "/baz/bar"},
        {"op" : "copy", "from" : "/baz/bar/0", "path" : "/baz/bar/1"},
        {"op" : "copy", "from" : "/baz/bar/2", "path" : "/baz/bar/0"},
        {"op" : "copy", "from" : "/rabby", "path" : "/rab"},
        {"op" : "copy", "from" : "/rab/3", "path" : "/rab/-"}
      ]
  )JSON");
  ASSERT_TRUE(result120);
  Json patch = (*result120);

  // From end of list
  auto result121 = Json::parse(R"JSON(
      [
        {"op" : "copy", "from" : "/rab/-", "path" : "/doesnotmatter"}
      ]
  )JSON");
  ASSERT_TRUE(result121);
  Json badPatch1 = (*result121);

  // From past end of list
  auto result122 = Json::parse(R"JSON(
      [
        {"op" : "copy", "from" : "/rab/5", "path" : "/doesnotmatter"}
      ]
  )JSON");
  ASSERT_TRUE(result122);
  Json badPatch2 = (*result122);

  // To past end of list
  auto result123 = Json::parse(R"JSON(
      [
        {"op" : "copy", "from" : "/rab/0", "path" : "/rab/6"}
      ]
  )JSON");
  ASSERT_TRUE(result123);
  Json badPatch3 = (*result123);

  // Source path does not exist
  auto result124 = Json::parse(R"JSON(
      [
        {"op" : "copy", "from" : "/omgomg", "path" : "/doesntmatter"}
      ]
  )JSON");
  ASSERT_TRUE(result124);
  Json badPatch4 = (*result124);

  // Dest path wrong type
  auto result125 = Json::parse(R"JSON(
      [
        {"op" : "copy", "from" : "/baz/bar", "path" : "/rabby/bar"}
      ]
  )JSON");
  ASSERT_TRUE(result125);
  Json badPatch5 = (*result125);

  auto result126 = patch.toArray();
  ASSERT_TRUE(result126);
  auto result127 = jsonPatch(before, (*result126));
  ASSERT_TRUE(result127);
  EXPECT_EQ((*result127), after);
  auto result128 = badPatch1.toArray();
  ASSERT_TRUE(result128);
  auto error129 = jsonPatch(before, (*result128));
  ASSERT_FALSE(error129);
  EXPECT_TRUE(error129.error().is<JsonPatchException>());
  auto result130 = badPatch2.toArray();
  ASSERT_TRUE(result130);
  auto error131 = jsonPatch(before, (*result130));
  ASSERT_FALSE(error131);
  EXPECT_TRUE(error131.error().is<JsonPatchException>());
  auto result132 = badPatch3.toArray();
  ASSERT_TRUE(result132);
  auto error133 = jsonPatch(before, (*result132));
  ASSERT_FALSE(error133);
  EXPECT_TRUE(error133.error().is<JsonPatchException>());
  auto result134 = badPatch4.toArray();
  ASSERT_TRUE(result134);
  auto error135 = jsonPatch(before, (*result134));
  ASSERT_FALSE(error135);
  EXPECT_TRUE(error135.error().is<JsonPatchException>());
  auto result136 = badPatch5.toArray();
  ASSERT_TRUE(result136);
  auto error137 = jsonPatch(before, (*result136));
  ASSERT_FALSE(error137);
  EXPECT_TRUE(error137.error().is<JsonPatchException>());
}

TEST(JsonTest, PatchingTest) {
  auto result138 = Json::parse(R"JSON(
      {
        "foo" : "bar",
        "foot" : "bart",
        "bar" : [1, 2, 3, "contact"],
        "baz" : {
          "baf" : 1,
          "bar" : 2,
          "0" : 3
        }
      }
    )JSON");
  ASSERT_TRUE(result138);
  Json base = (*result138);

  auto result139 = Json::parse(R"JSON(
      [
        {"op" : "test", "path" : "/foo", "value" : "bar"},
        {"op" : "test", "path" : "/foo", "value" : "bark", "inverse" : true},
        {"op" : "test", "path" : "/foot", "value" : "bart"},
        {"op" : "test", "path" : "/bar", "value" : [1, 2, 3, "contact"]},
        {"op" : "test", "path" : "/bar/0", "value" : 1},
        {"op" : "test", "path" : "/bar/1", "value" : 2},
        {"op" : "test", "path" : "/bar/2", "value" : 3},
        {"op" : "test", "path" : "/bar/3", "value" : "contact"},
        {"op" : "test", "path" : "/baz", "value" : {"0" : 3, "baf" : 1, "bar" : 2}},
        {"op" : "test", "path" : "/baz/baf", "value" : 1},
        {"op" : "test", "path" : "/baz/bar", "value" : 2},
        {"op" : "test", "path" : "/baz/0", "value" : 3},
        {"op" : "test", "path" : "/nothere", "inverse" : true},
        {"op" : "test", "path" : "/foo" }
      ]
  )JSON");
  ASSERT_TRUE(result139);
  Json goodTest = (*result139);

  auto result140 = Json::parse(R"JSON(
      [
        {"op" : "test", "path" : "/bar", "value" : [1, 3, 2, "contact"]}
      ]
  )JSON");
  ASSERT_TRUE(result140);
  Json failTest1 = (*result140);

  auto result141 = Json::parse(R"JSON(
      [
        {"op" : "test", "path" : "/bar/-", "value" : "contact"}
      ]
  )JSON");
  ASSERT_TRUE(result141);
  Json failTest2 = (*result141);

  auto result142 = Json::parse(R"JSON(
      [
        {"op" : "test", "path" : "/xyzzy", "value" : null}
      ]
  )JSON");
  ASSERT_TRUE(result142);
  Json failTest3 = (*result142);

  auto result143 = Json::parse(R"JSON(
      [
        {"op" : "test", "path" : "/xyzzy/zop", "value" : null}
      ]
  )JSON");
  ASSERT_TRUE(result143);
  Json failTest4 = (*result143);

  auto result144 = Json::parse(R"JSON(
      [
        {"op" : "test", "path" : "/nothere" }
      ]
  )JSON");
  ASSERT_TRUE(result144);
  Json failTest5 = (*result144);

  auto result145 = Json::parse(R"JSON(
      [
        {"op" : "test", "path" : "/bar", "inverse" : true }
      ]
  )JSON");
  ASSERT_TRUE(result145);
  Json failTest6 = (*result145);

  auto result146 = Json::parse(R"JSON(
      [
        {"op" : "test", "path" : "/foo", "value" : "bar", "inverse" : true }
      ]
  )JSON");
  ASSERT_TRUE(result146);
  Json failTest7 = (*result146);

  auto result147 = goodTest.toArray();
  ASSERT_TRUE(result147);
  auto result148 = jsonPatch(base, (*result147));
  ASSERT_TRUE(result148);

  auto result149 = failTest1.toArray();
  ASSERT_TRUE(result149);
  auto error150 = jsonPatch(base, (*result149));
  ASSERT_FALSE(error150);
  EXPECT_TRUE(error150.error().is<JsonPatchTestFail>());
  auto result151 = failTest2.toArray();
  ASSERT_TRUE(result151);
  auto error152 = jsonPatch(base, (*result151));
  ASSERT_FALSE(error152);
  EXPECT_TRUE(error152.error().is<JsonPatchTestFail>());
  auto result153 = failTest3.toArray();
  ASSERT_TRUE(result153);
  auto error154 = jsonPatch(base, (*result153));
  ASSERT_FALSE(error154);
  EXPECT_TRUE(error154.error().is<JsonPatchTestFail>());
  auto result155 = failTest4.toArray();
  ASSERT_TRUE(result155);
  auto error156 = jsonPatch(base, (*result155));
  ASSERT_FALSE(error156);
  EXPECT_TRUE(error156.error().is<JsonPatchTestFail>());
  auto result157 = failTest5.toArray();
  ASSERT_TRUE(result157);
  auto error158 = jsonPatch(base, (*result157));
  ASSERT_FALSE(error158);
  EXPECT_TRUE(error158.error().is<JsonPatchTestFail>());
  auto result159 = failTest6.toArray();
  ASSERT_TRUE(result159);
  auto error160 = jsonPatch(base, (*result159));
  ASSERT_FALSE(error160);
  EXPECT_TRUE(error160.error().is<JsonPatchTestFail>());
  auto result161 = failTest7.toArray();
  ASSERT_TRUE(result161);
  auto error162 = jsonPatch(base, (*result161));
  ASSERT_FALSE(error162);
  EXPECT_TRUE(error162.error().is<JsonPatchTestFail>());
}

TEST(JsonTest, PatchingEscaping) {
  auto result163 = Json::parse(R"JSON(
      {
        "~" : true,
        "/" : false,
        "~~0" : "foo",
        "~~1" : "bar",
        "~~0~1/~0~" : "ugh"
      }
  )JSON");
  ASSERT_TRUE(result163);
  Json base1 = (*result163);

  auto result164 = Json::parse(R"JSON(
      [
        {"op" : "test", "path" : "/~0", "value" : true},
        {"op" : "test", "path" : "/~1", "value" : false},
        {"op" : "test", "path" : "/~0~00", "value" : "foo"},
        {"op" : "test", "path" : "/~0~01", "value" : "bar"},
        {"op" : "test", "path" : "/~0~00~01~1~00~0", "value" : "ugh"}
      ]
  )JSON");
  ASSERT_TRUE(result164);
  Json test1 = (*result164);

  auto result165 = test1.toArray();
  ASSERT_TRUE(result165);
  auto result166 = jsonPatch(base1, (*result165));
  ASSERT_TRUE(result166);
}

TEST(JsonTest, MergeQuery) {
  auto result167 = Json::parse(R"JSON(
      {
        "foo" : "foo1",
        "bar" : "bar1",
        "baz" : {
          "1" : "1"
        },
        "fob" : {},
        "fizz" : 4
      }
    )JSON");
  ASSERT_TRUE(result167);
  Json json1 = (*result167);
  auto result168 = Json::parse(R"JSON(
      {
        "foo" : "foo2",
        "bar" : "bar2",
        "baz" : null,
        "baf" : {
          "2" : "2"
        },
        "fob" : 2
      }
    )JSON");
  ASSERT_TRUE(result168);
  Json json2 = (*result168);
  auto result169 = Json::parse(R"JSON(
      {
        "baz" : {
          "3" : "3"
        },
        "baf" : {
          "3" : "3"
        },
        "fizz" : {
        }
      }
    )JSON");
  ASSERT_TRUE(result169);
  Json json3 = (*result169);

  auto testIdentical = [&](String const& key) {
    auto result170 = jsonMergeQuery(key, json1, json2, json3);
    ASSERT_TRUE(result170);
    auto result171 = jsonMerge(json1, json2, json3).query(key, {});
    ASSERT_TRUE(result171);
    EXPECT_EQ((*result170), (*result171));
  };

  testIdentical("foo");
  testIdentical("bar");
  testIdentical("baz");
  testIdentical("baf");
  testIdentical("baz.1");
  testIdentical("baz.2");
  testIdentical("baz.3");
  testIdentical("baf.0");
  testIdentical("baf.2");
  testIdentical("baf.3");
  testIdentical("baz.blip");
  testIdentical("boo.blip");
  testIdentical("fob");
  testIdentical("fiz");
  testIdentical("nothing");
}
