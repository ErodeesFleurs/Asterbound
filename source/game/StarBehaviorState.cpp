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

namespace {
static_assert([] {
  for (std::size_t i = 0; i < node_parameter_type_names.size(); ++i)
    if (std::to_underlying(node_parameter_type_names[i].first) != i) return false;
  return true;
}(), "Blackboard buckets require dense parameter types");

BehaviorException invalid_parameter_type(NodeParameterType type) {
  return BehaviorException(Star::format("Invalid blackboard parameter type {}", std::to_underlying(type)));
}
}

Blackboard::Blackboard(LuaTable lua_context) : m_lua_context(std::move(lua_context)) {}

Blackboard::Subscribers& Blackboard::writable_subscribers(std::shared_ptr<Subscribers>& subscribers) {
  if (!subscribers)
    subscribers = std::make_shared<Subscribers>();
  else if (subscribers.use_count() != 1)
    subscribers = std::make_shared<Subscribers>(*subscribers);
  return *subscribers;
}

std::expected<void, StarException> Blackboard::set(NodeParameterType type, String const& key, LuaValue value) {
  auto index = std::to_underlying(type);
  if (index >= m_buckets.size()) return std::unexpected(invalid_parameter_type(type));
  auto& bucket = m_buckets[index];
  // A Lua __newindex may register or retire nodes, even for this very key.
  auto snapshot = bucket.subscribers.value(key);
  if (std::holds_alternative<LuaNilType>(value))
    bucket.values.erase(key);
  else if (snapshot)
    bucket.values.insert_or_assign(key, value);
  else
    bucket.values.insert_or_assign(key, std::move(value));
  if (snapshot) {
    for (auto const& input : snapshot->fields) {
      auto updated = input.table.set(input.key, value);
      if (!updated) return std::unexpected(std::move(updated.error()));
    }
    for (auto const& input : snapshot->components) {
      auto updated = input.table.set(input.key, value);
      if (!updated) return std::unexpected(std::move(updated.error()));
    }
  }
  return {};
}

std::expected<LuaValue, StarException> Blackboard::get(NodeParameterType type, String const& key) const {
  auto index = std::to_underlying(type);
  if (index >= m_buckets.size()) return std::unexpected(invalid_parameter_type(type));
  return m_buckets[index].values.value(key, lua_nil);
}

std::expected<LuaTable, StarException> Blackboard::parameters(StringMap<NodeParameter> const& parameters, std::uint64_t node_id) {
  if (auto found = m_parameters.find(node_id); found != m_parameters.end()) return found->second.table;
  auto created = m_lua_context.engine().create_table();
  if (!created) return std::unexpected(std::move(created.error()));
  LuaTable table = std::move(*created);

  struct PendingInput {
    BoardKey source;
    LuaTable table;
    std::variant<String, int> key;
  };
  std::vector<PendingInput> pending;
  for (auto const& [field, parameter] : parameters) {
    auto const& [type, binding] = parameter;
    auto index = std::to_underlying(type);
    if (index >= m_buckets.size()) return std::unexpected(invalid_parameter_type(type));
    if (std::holds_alternative<String>(binding)) {
      auto const& key = std::get<String>(binding);
      auto updated = table.set(field, m_buckets[index].values.value(key, lua_nil));
      if (!updated) return std::unexpected(std::move(updated.error()));
      pending.push_back({{type, key}, table, field});
      continue;
    }

    Json const& value = std::get<Json>(binding);
    if (value.is_null()) continue;
    if (type != NodeParameterType::Vec2) {
      auto updated = table.set(field, value);
      if (!updated) return std::unexpected(std::move(updated.error()));
      continue;
    }
    auto vector = value.array_ptr();
    if (!vector) return std::unexpected(std::move(vector.error()));
    if ((*vector)->size() < 2)
      return std::unexpected(BehaviorException(Star::format("Vec2 parameter must contain at least two elements for key {}", field)));
    auto component_table = m_lua_context.engine().create_table();
    if (!component_table) return std::unexpected(std::move(component_table.error()));
    for (int i = 0; i < 2; ++i) {
      auto const& component = (**vector)[i];
      if (component.is_type(Json::Type::String)) {
        auto key = component.to_string();
        if (!key) return std::unexpected(std::move(key.error()));
        auto const& numbers = m_buckets[std::to_underlying(NodeParameterType::Number)].values;
        auto updated = component_table->set(i + 1, numbers.value(*key, lua_nil));
        if (!updated) return std::unexpected(std::move(updated.error()));
        pending.push_back({{NodeParameterType::Number, std::move(*key)}, *component_table, i + 1});
      } else {
        auto updated = component_table->set(i + 1, component);
        if (!updated) return std::unexpected(std::move(updated.error()));
      }
    }
    auto updated = table.set(field, *component_table);
    if (!updated) return std::unexpected(std::move(updated.error()));
  }

  // No Lua calls or fallible conversions after this point.
  auto [entry, inserted] = m_parameters.try_emplace(node_id, ParameterTable{table, {}});
  if (!inserted) return entry->second.table;
  auto& sources = entry->second.sources;
  sources.reserve(pending.size());
  for (auto& input : pending) {
    auto& bucket = m_buckets[std::to_underlying(input.source.first)];
    auto& subscribers = writable_subscribers(bucket.subscribers[input.source.second]);
    if (std::holds_alternative<String>(input.key))
      subscribers.fields.push_back({node_id, std::move(input.table), std::move(std::get<String>(input.key))});
    else
      subscribers.components.push_back({node_id, std::move(input.table), std::get<int>(input.key)});
    sources.push_back(std::move(input.source));
  }
  return table;
}

