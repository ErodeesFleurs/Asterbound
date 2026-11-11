module;
import std;
import star.string;
import star.string_view;
import star.list;
import star.format;
import star.hash;
import star.data_stream;
import star.image_processing;

namespace Star {

struct DirectivesExceptionTag {
  static constexpr std::string_view name() { return "DirectivesException"; }
};
using DirectivesException = StarError<DirectivesExceptionTag, StarException>;

// Kae: My attempt at reducing memory allocation and per-frame string parsing for extremely long directives
class Directives {
public:
  struct Shared;
  struct Entry {
    mutable ImageOperation operation;
    std::size_t begin;
    std::size_t length;

    ImageOperation const& loadOperation(Shared const& parent) const;
    StringView string(Shared const& parent) const;
    Entry(ImageOperation&& newOperation, std::size_t begin, std::size_t end);
    Entry(ImageOperation const& newOperation, std::size_t begin, std::size_t end);
    Entry(Entry const& other);
  };

  struct Shared {
    List<Entry> entries;
    String string;
    std::size_t hash = 0;
    mutable std::mutex mutex;

    bool empty() const;
    Shared();
    Shared(List<Entry>&& givenEntries, String&& givenString);
  };

  Directives();
  Directives(String const& directives);
  Directives(String&& directives);
  Directives(std::string_view directives);
  template <std::size_t Size>
  Directives(char const (&directives)[Size]) : Directives(std::string_view(directives, Size - 1)) {}
  Directives(Directives const& directives);
  Directives(Directives&& directives) noexcept;
  ~Directives();

  Directives& operator=(String const& s);
  Directives& operator=(String&& s);
  Directives& operator=(std::string_view s);
  template <std::size_t Size>
  Directives& operator=(char const (&text)[Size]) {
    return *this = std::string_view(text, Size - 1);
  }
  Directives& operator=(Directives&& other) noexcept;
  Directives& operator=(Directives const& other);

  void loadOperations() const;
  void parse(String&& directives);
  StringView prefix() const;
  String string() const;
  std::optional<std::reference_wrapper<String const>> maybeString() const;
  String buildString() const;
  String& addToString(String& out) const;
  std::size_t hash() const;
  std::size_t size() const;
  bool empty() const;
  operator bool() const;

  [[nodiscard]] std::expected<std::reference_wrapper<Shared const>, StarException> get() const;

  bool equals(Directives const& other) const;
  bool equals(String const& string) const;

  bool operator==(Directives const& other) const;
  bool operator==(String const& string) const;
  bool operator!=(Directives const& other) const;
  bool operator!=(String const& string) const;

  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Directives& directives);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Directives const& directives);

  //friend bool operator==(Directives const& d1, String const& d2);
  //friend bool operator==(Directives const& directives, String const& string);
  //friend bool operator==(String const& string, Directives const& directives);

  std::shared_ptr<Shared const> m_shared;
};

template <typename Result>
concept directive_visit_result = std::is_void_v<Result> || requires(Result& result) {
  Result{};
  bool(!result);
  Result(std::unexpected(std::move(result.error())));
};

template <typename Callback>
concept directive_visitor = requires(Callback& callback, Directives::Entry const& entry, Directives const& directives) {
  callback(entry, directives);
} && directive_visit_result<std::invoke_result_t<Callback&, Directives::Entry const&, Directives const&>>;

template <typename Result>
concept abortable_directive_result = std::same_as<Result, bool> || requires(Result& result) {
  bool(!result);
  bool(!*result);
  Result(false);
  Result(true);
  Result(std::unexpected(std::move(result.error())));
};

template <typename Callback>
concept abortable_directive_visitor =
  requires(Callback& callback, Directives::Entry const& entry, Directives const& directives) {
    callback(entry, directives);
  } && abortable_directive_result<std::invoke_result_t<Callback&, Directives::Entry const&, Directives const&>>;

