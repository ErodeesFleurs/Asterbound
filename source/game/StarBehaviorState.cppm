module;

import std;
import star.string;
import star.list;
import star.map;
import star.set;
import star.format;
import star.json;
import star.game_types;
import star.lua;

import star.behavior_database;

namespace Star {

struct BehaviorExceptionTag {
  static constexpr std::string_view name() { return "BehaviorException"; }
};
using BehaviorException = StarError<BehaviorExceptionTag, StarException>;

class Blackboard {
public:
  explicit Blackboard(LuaTable lua_context);

  [[nodiscard]] std::expected<LuaValue, StarException> get(NodeParameterType type, String const& key) const;
  [[nodiscard]] std::expected<void, StarException> set(NodeParameterType type, String const& key, LuaValue value);

  [[nodiscard]] std::expected<LuaTable, StarException> parameters(StringMap<NodeParameter> const& parameters, std::uint64_t node_id);
  void retain_parameters(std::uint64_t node_id);
  void release_parameters(std::uint64_t node_id);
  [[nodiscard]] std::expected<void, StarException> set_output(ActionNode const& node, LuaTable const& output);

  // Only the owning behavior advances epochs. A failed run leaves the old keys pending.
  void begin_ephemerals();
  [[nodiscard]] std::expected<void, StarException> clear_ephemerals();

private:
  using BoardKey = std::pair<NodeParameterType, String>;

  template <typename Key> requires (std::same_as<Key, String> || std::same_as<Key, int>)
  struct Input {
    std::uint64_t node_id;
    LuaTable table;
    Key key;
  };
  struct Subscribers {
    std::vector<Input<String>> fields;
    std::vector<Input<int>> components;
  };
  struct Bucket {
    StringMap<LuaValue> values;
    StringMap<std::shared_ptr<Subscribers>> subscribers;
  };
  struct ParameterTable {
    LuaTable table;
    std::vector<BoardKey> sources;
    std::size_t owners = 0;
  };

  static Subscribers& writable_subscribers(std::shared_ptr<Subscribers>& subscribers);
  LuaTable m_lua_context;
  std::array<Bucket, node_parameter_type_names.size()> m_buckets;
  HashMap<std::uint64_t, ParameterTable> m_parameters;
  std::set<BoardKey> m_ephemeral;
  std::set<BoardKey> m_pending_ephemeral;
};

enum class NodeStatus {
  Invalid,
  Success,
  Failure,
  Running
};

class BehaviorState {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  BehaviorState(ConstructionKey, std::shared_ptr<BehaviorTree const> tree, LuaTable context,
      std::optional<std::weak_ptr<Blackboard>> board);
  ~BehaviorState();
  [[nodiscard]] static std::expected<std::shared_ptr<BehaviorState>, StarException> create(
      std::shared_ptr<BehaviorTree const> tree, LuaTable context, std::optional<std::weak_ptr<Blackboard>> board = {});

  [[nodiscard]] std::expected<NodeStatus, StarException> run(float dt);
  void clear();

  std::weak_ptr<Blackboard> blackboard() const;

private:
  struct NodeState;
  struct DecoratorState {
    LuaThread thread;
    std::unique_ptr<NodeState> child;
  };
  struct CursorState {
    std::size_t index = 0;
    std::unique_ptr<NodeState> child;
  };
  struct BranchState {
    std::unique_ptr<NodeState[]> children;
    std::size_t index = 0;
  };
  struct NodeState {
    std::variant<std::monostate, LuaThread, DecoratorState, CursorState, BranchState> value;
    bool empty() const { return std::holds_alternative<std::monostate>(value); }
    void clear() { value.emplace<std::monostate>(); }
  };

  using NodeResult = std::expected<NodeStatus, StarException>;
  [[nodiscard]] std::expected<LuaTable, StarException> parameters(Blackboard& board,
      StringMap<NodeParameter> const& inputs, std::uint64_t node_id);
  [[nodiscard]] std::expected<LuaThread, StarException> node_thread(String const& name);
  void recycle_thread(LuaThread& thread);

  [[nodiscard]] NodeResult run_node(BehaviorNode const& node, NodeState& state);
  [[nodiscard]] NodeResult run_child(BehaviorNode const& node, std::unique_ptr<NodeState>& child);
  [[nodiscard]] NodeResult execute(ActionNode const& node, NodeState& state);
  [[nodiscard]] NodeResult execute(DecoratorNode const& node, NodeState& state);
  [[nodiscard]] NodeResult execute(CompositeNode const& node, NodeState& state);
  [[nodiscard]] NodeResult execute(SequenceNode const& node, NodeState& state);
  [[nodiscard]] NodeResult execute(SelectorNode const& node, NodeState& state);
  [[nodiscard]] NodeResult execute(ParallelNode const& node, NodeState& state);
  [[nodiscard]] NodeResult execute(DynamicNode const& node, NodeState& state);
  [[nodiscard]] NodeResult execute(RandomizeNode const& node, NodeState& state);
  [[nodiscard]] NodeResult run_serial(List<std::shared_ptr<BehaviorNode const>> const& children,
      NodeState& state, NodeStatus stop, NodeStatus exhausted);

  std::shared_ptr<BehaviorTree const> m_tree;
  NodeState m_root_state;
  LuaTable m_lua_context;
  std::shared_ptr<Blackboard> m_owned_board;
  std::weak_ptr<Blackboard> m_board;
  HashSet<std::uint64_t> m_parameter_ids;

  List<LuaThread> m_threads;
  StringMap<LuaFunction> m_functions;

  float m_last_dt = 0;
  bool m_running = false;
  bool m_clear_requested = false;
};

}

export module star.behavior_state;

export namespace Star {
using ::Star::Blackboard;
using ::Star::BehaviorState;
using ::Star::BehaviorExceptionTag;
using ::Star::BehaviorException;
using ::Star::NodeStatus;
}
