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

struct codepoint_record {
  char32_t point;
  std::size_t offset;
  std::size_t width;
  bool operator==(codepoint_record const&) const = default;
};

struct move_only_codepoint_visitor {
  std::unique_ptr<int> calls = std::make_unique<int>(0);
  std::vector<codepoint_record>& records;
  explicit move_only_codepoint_visitor(std::vector<codepoint_record>& output) : records(output) {}
  move_only_codepoint_visitor(move_only_codepoint_visitor&&) = default;
  move_only_codepoint_visitor(move_only_codepoint_visitor const&) = delete;
  explicit operator bool() const { return false; }
  void operator()(char32_t&& point, std::size_t&& offset, std::size_t&& width) & {
    ++*calls;
    records.push_back({point, offset, width});
    point = U'X';
    offset = width = 99;
  }
};

// Deliberately not regular or a standard iterator: decoding needs only copies,
// contextual comparisons, byte conversion and bidirectional stepping.
struct observed_byte_iterator {
  struct proxy { char value; operator char() const { return value; } };
  char const* position;
  std::size_t* reads;
  observed_byte_iterator(char const* bytes, std::size_t& count) : position(bytes), reads(&count) {}
  proxy operator*() { ++*reads; return {*position}; }
  void operator++() { ++position; }
  void operator--() { --position; }
  bool operator==(observed_byte_iterator const& other) const { return position == other.position; }
};

static_assert(codepoint_visitor<move_only_codepoint_visitor>);
static_assert(codepoint_visitor<decltype([](char32_t, std::size_t, std::size_t) {})>);
static_assert(codepoint_visitor<decltype([](char32_t, std::size_t, std::size_t)
    -> std::expected<void, StarException> { return {}; })>);
static_assert(!codepoint_visitor<decltype([](char32_t&, std::size_t, std::size_t) {})>);
static_assert(!codepoint_visitor<decltype([](char32_t, std::size_t&, std::size_t) {})>);
static_assert(!codepoint_visitor<decltype([](char32_t, std::size_t, std::size_t) { return true; })>);
static_assert(!codepoint_visitor<decltype([](char32_t, std::size_t, std::size_t)
    -> std::expected<int, StarException> { return 1; })>);
static_assert(!codepoint_visitor<decltype([](char32_t, std::size_t, std::size_t)
    -> std::expected<void, StarException>& {
      static std::expected<void, StarException> result;
      return result;
    })>);
static_assert(has_utf8_decoder<observed_byte_iterator>);
static_assert(has_utf8_decoder<char8_t const*>);
static_assert(has_utf8_decoder<signed char const*>);
static_assert(has_utf8_decoder<unsigned char const*>);
static_assert(has_utf8_decoder<volatile char*>);
static_assert(is_utf16_lead_surrogate(Utf32Type(0xd800)));
static_assert(!is_utf16_lead_surrogate(Utf32Type(0xdc00)));
static_assert(is_utf16_trail_surrogate(Utf32Type(0xdfff)));
static_assert(!is_utf16_trail_surrogate(Utf32Type(0xe000)));
static_assert(noexcept(is_utf16_lead_surrogate(Utf32Type{})));
static_assert(noexcept(is_utf16_trail_surrogate(Utf32Type{})));

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

TEST(StringViewStorageTest, BulkCodepointVisitorBorrowsLvalueAndPassesIndependentValues) {
  std::vector<codepoint_record> records;
  move_only_codepoint_visitor visitor(records);
  auto bytes = std::string_view("A\0é日\xed\xa0\x80", 10);
  ASSERT_TRUE(for_each_codepoint(bytes, visitor));
  EXPECT_EQ(*visitor.calls, 5);
  EXPECT_EQ(records, (std::vector<codepoint_record>{
      {U'A', 0, 1}, {U'\0', 1, 1}, {U'é', 2, 2}, {U'日', 4, 3}, {char32_t(0xd800), 7, 3}}));
  records.clear();
  ASSERT_TRUE(for_each_codepoint("é", move_only_codepoint_visitor(records)));
  EXPECT_EQ(records, (std::vector<codepoint_record>{{U'é', 0, 2}}));
  ASSERT_TRUE(for_each_codepoint(std::string_view(nullptr, 0), visitor));
  EXPECT_EQ(*visitor.calls, 5);
}

