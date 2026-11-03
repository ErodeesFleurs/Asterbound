import std;
import star.json;
import star.json_extra;
import star.game_types;
import star.bi_map;
import star.list;
import star.string;
import star.format;
import star.assets;
import star.behavior_database;
namespace Star {
extern std::expected<EnumMap<NodeParameterType>, StarException> const NodeParameterTypeNames = EnumMap<NodeParameterType>::create({
  {NodeParameterType::Json, "json"}, {NodeParameterType::Entity, "entity"},
  {NodeParameterType::Position, "position"}, {NodeParameterType::Vec2, "vec2"},
  {NodeParameterType::Number, "number"}, {NodeParameterType::Bool, "bool"},
  {NodeParameterType::List, "list"}, {NodeParameterType::Table, "table"}, {NodeParameterType::String, "string"}
});
extern std::expected<EnumMap<BehaviorNodeType>, StarException> const BehaviorNodeTypeNames = EnumMap<BehaviorNodeType>::create({
  {BehaviorNodeType::Action, "Action"}, {BehaviorNodeType::Decorator, "Decorator"},
  {BehaviorNodeType::Composite, "Composite"}, {BehaviorNodeType::Module, "Module"}
});
extern std::expected<EnumMap<CompositeType>, StarException> const CompositeTypeNames = EnumMap<CompositeType>::create({
  {CompositeType::Sequence, "Sequence"}, {CompositeType::Selector, "Selector"},
  {CompositeType::Parallel, "Parallel"}, {CompositeType::Dynamic, "Dynamic"}, {CompositeType::Randomize, "Randomize"}
});
std::expected<NodeParameterValue, StarException> nodeParameterValueFromJson(Json const& json) {
  auto key = json.optString("key");
  if (!key) return std::unexpected(std::move(key.error()));
  if (*key) return NodeParameterValue(**key);
  auto value = json.get("value");
  if (!value) return std::unexpected(std::move(value.error()));
  return NodeParameterValue(*value);
}
std::expected<Json, StarException> jsonFromNodeParameter(NodeParameter const& parameter) {
  if (!NodeParameterTypeNames) return std::unexpected(NodeParameterTypeNames.error());
  auto type = NodeParameterTypeNames->getRight(parameter.first);
  if (!type) return std::unexpected(std::move(type.error()));
  JsonObject result{{"type", type->get()}};
  if (std::holds_alternative<String>(parameter.second)) result["key"] = std::get<String>(parameter.second);
  else result["value"] = std::get<Json>(parameter.second);
  return Json(std::move(result));
}
std::expected<NodeParameter, StarException> jsonToNodeParameter(Json const& json) {
  if (!NodeParameterTypeNames) return std::unexpected(NodeParameterTypeNames.error());
  auto name = json.getString("type");
  if (!name) return std::unexpected(std::move(name.error()));
  auto type = NodeParameterTypeNames->getLeft(*name);
  if (!type) return std::unexpected(std::move(type.error()));
  auto key = json.optString("key");
  if (!key) return std::unexpected(std::move(key.error()));
  if (*key) return NodeParameter{type->get(), **key};
  auto value = json.opt("value");
  if (!value) return std::unexpected(std::move(value.error()));
  return NodeParameter{type->get(), value->value_or(Json())};
}
std::expected<Json, StarException> jsonFromNodeOutput(NodeOutput const& output) {
  if (!NodeParameterTypeNames) return std::unexpected(NodeParameterTypeNames.error());
  auto type = NodeParameterTypeNames->getRight(output.first);
  if (!type) return std::unexpected(std::move(type.error()));
  return JsonObject{{"type", type->get()}, {"key", output.second.first ? Json(*output.second.first) : Json()}, {"ephemeral", output.second.second}};
}
std::expected<NodeOutput, StarException> jsonToNodeOutput(Json const& json) {
  if (!NodeParameterTypeNames) return std::unexpected(NodeParameterTypeNames.error());
  auto name = json.getString("type");
  if (!name) return std::unexpected(std::move(name.error()));
  auto type = NodeParameterTypeNames->getLeft(*name);
  if (!type) return std::unexpected(std::move(type.error()));
  auto keyJson = json.get("key");
  if (!keyJson) return std::unexpected(std::move(keyJson.error()));
  auto key = keyJson->optString();
  if (!key) return std::unexpected(std::move(key.error()));
  auto ephemeral = json.optBool("ephemeral");
  if (!ephemeral) return std::unexpected(std::move(ephemeral.error()));
  return NodeOutput{type->get(), {*key, ephemeral->value_or(false)}};
}
std::expected<void, StarException> applyTreeParameters(StringMap<NodeParameter>& parameters, StringMap<NodeParameterValue> const& treeParameters) {
  for (auto& p : parameters) {
    auto value = replaceBehaviorTag(p.second.second, treeParameters);
    if (!value) return std::unexpected(std::move(value.error()));
    p.second.second = std::move(*value);
  }
  return {};
}
std::expected<NodeParameterValue, StarException> replaceBehaviorTag(NodeParameterValue const& parameter, StringMap<NodeParameterValue> const& treeParameters) {
  std::optional<String> text;
  if (std::holds_alternative<String>(parameter)) text = std::get<String>(parameter);
  else if (std::get<Json>(parameter).isType(Json::Type::String)) {
    auto string = std::get<Json>(parameter).toString();
    if (!string) return std::unexpected(std::move(string.error()));
    text = *string;
  }
  if (text) {
    auto begins = text->beginsWith('<');
    if (!begins) return std::unexpected(std::move(begins.error()));
    if (*begins) {
      auto ends = text->endsWith('>');
      if (!ends) return std::unexpected(std::move(ends.error()));
      if (*ends) {
        auto size = text->size();
        if (!size) return std::unexpected(std::move(size.error()));
        auto key = text->substr(1, *size - 2);
        if (!key) return std::unexpected(std::move(key.error()));
        if (auto replacement = treeParameters.maybe(*key)) return *replacement;
        return std::unexpected(StarException(strf("No parameter specified for tag '{}'", *text)));
      }
    }
  }
  return parameter;
}
std::expected<std::optional<String>, StarException> replaceOutputBehaviorTag(std::optional<String> const& output, StringMap<NodeParameterValue> const& parameters) {
  if (!output) return output;
  auto replacement = replaceBehaviorTag(NodeParameterValue(*output), parameters);
  if (!replacement) return std::unexpected(std::move(replacement.error()));
  if (std::holds_alternative<String>(*replacement)) return std::optional<String>(std::get<String>(*replacement));
  auto const& value = std::get<Json>(*replacement);
  if (value.isType(Json::Type::String)) {
    auto string = value.toString();
    if (!string) return std::unexpected(std::move(string.error()));
    return std::optional<String>(*string);
  }
  return std::optional<String>();
}
ActionNode::ActionNode(String name, StringMap<NodeParameter> parameters, StringMap<NodeOutput> output)
  : name(std::move(name)), parameters(std::move(parameters)), output(std::move(output)) {}
DecoratorNode::DecoratorNode(String const& name, StringMap<NodeParameter> parameters, std::shared_ptr<BehaviorNode const> child)
  : name(name), parameters(std::move(parameters)), child(std::move(child)) {}
SequenceNode::SequenceNode(List<std::shared_ptr<BehaviorNode const>> children) : children(std::move(children)) {}
SelectorNode::SelectorNode(List<std::shared_ptr<BehaviorNode const>> children) : children(std::move(children)) {}
ParallelNode::ParallelNode(int succeed, int fail, List<std::shared_ptr<BehaviorNode const>> children)
  : succeed(succeed), fail(fail), children(std::move(children)) {}
DynamicNode::DynamicNode(List<std::shared_ptr<BehaviorNode const>> children) : children(std::move(children)) {}
RandomizeNode::RandomizeNode(List<std::shared_ptr<BehaviorNode const>> children) : children(std::move(children)) {}
BehaviorTree::BehaviorTree(String const& name, StringSet scripts, JsonObject const& parameters)
  : name(name), scripts(std::move(scripts)), parameters(parameters) {}

std::expected<std::shared_ptr<BehaviorDatabase>, StarException> BehaviorDatabase::create(std::shared_ptr<Assets const> assets) {
  auto database = std::shared_ptr<BehaviorDatabase>(new BehaviorDatabase());
  database->m_assets = assets;
  auto files = assets->scanExtension("nodes");
  if (!files) return std::unexpected(std::move(files.error()));
  auto queuedNodes = assets->queueJsons(files->get());
  if (!queuedNodes) return std::unexpected(std::move(queuedNodes.error()));
  for (auto const& fileKey : files->get()) {
    auto const& file = fileKey.string();
    auto load = [&]() -> std::expected<void, StarException> {
      auto nodes = assets->json(file);
      if (!nodes) return std::unexpected(std::move(nodes.error()));
      auto object = nodes->toObject();
      if (!object) return std::unexpected(std::move(object.error()));
      for (auto const& node : *object) {
        auto properties = node.second.getObject("properties", {});
        if (!properties) return std::unexpected(std::move(properties.error()));
        StringMap<NodeParameter> parameters;
        for (auto const& p : *properties) {
          auto parameter = jsonToNodeParameter(p.second);
          if (!parameter) return std::unexpected(std::move(parameter.error()));
          parameters.set(p.first, std::move(*parameter));
        }
        database->m_nodeParameters.set(node.first, std::move(parameters));
        auto outputs = node.second.getObject("output", {});
        if (!outputs) return std::unexpected(std::move(outputs.error()));
        StringMap<NodeOutput> output;
        for (auto const& p : *outputs) {
          auto value = jsonToNodeOutput(p.second);
          if (!value) return std::unexpected(std::move(value.error()));
          output.set(p.first, std::move(*value));
        }
        database->m_nodeOutput.set(node.first, std::move(output));
      }
      return {};
    }();
    if (!load) return std::unexpected(StarException(strf("Could not load nodes file '{}'", file), assets->withAssetContext(std::move(load.error()), file, "Loading behavior nodes")));
  }
  auto behaviors = assets->scanExtension("behavior");
  if (!behaviors) return std::unexpected(std::move(behaviors.error()));
  auto queuedBehaviors = assets->queueJsons(behaviors->get());
  if (!queuedBehaviors) return std::unexpected(std::move(queuedBehaviors.error()));
  for (auto const& fileKey : behaviors->get()) {
    auto const& file = fileKey.string();
    auto load = [&]() -> std::expected<void, StarException> {
      auto config = assets->json(file);
      if (!config) return std::unexpected(std::move(config.error()));
      auto name = config->getString("name");
      if (!name) return std::unexpected(std::move(name.error()));
      if (database->m_configs.contains(*name)) return std::unexpected(StarException(strf("Duplicate behavior tree '{}'", *name)));
      database->m_configs[*name] = std::move(*config);
      database->m_configPaths[*name] = file;
      return {};
    }();
    if (!load) return std::unexpected(StarException(strf("Could not load behavior file '{}'", file), assets->withAssetContext(std::move(load.error()), file, "Loading behavior configuration")));
  }
  for (auto const& pair : database->m_configs) {
    if (!database->m_behaviors.contains(pair.first)) {
      auto loaded = database->loadTree(pair.first);
      if (!loaded) return std::unexpected(std::move(loaded.error()));
    }
  }
  return database;
}
std::expected<std::shared_ptr<BehaviorTree const>, StarException> BehaviorDatabase::behaviorTree(String const& name) const {
  if (auto tree = m_behaviors.maybe(name)) return *tree;
  return std::unexpected(StarException(strf("No such behavior tree '{}'", name)));
}
std::expected<Json, StarException> BehaviorDatabase::behaviorConfig(String const& name) const {
  if (auto config = m_configs.maybe(name)) return *config;
  return std::unexpected(StarException(strf("No such behavior tree '{}'", name)));
}
std::expected<void, StarException> BehaviorDatabase::loadTree(String const& name) {
  auto config = behaviorConfig(name);
  if (!config) return std::unexpected(std::move(config.error()));
  auto tree = buildTree(*config, {});
  if (!tree) {
    if (auto path = m_configPaths.maybeRef(name))
      return std::unexpected(m_assets->withAssetContext(std::move(tree.error()), path->get(), "Building behavior tree"));
    return std::unexpected(std::move(tree.error()));
  }
  m_behaviors.set(name, *tree);
  return {};
}
std::expected<std::shared_ptr<BehaviorTree const>, StarException> BehaviorDatabase::buildTree(Json const& config, StringMap<NodeParameterValue> const& overrides) const {
  auto scriptsJson = config.get("scripts", JsonArray());
  if (!scriptsJson) return std::unexpected(std::move(scriptsJson.error()));
  auto scripts = jsonToStringSet(*scriptsJson);
  if (!scripts) return std::unexpected(std::move(scripts.error()));
  auto name = config.getString("name");
  if (!name) return std::unexpected(std::move(name.error()));
  auto properties = config.getObject("parameters", {});
  if (!properties) return std::unexpected(std::move(properties.error()));
  auto tree = std::make_shared<BehaviorTree>(*name, std::move(*scripts), *properties);
  StringMap<NodeParameterValue> parameters;
  for (auto const& p : *properties) parameters.set(p.first, p.second);
  for (auto const& p : overrides) parameters.set(p.first, p.second);
  auto rootJson = config.get("root");
  if (!rootJson) return std::unexpected(std::move(rootJson.error()));
  auto root = behaviorNode(*rootJson, parameters, *tree);
  if (!root) return std::unexpected(std::move(root.error()));
  tree->root = *root;
  return tree;
}
std::expected<CompositeNode, StarException> BehaviorDatabase::compositeNode(Json const& config, StringMap<NodeParameter> parameters, StringMap<NodeParameterValue> const& treeParameters, BehaviorTree& tree) const {
  auto array = config.getArray("children", {});
  if (!array) return std::unexpected(std::move(array.error()));
  List<std::shared_ptr<BehaviorNode const>> children;
  for (auto const& child : *array) {
    auto node = behaviorNode(child, treeParameters, tree);
    if (!node) return std::unexpected(std::move(node.error()));
    children.append(*node);
  }
  if (!CompositeTypeNames) return std::unexpected(CompositeTypeNames.error());
  auto name = config.getString("name");
  if (!name) return std::unexpected(std::move(name.error()));
  auto type = CompositeTypeNames->getLeft(*name);
  if (!type) return std::unexpected(std::move(type.error()));
  switch (type->get()) {
    case CompositeType::Sequence: return CompositeNode(SequenceNode(std::move(children)));
    case CompositeType::Selector: return CompositeNode(SelectorNode(std::move(children)));
    case CompositeType::Dynamic: return CompositeNode(DynamicNode(std::move(children)));
    case CompositeType::Randomize: return CompositeNode(RandomizeNode(std::move(children)));
    case CompositeType::Parallel: {
      auto success = parameters.get("success");
      if (!success) return std::unexpected(std::move(success.error()));
      auto failure = parameters.get("fail");
      if (!failure) return std::unexpected(std::move(failure.error()));
      auto const& successValue = success->get().second;
      auto const& failureValue = failure->get().second;
      if (!std::holds_alternative<Json>(successValue) || !std::holds_alternative<Json>(failureValue))
        return std::unexpected(StarException("Parallel thresholds must be JSON values"));
      auto s = std::get<Json>(successValue).optInt();
      if (!s) return std::unexpected(std::move(s.error()));
      auto f = std::get<Json>(failureValue).optInt();
      if (!f) return std::unexpected(std::move(f.error()));
      int succeed = s->value_or(-1), fail = f->value_or(-1);
      if (succeed == -1) succeed = children.size();
      if (fail == -1) fail = children.size();
      return CompositeNode(ParallelNode(succeed, fail, std::move(children)));
    }
  }
  return std::unexpected(StarException(strf("Composite node type '{}' could not be created from JSON", *name)));
}
std::expected<std::shared_ptr<BehaviorNode const>, StarException> BehaviorDatabase::behaviorNode(Json const& json, StringMap<NodeParameterValue> const& treeParameters, BehaviorTree& tree) const {
  if (!BehaviorNodeTypeNames) return std::unexpected(BehaviorNodeTypeNames.error());
  auto typeName = json.getString("type");
  if (!typeName) return std::unexpected(std::move(typeName.error()));
  auto type = BehaviorNodeTypeNames->getLeft(*typeName);
  if (!type) return std::unexpected(std::move(type.error()));
  auto name = json.getString("name");
  if (!name) return std::unexpected(std::move(name.error()));
  auto config = json.getObject("parameters", {});
  if (!config) return std::unexpected(std::move(config.error()));
  if (type->get() == BehaviorNodeType::Module) {
    auto parameters = treeParameters;
    for (auto const& p : *config) {
      auto value = nodeParameterValueFromJson(p.second);
      if (!value) return std::unexpected(std::move(value.error()));
      auto replacement = replaceBehaviorTag(*value, treeParameters);
      if (!replacement) return std::unexpected(std::move(replacement.error()));
      parameters.set(p.first, std::move(*replacement));
    }
    auto moduleConfig = behaviorConfig(*name);
    if (!moduleConfig) return std::unexpected(std::move(moduleConfig.error()));
    auto module = buildTree(*moduleConfig, parameters);
    if (!module) {
      if (auto path = m_configPaths.maybeRef(*name))
        return std::unexpected(m_assets->withAssetContext(std::move(module.error()), path->get(), "Building behavior tree"));
      return std::unexpected(std::move(module.error()));
    }
    tree.scripts.addAll((*module)->scripts);
    tree.functions.addAll((*module)->functions);
    return (*module)->root;
  }
  auto defaults = m_nodeParameters.get(*name);
  if (!defaults) return std::unexpected(std::move(defaults.error()));
  auto parameters = defaults->get();
  for (auto& p : parameters) {
    if (auto configured = config->maybe(p.first)) {
      auto value = nodeParameterValueFromJson(*configured);
      if (!value) return std::unexpected(std::move(value.error()));
      p.second.second = std::move(*value);
    }
  }
  auto applied = applyTreeParameters(parameters, treeParameters);
  if (!applied) return std::unexpected(std::move(applied.error()));
  if (type->get() == BehaviorNodeType::Action) {
    tree.functions.add(*name);
    auto outputs = json.getObject("output", {});
    if (!outputs) return std::unexpected(std::move(outputs.error()));
    auto defaults = m_nodeOutput.get(*name);
    if (!defaults) return std::unexpected(std::move(defaults.error()));
    auto output = defaults->get();
    Json outputConfig(*outputs);
    for (auto& p : output) {
      auto configured = outputConfig.optString(p.first);
      if (!configured) return std::unexpected(std::move(configured.error()));
      auto replacement = replaceOutputBehaviorTag(*configured ? *configured : p.second.second.first, treeParameters);
      if (!replacement) return std::unexpected(std::move(replacement.error()));
      p.second.second.first = std::move(*replacement);
    }
    return std::make_shared<BehaviorNode>(ActionNode(*name, std::move(parameters), std::move(output)));
  }
  if (type->get() == BehaviorNodeType::Decorator) {
    tree.functions.add(*name);
    auto childJson = json.get("child");
    if (!childJson) return std::unexpected(std::move(childJson.error()));
    auto child = behaviorNode(*childJson, treeParameters, tree);
    if (!child) return std::unexpected(std::move(child.error()));
    return std::make_shared<BehaviorNode>(DecoratorNode(*name, std::move(parameters), *child));
  }
  if (type->get() == BehaviorNodeType::Composite) {
    auto composite = compositeNode(json, std::move(parameters), treeParameters, tree);
    if (!composite) return std::unexpected(std::move(composite.error()));
    return std::make_shared<BehaviorNode>(std::move(*composite));
  }
  return std::unexpected(StarException(strf("Behavior node type '{}' could not be created from JSON", *typeName)));
}
}
