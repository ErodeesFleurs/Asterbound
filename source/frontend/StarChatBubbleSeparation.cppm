module;

import std;
import star.rect;
import star.list;
import star.format;

import star.vector;
namespace Star {

template <std::movable T>
struct BubbleState {
  T contents;
  Vec2F ideal_destination;
  Vec2F current_destination;
  RectF bound_box;
  Vec2F separated_offset;
  RectF separated_box;
  Vec2F current_offset;
  Vec2F previous_offset;
  bool presentation_continuous = false;

  Vec2F sample_offset(float alpha) const {
    if (!presentation_continuous)
      return current_offset;
    return previous_offset + std::clamp(alpha, 0.0f, 1.0f) * (current_offset - previous_offset);
  }

  void reset_presentation() {
    previous_offset = current_offset;
    presentation_continuous = false;
  }
};

namespace bubble_detail {

class EdgeIndex {
public:
  RectF separate(RectF box, std::optional<std::reference_wrapper<RectF const>> previous = {}) const;
  void insert(RectF const& box);
  void clear();
  void append(RectF const& box);
  void sort();

private:
  std::vector<RectF> m_left;
  std::vector<RectF> m_right;
};

template <typename Function, typename State, typename Value>
concept bubble_visitor = requires(Function& visit, State& state, Value& value) {
  { std::invoke(visit, state, value) } -> std::same_as<std::expected<void, StarException>>;
};

}

template <std::movable T>
class BubbleSeparator {
public:
  using Bubble = BubbleState<T>;

  explicit BubbleSeparator(float tween_factor = 0.5f, float movement_threshold = 2.0f)
    : m_tween_factor(tween_factor), m_movement_threshold(movement_threshold) {}

  float tween_factor() const { return m_tween_factor; }
  void set_tween_factor(float factor) { m_tween_factor = factor; }

  float movement_threshold() const { return m_movement_threshold; }
  void set_movement_threshold(float threshold) { m_movement_threshold = threshold; }

  void add_bubble(Vec2F position, RectF bound_box, T contents, unsigned margin = 0) {
    bound_box.set_y_max(bound_box.y_max() + margin);
    RectF separated = m_edges.separate(bound_box);
    m_edges.insert(separated);
    Vec2F offset = separated.min() - bound_box.min();
    m_bubbles.insert_sorted(Bubble{.contents = std::move(contents),
                                   .ideal_destination = position,
                                   .current_destination = position,
                                   .bound_box = bound_box,
                                   .separated_offset = offset,
                                   .separated_box = separated,
                                   .current_offset = offset,
                                   .previous_offset = offset}, compare_bubble_y);
  }

  template <typename Predicate>
    requires std::predicate<Predicate&, Bubble const&, T&>
  void filter(Predicate&& keep) {
    auto size = m_bubbles.size();
    m_bubbles.filter([&](Bubble& bubble) {
      return std::invoke(keep, std::as_const(bubble), bubble.contents);
    });
    if (size != m_bubbles.size())
      rebuild_edges();
  }

  template <typename Predicate>
    requires std::copy_constructible<T> && std::predicate<Predicate&, Bubble const&, T const&>
  List<Bubble> filtered(Predicate&& keep) const {
    List<Bubble> result;
    for (auto const& bubble : m_bubbles)
      if (std::invoke(keep, bubble, bubble.contents))
        result.push_back(bubble);
    return result;
  }

  template <typename Function>
    requires bubble_detail::bubble_visitor<Function, Bubble, T>
  [[nodiscard]] std::expected<void, StarException> update_destinations(Function&& visit) {
    for (auto& bubble : m_bubbles) {
      auto result = std::invoke(visit, bubble, bubble.contents);
      if (!result)
        return result;
    }

    // All bubbles solve against the same previous tick, excluding exactly
    // their own old rectangle. Publish the new geometry only after the pass.
    bool edges_changed = false;
    for (auto& bubble : m_bubbles) {
      auto bounds = RectF::with_center(bubble.ideal_destination, bubble.bound_box.size());
      auto separated = m_edges.separate(bounds, std::cref(bubble.separated_box));
      edges_changed |= separated != bubble.separated_box;
      bubble.current_destination = bubble.ideal_destination;
      bubble.bound_box = bounds;
      bubble.separated_offset = separated.min() - bounds.min();
      bubble.separated_box = separated;
    }
    if (edges_changed)
      rebuild_edges();
    if (!std::ranges::is_sorted(m_bubbles, compare_bubble_y))
      m_bubbles.sort(compare_bubble_y);
    return {};
  }

  template <typename Function>
    requires bubble_detail::bubble_visitor<Function, Bubble const, T const>
  [[nodiscard]] std::expected<void, StarException> for_each(Function&& visit) const {
    for (auto const& bubble : m_bubbles) {
      auto result = std::invoke(visit, bubble, bubble.contents);
      if (!result)
        return result;
    }
    return {};
  }

  void update(float) {
    for (auto& bubble : m_bubbles) {
      bubble.previous_offset = bubble.current_offset;
      bubble.current_offset += m_tween_factor * (bubble.separated_offset - bubble.current_offset);
      if (!bubble.presentation_continuous)
        bubble.previous_offset = bubble.current_offset;
      bubble.presentation_continuous = true;
    }
  }

  void reset_presentation() {
    for (auto& bubble : m_bubbles)
      bubble.reset_presentation();
  }

  void clear() {
    m_bubbles.clear();
    m_edges.clear();
  }

  bool empty() const { return m_bubbles.empty(); }

private:
  void rebuild_edges() {
    m_edges.clear();
    for (auto const& bubble : m_bubbles)
      m_edges.append(bubble.separated_box);
    m_edges.sort();
  }

  static bool compare_bubble_y(Bubble const& a, Bubble const& b) {
    return a.current_destination[1] < b.current_destination[1];
  }

  float m_tween_factor;
  float m_movement_threshold;
  List<Bubble> m_bubbles;
  bubble_detail::EdgeIndex m_edges;
};
}

export module star.chat_bubble_separation;

export namespace Star {
  using ::Star::BubbleState;
  using ::Star::BubbleSeparator;
}

export namespace Star::bubble_detail {
  using ::Star::bubble_detail::EdgeIndex;
}
