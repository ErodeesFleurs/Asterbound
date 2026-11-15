module;
import std;
import star.multi_array;
import star.interpolation;

import star.array;
namespace Star {

namespace multi_array_interpolation_detail {
template <typename T> void implicit_result(T) noexcept;
}

template <typename Function, typename Position, std::size_t Taps>
concept tensor_weight_function =
  std::regular_invocable<Function const&, Position> && requires(Function const& function, Position offset) {
    multi_array_interpolation_detail::implicit_result<Array<Position, Taps>>(function(Position(offset)));
  };

template <typename Element, typename Position>
concept tensor_linear_value = requires(Element const& value, Position weight) {
  Element{};
  multi_array_interpolation_detail::implicit_result<Element>(weight * value + weight * value);
};

template <typename Element, typename Position>
concept tensor_cubic_value = requires(Element const& value, Position weight) {
  Element{};
  multi_array_interpolation_detail::implicit_result<Element>(weight * value + weight * value + weight * value
                                                             + weight * value);
};

template <typename Element, typename Position>
concept tensor_piecewise_value = requires(Element& result, Element const& value, Position weight) {
  Element{};
  multi_array_interpolation_detail::implicit_result<Element>(value);
  result = value;
  result += value * (weight - 1) + value * weight;
};

// Weight callables are deterministic, side-effect-free mathematical functions
// invoked through const references. Weights are evaluated once per axis. Empty
// arrays return a default-valued element; singleton and nonperiodic cubic axes
// shorter than four use linear weights instead of the four-weight callable.

template <typename MultiArrayT, typename PositionT, typename WeightFunctionT = LinearWeightOperator<PositionT>>
  requires tensor_weight_function<WeightFunctionT, PositionT, 2> && (MultiArrayT::Rank > 0)
struct MultiArrayInterpolator2 {
  using MultiArray = MultiArrayT;
  using Position = PositionT;

  using Element = typename MultiArray::Element;
  static std::size_t const Rank = MultiArray::Rank;

  using IndexList = Array<std::size_t, Rank>;
  using SizeList = Array<std::size_t, Rank>;
  using PositionList = Array<Position, Rank>;
  using WeightList = Array<Position, 2>;

  using WeightFunction = WeightFunctionT;

  WeightFunction weight_function;
  BoundMode bound_mode;

  MultiArrayInterpolator2(WeightFunction wf, BoundMode b = BoundMode::Clamp)
      : weight_function(std::move(wf)), bound_mode(b) {}

  Element interpolate(MultiArray const& array, PositionList const& coord) const
    requires tensor_linear_value<Element, Position> && requires {
      array.view();
      get_bound2(coord[0], array.size(0), BoundMode::Clamp);
    }
  {
    if (array.count() == 0)
      return Element{};

    IndexList imin;
    IndexList imax;
    Array<WeightList, Rank> axis_weights;

    for (std::size_t i = 0; i < Rank; ++i) {
      auto binfo = get_bound2(coord[i], array.size(i), bound_mode);
      imin[i] = binfo.i0;
      imax[i] = binfo.i1;
      axis_weights[i] = weight_function(Position(binfo.offset));
    }

    return interpolate_sub(array.view(), imin, imax, axis_weights, IndexList(), 0);
  }

private:
  Element interpolate_sub(typename MultiArray::ConstView const& array, IndexList const& imin, IndexList const& imax,
                          Array<WeightList, Rank> const& axis_weights, IndexList const& index,
                          std::size_t const dim) const {
    IndexList minIndex = index;
    IndexList maxIndex = index;

    minIndex[dim] = imin[dim];
    maxIndex[dim] = imax[dim];

    auto const& weights = axis_weights[dim];

    if (dim == Rank - 1) {
      return weights[0] * array[std::span<std::size_t const, Rank>(minIndex)] +
        weights[1] * array[std::span<std::size_t const, Rank>(maxIndex)];
    } else {
      return weights[0] * interpolate_sub(array, imin, imax, axis_weights, minIndex, dim + 1)
        + weights[1] * interpolate_sub(array, imin, imax, axis_weights, maxIndex, dim + 1);
    }
  }
};

template <typename MultiArrayT, typename PositionT, typename WeightFunctionT = Cubic4WeightOperator<PositionT>>
  requires tensor_weight_function<WeightFunctionT, PositionT, 4> && (MultiArrayT::Rank > 0)
struct MultiArrayInterpolator4 {
  using MultiArray = MultiArrayT;
  using Position = PositionT;

