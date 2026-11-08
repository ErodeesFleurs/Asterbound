import std;
import star.memory;
import star.string;
import star.string_view;
import star.unicode;
import star.format;

#include "gtest/gtest.h"

using namespace Star;

namespace {
template <class Iterator>
concept has_utf8_decoder = requires { typename U8ToU32Iterator<Iterator>; };

template <class Iterator>
concept has_utf8_writer = requires { typename Utf8OutputIterator<Iterator>; };

template <class Owner>
concept can_make_cursor_end = requires(Owner&& owner) { Utf8Cursor::end(std::forward<Owner>(owner)); };

struct split_record {
  std::string bytes;
  std::size_t offset;
  std::size_t count;
  bool operator==(split_record const&) const = default;
};

struct move_only_split_callback {
  std::unique_ptr<int> calls = std::make_unique<int>(0);
  std::vector<split_record>& records;

  explicit move_only_split_callback(std::vector<split_record>& output) : records(output) {}
  move_only_split_callback(move_only_split_callback&&) = default;
  move_only_split_callback(move_only_split_callback const&) = delete;
  explicit operator bool() const { return false; }

  std::expected<void, StarException> operator()(StringView const& part, std::size_t const& offset, std::size_t count) & {
    ++*calls;
    records.push_back({std::string(part.utf8()), offset, count});
    return {};
  }
};

struct move_only_byte_sink {
  std::unique_ptr<int> writes = std::make_unique<int>(0);
  std::string& output;

  explicit move_only_byte_sink(std::string& bytes) : output(bytes) {}
  move_only_byte_sink(move_only_byte_sink&&) = default;
  move_only_byte_sink(move_only_byte_sink const&) = delete;
  move_only_byte_sink& operator*() { return *this; }
  move_only_byte_sink& operator++(int) { return *this; }
  void operator=(char byte) { ++*writes; output.push_back(byte); }
};

static_assert(std::same_as<std::iter_reference_t<StringView::const_iterator>, Utf32Type>);
static_assert(std::bidirectional_iterator<StringView::const_iterator>);
static_assert(std::same_as<std::iterator_traits<StringView::const_iterator>::iterator_category, std::input_iterator_tag>);
static_assert(!std::ranges::range<StringView>);
static_assert(!std::ranges::range<String>);
static_assert(has_utf8_decoder<char const*>);
static_assert(has_utf8_decoder<std::list<unsigned char>::const_iterator>);
static_assert(!has_utf8_decoder<std::forward_list<char>::const_iterator>);
static_assert(!has_utf8_decoder<std::istreambuf_iterator<char>>);
static_assert(has_utf8_writer<std::back_insert_iterator<std::string>>);
static_assert(has_utf8_writer<move_only_byte_sink>);
static_assert(!has_utf8_writer<char const*>);
static_assert(split_view_callback<move_only_split_callback>);
static_assert(!split_view_callback<decltype([](StringView, std::size_t, std::size_t) {})>);
static_assert(!split_view_callback<decltype([](StringView&, std::size_t, std::size_t) -> std::expected<void, StarException> { return {}; })>);
static_assert(split_view_callback<decltype([](StringView&&, std::size_t, std::size_t) -> std::expected<void, StarException> { return {}; })>);
static_assert(split_view_callback<decltype([](StringView, std::size_t&&, std::size_t&&)
    -> std::expected<void, StarException> { return {}; })>);
static_assert(!split_view_callback<decltype([](StringView, std::size_t&, std::size_t&)
    -> std::expected<void, StarException> { return {}; })>);
static_assert(std::same_as<decltype(std::declval<StringView const&>().utf8()), std::string_view>);
static_assert(std::constructible_from<StringView, String&>);
static_assert(std::constructible_from<StringView, String const&>);
static_assert(!std::constructible_from<StringView, String&&>);
static_assert(!std::constructible_from<StringView, String const&&>);
static_assert(std::constructible_from<StringView, std::string&>);
static_assert(std::constructible_from<StringView, std::string const&>);
static_assert(!std::constructible_from<StringView, std::string&&>);
static_assert(!std::constructible_from<StringView, std::string const&&>);
static_assert(std::constructible_from<StringView, std::string_view&&>);
static_assert(std::constructible_from<StringView, std::string_view const&&>);
static_assert(std::constructible_from<Utf8Cursor, std::string&>);
static_assert(!std::constructible_from<Utf8Cursor, std::string&&>);
static_assert(!std::constructible_from<Utf8Cursor, std::string const&&>);
static_assert(std::constructible_from<Utf8Cursor, std::string_view&&>);
static_assert(can_make_cursor_end<std::string&>);
static_assert(!can_make_cursor_end<std::string>);
static_assert(!can_make_cursor_end<std::string const>);
static_assert(can_make_cursor_end<std::string_view>);
}

