module;
import std;
import star.format;
import star.math_common;
import star.vector;

namespace Star {

template <typename Visitor>
concept block_visitor = requires(Visitor& visitor) {
  { std::invoke(visitor, int{}, int{}) } -> std::same_as<std::expected<bool, StarException>>;
};

namespace BlockLineDetail {
  template <block_visitor Visitor>
  bool visitor_present(Visitor const& visitor) {
    if constexpr (std::is_pointer_v<Visitor>)
      return visitor != nullptr;
    else
      return true;
  }

  template <typename Signature>
    requires block_visitor<std::function<Signature>>
  bool visitor_present(std::function<Signature> const& visitor) {
    return static_cast<bool>(visitor);
  }

  inline bool in_cell_range(double coordinate) {
    return coordinate >= std::numeric_limits<int>::min()
        && coordinate < double(std::numeric_limits<int>::max()) + 1.0;
  }
}

// Visit both floored endpoints; at an interior corner step diagonally. False
// stops at the current cell, and a visitor error is returned unchanged.
template <finite_classifiable Scalar, block_visitor Visitor>
[[nodiscard]] std::expected<bool, StarException> for_each_block_along_line(
    Vector<Scalar, 2> const& origin, Vector<Scalar, 2> const& delta, Visitor&& visitor) {
  if (!BlockLineDetail::visitor_present(visitor))
    return std::unexpected(StarException("Empty block traversal callback"));

  // Check the inputs before any fast-math arithmetic: using a NaN or
  // infinity in origin + delta can invalidate even a later bitwise check.
  auto finite = classify_finite(origin[0], origin[1], delta[0], delta[1]);
  if (std::ranges::find(finite, false) != finite.end()
      || !BlockLineDetail::in_cell_range(origin[0]) || !BlockLineDetail::in_cell_range(origin[1]))
    return std::unexpected(OutOfRangeException("Block traversal exceeds the finite cell range"));

  // With an int-range origin, adding any finite float/double delta cannot
  // overflow its format. Round in the caller's precision before flooring.
  Vector<Scalar, 2> const end = origin + delta;
  if (!BlockLineDetail::in_cell_range(end[0]) || !BlockLineDetail::in_cell_range(end[1]))
    return std::unexpected(OutOfRangeException("Block traversal exceeds the finite cell range"));

  int x = static_cast<int>(std::floor(double(origin[0])));
  int y = static_cast<int>(std::floor(double(origin[1])));
  int const end_x = static_cast<int>(std::floor(double(end[0])));
  int const end_y = static_cast<int>(std::floor(double(end[1])));
  int const step_x = x < end_x ? 1 : -1;
  int const step_y = y < end_y ? 1 : -1;
  double const dx = std::abs(double(delta[0]));
  double const dy = std::abs(double(delta[1]));
  double const first_x = step_x > 0 ? (double(x) + 1) - origin[0] : double(origin[0]) - x;
  double const first_y = step_y > 0 ? (double(y) + 1) - origin[1] : double(origin[1]) - y;
  double error = first_x * dy - first_y * dx;

  for (;;) {
    auto visited = std::invoke(visitor, int(x), int(y));
    if (!visited || !*visited)
      return visited;

    // At an exact endpoint corner only the axes whose floored cells differ
    // may advance. This also prevents stepping past either integer limit.
    if (x == end_x) {
      if (y == end_y)
        return true;
      y += step_y;
    } else if (y == end_y) {
      x += step_x;
    } else if (error > 0) {
      y += step_y;
      error -= dx;
    } else if (error < 0) {
      x += step_x;
      error += dy;
    } else {
      x += step_x;
      y += step_y;
      error = dy - dx;
    }
  }
}

}

export module star.blocks_along_line;

export namespace Star {
  using ::Star::block_visitor;
  using ::Star::for_each_block_along_line;
}