  using Element = typename MultiArray::Element;
  static std::size_t const Rank = MultiArray::Rank;

  using IndexList = Array<std::size_t, Rank>;
  using SizeList = Array<std::size_t, Rank>;
  using PositionList = Array<Position, Rank>;
  using WeightList = Array<Position, 4>;

  using WeightFunction = WeightFunctionT;

  WeightFunction weight_function;
  BoundMode bound_mode;

  MultiArrayInterpolator4(WeightFunction wf, BoundMode b = BoundMode::Clamp)
      : weight_function(std::move(wf)), bound_mode(b) {}

  Element interpolate(MultiArray const& array, PositionList const& coord) const
    requires tensor_cubic_value<Element, Position> && requires {
      array.view();
      get_bound4(coord[0], array.size(0), BoundMode::Clamp);
    }
  {
    if (array.count() == 0)
      return Element{};

    IndexList index0;
    IndexList index1;
    IndexList index2;
    IndexList index3;
    Array<WeightList, Rank> axis_weights;

    for (std::size_t i = 0; i < Rank; ++i) {
      auto bound = get_bound4(coord[i], array.size(i), bound_mode);
      index0[i] = bound.i0;
      index1[i] = bound.i1;
      index2[i] = bound.i2;
      index3[i] = bound.i3;
      axis_weights[i] = array.size(i) == 1 || (array.size(i) < 4 && bound_mode != BoundMode::Wrap)
        ? WeightList{0, 1 - bound.offset, bound.offset, 0}
        : weight_function(Position(bound.offset));
    }

    return interpolate_sub(array.view(), index0, index1, index2, index3, axis_weights, IndexList(), 0);
  }

private:
  Element interpolate_sub(typename MultiArray::ConstView const& array, IndexList const& i0, IndexList const& i1,
                          IndexList const& i2, IndexList const& i3, Array<WeightList, Rank> const& axis_weights,
                          IndexList const& index, std::size_t const dim) const {
    IndexList index0 = index;
    IndexList index1 = index;
    IndexList index2 = index;
    IndexList index3 = index;

    index0[dim] = i0[dim];
    index1[dim] = i1[dim];
    index2[dim] = i2[dim];
    index3[dim] = i3[dim];

    auto const& weights = axis_weights[dim];

    if (dim == Rank - 1) {
      return
        weights[0] * array[std::span<std::size_t const, Rank>(index0)] +
        weights[1] * array[std::span<std::size_t const, Rank>(index1)] +
        weights[2] * array[std::span<std::size_t const, Rank>(index2)] +
        weights[3] * array[std::span<std::size_t const, Rank>(index3)];
    } else {
      return weights[0] * interpolate_sub(array, i0, i1, i2, i3, axis_weights, index0, dim + 1)
        + weights[1] * interpolate_sub(array, i0, i1, i2, i3, axis_weights, index1, dim + 1)
        + weights[2] * interpolate_sub(array, i0, i1, i2, i3, axis_weights, index2, dim + 1)
        + weights[3] * interpolate_sub(array, i0, i1, i2, i3, axis_weights, index3, dim + 1);
    }
  }
};

template <typename MultiArrayT, typename PositionT, typename WeightFunctionT = StepWeightOperator<PositionT>>
  requires tensor_weight_function<WeightFunctionT, PositionT, 2> && (MultiArrayT::Rank > 0)
struct MultiArrayPiecewiseInterpolator {
  using MultiArray = MultiArrayT;
  using Position = PositionT;

  using Element = typename MultiArray::Element;
  static std::size_t const Rank = MultiArray::Rank;

  using IndexList = Array<std::size_t, Rank>;
  using SizeList = Array<std::size_t, Rank>;
  using PositionList = Array<Position, Rank>;
  using WeightList = Array<Position, 2>;

  using WeightFunction = WeightFunctionT;

  struct PiecewiseRange {
    std::size_t dim;
    Position offset;

    bool operator<(PiecewiseRange const& pr) const {
      return pr.offset < offset;
    }
  };
  using PiecewiseRangeList = Array<PiecewiseRange, Rank>;

  WeightFunction weight_function;
  BoundMode bound_mode;

