import std;
import star.assets;
import star.falling_blocks_agent;
import star.format;
import star.json;
import star.list;
import star.material_types;
import star.math_common;
import star.random;
import star.rect;
import star.root;
import star.set;
import star.string;
import star.vector;

namespace Star {

namespace {
constexpr int coord_min = std::numeric_limits<int>::min();
constexpr int coord_max = std::numeric_limits<int>::max();
}

FallingBlocksAgent::FallingBlocksAgent(ConstructionKey, std::shared_ptr<FallingBlocksFacade> facade, std::uint64_t seed, float probability)
  : m_facade(std::move(facade)), m_propagate_probability(probability), m_random(seed) {}

std::expected<std::shared_ptr<FallingBlocksAgent>, StarException> FallingBlocksAgent::create(Root& root, std::shared_ptr<FallingBlocksFacade> facade) {
  auto seed = root.random().next_u64();
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/worldserver.config:fallingBlocksImmediateUpwardPropogateProbability");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto probability = config->to_double();
  if (!probability)
    return std::unexpected(std::move(probability.error()));
  return create(std::move(facade), seed, *probability);
}

std::expected<std::shared_ptr<FallingBlocksAgent>, StarException> FallingBlocksAgent::create(std::shared_ptr<FallingBlocksFacade> facade, std::uint64_t seed, double probability) {
  if (!is_finite(probability) || probability < 0 || probability > 1)
    return std::unexpected(StarException("fallingBlocksImmediateUpwardPropogateProbability must be finite and within [0, 1]"));
  return std::make_shared<FallingBlocksAgent>(ConstructionKey{}, std::move(facade), seed, static_cast<float>(probability));
}

void FallingBlocksAgent::begin_wave(HashSet<Vec2I>& candidates) {
  m_positions.clear();
  m_positions.reserve(candidates.size());
  for (auto const& pos : candidates)
    m_positions.push_back(pos);
  // A retained bucket count would change iteration order before the next shuffle.
  candidates = HashSet<Vec2I>{};
  m_random.shuffle(m_positions);
  m_positions.sort([](Vec2I const& a, Vec2I const& b) { return a[1] < b[1]; });
  m_cursor = 0;
}

std::expected<std::optional<Vec2I>, StarException> FallingBlocksAgent::choose_target(Vec2I const& pos) {
  auto behavior = m_facade->source_behavior(pos);
  if (!behavior)
    return std::unexpected(std::move(behavior.error()));
  if (*behavior == FallingBehavior::Stationary || pos[1] == coord_min)
    return {};

  Vec2I below{pos[0], pos[1] - 1};
  auto open = m_facade->target_open(below);
  if (!open)
    return std::unexpected(std::move(open.error()));
  if (*open)
    return below;
  if (*behavior != FallingBehavior::Cascading)
    return {};

  bool left = false;
  if (pos[0] != coord_min) {
    auto open_left = m_facade->target_open({pos[0] - 1, below[1]});
    if (!open_left)
      return std::unexpected(std::move(open_left.error()));
    left = *open_left;
  }
  if (pos[0] != coord_max) {
    auto open_right = m_facade->target_open({pos[0] + 1, below[1]});
    if (!open_right)
      return std::unexpected(std::move(open_right.error()));
    if (*open_right) {
      if (!left)
        return Vec2I{pos[0] + 1, below[1]};
      // Re-probe the world on a pre-commit retry, but keep the already drawn
      // tie-breaker if both destinations are still open.
      if (!m_prefer_left)
        m_prefer_left = m_random.next_bool();
      if (!*m_prefer_left)
        return Vec2I{pos[0] + 1, below[1]};
    }
  }
  if (left)
    return Vec2I{pos[0] - 1, below[1]};
  return {};
}

void FallingBlocksAgent::schedule_change(Vec2I const& from, Vec2I const& to) {
  if (m_random.uniform_float() < m_propagate_probability && from[1] != coord_max) {
    int y = from[1] + 1;
    m_next_wave.add({from[0], y});
    if (from[0] != coord_min)
      m_next_wave.add({from[0] - 1, y});
    if (from[0] != coord_max)
      m_next_wave.add({from[0] + 1, y});
  }
  visit_location(from);
  visit_location(to);
}

std::expected<void, StarException> FallingBlocksAgent::update() {
  if (m_positions.empty()) {
    if (m_pending.empty())
      return {};
    begin_wave(m_pending);
  }

  for (;;) {
    while (m_cursor < m_positions.size()) {
      auto const& from = m_positions[m_cursor];
      auto to = choose_target(from);
      if (!to)
        return std::unexpected(std::move(to.error()));
      if (*to) {
        auto moved = m_facade->move_block(from, **to);
        if (!moved && moved.error().result == BlockMoveResult::Unchanged)
          return std::unexpected(std::move(moved.error().cause));
        if (!moved || *moved == BlockMoveResult::Changed)
          schedule_change(from, **to);
        if (!moved) {
          // Destruction committed before the error: never destroy or emit drops twice.
          ++m_cursor;
          m_prefer_left.reset();
          return std::unexpected(std::move(moved.error().cause));
        }
      }
      ++m_cursor;
      m_prefer_left.reset();
    }
    if (m_next_wave.empty())
      break;
    begin_wave(m_next_wave);
  }
  m_positions.clear();
  return {};
}

void FallingBlocksAgent::visit_location(Vec2I const& location) {
  // The edited cell can support three cells across, on this row and the one above.
  visit_region(RectI(location, {location[0] + (location[0] != coord_max), location[1] + (location[1] != coord_max)}));
}

void FallingBlocksAgent::visit_region(RectI const& region) {
  // Inclusive upper bounds, matching the single-cell 3x2 footprint. Widen the
  // loop counters so even an upper bound of INT_MAX has a representable successor.
  for (std::int64_t x = region.x_min() - (region.x_min() != coord_min); x <= region.x_max(); ++x)
    for (std::int64_t y = region.y_min(); y <= region.y_max(); ++y)
      m_pending.add({static_cast<int>(x), static_cast<int>(y)});
}

}
