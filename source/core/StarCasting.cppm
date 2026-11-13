module;
import std;
import star.format;
import star.type_identity;

namespace Star {

struct PointerConvertExceptionTag {
  static constexpr std::string_view name() { return "PointerConvertException"; }
};
using PointerConvertException = StarError<PointerConvertExceptionTag, StarException>;

template <typename T>
concept cast_target = std::is_void_v<T> || std::is_object_v<T>;

template <typename T>
concept cast_source = std::is_object_v<T> || std::is_void_v<T>;

template <cast_target Target, cast_source Source>
using cast_result_t = std::conditional_t<std::is_const_v<Source>, std::add_const_t<Target>, Target>;

template <typename Target, typename Source>
concept capability_castable =
  cast_target<Target> && cast_source<Source> && (!std::is_const_v<Source> || std::is_const_v<Target>)
  && (std::is_convertible_v<Source*, Target*> || (std::is_class_v<Target> && capability_queryable<Source>));

template <typename Target, typename Source>
concept reference_castable = cast_target<Target> && cast_source<Source> && !std::is_void_v<Target>
  && capability_castable<cast_result_t<Target, Source>, Source>;

// Raw addresses are confined to the erased protocol and standard pointer boundary.
template <cast_target Target, cast_source Source>
  requires capability_castable<Target, Source>
Target* query_cast(Source* value) noexcept {
  if (!value)
    return nullptr;
  if constexpr (std::is_convertible_v<Source*, Target*>) {
    return value;
  } else {
    auto result = static_cast<std::remove_const_t<Target> const*>(value->query_type(type_key<Target>()));
    if constexpr (std::is_const_v<Target>)
      return result;
    else
      return const_cast<Target*>(result);
  }
}

template <cast_target Target, cast_source Source>
  requires capability_castable<cast_result_t<Target, Source>, Source>
cast_result_t<Target, Source>* as(Source* value) noexcept {
  return query_cast<cast_result_t<Target, Source>>(value);
}

template <cast_target Target, cast_source Source>
  requires capability_castable<cast_result_t<Target, Source>, typename std::shared_ptr<Source>::element_type>
std::shared_ptr<cast_result_t<Target, Source>> as(std::shared_ptr<Source> const& owner) noexcept {
  if (auto adjusted = as<Target>(owner.get()))
    return std::shared_ptr<cast_result_t<Target, Source>>(owner, adjusted);
  return {};
}

template <cast_target Target, cast_source Source>
  requires capability_castable<cast_result_t<Target, Source>, typename std::shared_ptr<Source>::element_type>
std::shared_ptr<cast_result_t<Target, Source>> as(std::shared_ptr<Source>&& owner) noexcept {
  if (auto adjusted = as<Target>(owner.get()))
    return std::shared_ptr<cast_result_t<Target, Source>>(std::move(owner), adjusted);
  return {};
}

template <cast_target Target, cast_source Source>
  requires reference_castable<Target, Source>
auto as_ref(Source& value) noexcept -> std::optional<std::reference_wrapper<cast_result_t<Target, Source>>> {
  if (auto adjusted = as<Target>(std::addressof(value)))
    return std::ref(*adjusted);
  return std::nullopt;
}

template <cast_target Target, cast_source Source>
  requires capability_castable<cast_result_t<Target, Source>, Source>
bool is(Source* value) noexcept {
  return as<Target>(value) != nullptr;
}

template <cast_target Target, cast_source Source>
  requires capability_castable<cast_result_t<Target, Source>, typename std::shared_ptr<Source>::element_type>
bool is(std::shared_ptr<Source> const& owner) noexcept {
  return is<Target>(owner.get());
}

template <typename Target, typename Ptr>
concept cast_input = cast_target<Target> && requires(Ptr&& value) {
  as<Target>(std::forward<Ptr>(value));
  { static_cast<bool>(value) } -> std::same_as<bool>;
  typename std::pointer_traits<std::remove_cvref_t<Ptr>>::element_type;
};

template <cast_target Target, typename Ptr>
  requires cast_input<Target, Ptr>
[[nodiscard]] auto convert(Ptr&& value)
  -> std::expected<decltype(as<Target>(std::forward<Ptr>(value))), StarException> {
  if (!value)
    return std::unexpected(
      PointerConvertException::format("Could not convert from nullptr to {}", type_name<Target>()));
  if (auto result = as<Target>(std::forward<Ptr>(value)))
    return result;
  if constexpr (requires { value->dynamic_type_name(); }) {
    return std::unexpected(PointerConvertException::format("Could not convert from {} to {}",
                                                           value->dynamic_type_name(), type_name<Target>()));
  } else {
    using Source = typename std::pointer_traits<std::remove_cvref_t<Ptr>>::element_type;
    return std::unexpected(
      PointerConvertException::format("Could not convert from {} to {}", type_name<Source>(), type_name<Target>()));
  }
}

template <cast_target Target, cast_source Source>
  requires reference_castable<Target, Source>
[[nodiscard]] auto convert_ref(Source& value)
  -> std::expected<std::reference_wrapper<cast_result_t<Target, Source>>, StarException> {
  if (auto result = as_ref<Target>(value))
    return *result;
  using Result = cast_result_t<Target, Source>;
  if constexpr (requires { value.dynamic_type_name(); })
    return std::unexpected(PointerConvertException::format("Could not convert from {} to {}", value.dynamic_type_name(),
                                                           type_name<Result>()));
  else
    return std::unexpected(
      PointerConvertException::format("Could not convert from {} to {}", type_name<Source>(), type_name<Result>()));
}
}

export module star.casting;

export namespace Star {
using ::Star::as;
using ::Star::as_ref;
using ::Star::capability_castable;
using ::Star::cast_input;
using ::Star::cast_result_t;
using ::Star::cast_source;
using ::Star::cast_target;
using ::Star::convert;
using ::Star::convert_ref;
using ::Star::is;
using ::Star::PointerConvertException;
using ::Star::PointerConvertExceptionTag;
using ::Star::reference_castable;
}
