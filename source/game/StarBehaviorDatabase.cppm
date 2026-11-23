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

using CompositeNode = std::variant<SequenceNode, SelectorNode, ParallelNode, DynamicNode, RandomizeNode>;

using BehaviorNode = std::variant<ActionNode, DecoratorNode, CompositeNode>;

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
inline constexpr auto node_parameter_type_names = sorted_enum_names(
  std::to_array<std::pair<NodeParameterType, std::string_view>>({{NodeParameterType::Json, "json"},
                                                                 {NodeParameterType::Entity, "entity"},
                                                                 {NodeParameterType::Position, "position"},
                                                                 {NodeParameterType::Vec2, "vec2"},
                                                                 {NodeParameterType::Number, "number"},
                                                                 {NodeParameterType::Bool, "bool"},
                                                                 {NodeParameterType::List, "list"},
                                                                 {NodeParameterType::Table, "table"},
                                                                 {NodeParameterType::String, "string"}}));

using NodeParameterValue = std::variant<String, Json>;
using NodeParameter = std::pair<NodeParameterType, NodeParameterValue>;
struct NodeOutput {
  NodeParameterType type;
  std::optional<String> key;
  bool ephemeral;
};

[[nodiscard]] std::expected<NodeParameterValue, StarException> node_parameter_value_from_json(Json const& json);

[[nodiscard]] std::expected<Json, StarException> json_from_node_parameter(NodeParameter const& parameter);
[[nodiscard]] std::expected<NodeParameter, StarException> json_to_node_parameter(Json const& json);

[[nodiscard]] std::expected<Json, StarException> json_from_node_output(NodeOutput const& output);
[[nodiscard]] std::expected<NodeOutput, StarException> json_to_node_output(Json const& json);

enum class BehaviorNodeType : std::uint16_t {
  Action,
  Decorator,
  Composite,
  Module
};
inline constexpr auto behavior_node_type_names = sorted_enum_names(
  std::to_array<std::pair<BehaviorNodeType, std::string_view>>({{BehaviorNodeType::Action, "Action"},
                                                                {BehaviorNodeType::Decorator, "Decorator"},
                                                                {BehaviorNodeType::Composite, "Composite"},
                                                                {BehaviorNodeType::Module, "Module"}}));

enum class CompositeType : std::uint16_t {
  Sequence,
  Selector,
  Parallel,
  Dynamic,
  Randomize
};
inline constexpr auto composite_type_names =
  sorted_enum_names(std::to_array<std::pair<CompositeType, std::string_view>>({{CompositeType::Sequence, "Sequence"},
                                                                             {CompositeType::Selector, "Selector"},
                                                                             {CompositeType::Parallel, "Parallel"},
                                                                             {CompositeType::Dynamic, "Dynamic"},
                                                                             {CompositeType::Randomize, "Randomize"}}));

[[nodiscard]] std::expected<NodeParameterValue, StarException> replace_behavior_tag(NodeParameterValue const& parameter, StringMap<NodeParameterValue> const& tree_parameters);
[[nodiscard]] std::expected<std::optional<String>, StarException> replace_output_behavior_tag(std::optional<String> output, StringMap<NodeParameterValue> const& tree_parameters);
[[nodiscard]] std::expected<void, StarException> apply_tree_parameters(StringMap<NodeParameter>& parameters, StringMap<NodeParameterValue> const& tree_parameters);

struct ActionNode {
  ActionNode(String name, StringMap<NodeParameter> parameters, StringMap<NodeOutput> output);
  ActionNode(ActionNode&&) = default;

  String name;
  StringMap<NodeParameter> parameters;
  StringMap<NodeOutput> output;
  std::uint64_t const node_id;
};

struct DecoratorNode {
  DecoratorNode(String name, StringMap<NodeParameter> parameters, std::shared_ptr<BehaviorNode const> child);
  DecoratorNode(DecoratorNode&&) = default;

  String name;
  StringMap<NodeParameter> parameters;
  std::shared_ptr<BehaviorNode const> child;
  std::uint64_t const node_id;
};

struct SequenceNode {
  List<std::shared_ptr<BehaviorNode const>> children;
};

struct SelectorNode {
  List<std::shared_ptr<BehaviorNode const>> children;
};

struct ParallelNode {
  int succeed;
  int fail;
  List<std::shared_ptr<BehaviorNode const>> children;
};

struct DynamicNode {
  List<std::shared_ptr<BehaviorNode const>> children;
};

struct RandomizeNode {
  List<std::shared_ptr<BehaviorNode const>> children;
};

struct BehaviorTree {
  String name;
  StringSet scripts;
  StringSet functions;
  std::shared_ptr<BehaviorNode const> root;
};

class BehaviorDatabase {
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  BehaviorDatabase(ConstructionKey, std::shared_ptr<Assets const> assets);
  [[nodiscard]] static std::expected<std::shared_ptr<BehaviorDatabase>, StarException> create(std::shared_ptr<Assets const> assets);

  [[nodiscard]] std::expected<std::shared_ptr<BehaviorTree const>, StarException> behavior_tree(String const& name) const;
  [[nodiscard]] std::expected<std::shared_ptr<BehaviorTree const>, StarException> build_tree(Json const& config, StringMap<NodeParameterValue> const& overrides = {}) const;
  [[nodiscard]] std::expected<Json, StarException> behavior_config(String const& name) const;

private:
  struct TreeDefinition {
    Json config;
    String path;
    std::shared_ptr<BehaviorTree const> tree;
  };
  struct NodeDefinition {
    StringMap<NodeParameter> parameters;
    StringMap<NodeOutput> output;
  };
  struct Expansion {
    StringSet& scripts;
    StringSet& functions;
    List<std::string_view>& modules;
  };
  std::shared_ptr<Assets const> m_assets;
  StringMap<TreeDefinition> m_behaviors;
  StringMap<NodeDefinition> m_nodes;

  [[nodiscard]] std::expected<std::shared_ptr<BehaviorTree const>, StarException> compile_tree(String name, Json const& config, StringMap<NodeParameterValue> const& overrides, List<std::string_view> modules) const;
  [[nodiscard]] std::expected<std::shared_ptr<BehaviorNode const>, StarException> expand_tree(Json const& config, StringMap<NodeParameterValue> parameters, Expansion& expansion) const;
  [[nodiscard]] std::expected<CompositeNode, StarException> composite_node(String const& name, Json const& config, StringMap<NodeParameter> const& parameters, StringMap<NodeParameterValue> const& tree_parameters, Expansion& expansion) const;
  [[nodiscard]] std::expected<std::shared_ptr<BehaviorNode const>, StarException> behavior_node(Json const& json, StringMap<NodeParameterValue> const& tree_parameters, Expansion& expansion) const;
};

}

export module star.behavior_database;

export namespace Star {
  using ::Star::CompositeNode;
  using ::Star::BehaviorNode;
  using ::Star::NodeParameterType;
  using ::Star::node_parameter_type_names;
  using ::Star::NodeParameterValue;
  using ::Star::NodeParameter;
  using ::Star::NodeOutput;
  using ::Star::node_parameter_value_from_json;
  using ::Star::json_from_node_parameter;
  using ::Star::json_to_node_parameter;
  using ::Star::json_from_node_output;
  using ::Star::json_to_node_output;
  using ::Star::BehaviorNodeType;
  using ::Star::behavior_node_type_names;
  using ::Star::CompositeType;
  using ::Star::composite_type_names;
  using ::Star::replace_behavior_tag;
  using ::Star::replace_output_behavior_tag;
  using ::Star::apply_tree_parameters;
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