class DirectivesGroup {
public:
  friend struct hash<DirectivesGroup>;
  DirectivesGroup();
  DirectivesGroup(String const& directives);
  DirectivesGroup(String&& directives);
  DirectivesGroup(DirectivesGroup const& directives) = default;
  DirectivesGroup(DirectivesGroup&& directives) = default;
  DirectivesGroup& operator=(DirectivesGroup const& directives) = default;
  DirectivesGroup& operator=(DirectivesGroup&& directives) = default;
  // Keep destruction in the native companion for imported optional owners.
  ~DirectivesGroup();

  bool empty() const;
  operator bool() const;
  bool compare(DirectivesGroup const& other) const;
  void append(Directives const& other);
  void clear();

  DirectivesGroup& operator+=(Directives const& other);

  String toString() const;
  void addToString(String& string) const;

  template <directive_visitor Callback> [[nodiscard]] auto forEach(Callback&& callback) const;

  template <abortable_directive_visitor Callback> [[nodiscard]] auto forEachAbortable(Callback&& callback) const;

  [[nodiscard]] std::expected<Image, StarException> apply_new_image(const Image& image,
                                                                    ImageReferenceCallback refCallback = {}) const;
  [[nodiscard]] std::expected<void, StarException> apply_existing_image(Image& image,
                                                                        ImageReferenceCallback refCallback = {}) const;

  std::size_t hash() const;
  const List<Directives>& list() const;

  friend bool operator==(DirectivesGroup const& a, DirectivesGroup const& b);
  friend bool operator!=(DirectivesGroup const& a, DirectivesGroup const& b);

  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DirectivesGroup& directives);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DirectivesGroup const& directives);
private:
  void buildString(String& string, const DirectivesGroup& directives) const;

  List<Directives> m_directives;
  std::size_t m_count = 0;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Directives& directives);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Directives const& directives);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DirectivesGroup& directives);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DirectivesGroup const& directives);

template <>
struct hash<DirectivesGroup> {
  std::size_t operator()(DirectivesGroup const& s) const;
};

using ImageDirectives = DirectivesGroup;

inline std::expected<std::reference_wrapper<Directives::Shared const>, StarException> Directives::get() const {
  if (!m_shared)
    return std::unexpected(DirectivesException("Empty directives have no shared data"));
  return std::cref(*m_shared);
}

template <directive_visitor Callback> [[nodiscard]] auto DirectivesGroup::forEach(Callback&& callback) const {
  using Result = std::invoke_result_t<Callback&, Directives::Entry const&, Directives const&>;
  for (auto const& directives : m_directives) {
    if (!directives)
      continue;
    for (auto const& entry : directives.m_shared->entries) {
      if constexpr (std::is_void_v<Result>)
        callback(entry, directives);
      else {
        auto result = callback(entry, directives);
        if (!result)
          return Result(std::unexpected(std::move(result.error())));
      }
    }
  }
  if constexpr (!std::is_void_v<Result>)
    return Result{};
}

template <abortable_directive_visitor Callback>
[[nodiscard]] auto DirectivesGroup::forEachAbortable(Callback&& callback) const {
  using Result = std::invoke_result_t<Callback&, Directives::Entry const&, Directives const&>;
  for (auto const& directives : m_directives) {
    if (!directives)
      continue;
    for (auto const& entry : directives.m_shared->entries) {
      auto result = callback(entry, directives);
      if constexpr (std::is_same_v<Result, bool>) {
        if (!result)
          return false;
      } else {
        if (!result)
          return Result(std::unexpected(std::move(result.error())));
        if (!*result)
          return Result(false);
      }
    }
  }
  return Result(true);
}
}

export module star.directives;

export namespace Star {
  template <>
  struct hash<DirectivesGroup>;
}


export namespace Star {
using ::Star::abortable_directive_result;
using ::Star::abortable_directive_visitor;
using ::Star::directive_visit_result;
using ::Star::directive_visitor;
using ::Star::Directives;
using ::Star::DirectivesException;
using ::Star::DirectivesExceptionTag;
using ::Star::DirectivesGroup;
using ::Star::ImageDirectives;
}