  MultiArrayPiecewiseInterpolator(WeightFunction wf, BoundMode b = BoundMode::Clamp)
      : weight_function(std::move(wf)), bound_mode(b) {}

  // O(n log n) ordering and O(n) sampling for n dimensions.
  Element interpolate(MultiArray const& array, PositionList const& coord) const
    requires tensor_piecewise_value<Element, Position> && requires {
      array.view();
      get_bound2(coord[0], array.size(0), BoundMode::Clamp);
    }
  {
    if (array.count() == 0)
      return Element{};

    PiecewiseRangeList piecewiseRangeList;

    IndexList minIndex;
    IndexList maxIndex;

    for (std::size_t i = 0; i < Rank; ++i) {
        PiecewiseRange range;
        range.dim = i;

        auto bound = get_bound2(coord[i], array.size(i), bound_mode);
        minIndex[i] = bound.i0;
        maxIndex[i] = bound.i1;
        range.offset = bound.offset;

        piecewiseRangeList[i] = range;
    }

    std::sort(piecewiseRangeList.begin(), piecewiseRangeList.end());
    auto const view = array.view();

    IndexList location = minIndex;
    Element result = view[std::span<std::size_t const, Rank>(location)];
    Element last = result;
    Element current;

    for (std::size_t i = 0; i < Rank; ++i) {
      auto const& pr = piecewiseRangeList[i];
      location[pr.dim] = maxIndex[pr.dim];
      current = view[std::span<std::size_t const, Rank>(location)];

      WeightList weights = weight_function(Position(pr.offset));
      result += last * (weights[0] - 1) + current * weights[1];
      last = current;
    }

    return result;
  }
};

// Template specializations for Rank 2

template <typename ElementT, typename PositionT, typename WeightFunctionT>
  requires tensor_weight_function<WeightFunctionT, PositionT, 2>
struct MultiArrayInterpolator2<MultiArray<ElementT, 2>, PositionT, WeightFunctionT> {
  using MultiArray = Star::MultiArray<ElementT, 2>;
  using Position = PositionT;

  using Element = typename MultiArray::Element;
  static std::size_t const Rank = 2;

  using IndexList = Array<std::size_t, Rank>;
  using SizeList = Array<std::size_t, Rank>;
  using PositionList = Array<Position, Rank>;
  using WeightList = Array<Position, 2>;

  using WeightFunction = WeightFunctionT;

  WeightFunction weight_function;
  BoundMode bound_mode;

  MultiArrayInterpolator2(WeightFunction wf, BoundMode b = BoundMode::Clamp)
      : weight_function(std::move(wf)), bound_mode(b) {}

  Element interpolate(MultiArray const& array, PositionList const& coord) const
    requires tensor_linear_value<Element, Position> && requires {
      array.view();
      get_bound2(coord[0], array.size(0), BoundMode::Clamp);
    }
  {
    if (array.count() == 0)
      return Element{};

    IndexList imin;
    IndexList imax;
    PositionList offset;

    for (std::size_t i = 0; i < Rank; ++i) {
      auto bound = get_bound2(coord[i], array.size(i), bound_mode);
      imin[i] = bound.i0;
      imax[i] = bound.i1;
      offset[i] = bound.offset;
    }

    WeightList xweights = weight_function(Position(offset[0]));
    WeightList yweights = weight_function(Position(offset[1]));

    auto const view = array.view();
    {
#if defined(__clang__)
#pragma clang fp reassociate(off)
#endif
      // Typed weights may inline; preserve the historical weighted-sum grouping.
      return xweights[0] * (yweights[0] * view[imin[0], imin[1]] + yweights[1] * view[imin[0], imax[1]])
        + xweights[1] * (yweights[0] * view[imax[0], imin[1]] + yweights[1] * view[imax[0], imax[1]]);
    }
  }
};

template <typename ElementT, typename PositionT, typename WeightFunctionT>
  requires tensor_weight_function<WeightFunctionT, PositionT, 4>
struct MultiArrayInterpolator4<MultiArray<ElementT, 2>, PositionT, WeightFunctionT> {
  using MultiArray = Star::MultiArray<ElementT, 2>;
  using Position = PositionT;

  using Element = typename MultiArray::Element;
  static std::size_t const Rank = 2;

  using IndexList = Array<std::size_t, Rank>;
  using SizeList = Array<std::size_t, Rank>;
  using PositionList = Array<Position, Rank>;
  using WeightList = Array<Position, 4>;

