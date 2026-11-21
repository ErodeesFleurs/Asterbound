import std;
import star.string;
import star.byte_array;
import star.encode;
import star.bytes;
import star.format;
import star.shell_parser;
import star.option_parser;
import star.numeric_option;
import star.map;
import star.list;
import star.ordered_map;
import star.ordered_set;

#include "gtest/gtest.h"

TEST(ShellParser, Simple) {
  Star::ShellParser parser;
  auto result1 = parser.tokenize("The first test.");
  ASSERT_TRUE(result1);
  auto tokenSet1 = (*result1);

  ASSERT_EQ(tokenSet1.size(), 3u);
  EXPECT_EQ(tokenSet1[0].token, "The");
  EXPECT_EQ(tokenSet1[1].token, "first");
  EXPECT_EQ(tokenSet1[2].token, "test.");
}

TEST(ShellParser, DirectUnicode) {
  Star::ShellParser parser;
  auto result2 = parser.tokenize("Unicode Symbols: ❤ ☀ ☆ ☂ ☻ ♞ ☯ ☭ ☢ € → ☎ ❄ ♫ ✂ ▷ ✇ ♎ ⇧ ☮ ♻ ⌘ ⌛ ☘");
  ASSERT_TRUE(result2);
  auto tokenSet2 = (*result2);

  ASSERT_EQ(tokenSet2.size(), 26u);
  EXPECT_EQ(tokenSet2[0].token, "Unicode");
  EXPECT_EQ(tokenSet2[1].token, "Symbols:");
  EXPECT_EQ(tokenSet2[10].token, "☢");
}

TEST(ShellParser, SimpleQuotes) {
  Star::ShellParser parser;
  auto result3 = parser.tokenize("\"This is a test\" 'This is another test'");
  ASSERT_TRUE(result3);
  auto tokenSet3 = (*result3);

  ASSERT_EQ(tokenSet3.size(), 2u);
  EXPECT_EQ(tokenSet3[0].token, "This is a test");
  EXPECT_EQ(tokenSet3[1].token, "This is another test");
}

TEST(ShellParser, ComplexQuotes) {
  Star::ShellParser parser;
  auto result4 = parser.tokenize("\"'asdf' 'asdf asdf'\" '\"omg\" omg omg'");
  ASSERT_TRUE(result4);
  auto tokenSet4 = (*result4);

  ASSERT_EQ(tokenSet4.size(), 2u);
  EXPECT_EQ(tokenSet4[0].token, "'asdf' 'asdf asdf'");
  EXPECT_EQ(tokenSet4[1].token, "\"omg\" omg omg");
}

TEST(ShellParser, SpacelessQuotes) {
  Star::ShellParser parser;
  auto result5 = parser.tokenize("\"asdf\"asdf asdf");
  ASSERT_TRUE(result5);
  auto tokenSet5 = (*result5);

  ASSERT_EQ(tokenSet5.size(), 2u);
  EXPECT_EQ(tokenSet5[0].token, "asdfasdf");
  EXPECT_EQ(tokenSet5[1].token, "asdf");

  auto result6 = parser.tokenize("'asdf'asdf asdf");
  ASSERT_TRUE(result6);
  auto tokenSet6 = (*result6);

  ASSERT_EQ(tokenSet6.size(), 2u);
  EXPECT_EQ(tokenSet6[0].token, "asdfasdf");
  EXPECT_EQ(tokenSet6[1].token, "asdf");
}

TEST(ShellParser, EscapedSpaces) {
  Star::ShellParser parser;
  auto result7 = parser.tokenize("This\\ is\\ a test");
  ASSERT_TRUE(result7);
  auto tokenSet7 = (*result7);

  ASSERT_EQ(tokenSet7.size(), 2u);
  EXPECT_EQ(tokenSet7[0].token, "This is a");
  EXPECT_EQ(tokenSet7[1].token, "test");
}

TEST(ShellParser, EscapedUnicode) {
  Star::ShellParser parser;
  auto result8 = parser.tokenize("This is a unicode codepoint: \\u2603");
  ASSERT_TRUE(result8);
  auto tokenSet8 = (*result8);

  ASSERT_EQ(tokenSet8.size(), 6u);
  EXPECT_EQ(tokenSet8[0].token, "This");
  EXPECT_EQ(tokenSet8[5].token, "☃");

  auto result9 = parser.tokenize("\\u");
  ASSERT_TRUE(result9);
  auto tokenSet9 = (*result9);

  ASSERT_EQ(tokenSet9.size(), 1u);
  EXPECT_EQ(tokenSet9[0].token, "u");

  auto result10 = parser.tokenize("\\u2603\\u2603\\u2603 \\u2603");
  ASSERT_TRUE(result10);
  auto tokenSet10 = (*result10);

  ASSERT_EQ(tokenSet10.size(), 2u);
  EXPECT_EQ(tokenSet10[0].token, "☃☃☃");
  EXPECT_EQ(tokenSet10[1].token, "☃");
}

