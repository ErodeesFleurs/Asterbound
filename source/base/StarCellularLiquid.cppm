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
import star.math_common;
import star.algorithm;
namespace Star {

struct CellularLiquidCollisionCell {};

template <typename LiquidId>
struct CellularLiquidFlowCell {
  std::optional<LiquidId> liquid;
  float level;
  float pressure;
};

template <typename LiquidId>
struct CellularLiquidSourceCell {
  LiquidId liquid;
  float pressure;
};

template <typename LiquidId>
using CellularLiquidCell = std::variant<CellularLiquidCollisionCell, CellularLiquidFlowCell<LiquidId>, CellularLiquidSourceCell<LiquidId>>;

template <typename LiquidId>
struct CellularLiquidWorld {
  virtual ~CellularLiquidWorld();

  virtual Vec2I uniqueLocation(Vec2I const& location) const;

  [[nodiscard]] virtual std::expected<CellularLiquidCell<LiquidId>, StarException> cell(Vec2I const& location) const = 0;

  // Should return an amount between 0.0 and 1.0 as a percentage of liquid
  // drain at this position
  [[nodiscard]] virtual std::expected<float, StarException> drainLevel(Vec2I const& location) const;

  // Will be called only on cells which for which the cell method returned a
  // flow cell, to update the flow cell.
  [[nodiscard]] virtual std::expected<void, StarException> setFlow(Vec2I const& location, CellularLiquidFlowCell<LiquidId> const& flow) = 0;

  // Called once for every active liquid <-> liquid interaction of different
  // liquid types each update.  Will be called AFTER pushing all the flow
  // values back out so modifications to liquids are sensible.
  [[nodiscard]] virtual std::expected<void, StarException> liquidInteraction(Vec2I const& a, LiquidId aLiquid, Vec2I const& b, LiquidId bLiquid);
  // Called once for every liquid collision each update.  Also called after
  // pushing all the flow values out, so changes to liquids can sensibly be
  // performed here.
  [[nodiscard]] virtual std::expected<void, StarException> liquidCollision(Vec2I const& pos, LiquidId liquid, Vec2I const& collisionPos);
};

struct LiquidCellEngineParameters {
  float lateralMoveFactor;
  float spreadOverfillUpFactor;
  float spreadOverfillLateralFactor;
  float spreadOverfillDownFactor;
  float pressureEqualizeFactor;
  float pressureMoveFactor;
  float maximumPressureLevelImbalance;
  float minimumLivenPressureChange;
  float minimumLivenLevelChange;
  float minimumLiquidLevel;
  float interactTransformationLevel;
};

template <typename LiquidId>
class LiquidCellEngine {
public:
  LiquidCellEngine(std::uint64_t randomSeed, LiquidCellEngineParameters parameters, std::shared_ptr<CellularLiquidWorld<LiquidId>> cellWorld);

  unsigned liquidTickDelta(LiquidId liquid);
  void setLiquidTickDelta(LiquidId liquid, unsigned tickDelta);

  void setProcessingLimit(std::optional<unsigned> processingLimit);

  List<RectI> noProcessingLimitRegions() const;
  void setNoProcessingLimitRegions(List<RectI> noProcessingLimitRegions);

  void visitLocation(Vec2I const& location);
  void visitRegion(RectI const& region);

  [[nodiscard]] std::expected<void, StarException> update();

  std::size_t activeCells() const;
  std::size_t activeCells(LiquidId liquid) const;
  bool isActive(Vec2I const& pos) const;

private:
  enum class Adjacency {
    Left,
    Right,
    Bottom,
    Top
  };

  struct WorkingCellState {
    Vec2I position;
    std::optional<LiquidId> liquid;
    bool sourceCell;
    float level;
    float pressure;
  };

  // Neighbors only borrow physical state, not another cell's adjacency cache.
  // Keep that state complete before instantiating its reference wrappers: a
  // self-referential WorkingCell would recursively instantiate its own wrapper
  // when the active-cell list queries the wrapper's construction traits.
  struct WorkingCell : WorkingCellState {
    std::optional<std::reference_wrapper<WorkingCellState>> leftCell;
    std::optional<std::reference_wrapper<WorkingCellState>> rightCell;
    std::optional<std::reference_wrapper<WorkingCellState>> topCell;
    std::optional<std::reference_wrapper<WorkingCellState>> bottomCell;
  };

