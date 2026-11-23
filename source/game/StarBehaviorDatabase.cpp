import std;
import star.json;
import star.game_types;
import star.bi_map;
import star.list;
import star.string;
import star.format;
import star.assets;
import star.behavior_database;
namespace Star {
namespace {

// Only borrows from the caller's immutable JSON; missing/null objects have the
// same empty default as get_object(key, {}), without copying present objects.
std::expected<std::reference_wrapper<JsonObject const>, StarException> object_field(Json const& json, String const& key) {
  auto field = json.find(key);
  if (!field) return std::unexpected(std::move(field.error()));
  if (!*field || (*field)->get().is_null()) {
    static JsonObject const empty;
    return std::cref(empty);
  }
  auto object = (*field)->get().object_ptr();
  if (!object) return std::unexpected(std::move(object.error()));
  return std::cref(**object);
}

using TagBinding = std::optional<std::reference_wrapper<NodeParameterValue const>>;

std::expected<TagBinding, StarException> find_tag(String const& text, StringMap<NodeParameterValue> const& parameters) {
  auto begins = text.starts_with('<');
  if (!begins) return std::unexpected(std::move(begins.error()));
  if (!*begins) return TagBinding{};
  auto ends = text.ends_with('>');
  if (!ends) return std::unexpected(std::move(ends.error()));
  if (!*ends) return TagBinding{};
  auto size = text.size();
  if (!size) return std::unexpected(std::move(size.error()));
  auto key = text.substr(1, *size - 2);
  if (!key) return std::unexpected(std::move(key.error()));
  if (auto replacement = parameters.find_ref(*key)) return replacement;
  return std::unexpected(StarException(Star::format("No parameter specified for tag '{}'", text)));
}

std::expected<TagBinding, StarException> find_tag(NodeParameterValue const& parameter, StringMap<NodeParameterValue> const& parameters) {
  if (std::holds_alternative<String>(parameter))
    return find_tag(std::get<String>(parameter), parameters);
  auto const& value = std::get<Json>(parameter);
  if (!value.is_type(Json::Type::String)) return TagBinding{};
  auto text = value.string_ptr();
  if (!text) return std::unexpected(std::move(text.error()));
  return find_tag(**text, parameters);
}

// Different trees (and database generations) can share a Lua blackboard.
// Neither allocator addresses nor tree-local indices identify its bindings.
std::uint64_t next_node_id() {
  static std::atomic<std::uint64_t> next{1};
  auto id = next.fetch_add(1, std::memory_order_relaxed);
  if (id > std::numeric_limits<std::int64_t>::max()) std::terminate();
  return id;
}

}

std::expected<NodeParameterValue, StarException> node_parameter_value_from_json(Json const& json) {
  auto key = json.opt_string("key");
  if (!key) return std::unexpected(std::move(key.error()));
  if (*key) return NodeParameterValue(std::move(**key));
  auto value = json.get("value");
  if (!value) return std::unexpected(std::move(value.error()));
  return NodeParameterValue(std::move(*value));
}

std::expected<Json, StarException> json_from_node_parameter(NodeParameter const& parameter) {
  auto type = enum_name<node_parameter_type_names>(parameter.first);
  if (!type) return std::unexpected(std::move(type.error()));
  JsonObject result{{"type", *type}};
  if (std::holds_alternative<String>(parameter.second)) result["key"] = std::get<String>(parameter.second);
  else result["value"] = std::get<Json>(parameter.second);
  return Json(std::move(result));
}

std::expected<NodeParameter, StarException> json_to_node_parameter(Json const& json) {
  auto name = json.get_string("type");
  if (!name) return std::unexpected(std::move(name.error()));
  auto type = enum_value<node_parameter_type_names>(*name);
  if (!type) return std::unexpected(std::move(type.error()));
  auto key = json.opt_string("key");
  if (!key) return std::unexpected(std::move(key.error()));
  if (*key)
    return NodeParameter{*type, std::move(**key)};
  auto value = json.opt("value");
  if (!value) return std::unexpected(std::move(value.error()));
  return NodeParameter{*type, std::move(*value).value_or(Json())};
}

std::expected<Json, StarException> json_from_node_output(NodeOutput const& output) {
  auto type = enum_name<node_parameter_type_names>(output.type);
  if (!type) return std::unexpected(std::move(type.error()));
  return JsonObject{{"type", *type},
                    {"key", output.key ? Json(*output.key) : Json()},
                    {"ephemeral", output.ephemeral}};
}

std::expected<NodeOutput, StarException> json_to_node_output(Json const& json) {
  auto name = json.get_string("type");
  if (!name) return std::unexpected(std::move(name.error()));
  auto type = enum_value<node_parameter_type_names>(*name);
  if (!type) return std::unexpected(std::move(type.error()));
  auto key_json = json.get("key");
  if (!key_json) return std::unexpected(std::move(key_json.error()));
  auto key = key_json->opt_string();
  if (!key) return std::unexpected(std::move(key.error()));
  auto ephemeral = json.opt_bool("ephemeral");
  if (!ephemeral) return std::unexpected(std::move(ephemeral.error()));
  return NodeOutput{*type, std::move(*key), ephemeral->value_or(false)};
}

std::expected<void, StarException> apply_tree_parameters(StringMap<NodeParameter>& parameters, StringMap<NodeParameterValue> const& tree_parameters) {
  for (auto&& p : parameters) {
    auto binding = find_tag(p.second.second, tree_parameters);
    if (!binding) return std::unexpected(std::move(binding.error()));
    if (*binding) p.second.second = (*binding)->get();
  }
  return {};
}

std::expected<NodeParameterValue, StarException> replace_behavior_tag(NodeParameterValue const& parameter, StringMap<NodeParameterValue> const& tree_parameters) {
  auto binding = find_tag(parameter, tree_parameters);
  if (!binding) return std::unexpected(std::move(binding.error()));
  return *binding ? (*binding)->get() : parameter;
}

std::expected<std::optional<String>, StarException> replace_output_behavior_tag(std::optional<String> output, StringMap<NodeParameterValue> const& parameters) {
  if (!output) return output;
  auto binding = find_tag(*output, parameters);
  if (!binding) return std::unexpected(std::move(binding.error()));
  if (!*binding) return output;
  auto const& replacement = (*binding)->get();
  if (std::holds_alternative<String>(replacement)) return std::optional<String>(std::get<String>(replacement));
  auto const& value = std::get<Json>(replacement);
  if (value.is_type(Json::Type::String)) {
    auto string = value.to_string();
    if (!string) return std::unexpected(std::move(string.error()));
    return std::optional<String>(std::move(*string));
  }
  return std::optional<String>();
}

ActionNode::ActionNode(String name, StringMap<NodeParameter> parameters, StringMap<NodeOutput> output)
  : name(std::move(name)), parameters(std::move(parameters)), output(std::move(output)), node_id(next_node_id()) {}

DecoratorNode::DecoratorNode(String name, StringMap<NodeParameter> parameters, std::shared_ptr<BehaviorNode const> child)
  : name(std::move(name)), parameters(std::move(parameters)), child(std::move(child)), node_id(next_node_id()) {}

BehaviorDatabase::BehaviorDatabase(ConstructionKey, std::shared_ptr<Assets const> assets) : m_assets(std::move(assets)) {}

std::expected<std::shared_ptr<BehaviorDatabase>, StarException> BehaviorDatabase::create(std::shared_ptr<Assets const> assets) {
  auto database = std::make_shared<BehaviorDatabase>(ConstructionKey{}, assets);
  auto files = assets->scan_extension("nodes");
  if (!files) return std::unexpected(std::move(files.error()));
  auto queued_nodes = assets->queue_jsons(files->get());
  if (!queued_nodes) return std::unexpected(std::move(queued_nodes.error()));
  for (auto const& file_key : files->get()) {
    auto const& file = file_key.string();
    auto load = [&]() -> std::expected<void, StarException> {
      auto nodes = assets->json(file);
      if (!nodes) return std::unexpected(std::move(nodes.error()));
      auto object = nodes->iterate_object();
      if (!object) return std::unexpected(std::move(object.error()));
      for (auto const& node : *object) {
        auto properties = object_field(node.second, "properties");
        if (!properties) return std::unexpected(std::move(properties.error()));
        NodeDefinition definition;
        definition.parameters.reserve(properties->get().size());
        for (auto const& p : properties->get()) {
          auto parameter = json_to_node_parameter(p.second);
          if (!parameter) return std::unexpected(std::move(parameter.error()));
          definition.parameters.set(p.first, std::move(*parameter));
        }
        auto outputs = object_field(node.second, "output");
        if (!outputs) return std::unexpected(std::move(outputs.error()));
        definition.output.reserve(outputs->get().size());
        for (auto const& p : outputs->get()) {
          auto value = json_to_node_output(p.second);
          if (!value) return std::unexpected(std::move(value.error()));
          definition.output.set(p.first, std::move(*value));
        }
        database->m_nodes.set(node.first, std::move(definition));
      }
      return {};
    }();
    if (!load) return std::unexpected(StarException(Star::format("Could not load nodes file '{}'", file), assets->with_asset_context(std::move(load.error()), file, "Loading behavior nodes")));
  }
  auto behaviors = assets->scan_extension("behavior");
  if (!behaviors) return std::unexpected(std::move(behaviors.error()));
  auto queued_behaviors = assets->queue_jsons(behaviors->get());
  if (!queued_behaviors) return std::unexpected(std::move(queued_behaviors.error()));
  for (auto const& file_key : behaviors->get()) {
    auto const& file = file_key.string();
    auto load = [&]() -> std::expected<void, StarException> {
      auto config = assets->json(file);
      if (!config) return std::unexpected(std::move(config.error()));
      auto name = config->get_string("name");
      if (!name) return std::unexpected(std::move(name.error()));
      if (!database->m_behaviors.try_emplace(std::move(*name), std::move(*config), file, std::shared_ptr<BehaviorTree const>{}).second)
        return std::unexpected(StarException(Star::format("Duplicate behavior tree '{}'", *name)));
      return {};
    }();
    if (!load) return std::unexpected(StarException(Star::format("Could not load behavior file '{}'", file), assets->with_asset_context(std::move(load.error()), file, "Loading behavior configuration")));
  }
  for (auto&& [name, definition] : database->m_behaviors) {
    auto tree = database->compile_tree(name, definition.config, {}, {name.utf8()});
    if (!tree)
      return std::unexpected(assets->with_asset_context(std::move(tree.error()), definition.path, "Building behavior tree"));
    definition.tree = std::move(*tree);
  }
  return database;
}

std::expected<std::shared_ptr<BehaviorTree const>, StarException> BehaviorDatabase::behavior_tree(String const& name) const {
  if (auto entry = m_behaviors.find_ref(name)) return entry->get().tree;
  return std::unexpected(StarException(Star::format("No such behavior tree '{}'", name)));
}

std::expected<Json, StarException> BehaviorDatabase::behavior_config(String const& name) const {
  if (auto entry = m_behaviors.find_ref(name)) return entry->get().config;
  return std::unexpected(StarException(Star::format("No such behavior tree '{}'", name)));
}

std::expected<std::shared_ptr<BehaviorTree const>, StarException> BehaviorDatabase::build_tree(Json const& config, StringMap<NodeParameterValue> const& overrides) const {
  auto name = config.get_string("name");
  if (!name) return std::unexpected(std::move(name.error()));
  // An inline tree may have the same display name as a distinct registered module.
  return compile_tree(std::move(*name), config, overrides, {});
}

std::expected<std::shared_ptr<BehaviorTree const>, StarException> BehaviorDatabase::compile_tree(String name, Json const& config, StringMap<NodeParameterValue> const& overrides, List<std::string_view> modules) const {
  auto tree = std::make_shared<BehaviorTree>();
  tree->name = std::move(name);
  Expansion expansion{tree->scripts, tree->functions, modules};
  auto root = expand_tree(config, overrides, expansion);
  if (!root) return std::unexpected(std::move(root.error()));
  tree->root = std::move(*root);
  return tree;
}

std::expected<std::shared_ptr<BehaviorNode const>, StarException> BehaviorDatabase::expand_tree(Json const& config, StringMap<NodeParameterValue> parameters, Expansion& expansion) const {
  auto scripts = config.find("scripts");
  if (!scripts) return std::unexpected(std::move(scripts.error()));
  if (*scripts) {
    auto entries = (*scripts)->get().iterate_array();
    if (!entries) return std::unexpected(std::move(entries.error()));
    for (auto const& entry : *entries) {
      auto script = entry.to_string();
      if (!script) return std::unexpected(std::move(script.error()));
      expansion.scripts.add(std::move(*script));
    }
  }
  auto properties = object_field(config, "parameters");
  if (!properties) return std::unexpected(std::move(properties.error()));
  for (auto const& [key, value] : properties->get()) parameters.try_emplace(key, value);
  auto root = config.get("root");
  if (!root) return std::unexpected(std::move(root.error()));
  return behavior_node(*root, parameters, expansion);
}

std::expected<CompositeNode, StarException> BehaviorDatabase::composite_node(String const& name, Json const& config, StringMap<NodeParameter> const& parameters, StringMap<NodeParameterValue> const& tree_parameters, Expansion& expansion) const {
  auto array = config.opt("children");
  if (!array) return std::unexpected(std::move(array.error()));
  List<std::shared_ptr<BehaviorNode const>> children;
  if (*array) {
    auto entries = (**array).array_ptr();
    if (!entries) return std::unexpected(std::move(entries.error()));
    children.reserve((*entries)->size());
    for (auto const& child : **entries) {
      auto node = behavior_node(child, tree_parameters, expansion);
      if (!node) return std::unexpected(std::move(node.error()));
      children.push_back(std::move(*node));
    }
  }
  auto type = enum_value<composite_type_names>(name);
  if (!type) return std::unexpected(std::move(type.error()));
  switch (*type) {
  case CompositeType::Sequence: return CompositeNode(SequenceNode(std::move(children)));
  case CompositeType::Selector: return CompositeNode(SelectorNode(std::move(children)));
  case CompositeType::Dynamic:
  case CompositeType::Randomize:
    if (children.empty())
      return std::unexpected(StarException(Star::format("{} behavior node requires at least one child", name)));
    if (*type == CompositeType::Dynamic) return CompositeNode(DynamicNode{std::move(children)});
    return CompositeNode(RandomizeNode{std::move(children)});
  case CompositeType::Parallel: {
    auto success = parameters.get("success");
    if (!success)
      return std::unexpected(std::move(success.error()));
    auto failure = parameters.get("fail");
    if (!failure)
      return std::unexpected(std::move(failure.error()));
    auto const& success_value = success->get().second;
    auto const& failure_value = failure->get().second;
    if (!std::holds_alternative<Json>(success_value) || !std::holds_alternative<Json>(failure_value))
      return std::unexpected(StarException("Parallel thresholds must be JSON values"));
    auto s = std::get<Json>(success_value).opt_int();
    if (!s)
      return std::unexpected(std::move(s.error()));
    auto f = std::get<Json>(failure_value).opt_int();
    if (!f)
      return std::unexpected(std::move(f.error()));
    int succeed = s->value_or(-1), fail = f->value_or(-1);
    if (succeed == -1)
      succeed = children.size();
    if (fail == -1)
      fail = children.size();
    return CompositeNode(ParallelNode(succeed, fail, std::move(children)));
  }
  }
  return std::unexpected(StarException(Star::format("Composite node type '{}' could not be created from JSON", name)));
}

std::expected<std::shared_ptr<BehaviorNode const>, StarException> BehaviorDatabase::behavior_node(Json const& json, StringMap<NodeParameterValue> const& tree_parameters, Expansion& expansion) const {
  auto type_name = json.get_string("type");
  if (!type_name) return std::unexpected(std::move(type_name.error()));
  auto type = enum_value<behavior_node_type_names>(*type_name);
  if (!type) return std::unexpected(std::move(type.error()));
  auto name = json.get_string("name");
  if (!name) return std::unexpected(std::move(name.error()));
  auto config = object_field(json, "parameters");
  if (!config) return std::unexpected(std::move(config.error()));
  if (*type == BehaviorNodeType::Module) {
    auto module = m_behaviors.find_ref(*name);
    if (!module) return std::unexpected(StarException(Star::format("No such behavior tree '{}'", *name)));
    auto const& definition = module->get();
    std::string_view module_name = name->utf8();
    if (std::ranges::find(expansion.modules, module_name) != expansion.modules.end()) {
      std::string chain;
      for (auto step : expansion.modules) chain.append(step).append(" -> ");
      chain.append(module_name);
      return std::unexpected(m_assets->with_asset_context(StarException(Star::format("Behavior module cycle: {}", chain)), definition.path, "Expanding behavior module"));
    }
    auto parameters = tree_parameters;
    for (auto const& p : config->get()) {
      auto value = node_parameter_value_from_json(p.second);
      if (!value) return std::unexpected(std::move(value.error()));
      auto binding = find_tag(*value, tree_parameters);
      if (!binding) return std::unexpected(std::move(binding.error()));
      if (*binding) parameters.set(p.first, (*binding)->get());
      else parameters.set(p.first, std::move(*value));
    }
    // Merge in module-set iteration order as before: Lua require order can
    // matter if two scripts define the same global function.
    StringSet scripts, functions;
    Expansion nested{scripts, functions, expansion.modules};
    expansion.modules.push_back(module_name);
    auto root = expand_tree(definition.config, std::move(parameters), nested);
    expansion.modules.pop_back();
    if (!root)
      return std::unexpected(m_assets->with_asset_context(std::move(root.error()), definition.path, "Building behavior tree"));
    expansion.scripts.insert_range(scripts);
    expansion.functions.insert_range(functions);
    return root;
  }
  auto defaults = m_nodes.get(*name);
  if (!defaults) return std::unexpected(std::move(defaults.error()));
  auto parameters = defaults->get().parameters;
  for (auto&& p : parameters) {
    if (auto configured = config->get().find_ref(p.first)) {
      auto value = node_parameter_value_from_json(configured->get());
      if (!value) return std::unexpected(std::move(value.error()));
      p.second.second = std::move(*value);
    }
  }
  auto applied = apply_tree_parameters(parameters, tree_parameters);
  if (!applied) return std::unexpected(std::move(applied.error()));
  if (*type == BehaviorNodeType::Action) {
    expansion.functions.add(*name);
    auto outputs = object_field(json, "output");
    if (!outputs) return std::unexpected(std::move(outputs.error()));
    auto output = defaults->get().output;
    for (auto&& [key, binding] : output) {
      if (auto configured = outputs->get().find_ref(key)) {
        auto override_key = configured->get().opt_string();
        if (!override_key) return std::unexpected(std::move(override_key.error()));
        if (*override_key) binding.key = std::move(*override_key);
      }
      auto replacement = replace_output_behavior_tag(std::move(binding.key), tree_parameters);
      if (!replacement) return std::unexpected(std::move(replacement.error()));
      binding.key = std::move(*replacement);
    }
    return std::make_shared<BehaviorNode>(std::in_place_type<ActionNode>, std::move(*name), std::move(parameters), std::move(output));
  }
  if (*type == BehaviorNodeType::Decorator) {
    expansion.functions.add(*name);
    auto child_json = json.get("child");
    if (!child_json) return std::unexpected(std::move(child_json.error()));
    auto child = behavior_node(*child_json, tree_parameters, expansion);
    if (!child) return std::unexpected(std::move(child.error()));
    return std::make_shared<BehaviorNode>(std::in_place_type<DecoratorNode>, std::move(*name), std::move(parameters), std::move(*child));
  }
  if (*type == BehaviorNodeType::Composite) {
    auto composite = composite_node(*name, json, parameters, tree_parameters, expansion);
    if (!composite) return std::unexpected(std::move(composite.error()));
    return std::make_shared<BehaviorNode>(std::move(*composite));
  }
  return std::unexpected(StarException(Star::format("Behavior node type '{}' could not be created from JSON", *type_name)));
}
}