  using WeightFunction = WeightFunctionT;

  WeightFunction weight_function;
  BoundMode bound_mode;

  MultiArrayInterpolator4(WeightFunction wf, BoundMode b = BoundMode::Clamp)
      : weight_function(std::move(wf)), bound_mode(b) {}

  Element interpolate(MultiArray const& array, PositionList const& coord) const
    requires tensor_cubic_value<Element, Position> && requires {
      array.view();
      get_bound4(coord[0], array.size(0), BoundMode::Clamp);
    }
  {
    if (array.count() == 0)
      return Element{};

    IndexList index0;
    IndexList index1;
    IndexList index2;
    IndexList index3;
    PositionList offset;

    for (std::size_t i = 0; i < Rank; ++i) {
      auto bound = get_bound4(coord[i], array.size(i), bound_mode);
      index0[i] = bound.i0;
      index1[i] = bound.i1;
      index2[i] = bound.i2;
      index3[i] = bound.i3;
      offset[i] = bound.offset;
    }

    auto weights_for_axis = [&](std::size_t axis) {
      // Duplicated first taps encode singleton or non-periodic short axes.
      return index0[axis] == index1[axis] ? WeightList{0, 1 - offset[axis], offset[axis], 0}
                                          : weight_function(Position(offset[axis]));
    };
    WeightList xweights = weights_for_axis(0);
    WeightList yweights = weights_for_axis(1);
    auto const view = array.view();

    return
      xweights[0] * (
          yweights[0] * view[index0[0], index0[1]] +
          yweights[1] * view[index0[0], index1[1]] +
          yweights[2] * view[index0[0], index2[1]] +
          yweights[3] * view[index0[0], index3[1]]
        ) +
      xweights[1] * (
          yweights[0] * view[index1[0], index0[1]] +
          yweights[1] * view[index1[0], index1[1]] +
          yweights[2] * view[index1[0], index2[1]] +
          yweights[3] * view[index1[0], index3[1]]
        ) +
      xweights[2] * (
          yweights[0] * view[index2[0], index0[1]] +
          yweights[1] * view[index2[0], index1[1]] +
          yweights[2] * view[index2[0], index2[1]] +
          yweights[3] * view[index2[0], index3[1]]
        ) +
      xweights[3] * (
          yweights[0] * view[index3[0], index0[1]] +
          yweights[1] * view[index3[0], index1[1]] +
          yweights[2] * view[index3[0], index2[1]] +
          yweights[3] * view[index3[0], index3[1]]
        );
  }
};

// Template specializations for Rank 3

template <typename ElementT, typename PositionT, typename WeightFunctionT>
  requires tensor_weight_function<WeightFunctionT, PositionT, 2>
struct MultiArrayInterpolator2<MultiArray<ElementT, 3>, PositionT, WeightFunctionT> {
  using MultiArray = Star::MultiArray<ElementT, 3>;
  using Position = PositionT;

  using Element = typename MultiArray::Element;
  static std::size_t const Rank = 3;

  using IndexList = Array<std::size_t, Rank>;
  using SizeList = Array<std::size_t, Rank>;
  using PositionList = Array<Position, Rank>;
  using WeightList = Array<Position, 2>;

  using WeightFunction = WeightFunctionT;

  WeightFunction weight_function;
  BoundMode bound_mode;

  MultiArrayInterpolator2(WeightFunction wf, BoundMode b = BoundMode::Clamp)
      : weight_function(std::move(wf)), bound_mode(b) {}