void Blackboard::retain_parameters(std::uint64_t node_id) {
  auto entry = m_parameters.find(node_id);
  if (entry != m_parameters.end()) ++entry->second.owners;
}

void Blackboard::release_parameters(std::uint64_t node_id) {
  auto entry = m_parameters.find(node_id);
  if (entry == m_parameters.end()) return;
  if (entry->second.owners && --entry->second.owners) return;
  for (auto const& [type, key] : entry->second.sources) {
    auto& inputs = m_buckets[std::to_underlying(type)].subscribers;
    auto found = inputs.find(key);
    if (found == inputs.end()) continue;
    auto& subscribers = writable_subscribers(found->second);
    std::erase_if(subscribers.fields, [node_id](Input<String> const& input) { return input.node_id == node_id; });
    std::erase_if(subscribers.components, [node_id](Input<int> const& input) { return input.node_id == node_id; });
    if (subscribers.fields.empty() && subscribers.components.empty()) inputs.erase(found);
  }
  m_parameters.erase(entry);
}

std::expected<void, StarException> Blackboard::set_output(ActionNode const& node, LuaTable const& output) {
  for (auto const& [field, binding] : node.output) {
    if (std::to_underlying(binding.type) >= m_buckets.size()) return std::unexpected(invalid_parameter_type(binding.type));
    if (!binding.key) continue;
    auto value = output.get<LuaValue>(field);
    if (!value) return std::unexpected(std::move(value.error()));
    // set publishes the board value before notifying Lua, which can fail.
    if (binding.ephemeral) m_ephemeral.emplace(binding.type, *binding.key);
    auto updated = set(binding.type, *binding.key, std::move(*value));
    if (!updated) return std::unexpected(std::move(updated.error()));
  }
  return {};
}

void Blackboard::begin_ephemerals() {
  if (m_pending_ephemeral.empty())
    m_pending_ephemeral.swap(m_ephemeral);
  else {
    m_pending_ephemeral.merge(m_ephemeral);
    m_ephemeral.clear(); // duplicates were already pending
  }
}

std::expected<void, StarException> Blackboard::clear_ephemerals() {
  while (!m_pending_ephemeral.empty()) {
    auto old = m_pending_ephemeral.extract(m_pending_ephemeral.begin());
    if (m_ephemeral.contains(old.value())) continue;
    auto cleared = set(old.value().first, old.value().second, lua_nil);
    if (!cleared) {
      m_pending_ephemeral.insert(std::move(old));
      return std::unexpected(std::move(cleared.error()));
    }
  }
  return {};
}

