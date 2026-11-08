module;
import std;
import star.format;

namespace Star {

// Python style container slicing

struct SliceIndex {
  SliceIndex() : index(0), given(false) {}
  SliceIndex(int i) : index(i), given(true) {}

  int index;
  bool given;
};

inline SliceIndex const SliceNil = SliceIndex();

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

template <typename Res, typename In>
concept slice_result = slice_input<In> && std::is_default_constructible_v<Res> && std::is_constructible_v<Res, Res&&>
  && slice_value_constructible<Res, decltype(std::declval<In const&>()[std::size_t{}])>
  && requires(Res& result, In const& input, std::size_t index) {
       result.push_back(input[index]);
       requires std::same_as<decltype(result.push_back(input[index])), void>
         || std::same_as<decltype(result.push_back(input[index])), std::expected<void, StarException>>;
     };

}// namespace PythonicDetail

template <typename Res, typename In>
  requires PythonicDetail::slice_result<Res, In>
[[nodiscard]] auto slice(In const& r, SliceIndex a, SliceIndex b = SliceIndex(), int j = 1)
  -> std::conditional_t<std::is_void_v<decltype(std::declval<Res&>().push_back(r[std::size_t{}]))>, Res,
                        std::expected<Res, StarException>> {
  using AppendResult = decltype(std::declval<Res&>().push_back(r[std::size_t{}]));
  auto size = static_cast<std::size_t>(r.size());

  // A zero stride preserves the existing successful empty-slice behavior.
  if (j == 0 || size == 0)
    return Res();

  // Unsigned subtraction also handles INT_MIN without signed negation.
  auto magnitude = [](int index) { return 0u - static_cast<unsigned int>(index); };
  auto stride = j < 0 ? magnitude(j) : static_cast<unsigned int>(j);

  std::size_t start;
  if (!a.given)
    start = j > 0 ? 0 : size - 1;
  else if (a.index < 0)
    start = magnitude(a.index) >= size ? 0 : size - magnitude(a.index);
  else
    start = std::min(static_cast<std::size_t>(a.index), size);

  // A reverse slice starts at an element, never the past-the-end position.
  if (j < 0 && start == size)
    --start;

  // A disengaged end denotes the legacy before-begin (-1) bound, without
  // representing the whole sequence size in a signed integer.
  std::optional<std::size_t> end;
  if (!b.given) {
    if (j > 0)
      end = size;
  } else if (b.index < 0) {
    if (magnitude(b.index) < size)
      end = size - magnitude(b.index);
  } else {
    end = std::min(static_cast<std::size_t>(b.index), size);
  }

  std::size_t distance;
  if (j > 0) {
    if (!end || start >= *end)
      return Res();
    distance = *end - start;
  } else {
    if (end && start <= *end)
      return Res();
    distance = end ? start - *end : start + 1;
  }

  // Subtract before dividing so neither rounding up nor the final index step
  // can overflow, even when size() is the largest representable size_t.
  auto remaining = 1 + (distance - 1) / stride;
  Res returnSlice;
  // Fallible insertion must discover bounded overflow through ordered appends,
  // not through an eager reservation that could reject the successful prefix.
  if constexpr (std::is_void_v<AppendResult> && requires(Res& result, std::size_t count) {
                  { result.reserve(count) } -> std::same_as<void>;
                })
    returnSlice.reserve(remaining);

  auto i = start;
  while (remaining != 0) {
    if constexpr (std::is_void_v<AppendResult>) {
      returnSlice.push_back(r[i]);
    } else {
      auto appended = returnSlice.push_back(r[i]);
      if (!appended)
        return std::unexpected(std::move(appended.error()));
    }
    if (--remaining != 0) {
      if (j > 0)
        i += stride;
      else
        i -= stride;
    }
  }

  return returnSlice;
}

template <typename T>
  requires PythonicDetail::slice_result<T, T>
[[nodiscard]] auto slice(T const& r, SliceIndex a, SliceIndex b = SliceIndex(), int j = 1) {
  return slice<T, T>(r, a, b, j);
}
}

export module star.pythonic;

export namespace Star {
  using ::Star::SliceIndex;
  using ::Star::SliceNil;
  using ::Star::slice;
}
