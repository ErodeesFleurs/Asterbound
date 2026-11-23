module;
import std;
import star.format;
import star.optional;
import star.list;
import star.set;
import star.hash;
import star.rect;
import star.map;
import star.ordered_set;
import star.random;
import star.block_allocator;
import star.vector;
import star.algorithm;
namespace Star {

struct CellularLiquidCollisionCell {};

template <typename T>
concept liquid_id_type = std::copyable<T> && std::equality_comparable<T>
    && hashable<T> && hash_invocable<ValueHash<T> const&, T const&>;

template <liquid_id_type LiquidId>
struct CellularLiquidFlowCell {
  std::optional<LiquidId> liquid;
  float level;
  float pressure;
};

template <liquid_id_type LiquidId>
struct CellularLiquidSourceCell {
  LiquidId liquid;
  float pressure;
};

template <liquid_id_type LiquidId>
using CellularLiquidCell = std::variant<CellularLiquidCollisionCell, CellularLiquidFlowCell<LiquidId>, CellularLiquidSourceCell<LiquidId>>;

template <liquid_id_type LiquidId>
struct CellularLiquidWorld {
  virtual ~CellularLiquidWorld();

  virtual Vec2I unique_location(Vec2I const& location) const;

  [[nodiscard]] virtual std::expected<CellularLiquidCell<LiquidId>, StarException> cell(Vec2I const& location) const = 0;

  // Should return an amount between 0.0 and 1.0 as a percentage of liquid
  // drain at this position
  [[nodiscard]] virtual std::expected<float, StarException> drain_level(Vec2I const& location) const;

  // Only updates locations for which cell() returned a flow cell.
  [[nodiscard]] virtual std::expected<void, StarException> set_flow(Vec2I const& location, CellularLiquidFlowCell<LiquidId> const& flow) = 0;

  // Called once for every active liquid <-> liquid interaction of different
  // liquid types each update.  Will be called AFTER pushing all the flow
  // values back out so modifications to liquids are sensible.
  [[nodiscard]] virtual std::expected<void, StarException> liquid_interaction(Vec2I const& a, LiquidId a_liquid, Vec2I const& b, LiquidId b_liquid);
  // Called once for every liquid collision each update.  Also called after
  // pushing all the flow values out, so changes to liquids can sensibly be
  // performed here.
  [[nodiscard]] virtual std::expected<void, StarException> liquid_collision(Vec2I const& pos, LiquidId liquid, Vec2I const& collision_pos);
};

struct LiquidCellEngineParameters {
  float lateral_move_factor;
  float spread_overfill_up_factor;
  float spread_overfill_lateral_factor;
  float spread_overfill_down_factor;
  float pressure_equalize_factor;
  float pressure_move_factor;
  float maximum_pressure_level_imbalance;
  float minimum_liven_pressure_change;
  float minimum_liven_level_change;
  float minimum_liquid_level;
  float interact_transformation_level;
};

template <liquid_id_type LiquidId>
class LiquidCellEngine {
public:
  LiquidCellEngine(std::uint64_t random_seed, LiquidCellEngineParameters parameters, std::shared_ptr<CellularLiquidWorld<LiquidId>> cell_world);

  unsigned liquid_tick_delta(LiquidId const& liquid) const;
  void set_liquid_tick_delta(LiquidId const& liquid, unsigned tick_delta);

  void set_processing_limit(std::optional<unsigned> processing_limit);

  void set_no_processing_limit_regions(List<RectI> no_processing_limit_regions);

  void visit_location(Vec2I const& location);
  void visit_region(RectI const& region);

  [[nodiscard]] std::expected<void, StarException> update();

  std::size_t active_cells() const;
  std::size_t active_cells(LiquidId const& liquid) const;
  bool is_active(Vec2I const& pos) const;

private:
  enum class Adjacency : std::uint8_t {
    Left,
    Right,
    Bottom,
    Top
  };

  // Indexed by Adjacency; interaction and activation each keep their own visit order.
  static constexpr std::array<Vec2I, 4> offsets{Vec2I{-1, 0}, Vec2I{1, 0}, Vec2I{0, -1}, Vec2I{0, 1}};

