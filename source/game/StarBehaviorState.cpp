import std;
import star.lua_root;
import star.root;
import star.string;
import star.list;
import star.set;
import star.format;
import star.output_proxy;
import star.json;
import star.game_types;
import star.lua;
import star.random;
import star.lua_game_converters;
import star.behavior_database;
import star.behavior_state;
import star.algorithm;
namespace Star {

extern List<NodeParameterType> const BlackboardTypes = {
  NodeParameterType::Json, NodeParameterType::Entity, NodeParameterType::Position,
  NodeParameterType::Vec2, NodeParameterType::Number, NodeParameterType::Bool,
  NodeParameterType::List, NodeParameterType::Table, NodeParameterType::String
};

Blackboard::Blackboard(LuaTable luaContext) : m_luaContext(std::move(luaContext)) {
  for (auto type : BlackboardTypes) {
    m_board.set(type, {});
    m_input.set(type, {});
  }
}

std::expected<void, StarException> Blackboard::set(NodeParameterType type, String const& key, LuaValue value) {
  auto board = m_board.get(type);
  if (!board) return std::unexpected(std::move(board.error()));
  if (std::holds_alternative<LuaNilType>(value))
    board->get().remove(key);
  else
    board->get().set(key, value);
  auto inputs = m_input.get(type);
  if (!inputs) return std::unexpected(std::move(inputs.error()));
  for (auto const& input : inputs->get().maybe(key).value_or(List<std::pair<std::uint64_t, String>>{})) {
    auto table = m_parameters.get(input.first);
    if (!table) return std::unexpected(std::move(table.error()));
    auto set = table->get().set(input.second, value);
    if (!set) return std::unexpected(std::move(set.error()));
  }
  if (type == NodeParameterType::Number) {
    for (auto const& input : m_vectorNumberInput.maybe(key).value_or(List<std::pair<std::uint64_t, LuaTable>>{})) {
      auto set = input.second.set(input.first, value);
      if (!set) return std::unexpected(std::move(set.error()));
    }
  }
  return {};
}

std::expected<LuaValue, StarException> Blackboard::get(NodeParameterType type, String const& key) const {
  auto board = m_board.get(type);
  if (!board) return std::unexpected(std::move(board.error()));
  return board->get().maybe(key).value_or(LuaNil);
}

std::expected<LuaTable, StarException> Blackboard::parameters(StringMap<NodeParameter> const& parameters, std::uint64_t nodeId) {
  if (auto table = m_parameters.maybe(nodeId)) return *table;
  auto created = m_luaContext.engine().createTable();
  if (!created) return std::unexpected(std::move(created.error()));
  LuaTable table = std::move(*created);
  for (auto const& p : parameters) {
    if (std::holds_alternative<String>(p.second.second)) {
      auto const& key = std::get<String>(p.second.second);
      auto input = m_input.get(p.second.first);
      if (!input) return std::unexpected(std::move(input.error()));
      input->get()[key].append({nodeId, p.first});
      auto value = get(p.second.first, key);
      if (!value) return std::unexpected(std::move(value.error()));
      auto set = table.set(p.first, *value);
      if (!set) return std::unexpected(std::move(set.error()));
    } else {
      Json const& value = std::get<Json>(p.second.second);
      if (value.isNull()) continue;
      if (p.second.first == NodeParameterType::Vec2) {
        auto vector = value.toArray();
        if (!vector) return std::unexpected(std::move(vector.error()));
        if (vector->size() < 2)
          return std::unexpected(StarException(strf("Vec2 parameter must contain at least two elements for key {}", p.first)));
        auto createdVector = m_luaContext.engine().createTable();
        if (!createdVector) return std::unexpected(std::move(createdVector.error()));
        LuaTable luaVector = std::move(*createdVector);
        for (int i = 0; i < 2; ++i) {
          if ((*vector)[i].isType(Json::Type::String)) {
            auto key = (*vector)[i].toString();
            if (!key) return std::unexpected(std::move(key.error()));
            m_vectorNumberInput[*key].append({i + 1, luaVector});
            auto number = get(NodeParameterType::Number, *key);
            if (!number) return std::unexpected(std::move(number.error()));
            auto set = luaVector.set(i + 1, *number);
            if (!set) return std::unexpected(std::move(set.error()));
          } else {
            auto set = luaVector.set(i + 1, (*vector)[i]);
            if (!set) return std::unexpected(std::move(set.error()));
          }
        }
        auto set = table.set(p.first, luaVector);
        if (!set) return std::unexpected(std::move(set.error()));
      } else {
        auto set = table.set(p.first, value);
        if (!set) return std::unexpected(std::move(set.error()));
      }
    }
  }
  auto added = m_parameters.add(nodeId, table);
  if (!added) return std::unexpected(std::move(added.error()));
  return table;
}

std::expected<void, StarException> Blackboard::setOutput(ActionNode const& node, LuaTable const& output) {
  for (auto const& p : node.output) {
    auto const& out = p.second.second;
    if (auto const& key = out.first) {
      auto value = output.get<LuaValue>(p.first);
      if (!value) return std::unexpected(std::move(value.error()));
      auto setResult = set(p.second.first, *key, *value);
      if (!setResult) return std::unexpected(std::move(setResult.error()));
      if (out.second) m_ephemeral.add({p.second.first, *key});
    }
  }
  return {};
}
Set<std::pair<NodeParameterType, String>> Blackboard::takeEphemerals() { return take(m_ephemeral); }
std::expected<void, StarException> Blackboard::clearEphemerals(Set<std::pair<NodeParameterType, String>> ephemerals) {
  for (auto const& p : ephemerals) {
    if (!m_ephemeral.contains(p)) {
      auto result = set(p.first, p.second, LuaNil);
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}
DecoratorState::DecoratorState(LuaThread thread) : thread(std::move(thread)), child(std::make_shared<NodeState>()) {}
CompositeState::CompositeState(std::size_t count) : index() {
  for (std::size_t i = 0; i < count; ++i) children.append(std::make_shared<NodeState>());
}
CompositeState::CompositeState(std::size_t count, std::size_t begin) : CompositeState(count) { index = begin; }

BehaviorState::BehaviorState(std::shared_ptr<BehaviorTree const> tree, LuaTable context, std::optional<std::weak_ptr<Blackboard>> blackboard)
  : m_tree(std::move(tree)), m_luaContext(std::move(context)), m_lastDt(0) {
  if (blackboard) m_board = *blackboard;
  else m_board = std::make_shared<Blackboard>(m_luaContext);
}
std::expected<std::shared_ptr<BehaviorState>, StarException> BehaviorState::create(std::shared_ptr<BehaviorTree const> tree, LuaTable context, std::optional<std::weak_ptr<Blackboard>> blackboard) {
  auto state = std::shared_ptr<BehaviorState>(new BehaviorState(std::move(tree), std::move(context), std::move(blackboard)));
  if (!state->m_tree || !state->m_tree->root)
    return std::unexpected(BehaviorException("Behavior tree has no root"));
  auto require = state->m_luaContext.get<LuaFunction>("require");
  if (!require) return std::unexpected(std::move(require.error()));
  for (auto const& script : state->m_tree->scripts) {
    auto loaded = require->invoke(script);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
  }
  for (auto const& name : state->m_tree->functions) {
    auto function = state->m_luaContext.get<LuaFunction>(name);
    if (!function) return std::unexpected(std::move(function.error()));
    state->m_functions.set(name, *function);
  }
  return state;
}
std::expected<NodeStatus, StarException> BehaviorState::run(float dt) {
  m_lastDt = dt;
  std::optional<Set<std::pair<NodeParameterType, String>>> ephemeral;
  if (std::holds_alternative<std::shared_ptr<Blackboard>>(m_board)) ephemeral = std::get<std::shared_ptr<Blackboard>>(m_board)->takeEphemerals();
  auto status = runNode(*m_tree->root, m_rootState);
  if (!status) return std::unexpected(std::move(status.error()));
  if (ephemeral) {
    auto cleared = std::get<std::shared_ptr<Blackboard>>(m_board)->clearEphemerals(std::move(*ephemeral));
    if (!cleared) return std::unexpected(std::move(cleared.error()));
  }
  return *status;
}
void BehaviorState::clear() { m_rootState.reset(); }
std::weak_ptr<Blackboard> BehaviorState::blackboardPtr() {
  if (std::holds_alternative<std::shared_ptr<Blackboard>>(m_board)) return std::get<std::shared_ptr<Blackboard>>(m_board);
  return std::get<std::weak_ptr<Blackboard>>(m_board);
}
std::expected<std::shared_ptr<Blackboard>, StarException> BehaviorState::board() {
  auto value = blackboardPtr().lock();
  if (!value) return std::unexpected(BehaviorException("Behavior blackboard has expired"));
  return value;
}
std::expected<LuaThread, StarException> BehaviorState::nodeLuaThread(String const& name) {
  auto thread = m_threads.maybeTakeLast();
  if (!thread) {
    auto created = m_luaContext.engine().createThread();
    if (!created) return std::unexpected(std::move(created.error()));
    thread.emplace(std::move(*created));
  }
  auto function = m_functions.get(name);
  if (!function) return std::unexpected(std::move(function.error()));
  auto pushed = thread->pushFunction(function->get());
  if (!pushed) return std::unexpected(std::move(pushed.error()));
  return std::move(*thread);
}
std::expected<NodeStatus, StarException> BehaviorState::runNode(BehaviorNode const& node, NodeState& state) {
  auto status = [&self = *this, &node, &state]() -> std::expected<NodeStatus, StarException> {
    if (std::holds_alternative<ActionNode>(node)) return self.runAction(std::get<ActionNode>(node), state);
    if (std::holds_alternative<DecoratorNode>(node)) return self.runDecorator(std::get<DecoratorNode>(node), state);
    if (std::holds_alternative<CompositeNode>(node)) return self.runComposite(std::get<CompositeNode>(node), state);
    return std::unexpected(BehaviorException("Unidentified behavior node type"));
  }();
  if (!status) return std::unexpected(std::move(status.error()));
  if (*status != NodeStatus::Running) state.reset();
  return *status;
}
std::expected<NodeStatus, StarException> BehaviorState::runAction(ActionNode const& node, NodeState& state) {
  auto id = (std::uint64_t)&node;
  auto blackboard = board();
  if (!blackboard) return std::unexpected(std::move(blackboard.error()));
  ActionReturn result(NodeStatus::Invalid, LuaNil);
  if (!state) {
    auto parameters = (*blackboard)->parameters(node.parameters, id);
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    auto thread = nodeLuaThread(node.name);
    if (!thread) return std::unexpected(std::move(thread.error()));
    auto resumed = thread->resume<ActionReturn>(*parameters, blackboardPtr(), id, m_lastDt);
    if (!resumed) {
      if (!resumed.error().is<LuaException>()) return std::unexpected(std::move(resumed.error()));
      return std::unexpected(StarException(strf("Lua Exception caught running action node {} in behavior {}", node.name, m_tree->name), ExceptionDiagnostic(resumed.error())));
    }
    result = resumed->value_or(ActionReturn(NodeStatus::Invalid, LuaNil));
    auto status = std::get<0>(result);
    if (status != NodeStatus::Success && status != NodeStatus::Failure) state.emplace(ActionState{*thread});
  } else {
    LuaThread const& thread = std::get<ActionState>(*state).thread;
    auto resumed = thread.resume<ActionReturn>(m_lastDt);
    if (!resumed) {
      if (!resumed.error().is<LuaException>()) return std::unexpected(std::move(resumed.error()));
      return std::unexpected(StarException(strf("Lua Exception caught resuming action node {} in behavior {}", node.name, m_tree->name), ExceptionDiagnostic(resumed.error())));
    }
    result = resumed->value_or(ActionReturn(NodeStatus::Invalid, LuaNil));
    auto status = std::get<0>(result);
    if (status == NodeStatus::Success || status == NodeStatus::Failure) m_threads.append(thread);
  }
  if (std::holds_alternative<LuaTable>(std::get<1>(result))) {
    auto output = (*blackboard)->setOutput(node, std::get<LuaTable>(std::get<1>(result)));
    if (!output) return std::unexpected(std::move(output.error()));
  }
  return std::get<0>(result);
}
std::expected<NodeStatus, StarException> BehaviorState::runDecorator(DecoratorNode const& node, NodeState& state) {
  auto id = (std::uint64_t)&node;
  NodeStatus status = NodeStatus::Running;
  if (!state) {
    auto blackboard = board();
    if (!blackboard) return std::unexpected(std::move(blackboard.error()));
    auto parameters = (*blackboard)->parameters(node.parameters, id);
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    auto thread = nodeLuaThread(node.name);
    if (!thread) return std::unexpected(std::move(thread.error()));
    auto resumed = thread->resume<NodeStatus>(*parameters, blackboardPtr(), id);
    if (!resumed) {
      if (!resumed.error().is<LuaException>()) return std::unexpected(std::move(resumed.error()));
      return std::unexpected(StarException(strf("Lua Exception caught initializing decorator node {} in behavior {}", node.name, m_tree->name), ExceptionDiagnostic(resumed.error())));
    }
    status = resumed->value_or(NodeStatus::Invalid);
    if (status == NodeStatus::Success || status == NodeStatus::Failure) return status;
    state.emplace(DecoratorState(*thread));
  }
  auto& decorator = std::get<DecoratorState>(*state);
  while (status == NodeStatus::Running) {
    auto child = runNode(*node.child, *decorator.child);
    if (!child) return std::unexpected(std::move(child.error()));
    if (*child == NodeStatus::Success || *child == NodeStatus::Failure) {
      auto resumed = decorator.thread.resume<NodeStatus>(*child);
      if (!resumed) {
        if (!resumed.error().is<LuaException>()) return std::unexpected(std::move(resumed.error()));
        return std::unexpected(StarException(strf("Lua Exception caught resuming decorator node {} in behavior {}", node.name, m_tree->name), ExceptionDiagnostic(resumed.error())));
      }
      status = resumed->value_or(NodeStatus::Invalid);
    } else return NodeStatus::Running;
  }
  m_threads.append(decorator.thread);
  return status;
}
std::expected<NodeStatus, StarException> BehaviorState::runComposite(CompositeNode const& node, NodeState& state) {
  return std::visit([&self = *this, &state](auto const& value) -> std::expected<NodeStatus, StarException> {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, SequenceNode>) return self.runSequence(value, state);
    else if constexpr (std::is_same_v<T, SelectorNode>) return self.runSelector(value, state);
    else if constexpr (std::is_same_v<T, ParallelNode>) return self.runParallel(value, state);
    else if constexpr (std::is_same_v<T, DynamicNode>) return self.runDynamic(value, state);
    else return self.runRandomize(value, state);
  }, node);
}
std::expected<NodeStatus, StarException> BehaviorState::runSequence(SequenceNode const& node, NodeState& state) {
  if (!state) state.emplace(CompositeState(node.children.size()));
  auto& composite = std::get<CompositeState>(*state);
  while (composite.index < node.children.size()) {
    auto status = runNode(*node.children[composite.index], *composite.children[composite.index]);
    if (!status) return std::unexpected(std::move(status.error()));
    if (*status == NodeStatus::Failure || *status == NodeStatus::Running) return *status;
    ++composite.index;
  }
  return NodeStatus::Success;
}
std::expected<NodeStatus, StarException> BehaviorState::runSelector(SelectorNode const& node, NodeState& state) {
  if (!state) state.emplace(CompositeState(node.children.size()));
  auto& composite = std::get<CompositeState>(*state);
  while (composite.index < node.children.size()) {
    auto status = runNode(*node.children[composite.index], *composite.children[composite.index]);
    if (!status) return std::unexpected(std::move(status.error()));
    if (*status == NodeStatus::Success || *status == NodeStatus::Running) return *status;
    ++composite.index;
  }
  return NodeStatus::Failure;
}
std::expected<NodeStatus, StarException> BehaviorState::runParallel(ParallelNode const& node, NodeState& state) {
  if (!state) state.emplace(CompositeState(node.children.size()));
  auto& composite = std::get<CompositeState>(*state);
  int failed = 0, succeeded = 0;
  for (std::size_t i = 0; i < node.children.size(); ++i) {
    auto status = runNode(*node.children[i], *composite.children[i]);
    if (!status) return std::unexpected(std::move(status.error()));
    if (*status == NodeStatus::Success) ++succeeded;
    else if (*status == NodeStatus::Failure) ++failed;
    if (succeeded >= node.succeed || failed >= node.fail) return succeeded >= node.succeed ? NodeStatus::Success : NodeStatus::Failure;
  }
  return NodeStatus::Running;
}
std::expected<NodeStatus, StarException> BehaviorState::runDynamic(DynamicNode const& node, NodeState& state) {
  if (!state) state.emplace(CompositeState(node.children.size()));
  auto& composite = std::get<CompositeState>(*state);
  for (std::size_t i = 0; i <= composite.index; ++i) {
    auto child = node.children.at(i);
    if (!child) return std::unexpected(std::move(child.error()));
    auto childState = composite.children.at(i);
    if (!childState) return std::unexpected(std::move(childState.error()));
    auto status = runNode(*child->get(), *childState->get());
    if (!status) return std::unexpected(std::move(status.error()));
    if (*status == NodeStatus::Failure && i == composite.index) ++composite.index;
    if (i < composite.index && (*status == NodeStatus::Success || *status == NodeStatus::Running)) {
      auto previous = composite.children.at(composite.index);
      if (!previous) return std::unexpected(std::move(previous.error()));
      previous->get()->reset();
      composite.index = i;
    }
    if (*status == NodeStatus::Success || composite.index >= node.children.size()) return *status;
  }
  return NodeStatus::Running;
}
std::expected<NodeStatus, StarException> BehaviorState::runRandomize(RandomizeNode const& node, NodeState& state) {
  if (!state) {
    auto root = luaExecutionRoot(m_luaContext.engine());
    if (!root) return std::unexpected(std::move(root.error()));
    state.emplace(CompositeState(node.children.size(), root->get().random().randUInt(node.children.size() - 1)));
  }
  auto& composite = std::get<CompositeState>(*state);
  auto child = node.children.at(composite.index);
  if (!child) return std::unexpected(std::move(child.error()));
  auto childState = composite.children.at(composite.index);
  if (!childState) return std::unexpected(std::move(childState.error()));
  return runNode(*child->get(), *childState->get());
}
}
