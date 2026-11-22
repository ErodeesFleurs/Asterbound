import std;
import star.string;
import star.unicode;
import star.shell_parser;
import star.option_parser;
import star.numeric_option;
import star.map;
import star.list;
import star.ordered_map;
import star.ordered_set;

#include "gtest/gtest.h"

namespace {
void expect_words(Star::String const& command, Star::StringList const& expected) {
  SCOPED_TRACE(command.utf8());
  auto words = Star::split_shell_words(command);
  ASSERT_TRUE(words);
  EXPECT_EQ(*words, expected);
}
}

TEST(ShellParser, Simple) {
  expect_words("The first test.", {"The", "first", "test."});
}

TEST(ShellParser, DirectUnicode) {
  expect_words("Unicode Symbols: ❤ ☀ ☆ ☂ ☻ ♞ ☯ ☭ ☢ € → ☎ ❄ ♫ ✂ ▷ ✇ ♎ ⇧ ☮ ♻ ⌘ ⌛ ☘",
      {"Unicode", "Symbols:", "❤", "☀", "☆", "☂", "☻", "♞", "☯", "☭", "☢", "€", "→", "☎", "❄",
          "♫", "✂", "▷", "✇", "♎", "⇧", "☮", "♻", "⌘", "⌛", "☘"});
}

TEST(ShellParser, EmptyArgumentsAndSeparators) {
  expect_words("", {});
  expect_words(" \t\r\n\uFEFF ", {});
  expect_words("  a\uFEFFb \t c  ", {"a", "b", "c"});
  expect_words("'' \"\" x ''", {"", "", "x", ""});
  expect_words(R"(''""   )", {""});
  expect_words("a\vb\fc '\uFEFF' \\\uFEFF", {"a\vb\fc", "\uFEFF", "\uFEFF"});
}

TEST(ShellParser, SimpleQuotes) {
  expect_words(R"("This is a test" 'This is another test')", {"This is a test", "This is another test"});
}

TEST(ShellParser, ComplexQuotes) {
  expect_words(R"("'asdf' 'asdf asdf'" '"omg" omg omg')", {"'asdf' 'asdf asdf'", R"("omg" omg omg)"});
}

TEST(ShellParser, SpacelessQuotes) {
  expect_words(R"("asdf"asdf 'asdf'asdf asdf)", {"asdfasdf", "asdfasdf", "asdf"});
  expect_words(R"(a""b x'y z'"!")", {"ab", "xy z!"});
}

TEST(ShellParser, UnmatchedQuotesExtendToEnd) {
  expect_words("one 'two three", {"one", "two three"});
  expect_words("one \"", {"one", ""});
}

TEST(ShellParser, EscapedSpaces) {
  expect_words(R"(This\ is\ a test)", {"This is a", "test"});
}

TEST(ShellParser, EscapesInsideAndOutsideQuotes) {
  expect_words(R"('\n\t\r\b\v\f\a' \\ \" \' \q \水 end\)", {"\n\t\r\b\v\f\a", "\\", "\"", "'", "q", "水", "end\\"});
}

TEST(ShellParser, EmbeddedNulIsData) {
  using namespace std::string_view_literals;
  expect_words(Star::String("a\0b \\0 \\u0000z end"sv),
      {Star::String("a\0b"sv), Star::String("\0"sv), Star::String("\0z"sv), "end"});
}

TEST(ShellParser, EscapedUnicode) {
  expect_words(R"(This is a unicode codepoint: \u2603)", {"This", "is", "a", "unicode", "codepoint:", "☃"});
  expect_words(R"(\u)", {"u"});
  expect_words(R"(\u2603\u2603\u2603 \u2603)", {"☃☃☃", "☃"});
  expect_words(R"(\u0020 \u0027 \u005c)", {" ", "'", "\\"});
}

TEST(ShellParser, MalformedEscapesPreserveTheFollowingInput) {
  expect_words(R"(\u12 next \u123)", {"�", "next", "�"});
  expect_words(R"("a\u12" 'b\u3' \u0'c d')", {"a�", "b�", "�c d"});
  expect_words(R"(\u12\u2603 \u123水 \u0ａBC \ug)", {"�☃", "�水", "�ａBC", "�g"});
}

TEST(ShellParser, EscapedSurrogatesRequireAnAdjacentPair) {
  expect_words(R"(\ud83D\uDE00 \udbff\udfff)", {"😀", "\U0010FFFF"});
  expect_words(R"(\uDC00 \uD800x \uD800)", {"�", "�x", "�"});
  expect_words(R"(\uD800\u2603 \uD800\uD83D\uDE00)", {"�☃", "�😀"});
  expect_words(R"(\uD800\u12 next)", {"��", "next"});
}

TEST(ShellParser, InvalidUtf8PropagatesTheUnicodeError) {
  using namespace std::string_view_literals;
  for (auto bytes : {"\xff"sv, "valid \"unterminated \xc2"sv, "\\u12\x80"sv, "nul\0\xff"sv}) {
    auto words = Star::split_shell_words(Star::String(bytes));
    ASSERT_FALSE(words);
    EXPECT_TRUE(words.error().is<Star::UnicodeException>());
  }
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
