module;
import std;
import star.format;

namespace Star {

struct EitherExceptionTag {
  static constexpr std::string_view name() { return "EitherException"; }
};
using EitherException = StarError<EitherExceptionTag, StarException>;

template <typename T>
concept either_payload = std::is_object_v<T> && !std::is_array_v<T> && std::destructible<T>;

// A tag owns the argument until it is consumed. Only the index is stored by
// Either's variant, even when both sides have the same payload type.
template <std::size_t index, either_payload T>
  requires (index < 2)
struct EitherTag {
  T value;
};

// Test the aggregate member initialization used by the tags, not just
// constructible_from: an explicit or narrowing conversion is not implicit.
template <typename To, typename From>
concept either_payload_from = either_payload<To> && std::constructible_from<To, From> && requires(From&& from) {
  EitherTag<0, To>{std::forward<From>(from)};
};

template <typename To, typename From>
using EitherTagPayload = std::conditional_t<std::is_void_v<To>, std::decay_t<From>, To>;

template <typename To = void, typename From>
  requires either_payload_from<EitherTagPayload<To, From>, From>
[[nodiscard]] constexpr auto make_left(From&& value) {
  return EitherTag<0, EitherTagPayload<To, From>>{std::forward<From>(value)};
}

template <typename To = void, typename From>
  requires either_payload_from<EitherTagPayload<To, From>, From>
[[nodiscard]] constexpr auto make_right(From&& value) {
  return EitherTag<1, EitherTagPayload<To, From>>{std::forward<From>(value)};
}

// Moving a nullable payload consumes its value, not the active variant side.
// This policy depends on reset itself, never on optional_ref overloads.
template <typename T>
concept either_resettable_payload = std::is_class_v<T> && requires(T& value) {
  static_cast<bool>(value);
  *value;
  { value.reset() } -> std::same_as<void>;
};

template <typename T>
concept either_nothrow_reset = !either_resettable_payload<T> || requires(T& value) {
  { value.reset() } noexcept;
};

template <typename Self>
concept either_lvalue = std::is_lvalue_reference_v<Self>;

template <either_payload Left, either_payload Right>
class Either {
  using Storage = std::variant<Left, Right>;
  template <std::size_t index>
  using Side = std::variant_alternative_t<index, Storage>;
  static constexpr bool resets_source = either_resettable_payload<Left> || either_resettable_payload<Right>;
  static constexpr bool nothrow_reset = either_nothrow_reset<Left> && either_nothrow_reset<Right>;
  static constexpr bool trivial_move_assignment = !resets_source && std::is_trivially_move_assignable_v<Storage>;

public:
  Either() = default;
  Either(Either const&) = default;
  Either& operator=(Either const&) = default;

  Either(Either&&) requires (!resets_source) = default;
  Either(Either&& source) noexcept(std::is_nothrow_move_constructible_v<Storage> && nothrow_reset)
    requires (resets_source && std::is_move_constructible_v<Storage>)
    : m_value(std::move(source.m_value)) {
    source.reset_payload();
  }

  Either& operator=(Either&&) requires trivial_move_assignment = default;
  Either& operator=(Either&& source) noexcept(std::is_nothrow_move_assignable_v<Storage> && nothrow_reset)
    requires (!trivial_move_assignment && std::is_move_assignable_v<Storage>) {
    if (this != std::addressof(source)) {
      m_value = std::move(source.m_value);
      if constexpr (resets_source)
        source.reset_payload();
    }
    return *this;
  }

  template <std::size_t index, typename T>
    requires either_payload_from<Side<index>, T const&>
  constexpr Either(EitherTag<index, T> const& tag)
    : m_value(std::in_place_index<index>, tag.value) {}

  template <std::size_t index, typename T>
    requires either_payload_from<Side<index>, T>
  constexpr Either(EitherTag<index, T>&& tag)
    : m_value(std::in_place_index<index>, std::move(tag.value)) {}