TEST(StringViewStorageTest, CodepointValuesOutliveIteratorChangesAndDestruction) {
  StringView view("Aé日");
  auto first = view.begin();
  ASSERT_TRUE(first);
  std::optional<StringView::const_iterator> cursor(*first);
  auto const& saved = **cursor;
  auto copy = *cursor;
  ++*cursor;
  EXPECT_EQ(saved, U'A');
  EXPECT_EQ(**cursor, U'é');
  EXPECT_EQ(*copy, U'A');
  ++copy;
  ++copy;
  EXPECT_EQ(*copy, U'日');
  EXPECT_EQ(saved, U'A');
  cursor.reset();
  EXPECT_EQ(saved, U'A');
  EXPECT_EQ(*copy, U'日');
}

TEST(StringViewStorageTest, StandardAndReverseAlgorithmsConsumeCodepointValues) {
  StringView view("Aé日");
  auto first = view.begin();
  ASSERT_TRUE(first);
  std::u32string forward;
  std::copy(*first, view.end(), std::back_inserter(forward));
  EXPECT_EQ(forward, U"Aé日");
  std::u32string reverse;
  std::copy(std::make_reverse_iterator(view.end()), std::make_reverse_iterator(*first), std::back_inserter(reverse));
  EXPECT_EQ(reverse, U"日éA");
  EXPECT_EQ(std::ranges::distance(*first, view.end()), 3);
  EXPECT_EQ(*std::ranges::find(*first, view.end(), U'é'), U'é');

  std::list<unsigned char> bytes{0x41, 0xc3, 0xa9, 0xe6, 0x97, 0xa5};
  using decoder = U8ToU32Iterator<decltype(bytes.cbegin())>;
  auto linked_first = decoder::create(bytes.cbegin(), bytes.cend());
  ASSERT_TRUE(linked_first);
  std::u32string linked;
  std::ranges::copy(*linked_first, decoder::end(bytes.cend()), std::back_inserter(linked));
  EXPECT_EQ(linked, forward);
  auto linked_last = decoder::end(bytes.cend());
  EXPECT_EQ(*--linked_last, U'日');
}

TEST(StringViewStorageTest, ValueDescriptorSurvivesTemporaryViewWithLivingOwner) {
  std::string owner("a\0é", 4);
  auto const& descriptor = StringView(owner).utf8();
  EXPECT_EQ(descriptor, std::string_view(owner));
  EXPECT_EQ(descriptor.data(), owner.data());
  owner[0] = 'z';
  EXPECT_EQ(descriptor, std::string_view("z\0é", 4));

  String string_owner(std::string_view("x\0日", 5));
  auto const& string_descriptor = StringView(string_owner).utf8();
  EXPECT_EQ(string_descriptor.data(), string_owner.utf8_data());
  EXPECT_EQ(string_descriptor, std::string_view("x\0日", 5));
  StringView from_descriptor(std::string_view(owner.data(), owner.size()));
  EXPECT_EQ(from_descriptor.utf8(), descriptor);
  auto extracted = from_descriptor.take_utf8();
  EXPECT_TRUE(from_descriptor.empty());
  EXPECT_EQ(extracted, descriptor);
}

TEST(StringViewStorageTest, CheckedWriterAcceptsMoveOnlyOutputAndStopsBeforeInvalidCodepoint) {
  std::string bytes;
  Utf8OutputIterator<move_only_byte_sink> writer{move_only_byte_sink(bytes)};
  ASSERT_TRUE(writer.push(U'A'));
  ASSERT_TRUE(writer.push(U'é'));
  ASSERT_TRUE(writer.push(U'\0'));
  // Surrogates intentionally retain the existing permissive codec policy.
  ASSERT_TRUE(writer.push(static_cast<Utf32Type>(0xd800)));
  EXPECT_EQ(bytes, std::string("Aé\0\xed\xa0\x80", 7));
  auto before = bytes;
  auto invalid = writer.push(static_cast<Utf32Type>(0x110000));
  ASSERT_FALSE(invalid);
  EXPECT_TRUE(invalid.error().is<UnicodeException>());
  EXPECT_EQ(bytes, before);
  ASSERT_TRUE(writer.push(U'日'));
  EXPECT_EQ(bytes, before + "日");
}

