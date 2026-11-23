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

template <typename Callback>
concept directive_error_reporter = std::invocable<Callback&>
  && std::same_as<std::invoke_result_t<Callback&>, std::expected<void, StarException>>;

class Directives {
public:
  struct Entry {
    std::size_t begin = 0;
    std::size_t length = 0;
    std::size_t index = 0;
  };

  Directives();
  Directives(String const& directives);
  Directives(String&& directives);
  Directives(std::string_view directives);
  template <std::size_t extent>
  Directives(char const (&directives)[extent]) : Directives(std::string_view(directives, extent - 1)) {}
  Directives(Directives const& directives);
  Directives(Directives&& directives) noexcept;
  ~Directives();

  Directives& operator=(String const& s);
  Directives& operator=(String&& s);
  Directives& operator=(std::string_view s);
  template <std::size_t extent>
  Directives& operator=(char const (&text)[extent]) {
    return *this = std::string_view(text, extent - 1);
  }
  Directives& operator=(Directives&& other) noexcept;
  Directives& operator=(Directives const& other);

  void load_operations() const;
  std::span<Entry const> entries() const &;
  std::span<Entry const> entries() const && = delete;
  std::span<ImageOperation const> operations() const &;
  std::span<ImageOperation const> operations() const && = delete;
  StringView entry_string(Entry const& entry) const &;
  StringView entry_string(Entry const& entry) const && = delete;
  ImageOperation const& entry_operation(Entry const& entry) const &;
  ImageOperation const& entry_operation(Entry const& entry) const && = delete;
  StringView prefix() const &;
  StringView prefix() const && = delete;
  String string() const;
  std::optional<std::reference_wrapper<String const>> maybe_string() const &;
  std::optional<std::reference_wrapper<String const>> maybe_string() const && = delete;
  String build_string() const;
  String& append_to(String& out) const;
  std::size_t hash() const;
  std::size_t size() const;
  bool empty() const;
  operator bool() const;

  // Failed reports may be retried. Copies share successful suppression, while
  // concurrent callers may both report; callbacks never run under a lock.
  template <directive_error_reporter Callback>
  [[nodiscard]] std::expected<void, StarException> report_error(Entry const& entry, Callback&& callback) const {
    if (error_reported(entry))
      return {};
    Directives owner = *this;
    auto result = std::invoke(callback);
    if (result)
      owner.mark_error_reported(entry);
    return result;
  }

  bool equals(Directives const& other) const;
  bool equals(String const& string) const;

  bool operator==(Directives const& other) const;
  bool operator==(String const& string) const;
  bool operator!=(Directives const& other) const;
  bool operator!=(String const& string) const;

  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Directives& directives);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Directives const& directives);

private:
  struct Shared;
  void parse(String&& directives);
  bool error_reported(Entry const& entry) const;
  void mark_error_reported(Entry const& entry) const;

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
  friend struct ValueHash<DirectivesGroup>;
  DirectivesGroup();
  DirectivesGroup(String const& directives);
  DirectivesGroup(String&& directives);
  DirectivesGroup(DirectivesGroup const& directives);
  DirectivesGroup(DirectivesGroup&& directives) noexcept;
  DirectivesGroup& operator=(DirectivesGroup const& directives);
  DirectivesGroup& operator=(DirectivesGroup&& directives) noexcept;
  // Keep destruction in the native companion for imported optional owners.
  ~DirectivesGroup();

  bool empty() const;
  operator bool() const;
  bool compare(DirectivesGroup const& other) const;
  void append(Directives other);
  void clear();

  DirectivesGroup& operator+=(Directives other);

  String to_string() const;
  void append_to(String& string) const;

  template <directive_visitor Callback> [[nodiscard]] auto for_each(Callback&& callback) const;
  template <abortable_directive_visitor Callback> [[nodiscard]] auto for_each_abortable(Callback&& callback) const;

  [[nodiscard]] std::expected<Image, StarException> apply_new_image(const Image& image,
                                                                    ImageReferenceCallback ref_callback = {}) const;
  [[nodiscard]] std::expected<void, StarException> apply_existing_image(Image& image,
                                                                        ImageReferenceCallback ref_callback = {}) const;

  std::size_t hash() const;
  List<Directives> const& list() const &;
  List<Directives> const& list() const && = delete;

  friend bool operator==(DirectivesGroup const& a, DirectivesGroup const& b);
  friend bool operator!=(DirectivesGroup const& a, DirectivesGroup const& b);

  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DirectivesGroup& directives);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DirectivesGroup const& directives);
private:
  List<Directives> m_directives;
  std::size_t m_count = 0;
  mutable std::atomic<std::size_t> m_hash = 0;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Directives& directives);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Directives const& directives);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DirectivesGroup& directives);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DirectivesGroup const& directives);

template <>
struct ValueHash<DirectivesGroup> {
  std::size_t operator()(DirectivesGroup const& s) const;
};

using ImageDirectives = DirectivesGroup;

template <directive_visitor Callback> [[nodiscard]] auto DirectivesGroup::for_each(Callback&& callback) const {
  using Result = std::invoke_result_t<Callback&, Directives::Entry const&, Directives const&>;
  for (auto const& directives : m_directives) {
    for (auto const& entry : directives.entries()) {
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
[[nodiscard]] auto DirectivesGroup::for_each_abortable(Callback&& callback) const {
  using Result = std::invoke_result_t<Callback&, Directives::Entry const&, Directives const&>;
  for (auto const& directives : m_directives) {
    for (auto const& entry : directives.entries()) {
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
  struct ValueHash<DirectivesGroup>;
}

export namespace Star {
using ::Star::abortable_directive_result;
using ::Star::abortable_directive_visitor;
using ::Star::directive_error_reporter;
using ::Star::directive_visit_result;
using ::Star::directive_visitor;
using ::Star::Directives;
using ::Star::DirectivesException;
using ::Star::DirectivesExceptionTag;
using ::Star::DirectivesGroup;
using ::Star::ImageDirectives;
}
