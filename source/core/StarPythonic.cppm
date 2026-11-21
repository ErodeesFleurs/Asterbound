module;
import std;
import star.format;

namespace Star {

// start names an element only when count is nonzero.
struct SlicePlan {
  std::size_t start;
  std::size_t stride;
  std::size_t count;
  bool reverse;
};

[[nodiscard]] constexpr SlicePlan normalize_slice(std::size_t size, std::optional<int> start,
                                                  std::optional<int> stop, int step) noexcept {
  // Unsigned subtraction gives the magnitude of INT_MIN without signed negation.
  auto magnitude = [](int negative) { return 0u - static_cast<unsigned int>(negative); };
  SlicePlan plan{0, step < 0 ? magnitude(step) : static_cast<unsigned int>(step), 0, step < 0};
  if (size == 0 || step == 0)
    return plan;

  if (!start)
    plan.start = plan.reverse ? size - 1 : 0;
  else if (*start < 0)
    plan.start = size - std::min<std::size_t>(magnitude(*start), size);
  else
    plan.start = std::min<std::size_t>(*start, plan.reverse ? size - 1 : size);

  // A negative stop at or below -size is before-begin, even for forward slices.
  std::optional<std::size_t> end;
  if (!stop) {
    if (!plan.reverse)
      end = size;
  } else if (*stop < 0) {
    if (magnitude(*stop) < size)
      end = size - magnitude(*stop);
  } else {
    end = std::min<std::size_t>(*stop, size);
  }

  std::size_t distance;
  if (plan.reverse) {
    if (end && plan.start <= *end)
      return plan;
    distance = end ? plan.start - *end : plan.start + 1;
  } else {
    if (!end || plan.start >= *end)
      return plan;
    distance = *end - plan.start;
  }
  plan.count = 1 + (distance - 1) / plan.stride;
  return plan;
}

namespace PythonicDetail {

template <typename In>
concept slice_input =
  requires(In const& input, std::size_t index) {
    { input.size() } -> std::unsigned_integral;
    input[index];
  }
  && (std::numeric_limits<decltype(std::declval<In const&>().size())>::digits
      <= std::numeric_limits<std::size_t>::digits);

// Standard container push_back declarations alone do not expose the element
// construction requirement; retain duck-typed sinks without a value_type.
template <typename Res, typename Reference>
concept slice_value_constructible =
  !requires { typename Res::value_type; } || std::is_constructible_v<typename Res::value_type, Reference>;

template <typename Result, typename Input>
using SliceOutput = std::conditional_t<std::same_as<Result, void>, Input, Result>;

}// namespace PythonicDetail

template <typename Res, typename In>
concept slice_result = PythonicDetail::slice_input<In> && std::is_default_constructible_v<Res>
  && std::is_constructible_v<Res, Res&&>
  && PythonicDetail::slice_value_constructible<Res, decltype(std::declval<In const&>()[std::size_t{}])>
  && requires(Res& result, In const& input, std::size_t index) {
       result.push_back(input[index]);
       requires std::same_as<decltype(result.push_back(input[index])), void>
         || std::same_as<decltype(result.push_back(input[index])), std::expected<void, StarException>>;
     };

namespace PythonicDetail {

template <typename Res, typename In>
  requires slice_result<Res, In>
[[nodiscard]] auto copy_slice(In const& input, SlicePlan plan)
  -> std::conditional_t<std::is_void_v<decltype(std::declval<Res&>().push_back(input[std::size_t{}]))>, Res,
                        std::expected<Res, StarException>> {
  using AppendResult = decltype(std::declval<Res&>().push_back(input[std::size_t{}]));

  if (plan.count == 0)
    return Res();

  Res result;
  // Fallible insertion must discover bounded overflow through ordered appends,
  // not through an eager reservation that could reject the successful prefix.
  if constexpr (std::is_void_v<AppendResult> && requires(std::size_t count) {
                  { result.reserve(count) } -> std::same_as<void>;
                })
    result.reserve(plan.count);

  auto index = plan.start;
  while (true) {
    if constexpr (std::is_void_v<AppendResult>) {
      result.push_back(input[index]);
    } else {
      auto appended = result.push_back(input[index]);
      if (!appended)
        return std::unexpected(std::move(appended.error()));
    }
    // Do not step beyond the last element: even size_t can overflow there.
    if (--plan.count == 0)
      break;
    index = plan.reverse ? index - plan.stride : index + plan.stride;
  }

  return result;
}

}// namespace PythonicDetail

template <typename Result = void, typename Input>
  requires slice_result<PythonicDetail::SliceOutput<Result, Input>, Input>
[[nodiscard]] auto slice(Input const& input, std::optional<int> start = std::nullopt,
                         std::optional<int> stop = std::nullopt, int step = 1) {
  return PythonicDetail::copy_slice<PythonicDetail::SliceOutput<Result, Input>>(
      input, normalize_slice(input.size(), start, stop, step));
}
}

export module star.pythonic;

export namespace Star {
  using ::Star::SlicePlan;
  using ::Star::normalize_slice;
  using ::Star::slice_result;
  using ::Star::slice;
}