  // The cache has phase lifetime: no individual node is reclaimed while its
  // neighbors borrow it. A monotonic resource avoids searching the block
  // allocator's free lists once per node when the entire phase is discarded.
  struct WorkingCells {
    using Entry = std::pair<Vec2I const, std::optional<WorkingCell>>;
    WorkingCells(std::size_t bucketCount = 0)
      : cells(bucketCount, hash<Vec2I>(), std::equal_to<Vec2I>(),
          std::pmr::polymorphic_allocator<Entry>(&storage)) {}

    std::pmr::monotonic_buffer_resource storage;
    StableHashMap<Vec2I, std::optional<WorkingCell>, hash<Vec2I>, std::equal_to<Vec2I>,
        std::pmr::polymorphic_allocator<Entry>> cells;
  };

  template <typename Key, typename Value>
  using BAHashMap = StableHashMap<Key, Value, hash<Key>, std::equal_to<Key>, BlockAllocator<std::pair<Key const, Value>, 4096>>;

  template <typename Value>
  using BAHashSet = HashSet<Value, hash<Value>, std::equal_to<Value>>;

  template <typename Value>
  using BAOrderedHashSet = OrderedHashSet<Value, hash<Value>, std::equal_to<Value>, BlockAllocator<Value, 4096>>;

  [[nodiscard]] std::expected<void, StarException> setup();
  [[nodiscard]] std::expected<void, StarException> applyPressure();
  [[nodiscard]] std::expected<void, StarException> spreadPressure();
  [[nodiscard]] std::expected<void, StarException> limitPressure();
  [[nodiscard]] std::expected<void, StarException> pressureMove();
  [[nodiscard]] std::expected<void, StarException> spreadOverfill();
  [[nodiscard]] std::expected<void, StarException> levelMove();
  [[nodiscard]] std::expected<void, StarException> findInteractions();
  [[nodiscard]] std::expected<void, StarException> finish();

  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<WorkingCell>>, StarException> workingCell(Vec2I p);
  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<WorkingCellState>>, StarException> adjacentCell(WorkingCell& cell, Adjacency adjacency);
  bool inNoProcessingLimitRegion(Vec2I const& position) const;

  void setPressure(float pressure, WorkingCellState& cell);
  void transferPressure(float amount, WorkingCellState& source, WorkingCellState& dest, bool allowReverse);
  void transferLevel(float amount, WorkingCellState& source, WorkingCellState& dest, bool allowReverse);
  void setLevel(float level, WorkingCellState& cell);

  RandomSource m_random;
  LiquidCellEngineParameters m_engineParameters;
  std::shared_ptr<CellularLiquidWorld<LiquidId>> m_cellWorld;

  BAHashMap<LiquidId, BAOrderedHashSet<Vec2I>> m_activeCells;
  BAHashMap<LiquidId, unsigned> m_liquidTickDeltas;
  std::optional<unsigned> m_processingLimit;
  List<RectI> m_noProcessingLimitRegions;
  // Ordered subsets of the active queues: stable windows only require visiting
  // the ordinary budget prefix and these cells, never the skipped suffix.
  // An existing (even empty) entry is initialized for the current regions.
  // Missing entries are built only when that liquid is due to run.
  BAHashMap<LiquidId, BAOrderedHashSet<Vec2I>> m_unlimitedActiveCells;
  std::uint64_t m_step = 0;