namespace {

using ActionReturn = LuaTupleReturn<NodeStatus, LuaValue>;

template <typename Node>
concept executable_node = std::same_as<Node, ActionNode> || std::same_as<Node, DecoratorNode>
    || std::same_as<Node, CompositeNode> || std::same_as<Node, SequenceNode> || std::same_as<Node, SelectorNode>
    || std::same_as<Node, ParallelNode> || std::same_as<Node, DynamicNode> || std::same_as<Node, RandomizeNode>;

StarException node_error(StarException error, std::string_view operation, String const& node, String const& tree) {
  if (!error.is<LuaException>()) return error;
  return StarException(Star::format("Lua Exception caught {} node {} in behavior {}", operation, node, tree),
      ExceptionDiagnostic(error));
}

}

BehaviorState::BehaviorState(ConstructionKey, std::shared_ptr<BehaviorTree const> tree, LuaTable context,
    std::optional<std::weak_ptr<Blackboard>> board)
  : m_tree(std::move(tree)), m_lua_context(std::move(context)) {
  if (board) {
    m_board = std::move(*board);
  } else {
    m_owned_board = std::make_shared<Blackboard>(m_lua_context);
    m_board = m_owned_board;
  }
}

BehaviorState::~BehaviorState() {
  if (auto board = m_board.lock()) {
    for (auto id : m_parameter_ids)
      board->release_parameters(id);
  }
}

std::expected<std::shared_ptr<BehaviorState>, StarException> BehaviorState::create(
    std::shared_ptr<BehaviorTree const> tree, LuaTable context, std::optional<std::weak_ptr<Blackboard>> board) {
  if (!tree || !tree->root)
    return std::unexpected(BehaviorException("Behavior tree has no root"));
  auto require = context.get<LuaFunction>("require");
  if (!require) return std::unexpected(std::move(require.error()));
  for (auto const& script : tree->scripts) {
    auto loaded = require->invoke(script);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
  }
  auto state = std::make_shared<BehaviorState>(ConstructionKey{}, std::move(tree), std::move(context), std::move(board));
  for (auto const& name : state->m_tree->functions) {
    auto function = state->m_lua_context.get<LuaFunction>(name);
    if (!function) return std::unexpected(std::move(function.error()));
    state->m_functions.set(name, std::move(*function));
  }
  return state;
}

auto BehaviorState::run(float dt) -> NodeResult {
  if (m_running)
    return std::unexpected(BehaviorException("Cannot run the same behavior recursively"));
  m_running = true;
  auto finish = make_scope_exit([this] {
    m_running = false;
    if (std::exchange(m_clear_requested, false)) m_root_state.clear();
  });
  m_last_dt = dt;
  if (m_owned_board) m_owned_board->begin_ephemerals();
  auto status = run_node(*m_tree->root, m_root_state);
  if (status && m_owned_board) {
    auto cleared = m_owned_board->clear_ephemerals();
    if (!cleared) return std::unexpected(std::move(cleared.error()));
  }
  return status;
}

void BehaviorState::clear() {
  // A Lua callback can clear its own behavior while execute() borrows a child state.
  if (m_running) m_clear_requested = true;
  else m_root_state.clear();
}

std::weak_ptr<Blackboard> BehaviorState::blackboard() const {
  return m_board;
}

std::expected<LuaTable, StarException> BehaviorState::parameters(Blackboard& board,
    StringMap<NodeParameter> const& inputs, std::uint64_t node_id) {
  auto table = board.parameters(inputs, node_id);
  if (table && m_parameter_ids.add(node_id))
    board.retain_parameters(node_id);
  return table;
}

std::expected<LuaThread, StarException> BehaviorState::node_thread(String const& name) {
  auto function = m_functions.get(name);
  if (!function) return std::unexpected(std::move(function.error()));
  auto thread = m_threads.try_take_back();
  if (!thread) {
    auto created = m_lua_context.engine().create_thread();
    if (!created) return std::unexpected(std::move(created.error()));
    thread.emplace(std::move(*created));
  }
  auto pushed = thread->push_function(function->get());
  if (!pushed) return std::unexpected(std::move(pushed.error()));
  return std::move(*thread);
}

