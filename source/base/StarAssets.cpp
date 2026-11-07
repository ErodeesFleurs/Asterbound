import std;
import star.optional;
import star.format;
import star.output_proxy;
import star.json;
import star.json_extra;
import star.ordered_map;
import star.rect;
import star.bi_map;
import star.thread;
import star.ref_ptr;
import star.list;
import star.image;
import star.image_processing;
import star.hash;
import star.set;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.file;
import star.io_device;
import star.string;
import star.json_parser;
import star.json_builder;
import star.logging;
import star.random;
import star.font;
import star.audio;
import star.casting;
import star.lexical_cast;
import star.data_stream_devices;
import star.lua;

import star.assets;
import star.asset_source;
import star.directory_asset_source;
import star.image_lua_bindings;
import star.utility_lua_bindings;

import star.sha256;
import star.memory_asset_source;
import star.packed_asset_source;
import star.json_patch;

import star.byte_array;
import star.vector;
namespace Star {

// An empty diagnostic means the base path is valid.
static std::optional<std::string_view> validateBasePath(std::string_view const& basePath) {
  if (basePath.empty() || basePath[0] != '/')
    return "Path '<path>' must be absolute";

  bool first = true;
  bool slashed = true;
  bool dotted = false;
  for (auto c : basePath) {
    if (c == '/') {
      if (!first) {
        if (slashed)
          return "Path '<path>' contains consecutive //, not allowed";
        else if (dotted)
          return "Path '<path>' '.' and '..' not allowed";
      }
      slashed = true;
      dotted = false;
    } else if (c == ':') {
      if (slashed)
        return "Path '<path>' has ':' after directory";
      break;
    } else if (c == '?') {
      if (slashed)
        return "Path '<path>' has '?' after directory";
      break;
    } else {
      slashed = false;
      dotted = c == '.';
    }
    first = false;
  }
  if (slashed)
    return "Path '<path>' cannot be a file";

  return std::nullopt;
}

[[nodiscard]] static std::expected<void, StarException> validatePath(AssetPath const& components, bool canContainSubPath, bool canContainDirectives) {
  if (auto error = validateBasePath(components.basePath.utf8())) {
    std::array<std::pair<std::string_view, std::string_view>, 1> tags{{{"path", components.basePath.utf8()}}};
    auto text = formatTags(*error, tags);
    if (!text)
      return std::unexpected(std::move(text.error()));
    return std::unexpected(AssetException(std::move(*text)));
  }
  if (!canContainSubPath && components.subPath)
    return std::unexpected(AssetException::format("Path '{}' cannot contain sub-path", components));
  if (!canContainDirectives && !components.directives.empty())
    return std::unexpected(AssetException::format("Path '{}' cannot contain directives", components));
  return {};
}

// Only a confirmed vanished directory entry is optional. A dangling symlink,
// permission failure, corrupt package or unreadable existing file stays an error.
static std::expected<std::optional<ByteArray>, StarException> readPatch(AssetSource& source, String const& path,
                                                                        Logger& logger) {
  auto bytes = source.read(path);
  if (bytes)
    return std::optional<ByteArray>(std::move(*bytes));
  if (bytes.error().is<IOException>()) {
    if (auto directory = asRef<DirectoryAssetSource>(source)) {
      auto filesystem = directory->get().toFilesystem(path);
      if (filesystem) {
        std::error_code error;
        auto status = std::filesystem::symlink_status(std::filesystem::path(filesystem->utf8()), error);
        if (status.type() == std::filesystem::file_type::not_found
            && (!error || error == std::errc::no_such_file_or_directory)) {
          auto logged = logger.warn("Skipping vanished patch '{}' ({})", path, *filesystem);
          if (!logged)
            return std::unexpected(std::move(logged.error()));
          return std::nullopt;
        }
      }
    }
  }
  return std::unexpected(std::move(bytes.error()));
}

std::string Assets::sourceDiagnostic(AssetSource const& source,
    std::optional<std::reference_wrapper<String const>> sourcePath) {
  auto result = sourcePath ? strf("asset source '{}'", sourcePath->get()) : std::string("unregistered asset source");
  auto const& metadata = source.metadata();
  for (String const& key : {String("friendlyName"), String("name")}) {
    if (auto value = metadata.find_ref(key)) {
      // Invalid optional metadata must not hide the error being diagnosed.
      auto text = value->get().stringPtr();
      if (text && !(**text).empty())
        result += strf(", {} '{}'", key, **text);
    }
  }
  return result;
}

StarException Assets::withAssetContext(StarException error, String const& path, std::string_view operation) const {
  auto const& current = *error.diagnosticSummary();
  bool addAsset = current.asset.empty();
  bool addOperation = current.operation.empty() && !operation.empty();
  bool addProvider = current.assetProvider.empty() && (addAsset || current.asset == path.utf8());
  if (!addAsset && !addOperation && !addProvider)
    return error;
  std::string provider;
  if (addProvider) {
    auto descriptor = assetDescriptor(path);
    if (descriptor && *descriptor) {
      auto sourcePath = assetSourcePath((*descriptor)->source);
      provider = sourceDiagnostic(*(*descriptor)->source,
          sourcePath ? std::optional<std::reference_wrapper<String const>>(std::cref(*sourcePath)) : std::nullopt);
    }
  }
  if (!addAsset && !addOperation && provider.empty())
    return error;
  auto summary = current;
  if (addAsset)
    summary.asset = path.utf8();
  if (addOperation)
    summary.operation = operation;
  if (addAsset || !provider.empty()) {
    auto context = provider.empty() ? strf("{} '{}'", operation, path)
        : strf("{} '{}' from {}", operation, path, provider);
    error = std::move(error).withContext(std::move(context));
  }
  if (!provider.empty())
    summary.assetProvider = std::move(provider);
  return std::move(error).withSummary(std::move(summary));
}


static void addSummarySource(ExceptionSummary& summary, std::string_view source, std::string_view provider) {
  for (auto& location : summary.locations) {
    if (location.source == source && (location.provider.empty() || location.provider == provider)) {
      if (location.provider.empty())
        location.provider = provider;
      return;
    }
  }
  summary.locations.push_back({std::string(source), -1, std::string(provider)});
}

static StarException resourceFailure(StarException error, String const& path) {
  auto const& current = *error.diagnosticSummary();
  if (current.kind != ExceptionSummaryKind::Native && !current.asset.empty())
    return error;
  auto summary = *error.diagnosticSummary();
  if (summary.kind == ExceptionSummaryKind::Native)
    summary.kind = ExceptionSummaryKind::Resource;
  if (summary.asset.empty())
    summary.asset = path.utf8();
  return std::move(error).withSummary(std::move(summary));
}

static StarException scriptSourceFailure(StarException error, String const& path, std::string const& provider) {
  if (std::ranges::any_of(error.diagnosticSummary()->locations, [&](auto const& location) {
        return location.source == path.utf8() && location.provider == provider;
      }))
    return error;
  auto summary = *error.diagnosticSummary();
  addSummarySource(summary, path.utf8(), provider);
  return std::move(error).withSummary(std::move(summary));
}

static StarException luaPatchFailure(StarException error, String const& targetPath, String const& patchPath,
    AssetSource const& source, std::optional<std::reference_wrapper<String const>> sourcePath) {
  auto provider = Assets::sourceDiagnostic(source, sourcePath);
  auto const& current = *error.diagnosticSummary();
  auto patchBasePath = AssetPath::removeSubPath(patchPath);
  bool changeKind = current.kind != ExceptionSummaryKind::PatchSyntax && current.kind != ExceptionSummaryKind::PatchRuntime;
  bool changePatch = current.patch.empty();
  bool changeSource = std::ranges::none_of(current.locations, [&](auto const& location) {
    return location.source == patchBasePath.utf8() && location.provider == provider;
  });
  if (changeKind || changePatch || changeSource) {
    auto summary = current;
    if (changeKind)
      summary.kind = current.kind == ExceptionSummaryKind::ScriptSyntax
        ? ExceptionSummaryKind::PatchSyntax : ExceptionSummaryKind::PatchRuntime;
    if (changePatch) {
      summary.asset = targetPath.utf8();
      summary.patch = patchPath.utf8();
      summary.patchProvider = provider;
    }
    if (changeSource)
      addSummarySource(summary, patchBasePath.utf8(), provider);
    error = std::move(error).withSummary(std::move(summary));
  }
  return std::move(error).withContext(
      strf("Lua patch provider: target '{}', patch '{}', {}", targetPath, patchPath, provider));
}

std::expected<std::optional<RectU>, StarException> FramesSpecification::getRect(String const& frame) const {
  if (auto alias = aliases.find_ref(frame)) {
    if (auto rect = frames.find_ref(alias->get()))
      return std::optional<RectU>(rect->get());
    return std::unexpected(AssetException(strf("No such frame '{}' found for alias '{}'", alias->get(), frame)));
  } else {
    return frames.maybe(frame);
  }
}

std::expected<Json, StarException> FramesSpecification::toJson() const {
  auto aliasJson = jsonFromMap(aliases);
  if (!aliasJson)
    return std::unexpected(std::move(aliasJson.error()));
  auto frameJson = jsonFromMapV(frames, jsonFromRectU);
  if (!frameJson)
    return std::unexpected(std::move(frameJson.error()));
  return JsonObject{{"aliases", std::move(*aliasJson)}, {"frames", std::move(*frameJson)}, {"file", framesFile}};
}

Assets::Assets(Logger& logger) : m_logger(logger) {}

std::expected<std::shared_ptr<Assets>, StarException> Assets::create(Logger& logger, RandomService& random, LogMap& logMap, Settings settings, StringList assetSources) {
  auto assets = std::shared_ptr<Assets>(new Assets(logger));
  auto initialized = assets->initialize(random, logMap, std::move(settings), std::move(assetSources));
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return assets;
}

std::expected<void, StarException> Assets::initialize(RandomService& random, LogMap& logMap, Settings settings, StringList assetSources) {
  constexpr std::string_view AssetsPatchSuffix = ".patch";
  constexpr std::string_view AssetsPatchListSuffix = ".patchlist";
  constexpr std::string_view AssetsLuaPatchSuffix = ".patch.lua";

  auto fontLibrary = FontLibrary::create();
  if (!fontLibrary)
    return std::unexpected(std::move(fontLibrary.error()));
  m_fontLibrary.emplace(std::move(*fontLibrary));
  m_settings = std::move(settings);
  m_assetSources = std::move(assetSources);

  auto createdLuaEngine = LuaEngine::create();
  if (!createdLuaEngine)
    return std::unexpected(std::move(createdLuaEngine.error()));
  auto luaEngine = std::move(*createdLuaEngine);
  luaEngine->setImageResolver([this](String const& path) { return image(path); });
  m_luaEngine = luaEngine;
  auto pushGlobalContext = [&luaEngine](String const& name, LuaCallbacks const& callbacks) -> std::expected<void, StarException> {
    auto table = luaEngine->createTable();
    if (!table)
      return std::unexpected(std::move(table.error()));
    for (auto const& p : callbacks.callbacks()) {
      auto function = luaEngine->createWrappedFunction(p.second);
      if (!function)
        return std::unexpected(std::move(function.error()));
      auto set = table->set(p.first, std::move(*function));
      if (!set)
        return std::unexpected(std::move(set.error()));
    }
    return luaEngine->setGlobal(name, std::move(*table));
  };

  auto makeBaseAssetCallbacks = [this]() -> std::expected<LuaCallbacks, StarException> {
    LuaCallbacks callbacks;
    auto registered = callbacks.registerCallback("byExtension", [this](String const& extension) -> std::expected<StringSet, StarException> {
      auto paths = scanExtension(extension);
      if (!paths)
        return std::unexpected(std::move(paths.error()));
      StringSet result;
      result.reserve(paths->get().size());
      for (auto const& path : paths->get())
        result.insert(path.string());
      return result;
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));
    registered = callbacks.registerCallback("json", [this](String const& path) { return json(path); });
    if (!registered)
      return std::unexpected(std::move(registered.error()));
    registered = callbacks.registerCallback("exists", [this](String const& path) { return assetExists(path); });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks.registerCallback("sourcePaths", [this](LuaEngine& engine, std::optional<bool> withMetaData) -> std::expected<LuaTable, StarException> {
      auto assetSources = this->assetSources();
      auto table = engine.createTable(assetSources.size(), 0);
      if (!table)
        return std::unexpected(std::move(table.error()));
      if (withMetaData.value_or(false)) {
        for (auto const& assetSource : assetSources) {
          auto metadata = this->assetSourceMetadata(assetSource);
          if (!metadata)
            return std::unexpected(std::move(metadata.error()));
          auto set = table->set(assetSource, std::move(*metadata));
          if (!set)
            return std::unexpected(std::move(set.error()));
        }
      } else {
        std::size_t i = 0;
        for (auto const& assetSource : assetSources) {
          auto set = table->set(++i, assetSource);
          if (!set)
            return std::unexpected(std::move(set.error()));
        }
      }
      return table;
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered =
      callbacks.registerCallback("sourceMetadata", [this](String const& sourcePath) -> std::optional<JsonObject> {
        if (auto assetSource = m_assetSourcePaths.find_right_ref(sourcePath))
          return assetSource->get()->metadata();
        return {};
      });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks.registerCallback("origin", [this](String const& path) -> std::expected<std::optional<String>, StarException> {
      auto descriptor = this->assetDescriptor(path);
      if (!descriptor)
        return std::unexpected(std::move(descriptor.error()));
      if (*descriptor)
        return this->assetSourcePath((*descriptor)->source);
      return std::optional<String>{};
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks.registerCallback("bytes", [this](String const& path) -> std::expected<String, StarException> {
      auto assetBytes = bytes(path);
      if (!assetBytes)
        return std::unexpected(std::move(assetBytes.error()));
      return String(std::string_view((*assetBytes)->begin(), (*assetBytes)->end()));
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks.registerCallback(
      "sourceBytes", [this](String const& sourcePath, String const& path) -> std::expected<String, StarException> {
        auto source = m_assetSourcePaths.find_right_ref(sourcePath);
        if (!source)
          return std::unexpected(
            AssetException::format("Cannot read asset '{}' from unknown asset source '{}'", path, sourcePath));
        // DirectoryAssetSource::read also accepts unindexed filesystem-relative paths.
        // Only expose paths actually enumerated by this provider, including shadowed assets.
        if (!source->get()->contains(path))
          return std::unexpected(AssetException::format("Asset '{}' is not indexed in {}", path,
                                                        sourceDiagnostic(*source->get(), std::cref(sourcePath))));
        auto data = source->get()->read(path);
        if (!data) {
          auto provider = sourceDiagnostic(*source->get(), std::cref(sourcePath));
          auto error = std::move(data.error()).withContext(strf("Reading asset '{}' from {}", path, provider));
          auto summary = *error.diagnosticSummary();
          summary.kind = ExceptionSummaryKind::Resource;
          summary.asset = path.utf8();
          summary.assetProvider = std::move(provider);
          summary.operation = "Reading source asset bytes";
          return std::unexpected(std::move(error).withSummary(std::move(summary)));
        }
        return String(std::string_view(data->begin(), data->end()));
      });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks.registerCallback("image", [this](String const& path) -> std::expected<Image, StarException> {
      auto assetImage = image(path);
      if (!assetImage)
        return std::unexpected(std::move(assetImage.error()));
      if ((*assetImage)->bytesPerPixel() == 3)
        return (*assetImage)->convert(PixelFormat::RGBA32);
      return **assetImage;
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks.registerCallback("frames", [this](String const& path) -> std::expected<Json, StarException> {
      auto frames = imageFrames(path);
      if (!frames)
        return std::unexpected(std::move(frames.error()));
      if (*frames)
        return (*frames)->toJson();
      return Json();
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks.registerCallback("scan", [this](std::optional<String> const& a, std::optional<String> const& b) -> std::expected<StringList, StarException> {
      return b ? scan(a.value_or(String{}), *b) : scan(a.value_or(String{}));
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));
    return callbacks;
  };

  auto utilityCallbacks = LuaBindings::makeUtilityCallbacks(random, m_logger, logMap);
  if (!utilityCallbacks)
    return std::unexpected(std::move(utilityCallbacks.error()));
  auto pushedUtility = pushGlobalContext("sb", *utilityCallbacks);
  if (!pushedUtility)
    return std::unexpected(std::move(pushedUtility.error()));
  auto baseCallbacks = makeBaseAssetCallbacks();
  if (!baseCallbacks)
    return std::unexpected(std::move(baseCallbacks.error()));
  auto pushedAssets = pushGlobalContext("assets", *baseCallbacks);
  if (!pushedAssets)
    return std::unexpected(std::move(pushedAssets.error()));

  struct LoadScriptReference {
    std::shared_ptr<AssetSource> source;
    String path;

    bool operator==(LoadScriptReference const&) const = default;
  };
  struct LoadScriptReplacement {
    LoadScriptReference original;
    LoadScriptReference replacement;
  };
  struct LoadScriptState {
    List<LoadScriptReplacement> replacements;
    List<LoadScriptReference> unavailable;
    bool active = true;
  };
  // Callbacks retain their state even if a script retains a function past its group.
  auto loadScriptState = std::make_shared<LoadScriptState>();

  auto decorateLuaContext = [&](LuaContext& context, std::shared_ptr<MemoryAssetSource> newFiles) -> std::expected<void, StarException> {
    if (!newFiles)
      return {};
    // Re-add the assets callbacks with the load-script mutation functions.
    auto removed = context.remove("assets");
    if (!removed)
      return std::unexpected(std::move(removed.error()));
    auto callbacks = makeBaseAssetCallbacks();
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    auto registered = callbacks->registerCallback("add", [newFiles](LuaEngine& engine, String const& path, LuaValue const& data) -> std::expected<void, StarException> {
      auto str = engine.luaMaybeTo<String>(data);
      if (!str)
        return std::unexpected(std::move(str.error()));
      if (*str) {
        newFiles->set(path, (*str)->utf8Bytes());
        return {};
      }
      auto image = engine.luaMaybeTo<Image>(data);
      if (!image)
        return std::unexpected(std::move(image.error()));
      if (*image) {
        newFiles->set(path, std::move(**image));
        return {};
      }
      auto json = engine.luaTo<Json>(data);
      if (!json)
        return std::unexpected(std::move(json.error()));
      auto text = json->repr();
      if (!text)
        return std::unexpected(std::move(text.error()));
      newFiles->set(path, text->utf8Bytes());
      return {};
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks->registerCallback("patch", [this, newFiles](String const& path, String const& patchPath) -> std::expected<bool, StarException> {
      if (newFiles->contains(path)) {
        auto image = newFiles->image(path);
        if (!image)
          return std::unexpected(std::move(image.error()));
        std::shared_ptr<AssetSource> patchSource;
        if (newFiles->contains(patchPath)) {
          patchSource = newFiles;
        } else {
          auto patchKey = CaseInsensitiveStringView::create(patchPath);
          if (!patchKey)
            return std::unexpected(std::move(patchKey.error()));
          if (auto asset = m_files.find_ref(*patchKey))
            patchSource = asset->get().source;
        }
        if (!patchSource)
          return false;
        if (*image) {
          auto patched = applyImagePatches(*image, path, {std::make_pair(patchPath, patchSource)});
          if (!patched)
            return std::unexpected(std::move(patched.error()));
          newFiles->set(path, **patched);
        } else {
          auto file = newFiles->read(path);
          if (!file)
            return std::unexpected(std::move(file.error()));
          auto json = inputUtf8Json(file->begin(), file->end(), JsonParseType::Top);
          if (!json)
            return std::unexpected(std::move(json.error()));
          auto patched = applyJsonPatches(*json, path, {std::make_pair(patchPath, patchSource)});
          if (!patched)
            return std::unexpected(std::move(patched.error()));
          auto text = patched->repr();
          if (!text)
            return std::unexpected(std::move(text.error()));
          newFiles->set(path, text->utf8Bytes());
        }
        return true;
      }
      auto pathKey = CaseInsensitiveStringView::create(path);
      if (!pathKey)
        return std::unexpected(std::move(pathKey.error()));
      if (auto file = m_files.find_ref(*pathKey)) {
        if (newFiles->contains(patchPath)) {
          file->get().patchSources.append(std::make_pair(patchPath, newFiles));
          return true;
        }
        auto patchKey = CaseInsensitiveStringView::create(patchPath);
        if (!patchKey)
          return std::unexpected(std::move(patchKey.error()));
        if (auto asset = m_files.find_ref(*patchKey)) {
          file->get().patchSources.append(std::make_pair(patchPath, asset->get().source));
          return true;
        }
      }
      return false;
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks->registerCallback(
      "patchReplace",
      [this, newFiles](String const& target, String const& originalPatchPath, String const& sourcePath,
                       String const& replacementPatchPath,
                       std::optional<String> const& replacementTarget) -> std::expected<bool, StarException> {
        auto originalSource = m_assetSourcePaths.find_right_ref(sourcePath);
        if (!originalSource)
          return false;
        auto targetKey = CaseInsensitiveStringView::create(target);
        if (!targetKey)
          return std::unexpected(std::move(targetKey.error()));
        auto file = m_files.find_ref(*targetKey);
        if (!file)
          return false;
        auto matches = [&](auto const& patch) {
          return patch.first == originalPatchPath && patch.second == originalSource->get();
        };
        if (!std::ranges::any_of(file->get().patchSources, matches))
          return false;
        auto* destination = std::addressof(file->get());
        if (replacementTarget) {
          auto destinationKey = CaseInsensitiveStringView::create(*replacementTarget);
          if (!destinationKey)
            return std::unexpected(std::move(destinationKey.error()));
          auto descriptor = m_files.find_ref(*destinationKey);
          if (!descriptor)
            return false;
          destination = std::addressof(descriptor->get());
        }
        std::pair<String, std::shared_ptr<AssetSource>> replacement{replacementPatchPath, newFiles};
        if (!newFiles->contains(replacementPatchPath)) {
          auto replacementKey = CaseInsensitiveStringView::create(replacementPatchPath);
          if (!replacementKey)
            return std::unexpected(std::move(replacementKey.error()));
          auto descriptor = m_files.find_ref(*replacementKey);
          if (!descriptor)
            return false;
          replacement = {descriptor->get().sourceName, descriptor->get().source};
        }
        if (destination == std::addressof(file->get())) {
          for (auto& patch : file->get().patchSources) {
            if (matches(patch))
              patch = replacement;
          }
        } else {
          for (auto const& patch : file->get().patchSources) {
            if (matches(patch))
              destination->patchSources.append(replacement);
          }
          file->get().patchSources.filter([&](auto const& patch) { return !matches(patch); });
        }
        // As at the load-script group boundary, dependent assets may already be
        // cached. Invalidate immediately so this same context observes the repair.
        // A previously failed target must also be allowed to load its new patches.
        m_framesSpecifications.clear();
        m_assetsCache.clear();
        m_assetErrors.clear();
        return true;
      });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks->registerCallback("loadScriptReplace", [this, newFiles, loadScriptState](
        String const& sourcePath, String const& originalScriptPath, String const& replacementScriptPath) -> std::expected<bool, StarException> {
      if (!loadScriptState->active)
        return false;
      auto originalSource = m_assetSourcePaths.find_right_ref(sourcePath);
      if (!originalSource || !originalSource->get()->contains(originalScriptPath))
        return false;
      LoadScriptReference original{.source = originalSource->get(), .path = originalScriptPath};
      if (loadScriptState->unavailable.contains(original))
        return false;
      auto metadata = original.source->metadata();
      auto scripts = metadata.find_ref("scripts");
      if (!scripts)
        return false;
      bool declared = false;
      for (String const& group : {String("onLoad"), String("postLoad")}) {
        auto paths = scripts->get().optArray(group);
        if (!paths)
          return std::unexpected(std::move(paths.error()));
        if (*paths) {
          for (auto const& path : **paths) {
            auto text = path.toString();
            if (!text)
              return std::unexpected(std::move(text.error()));
            declared = declared || *text == originalScriptPath;
          }
        }
      }
      if (!declared)
        return false;
      LoadScriptReference replacement{.source = newFiles, .path = replacementScriptPath};
      if (!newFiles->contains(replacementScriptPath)) {
        auto replacementKey = CaseInsensitiveStringView::create(replacementScriptPath);
        if (!replacementKey)
          return std::unexpected(std::move(replacementKey.error()));
        auto descriptor = m_files.find_ref(*replacementKey);
        if (!descriptor)
          return false;
        replacement = {.source = descriptor->get().source, .path = descriptor->get().sourceName};
      }
      for (auto& entry : loadScriptState->replacements) {
        if (entry.original == original) {
          entry.replacement = std::move(replacement);
          return true;
        }
      }
      loadScriptState->replacements.append({.original = std::move(original), .replacement = std::move(replacement)});
      return true;
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks->registerCallback("erase", [this](String const& path) -> std::expected<bool, StarException> {
      auto pathKey = CaseInsensitiveStringView::create(path);
      if (!pathKey)
        return std::unexpected(std::move(pathKey.error()));
      auto file = m_files.find(*pathKey);
      bool erased = file != m_files.end();
      if (erased) {
        m_files.erase(file);
        auto extension = AssetPath::extension(path);
        auto extensionKey = CaseInsensitiveStringView::create(extension);
        if (!extensionKey)
          return std::unexpected(std::move(extensionKey.error()));
        if (auto paths = m_filesByExtension.find_ref(*extensionKey)) {
          auto indexed = paths->get().find(*pathKey);
          if (indexed != paths->get().end())
            paths->get().erase(indexed);
        }
      }
      return erased;
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));
    return context.setCallbacks("assets", *callbacks);
  };

  auto addSource = [&](String const& sourcePath, std::shared_ptr<AssetSource> source) -> std::expected<void, StarException> {
    auto added = m_assetSourcePaths.add(sourcePath, source);
    if (!added)
      return std::unexpected(std::move(added.error()));

    for (auto const& filename : source->assetPaths()) {
      auto filenameKey = CaseInsensitiveString::create(filename);
      if (!filenameKey)
        return std::unexpected(std::move(filenameKey.error()));
      auto indexPatch = [&](std::size_t suffixSize) -> std::expected<void, StarException> {
        auto size = filename.size();
        if (!size)
          return std::unexpected(std::move(size.error()));
        auto targetPatchFile = filename.substr(0, *size - suffixSize);
        if (!targetPatchFile)
          return std::unexpected(std::move(targetPatchFile.error()));
        auto targetKey = CaseInsensitiveStringView::create(*targetPatchFile);
        if (!targetKey)
          return std::unexpected(std::move(targetKey.error()));
        if (auto p = m_files.find_ref(*targetKey))
          p->get().patchSources.append({filename, source});
        return {};
      };
      auto containsPatch = filename.contains(AssetsPatchSuffix, String::CaseInsensitive);
      if (!containsPatch)
        return std::unexpected(std::move(containsPatch.error()));
      if (*containsPatch) {
        auto endsPatch = filename.endsWith(AssetsPatchSuffix, String::CaseInsensitive);
        if (!endsPatch)
          return std::unexpected(std::move(endsPatch.error()));
        if (*endsPatch) {
          auto indexed = indexPatch(AssetsPatchSuffix.size());
          if (!indexed)
            return std::unexpected(std::move(indexed.error()));
        } else {
          auto endsLuaPatch = filename.endsWith(AssetsLuaPatchSuffix, String::CaseInsensitive);
          if (!endsLuaPatch)
            return std::unexpected(std::move(endsLuaPatch.error()));
          if (*endsLuaPatch) {
            auto indexed = indexPatch(AssetsLuaPatchSuffix.size());
            if (!indexed)
              return std::unexpected(std::move(indexed.error()));
          } else {
            auto endsPatchList = filename.endsWith(AssetsPatchListSuffix, String::CaseInsensitive);
            if (!endsPatchList)
              return std::unexpected(std::move(endsPatchList.error()));
            if (*endsPatchList) {
              auto stream = source->read(filename);
              if (!stream)
                return std::unexpected(std::move(stream.error()));
              auto patchList = inputUtf8Json(stream->begin(), stream->end(), JsonParseType::Top);
              if (!patchList)
                return std::unexpected(std::move(patchList.error()));
              auto patchPairs = patchList->iterateArray();
              if (!patchPairs)
                return std::unexpected(std::move(patchPairs.error()));
              std::size_t patchIndex = 0;
              for (auto const& patchPair : *patchPairs) {
                auto patches = patchPair.getArray("patches");
                if (!patches)
                  return std::unexpected(std::move(patches.error()));
                auto paths = patchPair.getArray("paths");
                if (!paths)
                  return std::unexpected(std::move(paths.error()));
                for (auto const& path : *paths) {
                  auto targetPath = path.toString();
                  if (!targetPath)
                    return std::unexpected(std::move(targetPath.error()));
                  auto targetKey = CaseInsensitiveStringView::create(*targetPath);
                  if (!targetKey)
                    return std::unexpected(std::move(targetKey.error()));
                  if (auto p = m_files.find_ref(*targetKey)) {
                    for (std::size_t i = 0; i != patches->size(); ++i) {
                      auto const& patch = (*patches)[i];
                      if (patch.isType(Json::Type::String)) {
                        auto patchPath = patch.toString();
                        if (!patchPath)
                          return std::unexpected(std::move(patchPath.error()));
                        p->get().patchSources.append({std::move(*patchPath), source});
                      } else {
                        p->get().patchSources.append({strf("{}:[{}].patches[{}]", filename, patchIndex, i), source});
                      }
                    }
                  }
                }
                ++patchIndex;
              }
            } else {
              for (int i = 0; i < 10; ++i) {
                auto endsNumberedPatch = filename.endsWith(String(AssetsPatchSuffix) + toString(i), String::CaseInsensitive);
                if (!endsNumberedPatch)
                  return std::unexpected(std::move(endsNumberedPatch.error()));
                if (*endsNumberedPatch) {
                  auto indexed = indexPatch(AssetsPatchSuffix.size() + 1);
                  if (!indexed)
                    return std::unexpected(std::move(indexed.error()));
                  break;
                }
              }
            }
          }
        }
      }

      auto& descriptor = m_files[*filenameKey];
      descriptor.sourceName = filename;
      descriptor.source = source;
      auto extension = AssetPath::extension(filename).toLower();
      if (!extension)
        return std::unexpected(std::move(extension.error()));
      auto extensionKey = CaseInsensitiveString::create(std::move(*extension));
      if (!extensionKey)
        return std::unexpected(std::move(extensionKey.error()));
      m_filesByExtension[*extensionKey].insert(std::move(*filenameKey));
    }
    return {};
  };

  auto runLoadScripts = [&](String const& groupName, String const& sourcePath, std::shared_ptr<AssetSource> source) -> std::expected<void, StarException> {
    auto metadata = source->metadata();
    if (auto scripts = metadata.find_ref("scripts")) {
      auto scriptGroup = scripts->get().optArray(groupName);
      if (!scriptGroup)
        return std::unexpected(std::move(scriptGroup.error()));
      if (*scriptGroup) {
        auto sourceName = metadata.value("name", File::baseName(sourcePath)).repr();
        if (!sourceName)
          return std::unexpected(std::move(sourceName.error()));
        auto memoryName = strf("{}::{}", *sourceName, groupName);
        JsonObject memoryMetadata{{"name", memoryName}};
        auto memoryAssets = std::make_shared<MemoryAssetSource>(memoryName, memoryMetadata);
        auto now = std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now());
        auto scriptGroupText = Json(**scriptGroup).repr();
        if (!scriptGroupText)
          return std::unexpected(std::move(scriptGroupText.error()));
        auto logged = m_logger.info("Running {} scripts {}", groupName, *scriptGroupText);
        if (!logged)
          return std::unexpected(std::move(logged.error()));
        auto runScripts = [&]() -> std::expected<void, StarException> {
          auto context = luaEngine->createContext();
          if (!context)
            return std::unexpected(std::move(context.error()));
          auto decorated = decorateLuaContext(*context, memoryAssets);
          if (!decorated)
            return std::unexpected(std::move(decorated.error()));
          for (auto const& jPath : **scriptGroup) {
            auto path = jPath.toString();
            if (!path)
              return std::unexpected(std::move(path.error()));
            LoadScriptReference selected{.source = source, .path = *path};
            loadScriptState->unavailable.append(selected);
            for (auto const& entry : loadScriptState->replacements) {
              if (entry.original == selected) {
                selected = entry.replacement;
                break;
              }
            }
            auto provider = sourceDiagnostic(*selected.source, m_assetSourcePaths.find_left_ref(selected.source));
            auto script = selected.source->read(selected.path);
            if (!script)
              return std::unexpected(scriptSourceFailure(std::move(script.error()), selected.path, provider));
            auto loaded = context->load(script->span(), selected.path);
            if (!loaded)
              return std::unexpected(scriptSourceFailure(std::move(loaded.error()), selected.path, provider));
          }
          return {};
        };
        auto ran = runScripts();
        // A failed script closes the rest of its group too; those declarations
        // will not be executed later and must not report a successful replacement.
        for (auto const& jPath : **scriptGroup) {
          auto path = jPath.toString();
          if (!path)
            return std::unexpected(std::move(path.error()));
          LoadScriptReference closed{.source = source, .path = std::move(*path)};
          if (!loadScriptState->unavailable.contains(closed))
            loadScriptState->unavailable.append(std::move(closed));
        }
        if (!ran) {
          if (!ran.error().is<LuaException>())
            return std::unexpected(std::move(ran.error()));
          auto summary = *ran.error().diagnosticSummary();
          if (summary.operation.empty())
            summary.operation = strf("Running {} scripts", groupName);
          ran.error() = std::move(ran.error()).withSummary(std::move(summary));
          logged = m_logger.errorException(
              strf("Exception while running {} scripts from {}", groupName, sourceDiagnostic(*source, std::cref(sourcePath))),
              ExceptionDiagnostic(ran.error()));
          if (!logged)
            return std::unexpected(std::move(logged.error()));
        }
        logged = m_logger.info("Took {} seconds to run {} scripts {}",
            (std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()) - now).count(),
            groupName, *scriptGroupText);
        if (!logged)
          return std::unexpected(std::move(logged.error()));
        if (!memoryAssets->empty()) {
          auto added = addSource(strf("{}::{}", sourcePath, groupName), memoryAssets);
          if (!added)
            return std::unexpected(std::move(added.error()));
        }
      }
    }
    // Load scripts may have populated caches for assets they subsequently changed.
    m_framesSpecifications.clear();
    m_assetsCache.clear();
    return {};
  };

  List<std::pair<String, std::shared_ptr<AssetSource>>> sources;
  for (auto const& sourcePath : m_assetSources) {
    auto logged = m_logger.info("Loading assets from: '{}'", sourcePath);
    if (!logged)
      return std::unexpected(std::move(logged.error()));
    std::shared_ptr<AssetSource> source;
    auto isDirectory = File::isDirectory(sourcePath);
    if (!isDirectory)
      return std::unexpected(std::move(isDirectory.error()));
    if (*isDirectory) {
      auto created = DirectoryAssetSource::create(m_logger, sourcePath, m_settings.pathIgnore);
      if (!created)
        return std::unexpected(std::move(created.error()));
      source = std::move(*created);
    } else {
      auto created = PackedAssetSource::create(sourcePath);
      if (!created)
        return std::unexpected(std::move(created.error()));
      source = std::move(*created);
    }
    auto added = addSource(sourcePath, source);
    if (!added)
      return std::unexpected(std::move(added.error()));
    sources.append(std::make_pair(sourcePath, source));
    auto loaded = runLoadScripts("onLoad", sourcePath, source);
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
  }
  for (auto const& pair : sources) {
    auto loaded = runLoadScripts("postLoad", pair.first, pair.second);
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
  }

  loadScriptState->active = false;

  Sha256Hasher digest;
  StringList digestPaths;
  digestPaths.reserve(m_files.size());
  for (auto const& entry : m_files) {
    auto lower = entry.first.string().toLower();
    if (!lower)
      return std::unexpected(std::move(lower.error()));
    digestPaths.append(std::move(*lower));
  }
  digestPaths.sort();
  for (auto const& assetPath : digestPaths) {
    bool digestFile = true;
    for (auto const& pattern : m_settings.digestIgnore) {
      auto matches = assetPath.regexMatch(pattern, false, false);
      if (!matches)
        return std::unexpected(std::move(matches.error()));
      if (*matches) {
        digestFile = false;
        break;
      }
    }
    auto assetKey = CaseInsensitiveStringView::create(assetPath);
    if (!assetKey)
      return std::unexpected(std::move(assetKey.error()));
    auto descriptor = m_files.get(*assetKey);
    if (!descriptor)
      return std::unexpected(std::move(descriptor.error()));
    if (digestFile) {
      digest.push(assetPath);
      auto device = descriptor->get().source->open(descriptor->get().sourceName);
      if (!device)
        return std::unexpected(std::move(device.error()));
      auto size = (*device)->size();
      if (!size)
        return std::unexpected(std::move(size.error()));
      auto serialized = DataStreamBuffer::serialize(*size);
      if (!serialized)
        return std::unexpected(std::move(serialized.error()));
      digest.push(*serialized);
      for (auto const& pair : descriptor->get().patchSources) {
        auto patchDevice = pair.second->open(AssetPath::removeSubPath(pair.first));
        if (!patchDevice)
          return std::unexpected(std::move(patchDevice.error()));
        auto patchSize = (*patchDevice)->size();
        if (!patchSize)
          return std::unexpected(std::move(patchSize.error()));
        auto serializedPatchSize = DataStreamBuffer::serialize(*patchSize);
        if (!serializedPatchSize)
          return std::unexpected(std::move(serializedPatchSize.error()));
        digest.push(*serializedPatchSize);
      }
    }
  }
  m_digest = digest.compute();

  // The owning factory destroys this object on failure, stopping and joining
  // all workers while their mutexes, Lua engine, and asset sources are alive.
  m_workerThreads.reserve(m_settings.workerPoolSize);
  m_workerStopSource = std::stop_source{};
  for (unsigned i = 0; i < m_settings.workerPoolSize; ++i) {
    auto worker = Thread::invoke("Assets::workerMain", std::mem_fn(&Assets::workerMain), std::ref(*this), m_workerStopSource.get_token());
    if (!worker)
      return std::unexpected(std::move(worker.error()));
    m_workerThreads.append(std::move(*worker));
  }

  // preload.config contains files which will be loaded and then told to persist.
  auto preload = json("/preload.config");
  if (!preload)
    return std::unexpected(std::move(preload.error()));
  auto logged = m_logger.info("Preloading assets");
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  auto preloadScripts = preload->iterateArray();
  if (!preloadScripts)
    return std::unexpected(std::move(preloadScripts.error()));
  constexpr std::array<std::pair<std::string_view, AssetType>, 5> assetTypes{{
      {"json", AssetType::Json},
      {"image", AssetType::Image},
      {"audio", AssetType::Audio},
      {"font", AssetType::Font},
      {"bytes", AssetType::Bytes}
  }};
  for (auto const& script : *preloadScripts) {
    auto typeName = script.getString("type");
    if (!typeName)
      return std::unexpected(std::move(typeName.error()));
    auto lowerTypeName = typeName->toLower();
    if (!lowerTypeName)
      return std::unexpected(std::move(lowerTypeName.error()));
    std::optional<AssetType> type;
    for (auto const& [name, candidate] : assetTypes) {
      if (lowerTypeName->utf8() == name) {
        type = candidate;
        break;
      }
    }
    if (!type)
      return std::unexpected(AssetException::format("Unknown asset type '{}' in preload.config", *typeName));
    auto path = script.getString("path");
    if (!path)
      return std::unexpected(std::move(path.error()));
    auto components = AssetPath::split(*path);
    auto validated = validatePath(components, *type == AssetType::Json || *type == AssetType::Image, *type == AssetType::Image);
    if (!validated)
      return std::unexpected(std::move(validated.error()));
    auto asset = getAsset(AssetId{.type = *type, .path = std::move(components)});
    if (!asset)
      return std::unexpected(std::move(asset.error()));
    (*asset)->forcePersist = true;
  }
  return {};
}

Assets::~Assets() {
  if (auto result = stopWorkers(); !result) {
    if (auto logged = m_logger.errorException("Asset worker failed during shutdown", ExceptionDiagnostic(result.error())); !logged) {
      EmergencyLog::error("Asset worker failed during shutdown: {}", exceptionReport(result.error()));
      EmergencyLog::error("Asset worker shutdown logging failed: {}", exceptionReport(logged.error()));
    }
  }
}

std::expected<void, StarException> Assets::stopWorkers() {
  {
    std::lock_guard locker(m_assetsMutex);
    m_workerStopSource.request_stop();
    m_assetsQueued.notify_all();
    m_assetsDone.notify_all();
  }
  std::optional<StarException> error;
  for (auto& worker : m_workerThreads) {
    auto result = worker.finish();
    if (!result && !error)
      error = std::move(result.error());
  }
  m_workerThreads.clear();
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

void Assets::hotReload() const {
  std::lock_guard assetsLocker(m_assetsMutex);
  m_assetsCache.clear();
  m_assetErrors.clear();
  m_queue.clear();
  m_framesSpecifications.clear();
}

StringList Assets::assetSources() const {
  std::lock_guard assetsLocker(m_assetsMutex);
  return m_assetSources;
}

std::expected<JsonObject, StarException> Assets::assetSourceMetadata(String const& sourceName) const {
  std::lock_guard assetsLocker(m_assetsMutex);
  auto source = m_assetSourcePaths.get_right(sourceName);
  if (!source)
    return std::unexpected(std::move(source.error()));
  return source->get()->metadata();
}

ByteArray Assets::digest() const {
  std::lock_guard assetsLocker(m_assetsMutex);
  return m_digest;
}

std::expected<bool, StarException> Assets::assetExists(String const& path) const {
  auto key = CaseInsensitiveStringView::create(path);
  if (!key)
    return std::unexpected(std::move(key.error()));
  std::lock_guard assetsLocker(m_assetsMutex);
  return m_files.contains(*key);
}

std::expected<std::optional<Assets::AssetFileDescriptor>, StarException> Assets::assetDescriptor(String const& path) const {
  auto key = CaseInsensitiveStringView::create(path);
  if (!key)
    return std::unexpected(std::move(key.error()));
  std::lock_guard assetsLocker(m_assetsMutex);
  return m_files.maybe(*key);
}

std::expected<String, StarException> Assets::assetSource(String const& path) const {
  auto key = CaseInsensitiveStringView::create(path);
  if (!key)
    return std::unexpected(std::move(key.error()));
  std::lock_guard assetsLocker(m_assetsMutex);
  if (auto p = m_files.find_ref(*key)) {
    auto source = m_assetSourcePaths.get_left(p->get().source);
    if (!source)
      return std::unexpected(std::move(source.error()));
    return source->get();
  }
  return std::unexpected(AssetException(strf("No such asset '{}'", path)));
}

std::optional<String> Assets::assetSourcePath(std::shared_ptr<AssetSource> const& source) const {
  std::lock_guard assetsLocker(m_assetsMutex);
  return m_assetSourcePaths.maybe_left(source);
}

std::expected<StringList, StarException> Assets::scan(String const& suffix) const {
  auto suffixBytes = std::string_view(suffix.utf8());
  if (suffixBytes.starts_with(".") && suffixBytes.find('.', 1) == std::string_view::npos) {
    auto extension = scanExtension(suffix);
    if (!extension)
      return std::unexpected(std::move(extension.error()));
    StringList result;
    result.reserve(extension->get().size());
    for (auto const& key : extension->get())
      result.append(key.string());
    return result;
  }
  StringList result;
  for (auto const& fileEntry : m_files) {
    auto const& path = fileEntry.first.string();
    if (suffix.empty()) {
      result.append(path);
      continue;
    }
    auto match = path.endsWith(suffix, String::CaseInsensitive);
    if (!match)
      return std::unexpected(std::move(match.error()));
    if (*match)
      result.append(path);
  }
  return result;
}

std::expected<StringList, StarException> Assets::scan(String const& prefix, String const& suffix) const {
  auto candidates = scan(suffix);
  if (!candidates)
    return std::unexpected(std::move(candidates.error()));
  StringList result;
  for (auto const& file : *candidates) {
    auto match = file.beginsWith(prefix, String::CaseInsensitive);
    if (!match)
      return std::unexpected(std::move(match.error()));
    if (*match)
      result.append(file);
  }
  return result;
}

const CaseInsensitiveStringSet NullExtensionScan;

std::expected<std::reference_wrapper<CaseInsensitiveStringSet const>, StarException> Assets::scanExtension(String const& extension) const {
  auto bytes = std::string_view(extension.utf8());
  // The extension separator is ASCII; stripping it never splits a UTF8 scalar.
  auto key = CaseInsensitiveStringView::create(bytes.starts_with(".") ? bytes.substr(1) : bytes);
  if (!key)
    return std::unexpected(std::move(key.error()));
  auto find = m_filesByExtension.find(*key);
  return std::cref(find != m_filesByExtension.end() ? find->second : NullExtensionScan);
}

std::expected<Json, StarException> Assets::json(String const& path) const {
  auto components = AssetPath::split(path);
  if (auto valid = validatePath(components, true, false); !valid)
    return std::unexpected(std::move(valid.error()));
  auto data = getAsset(AssetId{.type = AssetType::Json, .path = std::move(components)});
  if (!data)
    return std::unexpected(std::move(data.error()));
  return asRef<JsonData>(**data)->get().json;
}

std::expected<Json, StarException> Assets::fetchJson(Json const& v, String const& dir) const {
  if (!v.isType(Json::Type::String))
    return v;
  auto path = v.toString();
  if (!path)
    return std::unexpected(std::move(path.error()));
  return json(AssetPath::relativeTo(dir, *path));
}

std::expected<void, StarException> Assets::queueJsons(StringList const& paths) const {
  List<AssetId> ids;
  for (auto const& path : paths) {
    auto components = AssetPath::split(path);
    if (auto valid = validatePath(components, true, false); !valid)
      return std::unexpected(std::move(valid.error()));
    ids.append({.type = AssetType::Json, .path = {std::move(components.basePath), {}, {}}});
  }
  queueAssets(ids);
  return {};
}

std::expected<void, StarException> Assets::queueJsons(CaseInsensitiveStringSet const& paths) const {
  std::lock_guard assetsLocker(m_assetsMutex);
  for (auto const& path : paths) {
    auto components = AssetPath::split(path.string());
    if (auto valid = validatePath(components, true, false); !valid)
      return std::unexpected(std::move(valid.error()));
    if (queueAsset({.type = AssetType::Json, .path = {std::move(components.basePath), {}, {}}}))
      m_assetsQueued.notify_one();
  }
  return {};
}

std::expected<std::shared_ptr<Image const>, StarException> Assets::image(AssetPath const& path) const {
  auto data = getAsset({.type = AssetType::Image, .path = path});
  if (!data)
    return std::unexpected(std::move(data.error()));
  return asRef<ImageData>(**data)->get().image;
}

std::expected<void, StarException> Assets::queueImages(StringList const& paths) const {
  List<AssetId> ids;
  for (auto const& path : paths) {
    auto components = AssetPath::split(path);
    if (auto valid = validatePath(components, true, true); !valid)
      return std::unexpected(std::move(valid.error()));
    ids.append({.type = AssetType::Image, .path = std::move(components)});
  }
  queueAssets(ids);
  return {};
}

std::expected<void, StarException> Assets::queueImages(CaseInsensitiveStringSet const& paths) const {
  std::lock_guard assetsLocker(m_assetsMutex);
  for (auto const& path : paths) {
    auto components = AssetPath::split(path.string());
    if (auto valid = validatePath(components, true, true); !valid)
      return std::unexpected(std::move(valid.error()));
    if (queueAsset({.type = AssetType::Image, .path = std::move(components)}))
      m_assetsQueued.notify_one();
  }
  return {};
}

std::expected<std::shared_ptr<Image const>, StarException> Assets::tryImage(AssetPath const& path) const {
  auto data = tryAsset({.type = AssetType::Image, .path = path});
  if (!data)
    return std::unexpected(std::move(data.error()));
  if (*data)
    return asRef<ImageData>(**data)->get().image;
  return std::shared_ptr<Image const>{};
}

std::expected<std::shared_ptr<FramesSpecification const>, StarException> Assets::imageFrames(String const& path) const {
  auto components = AssetPath::split(path);
  if (auto valid = validatePath(components, false, false); !valid)
    return std::unexpected(std::move(valid.error()));
  std::unique_lock assetsLocker(m_assetsMutex);
  return bestFramesSpecification(path);
}

std::expected<std::shared_ptr<Audio const>, StarException> Assets::audio(String const& path) const {
  auto components = AssetPath::split(path);
  if (auto valid = validatePath(components, false, false); !valid)
    return std::unexpected(std::move(valid.error()));
  auto data = getAsset({.type = AssetType::Audio, .path = std::move(components)});
  if (!data)
    return std::unexpected(std::move(data.error()));
  return asRef<AudioData>(**data)->get().audio;
}

std::expected<void, StarException> Assets::queueAudios(StringList const& paths) const {
  List<AssetId> ids;
  for (auto const& path : paths) {
    auto components = AssetPath::split(path);
    if (auto valid = validatePath(components, false, false); !valid)
      return std::unexpected(std::move(valid.error()));
    ids.append({.type = AssetType::Audio, .path = std::move(components)});
  }
  queueAssets(ids);
  return {};
}

std::expected<void, StarException> Assets::queueAudios(CaseInsensitiveStringSet const& paths) const {
  std::lock_guard assetsLocker(m_assetsMutex);
  for (auto const& path : paths) {
    auto components = AssetPath::split(path.string());
    if (auto valid = validatePath(components, false, true); !valid)
      return std::unexpected(std::move(valid.error()));
    if (queueAsset({.type = AssetType::Audio, .path = std::move(components)}))
      m_assetsQueued.notify_one();
  }
  return {};
}

std::expected<std::shared_ptr<Audio const>, StarException> Assets::tryAudio(String const& path) const {
  auto components = AssetPath::split(path);
  if (auto valid = validatePath(components, false, false); !valid)
    return std::unexpected(std::move(valid.error()));
  auto data = tryAsset({.type = AssetType::Audio, .path = std::move(components)});
  if (!data)
    return std::unexpected(std::move(data.error()));
  if (*data)
    return asRef<AudioData>(**data)->get().audio;
  return std::shared_ptr<Audio const>{};
}

std::expected<std::shared_ptr<Font const>, StarException> Assets::font(String const& path) const {
  auto components = AssetPath::split(path);
  if (auto valid = validatePath(components, false, false); !valid)
    return std::unexpected(std::move(valid.error()));
  auto data = getAsset({.type = AssetType::Font, .path = std::move(components)});
  if (!data)
    return std::unexpected(std::move(data.error()));
  return asRef<FontData>(**data)->get().font;
}

std::expected<std::shared_ptr<ByteArray const>, StarException> Assets::bytes(String const& path) const {
  auto components = AssetPath::split(path);
  if (auto valid = validatePath(components, false, false); !valid)
    return std::unexpected(std::move(valid.error()));
  auto data = getAsset({.type = AssetType::Bytes, .path = std::move(components)});
  if (!data)
    return std::unexpected(std::move(data.error()));
  return asRef<BytesData>(**data)->get().bytes;
}

std::expected<std::shared_ptr<IODevice>, StarException> Assets::openFile(String const& path) const {
  return open(path);
}

void Assets::clearCache() {
  std::lock_guard assetsLocker(m_assetsMutex);

  // Clear all assets that are not queued or broken.
  for (auto it = m_assetsCache.begin(); it != m_assetsCache.end();) {
    auto const& pair = *it;
    // Don't clean up queued, persistent, or broken assets.
    if (pair.second && !pair.second->shouldPersist() && !m_queue.contains(pair.first))
      it = m_assetsCache.erase(it);
    else
      ++it;
  }
}

void Assets::cleanup() {
  std::lock_guard assetsLocker(m_assetsMutex);

  auto time = std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now());
  HashSet<AssetData const*> keepLonger;
  if (m_settings.referenceImageTimeToLive > m_settings.assetTimeToLive && m_settings.referenceImagePixelBudget != 0) {
    std::vector<ImageData const*> references;
    for (auto const& entry : m_assetsCache) {
      if (entry.first.type == AssetType::Image && entry.second) {
        auto const& image = asRef<ImageData>(*entry.second)->get();
        if (image.isReference && image.image)
          references.push_back(&image);
      }
    }
    std::sort(references.begin(), references.end(),
              [](ImageData const* a, ImageData const* b) { return a->time > b->time; });
    std::uint64_t pixels = 0;
    for (auto image : references) {
      auto size = std::uint64_t(image->image->width()) * image->image->height();
      if (size > m_settings.referenceImagePixelBudget - pixels)
        continue;
      pixels += size;
      keepLonger.add(image);
    }
  }

  for (auto it = m_assetsCache.begin(); it != m_assetsCache.end();) {
    auto const& pair = *it;
    // Don't clean up broken assets or queued assets.
    if (pair.second && !m_queue.contains(pair.first)) {
      auto liveTime = time - pair.second->time;
      auto timeToLive = std::chrono::duration<float>(
        keepLonger.contains(pair.second.get()) ? m_settings.referenceImageTimeToLive : m_settings.assetTimeToLive);
      if (liveTime > timeToLive) {
        // If the asset should persist, just refresh the access time.
        if (pair.second->shouldPersist())
          pair.second->time = time;
        else {
          it = m_assetsCache.erase(it);
          continue;
        }
      }
    }
    ++it;
  }
}

bool Assets::AssetId::operator==(AssetId const& assetId) const {
  return std::tie(type, path) == std::tie(assetId.type, assetId.path);
}

std::size_t Assets::AssetIdHash::operator()(AssetId const& id) const {
  return hashOf(id.type, id.path.basePath, id.path.subPath, id.path.directives);
}

bool Assets::JsonData::shouldPersist() const {
  return forcePersist || !json.unique();
}

bool Assets::ImageData::shouldPersist() const {
  return forcePersist || (!alias && image.use_count() != 1);
}

bool Assets::AudioData::shouldPersist() const {
  return forcePersist || audio.use_count() != 1;
}

bool Assets::FontData::shouldPersist() const {
  return forcePersist || font.use_count() != 1;
}

bool Assets::BytesData::shouldPersist() const {
  return forcePersist || bytes.use_count() != 1;
}

std::expected<FramesSpecification, StarException> Assets::parseFramesSpecification(Json const& frameConfig, String path) {
  FramesSpecification spec;
  spec.framesFile = std::move(path);
  auto frameList = frameConfig.find("frameList");
  if (!frameList)
    return std::unexpected(std::move(frameList.error()));
  if (*frameList) {
    auto list = (*frameList)->get().iterateObject();
    if (!list)
      return std::unexpected(std::move(list.error()));
    for (auto const& pair : *list) {
      auto parsedRect = jsonToRectI(pair.second);
      if (!parsedRect)
        return std::unexpected(std::move(parsedRect.error()));
      RectU rect(*parsedRect);
      if (rect.isEmpty())
        return std::unexpected(AssetException(strf("Empty rect in frame specification in image {} frame {}", spec.framesFile, pair.first)));
      spec.frames[pair.first] = rect;
    }
  }
  auto frameGrid = frameConfig.find("frameGrid");
  if (!frameGrid)
    return std::unexpected(std::move(frameGrid.error()));
  if (*frameGrid) {
    auto gridResult = (*frameGrid)->get().objectPtr();
    if (!gridResult)
      return std::unexpected(std::move(gridResult.error()));
    auto const& grid = **gridResult;
    Vec2I beginValue{};
    auto beginResult = grid.find_ref("begin");
    if (beginResult) {
      auto parsedBegin = jsonToVec2I(beginResult->get());
      if (!parsedBegin)
        return std::unexpected(std::move(parsedBegin.error()));
      beginValue = *parsedBegin;
    }
    auto sizeConfig = grid.get("size");
    if (!sizeConfig)
      return std::unexpected(std::move(sizeConfig.error()));
    auto sizeResult = jsonToVec2I(sizeConfig->get());
    if (!sizeResult)
      return std::unexpected(std::move(sizeResult.error()));
    auto dimensionsConfig = grid.get("dimensions");
    if (!dimensionsConfig)
      return std::unexpected(std::move(dimensionsConfig.error()));
    auto dimensionsResult = jsonToVec2I(dimensionsConfig->get());
    if (!dimensionsResult)
      return std::unexpected(std::move(dimensionsResult.error()));
    Vec2U begin(beginValue), size(*sizeResult), dimensions(*dimensionsResult);
    if (dimensions[0] == 0 || dimensions[1] == 0)
      return std::unexpected(AssetException(strf("Image {} \"dimensions\" in frameGrid cannot be zero", spec.framesFile)));
    if (auto namesConfig = grid.find_ref("names")) {
      auto names = namesConfig->get().arrayPtr();
      if (!names)
        return std::unexpected(std::move(names.error()));
      for (std::size_t y = 0; y < (*names)->size(); ++y) {
        if (y >= dimensions[1])
          return std::unexpected(AssetException(strf("Image {} row {} is out of bounds for y-dimension {}", spec.framesFile, y + 1, dimensions[1])));
        auto const& rowConfig = (**names)[y];
        if (rowConfig.isNull())
          continue;
        auto row = rowConfig.arrayPtr();
        if (!row)
          return std::unexpected(std::move(row.error()));
        for (std::size_t x = 0; x < (*row)->size(); ++x) {
          if (x >= dimensions[0])
            return std::unexpected(AssetException(strf("Image {} column {} is out of bounds for x-dimension {}", spec.framesFile, x + 1, dimensions[0])));
          auto const& frame = (**row)[x];
          if (frame.isNull())
            continue;
          auto name = frame.toString();
          if (!name)
            return std::unexpected(std::move(name.error()));
          if (!name->empty())
            spec.frames[*name] = RectU::withSize(Vec2U(begin[0] + x * size[0], begin[1] + y * size[1]), size);
        }
      }
    } else {
      for (std::size_t y = 0; y < dimensions[1]; ++y)
        for (std::size_t x = 0; x < dimensions[0]; ++x)
          spec.frames[toString(y * dimensions[0] + x)] =
            RectU::withSize(Vec2U(begin[0] + x * size[0], begin[1] + y * size[1]), size);
    }
  }
  auto aliasesConfig = frameConfig.opt("aliases");
  if (!aliasesConfig)
    return std::unexpected(std::move(aliasesConfig.error()));
  if (*aliasesConfig) {
    auto aliases = (**aliasesConfig).toObject();
    if (!aliases)
      return std::unexpected(std::move(aliases.error()));
    for (auto const& pair : *aliases) {
      auto parsedValue = pair.second.toString();
      if (!parsedValue)
        return std::unexpected(std::move(parsedValue.error()));
      String value = std::move(*parsedValue);
      for (std::size_t i = 0; i <= aliases->size(); ++i) {
        auto alias = aliases->find_ref(value);
        if (!alias)
          break;
        if (i == aliases->size())
          return std::unexpected(AssetException(strf("Infinite alias loop detected for alias '{}'", pair.first)));
        auto next = alias->get().toString();
        if (!next)
          return std::unexpected(std::move(next.error()));
        value = std::move(*next);
      }
      if (!spec.frames.contains(value))
        return std::unexpected(AssetException(strf("No such frame '{}' found for alias '{}'", value, pair.first)));
      spec.aliases[pair.first] = std::move(value);
    }
  }
  return spec;
}

void Assets::queueAssets(List<AssetId> const& assetIds) const {
  std::lock_guard assetsLocker(m_assetsMutex);

  std::size_t queued = 0;
  for (auto const& id : assetIds)
    queued += queueAsset(id);
  if (queued == 1)
    m_assetsQueued.notify_one();
  else if (queued > 1)
    m_assetsQueued.notify_all();
}

bool Assets::queueAsset(AssetId const& assetId) const {
  auto i = m_assetsCache.find(assetId);
  if (i != m_assetsCache.end()) {
    if (i->second)
      freshen(i->second);
  } else {
    auto j = m_queue.find(assetId);
    if (j == m_queue.end()) {
      m_queue[assetId] = QueuePriority::Load;
      return true;
    }
  }
  return false;
}

std::expected<std::shared_ptr<Assets::AssetData>, StarException> Assets::tryAsset(AssetId const& id) const {
  std::lock_guard assetsLocker(m_assetsMutex);
  if (m_workerError)
    return std::unexpected(*m_workerError);
  if (m_workerStopSource.stop_requested())
    return std::unexpected(AssetException("Asset loading stopped"));
  if (auto error = m_assetErrors.find_ref(id))
    return std::unexpected(error->get());
  if (auto cached = m_assetsCache.find_ref(id)) {
    if (cached->get())
      freshen(cached->get());
    return cached->get();
  }
  if (!m_queue.contains(id)) {
    m_queue[id] = QueuePriority::Load;
    m_assetsQueued.notify_one();
  }
  return std::shared_ptr<AssetData>{};
}

std::expected<std::shared_ptr<Assets::AssetData>, StarException> Assets::getAsset(AssetId const& id) const {
  std::unique_lock assetsLocker(m_assetsMutex);
  while (true) {
    if (m_workerError)
      return std::unexpected(*m_workerError);
    if (m_workerStopSource.stop_requested())
      return std::unexpected(AssetException("Asset loading stopped"));
    if (auto error = m_assetErrors.find_ref(id))
      return std::unexpected(error->get());
    if (auto cached = m_assetsCache.find_ref(id)) {
      if (cached->get()) {
        freshen(cached->get());
        return cached->get();
      }
      return std::unexpected(AssetException::format("Error loading asset {}", id.path));
    }
    auto worked = doLoad(id);
    if (!worked)
      return std::unexpected(std::move(worked.error()));
    if (!*worked && !m_workerStopSource.stop_requested())
      m_assetsDone.wait(assetsLocker);
  }
}

std::expected<void, StarException> Assets::workerMain(std::stop_token stopToken) {
  while (true) {
    std::unique_lock assetsLocker(m_assetsMutex);
    if (stopToken.stop_requested())
      break;

    AssetId assetId;
    QueuePriority queuePriority = QueuePriority::None;

    // Find the highest priority queue entry
    for (auto const& pair : m_queue) {
      if (pair.second == QueuePriority::Load || pair.second == QueuePriority::PostProcess) {
        assetId = pair.first;
        queuePriority = pair.second;
        if (pair.second == QueuePriority::Load)
          break;
      }
    }

    if (queuePriority != QueuePriority::Load && queuePriority != QueuePriority::PostProcess) {
      // Only one worker collects a fully drained load burst. In particular,
      // queued Working entries still need the shared Lua VM, not an idle GC.
      if (m_queue.empty() && m_luaGcPending) {
        m_luaGcPending = false;
        assetsLocker.unlock();
        std::expected<void, StarException> collected;
        {
          std::unique_lock luaLocker(m_luaMutex, std::defer_lock);
          lockWithDiagnostics(luaLocker);
          collected = asRef<LuaEngine>(*m_luaEngine)->get().collectGarbage();
        }
        assetsLocker.lock();
        if (!collected) {
          if (!m_workerError)
            m_workerError = collected.error();
          m_workerStopSource.request_stop();
          m_assetsQueued.notify_all();
          m_assetsDone.notify_all();
          return std::unexpected(std::move(collected.error()));
        }
      }
      // Queueing and shutdown can happen while the asset mutex is released.
      m_assetsQueued.wait(assetsLocker, [&] {
        if (stopToken.stop_requested())
          return true;
        if (m_queue.empty() && m_luaGcPending)
          return true;
        for (auto const& entry : m_queue)
          if (entry.second == QueuePriority::Load || entry.second == QueuePriority::PostProcess)
            return true;
        return false;
      });
      continue;
    }

    auto worked = queuePriority == QueuePriority::PostProcess ? doPost(assetId) : doLoad(assetId);
    if (!worked) {
      if (!m_workerError)
        m_workerError = worked.error();
      m_workerStopSource.request_stop();
      m_assetsQueued.notify_all();
      m_assetsDone.notify_all();
      return std::unexpected(std::move(worked.error()));
    }
    if (!*worked && !stopToken.stop_requested()) {
      // We are blocking on some sort of busy asset, so need to wait on
      // something to complete here, rather than spinning and burning cpu.
      m_assetsDone.wait(assetsLocker);
      continue;
    }

    // After processing an asset, unlock the main asset mutex and yield so we
    // don't starve other threads.
    assetsLocker.unlock();
    std::this_thread::yield();
  }
  return {};
}

template <typename Function>
[[nodiscard]] decltype(auto) Assets::unlockDuring(Function f) const {
  // Borrow the caller's ownership: checked callbacks return before we restore
  // the mutex, including on failure, so its guard never outlives an unlocked mutex.
  m_assetsMutex.unlock();
  auto result = f();
  m_assetsMutex.lock();
  return result;
}

std::expected<std::shared_ptr<FramesSpecification const>, StarException> Assets::bestFramesSpecification(String const& image) const {
  if (auto cached = m_framesSpecifications.find_ref(image))
    return cached->get();
  String framesFile;
  if (auto cached = m_bestFramesFiles.find_ref(image)) {
    framesFile = cached->get();
  } else {
    String searchPath = AssetPath::directory(image);
    auto filename = AssetPath::filename(image);
    auto filenameBytes = std::string_view(filename.utf8());
    String filePrefix(filenameBytes.substr(0, filenameBytes.rfind('.')));
    while (!searchPath.empty()) {
      auto candidate = searchPath + filePrefix + ".frames";
      auto candidateKey = CaseInsensitiveStringView::create(candidate);
      if (!candidateKey)
        return std::unexpected(std::move(candidateKey.error()));
      if (!m_files.contains(*candidateKey)) {
        candidate = searchPath + "default.frames";
        candidateKey = CaseInsensitiveStringView::create(candidate);
        if (!candidateKey)
          return std::unexpected(std::move(candidateKey.error()));
      }
      if (m_files.contains(*candidateKey)) {
        framesFile = std::move(candidate);
        m_bestFramesFiles[image] = framesFile;
        break;
      }
      auto directory = std::string_view(searchPath.utf8());
      // searchPath ends in an ASCII slash. Ascend without decoding filenames.
      directory.remove_suffix(1);
      auto slash = directory.rfind('/');
      searchPath = slash == std::string_view::npos ? String{} : String(directory.substr(0, slash + 1));
    }
    if (framesFile.empty())
      return std::shared_ptr<FramesSpecification const>{};
  }
  auto parsed = unlockDuring([&]() -> std::expected<std::shared_ptr<FramesSpecification const>, StarException> {
    auto config = readJson(framesFile);
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto spec = parseFramesSpecification(*config, framesFile);
    if (!spec)
      return std::unexpected(std::move(spec.error()));
    return std::make_shared<FramesSpecification>(std::move(*spec));
  });
  if (!parsed)
    return std::unexpected(std::move(parsed.error()));
  m_framesSpecifications[image] = *parsed;
  return std::move(*parsed);
}

std::expected<std::shared_ptr<IODevice>, StarException> Assets::open(String const& path) const {
  auto key = CaseInsensitiveStringView::create(path);
  if (!key)
    return std::unexpected(std::move(key.error()));
  if (auto p = m_files.find_ref(*key))
    return p->get().source->open(p->get().sourceName);
  return std::unexpected(AssetException(strf("No such asset '{}'", path)));
}

std::expected<ByteArray, StarException> Assets::read(String const& path) const {
  auto key = CaseInsensitiveStringView::create(path);
  if (!key)
    return std::unexpected(std::move(key.error()));
  if (auto p = m_files.find_ref(*key))
    return p->get().source->read(p->get().sourceName);
  return std::unexpected(AssetException(strf("No such asset '{}'", path)));
}

std::expected<std::shared_ptr<Image const>, StarException> Assets::readImage(String const& path) const {
  auto key = CaseInsensitiveStringView::create(path);
  if (!key)
    return std::unexpected(std::move(key.error()));
  auto file = m_files.find_ref(*key);
  if (!file)
    return std::unexpected(AssetException(strf("No such asset '{}'", path)));
  std::shared_ptr<Image const> image;
  if (auto memorySource = asRef<MemoryAssetSource>(*file->get().source)) {
    auto stored = memorySource->get().image(file->get().sourceName);
    if (!stored)
      return std::unexpected(std::move(stored.error()));
    image = std::move(*stored);
  }
  if (!image) {
    auto device = file->get().source->open(file->get().sourceName);
    if (!device)
      return std::unexpected(std::move(device.error()));
    auto decoded = Image::readPng(m_logger, std::move(*device));
    if (!decoded)
      return std::unexpected(std::move(decoded.error()));
    image = std::make_shared<Image>(std::move(*decoded));
  }
  if (!file->get().patchSources.empty())
    return applyImagePatches(std::move(image), path, file->get().patchSources);
  return image;
}

std::expected<std::shared_ptr<Image const>, StarException> Assets::applyImagePatches(std::shared_ptr<Image const> image, String const& path, List<std::pair<String, std::shared_ptr<AssetSource>>> patches) const {
  std::unique_lock luaLocker(m_luaMutex, std::defer_lock);
  lockWithDiagnostics(luaLocker);
  auto& luaEngine = asRef<LuaEngine>(*m_luaEngine)->get();
  auto imageUserData = luaEngine.createUserData(*image);
  if (!imageUserData)
    return std::unexpected(std::move(imageUserData.error()));
  LuaValue result = std::move(*imageUserData);
  luaLocker.unlock();
  for (auto const& pair : patches) {
    auto& patchPath = pair.first;
    auto& patchSource = pair.second;
    auto patchStream = readPatch(*patchSource, patchPath, m_logger);
    if (!patchStream)
      return std::unexpected(std::move(patchStream.error()));
    if (!*patchStream)
      continue;
    auto isLua = patchPath.endsWith(".lua");
    if (!isLua)
      return std::unexpected(std::move(isLua.error()));
    if (*isLua) {
      PatchContextKey contextKey{.source = std::ref(*patchSource), .path = patchPath};
      lockWithDiagnostics(luaLocker);
      std::shared_ptr<LuaContext>& context = m_patchContexts[contextKey];
      if (!context) {
        auto createdContext = luaEngine.createContext();
        if (!createdContext)
          return std::unexpected(std::move(createdContext.error()));
        auto newContext = std::make_shared<LuaContext>(std::move(*createdContext));
        auto loaded = newContext->load((**patchStream).span(), patchPath);
        if (!loaded)
          return std::unexpected(luaPatchFailure(std::move(loaded.error()), path, patchPath, *patchSource,
                                                 m_assetSourcePaths.find_left_ref(patchSource)));
        context = std::move(newContext);
      }
      auto newResult = context->invokePath<LuaValue>("patch", result, path);
      if (!newResult)
        return std::unexpected(luaPatchFailure(std::move(newResult.error()), path, patchPath, *patchSource,
                                               m_assetSourcePaths.find_left_ref(patchSource)));
      if (!std::holds_alternative<LuaNilType>(*newResult)) {
        if (std::holds_alternative<LuaUserData>(*newResult)) {
          auto isImage = std::get<LuaUserData>(*newResult).is<Image>();
          if (!isImage)
            return std::unexpected(std::move(isImage.error()));
          if (*isImage) {
            result = std::move(*newResult);
          } else {
            auto logged = m_logger.warn("Patch '{}' for image '{}' returned a non-Image userdata value, ignoring", patchPath, path);
            if (!logged)
              return std::unexpected(std::move(logged.error()));
          }
        } else {
          auto logged = m_logger.warn("Patch '{}' for image '{}' returned a non-Image value, ignoring", patchPath, path);
          if (!logged)
            return std::unexpected(std::move(logged.error()));
        }
      }
      luaLocker.unlock();
    } else {
      auto logged = m_logger.warn("Patch '{}' for image '{}' isn't a Lua script, ignoring", patchPath, path);
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    }
  }
  auto resultImage = std::get<LuaUserData>(result).get<Image>();
  if (!resultImage)
    return std::unexpected(std::move(resultImage.error()));
  return std::make_shared<Image>(std::move(resultImage->get()));
}

std::expected<void, StarException> Assets::reportPatchRejection(String const& path,
    std::shared_ptr<AssetSource> const& source, StarException const& error) const {
  auto provider = sourceDiagnostic(*source, m_assetSourcePaths.find_left_ref(source));
  if (error.is<JsonPatchTestFail>())
    return m_logger.logException(LogLevel::Debug, strf("Patch test failure from file {} in {}", path, provider),
        ExceptionDiagnostic(error));
  return m_logger.errorException(strf("Could not apply patch from file {} in {}", path, provider),
      ExceptionDiagnostic(error));
}

std::expected<Json, StarException> Assets::checkPatchArray(String const& path,
                                                           std::shared_ptr<AssetSource> const& source,
                                                           Json const result, JsonArray const& patchData,
                                                           std::optional<Json> const external) const {
  auto externalRef = external.value_or(Json{});
  auto newResult = result;
  for (auto const& patch : patchData) {
    switch (patch.type()) {
      case Json::Type::Array: { // Nested groups commit only if the whole group succeeds.
        auto patchData = patch.arrayPtr();
        if (!patchData)
          return std::unexpected(std::move(patchData.error()));
        auto patched = checkPatchArray(path, source, newResult, **patchData, externalRef);
        if (patched) {
          newResult = std::move(*patched);
        } else {
          auto const& error = patched.error();
          if (!error.is<JsonPatchTestFail>() && !error.is<JsonPatchException>())
            return std::unexpected(std::move(patched.error()));
          auto logged = reportPatchRejection(path, source, error);
          if (!logged)
            return std::unexpected(std::move(logged.error()));
        }
        break;
      }
      case Json::Type::Object: {
        auto patched = JsonPatching::applyOperation(newResult, patch, externalRef);
        if (!patched)
          return std::unexpected(std::move(patched.error()));
        newResult = std::move(*patched);
        break;
      }
      case Json::Type::String: {
        auto referencePath = patch.toString();
        if (!referencePath)
          return std::unexpected(std::move(referencePath.error()));
        auto reference = json(*referencePath);
        if (!reference)
          return std::unexpected(JsonPatchTestFail(strf("Unable to load reference asset: {}", *referencePath), reference.error()));
        externalRef = std::move(*reference);
        break;
      }
      default:
        return std::unexpected(JsonPatchException(strf("Patch data is wrong type: {}", Json::typeName(patch.type()))));
    }
  }
  return newResult;
}

std::expected<Json, StarException> Assets::readJson(String const& path) const {
  auto streamData = read(path);
  if (!streamData)
    return std::unexpected(std::move(streamData.error()));
  auto parsed = inputUtf8Json(streamData->begin(), streamData->end(), JsonParseType::Top);
  if (!parsed)
    return std::unexpected(JsonParsingException(strf("Cannot parse json file: {}", path), parsed.error()));
  auto key = CaseInsensitiveStringView::create(path);
  if (!key)
    return std::unexpected(JsonParsingException(strf("Cannot parse json file: {}", path), key.error()));
  auto file = m_files.get(*key);
  if (!file)
    return std::unexpected(JsonParsingException(strf("Cannot parse json file: {}", path), file.error()));
  auto patched = applyJsonPatches(*parsed, path, file->get().patchSources);
  if (!patched)
    return std::unexpected(JsonParsingException(strf("Cannot parse json file: {}", path), patched.error()));
  return std::move(*patched);
}

std::expected<Json, StarException> Assets::applyJsonPatches(Json const& input, String const& path, List<std::pair<String, std::shared_ptr<AssetSource>>> patches) const {
  Json result = input;
  for (auto const& pair : patches) {
    auto patchAssetPath = AssetPath::split(pair.first);
    auto& patchBasePath = patchAssetPath.basePath;
    auto& patchSource = pair.second;
    auto patchStream = readPatch(*patchSource, patchBasePath, m_logger);
    if (!patchStream)
      return std::unexpected(std::move(patchStream.error()));
    if (!*patchStream)
      continue;
    auto isLua = patchBasePath.endsWith(".lua");
    if (!isLua)
      return std::unexpected(std::move(isLua.error()));
    if (*isLua) {
      PatchContextKey contextKey{.source = std::ref(*patchSource), .path = patchBasePath};
      std::unique_lock luaLocker(m_luaMutex, std::defer_lock);
      lockWithDiagnostics(luaLocker);
      std::shared_ptr<LuaContext>& context = m_patchContexts[contextKey];
      if (!context) {
        auto createdContext = asRef<LuaEngine>(*m_luaEngine)->get().createContext();
        if (!createdContext)
          return std::unexpected(std::move(createdContext.error()));
        auto newContext = std::make_shared<LuaContext>(std::move(*createdContext));
        auto loaded = newContext->load((**patchStream).span(), patchBasePath);
        if (!loaded)
          return std::unexpected(luaPatchFailure(std::move(loaded.error()), path, pair.first, *patchSource,
                                                 m_assetSourcePaths.find_left_ref(patchSource)));
        context = std::move(newContext);
      }
      auto newResult = context->invokePath<Json>("patch", result, path);
      if (!newResult)
        return std::unexpected(luaPatchFailure(std::move(newResult.error()), path, pair.first, *patchSource,
                                               m_assetSourcePaths.find_left_ref(patchSource)));
      if (*newResult)
        result = std::move(*newResult);
    } else {
      auto applyPatch = [&]() -> std::expected<void, StarException> {
        auto& bytes = **patchStream;
        auto patchJson = inputUtf8Json(bytes.begin(), bytes.end(), JsonParseType::Top);
        if (!patchJson)
          return std::unexpected(std::move(patchJson.error()));
        if (patchAssetPath.subPath) {
          auto selected = patchJson->query(*patchAssetPath.subPath);
          if (!selected)
            return std::unexpected(std::move(selected.error()));
          *patchJson = std::move(*selected);
        }
        if (patchJson->isType(Json::Type::Array)) {
          auto patchData = patchJson->arrayPtr();
          if (!patchData)
            return std::unexpected(std::move(patchData.error()));
          auto patched = checkPatchArray(pair.first, patchSource, result, **patchData, {});
          if (patched) {
            result = std::move(*patched);
          } else {
            auto const& error = patched.error();
            if (!error.is<JsonPatchTestFail>() && !error.is<JsonPatchException>())
              return std::unexpected(std::move(patched.error()));
            auto logged = reportPatchRejection(pair.first, patchSource, error);
            if (!logged)
              return std::unexpected(std::move(logged.error()));
          }
        } else if (patchJson->isType(Json::Type::Object)) {
          result = jsonMergeNulling(result, *patchJson);
        }
        return {};
      };
      auto applied = applyPatch();
      if (!applied) {
        return std::unexpected(
          JsonParsingException(strf("Cannot parse json patch file '{}' from {}", patchBasePath,
                                    sourceDiagnostic(*patchSource, m_assetSourcePaths.find_left_ref(patchSource))),
                               applied.error()));
      }
    }
  }
  return result;
}

std::expected<bool, StarException> Assets::doLoad(AssetId const& id) const {
  auto loaded = loadAsset(id);
  if (loaded)
    return static_cast<bool>(*loaded);
  auto logged = m_logger.errorException(strf("Error loading asset: {}", id.path), ExceptionDiagnostic(loaded.error()));
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  return true;
}

std::expected<bool, StarException> Assets::doPost(AssetId const& id) const {
  auto asset = m_assetsCache.value(id);
  std::optional<StarException> logError;
  if (id.type == AssetType::Audio && asset) {
    auto processed = postProcessAudio(asset);
    if (processed) {
      asset = std::move(*processed);
    } else {
      processed.error() = resourceFailure(std::move(processed.error()), id.path.basePath);
      if (auto logged = m_logger.errorException(strf("Error post-processing asset: {}", id.path),
            ExceptionDiagnostic(processed.error())); !logged)
        logError = std::move(logged.error());
    }
  }
  // Optional decompression failure retains the original compressed audio.
  m_queue.remove(id);
  if (asset) {
    asset->needsPostProcessing = false;
    m_assetsCache[id] = asset;
    freshen(asset);
    m_assetsDone.notify_all();
  }
  if (m_queue.empty() && m_luaGcPending)
    m_assetsQueued.notify_one();
  if (logError)
    return std::unexpected(std::move(*logError));
  return true;
}

void Assets::publishAssetFailure(AssetId const& id, StarException const& error) const {
  m_queue.remove(id);
  m_assetsCache[id] = {};
  m_assetErrors.set(id, error);
  m_assetsDone.notify_all();
  if (m_queue.empty() && m_luaGcPending)
    m_assetsQueued.notify_one();
}

std::expected<std::shared_ptr<Assets::AssetData>, StarException> Assets::loadAsset(AssetId const& id) const {
  if (auto error = m_assetErrors.find_ref(id))
    return std::unexpected(error->get());
  if (auto asset = m_assetsCache.value(id))
    return asset;
  if (m_queue.value(id, QueuePriority::None) == QueuePriority::Working)
    return std::shared_ptr<AssetData>{};
  m_luaGcPending = true;
  m_queue[id] = QueuePriority::Working;
  auto loaded = [&]() -> std::expected<std::shared_ptr<AssetData>, StarException> {
    switch (id.type) {
      case AssetType::Json: return loadJson(id.path);
      case AssetType::Image: return loadImage(id.path);
      case AssetType::Audio: return loadAudio(id.path);
      case AssetType::Font: return loadFont(id.path);
      case AssetType::Bytes: return loadBytes(id.path);
    }
    return std::unexpected(AssetException("Unrecognized asset type"));
  }();
  if (!loaded) {
    loaded.error() = resourceFailure(std::move(loaded.error()), id.path.basePath);
    if (id.type == AssetType::Image && m_settings.missingImage) {
      auto logged = m_logger.errorException(strf("Could not load image asset '{}', using placeholder default.", id.path),
          ExceptionDiagnostic(loaded.error()));
      if (logged) {
        loaded = loadImage({*m_settings.missingImage, {}, {}});
        if (!loaded)
          loaded.error() = resourceFailure(std::move(loaded.error()), *m_settings.missingImage);
      } else {
        loaded = std::unexpected(std::move(logged.error()));
      }
    } else if (id.type == AssetType::Audio && m_settings.missingAudio) {
      auto logged = m_logger.errorException(strf("Could not load audio asset '{}', using placeholder default.", id.path),
          ExceptionDiagnostic(loaded.error()));
      if (logged) {
        loaded = loadAudio({*m_settings.missingAudio, {}, {}});
        if (!loaded)
          loaded.error() = resourceFailure(std::move(loaded.error()), *m_settings.missingAudio);
      } else {
        loaded = std::unexpected(std::move(logged.error()));
      }
    }
  }
  if (!loaded) {
    publishAssetFailure(id, loaded.error());
    return std::unexpected(std::move(loaded.error()));
  }
  auto asset = std::move(*loaded);
  if (asset) {
    if (asset->needsPostProcessing)
      m_queue[id] = QueuePriority::PostProcess;
    else
      m_queue.remove(id);
    m_assetsCache[id] = asset;
    m_assetsDone.notify_all();
    freshen(asset);
    if (m_queue.empty() && m_luaGcPending)
      m_assetsQueued.notify_one();
  } else {
    m_queue[id] = QueuePriority::Load;
    m_assetsQueued.notify_one();
    if (auto moved = m_queue.move_to_back(id); !moved) {
      publishAssetFailure(id, moved.error());
      return std::unexpected(std::move(moved.error()));
    }
  }
  return asset;
}

std::expected<std::shared_ptr<Assets::AssetData>, StarException> Assets::loadJson(AssetPath const& path) const {
  if (path.subPath) {
    auto top = loadAsset({.type = AssetType::Json, .path = {path.basePath, {}, {}}});
    if (!top)
      return std::unexpected(std::move(top.error()));
    if (!*top)
      return std::shared_ptr<AssetData>{};
    auto value = asRef<JsonData>(**top)->get().json.query(*path.subPath);
    if (!value)
      return std::unexpected(AssetException(strf("Could not read JSON value {}", path), value.error()));
    auto data = std::make_shared<JsonData>();
    data->json = std::move(*value);
    return data;
  }
  return unlockDuring([&]() -> std::expected<std::shared_ptr<AssetData>, StarException> {
    auto value = readJson(path.basePath);
    if (!value)
      return std::unexpected(AssetException(strf("Could not read JSON asset {}", path), value.error()));
    auto data = std::make_shared<JsonData>();
    data->json = std::move(*value);
    return data;
  });
}

std::expected<std::shared_ptr<Assets::AssetData>, StarException> Assets::loadImage(AssetPath const& path) const {
  if (auto valid = validatePath(path, true, true); !valid)
    return std::unexpected(std::move(valid.error()));
  if (!path.directives.empty()) {
    auto loaded = loadAsset({.type = AssetType::Image, .path = {path.basePath, path.subPath, {}}});
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
    if (!*loaded)
      return std::shared_ptr<AssetData>{};
    // Keep loaded owning the wrapper throughout the unlocked image operation.
    auto& source = asRef<ImageData>(**loaded)->get();
    StringMap<std::shared_ptr<Image const>> references;
    StringList referencePaths;
    for (auto const& directives : path.directives.list())
      directives.loadOperations();
    path.directives.forEach([&](auto const& entry, Directives const&) {
      addImageOperationReferences(entry.operation, referencePaths);
    });
    for (auto const& ref : referencePaths) {
      auto components = AssetPath::split(ref);
      if (auto valid = validatePath(components, true, false); !valid)
        return std::unexpected(std::move(valid.error()));
      auto reference = loadAsset({.type = AssetType::Image, .path = std::move(components)});
      if (!reference)
        return std::unexpected(std::move(reference.error()));
      if (!*reference)
        return std::shared_ptr<AssetData>{};
      auto& image = asRef<ImageData>(**reference)->get();
      image.isReference = true;
      freshen(*reference);
      references[ref] = image.image;
    }
    return unlockDuring([&]() -> std::expected<std::shared_ptr<AssetData>, StarException> {
      auto image = path.directives.applyNewImage(
        *source.image,
        [&](String const& ref) -> std::expected<std::optional<std::reference_wrapper<Image const>>, StarException> {
          if (auto reference = references.find_ref(ref))
            return std::optional{std::cref(*reference->get())};
          return std::unexpected(AssetException(strf("No such image directive reference '{}'", ref)));
        });
      if (!image)
        return std::unexpected(std::move(image.error()));
      auto data = std::make_shared<ImageData>();
      data->image = std::make_shared<Image>(std::move(*image));
      return data;
    });
  }
  if (path.subPath) {
    auto loaded = loadAsset({.type = AssetType::Image, .path = {path.basePath, {}, {}}});
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
    if (!*loaded)
      return std::shared_ptr<AssetData>{};
    auto& source = asRef<ImageData>(**loaded)->get();
    if (!source.frames)
      return std::unexpected(AssetException::format("No associated frames file found for image '{}' while resolving image frame '{}'", path.basePath, path));
    if (auto alias = source.frames->aliases.find_ref(*path.subPath)) {
      auto target = loadAsset({.type = AssetType::Image, .path = {path.basePath, alias->get(), path.directives}});
      if (!target)
        return std::unexpected(std::move(target.error()));
      if (!*target)
        return std::shared_ptr<AssetData>{};
      auto data = std::make_shared<ImageData>();
      data->image = asRef<ImageData>(**target)->get().image;
      data->alias = true;
      return data;
    }
    auto frame = source.frames->frames.find_ref(*path.subPath);
    if (!frame)
      return std::unexpected(AssetException(strf("No such frame {} in frames spec {}", *path.subPath, source.frames->framesFile)));
    return unlockDuring([&]() -> std::expected<std::shared_ptr<AssetData>, StarException> {
      auto image = source.image->subImage(Vec2U(frame->get().xMin(), source.image->height() - frame->get().yMax()), frame->get().size());
      if (!image)
        return std::unexpected(std::move(image.error()));
      auto data = std::make_shared<ImageData>();
      data->image = std::make_shared<Image>(std::move(*image));
      return data;
    });
  }
  auto image = unlockDuring([&]() { return readImage(path.basePath); });
  if (!image)
    return std::unexpected(std::move(image.error()));
  auto frames = bestFramesSpecification(path.basePath);
  if (!frames)
    return std::unexpected(std::move(frames.error()));
  auto data = std::make_shared<ImageData>();
  data->image = std::move(*image);
  data->frames = std::move(*frames);
  return data;
}

std::expected<std::shared_ptr<Assets::AssetData>, StarException> Assets::loadAudio(AssetPath const& path) const {
  return unlockDuring([&]() -> std::expected<std::shared_ptr<AssetData>, StarException> {
    auto device = open(path.basePath);
    if (!device)
      return std::unexpected(std::move(device.error()));
    auto audio = Audio::create(std::move(*device), path.basePath);
    if (!audio)
      return std::unexpected(std::move(audio.error()));
    auto data = std::make_shared<AudioData>();
    data->audio = std::make_shared<Audio>(std::move(*audio));
    data->needsPostProcessing = data->audio->compressed();
    return data;
  });
}

std::expected<std::shared_ptr<Assets::AssetData>, StarException> Assets::loadFont(AssetPath const& path) const {
  return unlockDuring([&]() -> std::expected<std::shared_ptr<AssetData>, StarException> {
    auto bytes = read(path.basePath);
    if (!bytes)
      return std::unexpected(std::move(bytes.error()));
    auto font = Font::loadFont(*m_fontLibrary, std::make_shared<ByteArray>(std::move(*bytes)));
    if (!font)
      return std::unexpected(std::move(font.error()));
    auto data = std::make_shared<FontData>();
    data->font = std::move(*font);
    return data;
  });
}

std::expected<std::shared_ptr<Assets::AssetData>, StarException> Assets::loadBytes(AssetPath const& path) const {
  return unlockDuring([&]() -> std::expected<std::shared_ptr<AssetData>, StarException> {
    auto bytes = read(path.basePath);
    if (!bytes)
      return std::unexpected(std::move(bytes.error()));
    auto data = std::make_shared<BytesData>();
    data->bytes = std::make_shared<ByteArray>(std::move(*bytes));
    return data;
  });
}

std::expected<std::shared_ptr<Assets::AssetData>, StarException> Assets::postProcessAudio(std::shared_ptr<AssetData> const& original) const {
  return unlockDuring([&]() -> std::expected<std::shared_ptr<AssetData>, StarException> {
    auto audioData = asRef<AudioData>(*original);
    if (!audioData)
      return std::shared_ptr<AssetData>{};
    auto duration = audioData->get().audio->totalTime();
    if (!duration)
      return std::unexpected(std::move(duration.error()));
    if (*duration >= m_settings.audioDecompressLimit)
      return original;
    auto audio = audioData->get().audio->clone();
    if (!audio)
      return std::unexpected(std::move(audio.error()));
    if (auto decompressed = audio->uncompress(); !decompressed)
      return std::unexpected(std::move(decompressed.error()));
    auto data = std::make_shared<AudioData>();
    data->audio = std::make_shared<Audio>(std::move(*audio));
    return data;
  });
}

void Assets::freshen(std::shared_ptr<AssetData> const& asset) const {
  asset->time = std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now());
}

}