  std::unique_ptr<WorkingCells> m_workingCells;
  List<std::reference_wrapper<WorkingCell>> m_currentActiveCells;
  BAHashSet<Vec2I> m_nextActiveCells;
  BAHashSet<std::tuple<Vec2I, LiquidId, Vec2I, LiquidId>> m_liquidInteractions;
  BAHashSet<std::tuple<Vec2I, LiquidId, Vec2I>> m_liquidCollisions;
};

template <typename LiquidId> CellularLiquidWorld<LiquidId>::~CellularLiquidWorld() = default;

template <typename LiquidId>
Vec2I CellularLiquidWorld<LiquidId>::uniqueLocation(Vec2I const& location) const {
  return location;
}

template <typename LiquidId>
[[nodiscard]] std::expected<float, StarException> CellularLiquidWorld<LiquidId>::drainLevel(Vec2I const&) const {
  return 0.0f;
}

template <typename LiquidId>
[[nodiscard]] std::expected<void, StarException> CellularLiquidWorld<LiquidId>::liquidInteraction(Vec2I const&, LiquidId, Vec2I const&, LiquidId) { return {}; }

template <typename LiquidId>
[[nodiscard]] std::expected<void, StarException> CellularLiquidWorld<LiquidId>::liquidCollision(Vec2I const&, LiquidId, Vec2I const&) { return {}; }

template <typename LiquidId>
LiquidCellEngine<LiquidId>::LiquidCellEngine(std::uint64_t randomSeed, LiquidCellEngineParameters parameters,
                                             std::shared_ptr<CellularLiquidWorld<LiquidId>> cellWorld)
    : m_random(randomSeed), m_engineParameters(parameters), m_cellWorld(cellWorld) {}

template <typename LiquidId>
unsigned LiquidCellEngine<LiquidId>::liquidTickDelta(LiquidId liquid) {
  return m_liquidTickDeltas.value(liquid, 1);
}

template <typename LiquidId>
void LiquidCellEngine<LiquidId>::setLiquidTickDelta(LiquidId liquid, unsigned tickDelta) {
  m_liquidTickDeltas[liquid] = tickDelta;
}

template <typename LiquidId>
void LiquidCellEngine<LiquidId>::setProcessingLimit(std::optional<unsigned> processingLimit) {
  m_processingLimit = processingLimit;
}

template <typename LiquidId>
List<RectI> LiquidCellEngine<LiquidId>::noProcessingLimitRegions() const {
  return m_noProcessingLimitRegions;
}

template <typename LiquidId>
void LiquidCellEngine<LiquidId>::setNoProcessingLimitRegions(List<RectI> noProcessingLimitRegions) {
  if (m_noProcessingLimitRegions == noProcessingLimitRegions)
    return;
  m_noProcessingLimitRegions = std::move(noProcessingLimitRegions);
  m_unlimitedActiveCells.clear();
}

template <typename LiquidId>
void LiquidCellEngine<LiquidId>::visitLocation(Vec2I const& p) {
  m_nextActiveCells.add(p);
}

template <typename LiquidId>
void LiquidCellEngine<LiquidId>::visitRegion(RectI const& region) {
  for (int x = region.xMin(); x < region.xMax(); ++x) {
    for (int y = region.yMin(); y < region.yMax(); ++y)
      m_nextActiveCells.add({x, y});
  }
}

template <typename LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::update() {
  auto prepared = setup();
  if (!prepared) return std::unexpected(std::move(prepared.error()));
  auto applied = applyPressure();
  if (!applied) return std::unexpected(std::move(applied.error()));
  auto spread = spreadPressure();
  if (!spread) return std::unexpected(std::move(spread.error()));
  auto limited = limitPressure();
  if (!limited) return std::unexpected(std::move(limited.error()));
  auto pressureMoved = pressureMove();
  if (!pressureMoved) return std::unexpected(std::move(pressureMoved.error()));
  auto overfill = spreadOverfill();
  if (!overfill) return std::unexpected(std::move(overfill.error()));
  auto moved = levelMove();
  if (!moved) return std::unexpected(std::move(moved.error()));
  auto interactions = findInteractions();
  if (!interactions) return std::unexpected(std::move(interactions.error()));
  auto finished = finish();
  if (!finished) return std::unexpected(std::move(finished.error()));
  ++m_step;
  return {};
}

template <typename LiquidId>
std::size_t LiquidCellEngine<LiquidId>::activeCells() const {
  std::size_t totalSize = 0;
  for (auto const& p : m_activeCells)
    totalSize += p.second.size();
  return totalSize;
}

template <typename LiquidId>
std::size_t LiquidCellEngine<LiquidId>::activeCells(LiquidId liquid) const {
  return m_activeCells.value(liquid).size();
}

template <typename LiquidId>
bool LiquidCellEngine<LiquidId>::isActive(Vec2I const& pos) const {
  for (auto const& p : m_activeCells) {
    if (p.second.contains(pos))
      return true;
  }
  return false;
}

template <typename LiquidId> bool LiquidCellEngine<LiquidId>::inNoProcessingLimitRegion(Vec2I const& position) const {
  for (auto const& region : m_noProcessingLimitRegions) {
    if (region.contains(position))
      return true;
  }
  return false;
}

template <typename LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::setup() {
  // Clear transient state left by a failed update.
  if (m_workingCells) {
    // clear() retained this bucket count before arena allocation. Preserve it:
    // bucket grouping determines flow write order, including failed prefixes.
    auto bucketCount = m_workingCells->cells.bucket_count();
    m_workingCells.reset();
    m_workingCells = std::make_unique<WorkingCells>(bucketCount);
  }
  m_currentActiveCells.clear();

  for (auto& activeCellsPair : m_activeCells) {
    unsigned tickDelta = liquidTickDelta(activeCellsPair.first);
    if (tickDelta == 0 || m_step % tickDelta != 0)
      continue;

    auto unlimited = m_unlimitedActiveCells.find(activeCellsPair.first);
    if (m_processingLimit && !m_noProcessingLimitRegions.empty() && unlimited == m_unlimitedActiveCells.end()) {
      auto& indexed = m_unlimitedActiveCells[activeCellsPair.first];
      for (auto const& position : activeCellsPair.second) {
        if (inNoProcessingLimitRegion(position))
          indexed.add(position);
      }
      unlimited = m_unlimitedActiveCells.find(activeCellsPair.first);
    }

    auto process = [&](Vec2I const& pos) -> std::expected<void, StarException> {
      auto found = workingCell(pos);
      if (!found)
        return std::unexpected(std::move(found.error()));
      auto cell = *found;
      if (cell && cell->get().liquid == activeCellsPair.first)
        m_currentActiveCells.push_back(*cell);
      activeCellsPair.second.remove(pos);
      if (unlimited != m_unlimitedActiveCells.end())
        unlimited->second.remove(pos);
      return {};
    };

    std::size_t limitedCellNumber = 0;
    auto position = activeCellsPair.second.begin();
    while (position != activeCellsPair.second.end() && (!m_processingLimit || limitedCellNumber < *m_processingLimit)) {
      // Copy only the current coordinate; advance before erasing its list node.
      auto pos = *position++;
      if (m_processingLimit && (unlimited == m_unlimitedActiveCells.end() || !unlimited->second.contains(pos)))
        ++limitedCellNumber;
      auto processed = process(pos);
      if (!processed)
        return std::unexpected(std::move(processed.error()));
    }

    // Cells beyond the ordinary budget still run if monitored. Removing the
    // prefix above also removes it from this ordered subset, preserving the
    // original input order (including equal-Y ties) without scanning skips.
    if (m_processingLimit && unlimited != m_unlimitedActiveCells.end()) {
      auto remaining = unlimited->second.begin();
      while (remaining != unlimited->second.end()) {
        auto pos = *remaining++;
        auto processed = process(pos);
        if (!processed)
          return std::unexpected(std::move(processed.error()));
      }
    }
  }

  std::sort(m_currentActiveCells.begin(), m_currentActiveCells.end(), [](std::reference_wrapper<WorkingCell> lhs, std::reference_wrapper<WorkingCell> rhs) {
      return lhs.get().position[1] < rhs.get().position[1];
    });
  return {};
}

template <typename LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::applyPressure() {
  for (auto const& selfCellRef : m_currentActiveCells) {
    auto& selfCell = selfCellRef.get();
    if (!selfCell.liquid || selfCell.sourceCell)
      continue;

    auto found = adjacentCell(selfCell, Adjacency::Top);
    if (!found) return std::unexpected(std::move(found.error()));
    auto topCell = *found;
    if (topCell && selfCell.liquid == topCell->get().liquid)
      setPressure(std::max(selfCell.pressure, topCell->get().pressure + std::min(topCell->get().level, 1.0f)), selfCell);
  }
  return {};
}

template <typename LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::spreadPressure() {
  for (auto const& selfCellRef : m_currentActiveCells) {
    auto& selfCell = selfCellRef.get();
    if (!selfCell.liquid)
      continue;

    auto spreadPressure = [&](Adjacency adjacency, float bias) -> std::expected<void, StarException> {
      auto found = adjacentCell(selfCell, adjacency);
      if (!found) return std::unexpected(std::move(found.error()));
      auto targetCell = *found;
      if (targetCell && !targetCell->get().sourceCell)
        transferPressure((selfCell.pressure + bias - targetCell->get().pressure) * m_engineParameters.pressureEqualizeFactor, selfCell, targetCell->get(), true);
      return {};
    };

    bool leftFirst = m_random.randb();
    auto first = spreadPressure(leftFirst ? Adjacency::Left : Adjacency::Right, 0.0f);
    if (!first) return std::unexpected(std::move(first.error()));
    auto second = spreadPressure(leftFirst ? Adjacency::Right : Adjacency::Left, 0.0f);
    if (!second) return std::unexpected(std::move(second.error()));
    auto bottom = spreadPressure(Adjacency::Bottom, 1.0f);
    if (!bottom) return std::unexpected(std::move(bottom.error()));
    auto top = spreadPressure(Adjacency::Top, -1.0f);
    if (!top) return std::unexpected(std::move(top.error()));
  }
  return {};
}

template <typename LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::limitPressure() {
  for (auto const& selfCellRef : m_currentActiveCells) {
    auto& selfCell = selfCellRef.get();
    float level = std::min(selfCell.level, 1.0f);
    auto found = adjacentCell(selfCell, Adjacency::Top);
    if (!found) return std::unexpected(std::move(found.error()));
    auto topCell = *found;

    // Force the pressure to the cell level if there is empty space above,
    // otherwise simply make sure the pressure is at least the level
    if (topCell && !topCell->get().liquid)
      setPressure(level, selfCell);
    else
      setPressure(std::max(selfCell.pressure, level), selfCell);
  }
  return {};
}

template <typename LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::pressureMove() {
  for (auto const& selfCellRef : m_currentActiveCells) {
    auto& selfCell = selfCellRef.get();
    if (!selfCell.liquid)
      continue;

    auto pressureMove = [&](Adjacency adjacency) -> std::expected<void, StarException> {
      auto found = adjacentCell(selfCell, adjacency);
      if (!found) return std::unexpected(std::move(found.error()));
      auto targetCell = *found;
      if (targetCell && !targetCell->get().sourceCell && targetCell->get().level >= selfCell.level) {
        float amount = (selfCell.pressure - targetCell->get().pressure) * m_engineParameters.pressureMoveFactor;
        amount = std::min(amount, selfCell.level - (1.0f - m_engineParameters.maximumPressureLevelImbalance));
        amount = std::min(amount, (1.0f + m_engineParameters.maximumPressureLevelImbalance) - targetCell->get().level);
        transferLevel(amount, selfCell, targetCell->get(), false);
      }
      return {};
    };

    bool leftFirst = m_random.randb();
    auto first = pressureMove(leftFirst ? Adjacency::Left : Adjacency::Right);
    if (!first) return std::unexpected(std::move(first.error()));
    auto second = pressureMove(leftFirst ? Adjacency::Right : Adjacency::Left);
    if (!second) return std::unexpected(std::move(second.error()));
  }
  return {};
}

template <typename LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::spreadOverfill() {
  for (auto const& selfCellRef : m_currentActiveCells) {
    auto& selfCell = selfCellRef.get();
    if (!selfCell.liquid || selfCell.sourceCell)
      continue;

    auto spreadOverfill = [&](Adjacency adjacency, float factor) -> std::expected<void, StarException> {
      float overfill = selfCell.level - 1.0f;
      if (overfill > 0.0f) {
        auto found = adjacentCell(selfCell, adjacency);
        if (!found) return std::unexpected(std::move(found.error()));
        auto targetCell = *found;
        if (targetCell)
          transferLevel(std::min(overfill, (selfCell.level - targetCell->get().level)) * factor, selfCell, targetCell->get(), false);
      }
      return {};
    };

    auto top = spreadOverfill(Adjacency::Top, m_engineParameters.spreadOverfillUpFactor);
    if (!top) return std::unexpected(std::move(top.error()));
    bool leftFirst = m_random.randb();
    auto first = spreadOverfill(leftFirst ? Adjacency::Left : Adjacency::Right, m_engineParameters.spreadOverfillLateralFactor);
    if (!first) return std::unexpected(std::move(first.error()));
    auto second = spreadOverfill(leftFirst ? Adjacency::Right : Adjacency::Left, m_engineParameters.spreadOverfillLateralFactor);
    if (!second) return std::unexpected(std::move(second.error()));
    auto bottom = spreadOverfill(Adjacency::Bottom, m_engineParameters.spreadOverfillDownFactor);
    if (!bottom) return std::unexpected(std::move(bottom.error()));
  }
  return {};
}

template <typename LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::levelMove() {
  for (auto const& selfCellRef : m_currentActiveCells) {
    auto& selfCell = selfCellRef.get();
    if (!selfCell.liquid)
      continue;

    auto found = adjacentCell(selfCell, Adjacency::Bottom);
    if (!found) return std::unexpected(std::move(found.error()));
    auto belowCell = *found;
    if (belowCell)
      transferLevel(std::min(1.0f - belowCell->get().level, selfCell.level), selfCell, belowCell->get(), false);

    auto drain = m_cellWorld->drainLevel(selfCell.position);
    if (!drain) return std::unexpected(std::move(drain.error()));
    setLevel(selfCell.level * (1.0f - *drain), selfCell);

    auto lateralMove = [&](Adjacency adjacency) -> std::expected<void, StarException> {
      auto found = adjacentCell(selfCell, adjacency);
      if (!found) return std::unexpected(std::move(found.error()));
      auto targetCell = *found;
      if (targetCell)
        transferLevel((selfCell.level - targetCell->get().level) * m_engineParameters.lateralMoveFactor, selfCell, targetCell->get(), false);
      return {};
    };

    bool leftFirst = m_random.randb();
    auto first = lateralMove(leftFirst ? Adjacency::Left : Adjacency::Right);
    if (!first) return std::unexpected(std::move(first.error()));
    auto second = lateralMove(leftFirst ? Adjacency::Right : Adjacency::Left);
    if (!second) return std::unexpected(std::move(second.error()));
  }
  return {};
}

template <typename LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::findInteractions() {
  for (auto const& selfCellRef : m_currentActiveCells) {
    auto& selfCell = selfCellRef.get();
    if (!selfCell.liquid)
      continue;

    for (auto adjacency : {Adjacency::Bottom, Adjacency::Top, Adjacency::Left, Adjacency::Right}) {
      auto found = adjacentCell(selfCell, adjacency);
      if (!found) return std::unexpected(std::move(found.error()));
      auto targetCell = *found;
      if (!targetCell) {
        Vec2I adjacentPos = selfCell.position;
        if (adjacency == Adjacency::Left)
          adjacentPos += Vec2I(-1, 0);
        else if (adjacency == Adjacency::Right)
          adjacentPos += Vec2I(1, 0);
        else if (adjacency == Adjacency::Bottom)
          adjacentPos += Vec2I(0, -1);
        else if (adjacency == Adjacency::Top)
          adjacentPos += Vec2I(0, 1);
        m_liquidCollisions.add(std::make_tuple(selfCell.position, *selfCell.liquid, adjacentPos));

      } else if (targetCell->get().liquid && *targetCell->get().liquid != *selfCell.liquid) {
        if (targetCell->get().level <= m_engineParameters.interactTransformationLevel
            || selfCell.level <= m_engineParameters.interactTransformationLevel) {
          if (selfCell.level > targetCell->get().level)
            targetCell->get().liquid = selfCell.liquid;
          else
            selfCell.liquid = targetCell->get().liquid;
        } else {
          // Make sure to add the point pair in a predictable order so that any
          // combination of Vec2I points will be unique in m_liquidInteractions
          if (selfCell.position < targetCell->get().position)
            m_liquidInteractions.add(std::make_tuple(selfCell.position, *selfCell.liquid, targetCell->get().position, *targetCell->get().liquid));
          else
            m_liquidInteractions.add(std::make_tuple(targetCell->get().position, *targetCell->get().liquid, selfCell.position, *selfCell.liquid));
        }
      }
    }
  }
  return {};
}

template <typename LiquidId>
[[nodiscard]] std::expected<void, StarException> LiquidCellEngine<LiquidId>::finish() {
  m_currentActiveCells.clear();

  auto workingCells = std::exchange(m_workingCells, {});
  if (workingCells) for (auto& workingCellPair : workingCells->cells) {
    if (workingCellPair.second && !workingCellPair.second->sourceCell) {
      if (workingCellPair.second->liquid) {
        if (workingCellPair.second->level < m_engineParameters.minimumLiquidLevel)
          workingCellPair.second->level = 0.0f;
      } else {
        workingCellPair.second->level = 0.0f;
      }

      if (workingCellPair.second->level == 0.0f) {
        workingCellPair.second->liquid = {};
        workingCellPair.second->pressure = 0.0f;
      }

      auto set = m_cellWorld->setFlow(workingCellPair.second->position, CellularLiquidFlowCell<LiquidId>{
          workingCellPair.second->liquid, workingCellPair.second->level, workingCellPair.second->pressure});
      if (!set) return std::unexpected(std::move(set.error()));
    }
  }
  workingCells.reset();

  for (auto const& interaction : std::exchange(m_liquidInteractions, {})) {
    auto applied = m_cellWorld->liquidInteraction(std::get<0>(interaction), std::get<1>(interaction), std::get<2>(interaction), std::get<3>(interaction));
    if (!applied) return std::unexpected(std::move(applied.error()));
  }

  for (auto const& interaction : std::exchange(m_liquidCollisions, {})) {
    auto applied = m_cellWorld->liquidCollision(std::get<0>(interaction), std::get<1>(interaction), std::get<2>(interaction));
    if (!applied) return std::unexpected(std::move(applied.error()));
  }

  for (auto const& c : std::exchange(m_nextActiveCells, {})) {
    auto visit = [this](Vec2I p) -> std::expected<void, StarException> {
      auto found = workingCell(p);
      if (!found) return std::unexpected(std::move(found.error()));
      auto cell = *found;
      if (cell && cell->get().liquid) {
        auto liquid = *cell->get().liquid;
        auto const& position = cell->get().position;
        if (m_activeCells[liquid].add(position)) {
          auto unlimited = m_unlimitedActiveCells.find(liquid);
          if (unlimited != m_unlimitedActiveCells.end() && inNoProcessingLimitRegion(position))
            unlimited->second.add(position);
        }
      }
      return {};
    };
    for (auto const& p : {c, c + Vec2I(-1, 0), c + Vec2I(1, 0), c + Vec2I(0, -1), c + Vec2I(0, 1)}) {
      auto visited = visit(p);
      if (!visited) return std::unexpected(std::move(visited.error()));
    }
  }

  eraseWhere(m_activeCells, [](auto const& p) { return p.second.empty(); });
  eraseWhere(m_unlimitedActiveCells, [this](auto const& p) { return !m_activeCells.contains(p.first); });
  return {};
}

template <typename LiquidId>
[[nodiscard]] std::expected<std::optional<std::reference_wrapper<typename LiquidCellEngine<LiquidId>::WorkingCell>>, StarException> LiquidCellEngine<LiquidId>::workingCell(Vec2I p) {
  p = m_cellWorld->uniqueLocation(p);

  if (!m_workingCells)
    m_workingCells = std::make_unique<WorkingCells>();
  // try_emplace does not allocate a candidate node on a cache hit.
  auto res = m_workingCells->cells.try_emplace(p);
  if (res.second) {
    auto cellData = m_cellWorld->cell(p);
    if (!cellData) {
      m_workingCells->cells.erase(res.first);
      return std::unexpected(std::move(cellData.error()));
    }
    if (std::holds_alternative<CellularLiquidFlowCell<LiquidId>>(*cellData)) {
      auto const& flowCell = std::get<CellularLiquidFlowCell<LiquidId>>(*cellData);
      res.first->second = WorkingCell{{p, flowCell.liquid, false, flowCell.level, flowCell.pressure}, {}, {}, {}, {}};
    } else if (std::holds_alternative<CellularLiquidSourceCell<LiquidId>>(*cellData)) {
      auto const& sourceCell = std::get<CellularLiquidSourceCell<LiquidId>>(*cellData);
      res.first->second = WorkingCell{{p, sourceCell.liquid, true, 1.0f, sourceCell.pressure}, {}, {}, {}, {}};
    }
  }
  return optionalRef(res.first->second);
}

template <typename LiquidId>
[[nodiscard]] std::expected<std::optional<std::reference_wrapper<typename LiquidCellEngine<LiquidId>::WorkingCellState>>, StarException> LiquidCellEngine<LiquidId>::adjacentCell(
    WorkingCell& cell, Adjacency adjacency) {
  auto getCell = [this](std::optional<std::reference_wrapper<WorkingCellState>>& cellRef, Vec2I cellPos) -> std::expected<std::optional<std::reference_wrapper<WorkingCellState>>, StarException> {
    if (cellRef)
      return cellRef;
    auto found = workingCell(cellPos);
    if (!found) return std::unexpected(std::move(found.error()));
    if (*found) {
      WorkingCellState& state = (*found)->get();
      cellRef = std::ref(state);
    }
    return cellRef;
  };

  if (adjacency == Adjacency::Left)
    return getCell(cell.leftCell, cell.position + Vec2I(-1, 0));
  else if (adjacency == Adjacency::Right)
    return getCell(cell.rightCell, cell.position + Vec2I(1, 0));
  else if (adjacency == Adjacency::Bottom)
    return getCell(cell.bottomCell, cell.position + Vec2I(0, -1));
  else if (adjacency == Adjacency::Top)
    return getCell(cell.topCell, cell.position + Vec2I(0, 1));

  return std::nullopt;
}

template <typename LiquidId>
void LiquidCellEngine<LiquidId>::setPressure(float pressure, WorkingCellState& cell) {
  if (!cell.liquid || cell.sourceCell)
    return;

  if (std::fabs(cell.pressure - pressure) > m_engineParameters.minimumLivenPressureChange)
    m_nextActiveCells.add(cell.position);
  cell.pressure = pressure;
}

template <typename LiquidId>
void LiquidCellEngine<LiquidId>::transferPressure(float amount, WorkingCellState& source, WorkingCellState& dest, bool allowReverse) {
  if (amount < 0.0f && allowReverse) {
    return transferPressure(-amount, dest, source, false);
  } else if (amount > 0.0f) {
    if (!source.liquid)
      return;

    if (source.sourceCell && dest.sourceCell)
      return;

    if (dest.liquid && dest.liquid != source.liquid)
      return;

    amount = std::min(amount, source.pressure);

    if (!source.sourceCell)
      source.pressure -= amount;

    if (dest.liquid && !dest.sourceCell)
      dest.pressure += amount;

    if (amount > m_engineParameters.minimumLivenPressureChange) {
      m_nextActiveCells.add(source.position);
      m_nextActiveCells.add(dest.position);
    }
  }
}

template <typename LiquidId>
void LiquidCellEngine<LiquidId>::setLevel(float level, WorkingCellState& cell) {
  if (!cell.liquid || cell.sourceCell)
    return;

  if (std::fabs(cell.level - level) > m_engineParameters.minimumLivenLevelChange)
    m_nextActiveCells.add(cell.position);

  cell.level = level;

  if (cell.level <= 0.0f) {
    cell.liquid = {};
    cell.level = 0.0f;
  }
}

template <typename LiquidId>
void LiquidCellEngine<LiquidId>::transferLevel(
    float amount, WorkingCellState& source, WorkingCellState& dest, bool allowReverse) {
  if (amount < 0.0f && allowReverse) {
    transferLevel(-amount, dest, source, false);

  } else if (amount > 0.0f) {
    if (!source.liquid)
      return;

    if (source.sourceCell && dest.sourceCell)
      return;

    if (dest.liquid && dest.liquid != source.liquid)
      return;

    amount = std::min(amount, source.level);
    if (!source.sourceCell)
      source.level -= amount;

    if (!dest.sourceCell) {
      dest.level += amount;
      dest.liquid = source.liquid;
    }

    if (!source.sourceCell && source.level == 0.0f)
      source.liquid = {};

    if (amount > m_engineParameters.minimumLivenLevelChange) {
      m_nextActiveCells.add(source.position);
      m_nextActiveCells.add(dest.position);
    }
  }
}

}

export module star.cellular_liquid;

export namespace Star {
  using ::Star::CellularLiquidCollisionCell;
  using ::Star::CellularLiquidFlowCell;
  using ::Star::CellularLiquidSourceCell;
  using ::Star::CellularLiquidCell;
  using ::Star::CellularLiquidWorld;
  using ::Star::LiquidCellEngineParameters;
  using ::Star::LiquidCellEngine;
}
