import std;
import star.chat_bubble_separation;
import star.rect;
import star.vector;

namespace Star::bubble_detail {
namespace {
float left_edge(RectF const& box) { return box.x_min(); }
float right_edge(RectF const& box) { return box.x_max(); }
}

RectF EdgeIndex::separate(RectF box, std::optional<std::reference_wrapper<RectF const>> previous) const {
  // Every strict X overlap is in BOTH prefixes. Scan only the smaller one;
  // stopping at an intervening non-overlap could hide a containing rectangle.
  auto left_count = std::ranges::lower_bound(m_left, box.x_max(), std::less{}, left_edge) - m_left.begin();
  auto right_count = std::ranges::lower_bound(m_right, box.x_min(), std::greater{}, right_edge) - m_right.begin();
  auto candidates = left_count <= right_count ? std::span(m_left).first(left_count) : std::span(m_right).first(right_count);
  auto width = box.width();
  while (true) {
    auto height = box.height();
    auto center_y = box.center()[1];
    auto next_bottom = box.y_min();
    bool skipped_self = false;
    for (auto const& other : candidates) {
      if (!other.intersects(box, false))
        continue;
      if (!skipped_self && previous && other == previous->get()) {
        skipped_self = true;
        continue;
      }
      auto other_width = other.width();
      if (other_width < width)
        continue;
      if (other_width == width) {
        auto other_height = other.height();
        if (other_height < height || (other_height == height && other.center()[1] > center_y))
          continue;
      }
      next_bottom = std::max(next_bottom, other.y_max());
    }
    if (next_bottom == box.y_min())
      return box;
    // Move above the union of this iteration's box and its blockers.
    next_bottom = std::max(next_bottom, box.y_max());
    box.set_y_min(next_bottom);
    box.set_y_max(next_bottom + height);
  }
}

void EdgeIndex::insert(RectF const& box) {
  m_left.insert(std::ranges::upper_bound(m_left, box.x_min(), std::less{}, left_edge), box);
  m_right.insert(std::ranges::upper_bound(m_right, box.x_max(), std::greater{}, right_edge), box);
}

void EdgeIndex::clear() {
  m_left.clear();
  m_right.clear();
}

void EdgeIndex::append(RectF const& box) {
  m_left.push_back(box);
  m_right.push_back(box);
}

void EdgeIndex::sort() {
  std::ranges::sort(m_left, std::less{}, left_edge);
  std::ranges::sort(m_right, std::greater{}, right_edge);
}

}