  struct WorkingCellState {
    Vec2I position;
    std::optional<LiquidId> liquid;
    bool source_cell;
    float level;
    float pressure;
  };

  // Neighbors only borrow physical state, not another cell's adjacency cache.
  // Keep that state complete before instantiating its reference wrappers: a
  // self-referential WorkingCell would recursively instantiate its own wrapper
  // when the active-cell list queries the wrapper's construction traits.
  struct WorkingCell : WorkingCellState {
    std::array<std::optional<std::reference_wrapper<WorkingCellState>>, offsets.size()> neighbors{};
    std::uint8_t resolved = 0; // A resolved empty slot is a collision.
  };

  // The cache has phase lifetime: no individual node is reclaimed while its
  // neighbors borrow it. A monotonic resource avoids searching the block
  // allocator's free lists once per node when the entire phase is discarded.
  struct WorkingCells {
    explicit WorkingCells(std::size_t bucket_count) : cells(bucket_count, &storage) {}

    std::pmr::monotonic_buffer_resource storage;
    std::pmr::unordered_map<Vec2I, std::optional<WorkingCell>, ValueHash<Vec2I>> cells;
  };

  using ActiveQueue = OrderedHashSet<Vec2I, ValueHash<Vec2I>, std::equal_to<Vec2I>, BlockAllocator<Vec2I, 4096>>;
  using ActiveQueues = StableHashMap<LiquidId, ActiveQueue, ValueHash<LiquidId>, std::equal_to<LiquidId>,
      BlockAllocator<std::pair<LiquidId const, ActiveQueue>, 4096>>;

  [[nodiscard]] std::expected<void, StarException> setup();
  template <typename Visitor>
    requires requires(Visitor& visit) { { visit(Adjacency::Left) } -> std::same_as<std::expected<void, StarException>>; }
  [[nodiscard]] std::expected<void, StarException> visit_lateral(Visitor&& visit) {
    bool left_first = m_random.next_bool();
    auto first = visit(left_first ? Adjacency::Left : Adjacency::Right);
    if (!first) return first;
    return visit(left_first ? Adjacency::Right : Adjacency::Left);
  }

  [[nodiscard]] std::expected<void, StarException> apply_pressure();
  [[nodiscard]] std::expected<void, StarException> spread_pressure();
  [[nodiscard]] std::expected<void, StarException> limit_pressure();
  [[nodiscard]] std::expected<void, StarException> pressure_move();
  [[nodiscard]] std::expected<void, StarException> spread_overfill();
  [[nodiscard]] std::expected<void, StarException> level_move();
  [[nodiscard]] std::expected<void, StarException> find_interactions();
  [[nodiscard]] std::expected<void, StarException> finish();

  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<WorkingCell>>, StarException> working_cell(Vec2I p);
  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<WorkingCellState>>, StarException> adjacent_cell(WorkingCell& cell, Adjacency adjacency);
  bool in_no_processing_limit_region(Vec2I const& position) const;

  void set_pressure(float pressure, WorkingCellState& cell);
  void transfer_pressure(float amount, WorkingCellState& source, WorkingCellState& dest);
  void transfer_level(float amount, WorkingCellState& source, WorkingCellState& dest);
  void set_level(float level, WorkingCellState& cell);

  RandomSource m_random;
  LiquidCellEngineParameters m_engine_parameters;
  std::shared_ptr<CellularLiquidWorld<LiquidId>> m_cell_world;

  ActiveQueues m_active_cells;
  StableHashMap<LiquidId, unsigned, ValueHash<LiquidId>, std::equal_to<LiquidId>,
      BlockAllocator<std::pair<LiquidId const, unsigned>, 4096>> m_liquid_tick_deltas;
  std::optional<unsigned> m_processing_limit;
  List<RectI> m_no_processing_limit_regions;
  // Ordered subsets of the active queues: stable windows only require visiting
  // the ordinary budget prefix and these cells, never the skipped suffix.
  // An existing (even empty) entry is initialized for the current regions.
  // Missing entries are built only when that liquid is due to run.
  ActiveQueues m_unlimited_active_cells;
  std::uint64_t m_step = 0;