TEST(StringViewStorageTest, SplitBorrowsMoveOnlyCallbacksAndPreservesByteOffsets) {
  StringView view(std::string_view("a\0,b,", 5));
  std::vector<split_record> records;
  move_only_split_callback callback(records);
  ASSERT_TRUE(view.for_each_split_any(std::string_view("\0,", 2), callback));
  EXPECT_EQ(*callback.calls, 4);
  EXPECT_EQ(records, (std::vector<split_record>{{"a", 0, 1}, {"", 2, 0}, {"b", 3, 1}, {"", 5, 0}}));

  records.clear();
  ASSERT_TRUE(StringView("a::b::").for_each_split("::", move_only_split_callback(records)));
  EXPECT_EQ(records, (std::vector<split_record>{{"a", 0, 1}, {"b", 3, 1}, {"", 6, 0}}));

  records.clear();
  ASSERT_TRUE(StringView("aé日").for_each_split_any(std::string_view("\xc3", 1),
      [state = std::make_unique<int>(0), &records](StringView part, std::size_t offset, std::size_t count) mutable -> std::expected<void, StarException> {
        ++*state;
        records.push_back({std::string(part.utf8()), offset, count});
        return {};
      }));
  EXPECT_EQ(records, (std::vector<split_record>{{"a", 0, 1}, {std::string("\xa9日", 4), 2, 4}}));
}

TEST(StringViewStorageTest, SplitPassesIndependentValueMetadataToRvalueReferenceCallbacks) {
  std::vector<split_record> records;
  auto callback = [&records](StringView part, std::size_t&& offset, std::size_t&& count)
      -> std::expected<void, StarException> {
    records.push_back({std::string(part.utf8()), offset, count});
    offset = NPos;
    count = NPos;
    return {};
  };
  ASSERT_TRUE(StringView("a|bc|").for_each_split("|", callback));
  EXPECT_EQ(records, (std::vector<split_record>{{"a", 0, 1}, {"bc", 2, 2}, {"", 5, 0}}));
}

TEST(StringViewStorageTest, SplitPreservesEmptyCallbackAndFirstErrorOrder) {
  using callback_type = std::expected<void, StarException>(StringView, std::size_t, std::size_t);
  callback_type* null_callback = nullptr;
  std::function<callback_type> empty_callback;
  for (bool any : {false, true}) {
    auto invoke = [&](StringView pattern, auto&& callback) {
      return any ? StringView("a,b,c").for_each_split_any(pattern, callback)
                 : StringView("a,b,c").for_each_split(pattern, callback);
    };
    auto missing_function = invoke(",", empty_callback);
    ASSERT_FALSE(missing_function);
    EXPECT_TRUE(missing_function.error().is<StringException>());
    auto missing_pointer = invoke(",", null_callback);
    ASSERT_FALSE(missing_pointer);
    EXPECT_TRUE(missing_pointer.error().is<StringException>());
    auto missing_literal = invoke(",", nullptr);
    ASSERT_FALSE(missing_literal);
    EXPECT_TRUE(missing_literal.error().is<StringException>());
    EXPECT_TRUE(invoke("", empty_callback));
    EXPECT_TRUE(invoke("", null_callback));
    EXPECT_TRUE(invoke("", nullptr));

    std::vector<std::string> visited;
    auto result = invoke(",", [state = std::make_unique<int>(0), &visited](StringView part, std::size_t, std::size_t) mutable -> std::expected<void, StarException> {
      visited.emplace_back(part.utf8());
      if (++*state == 2)
        return std::unexpected(OutOfRangeException("first split failure", false));
      return {};
    });
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().is<OutOfRangeException>());
    EXPECT_EQ(visited, (std::vector<std::string>{"a", "b"}));
  }
}

TEST(StringViewStorageTest, CursorSearchPreservesLazyShortCircuitAndFirstDecodeError) {
  StringView malformed(std::string_view("a\xff", 2));
  auto first = malformed.begin();
  ASSERT_FALSE(first);
  EXPECT_TRUE(first.error().is<UnicodeException>());
  EXPECT_EQ(malformed.find(U'a'), 0u);
  EXPECT_EQ(malformed.find("a"), 0u);
  EXPECT_EQ(malformed.find_first_of("a"), 0u);
  EXPECT_EQ(malformed.find_first_not_of("b"), 0u);
  auto absent = malformed.find(U'b');
  ASSERT_FALSE(absent);
  EXPECT_TRUE(absent.error().is<UnicodeException>());
  auto last = malformed.find_last("a");
  ASSERT_FALSE(last);
  EXPECT_TRUE(last.error().is<UnicodeException>());
  EXPECT_EQ(StringView("a").find(std::string_view("b\xff", 2)), NPos);
  EXPECT_EQ(StringView("a").find_first_of(std::string_view("a\xff", 2)), 0u);

  StringView invalid_source(std::string_view("\xff", 1));
  StringView invalid_pattern(std::string_view("\xc2", 1));
  Utf8Cursor pattern_cursor(invalid_pattern.utf8());
  auto pattern_error = pattern_cursor.read();
  ASSERT_FALSE(pattern_error);
  auto search_error = invalid_source.find(invalid_pattern);
  ASSERT_FALSE(search_error);
  EXPECT_EQ(std::string(search_error.error().what()), std::string(pattern_error.error().what()));
}