  Element interpolate(MultiArray const& array, PositionList const& coord) const
    requires tensor_linear_value<Element, Position> && requires {
      array.view();
      get_bound2(coord[0], array.size(0), BoundMode::Clamp);
    }
  {
    if (array.count() == 0)
      return Element{};

    IndexList imin;
    IndexList imax;
    PositionList offset;

    for (std::size_t i = 0; i < Rank; ++i) {
      auto bound = get_bound2(coord[i], array.size(i), bound_mode);
      imin[i] = bound.i0;
      imax[i] = bound.i1;
      offset[i] = bound.offset;
    }

    WeightList xweights = weight_function(Position(offset[0]));
    WeightList yweights = weight_function(Position(offset[1]));
    WeightList zweights = weight_function(Position(offset[2]));
    auto const view = array.view();

    {
#if defined(__clang__)
#pragma clang fp reassociate(off)
#endif
      // Typed weights may inline; preserve the historical weighted-sum grouping.
      return xweights[0]
        * (yweights[0] * (zweights[0] * view[imin[0], imin[1], imin[2]] + zweights[1] * view[imin[0], imin[1], imax[2]])
           + yweights[1]
             * (zweights[0] * view[imin[0], imax[1], imin[2]] + zweights[1] * view[imin[0], imax[1], imax[2]]))
        + xweights[1]
        * (yweights[0] * (zweights[0] * view[imax[0], imin[1], imin[2]] + zweights[1] * view[imax[0], imin[1], imax[2]])
           + yweights[1]
             * (zweights[0] * view[imax[0], imax[1], imin[2]] + zweights[1] * view[imax[0], imax[1], imax[2]]));
    }
  }
};

template <typename ElementT, typename PositionT, typename WeightFunctionT>
  requires tensor_weight_function<WeightFunctionT, PositionT, 4>
struct MultiArrayInterpolator4<MultiArray<ElementT, 3>, PositionT, WeightFunctionT> {
  using MultiArray = Star::MultiArray<ElementT, 3>;
  using Position = PositionT;

  using Element = typename MultiArray::Element;
  static std::size_t const Rank = 3;

  using IndexList = Array<std::size_t, Rank>;
  using SizeList = Array<std::size_t, Rank>;
  using PositionList = Array<Position, Rank>;
  using WeightList = Array<Position, 4>;

  using WeightFunction = WeightFunctionT;

  WeightFunction weight_function;
  BoundMode bound_mode;

  MultiArrayInterpolator4(WeightFunction wf, BoundMode b = BoundMode::Clamp)
      : weight_function(std::move(wf)), bound_mode(b) {}