void BehaviorState::recycle_thread(LuaThread& thread) {
  // A script can yield a terminal status without actually returning from its coroutine.
  if (thread.status() == LuaThread::Status::Dead)
    m_threads.push_back(std::move(thread));
}

auto BehaviorState::run_node(BehaviorNode const& node, NodeState& state) -> NodeResult {
  auto status = std::visit([&](executable_node auto const& value) { return execute(value, state); }, node);
  if (status && *status != NodeStatus::Running) state.clear();
  return status;
}

auto BehaviorState::run_child(BehaviorNode const& node, std::unique_ptr<NodeState>& child) -> NodeResult {
  if (child) return run_node(node, *child);
  // Synchronous children need only stack storage. Preserve a continuation on suspension or partial failure.
  NodeState local;
  auto status = run_node(node, local);
  if (!local.empty()) child = std::make_unique<NodeState>(std::move(local));
  return status;
}

auto BehaviorState::execute(ActionNode const& node, NodeState& state) -> NodeResult {
  auto board = m_board.lock();
  if (!board) return std::unexpected(BehaviorException("Behavior blackboard has expired"));
  bool starting = state.empty();
  auto finish = [&](std::optional<ActionReturn> const& result, LuaThread& thread) -> NodeResult {
    auto status = result ? std::get<0>(*result) : NodeStatus::Invalid;
    if (starting && status == NodeStatus::Running)
      state.value.emplace<LuaThread>(std::move(thread));
    if (result && std::holds_alternative<LuaTable>(std::get<1>(*result))) {
      auto output = board->set_output(node, std::get<LuaTable>(std::get<1>(*result)));
      if (!output) return std::unexpected(std::move(output.error()));
    }
    if (!starting && (status == NodeStatus::Success || status == NodeStatus::Failure))
      recycle_thread(thread);
    return status;
  };
  if (starting) {
    auto inputs = parameters(*board, node.parameters, node.node_id);
    if (!inputs) return std::unexpected(std::move(inputs.error()));
    auto thread = node_thread(node.name);
    if (!thread) return std::unexpected(std::move(thread.error()));
    auto resumed = thread->resume<ActionReturn>(*inputs, m_board, node.node_id, m_last_dt);
    if (!resumed)
      return std::unexpected(node_error(std::move(resumed.error()), "running action", node.name, m_tree->name));
    return finish(*resumed, *thread);
  }
  auto& thread = std::get<LuaThread>(state.value);
  auto resumed = thread.resume<ActionReturn>(m_last_dt);
  if (!resumed)
    return std::unexpected(node_error(std::move(resumed.error()), "resuming action", node.name, m_tree->name));
  return finish(*resumed, thread);
}

auto BehaviorState::execute(DecoratorNode const& node, NodeState& state) -> NodeResult {
  if (state.empty()) {
    auto board = m_board.lock();
    if (!board) return std::unexpected(BehaviorException("Behavior blackboard has expired"));
    auto inputs = parameters(*board, node.parameters, node.node_id);
    if (!inputs) return std::unexpected(std::move(inputs.error()));
    auto thread = node_thread(node.name);
    if (!thread) return std::unexpected(std::move(thread.error()));
    auto resumed = thread->resume<NodeStatus>(*inputs, m_board, node.node_id);
    if (!resumed)
      return std::unexpected(node_error(std::move(resumed.error()), "initializing decorator", node.name, m_tree->name));
    auto status = resumed->value_or(NodeStatus::Invalid);
    if (status != NodeStatus::Running) {
      if (status == NodeStatus::Invalid) recycle_thread(*thread);
      return status;
    }
    state.value.emplace<DecoratorState>(DecoratorState{std::move(*thread), {}});
  }
  auto& decorator = std::get<DecoratorState>(state.value);
  for (;;) {
    auto child = run_child(*node.child, decorator.child);
    if (!child) return std::unexpected(std::move(child.error()));
    if (*child != NodeStatus::Success && *child != NodeStatus::Failure)
      return NodeStatus::Running;
    auto resumed = decorator.thread.resume<NodeStatus>(*child);
    if (!resumed)
      return std::unexpected(node_error(std::move(resumed.error()), "resuming decorator", node.name, m_tree->name));
    auto status = resumed->value_or(NodeStatus::Invalid);
    if (status != NodeStatus::Running) {
      recycle_thread(decorator.thread);
      return status;
    }
  }
}

