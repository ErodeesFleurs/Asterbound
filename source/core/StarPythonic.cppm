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

// T must have operator[](int), size(), and
// push_back(typeof T::operator[](int()))
template <typename Res, typename In>
[[nodiscard]] auto slice(In const& r, SliceIndex a, SliceIndex b = SliceIndex(), int j = 1)
    -> std::conditional_t<std::is_void_v<decltype(std::declval<Res&>().push_back(r[0]))>, Res, std::expected<Res, StarException>> {
  using AppendResult = decltype(std::declval<Res&>().push_back(r[0]));
  int size = static_cast<int>(r.size());
  int start, end;

  // A zero stride preserves the existing successful empty-slice behavior.
  if (j == 0 || size == 0)
    return Res();

  if (!a.given) {
    if (j > 0)
      start = 0;
    else
      start = size - 1;
  } else if (a.index < 0) {
    if (-a.index > size - 1)
      start = 0;
    else
      start = size - -a.index;
  } else {
    if (a.index > size)
      start = size;
    else
      start = a.index;
  }

  if (!b.given) {
    if (j > 0)
      end = size;
    else
      end = -1;
  } else if (b.index < 0) {
    if (-b.index > size - 1) {
      end = -1;
    } else {
      end = size - -b.index;
    }
  } else {
    if (b.index > size - 1) {
      end = size;
    } else {
      end = b.index;
    }
  }

  if (start < end && j < 0)
    return Res();
  if (start > end && j > 0)
    return Res();

  Res returnSlice;
  int i;
  for (i = start; i < end; i += j) {
    if constexpr (std::is_void_v<AppendResult>) {
      returnSlice.push_back(r[i]);
    } else {
      auto appended = returnSlice.push_back(r[i]);
      if (!appended)
        return std::unexpected(std::move(appended.error()));
    }
  }

  return returnSlice;
}

template <typename T>
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
