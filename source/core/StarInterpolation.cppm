module;
import std;
import star.format;

import star.math_common;
import star.array;
import star.algorithm;

namespace Star {

enum class BoundMode {
  Clamp,
  Extrapolate,
  Wrap
};

enum class InterpolationMode {
  HalfStep,
  Linear,
  Cubic
};

namespace InterpolationDetail {
// Parameter initialization checks the same implicit conversion as a return,
// including guaranteed elision for copy-only algebraic results.
template <typename T> void implicit_result(T) noexcept;
}// namespace InterpolationDetail

template <typename Offset, typename Value>
concept lerp_compatible = requires(Offset const& x, Value const& a, Value const& b) {
  InterpolationDetail::implicit_result<Value>(a * (1 - x) + b * (x));
};

template <typename Offset, typename Value>
concept angle_lerp_compatible = requires(Offset const& x, Value const& a, Value const& b) {
  InterpolationDetail::implicit_result<Value>(a + angle_difference(a, b) * x);
};

template <typename Offset, typename Value>
concept sin_ease_compatible = requires(Offset const& x, Offset const& w, Value const& a, Value const& b) {
  InterpolationDetail::implicit_result<Offset>((std::sin(x * std::numbers::pi - std::numbers::pi / 2) + 1) / 2);
  InterpolationDetail::implicit_result<Value>(a * (1 - w) + b * w);
};

template <typename Offset, typename Value>
concept limited_lerp_compatible =
  lerp_compatible<Offset, Value> && requires(Value const& a, Value const& b, Value const& limit) {
    (std::abs(b - a) > limit) ? 0 : 0;
    InterpolationDetail::implicit_result<Value>(b);
  };

template <typename Position, typename Value>
concept step_compatible = requires(Position const& x, Position const& threshold, Value const& value) {
  (x < threshold) ? 0 : 0;
  InterpolationDetail::implicit_result<Value>(value);
};

template <typename Position, typename Value>
concept half_step_compatible = requires(Position const& x, Value const& value) {
  (x < 0.5) ? 0 : 0;
  InterpolationDetail::implicit_result<Value>(value);
};

template <typename Offset, typename Value>
concept cubic4_compatible =
  requires(Offset const& x, Value const& f0, Value const& f1, Value const& f2, Value const& f3) {
    InterpolationDetail::implicit_result<Value>(
      f1 + (f2 - f0 + (f0 * 2.0 - f1 * 5.0 + f2 * 4.0 - f3 + ((f1 - f2) * 3.0 + f3 - f0) * x) * x) * x * 0.5);
  };

template <typename Offset, typename Value>
concept hermite2_compatible = requires(Offset const& x, Value const& a, Value const& b) {
  InterpolationDetail::implicit_result<Value>(a + (b - a) * x * x * (3 - 2 * x));
};

template <typename Offset, typename Value>
concept quintic2_compatible = requires(Offset const& x, Value const& a, Value const& b) {
  InterpolationDetail::implicit_result<Value>(a + (b - a) * x * x * x * (x * (x * 6 - 15) + 10));
};

template <typename T1, typename T2>
  requires angle_lerp_compatible<T1, T2>
constexpr T2 angle_lerp(T1 const& offset, T2 const& f0, T2 const& f1) {
  return f0 + angle_difference(f0, f1) * offset;
}

template <typename T1, typename T2>
  requires sin_ease_compatible<T1, T2>
constexpr T2 sin_ease(T1 const& offset, T2 const& f0, T2 const& f1) {
  T1 w = (std::sin(offset * std::numbers::pi - std::numbers::pi / 2) + 1) / 2;
  return f0 * (1 - w) + f1 * w;
}

template <typename T1, typename T2>
  requires lerp_compatible<T1, T2>
constexpr T2
lerp(T1 const& offset, T2 const& f0,
     T2 const& f1) noexcept(noexcept(InterpolationDetail::implicit_result<T2>(f0 * (1 - offset) + f1 * (offset)))) {
  return f0 * (1 - offset) + f1 * (offset);
}

template <typename T1, typename T2>
  requires limited_lerp_compatible<T1, T2>
constexpr T2 lerp_with_limit(std::optional<T2> const& limit, T1 const& offset, T2 const& f0, T2 const& f1) {
  if (limit && std::abs(f1 - f0) > *limit)
    return f1;
  return lerp(offset, f0, f1);
}

// Only the selected endpoint is copied; the owning result never borrows a
// temporary endpoint. No move or assignment capability is required.
template <typename T1, typename T2>
  requires step_compatible<T1, T2>
constexpr T2 step(T1 const& threshold, T1 const& x, T2 const& a,
                  T2 const& b) noexcept(noexcept(x < threshold ? 0 : 0)
                                        && noexcept(InterpolationDetail::implicit_result<T2>(a))) {
  if (x < threshold)
    return a;
  else
    return b;
}

template <typename T1, typename T2>
  requires half_step_compatible<T1, T2>
constexpr T2 half_step(T1 const& x, T2 const& a,
                       T2 const& b) noexcept(noexcept(x < 0.5 ? 0 : 0)
                                             && noexcept(InterpolationDetail::implicit_result<T2>(a))) {
  if (x < 0.5)
    return a;
  else
    return b;
}

template <typename T1, typename T2>
  requires cubic4_compatible<T1, T2>
constexpr T2 cubic4(T1 const& x, T2 const& f0, T2 const& f1, T2 const& f2, T2 const& f3) {
  return f1 + (f2 - f0 + (f0 * 2.0 - f1 * 5.0 + f2 * 4.0 - f3 + ((f1 - f2) * 3.0 + f3 - f0) * x) * x) * x * 0.5;
}

template <typename T1, typename T2>
  requires hermite2_compatible<T1, T2>
constexpr T2 hermite2(T1 const& x, T2 const& a, T2 const& b) {
  return a + (b - a) * x * x * (3 - 2 * x);
}

template <typename T1, typename T2>
  requires quintic2_compatible<T1, T2>
constexpr T2 quintic2(T1 const& x, T2 const& a, T2 const& b) {
  return a + (b - a) * x * x * x * (x * (x * 6 - 15) + 10);
}

template <typename Weight>
concept linear_weight = requires(Weight x) { Array<Weight, 2>{1 - x, x}; };

template <typename Weight>
concept step_weight = requires(Weight const& x) {
  InterpolationDetail::implicit_result<Weight>(0.5);
  (x < x) ? 0 : 0;
  Array<Weight, 2>{1, 0};
  Array<Weight, 2>{0, 1};
};

template <typename Weight>
concept sin_weight = linear_weight<Weight> && requires(Weight const& x) {
  InterpolationDetail::implicit_result<Weight>((std::sin(x * std::numbers::pi - std::numbers::pi / 2) + 1) / 2);
};

template <typename Weight>
concept cubic_weight = requires(Weight const& x) {
  (x > 1) ? 0 : 0;
  (x < 0) ? 0 : 0;
  InterpolationDetail::implicit_result<Weight>(x * x);
  Array<Weight, 4>{0, 0, 2 - x, x - 1};
  Array<Weight, 4>{-x, 1 + x, 0, 0};
  Array<Weight, 4>(-0.5 * x + 1 * x - 0.5 * x, 1.5 * x + -2.5 * x + 1.0, -1.5 * x + 2.0 * x + 0.5 * x,
                   0.5 * x - 0.5 * x);
};

template <linear_weight WeightT> struct LinearWeightOperator {
  using Weight = WeightT;
  using WeightVec = Array<Weight, 2>;

  constexpr WeightVec operator()(Weight x) const { return {1 - x, x}; }
};

template <step_weight WeightT> struct StepWeightOperator {
  using Weight = WeightT;
  using WeightVec = Array<Weight, 2>;

  constexpr StepWeightOperator(Weight threshold = 0.5) : threshold(threshold) {}

  constexpr WeightVec operator()(Weight x) const {
    if (x < threshold)
      return {1, 0};
    else
      return {0, 1};
  }

  Weight threshold;
};

template <sin_weight WeightT> struct SinWeightOperator {
  using Weight = WeightT;
  using WeightVec = Array<Weight, 2>;

  constexpr WeightVec operator()(Weight x) const {
    Weight w = (std::sin(x * std::numbers::pi - std::numbers::pi / 2) + 1) / 2;
    return {1 - w, w};
  }
};

// Linear extrapolation changes weights only outside [0, 1].
template <cubic_weight WeightT> struct Cubic4WeightOperator {
  using Weight = WeightT;
  using WeightVec = Array<Weight, 4>;

  constexpr Cubic4WeightOperator(bool le = false) : linear_extrapolate(le) {}

  constexpr WeightVec operator()(Weight x) const {
    if (linear_extrapolate && x > 1) {
      return {0, 0, 2 - x, x - 1};
    } else if (linear_extrapolate && x < 0) {
      return {-x, 1 + x, 0, 0};
    } else {
      Weight x2 = x * x;
      Weight x3 = x2 * x;
      return WeightVec(-0.5 * x3 + 1 * x2 - 0.5 * x,
          1.5 * x3 + -2.5 * x2 + 1.0,
          -1.5 * x3 + 2.0 * x2 + 0.5 * x,
          0.5 * x3 - 0.5 * x2);
    }
  }
  bool linear_extrapolate;
};

template <typename Loctype, typename IndexType>
struct Bound2 {
  IndexType i0;
  IndexType i1;
  Loctype offset;
};

template <typename Location, typename Index>
concept interpolation_location = (std::integral<Location> || std::floating_point<Location>)
  && !std::same_as<Location, bool> && std::integral<Index> && !std::same_as<Index, bool>;

// loc should be in "index space", meaning that 0 points exactly to the first
// element and extent - 1
// points exactly to the last element.
template <typename LocType, typename IndexType>
  requires interpolation_location<LocType, IndexType>
constexpr Bound2<LocType, IndexType> get_bound2(LocType loc, IndexType extent, BoundMode bmode) {
  Bound2<LocType, IndexType> bound{};
  if (extent <= 1) {
    return bound;
  }

  bound.offset = 0;
  if (bmode == BoundMode::Wrap) {
    loc = positive_mod<LocType>(loc, extent);
  } else {
    LocType new_loc = std::clamp<LocType>(loc, 0, extent - 1);
    if (bmode == BoundMode::Extrapolate)
      bound.offset += loc - new_loc;

    loc = new_loc;
  }

  bound.i0 = IndexType(loc);

  if (bound.i0 == extent - 1) {
    if (bmode == BoundMode::Wrap) {
      bound.i1 = 0;
    } else {
      bound.i1 = bound.i0;
      bound.i0 -= 1;
    }
  } else {
    bound.i1 = bound.i0 + 1;
  }

  bound.offset += loc - static_cast<LocType>(bound.i0);

  return bound;
}

template <typename Loctype, typename IndexType>
struct Bound4 {
  IndexType i0;
  IndexType i1;
  IndexType i2;
  IndexType i3;
  Loctype offset;
};

// loc should be in "index space", meaning that 0 points exactly to the first
// element and extent - 1
// points exactly to the last element.
// Keep the per-axis fast path inline; short-axis safety must not add a struct-return call.
template <typename LocType, typename IndexType>
  requires interpolation_location<LocType, IndexType>
[[gnu::always_inline]] constexpr Bound4<LocType, IndexType> get_bound4(LocType loc, IndexType extent, BoundMode bmode) {
  Bound4<LocType, IndexType> bound{};
  if (extent < 4) [[unlikely]] {
    if (extent <= 1) {
      return bound;
    }
    if (extent == 2 || bmode != BoundMode::Wrap) {
      auto linear = get_bound2(loc, extent, bmode);
      if (bmode == BoundMode::Wrap) {
        // Two-sample periodic cubic needs modular neighbors; three samples
        // already use the original four-bound path below.
        IndexType previous = linear.i0 == 0 ? extent - 1 : linear.i0 - 1;
        IndexType next_next = linear.i1 == extent - 1 ? 0 : linear.i1 + 1;
        return {previous, linear.i0, linear.i1, next_next, linear.offset};
      }
      // Non-periodic short axes require linear weights on duplicated taps.
      return {linear.i0, linear.i0, linear.i1, linear.i1, linear.offset};
    }
  }

  bound.offset = 0;
  if (bmode == BoundMode::Wrap) {
    loc = positive_mod<LocType>(loc, extent);
  } else {
    LocType new_loc = std::clamp<LocType>(loc, 0, extent - 1);
    if (bmode == BoundMode::Extrapolate)
      bound.offset += loc - new_loc;

    loc = new_loc;
  }

  bound.i1 = IndexType(loc);

  if (bound.i1 == extent - 1) {
    if (bmode == BoundMode::Wrap) {
      bound.i0 = bound.i1 - 1;
      bound.i2 = 0;
      bound.i3 = 1;
    } else {
      bound.i1 = bound.i1 - 2;

      bound.i0 = bound.i1 - 1;
      bound.i2 = bound.i1 + 1;
      bound.i3 = bound.i2 + 1;
    }
  } else if (bound.i1 == extent - 2) {
    if (bmode == BoundMode::Wrap) {
      bound.i0 = bound.i1 - 1;
      bound.i2 = bound.i1 + 1;
      bound.i3 = 0;
    } else {
      bound.i1 = bound.i1 - 1;

      bound.i0 = bound.i1 - 1;
      bound.i2 = bound.i1 + 1;
      bound.i3 = bound.i2 + 1;
    }
  } else if (bound.i1 == 0) {
    if (bmode == BoundMode::Wrap) {
      bound.i0 = extent - 1;
      bound.i2 = bound.i1 + 1;
      bound.i3 = bound.i2 + 1;
    } else {
      bound.i1 = bound.i1 + 1;

      bound.i0 = bound.i1 - 1;
      bound.i2 = bound.i1 + 1;
      bound.i3 = bound.i2 + 1;
    }
  } else {
    bound.i0 = bound.i1 - 1;
    bound.i2 = bound.i1 + 1;
    bound.i3 = bound.i1 + 2;
  }

  bound.offset += loc - static_cast<LocType>(bound.i1);

  return bound;
}

template <typename Container, typename Pos>
concept interpolation_sequence = requires(Container const& cont, Pos x) {
  typename Container::value_type;
  get_bound2(x, cont.size(), BoundMode::Clamp);
  InterpolationDetail::implicit_result<typename Container::value_type>(typename Container::value_type());
  InterpolationDetail::implicit_result<typename Container::value_type>(cont[0]);
};

template <typename Container, typename Pos, typename WeightOp>
concept linear_sequence_interpolatable = interpolation_sequence<Container, Pos>
  && requires(Container const& cont, Pos x, WeightOp& weight_op, std::remove_cvref_t<decltype(weight_op(x))>& weights) {
       InterpolationDetail::implicit_result<std::remove_cvref_t<decltype(weight_op(x))>>(weight_op(x));
       InterpolationDetail::implicit_result<typename Container::value_type>(cont[0] * weights[0]
                                                                            + cont[0] * weights[1]);
     };

template <typename Container, typename Pos, typename WeightOp>
concept cubic_sequence_interpolatable =
  interpolation_sequence<Container, Pos>
  && requires(
    Container const& cont,
    Pos x) { InterpolationDetail::implicit_result<typename Container::value_type>(cont[0] * (1 - x) + cont[0] * (x)); }
  && requires(Container const& cont, Pos x, WeightOp& weight_op, std::remove_cvref_t<decltype(weight_op(x))>& weights) {
       InterpolationDetail::implicit_result<std::remove_cvref_t<decltype(weight_op(x))>>(weight_op(x));
       InterpolationDetail::implicit_result<typename Container::value_type>(
         cont[0] * weights[0] + cont[0] * weights[1] + cont[0] * weights[2] + cont[0] * weights[3]);
     };

template <typename Container, typename Pos, typename WeightOp>
  requires linear_sequence_interpolatable<Container, Pos, WeightOp>
constexpr typename Container::value_type list_interpolate2(Container const& cont, Pos x, WeightOp&& weight_op,
                                                           BoundMode bmode = BoundMode::Clamp) {
  if (cont.size() == 0) {
    return typename Container::value_type();
  } else if (cont.size() == 1) {
    return cont[0];
  } else {
    auto bound = get_bound2(x, cont.size(), bmode);
    auto weights = weight_op(bound.offset);
    return cont[bound.i0] * weights[0] + cont[bound.i1] * weights[1];
  }
}

// Preserve callback value visibility for the historical fast-math evaluation;
// an outlined forwarding-reference call can change cubic result bits.
template <typename Container, typename Pos, typename WeightOp>
  requires cubic_sequence_interpolatable<Container, Pos, WeightOp>
[[gnu::always_inline]] constexpr typename Container::value_type
list_interpolate4(Container const& cont, Pos x, WeightOp&& weight_op, BoundMode bmode = BoundMode::Clamp) {
  if (cont.size() == 0) {
    return typename Container::value_type();
  } else if (cont.size() == 1) {
    return cont[0];
  } else if (cont.size() < 4 && bmode != BoundMode::Wrap) {
    // Non-periodic short sequences are linear, intentionally bypassing the
    // custom four-weight callback. Keep the established weighted arithmetic.
    auto bound = get_bound2(x, cont.size(), bmode);
    return cont[bound.i0] * (1 - bound.offset) + cont[bound.i1] * (bound.offset);
  } else {
    auto bound = get_bound4(x, cont.size(), bmode);
    auto weights = weight_op(bound.offset);
    return cont[bound.i0] * weights[0] + cont[bound.i1] * weights[1] + cont[bound.i2] * weights[2]
        + cont[bound.i3] * weights[3];
  }
}

// Returns an index value (not integer) that represents the value that, if
// passed in as an index to a simple linear interpolation of the given
// container, would yield the given value.  (In other words, this goes from
// function space to index space on a list of points). Useful for doing
// interpolation on functions that are unevenly spaced.  Given container must
// be sorted.  If there is an ambiguity on points due to repeat points, will
// choose the lower-most of the points.
template <typename Iterator, typename Pos, typename PosGetter>
concept inverse_interpolation_source =
  std::bidirectional_iterator<Iterator> && requires(Iterator i, Pos const& t, PosGetter& get_position) {
    Pos();
    Pos(std::distance(i, i));
    InterpolationDetail::implicit_result<Pos>(get_position(*i));
    InterpolationDetail::implicit_result<Pos>(t - t);
    InterpolationDetail::implicit_result<Pos>(t + (t - t) / t);
    (t == 0) ? 0 : 0;
  };

// lower_bound calls (element, position); upper_bound calls (position, element).
// Do not demand the unused comparison direction or copyability of callbacks.
template <typename Iterator, typename Pos, typename Comp, typename PosGetter>
concept lower_inverse_interpolatable =
  inverse_interpolation_source<Iterator, Pos, PosGetter> && requires(Iterator i, Pos const& t, Comp& comp) {
    std::invoke(comp, *i, t) ? 0 : 0;
    InterpolationDetail::implicit_result<Pos>(t);
  };

template <typename Iterator, typename Pos, typename Comp, typename PosGetter>
concept upper_inverse_interpolatable =
  inverse_interpolation_source<Iterator, Pos, PosGetter> && requires(Iterator i, Pos const& t, Comp& comp) {
    std::invoke(comp, t, *i) ? 0 : 0;
    InterpolationDetail::implicit_result<Pos>(t + 1);
  };

template <typename Iterator, typename Pos, typename Comp, typename PosGetter>
  requires lower_inverse_interpolatable<Iterator, Pos, Comp, PosGetter>
constexpr Pos inverse_linear_interpolate_lower(Iterator begin, Iterator end, Pos t, Comp&& comp,
                                               PosGetter&& pos_getter) {
  // Container must be at least size 2 for this to make sense.
  if (begin == end || std::next(begin) == end)
    return Pos();

  Iterator i = std::lower_bound(std::next(begin), std::prev(end), t, std::ref(comp));

  --i;
  Pos min = pos_getter(*i);
  Pos max = pos_getter(*(++i));
  Pos ipos = Pos(std::distance(begin, --i));

  Pos dist = max - min;
  if (dist == 0)
    return ipos;
  else
    return ipos + (t - min) / dist;
}

template <typename Iterator, typename Pos>
  requires lower_inverse_interpolatable<Iterator, Pos, std::less<Pos>, std::identity>
constexpr Pos inverse_linear_interpolate_lower(Iterator begin, Iterator end, Pos t) {
  return inverse_linear_interpolate_lower(begin, end, t, std::less<Pos>(), std::identity());
}

// Same inverse mapping, choosing the upper-most repeated point.
template <typename Iterator, typename Pos, typename Comp, typename PosGetter>
  requires upper_inverse_interpolatable<Iterator, Pos, Comp, PosGetter>
constexpr Pos inverse_linear_interpolate_upper(Iterator begin, Iterator end, Pos t, Comp&& comp,
                                               PosGetter&& pos_getter) {
  // Container must be at least size 2 for this to make sense.
  if (begin == end || std::next(begin) == end)
    return Pos();

  Iterator i = std::upper_bound(std::next(begin), std::prev(end), t, std::ref(comp));

  --i;
  Pos min = pos_getter(*i);
  Pos max = pos_getter(*(++i));
  Pos ipos = Pos(std::distance(begin, --i));

  Pos dist = max - min;
  if (dist == 0)
    return ipos + 1;
  else
    return ipos + (t - min) / dist;
}

template <typename Iterator, typename Pos>
  requires upper_inverse_interpolatable<Iterator, Pos, std::less<Pos>, std::identity>
constexpr Pos inverse_linear_interpolate_upper(Iterator begin, Iterator end, Pos t) {
  return inverse_linear_interpolate_upper(begin, end, t, std::less<Pos>(), std::identity());
}

template <typename XContainer, typename YContainer, typename Position>
concept parametric_interpolation_source =
  requires(XContainer const& xvals, YContainer const& yvals, Position const& position) {
    (xvals.size() != 0) ? 0 : 0;
    (xvals.size() == yvals.size()) ? 0 : 0;
    inverse_linear_interpolate_lower(xvals.begin(), xvals.end(), position);
  };

template <typename XContainer, typename YContainer, typename PositionType, typename WeightOp>
  requires parametric_interpolation_source<XContainer, YContainer, PositionType>
  && linear_sequence_interpolatable<YContainer, PositionType, WeightOp>
constexpr typename YContainer::value_type parametric_interpolate2(XContainer const& xvals, YContainer const& yvals,
                                                                  PositionType const& position, WeightOp&& weight_op,
                                                                  BoundMode bmode) {
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((xvals.size() != 0));
  }
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((xvals.size() == yvals.size()));
  }

  if (yvals.size() == 1)
    return yvals[0];

  PositionType ipos = inverse_linear_interpolate_lower(xvals.begin(), xvals.end(), position);

  return list_interpolate2(yvals, ipos, std::forward<WeightOp>(weight_op), bmode);
}