  // Capture the tag first: it may itself be a member of the active payload.
  template <std::size_t index, typename T>
    requires either_payload_from<Side<index>, T>
  constexpr Either& operator=(EitherTag<index, T> tag) {
    m_value.template emplace<index>(std::move(tag.value));
    return *this;
  }

  [[nodiscard]] constexpr bool is_left() const noexcept { return m_value.index() == 0; }
  [[nodiscard]] constexpr bool is_right() const noexcept { return m_value.index() == 1; }

  // Own the replacement before destroying a side: value may alias its payload.
  template <typename T>
    requires (either_payload_from<Left, T> && either_payload_from<Left, Left>)
  constexpr void set_left(T&& value) { *this = make_left<Left>(std::forward<T>(value)); }
  template <typename T>
    requires (either_payload_from<Right, T> && either_payload_from<Right, Right>)
  constexpr void set_right(T&& value) { *this = make_right<Right>(std::forward<T>(value)); }

  [[nodiscard]] constexpr Left& left_ref() & { return std::get<0>(m_value); }
  [[nodiscard]] constexpr Left const& left_ref() const& { return std::get<0>(m_value); }
  void left_ref() const&& = delete;
  [[nodiscard]] constexpr Right& right_ref() & { return std::get<1>(m_value); }
  [[nodiscard]] constexpr Right const& right_ref() const& { return std::get<1>(m_value); }
  void right_ref() const&& = delete;

  [[nodiscard]] constexpr auto maybe_left_ref() & { return borrow<0>(*this); }
  [[nodiscard]] constexpr auto maybe_left_ref() const& { return borrow<0>(*this); }
  void maybe_left_ref() const&& = delete;
  [[nodiscard]] constexpr auto maybe_right_ref() & { return borrow<1>(*this); }
  [[nodiscard]] constexpr auto maybe_right_ref() const& { return borrow<1>(*this); }
  void maybe_right_ref() const&& = delete;

  [[nodiscard]] auto left() & { return checked<0>(*this); }
  [[nodiscard]] auto left() const& { return checked<0>(*this); }
  void left() const&& = delete;
  [[nodiscard]] auto right() & { return checked<1>(*this); }
  [[nodiscard]] auto right() const& { return checked<1>(*this); }
  void right() const&& = delete;

  [[nodiscard]] constexpr std::optional<Left> maybe_left() const requires std::is_copy_constructible_v<Left> {
    if (is_left())
      return std::optional<Left>(std::in_place, left_ref());
    return {};
  }
  [[nodiscard]] constexpr std::optional<Right> maybe_right() const requires std::is_copy_constructible_v<Right> {
    if (is_right())
      return std::optional<Right>(std::in_place, right_ref());
    return {};
  }

private:
  void reset_payload() noexcept(nothrow_reset) requires resets_source {
    if constexpr (either_resettable_payload<Left>)
      if (is_left())
        left_ref().reset();
    if constexpr (either_resettable_payload<Right>)
      if (is_right())
        right_ref().reset();
  }

  template <std::size_t index, either_lvalue Self>
    requires (index < 2)
  static constexpr auto borrow(Self&& self) {
    using Result = std::optional<decltype(std::ref(std::get<index>(self.m_value)))>;
    if (self.m_value.index() == index)
      return Result(std::ref(std::get<index>(self.m_value)));
    return Result{};
  }

  template <std::size_t index, either_lvalue Self>
    requires (index < 2)
  static auto checked(Self&& self) {
    using Result = std::expected<decltype(std::ref(std::get<index>(self.m_value))), StarException>;
    if (auto ref = borrow<index>(self))
      return Result(*ref);
    return Result(std::unexpect, EitherException(index == 0
        ? "Improper access of left side of Either" : "Improper access of right side of Either"));
  }

  Storage m_value;
};

}

export module star.either;

export namespace Star {
  using ::Star::EitherExceptionTag;
  using ::Star::EitherException;
  using ::Star::EitherTag;
  using ::Star::make_left;
  using ::Star::make_right;
  using ::Star::Either;
}
