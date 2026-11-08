import star.json;
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
import star.lua;
import star.output_proxy;

import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.lua_root;
import star.utility_lua_bindings;

import star.root_lua_bindings;
import star.rebuilder;

namespace Star {

std::expected<std::shared_ptr<Rebuilder>, StarException> Rebuilder::create(Root& root, String const& id, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) {
  auto rebuilder = std::shared_ptr<Rebuilder>(new Rebuilder());
  auto luaRoot = LuaRoot::create(root);
  if (!luaRoot)
    return std::unexpected(std::move(luaRoot.error()));
  rebuilder->m_luaRoot = std::move(*luaRoot);
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  rebuilder->m_contexts = std::make_shared<List<LuaContext>>();

  for (auto const& path : (*assets)->assetSources()) {
    auto metadata = (*assets)->assetSourceMetadata(path);
    if (!metadata)
      return std::unexpected(std::move(metadata.error()));
    auto scripts = metadata->maybe("errorHandlers");
    if (!scripts)
      continue;
    auto scriptPaths = scripts->optArray(id);
    if (!scriptPaths)
      return std::unexpected(std::move(scriptPaths.error()));
    if (!*scriptPaths)
      continue;
    for (auto const& scriptPath : **scriptPaths) {
      auto pathString = scriptPath.toString();
      if (!pathString)
        return std::unexpected(std::move(pathString.error()));
      auto context = rebuilder->m_luaRoot->createContext(*pathString);
      if (!context)
        return std::unexpected(std::move(context.error()));
      auto rootCallbacks = LuaBindings::makeRootCallbacks(root, itemCatalog);
      if (!rootCallbacks)
        return std::unexpected(std::move(rootCallbacks.error()));
      auto setRoot = context->setCallbacks("root", *rootCallbacks);
      if (!setRoot)
        return std::unexpected(std::move(setRoot.error()));
      auto utilityCallbacks = LuaBindings::makeUtilityCallbacks(root.random(), root.logger(), root.logMap());
      if (!utilityCallbacks)
        return std::unexpected(std::move(utilityCallbacks.error()));
      auto setUtility = context->setCallbacks("sb", *utilityCallbacks);
      if (!setUtility)
        return std::unexpected(std::move(setUtility.error()));
      rebuilder->m_contexts->push_back(std::move(*context));
    }
  }
  return rebuilder;
}

std::expected<bool, StarException> Rebuilder::rebuild(Json store, StarException lastError, AttemptCallback attempt) const {
  if (!attempt)
    return std::unexpected(StarException("Cannot rebuild with an empty attempt callback"));
  std::unique_lock locker(m_luaMutex, std::defer_lock);
  lockWithDiagnostics(locker);
  for (auto& context : *m_contexts) {
    auto next = context.invokePath<Json>("error", store, Star::format("{}", Star::output_exception(lastError, false)));
    if (!next)
      return std::unexpected(std::move(next.error()));
    if (!*next || *next == store)
      break;
    store = std::move(*next);
    auto result = attempt(store);
    if (result)
      return true;
    lastError = std::move(result.error());
  }
  return false;
}

}