  // Only bucket grouping survives a retired cache: it determines the next
  // physical write order, including the prefix observable on write failure.
  std::size_t m_next_working_bucket_count = 0;
  std::unique_ptr<WorkingCells> m_working_cells;
  List<std::reference_wrapper<WorkingCell>> m_current_active_cells;
  HashSet<Vec2I> m_next_active_cells;
  HashSet<std::tuple<Vec2I, LiquidId, Vec2I, LiquidId>> m_liquid_interactions;
  HashSet<std::tuple<Vec2I, LiquidId, Vec2I>> m_liquid_collisions;
};

template <liquid_id_type LiquidId> CellularLiquidWorld<LiquidId>::~CellularLiquidWorld() = default;

template <liquid_id_type LiquidId>
Vec2I CellularLiquidWorld<LiquidId>::unique_location(Vec2I const& location) const {
  return location;
}

template <liquid_id_type LiquidId>
[[nodiscard]] std::expected<float, StarException> CellularLiquidWorld<LiquidId>::drain_level(Vec2I const&) const {
  return 0.0f;
}

template <liquid_id_type LiquidId>
[[nodiscard]] std::expected<void, StarException> CellularLiquidWorld<LiquidId>::liquid_interaction(Vec2I const&, LiquidId, Vec2I const&, LiquidId) { return {}; }

template <liquid_id_type LiquidId>
[[nodiscard]] std::expected<void, StarException> CellularLiquidWorld<LiquidId>::liquid_collision(Vec2I const&, LiquidId, Vec2I const&) { return {}; }

template <liquid_id_type LiquidId>
LiquidCellEngine<LiquidId>::LiquidCellEngine(std::uint64_t random_seed, LiquidCellEngineParameters parameters,
                                             std::shared_ptr<CellularLiquidWorld<LiquidId>> cell_world)
    : m_random(random_seed), m_engine_parameters(parameters), m_cell_world(std::move(cell_world)) {}

template <liquid_id_type LiquidId>
unsigned LiquidCellEngine<LiquidId>::liquid_tick_delta(LiquidId const& liquid) const {
  return m_liquid_tick_deltas.value(liquid, 1);
}

template <liquid_id_type LiquidId>
void LiquidCellEngine<LiquidId>::set_liquid_tick_delta(LiquidId const& liquid, unsigned tick_delta) {
  m_liquid_tick_deltas[liquid] = tick_delta;
}

template <liquid_id_type LiquidId>
void LiquidCellEngine<LiquidId>::set_processing_limit(std::optional<unsigned> processing_limit) {
  m_processing_limit = processing_limit;
}

template <liquid_id_type LiquidId>
void LiquidCellEngine<LiquidId>::set_no_processing_limit_regions(List<RectI> no_processing_limit_regions) {
  if (m_no_processing_limit_regions == no_processing_limit_regions)
    return;
  m_no_processing_limit_regions = std::move(no_processing_limit_regions);
  m_unlimited_active_cells.clear();
}

template <liquid_id_type LiquidId>
void LiquidCellEngine<LiquidId>::visit_location(Vec2I const& p) {
  m_next_active_cells.add(p);
}

template <liquid_id_type LiquidId>
void LiquidCellEngine<LiquidId>::visit_region(RectI const& region) {
  for (int x = region.x_min(); x < region.x_max(); ++x) {
    for (int y = region.y_min(); y < region.y_max(); ++y)
      m_next_active_cells.add({x, y});
  }
}

template <liquid_id_type LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::update() {
  auto prepared = setup();
  if (!prepared) return std::unexpected(std::move(prepared.error()));
  auto applied = apply_pressure();
  if (!applied) return std::unexpected(std::move(applied.error()));
  auto spread = spread_pressure();
  if (!spread) return std::unexpected(std::move(spread.error()));
  auto limited = limit_pressure();
  if (!limited) return std::unexpected(std::move(limited.error()));
  auto pressure_moved = pressure_move();
  if (!pressure_moved) return std::unexpected(std::move(pressure_moved.error()));
  auto overfill = spread_overfill();
  if (!overfill) return std::unexpected(std::move(overfill.error()));
  auto moved = level_move();
  if (!moved) return std::unexpected(std::move(moved.error()));
  auto interactions = find_interactions();
  if (!interactions) return std::unexpected(std::move(interactions.error()));
  auto finished = finish();
  if (!finished) return std::unexpected(std::move(finished.error()));
  ++m_step;
  return {};
}

template <liquid_id_type LiquidId>
std::size_t LiquidCellEngine<LiquidId>::active_cells() const {
  std::size_t total_size = 0;
  for (auto const& p : m_active_cells)
    total_size += p.second.size();
  return total_size;
}

template <liquid_id_type LiquidId>
std::size_t LiquidCellEngine<LiquidId>::active_cells(LiquidId const& liquid) const {
  auto entry = m_active_cells.find(liquid);
  return entry == m_active_cells.end() ? 0 : entry->second.size();
}

template <liquid_id_type LiquidId>
bool LiquidCellEngine<LiquidId>::is_active(Vec2I const& pos) const {
  for (auto const& p : m_active_cells) {
    if (p.second.contains(pos))
      return true;
  }
  return false;
}

template <liquid_id_type LiquidId> bool LiquidCellEngine<LiquidId>::in_no_processing_limit_region(Vec2I const& position) const {
  for (auto const& region : m_no_processing_limit_regions) {
    if (region.contains(position))
      return true;
  }
  return false;
}

template <liquid_id_type LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::setup() {
  m_current_active_cells.clear();
  if (m_working_cells) {
    // A failed physical phase, rather than an activation scan, supplied the buckets.
    m_next_working_bucket_count = m_working_cells->cells.bucket_count();
    m_working_cells.reset();
  }

  for (auto& active_cells_pair : m_active_cells) {
    unsigned tick_delta = liquid_tick_delta(active_cells_pair.first);
    if (tick_delta == 0 || m_step % tick_delta != 0)
      continue;

    auto unlimited = m_unlimited_active_cells.find(active_cells_pair.first);
    if (m_processing_limit && !m_no_processing_limit_regions.empty() && unlimited == m_unlimited_active_cells.end()) {
      auto& indexed = m_unlimited_active_cells[active_cells_pair.first];
      for (auto const& position : active_cells_pair.second) {
        if (in_no_processing_limit_region(position))
          indexed.add(position);
      }
      unlimited = m_unlimited_active_cells.find(active_cells_pair.first);
    }

    auto process = [&](Vec2I const& pos) -> std::expected<void, StarException> {
      auto found = working_cell(pos);
      if (!found)
        return std::unexpected(std::move(found.error()));
      auto cell = *found;
      if (cell && cell->get().liquid == active_cells_pair.first)
        m_current_active_cells.push_back(*cell);
      active_cells_pair.second.remove(pos);
      if (unlimited != m_unlimited_active_cells.end())
        unlimited->second.remove(pos);
      return {};
    };

    std::size_t limited_cell_number = 0;
    auto position = active_cells_pair.second.begin();
    while (position != active_cells_pair.second.end() && (!m_processing_limit || limited_cell_number < *m_processing_limit)) {
      // Copy only the current coordinate; advance before erasing its list node.
      auto pos = *position++;
      if (m_processing_limit && (unlimited == m_unlimited_active_cells.end() || !unlimited->second.contains(pos)))
        ++limited_cell_number;
      auto processed = process(pos);
      if (!processed)
        return std::unexpected(std::move(processed.error()));
    }

    // Cells beyond the ordinary budget still run if monitored. Removing the
    // prefix above also removes it from this ordered subset, preserving the
    // original input order (including equal-Y ties) without scanning skips.
    if (m_processing_limit && unlimited != m_unlimited_active_cells.end()) {
      auto remaining = unlimited->second.begin();
      while (remaining != unlimited->second.end()) {
        auto pos = *remaining++;
        auto processed = process(pos);
        if (!processed)
          return std::unexpected(std::move(processed.error()));
      }
    }
  }

  std::sort(m_current_active_cells.begin(), m_current_active_cells.end(), [](std::reference_wrapper<WorkingCell> lhs, std::reference_wrapper<WorkingCell> rhs) {
      return lhs.get().position[1] < rhs.get().position[1];
    });
  return {};
}

template <liquid_id_type LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::apply_pressure() {
  for (auto const& self_cell_ref : m_current_active_cells) {
    auto& self_cell = self_cell_ref.get();
    if (!self_cell.liquid || self_cell.source_cell)
      continue;

    auto found = adjacent_cell(self_cell, Adjacency::Top);
    if (!found) return std::unexpected(std::move(found.error()));
    auto top_cell = *found;
    if (top_cell && self_cell.liquid == top_cell->get().liquid)
      set_pressure(std::max(self_cell.pressure, top_cell->get().pressure + std::min(top_cell->get().level, 1.0f)), self_cell);
  }
  return {};
}

template <liquid_id_type LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::spread_pressure() {
  for (auto const& self_cell_ref : m_current_active_cells) {
    auto& self_cell = self_cell_ref.get();
    if (!self_cell.liquid)
      continue;

    auto spread_pressure = [&](Adjacency adjacency, float bias) -> std::expected<void, StarException> {
      auto found = adjacent_cell(self_cell, adjacency);
      if (!found) return std::unexpected(std::move(found.error()));
      auto target_cell = *found;
      if (target_cell && !target_cell->get().source_cell)
        transfer_pressure((self_cell.pressure + bias - target_cell->get().pressure) * m_engine_parameters.pressure_equalize_factor, self_cell, target_cell->get());
      return {};
    };

    auto lateral = visit_lateral([&](Adjacency side) { return spread_pressure(side, 0.0f); });
    if (!lateral) return lateral;
    auto bottom = spread_pressure(Adjacency::Bottom, 1.0f);
    if (!bottom) return std::unexpected(std::move(bottom.error()));
    auto top = spread_pressure(Adjacency::Top, -1.0f);
    if (!top) return std::unexpected(std::move(top.error()));
  }
  return {};
}

template <liquid_id_type LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::limit_pressure() {
  for (auto const& self_cell_ref : m_current_active_cells) {
    auto& self_cell = self_cell_ref.get();
    float level = std::min(self_cell.level, 1.0f);
    auto found = adjacent_cell(self_cell, Adjacency::Top);
    if (!found) return std::unexpected(std::move(found.error()));
    auto top_cell = *found;

    // Force the pressure to the cell level if there is empty space above,
    // otherwise simply make sure the pressure is at least the level
    if (top_cell && !top_cell->get().liquid)
      set_pressure(level, self_cell);
    else
      set_pressure(std::max(self_cell.pressure, level), self_cell);
  }
  return {};
}

template <liquid_id_type LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::pressure_move() {
  for (auto const& self_cell_ref : m_current_active_cells) {
    auto& self_cell = self_cell_ref.get();
    if (!self_cell.liquid)
      continue;

    auto pressure_move = [&](Adjacency adjacency) -> std::expected<void, StarException> {
      auto found = adjacent_cell(self_cell, adjacency);
      if (!found) return std::unexpected(std::move(found.error()));
      auto target_cell = *found;
      if (target_cell && !target_cell->get().source_cell && target_cell->get().level >= self_cell.level) {
        float amount = (self_cell.pressure - target_cell->get().pressure) * m_engine_parameters.pressure_move_factor;
        amount = std::min(amount, self_cell.level - (1.0f - m_engine_parameters.maximum_pressure_level_imbalance));
        amount = std::min(amount, (1.0f + m_engine_parameters.maximum_pressure_level_imbalance) - target_cell->get().level);
        transfer_level(amount, self_cell, target_cell->get());
      }
      return {};
    };

    auto lateral = visit_lateral(pressure_move);
    if (!lateral) return lateral;
  }
  return {};
}

template <liquid_id_type LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::spread_overfill() {
  for (auto const& self_cell_ref : m_current_active_cells) {
    auto& self_cell = self_cell_ref.get();
    if (!self_cell.liquid || self_cell.source_cell)
      continue;

    auto spread_overfill = [&](Adjacency adjacency, float factor) -> std::expected<void, StarException> {
      float overfill = self_cell.level - 1.0f;
      if (overfill > 0.0f) {
        auto found = adjacent_cell(self_cell, adjacency);
        if (!found) return std::unexpected(std::move(found.error()));
        auto target_cell = *found;
        if (target_cell)
          transfer_level(std::min(overfill, (self_cell.level - target_cell->get().level)) * factor, self_cell, target_cell->get());
      }
      return {};
    };

    auto top = spread_overfill(Adjacency::Top, m_engine_parameters.spread_overfill_up_factor);
    if (!top) return std::unexpected(std::move(top.error()));
    auto lateral = visit_lateral([&](Adjacency side) { return spread_overfill(side, m_engine_parameters.spread_overfill_lateral_factor); });
    if (!lateral) return lateral;
    auto bottom = spread_overfill(Adjacency::Bottom, m_engine_parameters.spread_overfill_down_factor);
    if (!bottom) return std::unexpected(std::move(bottom.error()));
  }
  return {};
}

template <liquid_id_type LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::level_move() {
  for (auto const& self_cell_ref : m_current_active_cells) {
    auto& self_cell = self_cell_ref.get();
    if (!self_cell.liquid)
      continue;

    auto found = adjacent_cell(self_cell, Adjacency::Bottom);
    if (!found) return std::unexpected(std::move(found.error()));
    auto below_cell = *found;
    if (below_cell)
      transfer_level(std::min(1.0f - below_cell->get().level, self_cell.level), self_cell, below_cell->get());

    auto drain = m_cell_world->drain_level(self_cell.position);
    if (!drain) return std::unexpected(std::move(drain.error()));
    set_level(self_cell.level * (1.0f - *drain), self_cell);

    auto lateral_move = [&](Adjacency adjacency) -> std::expected<void, StarException> {
      auto found = adjacent_cell(self_cell, adjacency);
      if (!found) return std::unexpected(std::move(found.error()));
      auto target_cell = *found;
      if (target_cell)
        transfer_level((self_cell.level - target_cell->get().level) * m_engine_parameters.lateral_move_factor, self_cell, target_cell->get());
      return {};
    };

    auto lateral = visit_lateral(lateral_move);
    if (!lateral) return lateral;
  }
  return {};
}

template <liquid_id_type LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::find_interactions() {
  for (auto const& self_cell_ref : m_current_active_cells) {
    auto& self_cell = self_cell_ref.get();
    if (!self_cell.liquid)
      continue;

    for (auto adjacency : {Adjacency::Bottom, Adjacency::Top, Adjacency::Left, Adjacency::Right}) {
      auto found = adjacent_cell(self_cell, adjacency);
      if (!found) return std::unexpected(std::move(found.error()));
      auto target_cell = *found;
      if (!target_cell) {
        auto adjacent_pos = self_cell.position + offsets[std::to_underlying(adjacency)];
        m_liquid_collisions.add(std::make_tuple(self_cell.position, *self_cell.liquid, adjacent_pos));

      } else if (target_cell->get().liquid && *target_cell->get().liquid != *self_cell.liquid) {
        if (target_cell->get().level <= m_engine_parameters.interact_transformation_level
            || self_cell.level <= m_engine_parameters.interact_transformation_level) {
          if (self_cell.level > target_cell->get().level)
            target_cell->get().liquid = self_cell.liquid;
          else
            self_cell.liquid = target_cell->get().liquid;
        } else {
          // Make sure to add the point pair in a predictable order so that any
          // combination of Vec2I points will be unique in m_liquid_interactions
          if (self_cell.position < target_cell->get().position)
            m_liquid_interactions.add(std::make_tuple(self_cell.position, *self_cell.liquid, target_cell->get().position, *target_cell->get().liquid));
          else
            m_liquid_interactions.add(std::make_tuple(target_cell->get().position, *target_cell->get().liquid, self_cell.position, *self_cell.liquid));
        }
      }
    }
  }
  return {};
}

template <liquid_id_type LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::finish() {
  m_current_active_cells.clear();

  m_next_working_bucket_count = 0;
  auto working_cells = std::exchange(m_working_cells, {});
  if (working_cells) {
    for (auto& [position, cell] : working_cells->cells) {
      if (!cell || cell->source_cell)
        continue;
      if (!cell->liquid || cell->level < m_engine_parameters.minimum_liquid_level)
        cell->level = 0.0f;
      if (cell->level == 0.0f) {
        cell->liquid = {};
        cell->pressure = 0.0f;
      }
      auto set = m_cell_world->set_flow(position, CellularLiquidFlowCell<LiquidId>{cell->liquid, cell->level, cell->pressure});
      if (!set) return set;
    }
  }
  working_cells.reset();

  for (auto const& interaction : std::exchange(m_liquid_interactions, {})) {
    auto applied = m_cell_world->liquid_interaction(std::get<0>(interaction), std::get<1>(interaction), std::get<2>(interaction), std::get<3>(interaction));
    if (!applied) return std::unexpected(std::move(applied.error()));
  }

  for (auto const& interaction : std::exchange(m_liquid_collisions, {})) {
    auto applied = m_cell_world->liquid_collision(std::get<0>(interaction), std::get<1>(interaction), std::get<2>(interaction));
    if (!applied) return std::unexpected(std::move(applied.error()));
  }

  // Callbacks may have replaced a cell. Read fresh ids, once per canonical
  // location, without keeping this phase's arena or physical state alive.
  std::pmr::monotonic_buffer_resource storage;
  std::pmr::unordered_map<Vec2I, std::optional<LiquidId>, ValueHash<Vec2I>> liquids(&storage);
  auto visit = [&](Vec2I const& p) -> std::expected<void, StarException> {
    auto position = m_cell_world->unique_location(p);
    auto [entry, inserted] = liquids.try_emplace(position);
    if (inserted) {
      auto data = m_cell_world->cell(position);
      if (!data) {
        // The failed insertion may already have grown the table. Its nodes
        // retire on return, but this count still seeds the next physical phase.
        m_next_working_bucket_count = liquids.bucket_count();
        return std::unexpected(std::move(data.error()));
      }
      if (std::holds_alternative<CellularLiquidFlowCell<LiquidId>>(*data))
        entry->second = std::move(std::get<CellularLiquidFlowCell<LiquidId>>(*data).liquid);
      else if (std::holds_alternative<CellularLiquidSourceCell<LiquidId>>(*data))
        entry->second = std::move(std::get<CellularLiquidSourceCell<LiquidId>>(*data).liquid);
    }
    if (entry->second && m_active_cells[*entry->second].add(position)) {
      auto unlimited = m_unlimited_active_cells.find(*entry->second);
      if (unlimited != m_unlimited_active_cells.end() && in_no_processing_limit_region(position))
        unlimited->second.add(position);
    }
    return {};
  };
  for (auto const& c : std::exchange(m_next_active_cells, {})) {
    auto center = visit(c);
    if (!center) return center;
    for (auto const& offset : offsets) {
      auto neighbor = visit(c + offset);
      if (!neighbor) return neighbor;
    }
  }
  m_next_working_bucket_count = liquids.empty() ? 0 : liquids.bucket_count();

  Star::erase_if(m_active_cells, [](auto const& p) { return p.second.empty(); });
  Star::erase_if(m_unlimited_active_cells, [this](auto const& p) { return !m_active_cells.contains(p.first); });
  return {};
}

template <liquid_id_type LiquidId>
[[nodiscard]] std::expected<std::optional<std::reference_wrapper<typename LiquidCellEngine<LiquidId>::WorkingCell>>, StarException> LiquidCellEngine<LiquidId>::working_cell(Vec2I p) {
  p = m_cell_world->unique_location(p);

  if (!m_working_cells)
    m_working_cells = std::make_unique<WorkingCells>(m_next_working_bucket_count);
  // try_emplace does not allocate a candidate node on a cache hit.
  auto res = m_working_cells->cells.try_emplace(p);
  if (res.second) {
    auto cell_data = m_cell_world->cell(p);
    if (!cell_data) {
      m_working_cells->cells.erase(res.first);
      return std::unexpected(std::move(cell_data.error()));
    }
    if (std::holds_alternative<CellularLiquidFlowCell<LiquidId>>(*cell_data)) {
      auto const& flow_cell = std::get<CellularLiquidFlowCell<LiquidId>>(*cell_data);
      res.first->second = WorkingCell{{p, flow_cell.liquid, false, flow_cell.level, flow_cell.pressure}};
    } else if (std::holds_alternative<CellularLiquidSourceCell<LiquidId>>(*cell_data)) {
      auto const& source_cell = std::get<CellularLiquidSourceCell<LiquidId>>(*cell_data);
      res.first->second = WorkingCell{{p, source_cell.liquid, true, 1.0f, source_cell.pressure}};
    }
  }
  return optional_ref(res.first->second);
}

template <liquid_id_type LiquidId>
[[nodiscard]] std::expected<std::optional<std::reference_wrapper<typename LiquidCellEngine<LiquidId>::WorkingCellState>>, StarException> LiquidCellEngine<LiquidId>::adjacent_cell(
    WorkingCell& cell, Adjacency adjacency) {
  auto index = std::to_underlying(adjacency);
  auto mask = static_cast<std::uint8_t>(1u << index);
  auto& neighbor = cell.neighbors[index];
  if (!(cell.resolved & mask)) {
    auto found = working_cell(cell.position + offsets[index]);
    if (!found) return std::unexpected(std::move(found.error()));
    if (*found)
      neighbor = std::ref(static_cast<WorkingCellState&>((*found)->get()));
    cell.resolved |= mask;
  }
  return neighbor;
}

template <liquid_id_type LiquidId>
void LiquidCellEngine<LiquidId>::set_pressure(float pressure, WorkingCellState& cell) {
  if (!cell.liquid || cell.source_cell)
    return;

  if (std::fabs(cell.pressure - pressure) > m_engine_parameters.minimum_liven_pressure_change)
    m_next_active_cells.add(cell.position);
  cell.pressure = pressure;
}

template <liquid_id_type LiquidId>
void LiquidCellEngine<LiquidId>::transfer_pressure(float amount, WorkingCellState& from, WorkingCellState& to) {
  bool reverse = amount < 0.0f;
  if (reverse)
    amount = -amount;
  if (!(amount > 0.0f))
    return;
  auto& source = reverse ? to : from;
  auto& dest = reverse ? from : to;
  if (!source.liquid || (source.source_cell && dest.source_cell) || (dest.liquid && dest.liquid != source.liquid))
    return;

  amount = std::min(amount, source.pressure);
  if (!source.source_cell)
    source.pressure -= amount;
  if (dest.liquid && !dest.source_cell)
    dest.pressure += amount;

  if (amount > m_engine_parameters.minimum_liven_pressure_change) {
    m_next_active_cells.add(source.position);
    m_next_active_cells.add(dest.position);
  }
}

template <liquid_id_type LiquidId>
void LiquidCellEngine<LiquidId>::set_level(float level, WorkingCellState& cell) {
  if (!cell.liquid || cell.source_cell)
    return;

  if (std::fabs(cell.level - level) > m_engine_parameters.minimum_liven_level_change)
    m_next_active_cells.add(cell.position);

  cell.level = level;

  if (cell.level <= 0.0f) {
    cell.liquid = {};
    cell.level = 0.0f;
  }
}

template <liquid_id_type LiquidId>
void LiquidCellEngine<LiquidId>::transfer_level(float amount, WorkingCellState& source, WorkingCellState& dest) {
  if (!(amount > 0.0f) || !source.liquid || (source.source_cell && dest.source_cell) || (dest.liquid && dest.liquid != source.liquid))
    return;

  amount = std::min(amount, source.level);
  if (!source.source_cell)
    source.level -= amount;
  if (!dest.source_cell) {
    dest.level += amount;
    dest.liquid = source.liquid;
  }
  if (!source.source_cell && source.level == 0.0f)
    source.liquid = {};

  if (amount > m_engine_parameters.minimum_liven_level_change) {
    m_next_active_cells.add(source.position);
    m_next_active_cells.add(dest.position);
  }
}

}

export module star.cellular_liquid;

export namespace Star {
  using ::Star::liquid_id_type;
  using ::Star::CellularLiquidCollisionCell;
  using ::Star::CellularLiquidFlowCell;
  using ::Star::CellularLiquidSourceCell;
  using ::Star::CellularLiquidCell;
  using ::Star::CellularLiquidWorld;
  using ::Star::LiquidCellEngineParameters;
  using ::Star::LiquidCellEngine;
}
