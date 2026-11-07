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

struct ActionState;
struct DecoratorState;
struct CompositeState;

struct BehaviorExceptionTag {
  static constexpr std::string_view name() { return "BehaviorException"; }
};
using BehaviorException = StarError<BehaviorExceptionTag, StarException>;

extern List<NodeParameterType> const BlackboardTypes;

class Blackboard {
public:
  Blackboard(LuaTable luaContext);

  [[nodiscard]] std::expected<LuaValue, StarException> get(NodeParameterType type, String const& key) const;
  [[nodiscard]] std::expected<void, StarException> set(NodeParameterType type, String const& key, LuaValue value);

  [[nodiscard]] std::expected<LuaTable, StarException> parameters(StringMap<NodeParameter> const& nodeParameters, std::uint64_t nodeId);
  [[nodiscard]] std::expected<void, StarException> setOutput(ActionNode const& node, LuaTable const& output);

  // takes the set of currently held ephemeral values
  Set<std::pair<NodeParameterType, String>> takeEphemerals();

  // clears any provided ephemerals that are not currently held
  [[nodiscard]] std::expected<void, StarException> clearEphemerals(Set<std::pair<NodeParameterType, String>> ephemerals);
private:
  LuaTable m_luaContext;

  HashMap<std::uint64_t, LuaTable> m_parameters;
  HashMap<NodeParameterType, StringMap<LuaValue>> m_board;

  HashMap<NodeParameterType, StringMap<List<std::pair<std::uint64_t, String>>>> m_input;
  StringMap<List<std::pair<std::uint64_t, LuaTable>>> m_vectorNumberInput;

  Set<std::pair<NodeParameterType, String>> m_ephemeral;
};

using NodeState = std::optional<std::variant<ActionState, DecoratorState, CompositeState>>;

using Coroutine = std::pair<LuaFunction, LuaThread>;

enum class NodeStatus {
  Invalid,
  Success,
  Failure,
  Running
};

using ActionReturn = LuaTupleReturn<NodeStatus, LuaValue>;
struct ActionState {
  LuaThread thread;
};

struct DecoratorState {
  DecoratorState(LuaThread thread);
  LuaThread thread;
  std::shared_ptr<NodeState> child;
};

struct CompositeState {
  CompositeState(std::size_t children);
  CompositeState(std::size_t children, std::size_t index);

  std::size_t index;
  List<std::shared_ptr<NodeState>> children;
};

class BehaviorState {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<BehaviorState>, StarException> create(std::shared_ptr<BehaviorTree const> tree, LuaTable context, std::optional<std::weak_ptr<Blackboard>> blackboard = {});

  [[nodiscard]] std::expected<NodeStatus, StarException> run(float dt);
  void clear();

  std::weak_ptr<Blackboard> blackboardPtr();
private:
  BehaviorState(std::shared_ptr<BehaviorTree const> tree, LuaTable context, std::optional<std::weak_ptr<Blackboard>> blackboard);
  [[nodiscard]] std::expected<std::shared_ptr<Blackboard>, StarException> board();

  [[nodiscard]] std::expected<LuaThread, StarException> nodeLuaThread(String const& funcName);

  [[nodiscard]] std::expected<NodeStatus, StarException> runNode(BehaviorNode const& node, NodeState& state);

  [[nodiscard]] std::expected<NodeStatus, StarException> runAction(ActionNode const& node, NodeState& state);
  [[nodiscard]] std::expected<NodeStatus, StarException> runDecorator(DecoratorNode const& node, NodeState& state);

  [[nodiscard]] std::expected<NodeStatus, StarException> runComposite(CompositeNode const& node, NodeState& state);
  [[nodiscard]] std::expected<NodeStatus, StarException> runSequence(SequenceNode const& node, NodeState& state);
  [[nodiscard]] std::expected<NodeStatus, StarException> runSelector(SelectorNode const& node, NodeState& state);
  [[nodiscard]] std::expected<NodeStatus, StarException> runParallel(ParallelNode const& node, NodeState& state);
  [[nodiscard]] std::expected<NodeStatus, StarException> runDynamic(DynamicNode const& node, NodeState& state);
  [[nodiscard]] std::expected<NodeStatus, StarException> runRandomize(RandomizeNode const& node, NodeState& state);

  std::shared_ptr<BehaviorTree const> m_tree;
  NodeState m_rootState;

  LuaTable m_luaContext;

  // The blackboard can either be created and owned by this behavior,
  // or a blackboard from another behavior can be used
  std::variant<std::shared_ptr<Blackboard>, std::weak_ptr<Blackboard>> m_board;

  // Keep threads here for recycling
  List<LuaThread> m_threads;
  StringMap<LuaFunction> m_functions;

  float m_lastDt;
};

}

export module star.behavior_state;

export namespace Star {
using ::Star::Blackboard;
using ::Star::BehaviorState;
using ::Star::ActionState;
using ::Star::DecoratorState;
using ::Star::CompositeState;
using ::Star::BehaviorExceptionTag;
using ::Star::BehaviorException;
using ::Star::BlackboardTypes;
using ::Star::NodeState;
using ::Star::Coroutine;
using ::Star::NodeStatus;
using ::Star::ActionReturn;
}