TEST(StringViewStorageTest, BulkCodepointVisitorStopsAtFirstCallbackOrDecodeFailure) {
  std::vector<codepoint_record> records;
  auto callback_error = for_each_codepoint(std::string_view("Aé\xff", 4),
      [state = std::make_unique<int>(0), &records](char32_t point, std::size_t offset, std::size_t width) mutable
          -> std::expected<void, StarException> {
        ++*state;
        records.push_back({point, offset, width});
        return std::unexpected(OutOfRangeException("visitor failure", false));
      });
  ASSERT_FALSE(callback_error);
  EXPECT_TRUE(callback_error.error().is<OutOfRangeException>());
  EXPECT_EQ(records, (std::vector<codepoint_record>{{U'A', 0, 1}}));
  records.clear();
  auto decode_error = for_each_codepoint(std::string_view("Aé\xff", 4),
      [&](char32_t point, std::size_t offset, std::size_t width) -> std::expected<void, StarException> {
        records.push_back({point, offset, width});
        return {};
      });
  ASSERT_FALSE(decode_error);
  EXPECT_TRUE(decode_error.error().is<UnicodeException>());
  EXPECT_EQ(records, (std::vector<codepoint_record>{{U'A', 0, 1}, {U'é', 1, 2}}));
  records.clear();
  auto first_error = for_each_codepoint(std::string_view("\xff" "A", 2), move_only_codepoint_visitor(records));
  ASSERT_FALSE(first_error);
  EXPECT_TRUE(first_error.error().is<UnicodeException>());
  EXPECT_TRUE(records.empty());
}

TEST(StringViewStorageTest, GenericDecoderReadsOnlyTheWantedSequenceWidth) {
  using decoder = U8ToU32Iterator<observed_byte_iterator>;
  for (std::string_view bytes : {"ABC", "Aé日", "\xed\xa0\x80"}) {
    std::size_t reads = 0;
    auto first = decoder::create({bytes.data(), reads}, {bytes.data() + bytes.size(), reads});
    ASSERT_TRUE(first);
    EXPECT_EQ(reads, bytes.size());
  }
  for (std::string_view bytes : {"\xff" "ABC", "\xc0\x80" "ABC", "\xe0" "A"}) {
    std::size_t reads = 0;
    auto first = decoder::create({bytes.data(), reads}, {bytes.data() + bytes.size(), reads});
    ASSERT_FALSE(first);
    EXPECT_TRUE(first.error().is<UnicodeException>());
    EXPECT_EQ(reads, bytes[0] == '\xff' ? 1u : 2u);
  }
}

TEST(StringViewStorageTest, ByteSourceKindsPreserveCodepointAndConversionSemantics) {
  auto check = []<class Byte>(Byte const* first, Byte const* last) {
    using decoder = U8ToU32Iterator<Byte const*>;
    auto position = decoder::create(first, last);
    ASSERT_TRUE(position);
    EXPECT_EQ(**position, U'A');
    ++*position;
    EXPECT_EQ(**position, U'é');
    ++*position;
    EXPECT_EQ(*position, decoder::end(last));
  };
  char bytes[]{'A', char(0xc3), char(0xa9)};
  signed char signed_bytes[]{'A', static_cast<signed char>(0xc3), static_cast<signed char>(0xa9)};
  unsigned char unsigned_bytes[]{'A', 0xc3, 0xa9};
  char8_t utf8_bytes[]{u8'A', char8_t(0xc3), char8_t(0xa9)};
  check(bytes, bytes + 3);
  check(signed_bytes, signed_bytes + 3);
  check(unsigned_bytes, unsigned_bytes + 3);
  check(utf8_bytes, utf8_bytes + 3);
  // Wider integral sources retain conversion-to-char validation rather than
  // treating their object representation as a contiguous byte sequence.
  unsigned wide[]{0x141, 0x1c3, 0x1a9};
  check(wide, wide + 3);
  volatile char volatile_bytes[]{'A', char(0xc3), char(0xa9)};
  using volatile_decoder = U8ToU32Iterator<volatile char*>;
  auto position = volatile_decoder::create(volatile_bytes, volatile_bytes + 3);
  ASSERT_TRUE(position);
  EXPECT_EQ(**position, U'A');
  ++*position;
  EXPECT_EQ(**position, U'é');
  using empty_decoder = U8ToU32Iterator<unsigned char const*>;
  auto empty = empty_decoder::create(nullptr, nullptr);
  ASSERT_TRUE(empty);
  EXPECT_EQ(*empty, empty_decoder::end(nullptr));
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
