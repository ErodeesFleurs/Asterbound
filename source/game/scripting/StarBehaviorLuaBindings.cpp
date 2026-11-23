module;
import star.json;
import star.lua;
import star.list;
import std;
import star.format;
import star.string;

import star.lua_game_converters;
import star.root;

module star.behavior_lua_bindings;

import star.behavior_database;
import star.behavior_state;

namespace Star {

std::expected<LuaCallbacks, StarException> LuaBindings::make_behavior_callbacks(Root& root, List<std::shared_ptr<BehaviorState>>& list) {
  LuaCallbacks callbacks;

  auto registration = callbacks.register_callback("behavior", [&root, &list](Json const& config, JsonObject const& parameters, LuaTable context, std::optional<LuaUserData> blackboard) -> std::expected<std::weak_ptr<BehaviorState>, StarException> {
    auto behavior_database = root.behavior_database();
    if (!behavior_database)
      return std::unexpected(std::move(behavior_database.error()));
    std::optional<std::weak_ptr<Blackboard>> board;
    if (blackboard) {
      auto matches = blackboard->is<std::weak_ptr<Blackboard>>();
      if (!matches)
        return std::unexpected(std::move(matches.error()));
      if (*matches) {
        auto value = blackboard->get<std::weak_ptr<Blackboard>>();
        if (!value)
          return std::unexpected(std::move(value.error()));
        board = value->get();
      }
    }

    std::expected<std::shared_ptr<BehaviorTree const>, StarException> tree;
    if (config.is_type(Json::Type::String)) {
      auto name = config.to_string();
      if (!name)
        return std::unexpected(std::move(name.error()));
      if (parameters.empty()) {
        tree = (*behavior_database)->behavior_tree(*name);
      } else {
        auto tree_config = (*behavior_database)->behavior_config(*name);
        if (!tree_config)
          return std::unexpected(std::move(tree_config.error()));
        auto object = tree_config->to_object();
        if (!object)
          return std::unexpected(std::move(object.error()));
        auto parameter = object->get("parameters");
        if (!parameter)
          return std::unexpected(std::move(parameter.error()));
        object->set("parameters", json_merge(parameter->get(), parameters));
        tree = (*behavior_database)->build_tree(*object);
      }
    } else {
      auto defaults = config.get_object("parameters", {});
      if (!defaults)
        return std::unexpected(std::move(defaults.error()));
      auto tree_config = config.set("parameters", json_merge(*defaults, parameters));
      if (!tree_config)
        return std::unexpected(std::move(tree_config.error()));
      tree = (*behavior_database)->build_tree(*tree_config);
    }
    if (!tree)
      return std::unexpected(std::move(tree.error()));
    auto state = BehaviorState::create(std::move(*tree), std::move(context), std::move(board));
    if (!state)
      return std::unexpected(std::move(state.error()));
    auto handle = std::weak_ptr<BehaviorState>(*state);
    list.push_back(std::move(*state));
    return handle;
  });
  if (!registration)
    return std::unexpected(std::move(registration.error()));

  return callbacks;
}

}