template <typename XContainer, typename YContainer, typename PositionType, typename WeightOp>
  requires parametric_interpolation_source<XContainer, YContainer, PositionType>
  && cubic_sequence_interpolatable<YContainer, PositionType, WeightOp>
constexpr typename YContainer::value_type parametric_interpolate4(XContainer const& xvals, YContainer const& yvals,
                                                                  PositionType const& position, WeightOp&& weight_op,
                                                                  BoundMode bmode) {
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((xvals.size() != 0));
  }
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((xvals.size() == yvals.size()));
  }

  if (yvals.size() == 1)
    return yvals[0];

  PositionType ipos = inverse_linear_interpolate_lower(xvals.begin(), xvals.end(), position);

  return list_interpolate4(yvals, ipos, std::forward<WeightOp>(weight_op), bmode);
}
}

export module star.interpolation;

export namespace Star {
using ::Star::angle_lerp;
using ::Star::angle_lerp_compatible;
using ::Star::Bound2;
using ::Star::Bound4;
using ::Star::BoundMode;
using ::Star::cubic4;
using ::Star::cubic4_compatible;
using ::Star::Cubic4WeightOperator;
using ::Star::cubic_sequence_interpolatable;
using ::Star::cubic_weight;
using ::Star::get_bound2;
using ::Star::get_bound4;
using ::Star::half_step;
using ::Star::half_step_compatible;
using ::Star::hermite2;
using ::Star::hermite2_compatible;
using ::Star::interpolation_location;
using ::Star::interpolation_sequence;
using ::Star::InterpolationMode;
using ::Star::inverse_interpolation_source;
using ::Star::inverse_linear_interpolate_lower;
using ::Star::inverse_linear_interpolate_upper;
using ::Star::lerp;
using ::Star::lerp_compatible;
using ::Star::lerp_with_limit;
using ::Star::limited_lerp_compatible;
using ::Star::linear_sequence_interpolatable;
using ::Star::linear_weight;
using ::Star::LinearWeightOperator;
using ::Star::list_interpolate2;
using ::Star::list_interpolate4;
using ::Star::lower_inverse_interpolatable;
using ::Star::parametric_interpolate2;
using ::Star::parametric_interpolate4;
using ::Star::parametric_interpolation_source;
using ::Star::quintic2;
using ::Star::quintic2_compatible;
using ::Star::sin_ease;
using ::Star::sin_ease_compatible;
using ::Star::sin_weight;
using ::Star::SinWeightOperator;
using ::Star::step;
using ::Star::step_compatible;
using ::Star::step_weight;
using ::Star::StepWeightOperator;
using ::Star::upper_inverse_interpolatable;
}