TEST(OptionParser, ConsumesParameterValueBeforeRecognizingFlags) {
  Star::OptionParser parser;
  ASSERT_TRUE(parser.add_parameter("value", "text", Star::OptionParser::Optional));
  ASSERT_TRUE(parser.add_switch("quiet"));
  parser.add_argument("arguments", Star::OptionParser::Multiple);

  auto result = parser.parse_options({"-value", "--", "-quiet", "--", "-quiet", "tail"});
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->errors.empty());
  auto values = result->options.parameters.get("value");
  ASSERT_TRUE(values);
  EXPECT_EQ(values->get(), (Star::StringList{"--"}));
  EXPECT_TRUE(result->options.switches.contains("quiet"));
  EXPECT_EQ(result->options.arguments, (Star::StringList{"-quiet", "tail"}));
}

TEST(OptionParser, DuplicateParameterStillConsumesItsValue) {
  Star::OptionParser parser;
  ASSERT_TRUE(parser.add_parameter("value", "text", Star::OptionParser::Optional));
  ASSERT_TRUE(parser.add_switch("quiet"));
  parser.add_argument("arguments", Star::OptionParser::Multiple);

  auto result = parser.parse_options({"-value", "first", "-value", "-quiet", "tail", "-value"});
  ASSERT_TRUE(result);
  EXPECT_EQ(result->errors, (Star::StringList{
      "Option with argument '-value' specified multiple times",
      "Option '-value' must be followed by an argument"}));
  auto values = result->options.parameters.get("value");
  ASSERT_TRUE(values);
  EXPECT_EQ(values->get(), (Star::StringList{"first"}));
  EXPECT_TRUE(result->options.switches.empty());
  EXPECT_EQ(result->options.arguments, (Star::StringList{"tail"}));
}

TEST(OptionParser, KeepsAllTokensAfterFirstStandaloneDelimiter) {
  Star::OptionParser parser;
  ASSERT_TRUE(parser.add_switch("quiet"));
  parser.add_argument("files", Star::OptionParser::Multiple);

  auto result = parser.parse_options({"before", "--", "--", "-quiet", "", "file"});
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->errors.empty());
  EXPECT_TRUE(result->options.switches.empty());
  EXPECT_EQ(result->options.arguments, (Star::StringList{"before", "--", "-quiet", "", "file"}));
}

namespace {
template <typename T>
concept can_borrow_parameter = requires(T&& options) {
  { std::forward<T>(options).first_parameter("value") } -> std::same_as<std::optional<std::reference_wrapper<Star::String const>>>;
};

static_assert(can_borrow_parameter<Star::OptionParser::Options&>);
static_assert(can_borrow_parameter<Star::OptionParser::Options const&>);
static_assert(!can_borrow_parameter<Star::OptionParser::Options>);
static_assert(!can_borrow_parameter<Star::OptionParser::Options const>);
}

TEST(OptionParser, FirstParameterBorrowsStoredValue) {
  Star::OptionParser parser;
  ASSERT_TRUE(parser.add_parameter("value", "text", Star::OptionParser::Multiple));
  auto result = parser.parse_options({"-value", "first", "-value", "second"});
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->errors.empty());

  auto& options = result->options;
  EXPECT_FALSE(options.first_parameter("missing"));
  options.parameters.try_emplace("empty");
  EXPECT_FALSE(options.first_parameter("empty"));
  auto values = options.parameters.get("value");
  ASSERT_TRUE(values);
  EXPECT_EQ(values->get(), (Star::StringList{"first", "second"}));
  auto first = options.first_parameter("value");
  ASSERT_TRUE(first);
  EXPECT_EQ(&first->get(), &values->get().front());
}

TEST(OptionParser, ReportsMissingRequiredParametersInDeclarationOrder) {
  Star::OptionParser parser;
  ASSERT_TRUE(parser.add_parameter("second", "text", Star::OptionParser::Required));
  ASSERT_TRUE(parser.add_parameter("first", "text", Star::OptionParser::Required));
  auto result = parser.parse_options({"-unknown", "-first"});
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->options.parameters.empty());
  EXPECT_EQ(result->errors, (Star::StringList{
      "No such option '-unknown'", "Option '-first' must be followed by an argument",
      "Missing required flag with argument '-second'", "Missing required flag with argument '-first'"}));
}

TEST(OptionParser, NumericOptionDistinguishesMissingEmptyAndInvalidValues) {
  Star::OptionParser::Options options;
  for (auto policy : {Star::EmptyParameterList::UseFallback, Star::EmptyParameterList::Reject}) {
    auto missing = Star::numeric_option<int>(options, "absent", 17, policy);
    ASSERT_TRUE(missing);
    EXPECT_EQ(*missing, 17);
  }
  auto& numbers = options.parameters["number"];
  auto empty = Star::numeric_option<int>(options, "number", 17, Star::EmptyParameterList::UseFallback);
  ASSERT_TRUE(empty);
  EXPECT_EQ(*empty, 17);
  EXPECT_FALSE(Star::numeric_option<int>(options, "number", 17, Star::EmptyParameterList::Reject));

  numbers.push_back("not-a-number");
  for (auto policy : {Star::EmptyParameterList::UseFallback, Star::EmptyParameterList::Reject})
    EXPECT_FALSE(Star::numeric_option<int>(options, "number", 17, policy));
  numbers.front() = "0";
  for (auto policy : {Star::EmptyParameterList::UseFallback, Star::EmptyParameterList::Reject}) {
    auto zero = Star::numeric_option<int>(options, "number", 17, policy);
    ASSERT_TRUE(zero);
    EXPECT_EQ(*zero, 0);
  }
}
