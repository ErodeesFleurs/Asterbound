module;
import std;
import star.format;
import star.list;
import star.material_types;
import star.random;
import star.rect;
import star.set;
import star.vector;

namespace Star {
class Root;

enum class BlockMoveResult { Unchanged, Changed };

struct BlockMoveError {
  StarException cause;
  BlockMoveResult result = BlockMoveResult::Unchanged;
};

class FallingBlocksFacade {
public:
  virtual ~FallingBlocksFacade() = default;

  [[nodiscard]] virtual std::expected<FallingBehavior, StarException> source_behavior(Vec2I const& pos) = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> target_open(Vec2I const& pos) = 0;
  [[nodiscard]] virtual std::expected<BlockMoveResult, BlockMoveError> move_block(Vec2I const& from, Vec2I const& to) = 0;
};

class FallingBlocksAgent {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  FallingBlocksAgent(ConstructionKey, std::shared_ptr<FallingBlocksFacade> facade, std::uint64_t seed, float probability);
  [[nodiscard]] static std::expected<std::shared_ptr<FallingBlocksAgent>, StarException> create(Root& root, std::shared_ptr<FallingBlocksFacade> facade);
  [[nodiscard]] static std::expected<std::shared_ptr<FallingBlocksAgent>, StarException> create(std::shared_ptr<FallingBlocksFacade> facade, std::uint64_t seed, double probability);

  [[nodiscard]] std::expected<void, StarException> update();

  void visit_location(Vec2I const& location);
  void visit_region(RectI const& region);

private:
  void begin_wave(HashSet<Vec2I>& candidates);
  [[nodiscard]] std::expected<std::optional<Vec2I>, StarException> choose_target(Vec2I const& pos);
  void schedule_change(Vec2I const& from, Vec2I const& to);

  std::shared_ptr<FallingBlocksFacade> m_facade;
  float m_propagate_probability;
  HashSet<Vec2I> m_pending;
  HashSet<Vec2I> m_next_wave;
  List<Vec2I> m_positions;
  std::size_t m_cursor = 0;
  std::optional<bool> m_prefer_left;
  RandomSource m_random;
};

}

export module star.falling_blocks_agent;

export namespace Star {
  using ::Star::BlockMoveResult;
  using ::Star::BlockMoveError;
  using ::Star::FallingBlocksFacade;
  using ::Star::FallingBlocksAgent;
}
