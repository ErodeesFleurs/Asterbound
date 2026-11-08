module;
import star.json;
import star.lua;
import star.ordered_map;
import star.bi_map;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.string;
import star.version;


// Match client include order for SIMD intrinsics used by xxhash.
import star.lua_game_converters;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

module star.behavior_lua_bindings;

import star.behavior_database;
import star.behavior_state;

namespace Star {

std::expected<LuaCallbacks, StarException> LuaBindings::makeBehaviorCallbacks(Root& root, List<std::shared_ptr<BehaviorState>>& list) {
  LuaCallbacks callbacks;

  auto registration = callbacks.registerCallback("behavior", [&root, &list](Json const& config, JsonObject const& parameters, LuaTable context, std::optional<LuaUserData> blackboard) -> std::expected<std::weak_ptr<BehaviorState>, StarException> {
    auto behaviorDatabase = root.behaviorDatabase();
    if (!behaviorDatabase)
      return std::unexpected(std::move(behaviorDatabase.error()));
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
    if (config.isType(Json::Type::String)) {
      auto name = config.toString();
      if (!name)
        return std::unexpected(std::move(name.error()));
      if (parameters.empty()) {
        tree = (*behaviorDatabase)->behaviorTree(*name);
      } else {
        auto treeConfig = (*behaviorDatabase)->behaviorConfig(*name);
        if (!treeConfig)
          return std::unexpected(std::move(treeConfig.error()));
        auto object = treeConfig->toObject();
        if (!object)
          return std::unexpected(std::move(object.error()));
        auto parameter = object->get("parameters");
        if (!parameter)
          return std::unexpected(std::move(parameter.error()));
        object->set("parameters", jsonMerge(parameter->get(), parameters));
        tree = (*behaviorDatabase)->buildTree(*object);
      }
    } else {
      auto defaults = config.getObject("parameters", {});
      if (!defaults)
        return std::unexpected(std::move(defaults.error()));
      auto treeConfig = config.set("parameters", jsonMerge(*defaults, parameters));
      if (!treeConfig)
        return std::unexpected(std::move(treeConfig.error()));
      tree = (*behaviorDatabase)->buildTree(*treeConfig);
    }
    if (!tree)
      return std::unexpected(std::move(tree.error()));
    auto state = BehaviorState::create(*tree, std::move(context), board);
    if (!state)
      return std::unexpected(std::move(state.error()));
    list.push_back(*state);
    return std::weak_ptr<BehaviorState>(*state);
  });
  if (!registration)
    return std::unexpected(std::move(registration.error()));

  return callbacks;
}

}
