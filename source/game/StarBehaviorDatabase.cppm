module;

import std;
import star.format;
import star.string;
import star.list;
import star.bi_map;
import star.json;
import star.game_types;

namespace Star {

class Assets;

struct ActionNode;
struct DecoratorNode;
struct SequenceNode;
struct SelectorNode;
struct ParallelNode;
struct DynamicNode;
struct RandomizeNode;
struct BehaviorTree;

using CompositeNode = std::variant<SequenceNode, SelectorNode, ParallelNode, DynamicNode, RandomizeNode>;

using BehaviorNode = std::variant<ActionNode, DecoratorNode, CompositeNode, std::shared_ptr<BehaviorTree const>>;

enum class NodeParameterType : std::uint8_t {
  Json,
  Entity,
  Position,
  Vec2,
  Number,
  Bool,
  List,
  Table,
  String
};
extern std::expected<EnumMap<NodeParameterType>, StarException> const NodeParameterTypeNames;

using NodeParameterValue = std::variant<String, Json>;
using NodeParameter = std::pair<NodeParameterType, NodeParameterValue>;
using NodeOutput = std::pair<NodeParameterType, std::pair<std::optional<String>, bool>>;

[[nodiscard]] std::expected<NodeParameterValue, StarException> nodeParameterValueFromJson(Json const& json);

[[nodiscard]] std::expected<Json, StarException> jsonFromNodeParameter(NodeParameter const& parameter);
[[nodiscard]] std::expected<NodeParameter, StarException> jsonToNodeParameter(Json const& json);

[[nodiscard]] std::expected<Json, StarException> jsonFromNodeOutput(NodeOutput const& output);
[[nodiscard]] std::expected<NodeOutput, StarException> jsonToNodeOutput(Json const& json);

enum class BehaviorNodeType : std::uint16_t {
  Action,
  Decorator,
  Composite,
  Module
};
extern std::expected<EnumMap<BehaviorNodeType>, StarException> const BehaviorNodeTypeNames;

enum class CompositeType : std::uint16_t {
  Sequence,
  Selector,
  Parallel,
  Dynamic,
  Randomize
};
extern std::expected<EnumMap<CompositeType>, StarException> const CompositeTypeNames;

// replaces global tags in nodeParameters in place
[[nodiscard]] std::expected<NodeParameterValue, StarException> replaceBehaviorTag(NodeParameterValue const& parameter, StringMap<NodeParameterValue> const& treeParameters);
[[nodiscard]] std::expected<std::optional<String>, StarException> replaceOutputBehaviorTag(std::optional<String> const& output, StringMap<NodeParameterValue> const& treeParameters);
[[nodiscard]] std::expected<void, StarException> applyTreeParameters(StringMap<NodeParameter>& nodeParameters, StringMap<NodeParameterValue> const& treeParameters);

struct ActionNode {
  ActionNode(String name, StringMap<NodeParameter> parameters, StringMap<NodeOutput> output);

  String name;
  StringMap<NodeParameter> parameters;
  StringMap<NodeOutput> output;
};

struct DecoratorNode {
  DecoratorNode(String const& name, StringMap<NodeParameter> parameters, std::shared_ptr<BehaviorNode const> child);

  String name;
  StringMap<NodeParameter> parameters;
  std::shared_ptr<BehaviorNode const> child;
};

struct SequenceNode {
  SequenceNode(List<std::shared_ptr<BehaviorNode const>> children);

  List<std::shared_ptr<BehaviorNode const>> children;
};

struct SelectorNode {
  SelectorNode(List<std::shared_ptr<BehaviorNode const>> children);

  List<std::shared_ptr<BehaviorNode const>> children;
};

struct ParallelNode {
  ParallelNode(int succeed, int fail, List<std::shared_ptr<BehaviorNode const>> children);

  int succeed;
  int fail;
  List<std::shared_ptr<BehaviorNode const>> children;
};

struct DynamicNode {
  DynamicNode(List<std::shared_ptr<BehaviorNode const>> children);

  List<std::shared_ptr<BehaviorNode const>> children;
};

struct RandomizeNode {
  RandomizeNode(List<std::shared_ptr<BehaviorNode const>> children);

  List<std::shared_ptr<BehaviorNode const>> children;
};

struct BehaviorTree {
  BehaviorTree(String const& name, StringSet scripts, JsonObject const& parameters);

  String name;
  StringSet scripts;
  StringSet functions;
  JsonObject parameters;

  std::shared_ptr<BehaviorNode const> root;
};

class BehaviorDatabase {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<BehaviorDatabase>, StarException> create(std::shared_ptr<Assets const> assets);

  [[nodiscard]] std::expected<std::shared_ptr<BehaviorTree const>, StarException> behaviorTree(String const& name) const;
  [[nodiscard]] std::expected<std::shared_ptr<BehaviorTree const>, StarException> buildTree(Json const& config, StringMap<NodeParameterValue> const& overrides = {}) const;
  [[nodiscard]] std::expected<Json, StarException> behaviorConfig(String const& name) const;

private:
  BehaviorDatabase() = default;
  std::shared_ptr<Assets const> m_assets;
  StringMap<String> m_configPaths;
  StringMap<Json> m_configs;
  StringMap<std::shared_ptr<BehaviorTree const>> m_behaviors;
  StringMap<StringMap<NodeParameter>> m_nodeParameters;
  StringMap<StringMap<NodeOutput>> m_nodeOutput;

  [[nodiscard]] std::expected<void, StarException> loadTree(String const& name);

  // constructs node variants
  [[nodiscard]] std::expected<CompositeNode, StarException> compositeNode(Json const& config, StringMap<NodeParameter> parameters, StringMap<NodeParameterValue> const& treeParameters, BehaviorTree& tree) const;
  [[nodiscard]] std::expected<std::shared_ptr<BehaviorNode const>, StarException> behaviorNode(Json const& json, StringMap<NodeParameterValue> const& treeParameters, BehaviorTree& tree) const;
};

}

export module star.behavior_database;

export namespace Star {
  using ::Star::CompositeNode;
  using ::Star::BehaviorNode;
  using ::Star::NodeParameterType;
  using ::Star::NodeParameterTypeNames;
  using ::Star::NodeParameterValue;
  using ::Star::NodeParameter;
  using ::Star::NodeOutput;
  using ::Star::nodeParameterValueFromJson;
  using ::Star::jsonFromNodeParameter;
  using ::Star::jsonToNodeParameter;
  using ::Star::jsonFromNodeOutput;
  using ::Star::jsonToNodeOutput;
  using ::Star::BehaviorNodeType;
  using ::Star::BehaviorNodeTypeNames;
  using ::Star::CompositeType;
  using ::Star::CompositeTypeNames;
  using ::Star::replaceBehaviorTag;
  using ::Star::replaceOutputBehaviorTag;
  using ::Star::applyTreeParameters;
  using ::Star::BehaviorDatabase;
  using ::Star::ActionNode;
  using ::Star::DecoratorNode;
  using ::Star::SequenceNode;
  using ::Star::SelectorNode;
  using ::Star::ParallelNode;
  using ::Star::DynamicNode;
  using ::Star::RandomizeNode;
  using ::Star::BehaviorTree;
}