auto BehaviorState::execute(CompositeNode const& node, NodeState& state) -> NodeResult {
  return std::visit([&](executable_node auto const& value) { return execute(value, state); }, node);
}

auto BehaviorState::execute(SequenceNode const& node, NodeState& state) -> NodeResult {
  return run_serial(node.children, state, NodeStatus::Failure, NodeStatus::Success);
}

auto BehaviorState::execute(SelectorNode const& node, NodeState& state) -> NodeResult {
  return run_serial(node.children, state, NodeStatus::Success, NodeStatus::Failure);
}

auto BehaviorState::run_serial(List<std::shared_ptr<BehaviorNode const>> const& children,
    NodeState& state, NodeStatus stop, NodeStatus exhausted) -> NodeResult {
  if (state.empty()) state.value.emplace<CursorState>(CursorState{});
  auto& cursor = std::get<CursorState>(state.value);
  while (cursor.index < children.size()) {
    auto status = run_child(*children[cursor.index], cursor.child);
    if (!status || *status == stop || *status == NodeStatus::Running) return status;
    ++cursor.index;
  }
  return exhausted;
}

auto BehaviorState::execute(ParallelNode const& node, NodeState& state) -> NodeResult {
  if (node.children.empty()) return NodeStatus::Running;
  if (state.empty()) state.value.emplace<BranchState>(BranchState{std::make_unique<NodeState[]>(node.children.size())});
  auto& branch = std::get<BranchState>(state.value);
  int failed = 0, succeeded = 0;
  for (std::size_t i = 0; i < node.children.size(); ++i) {
    auto status = run_node(*node.children[i], branch.children[i]);
    if (!status) return std::unexpected(std::move(status.error()));
    if (*status == NodeStatus::Success) ++succeeded;
    else if (*status == NodeStatus::Failure) ++failed;
    if (succeeded >= node.succeed) return NodeStatus::Success;
    if (failed >= node.fail) return NodeStatus::Failure;
  }
  return NodeStatus::Running;
}

auto BehaviorState::execute(DynamicNode const& node, NodeState& state) -> NodeResult {
  if (node.children.empty()) return std::unexpected(BehaviorException("Dynamic behavior node has no children"));
  if (state.empty()) state.value.emplace<BranchState>(BranchState{std::make_unique<NodeState[]>(node.children.size())});
  auto& branch = std::get<BranchState>(state.value);
  for (std::size_t i = 0; i <= branch.index; ++i) {
    auto status = run_node(*node.children[i], branch.children[i]);
    if (!status) return std::unexpected(std::move(status.error()));
    if (i == branch.index && *status == NodeStatus::Failure) {
      if (++branch.index == node.children.size()) return NodeStatus::Failure;
    } else if (i < branch.index && (*status == NodeStatus::Success || *status == NodeStatus::Running)) {
      branch.children[branch.index].clear();
      branch.index = i;
    }
    if (*status == NodeStatus::Success) return NodeStatus::Success;
  }
  return NodeStatus::Running;
}

auto BehaviorState::execute(RandomizeNode const& node, NodeState& state) -> NodeResult {
  if (state.empty()) {
    if (node.children.empty()) return std::unexpected(BehaviorException("Randomize behavior node has no children"));
    auto root = lua_execution_root(m_lua_context.engine());
    if (!root) return std::unexpected(std::move(root.error()));
    auto index = root->get().random().uniform_uint(node.children.size() - 1);
    state.value.emplace<CursorState>(CursorState{static_cast<std::size_t>(index), {}});
  }
  auto& cursor = std::get<CursorState>(state.value);
  return run_child(*node.children[cursor.index], cursor.child);
}
}