  Element interpolate(MultiArray const& array, PositionList const& coord) const
    requires tensor_cubic_value<Element, Position> && requires {
      array.view();
      get_bound4(coord[0], array.size(0), BoundMode::Clamp);
    }
  {
    if (array.count() == 0)
      return Element{};

    IndexList index0;
    IndexList index1;
    IndexList index2;
    IndexList index3;
    PositionList offset;

    for (std::size_t i = 0; i < Rank; ++i) {
      auto bound = get_bound4(coord[i], array.size(i), bound_mode);
      index0[i] = bound.i0;
      index1[i] = bound.i1;
      index2[i] = bound.i2;
      index3[i] = bound.i3;
      offset[i] = bound.offset;
    }

    auto weights_for_axis = [&](std::size_t axis) {
      // Duplicated first taps encode singleton or non-periodic short axes.
      return index0[axis] == index1[axis] ? WeightList{0, 1 - offset[axis], offset[axis], 0}
                                          : weight_function(Position(offset[axis]));
    };
    WeightList xweights = weights_for_axis(0);
    WeightList yweights = weights_for_axis(1);
    WeightList zweights = weights_for_axis(2);
    auto const view = array.view();

    return
      xweights[0] * (
            yweights[0] * (
              zweights[0] * view[index0[0], index0[1], index0[2]] +
              zweights[1] * view[index0[0], index0[1], index1[2]] +
              zweights[2] * view[index0[0], index0[1], index2[2]] +
              zweights[3] * view[index0[0], index0[1], index3[2]]
            ) +
            yweights[1] * (
              zweights[0] * view[index0[0], index1[1], index0[2]] +
              zweights[1] * view[index0[0], index1[1], index1[2]] +
              zweights[2] * view[index0[0], index1[1], index2[2]] +
              zweights[3] * view[index0[0], index1[1], index3[2]]
            ) +
            yweights[2] * (
              zweights[0] * view[index0[0], index2[1], index0[2]] +
              zweights[1] * view[index0[0], index2[1], index1[2]] +
              zweights[2] * view[index0[0], index2[1], index2[2]] +
              zweights[3] * view[index0[0], index2[1], index3[2]]
            ) +
            yweights[3] * (
              zweights[0] * view[index0[0], index3[1], index0[2]] +
              zweights[1] * view[index0[0], index3[1], index1[2]] +
              zweights[2] * view[index0[0], index3[1], index2[2]] +
              zweights[3] * view[index0[0], index3[1], index3[2]]
            )
          ) +
      xweights[1] * (
            yweights[0] * (
              zweights[0] * view[index1[0], index0[1], index0[2]] +
              zweights[1] * view[index1[0], index0[1], index1[2]] +
              zweights[2] * view[index1[0], index0[1], index2[2]] +
              zweights[3] * view[index1[0], index0[1], index3[2]]
            ) +
            yweights[1] * (
              zweights[0] * view[index1[0], index1[1], index0[2]] +
              zweights[1] * view[index1[0], index1[1], index1[2]] +
              zweights[2] * view[index1[0], index1[1], index2[2]] +
              zweights[3] * view[index1[0], index1[1], index3[2]]
            ) +
            yweights[2] * (
              zweights[0] * view[index1[0], index2[1], index0[2]] +
              zweights[1] * view[index1[0], index2[1], index1[2]] +
              zweights[2] * view[index1[0], index2[1], index2[2]] +
              zweights[3] * view[index1[0], index2[1], index3[2]]
            ) +
            yweights[3] * (
              zweights[0] * view[index1[0], index3[1], index0[2]] +
              zweights[1] * view[index1[0], index3[1], index1[2]] +
              zweights[2] * view[index1[0], index3[1], index2[2]] +
              zweights[3] * view[index1[0], index3[1], index3[2]]
            )
          ) +
      xweights[2] * (
            yweights[0] * (
              zweights[0] * view[index2[0], index0[1], index0[2]] +
              zweights[1] * view[index2[0], index0[1], index1[2]] +
              zweights[2] * view[index2[0], index0[1], index2[2]] +
              zweights[3] * view[index2[0], index0[1], index3[2]]
            ) +
            yweights[1] * (
              zweights[0] * view[index2[0], index1[1], index0[2]] +
              zweights[1] * view[index2[0], index1[1], index1[2]] +
              zweights[2] * view[index2[0], index1[1], index2[2]] +
              zweights[3] * view[index2[0], index1[1], index3[2]]
            ) +
            yweights[2] * (
              zweights[0] * view[index2[0], index2[1], index0[2]] +
              zweights[1] * view[index2[0], index2[1], index1[2]] +
              zweights[2] * view[index2[0], index2[1], index2[2]] +
              zweights[3] * view[index2[0], index2[1], index3[2]]
            ) +
            yweights[3] * (
              zweights[0] * view[index2[0], index3[1], index0[2]] +
              zweights[1] * view[index2[0], index3[1], index1[2]] +
              zweights[2] * view[index2[0], index3[1], index2[2]] +
              zweights[3] * view[index2[0], index3[1], index3[2]]
            )
          ) +
      xweights[3] * (
            yweights[0] * (
              zweights[0] * view[index3[0], index0[1], index0[2]] +
              zweights[1] * view[index3[0], index0[1], index1[2]] +
              zweights[2] * view[index3[0], index0[1], index2[2]] +
              zweights[3] * view[index3[0], index0[1], index3[2]]
            ) +
            yweights[1] * (
              zweights[0] * view[index3[0], index1[1], index0[2]] +
              zweights[1] * view[index3[0], index1[1], index1[2]] +
              zweights[2] * view[index3[0], index1[1], index2[2]] +
              zweights[3] * view[index3[0], index1[1], index3[2]]
            ) +
            yweights[2] * (
              zweights[0] * view[index3[0], index2[1], index0[2]] +
              zweights[1] * view[index3[0], index2[1], index1[2]] +
              zweights[2] * view[index3[0], index2[1], index2[2]] +
              zweights[3] * view[index3[0], index2[1], index3[2]]
            ) +
            yweights[3] * (
              zweights[0] * view[index3[0], index3[1], index0[2]] +
              zweights[1] * view[index3[0], index3[1], index1[2]] +
              zweights[2] * view[index3[0], index3[1], index2[2]] +
              zweights[3] * view[index3[0], index3[1], index3[2]]
            )
          );
  }
};
}

export module star.multi_array_interpolator;

export namespace Star {
  using ::Star::MultiArrayInterpolator2;
  using ::Star::MultiArrayInterpolator4;
  using ::Star::MultiArrayPiecewiseInterpolator;
  using ::Star::tensor_cubic_value;
  using ::Star::tensor_linear_value;
  using ::Star::tensor_piecewise_value;
  using ::Star::tensor_weight_function;
}
