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

import star.lexical_cast;
import star.data_stream_devices;
import star.lua;
import star.path_embed;
import star.buffer;

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

namespace {
// A patch can re-enter Assets while the asset mutex is released, and even
// enter another service. Keep the task chain on the stack, not the heap.
struct LoadEpochScope {
  using Link = std::optional<std::reference_wrapper<LoadEpochScope const>>;
  static inline constinit thread_local Link active;

  LoadEpochScope(Assets const& owner, std::uint64_t epoch)
    : service(owner), epoch(epoch), previous(active) {
    active.emplace(std::cref(*this));
  }
  LoadEpochScope(LoadEpochScope const&) = delete;
  LoadEpochScope& operator=(LoadEpochScope const&) = delete;
  ~LoadEpochScope() { active = previous; }

  Assets const& service;
  std::uint64_t const epoch;
  Link const previous;
};

std::unexpected<StarException> superseded_load() {
  return std::unexpected<StarException>(AssetException("Asset loading superseded by reload"));
}
}

std::uint64_t Assets::current_load_epoch() const {
  for (auto scope = LoadEpochScope::active; scope; scope = scope->get().previous)
    if (std::addressof(scope->get().service) == this)
      return scope->get().epoch;
  return m_load_epoch;
}

template <typename T>
concept cached_asset_data = std::same_as<T, Assets::JsonData>
  || std::same_as<T, Assets::ImageData>
  || std::same_as<T, Assets::AudioData>
  || std::same_as<T, Assets::FontData>
  || std::same_as<T, Assets::BytesData>;

template <cached_asset_data T>
static std::optional<std::reference_wrapper<T>> cached_data_ref(Assets::AssetData& data) noexcept {
  if (data.type != T::kind)
    return std::nullopt;
  return std::ref(static_cast<T&>(data));
}

// An empty diagnostic means the base path is valid.
static std::optional<std::string_view> validate_base_path(std::string_view const& base_path) {
  if (base_path.empty() || base_path[0] != '/')
    return "Path '<path>' must be absolute";

  bool first = true;
  bool slashed = true;
  bool dotted = false;
  for (auto c : base_path) {
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

[[nodiscard]] static std::expected<void, StarException> validate_path(AssetPath const& components, bool can_contain_sub_path, bool can_contain_directives) {
  if (auto error = validate_base_path(components.base_path.utf8())) {
    std::array<std::pair<std::string_view, std::string_view>, 1> tags{{{"path", components.base_path.utf8()}}};
    auto text = Star::format_tags(*error, tags);
    if (!text)
      return std::unexpected(std::move(text.error()));
    return std::unexpected(AssetException(std::move(*text)));
  }
  if (!can_contain_sub_path && components.sub_path)
    return std::unexpected(AssetException::format("Path '{}' cannot contain sub-path", components));
  if (!can_contain_directives && !components.directives.empty())
    return std::unexpected(AssetException::format("Path '{}' cannot contain directives", components));
  return {};
}

static std::expected<EmbeddedData, StarException> read_embedded(String const& path, bool allowed, DecodeMode mode = DecodeMode::Asset) {
  if (!allowed)
    return std::unexpected(AssetException("Embedded assets are disabled by safe.allowRemoteAssets"));
  auto data = decode_embedded_data(path, mode);
  if (!data)
    return std::unexpected(AssetException("Invalid embedded asset", std::move(data.error())));
  return data;
}

// Only a confirmed vanished directory entry is optional. A dangling symlink,
// permission failure, corrupt package or unreadable existing file stays an error.
static std::expected<std::optional<ByteArray>, StarException> read_patch(AssetSource& source, String const& path,
                                                                        Logger& logger) {
  auto bytes = source.read(path);
  if (bytes)
    return std::optional<ByteArray>(std::move(*bytes));
  if (bytes.error().is<IOException>()) {
    if (auto filesystem = source.missing_file_path(path)) {
      auto logged = logger.warn("Skipping vanished patch '{}' ({})", path, *filesystem);
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      return std::nullopt;
    }
  }
  return std::unexpected(std::move(bytes.error()));
}

std::string Assets::source_diagnostic(AssetSource const& source,
    std::optional<std::reference_wrapper<String const>> source_path) {
  auto result = source_path ? Star::format("asset source '{}'", source_path->get()) : std::string("unregistered asset source");
  auto const& metadata = source.metadata();
  for (String const& key : {String("friendlyName"), String("name")}) {
    if (auto value = metadata.find_ref(key)) {
      // Invalid optional metadata must not hide the error being diagnosed.
      auto text = value->get().string_ptr();
      if (text && !(**text).empty())
        result += Star::format(", {} '{}'", key, **text);
    }
  }
  return result;
}

StarException Assets::with_asset_context(StarException error, String const& path, std::string_view operation) const {
  auto const& current = *error.diagnostic_summary();
  bool add_asset = current.asset.empty();
  bool add_operation = current.operation.empty() && !operation.empty();
  bool add_provider = current.asset_provider.empty() && (add_asset || current.asset == path.utf8());
  if (!add_asset && !add_operation && !add_provider)
    return error;
  std::string provider;
  if (add_provider) {
    auto descriptor = asset_descriptor(path);
    if (descriptor && *descriptor) {
      auto source_path = asset_source_path((*descriptor)->source);
      provider = source_diagnostic(*(*descriptor)->source,
          source_path ? std::optional<std::reference_wrapper<String const>>(std::cref(*source_path)) : std::nullopt);
    }
  }
  if (!add_asset && !add_operation && provider.empty())
    return error;
  auto summary = current;
  if (add_asset)
    summary.asset = path.utf8();
  if (add_operation)
    summary.operation = operation;
  if (add_asset || !provider.empty()) {
    auto context = provider.empty() ? Star::format("{} '{}'", operation, path)
        : Star::format("{} '{}' from {}", operation, path, provider);
    error = std::move(error).with_context(std::move(context));
  }
  if (!provider.empty())
    summary.asset_provider = std::move(provider);
  return std::move(error).with_summary(std::move(summary));
}


static void add_summary_source(ExceptionSummary& summary, std::string_view source, std::string_view provider) {
  for (auto& location : summary.locations) {
    if (location.source == source && (location.provider.empty() || location.provider == provider)) {
      if (location.provider.empty())
        location.provider = provider;
      return;
    }
  }
  summary.locations.push_back({std::string(source), -1, std::string(provider)});
}

static StarException resource_failure(StarException error, String const& path) {
  auto const& current = *error.diagnostic_summary();
  if (current.kind != ExceptionSummaryKind::Native && !current.asset.empty())
    return error;
  auto summary = *error.diagnostic_summary();
  if (summary.kind == ExceptionSummaryKind::Native)
    summary.kind = ExceptionSummaryKind::Resource;
  if (summary.asset.empty())
    summary.asset = path.utf8();
  return std::move(error).with_summary(std::move(summary));
}

static StarException script_source_failure(StarException error, String const& path, std::string const& provider) {
  if (std::ranges::any_of(error.diagnostic_summary()->locations, [&](ExceptionSummary::Location const& location) {
        return location.source == path.utf8() && location.provider == provider;
      }))
    return error;
  auto summary = *error.diagnostic_summary();
  add_summary_source(summary, path.utf8(), provider);
  return std::move(error).with_summary(std::move(summary));
}

static StarException lua_patch_failure(StarException error, String const& target_path, String const& patch_path,
    AssetSource const& source, std::optional<std::reference_wrapper<String const>> source_path) {
  auto provider = Assets::source_diagnostic(source, source_path);
  auto const& current = *error.diagnostic_summary();
  auto patch_base_path = AssetPath::remove_sub_path(patch_path);
  bool change_kind = current.kind != ExceptionSummaryKind::PatchSyntax && current.kind != ExceptionSummaryKind::PatchRuntime;
  bool change_patch = current.patch.empty();
  bool change_source = std::ranges::none_of(current.locations, [&](ExceptionSummary::Location const& location) {
    return location.source == patch_base_path.utf8() && location.provider == provider;
  });
  if (change_kind || change_patch || change_source) {
    auto summary = current;
    if (change_kind)
      summary.kind = current.kind == ExceptionSummaryKind::ScriptSyntax
        ? ExceptionSummaryKind::PatchSyntax : ExceptionSummaryKind::PatchRuntime;
    if (change_patch) {
      summary.asset = target_path.utf8();
      summary.patch = patch_path.utf8();
      summary.patch_provider = provider;
    }
    if (change_source)
      add_summary_source(summary, patch_base_path.utf8(), provider);
    error = std::move(error).with_summary(std::move(summary));
  }
  return std::move(error).with_context(
      Star::format("Lua patch provider: target '{}', patch '{}', {}", target_path, patch_path, provider));
}

std::expected<std::optional<RectU>, StarException> FramesSpecification::get_rect(String const& frame) const {
  if (auto alias = aliases.find_ref(frame)) {
    if (auto rect = frames.find_ref(alias->get()))
      return std::optional<RectU>(rect->get());
    return std::unexpected(AssetException(Star::format("No such frame '{}' found for alias '{}'", alias->get(), frame)));
  } else {
    return frames.maybe(frame);
  }
}

std::expected<Json, StarException> FramesSpecification::to_json() const {
  auto alias_json = json_from_map(aliases);
  if (!alias_json)
    return std::unexpected(std::move(alias_json.error()));
  auto frame_json = json_from_map_v(frames, json_from_rectu);
  if (!frame_json)
    return std::unexpected(std::move(frame_json.error()));
  return JsonObject{{"aliases", std::move(*alias_json)}, {"frames", std::move(*frame_json)}, {"file", frames_file}};
}

Assets::Assets(Logger& logger) : m_logger(logger) {}

std::expected<std::shared_ptr<Assets>, StarException> Assets::create(Logger& logger, RandomService& random, LogMap& log_map, Settings settings, StringList asset_sources) {
  auto assets = std::shared_ptr<Assets>(new Assets(logger));
  auto initialized = assets->initialize(random, log_map, std::move(settings), std::move(asset_sources));
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return assets;
}

std::expected<void, StarException> Assets::initialize(RandomService& random, LogMap& log_map, Settings settings, StringList asset_sources) {
  constexpr std::string_view assets_patch_suffix = ".patch";
  constexpr std::string_view assets_patch_list_suffix = ".patchlist";
  constexpr std::string_view assets_lua_patch_suffix = ".patch.lua";

  auto font_library = FontLibrary::create();
  if (!font_library)
    return std::unexpected(std::move(font_library.error()));
  m_font_library.emplace(std::move(*font_library));
  m_settings = std::move(settings);
  m_asset_sources = std::move(asset_sources);

  auto created_lua_engine = LuaEngine::create();
  if (!created_lua_engine)
    return std::unexpected(std::move(created_lua_engine.error()));
  m_lua_engine = std::move(*created_lua_engine);
  m_lua_engine->set_image_resolver([this](String const& path) { return image(path); });
  auto push_global_context = [this](String const& name, LuaCallbacks const& callbacks) -> std::expected<void, StarException> {
    auto table = m_lua_engine->create_table();
    if (!table)
      return std::unexpected(std::move(table.error()));
    for (auto const& p : callbacks.callbacks()) {
      auto function = m_lua_engine->create_wrapped_function(p.second);
      if (!function)
        return std::unexpected(std::move(function.error()));
      auto set = table->set(p.first, std::move(*function));
      if (!set)
        return std::unexpected(std::move(set.error()));
    }
    return m_lua_engine->set_global(name, std::move(*table));
  };

  auto make_base_asset_callbacks = [this]() -> std::expected<LuaCallbacks, StarException> {
    LuaCallbacks callbacks;
    auto registered = callbacks.register_callback("byExtension", [this](String const& extension) -> std::expected<StringSet, StarException> {
      auto paths = scan_extension(extension);
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
    registered = callbacks.register_callback("json", [this](String const& path) { return json(path); });
    if (!registered)
      return std::unexpected(std::move(registered.error()));
    registered = callbacks.register_callback("exists", [this](String const& path) { return asset_exists(path); });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks.register_callback("sourcePaths", [this](LuaEngine& engine, std::optional<bool> with_meta_data) -> std::expected<LuaTable, StarException> {
      auto asset_sources = this->asset_sources();
      auto table = engine.create_table(asset_sources.size(), 0);
      if (!table)
        return std::unexpected(std::move(table.error()));
      if (with_meta_data.value_or(false)) {
        for (auto const& asset_source : asset_sources) {
          auto metadata = this->asset_source_metadata(asset_source);
          if (!metadata)
            return std::unexpected(std::move(metadata.error()));
          auto set = table->set(asset_source, std::move(*metadata));
          if (!set)
            return std::unexpected(std::move(set.error()));
        }
      } else {
        std::size_t i = 0;
        for (auto const& asset_source : asset_sources) {
          auto set = table->set(++i, asset_source);
          if (!set)
            return std::unexpected(std::move(set.error()));
        }
      }
      return table;
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered =
      callbacks.register_callback("sourceMetadata", [this](String const& source_path) -> std::optional<JsonObject> {
        if (auto asset_source = m_asset_source_paths.find_right_ref(source_path))
          return asset_source->get()->metadata();
        return {};
      });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks.register_callback("origin", [this](String const& path) -> std::expected<std::optional<String>, StarException> {
      auto descriptor = this->asset_descriptor(path);
      if (!descriptor)
        return std::unexpected(std::move(descriptor.error()));
      if (*descriptor)
        return this->asset_source_path((*descriptor)->source);
      return std::optional<String>{};
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks.register_callback("bytes", [this](String const& path, std::optional<bool> allow_remote) -> std::expected<String, StarException> {
      auto asset_bytes = bytes(path, allow_remote.value_or(false));
      if (!asset_bytes)
        return std::unexpected(std::move(asset_bytes.error()));
      return String(std::string_view((*asset_bytes)->begin(), (*asset_bytes)->end()));
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks.register_callback(
      "sourceBytes", [this](String const& source_path, String const& path) -> std::expected<String, StarException> {
        auto source = m_asset_source_paths.find_right_ref(source_path);
        if (!source)
          return std::unexpected(
            AssetException::format("Cannot read asset '{}' from unknown asset source '{}'", path, source_path));
        // DirectoryAssetSource::read also accepts unindexed filesystem-relative paths.
        // Only expose paths actually enumerated by this provider, including shadowed assets.
        if (!source->get()->contains(path))
          return std::unexpected(AssetException::format("Asset '{}' is not indexed in {}", path,
                                                        source_diagnostic(*source->get(), std::cref(source_path))));
        auto data = source->get()->read(path);
        if (!data) {
          auto provider = source_diagnostic(*source->get(), std::cref(source_path));
          auto error = std::move(data.error()).with_context(Star::format("Reading asset '{}' from {}", path, provider));
          auto summary = *error.diagnostic_summary();
          summary.kind = ExceptionSummaryKind::Resource;
          summary.asset = path.utf8();
          summary.asset_provider = std::move(provider);
          summary.operation = "Reading source asset bytes";
          return std::unexpected(std::move(error).with_summary(std::move(summary)));
        }
        return String(std::string_view(data->begin(), data->end()));
      });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks.register_callback("image", [this](String const& path) -> std::expected<Image, StarException> {
      auto asset_image = image(path);
      if (!asset_image)
        return std::unexpected(std::move(asset_image.error()));
      if ((*asset_image)->bytes_per_pixel() == 3)
        return (*asset_image)->convert(PixelFormat::RGBA32);
      return **asset_image;
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks.register_callback("frames", [this](String const& path) -> std::expected<Json, StarException> {
      auto frames = image_frames(path);
      if (!frames)
        return std::unexpected(std::move(frames.error()));
      if (*frames)
        return (*frames)->to_json();
      return Json();
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks.register_callback("scan", [this](std::optional<String> const& a, std::optional<String> const& b) -> std::expected<StringList, StarException> {
      return b ? scan(a.value_or(String{}), *b) : scan(a.value_or(String{}));
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));
    return callbacks;
  };

  auto utility_callbacks = LuaBindings::make_utility_callbacks(random, m_logger, log_map);
  if (!utility_callbacks)
    return std::unexpected(std::move(utility_callbacks.error()));
  auto pushed_utility = push_global_context("sb", *utility_callbacks);
  if (!pushed_utility)
    return std::unexpected(std::move(pushed_utility.error()));
  auto base_callbacks = make_base_asset_callbacks();
  if (!base_callbacks)
    return std::unexpected(std::move(base_callbacks.error()));
  auto pushed_assets = push_global_context("assets", *base_callbacks);
  if (!pushed_assets)
    return std::unexpected(std::move(pushed_assets.error()));

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
  auto load_script_state = std::make_shared<LoadScriptState>();

  auto decorate_lua_context = [&](LuaContext& context, std::shared_ptr<MemoryAssetSource> new_files) -> std::expected<void, StarException> {
    if (!new_files)
      return {};
    // Re-add the assets callbacks with the load-script mutation functions.
    auto removed = context.remove("assets");
    if (!removed)
      return std::unexpected(std::move(removed.error()));
    auto callbacks = make_base_asset_callbacks();
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    // A staged file is invisible until its group finishes, except when it has
    // already been attached as a patch to an indexed descriptor.
    auto set_staged_file = [this, new_files]<typename T>(String const& path, T&& contents) -> std::expected<void, StarException>
      requires (std::same_as<std::remove_cvref_t<T>, Image> || std::same_as<std::remove_cvref_t<T>, ByteArray>) {
      if (new_files->contains(path)) {
        bool changed = false;
        for (auto const& [image, file] : m_frame_files_by_image) {
          if (file.shareable || file.path.empty())
            continue;
          auto key = CaseInsensitiveStringView::create(file.path);
          if (!key)
            return std::unexpected(std::move(key.error()));
          auto descriptor = m_files.find_ref(*key);
          if (descriptor && std::ranges::any_of(descriptor->get().patch_sources, [&](std::pair<String, std::shared_ptr<AssetSource>> const& patch) {
                return patch.second == new_files && patch.first == path;
              })) {
            m_patched_frames_by_image.remove(image);
            changed = true;
          }
        }
        if (changed) {
          m_assets_cache.clear();
          m_asset_errors.clear();
        }
      }
      new_files->set(path, std::forward<T>(contents));
      return {};
    };
    auto registered = callbacks->register_callback("add", [set_staged_file](LuaEngine& engine, String const& path, LuaValue const& data) -> std::expected<void, StarException> {
      auto str = engine.lua_maybe_to<String>(data);
      if (!str)
        return std::unexpected(std::move(str.error()));
      if (*str)
        return set_staged_file(path, (*str)->utf8_bytes());
      auto image = engine.lua_maybe_to<Image>(data);
      if (!image)
        return std::unexpected(std::move(image.error()));
      if (*image)
        return set_staged_file(path, std::move(**image));
      auto json = engine.lua_to<Json>(data);
      if (!json)
        return std::unexpected(std::move(json.error()));
      auto text = json->repr();
      if (!text)
        return std::unexpected(std::move(text.error()));
      return set_staged_file(path, text->utf8_bytes());
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks->register_callback("patch", [this, new_files, set_staged_file](String const& path, String const& patch_path) -> std::expected<bool, StarException> {
      if (new_files->contains(path)) {
        auto image = new_files->image(path);
        if (!image)
          return std::unexpected(std::move(image.error()));
        std::shared_ptr<AssetSource> patch_source;
        if (new_files->contains(patch_path)) {
          patch_source = new_files;
        } else {
          auto patch_key = CaseInsensitiveStringView::create(patch_path);
          if (!patch_key)
            return std::unexpected(std::move(patch_key.error()));
          if (auto asset = m_files.find_ref(*patch_key))
            patch_source = asset->get().source;
        }
        if (!patch_source)
          return false;
        if (*image) {
          auto patched = apply_image_patches(*image, path, {std::make_pair(patch_path, patch_source)});
          if (!patched)
            return std::unexpected(std::move(patched.error()));
          if (auto stored = set_staged_file(path, **patched); !stored)
            return std::unexpected(std::move(stored.error()));
        } else {
          auto file = new_files->read(path);
          if (!file)
            return std::unexpected(std::move(file.error()));
          auto json = input_utf8_json(file->begin(), file->end(), JsonParseType::Top);
          if (!json)
            return std::unexpected(std::move(json.error()));
          auto patched = apply_json_patches(*json, path, {std::make_pair(patch_path, patch_source)});
          if (!patched)
            return std::unexpected(std::move(patched.error()));
          auto text = patched->repr();
          if (!text)
            return std::unexpected(std::move(text.error()));
          if (auto stored = set_staged_file(path, text->utf8_bytes()); !stored)
            return std::unexpected(std::move(stored.error()));
        }
        return true;
      }
      auto path_key = CaseInsensitiveStringView::create(path);
      if (!path_key)
        return std::unexpected(std::move(path_key.error()));
      auto file = m_files.find_ref(*path_key);
      if (!file)
        return false;
      std::shared_ptr<AssetSource> patch_source;
      if (new_files->contains(patch_path)) {
        patch_source = new_files;
      } else {
        auto patch_key = CaseInsensitiveStringView::create(patch_path);
        if (!patch_key)
          return std::unexpected(std::move(patch_key.error()));
        auto asset = m_files.find_ref(*patch_key);
        if (!asset)
          return false;
        patch_source = asset->get().source;
      }
      if (auto invalidated = invalidate_frame_file(path); !invalidated)
        return std::unexpected(std::move(invalidated.error()));
      file->get().patch_sources.emplace_back(patch_path, std::move(patch_source));
      return true;
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks->register_callback(
      "patchReplace",
      [this, new_files](String const& target, String const& original_patch_path, String const& source_path,
                       String const& replacement_patch_path,
                       std::optional<String> const& replacement_target) -> std::expected<bool, StarException> {
        auto original_source = m_asset_source_paths.find_right_ref(source_path);
        if (!original_source)
          return false;
        auto target_key = CaseInsensitiveStringView::create(target);
        if (!target_key)
          return std::unexpected(std::move(target_key.error()));
        auto file = m_files.find_ref(*target_key);
        if (!file)
          return false;
        using PatchSource = std::pair<String, std::shared_ptr<AssetSource>>;
        auto matches = [&](PatchSource const& patch) {
          return patch.first == original_patch_path && patch.second == original_source->get();
        };
        if (!std::ranges::any_of(file->get().patch_sources, matches))
          return false;
        auto destination = *file;
        if (replacement_target) {
          auto destination_key = CaseInsensitiveStringView::create(*replacement_target);
          if (!destination_key)
            return std::unexpected(std::move(destination_key.error()));
          auto descriptor = m_files.find_ref(*destination_key);
          if (!descriptor)
            return false;
          destination = *descriptor;
        }
        PatchSource replacement{replacement_patch_path, new_files};
        if (!new_files->contains(replacement_patch_path)) {
          auto replacement_key = CaseInsensitiveStringView::create(replacement_patch_path);
          if (!replacement_key)
            return std::unexpected(std::move(replacement_key.error()));
          auto descriptor = m_files.find_ref(*replacement_key);
          if (!descriptor)
            return false;
          replacement = {descriptor->get().source_name, descriptor->get().source};
        }
        if (std::addressof(destination.get()) == std::addressof(file->get())) {
          for (auto& patch : file->get().patch_sources) {
            if (matches(patch))
              patch = replacement;
          }
        } else {
          for (auto const& patch : file->get().patch_sources) {
            if (matches(patch))
              destination.get().patch_sources.push_back(replacement);
          }
          file->get().patch_sources.filter(std::not_fn(matches));
        }
        // As at the load-script group boundary, dependent assets may already be
        // cached. Invalidate immediately so this same context observes the repair.
        // A previously failed target must also be allowed to load its new patches.
        clear_frame_catalog_cache();
        m_assets_cache.clear();
        m_asset_errors.clear();
        return true;
      });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks->register_callback("loadScriptReplace", [this, new_files, load_script_state](
        String const& source_path, String const& original_script_path, String const& replacement_script_path) -> std::expected<bool, StarException> {
      if (!load_script_state->active)
        return false;
      auto original_source = m_asset_source_paths.find_right_ref(source_path);
      if (!original_source || !original_source->get()->contains(original_script_path))
        return false;
      LoadScriptReference original{.source = original_source->get(), .path = original_script_path};
      if (load_script_state->unavailable.contains(original))
        return false;
      auto const& metadata = original.source->metadata();
      auto scripts = metadata.find_ref("scripts");
      if (!scripts)
        return false;
      bool declared = false;
      for (String const& group : {String("onLoad"), String("postLoad")}) {
        auto paths = scripts->get().opt_array(group);
        if (!paths)
          return std::unexpected(std::move(paths.error()));
        if (*paths) {
          for (auto const& path : **paths) {
            auto text = path.to_string();
            if (!text)
              return std::unexpected(std::move(text.error()));
            declared = declared || *text == original_script_path;
          }
        }
      }
      if (!declared)
        return false;
      LoadScriptReference replacement{.source = new_files, .path = replacement_script_path};
      if (!new_files->contains(replacement_script_path)) {
        auto replacement_key = CaseInsensitiveStringView::create(replacement_script_path);
        if (!replacement_key)
          return std::unexpected(std::move(replacement_key.error()));
        auto descriptor = m_files.find_ref(*replacement_key);
        if (!descriptor)
          return false;
        replacement = {.source = descriptor->get().source, .path = descriptor->get().source_name};
      }
      for (auto& entry : load_script_state->replacements) {
        if (entry.original == original) {
          entry.replacement = std::move(replacement);
          return true;
        }
      }
      load_script_state->replacements.push_back({.original = std::move(original), .replacement = std::move(replacement)});
      return true;
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    registered = callbacks->register_callback("erase", [this](String const& path) -> std::expected<bool, StarException> {
      auto path_key = CaseInsensitiveStringView::create(path);
      if (!path_key)
        return std::unexpected(std::move(path_key.error()));
      auto file = m_files.find(*path_key);
      bool erased = file != m_files.end();
      if (erased) {
        if (auto invalidated = invalidate_frame_file(path); !invalidated)
          return std::unexpected(std::move(invalidated.error()));
        m_files.erase(file);
        auto extension = AssetPath::extension(path);
        auto extension_key = CaseInsensitiveStringView::create(extension);
        if (!extension_key)
          return std::unexpected(std::move(extension_key.error()));
        if (auto paths = m_files_by_extension.find_ref(*extension_key)) {
          auto indexed = paths->get().find(*path_key);
          if (indexed != paths->get().end())
            paths->get().erase(indexed);
        }
      }
      return erased;
    });
    if (!registered)
      return std::unexpected(std::move(registered.error()));
    return context.set_callbacks("assets", *callbacks);
  };

  auto add_source = [&](String const& source_path, std::shared_ptr<AssetSource> source) -> std::expected<void, StarException> {
    auto added = m_asset_source_paths.add(source_path, source);
    if (!added)
      return std::unexpected(std::move(added.error()));

    return source->for_each_path([&](String const& filename) -> std::expected<void, StarException> {
      auto filename_key = CaseInsensitiveString::create(filename);
      if (!filename_key)
        return std::unexpected(std::move(filename_key.error()));
      auto index_patch = [&](std::size_t suffix_size) -> std::expected<void, StarException> {
        auto size = filename.size();
        if (!size)
          return std::unexpected(std::move(size.error()));
        auto target_patch_file = filename.substr(0, *size - suffix_size);
        if (!target_patch_file)
          return std::unexpected(std::move(target_patch_file.error()));
        auto target_key = CaseInsensitiveStringView::create(*target_patch_file);
        if (!target_key)
          return std::unexpected(std::move(target_key.error()));
        if (auto p = m_files.find_ref(*target_key))
          p->get().patch_sources.push_back({filename, source});
        return {};
      };
      auto contains_patch = filename.contains(assets_patch_suffix, String::CaseInsensitive);
      if (!contains_patch)
        return std::unexpected(std::move(contains_patch.error()));
      if (*contains_patch) {
        auto ends_patch = filename.ends_with(assets_patch_suffix, String::CaseInsensitive);
        if (!ends_patch)
          return std::unexpected(std::move(ends_patch.error()));
        if (*ends_patch) {
          auto indexed = index_patch(assets_patch_suffix.size());
          if (!indexed)
            return std::unexpected(std::move(indexed.error()));
        } else {
          auto ends_lua_patch = filename.ends_with(assets_lua_patch_suffix, String::CaseInsensitive);
          if (!ends_lua_patch)
            return std::unexpected(std::move(ends_lua_patch.error()));
          if (*ends_lua_patch) {
            auto indexed = index_patch(assets_lua_patch_suffix.size());
            if (!indexed)
              return std::unexpected(std::move(indexed.error()));
          } else {
            auto ends_patch_list = filename.ends_with(assets_patch_list_suffix, String::CaseInsensitive);
            if (!ends_patch_list)
              return std::unexpected(std::move(ends_patch_list.error()));
            if (*ends_patch_list) {
              auto stream = source->read(filename);
              if (!stream)
                return std::unexpected(std::move(stream.error()));
              auto patch_list = input_utf8_json(stream->begin(), stream->end(), JsonParseType::Top);
              if (!patch_list)
                return std::unexpected(std::move(patch_list.error()));
              auto patch_pairs = patch_list->iterate_array();
              if (!patch_pairs)
                return std::unexpected(std::move(patch_pairs.error()));
              std::size_t patch_index = 0;
              for (auto const& patch_pair : *patch_pairs) {
                auto patches = patch_pair.get_array("patches");
                if (!patches)
                  return std::unexpected(std::move(patches.error()));
                auto paths = patch_pair.get_array("paths");
                if (!paths)
                  return std::unexpected(std::move(paths.error()));
                for (auto const& path : *paths) {
                  auto target_path = path.to_string();
                  if (!target_path)
                    return std::unexpected(std::move(target_path.error()));
                  auto target_key = CaseInsensitiveStringView::create(*target_path);
                  if (!target_key)
                    return std::unexpected(std::move(target_key.error()));
                  if (auto p = m_files.find_ref(*target_key)) {
                    for (std::size_t i = 0; i != patches->size(); ++i) {
                      auto const& patch = (*patches)[i];
                      if (patch.is_type(Json::Type::String)) {
                        auto patch_path = patch.to_string();
                        if (!patch_path)
                          return std::unexpected(std::move(patch_path.error()));
                        p->get().patch_sources.push_back({std::move(*patch_path), source});
                      } else {
                        p->get().patch_sources.push_back({Star::format("{}:[{}].patches[{}]", filename, patch_index, i), source});
                      }
                    }
                  }
                }
                ++patch_index;
              }
            } else {
              for (int i = 0; i < 10; ++i) {
                auto ends_numbered_patch = filename.ends_with(String(assets_patch_suffix) + Star::to_string(i), String::CaseInsensitive);
                if (!ends_numbered_patch)
                  return std::unexpected(std::move(ends_numbered_patch.error()));
                if (*ends_numbered_patch) {
                  auto indexed = index_patch(assets_patch_suffix.size() + 1);
                  if (!indexed)
                    return std::unexpected(std::move(indexed.error()));
                  break;
                }
              }
            }
          }
        }
      }

      auto& descriptor = m_files[*filename_key];
      descriptor.source_name = filename;
      descriptor.source = source;
      auto extension = AssetPath::extension(filename).to_lower();
      if (!extension)
        return std::unexpected(std::move(extension.error()));
      auto extension_key = CaseInsensitiveString::create(std::move(*extension));
      if (!extension_key)
        return std::unexpected(std::move(extension_key.error()));
      m_files_by_extension[*extension_key].insert(std::move(*filename_key));
      return {};
    });
  };

  auto run_load_scripts = [&](String const& group_name, String const& source_path, std::shared_ptr<AssetSource> source) -> std::expected<void, StarException> {
    auto const& metadata = source->metadata();
    if (auto scripts = metadata.find_ref("scripts")) {
      auto script_group = scripts->get().opt_array(group_name);
      if (!script_group)
        return std::unexpected(std::move(script_group.error()));
      if (*script_group) {
        auto source_name = metadata.value("name", File::base_name(source_path)).repr();
        if (!source_name)
          return std::unexpected(std::move(source_name.error()));
        auto memory_name = Star::format("{}::{}", *source_name, group_name);
        JsonObject memory_metadata{{"name", memory_name}};
        auto memory_assets = std::make_shared<MemoryAssetSource>(memory_name, memory_metadata);
        auto now = std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now());
        auto script_group_text = Json(**script_group).repr();
        if (!script_group_text)
          return std::unexpected(std::move(script_group_text.error()));
        auto logged = m_logger.info("Running {} scripts {}", group_name, *script_group_text);
        if (!logged)
          return std::unexpected(std::move(logged.error()));
        auto run_scripts = [&]() -> std::expected<void, StarException> {
          auto context = m_lua_engine->create_context();
          if (!context)
            return std::unexpected(std::move(context.error()));
          auto decorated = decorate_lua_context(*context, memory_assets);
          if (!decorated)
            return std::unexpected(std::move(decorated.error()));
          for (auto const& j_path : **script_group) {
            auto path = j_path.to_string();
            if (!path)
              return std::unexpected(std::move(path.error()));
            LoadScriptReference selected{.source = source, .path = *path};
            load_script_state->unavailable.push_back(selected);
            for (auto const& entry : load_script_state->replacements) {
              if (entry.original == selected) {
                selected = entry.replacement;
                break;
              }
            }
            auto provider = source_diagnostic(*selected.source, m_asset_source_paths.find_left_ref(selected.source));
            auto script = selected.source->read(selected.path);
            if (!script)
              return std::unexpected(script_source_failure(std::move(script.error()), selected.path, provider));
            auto loaded = context->load(script->span(), selected.path);
            if (!loaded)
              return std::unexpected(script_source_failure(std::move(loaded.error()), selected.path, provider));
          }
          return {};
        };
        auto ran = run_scripts();
        // A failed script closes the rest of its group too; those declarations
        // will not be executed later and must not report a successful replacement.
        for (auto const& j_path : **script_group) {
          auto path = j_path.to_string();
          if (!path)
            return std::unexpected(std::move(path.error()));
          LoadScriptReference closed{.source = source, .path = std::move(*path)};
          if (!load_script_state->unavailable.contains(closed))
            load_script_state->unavailable.push_back(std::move(closed));
        }
        if (!ran) {
          if (!ran.error().is<LuaException>())
            return std::unexpected(std::move(ran.error()));
          auto summary = *ran.error().diagnostic_summary();
          if (summary.operation.empty())
            summary.operation = Star::format("Running {} scripts", group_name);
          ran.error() = std::move(ran.error()).with_summary(std::move(summary));
          logged = m_logger.error_exception(Star::format("Exception while running {} scripts from {}", group_name,
                                                         source_diagnostic(*source, std::cref(source_path))),
                                            ExceptionDiagnostic(ran.error()));
          if (!logged)
            return std::unexpected(std::move(logged.error()));
        }
        logged = m_logger.info("Took {} seconds to run {} scripts {}",
            (std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()) - now).count(),
            group_name, *script_group_text);
        if (!logged)
          return std::unexpected(std::move(logged.error()));
        if (!memory_assets->empty()) {
          auto added = add_source(Star::format("{}::{}", source_path, group_name), memory_assets);
          if (!added)
            return std::unexpected(std::move(added.error()));
        }
      }
    }
    // Load scripts may have populated caches for assets they subsequently changed.
    clear_frame_catalog_cache();
    m_assets_cache.clear();
    return {};
  };

  List<std::pair<String, std::shared_ptr<AssetSource>>> sources;
  for (auto const& source_path : m_asset_sources) {
    auto logged = m_logger.info("Loading assets from: '{}'", source_path);
    if (!logged)
      return std::unexpected(std::move(logged.error()));
    std::shared_ptr<AssetSource> source;
    auto is_directory = File::is_directory(source_path);
    if (!is_directory)
      return std::unexpected(std::move(is_directory.error()));
    if (*is_directory) {
      auto created = DirectoryAssetSource::create(m_logger, source_path, m_settings.path_ignore);
      if (!created)
        return std::unexpected(std::move(created.error()));
      source = std::move(*created);
    } else {
      auto created = PackedAssetSource::create(source_path);
      if (!created)
        return std::unexpected(std::move(created.error()));
      source = std::move(*created);
    }
    auto added = add_source(source_path, source);
    if (!added)
      return std::unexpected(std::move(added.error()));
    sources.push_back(std::make_pair(source_path, source));
    auto loaded = run_load_scripts("onLoad", source_path, source);
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
  }
  for (auto const& pair : sources) {
    auto loaded = run_load_scripts("postLoad", pair.first, pair.second);
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
  }

  load_script_state->active = false;

  Sha256Hasher digest;
  StringList digest_paths;
  digest_paths.reserve(m_files.size());
  for (auto const& entry : m_files) {
    auto lower = entry.first.string().to_lower();
    if (!lower)
      return std::unexpected(std::move(lower.error()));
    digest_paths.push_back(std::move(*lower));
  }
  digest_paths.sort();
  struct SizeGroup {
    std::reference_wrapper<AssetSource> source;
    std::vector<std::reference_wrapper<String const>> paths;
    std::vector<std::expected<StreamOffset, StarException>> sizes;
  };
  struct DigestEntry {
    std::optional<std::reference_wrapper<String const>> path;
    std::size_t group;
    std::size_t index;
  };
  std::vector<SizeGroup> size_groups;
  std::unordered_map<std::shared_ptr<AssetSource>, std::size_t> source_groups;
  std::vector<DigestEntry> digest_entries;
  digest_entries.reserve(digest_paths.size());
  // Patch subpaths need owned stripped names; deque keeps borrows stable.
  std::deque<String> patch_names;
  auto add_size = [&](std::shared_ptr<AssetSource> const& source, String const& path,
                      std::optional<std::reference_wrapper<String const>> digest_path = {}) {
    auto [group, inserted] = source_groups.try_emplace(source, size_groups.size());
    if (inserted)
      size_groups.push_back({*source, {}, {}});
    auto& requests = size_groups[group->second].paths;
    digest_entries.push_back({digest_path, group->second, requests.size()});
    requests.emplace_back(path);
  };
  // Retain a preparation error until all preceding metadata results have been
  // consumed. Group traversal must never determine the first reported error.
  auto prepared = [&]() -> std::expected<void, StarException> {
    // Only the reached prefix is compiled: an earlier match can hide an invalid later pattern.
    std::vector<CompiledRegex> ignore_patterns;
    if (!digest_paths.empty())
      ignore_patterns.reserve(m_settings.digest_ignore.size());
    for (auto const& asset_path : digest_paths) {
      bool digest_file = true;
      for (std::size_t i = 0; i < m_settings.digest_ignore.size(); ++i) {
        if (i == ignore_patterns.size()) {
          auto pattern = CompiledRegex::compile(m_settings.digest_ignore[i], false);
          if (!pattern)
            return std::unexpected(std::move(pattern.error()));
          ignore_patterns.push_back(std::move(*pattern));
        }
        if (ignore_patterns[i].matches(asset_path, false)) {
          digest_file = false;
          break;
        }
      }
      auto asset_key = CaseInsensitiveStringView::create(asset_path);
      if (!asset_key)
        return std::unexpected(std::move(asset_key.error()));
      auto descriptor = m_files.get(*asset_key);
      if (!descriptor)
        return std::unexpected(std::move(descriptor.error()));
      if (digest_file) {
        add_size(descriptor->get().source, descriptor->get().source_name, std::cref(asset_path));
        for (auto const& pair : descriptor->get().patch_sources) {
          if (pair.first.utf8().find_first_of(":?") == std::string::npos) {
            add_size(pair.second, pair.first);
          } else {
            patch_names.push_back(AssetPath::remove_sub_path(pair.first));
            add_size(pair.second, patch_names.back());
          }
        }
      }
    }
    return {};
  }();
  for (auto& group : size_groups)
    group.sizes = group.source.get().file_sizes(group.paths);
  for (auto const& entry : digest_entries) {
    if (entry.path)
      digest.push(entry.path->get());
    auto& size = size_groups[entry.group].sizes[entry.index];
    if (!size)
      return std::unexpected(std::move(size.error()));
    auto serialized = DataStreamBuffer::serialize(*size);
    if (!serialized)
      return std::unexpected(std::move(serialized.error()));
    digest.push(*serialized);
  }
  if (!prepared)
    return std::unexpected(std::move(prepared.error()));
  m_digest = digest.compute();

  // The owning factory destroys this object on failure, stopping and joining
  // all workers while their mutexes, Lua engine, and asset sources are alive.
  m_worker_threads.reserve(m_settings.worker_pool_size);
  m_worker_stop_source = std::stop_source{};
  for (unsigned i = 0; i < m_settings.worker_pool_size; ++i) {
    auto worker = Thread::invoke("Assets::workerMain", std::mem_fn(&Assets::worker_main), std::ref(*this), m_worker_stop_source.get_token());
    if (!worker)
      return std::unexpected(std::move(worker.error()));
    m_worker_threads.push_back(std::move(*worker));
  }

  // preload.config contains files which will be loaded and then told to persist.
  auto preload = json("/preload.config");
  if (!preload)
    return std::unexpected(std::move(preload.error()));
  auto logged = m_logger.info("Preloading assets");
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  auto preload_scripts = preload->iterate_array();
  if (!preload_scripts)
    return std::unexpected(std::move(preload_scripts.error()));
  constexpr std::array<std::pair<std::string_view, AssetType>, 5> asset_types{{
      {"json", AssetType::Json},
      {"image", AssetType::Image},
      {"audio", AssetType::Audio},
      {"font", AssetType::Font},
      {"bytes", AssetType::Bytes}
  }};
  for (auto const& script : *preload_scripts) {
    auto type_name = script.get_string("type");
    if (!type_name)
      return std::unexpected(std::move(type_name.error()));
    auto lower_type_name = type_name->to_lower();
    if (!lower_type_name)
      return std::unexpected(std::move(lower_type_name.error()));
    std::optional<AssetType> type;
    for (auto const& [name, candidate] : asset_types) {
      if (lower_type_name->utf8() == name) {
        type = candidate;
        break;
      }
    }
    if (!type)
      return std::unexpected(AssetException::format("Unknown asset type '{}' in preload.config", *type_name));
    auto path = script.get_string("path");
    if (!path)
      return std::unexpected(std::move(path.error()));
    auto components = AssetPath::split(*path);
    auto validated = validate_path(components, *type == AssetType::Json || *type == AssetType::Image, *type == AssetType::Image);
    if (!validated)
      return std::unexpected(std::move(validated.error()));
    auto asset = get_asset(AssetId{.type = *type, .path = std::move(components)}, true);
    if (!asset)
      return std::unexpected(std::move(asset.error()));
  }
  return {};
}

Assets::~Assets() {
  if (auto result = stop_workers(); !result) {
    if (auto logged =
          m_logger.error_exception("Asset worker failed during shutdown", ExceptionDiagnostic(result.error()));
        !logged) {
      EmergencyLog::error("Asset worker failed during shutdown: {}", Star::exception_report(result.error()));
      EmergencyLog::error("Asset worker shutdown logging failed: {}", Star::exception_report(logged.error()));
    }
  }
}

std::expected<void, StarException> Assets::stop_workers() {
  {
    std::lock_guard locker(m_assets_mutex);
    m_worker_stop_source.request_stop();
    m_assets_queued.notify_all();
    m_assets_done.notify_all();
  }
  std::optional<StarException> error;
  for (auto& worker : m_worker_threads) {
    auto result = worker.finish();
    if (!result && !error)
      error = std::move(result.error());
  }
  m_worker_threads.clear();
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

void Assets::hot_reload() const {
  std::lock_guard lock(m_assets_mutex);
  ++m_load_epoch;
  m_assets_cache.clear();
  m_asset_errors.clear();
  m_queue.clear();
  // The source index is unchanged; only parsed contents become stale.
  m_frames_by_file.clear();
  m_patched_frames_by_image.clear();
  m_assets_done.notify_all();
  m_assets_queued.notify_all();
}

StringList Assets::asset_sources() const {
  std::lock_guard assets_locker(m_assets_mutex);
  return m_asset_sources;
}

std::expected<JsonObject, StarException> Assets::asset_source_metadata(String const& source_name) const {
  std::lock_guard assets_locker(m_assets_mutex);
  auto source = m_asset_source_paths.get_right(source_name);
  if (!source)
    return std::unexpected(std::move(source.error()));
  return source->get()->metadata();
}

ByteArray Assets::digest() const {
  std::lock_guard assets_locker(m_assets_mutex);
  return m_digest;
}

std::expected<bool, StarException> Assets::asset_exists(String const& path) const {
  auto key = CaseInsensitiveStringView::create(path);
  if (!key)
    return std::unexpected(std::move(key.error()));
  std::lock_guard assets_locker(m_assets_mutex);
  return m_files.contains(*key);
}

std::expected<std::optional<Assets::AssetFileDescriptor>, StarException> Assets::asset_descriptor(String const& path) const {
  auto key = CaseInsensitiveStringView::create(path);
  if (!key)
    return std::unexpected(std::move(key.error()));
  std::lock_guard assets_locker(m_assets_mutex);
  return m_files.maybe(*key);
}

std::expected<String, StarException> Assets::asset_source(String const& path) const {
  auto key = CaseInsensitiveStringView::create(path);
  if (!key)
    return std::unexpected(std::move(key.error()));
  std::lock_guard assets_locker(m_assets_mutex);
  if (auto p = m_files.find_ref(*key)) {
    auto source = m_asset_source_paths.get_left(p->get().source);
    if (!source)
      return std::unexpected(std::move(source.error()));
    return source->get();
  }
  return std::unexpected(AssetException(Star::format("No such asset '{}'", path)));
}

std::optional<String> Assets::asset_source_path(std::shared_ptr<AssetSource> const& source) const {
  std::lock_guard assets_locker(m_assets_mutex);
  return m_asset_source_paths.maybe_left(source);
}

std::expected<StringList, StarException> Assets::scan(String const& suffix) const {
  return scan(String{}, suffix);
}

std::expected<StringList, StarException> Assets::scan(String const& prefix, String const& suffix) const {
  // Index keys are validated UTF-8. String's case-insensitive prefix/suffix
  // comparisons fold ASCII bytes only; validate the query once, not per key.
  auto equal_byte = [](unsigned char lhs, unsigned char rhs) {
    return String::char_equal(lhs, rhs, String::CaseInsensitive);
  };
  auto prefix_bytes = std::string_view(prefix.utf8());
  auto suffix_bytes = std::string_view(suffix.utf8());
  bool prefix_checked = prefix_bytes.empty();
  StringList result;
  auto append_candidate = [&](String const& path) -> std::expected<void, StarException> {
    // A malformed prefix was never examined when the suffix had no hits.
    if (!prefix_checked) {
      if (auto valid = prefix.size(); !valid)
        return std::unexpected(std::move(valid.error()));
      prefix_checked = true;
    }
    if (prefix_bytes.empty() || std::ranges::equal(std::string_view(path.utf8()).substr(0, prefix_bytes.size()),
                                                  prefix_bytes, equal_byte))
      result.push_back(path);
    return {};
  };

  if (suffix_bytes.starts_with('.') && suffix_bytes.find('.', 1) == std::string_view::npos) {
    auto extension = scan_extension(suffix);
    if (!extension)
      return std::unexpected(std::move(extension.error()));
    if (prefix_bytes.empty())
      result.reserve(extension->get().size());
    for (auto const& key : extension->get()) {
      if (auto appended = append_candidate(key.string()); !appended)
        return std::unexpected(std::move(appended.error()));
    }
  } else {
    if (!suffix_bytes.empty() && !m_files.empty()) {
      if (auto valid = suffix.size(); !valid)
        return std::unexpected(std::move(valid.error()));
    }
    if (suffix_bytes.empty() && prefix_bytes.empty())
      result.reserve(m_files.size());
    for (auto const& file : m_files) {
      auto const& path = file.first.string();
      auto bytes = std::string_view(path.utf8());
      if (!suffix_bytes.empty() && (bytes.size() < suffix_bytes.size()
          || !std::ranges::equal(bytes.substr(bytes.size() - suffix_bytes.size()), suffix_bytes, equal_byte)))
        continue;
      if (auto appended = append_candidate(path); !appended)
        return std::unexpected(std::move(appended.error()));
    }
  }
  return result;
}

const CaseInsensitiveStringSet empty_extension_scan;

std::expected<std::reference_wrapper<CaseInsensitiveStringSet const>, StarException> Assets::scan_extension(String const& extension) const {
  auto bytes = std::string_view(extension.utf8());
  // The extension separator is ASCII; stripping it never splits a UTF8 scalar.
  auto key = CaseInsensitiveStringView::create(bytes.starts_with(".") ? bytes.substr(1) : bytes);
  if (!key)
    return std::unexpected(std::move(key.error()));
  auto found = m_files_by_extension.find(*key);
  return std::cref(found != m_files_by_extension.end() ? found->second : empty_extension_scan);
}

std::expected<Json, StarException> Assets::json(String const& path) const {
  auto components = AssetPath::split(path);
  if (auto valid = validate_path(components, true, false); !valid)
    return std::unexpected(std::move(valid.error()));
  auto data = get_asset(AssetId{.type = AssetType::Json, .path = std::move(components)});
  if (!data)
    return std::unexpected(std::move(data.error()));
  return cached_data_ref<JsonData>(**data)->get().json;
}

std::expected<Json, StarException> Assets::fetch_json(Json const& v, String const& dir) const {
  if (!v.is_type(Json::Type::String))
    return v;
  auto path = v.to_string();
  if (!path)
    return std::unexpected(std::move(path.error()));
  return json(AssetPath::relative_to(dir, *path));
}

std::expected<Assets::AssetId, StarException> Assets::prepare_queued_asset(AssetType type, String const& path) {
  auto components = AssetPath::split(path);
  if (auto valid = validate_path(components, type != AssetType::Audio, type == AssetType::Image); !valid)
    return std::unexpected(std::move(valid.error()));
  if (type == AssetType::Json)
    return AssetId{.type = type, .path = {std::move(components.base_path), {}, {}}};
  return AssetId{.type = type, .path = std::move(components)};
}

std::expected<std::shared_ptr<Image const>, StarException> Assets::image(AssetPath const& path) const {
  auto data = get_asset({.type = AssetType::Image, .path = path});
  if (!data)
    return std::unexpected(std::move(data.error()));
  return cached_data_ref<ImageData>(**data)->get().image;
}

std::expected<std::shared_ptr<Image const>, StarException> Assets::try_image(AssetPath const& path) const {
  auto data = try_asset({.type = AssetType::Image, .path = path});
  if (!data)
    return std::unexpected(std::move(data.error()));
  if (*data)
    return cached_data_ref<ImageData>(**data)->get().image;
  return std::shared_ptr<Image const>{};
}

std::expected<std::shared_ptr<FramesSpecification const>, StarException> Assets::image_frames(String const& path) const {
  auto components = AssetPath::split(path);
  if (auto valid = validate_path(components, false, false); !valid)
    return std::unexpected(std::move(valid.error()));
  if (AssetPath::is_embedded(components.base_path)) {
    auto image = get_asset({.type = AssetType::Image, .path = std::move(components)});
    if (!image)
      return std::unexpected(std::move(image.error()));
    return cached_data_ref<ImageData>(**image)->get().frames;
  }
  std::unique_lock assets_locker(m_assets_mutex);
  return best_frames_specification(path);
}

std::expected<std::shared_ptr<Audio const>, StarException> Assets::audio(String const& path) const {
  auto components = AssetPath::split(path);
  if (auto valid = validate_path(components, false, false); !valid)
    return std::unexpected(std::move(valid.error()));
  auto data = get_asset({.type = AssetType::Audio, .path = std::move(components)});
  if (!data)
    return std::unexpected(std::move(data.error()));
  return cached_data_ref<AudioData>(**data)->get().audio;
}

std::expected<std::shared_ptr<Audio const>, StarException> Assets::try_audio(String const& path) const {
  auto components = AssetPath::split(path);
  if (auto valid = validate_path(components, false, false); !valid)
    return std::unexpected(std::move(valid.error()));
  auto data = try_asset({.type = AssetType::Audio, .path = std::move(components)});
  if (!data)
    return std::unexpected(std::move(data.error()));
  if (*data)
    return cached_data_ref<AudioData>(**data)->get().audio;
  return std::shared_ptr<Audio const>{};
}

std::expected<std::shared_ptr<Font const>, StarException> Assets::font(String const& path) const {
  auto components = AssetPath::split(path);
  if (auto valid = validate_path(components, false, false); !valid)
    return std::unexpected(std::move(valid.error()));
  auto data = get_asset({.type = AssetType::Font, .path = std::move(components)});
  if (!data)
    return std::unexpected(std::move(data.error()));
  return cached_data_ref<FontData>(**data)->get().font;
}

std::expected<std::shared_ptr<ByteArray const>, StarException> Assets::bytes(String const& path, bool allow_remote) const {
  auto components = AssetPath::split(path);
  if (AssetPath::is_remote(components.base_path) && !allow_remote)
    return std::unexpected(AssetException("Reading embedded bytes requires explicit allowRemote permission"));
  if (auto valid = validate_path(components, false, false); !valid)
    return std::unexpected(std::move(valid.error()));
  auto data = get_asset({.type = AssetType::Bytes, .path = std::move(components)});
  if (!data)
    return std::unexpected(std::move(data.error()));
  return cached_data_ref<BytesData>(**data)->get().bytes;
}

std::expected<std::shared_ptr<IODevice>, StarException> Assets::open_file(String const& path) const {
  return open(path);
}

void Assets::clear_cache() {
  std::lock_guard assets_locker(m_assets_mutex);

  // Clear all assets that are not queued or broken.
  for (auto it = m_assets_cache.begin(); it != m_assets_cache.end();) {
    auto const& pair = *it;
    // Don't clean up queued, persistent, or broken assets.
    if (pair.second && !pair.second->should_persist() && !m_queue.contains(pair.first))
      it = m_assets_cache.erase(it);
    else
      ++it;
  }
}

void Assets::cleanup() {
  std::lock_guard lock(m_assets_mutex);

  auto time = std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now());
  if (m_settings.reference_image_time_to_live > m_settings.asset_time_to_live && m_settings.reference_image_pixel_budget != 0) {
    std::vector<std::reference_wrapper<ImageData>> references;
    for (auto const& entry : m_assets_cache) {
      if (entry.first.type == AssetType::Image && entry.second) {
        auto& image = cached_data_ref<ImageData>(*entry.second)->get();
        image.extended_ttl = false;
        if (image.is_reference && image.image)
          references.emplace_back(image);
      }
    }
    std::sort(references.begin(), references.end(),
              [](ImageData const& lhs, ImageData const& rhs) { return lhs.time > rhs.time; });
    std::uint64_t pixels = 0;
    for (ImageData& image : references) {
      auto size = std::uint64_t(image.image->width()) * image.image->height();
      if (size > m_settings.reference_image_pixel_budget - pixels)
        continue;
      pixels += size;
      image.extended_ttl = true;
    }
  }

  for (auto it = m_assets_cache.begin(); it != m_assets_cache.end();) {
    auto const& pair = *it;
    // Don't clean up broken assets or queued assets.
    if (pair.second && !m_queue.contains(pair.first)) {
      auto live_time = time - pair.second->time;
      auto time_to_live = std::chrono::duration<float>(
        pair.second->extended_ttl ? m_settings.reference_image_time_to_live : m_settings.asset_time_to_live);
      if (live_time > time_to_live) {
        // If the asset should persist, just refresh the access time.
        if (pair.second->should_persist())
          pair.second->time = time;
        else {
          it = m_assets_cache.erase(it);
          continue;
        }
      }
    }
    ++it;
  }
}

bool Assets::AssetId::operator==(AssetId const& asset_id) const {
  return std::tie(type, path) == std::tie(asset_id.type, asset_id.path);
}

std::size_t Assets::AssetIdHash::operator()(AssetId const& id) const {
  return hash_of(id.type, id.path.base_path, id.path.sub_path, id.path.directives);
}

bool Assets::JsonData::should_persist() const {
  return force_persist || !json.unique();
}

bool Assets::ImageData::should_persist() const {
  return force_persist || (!alias && image.use_count() != 1);
}

bool Assets::AudioData::should_persist() const {
  return force_persist || audio.use_count() != 1;
}

bool Assets::FontData::should_persist() const {
  return force_persist || font.use_count() != 1 || font->shares_backing_buffer();
}

bool Assets::BytesData::should_persist() const {
  return force_persist || bytes.use_count() != 1;
}

std::expected<FramesSpecification, StarException> Assets::parse_frames_specification(Json const& frame_config, String path) {
  FramesSpecification spec;
  spec.frames_file = std::move(path);
  auto frame_list = frame_config.find("frameList");
  if (!frame_list)
    return std::unexpected(std::move(frame_list.error()));
  if (*frame_list) {
    auto list = (*frame_list)->get().iterate_object();
    if (!list)
      return std::unexpected(std::move(list.error()));
    for (auto const& pair : *list) {
      auto parsed_rect = json_to_recti(pair.second);
      if (!parsed_rect)
        return std::unexpected(std::move(parsed_rect.error()));
      RectU rect(*parsed_rect);
      if (rect.is_empty())
        return std::unexpected(AssetException(Star::format("Empty rect in frame specification in image {} frame {}", spec.frames_file, pair.first)));
      spec.frames[pair.first] = rect;
    }
  }
  auto frame_grid = frame_config.find("frameGrid");
  if (!frame_grid)
    return std::unexpected(std::move(frame_grid.error()));
  if (*frame_grid) {
    auto grid_result = (*frame_grid)->get().object_ptr();
    if (!grid_result)
      return std::unexpected(std::move(grid_result.error()));
    auto const& grid = **grid_result;
    Vec2I begin_value{};
    auto begin_result = grid.find_ref("begin");
    if (begin_result) {
      auto parsed_begin = json_to_vec2i(begin_result->get());
      if (!parsed_begin)
        return std::unexpected(std::move(parsed_begin.error()));
      begin_value = *parsed_begin;
    }
    auto size_config = grid.get("size");
    if (!size_config)
      return std::unexpected(std::move(size_config.error()));
    auto size_result = json_to_vec2i(size_config->get());
    if (!size_result)
      return std::unexpected(std::move(size_result.error()));
    auto dimensions_config = grid.get("dimensions");
    if (!dimensions_config)
      return std::unexpected(std::move(dimensions_config.error()));
    auto dimensions_result = json_to_vec2i(dimensions_config->get());
    if (!dimensions_result)
      return std::unexpected(std::move(dimensions_result.error()));
    Vec2U begin(begin_value), size(*size_result), dimensions(*dimensions_result);
    if (dimensions[0] == 0 || dimensions[1] == 0)
      return std::unexpected(AssetException(Star::format("Image {} \"dimensions\" in frameGrid cannot be zero", spec.frames_file)));
    if (auto names_config = grid.find_ref("names")) {
      auto names = names_config->get().array_ptr();
      if (!names)
        return std::unexpected(std::move(names.error()));
      for (std::size_t y = 0; y < (*names)->size(); ++y) {
        if (y >= dimensions[1])
          return std::unexpected(AssetException(Star::format("Image {} row {} is out of bounds for y-dimension {}", spec.frames_file, y + 1, dimensions[1])));
        auto const& row_config = (**names)[y];
        if (row_config.is_null())
          continue;
        auto row = row_config.array_ptr();
        if (!row)
          return std::unexpected(std::move(row.error()));
        for (std::size_t x = 0; x < (*row)->size(); ++x) {
          if (x >= dimensions[0])
            return std::unexpected(AssetException(Star::format("Image {} column {} is out of bounds for x-dimension {}", spec.frames_file, x + 1, dimensions[0])));
          auto const& frame = (**row)[x];
          if (frame.is_null())
            continue;
          auto name = frame.to_string();
          if (!name)
            return std::unexpected(std::move(name.error()));
          if (!name->empty())
            spec.frames[*name] = RectU::with_size(Vec2U(begin[0] + x * size[0], begin[1] + y * size[1]), size);
        }
      }
    } else {
      for (std::size_t y = 0; y < dimensions[1]; ++y)
        for (std::size_t x = 0; x < dimensions[0]; ++x)
          spec.frames[Star::to_string(y * dimensions[0] + x)] =
            RectU::with_size(Vec2U(begin[0] + x * size[0], begin[1] + y * size[1]), size);
    }
  }
  auto aliases_config = frame_config.opt("aliases");
  if (!aliases_config)
    return std::unexpected(std::move(aliases_config.error()));
  if (*aliases_config) {
    auto aliases = (**aliases_config).to_object();
    if (!aliases)
      return std::unexpected(std::move(aliases.error()));
    for (auto const& pair : *aliases) {
      auto parsed_value = pair.second.to_string();
      if (!parsed_value)
        return std::unexpected(std::move(parsed_value.error()));
      String value = std::move(*parsed_value);
      for (std::size_t i = 0; i <= aliases->size(); ++i) {
        auto alias = aliases->find_ref(value);
        if (!alias)
          break;
        if (i == aliases->size())
          return std::unexpected(AssetException(Star::format("Infinite alias loop detected for alias '{}'", pair.first)));
        auto next = alias->get().to_string();
        if (!next)
          return std::unexpected(std::move(next.error()));
        value = std::move(*next);
      }
      if (!spec.frames.contains(value))
        return std::unexpected(AssetException(Star::format("No such frame '{}' found for alias '{}'", value, pair.first)));
      spec.aliases[pair.first] = std::move(value);
    }
  }
  return spec;
}

std::expected<void, StarException> Assets::publish_queued_assets(std::span<AssetId> ids) const {
  std::size_t queued = 0;
  {
    std::lock_guard lock(m_assets_mutex);
    if (current_load_epoch() != m_load_epoch)
      return superseded_load();
    for (auto& id : ids) {
      if (auto cached = m_assets_cache.find_ref(id)) {
        if (cached->get())
          freshen(cached->get());
      } else if (!m_asset_errors.contains(id)) {
        queued += m_queue.try_emplace(std::move(id), QueuePriority::Load).second;
      }
    }
  }
  if (queued == 1)
    m_assets_queued.notify_one();
  else if (queued > 1)
    m_assets_queued.notify_all();
  return {};
}

std::expected<std::shared_ptr<Assets::AssetData>, StarException> Assets::try_asset(AssetId const& id) const {
  std::lock_guard assets_locker(m_assets_mutex);
  if (current_load_epoch() != m_load_epoch)
    return superseded_load();
  if (m_worker_error)
    return std::unexpected(*m_worker_error);
  if (m_worker_stop_source.stop_requested())
    return std::unexpected(AssetException("Asset loading stopped"));
  if (auto error = m_asset_errors.find_ref(id))
    return std::unexpected(error->get());
  if (auto cached = m_assets_cache.find_ref(id)) {
    if (cached->get())
      freshen(cached->get());
    return cached->get();
  }
  if (!m_queue.contains(id)) {
    m_queue[id] = QueuePriority::Load;
    m_assets_queued.notify_one();
  }
  return std::shared_ptr<AssetData>{};
}

std::expected<std::shared_ptr<Assets::AssetData>, StarException> Assets::get_asset(AssetId const& id, bool persist) const {
  std::unique_lock assets_locker(m_assets_mutex);
  while (true) {
    if (current_load_epoch() != m_load_epoch)
      return superseded_load();
    if (m_worker_error)
      return std::unexpected(*m_worker_error);
    if (m_worker_stop_source.stop_requested())
      return std::unexpected(AssetException("Asset loading stopped"));
    if (auto error = m_asset_errors.find_ref(id))
      return std::unexpected(error->get());
    if (auto cached = m_assets_cache.find_ref(id)) {
      if (cached->get()) {
        if (persist)
          cached->get()->force_persist = true;
        freshen(cached->get());
        return cached->get();
      }
      return std::unexpected(AssetException::format("Error loading asset {}", id.path));
    }
    auto worked = do_load(id);
    if (!worked)
      return std::unexpected(std::move(worked.error()));
    if (!*worked && !m_worker_stop_source.stop_requested())
      m_assets_done.wait(assets_locker);
  }
}

std::optional<Assets::WorkerTask> Assets::next_worker_task() const {
  auto post = m_queue.end();
  for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
    if (it->second == QueuePriority::Load)
      return WorkerTask{it->first, {}}; // load_asset claims it under the same lock.
    if (it->second == QueuePriority::PostProcess)
      post = it;
  }
  if (post == m_queue.end())
    return {};
  post->second = QueuePriority::Working;
  return WorkerTask{post->first, PostTask{m_load_epoch, m_assets_cache.value(post->first)}};
}

std::expected<void, StarException> Assets::worker_main(std::stop_token stop_token) {
  while (true) {
    std::unique_lock assets_locker(m_assets_mutex);
    if (stop_token.stop_requested())
      break;

    auto task = next_worker_task();
    if (!task) {
      // Only one worker collects a fully drained load burst. In particular,
      // queued Working entries still need the shared Lua VM, not an idle GC.
      if (m_queue.empty() && m_lua_gc_pending) {
        m_lua_gc_pending = false;
        assets_locker.unlock();
        std::expected<void, StarException> collected;
        {
          std::unique_lock lua_locker(m_lua_mutex, std::defer_lock);
          lock_with_diagnostics(lua_locker);
          collected = m_lua_engine->collect_garbage();
        }
        assets_locker.lock();
        if (!collected) {
          if (!m_worker_error)
            m_worker_error = collected.error();
          m_worker_stop_source.request_stop();
          m_assets_queued.notify_all();
          m_assets_done.notify_all();
          return std::unexpected(std::move(collected.error()));
        }
      }
      // Queueing and shutdown can happen while the asset mutex is released.
      m_assets_queued.wait(assets_locker, [&] {
        if (stop_token.stop_requested())
          return true;
        if (m_queue.empty() && m_lua_gc_pending)
          return true;
        for (auto const& entry : m_queue)
          if (entry.second == QueuePriority::Load || entry.second == QueuePriority::PostProcess)
            return true;
        return false;
      });
      continue;
    }

    std::expected<bool, StarException> worked;
    if (task->post) {
      assets_locker.unlock();
      auto processed = post_process_audio(*task->post);
      assets_locker.lock();
      worked = publish_post(task->id, std::move(*task->post), std::move(processed));
    } else {
      worked = do_load(task->id);
    }
    if (!worked) {
      if (!m_worker_error)
        m_worker_error = worked.error();
      m_worker_stop_source.request_stop();
      m_assets_queued.notify_all();
      m_assets_done.notify_all();
      return std::unexpected(std::move(worked.error()));
    }
    if (!*worked && !stop_token.stop_requested()) {
      // We are blocking on some sort of busy asset, so need to wait on
      // something to complete here, rather than spinning and burning cpu.
      m_assets_done.wait(assets_locker);
      continue;
    }

    // After processing an asset, unlock the main asset mutex and yield so we
    // don't starve other threads.
    assets_locker.unlock();
    std::this_thread::yield();
  }
  return {};
}

template <typename Function> requires std::invocable<Function&>
[[nodiscard]] auto Assets::unlock_during(Function&& function) const {
  // Borrow the caller's ownership: checked callbacks return before we restore
  // the mutex, including on failure, so its guard never outlives an unlocked mutex.
  m_assets_mutex.unlock();
  auto result = std::invoke(function);
  m_assets_mutex.lock();
  return result;
}

std::expected<void, StarException> Assets::invalidate_frame_file(String const& path) const {
  auto is_frames = path.ends_with(".frames", String::CaseInsensitive);
  if (!is_frames)
    return std::unexpected(std::move(is_frames.error()));
  if (!*is_frames)
    return {};
  // Load scripts can attach a patch or erase an indexed frame file and query
  // it again in the same group. Leave unrelated stateful patch results alone.
  for (auto it = m_frame_files_by_image.begin(); it != m_frame_files_by_image.end();) {
    auto matches = it->second.path.equals_ignore_case(path);
    if (!matches)
      return std::unexpected(std::move(matches.error()));
    if (*matches) {
      m_frames_by_file.remove(it->second.path);
      m_patched_frames_by_image.remove(it->first);
      it = m_frame_files_by_image.erase(it);
    } else {
      ++it;
    }
  }
  // Decoded images (and errors for their sub-frames) also depend on the spec.
  m_assets_cache.clear();
  m_asset_errors.clear();
  return {};
}

void Assets::clear_frame_catalog_cache() const {
  m_frame_files_by_image.clear();
  m_frames_by_file.clear();
  m_patched_frames_by_image.clear();
}

std::expected<Assets::FrameFile, StarException> Assets::find_frame_file(String const& image) const {
  auto path = std::string_view(image.utf8());
  auto slash = path.rfind('/');
  if (slash == std::string_view::npos)
    return FrameFile{};
  auto directory = path.substr(0, slash + 1);
  auto filename = path.substr(slash + 1);
  auto stem = filename.substr(0, filename.rfind('.'));
  constexpr std::string_view fallback = "default", extension = ".frames";
  std::string candidate;
  candidate.reserve(directory.size() + std::max(stem.size(), fallback.size()) + extension.size());
  while (!directory.empty()) {
    for (auto name : {stem, fallback}) {
      candidate.assign(directory).append(name).append(extension);
      auto key = CaseInsensitiveStringView::create(std::string_view(candidate));
      if (!key)
        return std::unexpected(std::move(key.error()));
      if (auto file = m_files.find_ref(*key)) {
        // Even JSON patches can load external, Lua-patched assets. Only an
        // unpatched file is independent of patch state and request spelling.
        if (file->get().patch_sources.empty())
          return FrameFile{.path = file->get().source_name, .shareable = true};
        return FrameFile{.path = String(std::move(candidate))};
      }
      if (stem == fallback)
        break;
    }
    directory.remove_suffix(1);
    auto parent = directory.rfind('/');
    directory = parent == std::string_view::npos ? std::string_view{} : directory.substr(0, parent + 1);
  }
  return FrameFile{};
}

std::expected<std::shared_ptr<FramesSpecification const>, StarException> Assets::best_frames_specification(String const& image) const {
  for (;;) {
    if (current_load_epoch() != m_load_epoch)
      return superseded_load();
    auto selected = m_frame_files_by_image.find(image);
    if (selected == m_frame_files_by_image.end()) {
      auto found = find_frame_file(image);
      if (!found)
        return std::unexpected(std::move(found.error()));
      if (found->path.empty()) {
        // A frames query need not name a catalog image. Do not intern
        // arbitrary missing names (or spelling variants) indefinitely.
        auto key = CaseInsensitiveStringView::create(image);
        if (!key)
          return std::unexpected(std::move(key.error()));
        auto file = m_files.find_ref(*key);
        if (!file || file->get().source_name != image)
          return std::shared_ptr<FramesSpecification const>{};
      }
      selected = m_frame_files_by_image.try_emplace(image, std::move(*found)).first;
    }
    auto const& file = selected->second;
    if (file.path.empty())
      return std::shared_ptr<FramesSpecification const>{};
    auto& cache = file.shareable ? m_frames_by_file : m_patched_frames_by_image;
    if (auto cached = cache.find_ref(file.shareable ? file.path : image))
      return cached->get();

    // Do not borrow a map entry across unlock: another image can rehash it.
    String frames_file = file.path;
    bool shareable = file.shareable;
    LoadEpochScope task(*this, m_load_epoch);
    auto parsed = unlock_during([&]() -> std::expected<std::shared_ptr<FramesSpecification const>, StarException> {
      auto config = read_json(frames_file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      auto spec = parse_frames_specification(*config, frames_file);
      if (!spec)
        return std::unexpected(std::move(spec.error()));
      return std::make_shared<FramesSpecification>(std::move(*spec));
    });
    if (task.epoch != m_load_epoch)
      continue; // Drop stale success or failure; only top-level lookups may retry.
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    if (shareable)
      return cache.try_emplace(std::move(frames_file), std::move(*parsed)).first->second;
    cache[image] = *parsed;
    return std::move(*parsed);
  }
}

std::expected<std::shared_ptr<IODevice>, StarException> Assets::open(String const& path) const {
  if (AssetPath::is_embedded(path)) {
    auto data = read_embedded(path, m_settings.allow_remote_assets, DecodeMode::RawBytes);
    if (!data)
      return std::unexpected(std::move(data.error()));
    return std::make_shared<Buffer>(std::move(data->bytes));
  }
  auto key = CaseInsensitiveStringView::create(path);
  if (!key)
    return std::unexpected(std::move(key.error()));
  if (auto p = m_files.find_ref(*key))
    return p->get().source->open(p->get().source_name);
  return std::unexpected(AssetException(Star::format("No such asset '{}'", path)));
}

std::expected<ByteArray, StarException> Assets::read(String const& path) const {
  if (AssetPath::is_embedded(path)) {
    auto data = read_embedded(path, m_settings.allow_remote_assets, DecodeMode::RawBytes);
    if (!data)
      return std::unexpected(std::move(data.error()));
    return std::move(data->bytes);
  }
  auto key = CaseInsensitiveStringView::create(path);
  if (!key)
    return std::unexpected(std::move(key.error()));
  if (auto p = m_files.find_ref(*key))
    return p->get().source->read(p->get().source_name);
  return std::unexpected(AssetException(Star::format("No such asset '{}'", path)));
}

std::expected<std::shared_ptr<Image const>, StarException> Assets::read_image(String const& path) const {
  auto key = CaseInsensitiveStringView::create(path);
  if (!key)
    return std::unexpected(std::move(key.error()));
  auto file = m_files.find_ref(*key);
  if (!file)
    return std::unexpected(AssetException(Star::format("No such asset '{}'", path)));
  auto stored = file->get().source->image(file->get().source_name);
  if (!stored)
    return std::unexpected(std::move(stored.error()));
  auto image = std::move(*stored);
  if (!image) {
    auto device = file->get().source->open(file->get().source_name);
    if (!device)
      return std::unexpected(std::move(device.error()));
    auto decoded = Image::read_png(m_logger, std::move(*device));
    if (!decoded)
      return std::unexpected(std::move(decoded.error()));
    image = std::make_shared<Image>(std::move(*decoded));
  }
  if (!file->get().patch_sources.empty())
    return apply_image_patches(std::move(image), path, file->get().patch_sources);
  return image;
}

std::expected<std::shared_ptr<Image const>, StarException> Assets::apply_image_patches(std::shared_ptr<Image const> image, String const& path, List<std::pair<String, std::shared_ptr<AssetSource>>> patches) const {
  std::unique_lock lua_locker(m_lua_mutex, std::defer_lock);
  lock_with_diagnostics(lua_locker);
  auto image_user_data = m_lua_engine->create_userdata(*image);
  if (!image_user_data)
    return std::unexpected(std::move(image_user_data.error()));
  LuaValue result = std::move(*image_user_data);
  lua_locker.unlock();
  for (auto const& pair : patches) {
    auto& patch_path = pair.first;
    auto& patch_source = pair.second;
    auto patch_stream = read_patch(*patch_source, patch_path, m_logger);
    if (!patch_stream)
      return std::unexpected(std::move(patch_stream.error()));
    if (!*patch_stream)
      continue;
    auto is_lua = patch_path.ends_with(".lua");
    if (!is_lua)
      return std::unexpected(std::move(is_lua.error()));
    if (*is_lua) {
      PatchContextKey context_key{.source = std::ref(*patch_source), .path = patch_path};
      lock_with_diagnostics(lua_locker);
      std::shared_ptr<LuaContext>& context = m_patch_contexts[context_key];
      if (!context) {
        auto created_context = m_lua_engine->create_context();
        if (!created_context)
          return std::unexpected(std::move(created_context.error()));
        auto new_context = std::make_shared<LuaContext>(std::move(*created_context));
        auto loaded = new_context->load((**patch_stream).span(), patch_path);
        if (!loaded)
          return std::unexpected(lua_patch_failure(std::move(loaded.error()), path, patch_path, *patch_source,
                                                 m_asset_source_paths.find_left_ref(patch_source)));
        context = std::move(new_context);
      }
      auto new_result = context->invoke_path<LuaValue>("patch", result, path);
      if (!new_result)
        return std::unexpected(lua_patch_failure(std::move(new_result.error()), path, patch_path, *patch_source,
                                               m_asset_source_paths.find_left_ref(patch_source)));
      if (!std::holds_alternative<LuaNilType>(*new_result)) {
        if (std::holds_alternative<LuaUserData>(*new_result)) {
          auto is_image = std::get<LuaUserData>(*new_result).is<Image>();
          if (!is_image)
            return std::unexpected(std::move(is_image.error()));
          if (*is_image) {
            result = std::move(*new_result);
          } else {
            auto logged = m_logger.warn("Patch '{}' for image '{}' returned a non-Image userdata value, ignoring", patch_path, path);
            if (!logged)
              return std::unexpected(std::move(logged.error()));
          }
        } else {
          auto logged = m_logger.warn("Patch '{}' for image '{}' returned a non-Image value, ignoring", patch_path, path);
          if (!logged)
            return std::unexpected(std::move(logged.error()));
        }
      }
      lua_locker.unlock();
    } else {
      auto logged = m_logger.warn("Patch '{}' for image '{}' isn't a Lua script, ignoring", patch_path, path);
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    }
  }
  auto result_image = std::get<LuaUserData>(result).get<Image>();
  if (!result_image)
    return std::unexpected(std::move(result_image.error()));
  return std::make_shared<Image>(std::move(result_image->get()));
}

std::expected<void, StarException> Assets::report_patch_rejection(String const& path,
    std::shared_ptr<AssetSource> const& source, StarException const& error) const {
  auto provider = source_diagnostic(*source, m_asset_source_paths.find_left_ref(source));
  if (error.is<JsonPatchTestFail>())
    return m_logger.log_exception(LogLevel::Debug,
                                  Star::format("Patch test failure from file {} in {}", path, provider),
                                  ExceptionDiagnostic(error));
  return m_logger.error_exception(Star::format("Could not apply patch from file {} in {}", path, provider),
                                  ExceptionDiagnostic(error));
}

std::expected<Json, StarException> Assets::check_patch_array(String const& path,
                                                           std::shared_ptr<AssetSource> const& source,
                                                           Json const result, JsonArray const& patch_data,
                                                           std::optional<Json> const external) const {
  auto external_ref = external.value_or(Json{});
  auto new_result = result;
  for (auto const& patch : patch_data) {
    switch (patch.type()) {
      case Json::Type::Array: { // Nested groups commit only if the whole group succeeds.
        auto patch_data = patch.array_ptr();
        if (!patch_data)
          return std::unexpected(std::move(patch_data.error()));
        auto patched = check_patch_array(path, source, new_result, **patch_data, external_ref);
        if (patched) {
          new_result = std::move(*patched);
        } else {
          auto const& error = patched.error();
          if (!error.is<JsonPatchTestFail>() && !error.is<JsonPatchException>())
            return std::unexpected(std::move(patched.error()));
          auto logged = report_patch_rejection(path, source, error);
          if (!logged)
            return std::unexpected(std::move(logged.error()));
        }
        break;
      }
      case Json::Type::Object: {
        auto patched = JsonPatching::apply_operation(new_result, patch);
        if (!patched)
          return std::unexpected(std::move(patched.error()));
        new_result = std::move(*patched);
        break;
      }
      case Json::Type::String: {
        auto reference_path = patch.to_string();
        if (!reference_path)
          return std::unexpected(std::move(reference_path.error()));
        auto reference = json(*reference_path);
        if (!reference)
          return std::unexpected(JsonPatchTestFail(Star::format("Unable to load reference asset: {}", *reference_path), reference.error()));
        external_ref = std::move(*reference);
        break;
      }
      default:
        return std::unexpected(JsonPatchException(Star::format("Patch data is wrong type: {}", Json::type_name(patch.type()))));
    }
  }
  return new_result;
}

std::expected<Json, StarException> Assets::read_json(String const& path) const {
  if (AssetPath::is_embedded(path)) {
    auto data = read_embedded(path, m_settings.allow_remote_assets);
    if (!data)
      return std::unexpected(std::move(data.error()));
    return embedded_data_to_json(*data);
  }
  auto stream_data = read(path);
  if (!stream_data)
    return std::unexpected(std::move(stream_data.error()));
  auto parsed = input_utf8_json(stream_data->begin(), stream_data->end(), JsonParseType::Top);
  if (!parsed)
    return std::unexpected(JsonParsingException(Star::format("Cannot parse json file: {}", path), parsed.error()));
  auto key = CaseInsensitiveStringView::create(path);
  if (!key)
    return std::unexpected(JsonParsingException(Star::format("Cannot parse json file: {}", path), key.error()));
  auto file = m_files.get(*key);
  if (!file)
    return std::unexpected(JsonParsingException(Star::format("Cannot parse json file: {}", path), file.error()));
  auto patched = apply_json_patches(*parsed, path, file->get().patch_sources);
  if (!patched)
    return std::unexpected(JsonParsingException(Star::format("Cannot parse json file: {}", path), patched.error()));
  return std::move(*patched);
}

std::expected<Json, StarException> Assets::apply_json_patches(Json const& input, String const& path, List<std::pair<String, std::shared_ptr<AssetSource>>> patches) const {
  Json result = input;
  for (auto const& pair : patches) {
    auto patch_asset_path = AssetPath::split(pair.first);
    auto& patch_base_path = patch_asset_path.base_path;
    auto& patch_source = pair.second;
    auto patch_stream = read_patch(*patch_source, patch_base_path, m_logger);
    if (!patch_stream)
      return std::unexpected(std::move(patch_stream.error()));
    if (!*patch_stream)
      continue;
    auto is_lua = patch_base_path.ends_with(".lua");
    if (!is_lua)
      return std::unexpected(std::move(is_lua.error()));
    if (*is_lua) {
      PatchContextKey context_key{.source = std::ref(*patch_source), .path = patch_base_path};
      std::unique_lock lua_locker(m_lua_mutex, std::defer_lock);
      lock_with_diagnostics(lua_locker);
      std::shared_ptr<LuaContext>& context = m_patch_contexts[context_key];
      if (!context) {
        auto created_context = m_lua_engine->create_context();
        if (!created_context)
          return std::unexpected(std::move(created_context.error()));
        auto new_context = std::make_shared<LuaContext>(std::move(*created_context));
        auto loaded = new_context->load((**patch_stream).span(), patch_base_path);
        if (!loaded)
          return std::unexpected(lua_patch_failure(std::move(loaded.error()), path, pair.first, *patch_source,
                                                 m_asset_source_paths.find_left_ref(patch_source)));
        context = std::move(new_context);
      }
      auto new_result = context->invoke_path<Json>("patch", result, path);
      if (!new_result)
        return std::unexpected(lua_patch_failure(std::move(new_result.error()), path, pair.first, *patch_source,
                                               m_asset_source_paths.find_left_ref(patch_source)));
      if (*new_result)
        result = std::move(*new_result);
    } else {
      auto apply_patch = [&]() -> std::expected<void, StarException> {
        auto& bytes = **patch_stream;
        auto patch_json = input_utf8_json(bytes.begin(), bytes.end(), JsonParseType::Top);
        if (!patch_json)
          return std::unexpected(std::move(patch_json.error()));
        if (patch_asset_path.sub_path) {
          auto selected = patch_json->query(*patch_asset_path.sub_path);
          if (!selected)
            return std::unexpected(std::move(selected.error()));
          *patch_json = std::move(*selected);
        }
        if (patch_json->is_type(Json::Type::Array)) {
          auto patch_data = patch_json->array_ptr();
          if (!patch_data)
            return std::unexpected(std::move(patch_data.error()));
          auto patched = check_patch_array(pair.first, patch_source, result, **patch_data, {});
          if (patched) {
            result = std::move(*patched);
          } else {
            auto const& error = patched.error();
            if (!error.is<JsonPatchTestFail>() && !error.is<JsonPatchException>())
              return std::unexpected(std::move(patched.error()));
            auto logged = report_patch_rejection(pair.first, patch_source, error);
            if (!logged)
              return std::unexpected(std::move(logged.error()));
          }
        } else if (patch_json->is_type(Json::Type::Object)) {
          result = json_merge_nulling(result, *patch_json);
        }
        return {};
      };
      auto applied = apply_patch();
      if (!applied) {
        return std::unexpected(
          JsonParsingException(Star::format("Cannot parse json patch file '{}' from {}", patch_base_path,
                                    source_diagnostic(*patch_source, m_asset_source_paths.find_left_ref(patch_source))),
                               applied.error()));
      }
    }
  }
  return result;
}

std::expected<bool, StarException> Assets::do_load(AssetId const& id) const {
  auto const epoch = m_load_epoch;
  auto loaded = load_asset(id);
  if (epoch != m_load_epoch)
    return true; // Recheck the new epoch instead of waiting for or logging an obsolete task.
  if (loaded)
    return static_cast<bool>(*loaded);
  auto logged =
    m_logger.error_exception(Star::format("Error loading asset: {}", id.path), ExceptionDiagnostic(loaded.error()));
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  return true;
}

std::expected<bool, StarException> Assets::publish_post(AssetId const& id, PostTask task,
    std::expected<std::shared_ptr<AssetData>, StarException> processed) const {
  if (task.epoch != m_load_epoch)
    return true;

  // Optional decompression failure retains the readable compressed original.
  std::expected<void, StarException> logged;
  if (!processed) {
    auto error = resource_failure(std::move(processed.error()), id.path.base_path);
    logged = m_logger.error_exception(Star::format("Error post-processing asset: {}", id.path), ExceptionDiagnostic(error));
  } else {
    (*processed)->force_persist = task.original->force_persist;
    task.original = std::move(*processed);
  }
  task.original->needs_post_processing = false;
  freshen(task.original);
  m_assets_cache[id] = std::move(task.original);
  m_queue.remove(id);
  m_assets_done.notify_all();
  if (m_queue.empty() && m_lua_gc_pending)
    m_assets_queued.notify_one();
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  return true;
}

void Assets::publish_asset_failure(AssetId const& id, StarException const& error) const {
  m_queue.remove(id);
  m_assets_cache[id] = {};
  m_asset_errors.set(id, error);
  m_assets_done.notify_all();
  if (m_queue.empty() && m_lua_gc_pending)
    m_assets_queued.notify_one();
}

std::expected<std::shared_ptr<Assets::AssetData>, StarException> Assets::load_asset(AssetId const& id) const {
  if (current_load_epoch() != m_load_epoch)
    return superseded_load();
  if (auto error = m_asset_errors.find_ref(id))
    return std::unexpected(error->get());
  if (auto asset = m_assets_cache.value(id))
    return asset;
  if (m_queue.value(id, QueuePriority::None) == QueuePriority::Working)
    return std::shared_ptr<AssetData>{};
  LoadEpochScope task(*this, m_load_epoch);
  m_lua_gc_pending = true;
  m_queue[id] = QueuePriority::Working;
  auto loaded = [&]() -> std::expected<std::shared_ptr<AssetData>, StarException> {
    switch (id.type) {
      case AssetType::Json: return load_json(id.path);
      case AssetType::Image: return load_image(id.path);
      case AssetType::Audio: return load_audio(id.path);
      case AssetType::Font: return load_font(id.path);
      case AssetType::Bytes: return load_bytes(id.path);
    }
    return std::unexpected(AssetException("Unrecognized asset type"));
  }();
  if (task.epoch != m_load_epoch)
    return superseded_load();
  if (!loaded) {
    loaded.error() = resource_failure(std::move(loaded.error()), id.path.base_path);
    if (id.type == AssetType::Image && m_settings.missing_image) {
      auto logged =
        m_logger.error_exception(Star::format("Could not load image asset '{}', using placeholder default.", id.path),
                                 ExceptionDiagnostic(loaded.error()));
      if (logged) {
        loaded = load_image({*m_settings.missing_image, {}, {}});
        if (!loaded)
          loaded.error() = resource_failure(std::move(loaded.error()), *m_settings.missing_image);
      } else {
        loaded = std::unexpected(std::move(logged.error()));
      }
    } else if (id.type == AssetType::Audio && m_settings.missing_audio) {
      auto logged =
        m_logger.error_exception(Star::format("Could not load audio asset '{}', using placeholder default.", id.path),
                                 ExceptionDiagnostic(loaded.error()));
      if (logged) {
        loaded = load_audio({*m_settings.missing_audio, {}, {}});
        if (!loaded)
          loaded.error() = resource_failure(std::move(loaded.error()), *m_settings.missing_audio);
      } else {
        loaded = std::unexpected(std::move(logged.error()));
      }
    }
    if (task.epoch != m_load_epoch) // The placeholder load may also have unlocked.
      return superseded_load();
  }
  if (!loaded) {
    publish_asset_failure(id, loaded.error());
    return std::unexpected(std::move(loaded.error()));
  }
  auto asset = std::move(*loaded);
  if (asset) {
    if (asset->needs_post_processing) {
      m_queue[id] = QueuePriority::PostProcess;
      m_assets_queued.notify_one();
    } else
      m_queue.remove(id);
    m_assets_cache[id] = asset;
    m_assets_done.notify_all();
    freshen(asset);
    if (m_queue.empty() && m_lua_gc_pending)
      m_assets_queued.notify_one();
  } else {
    m_queue[id] = QueuePriority::Load;
    m_assets_queued.notify_one();
    if (auto moved = m_queue.move_to_back(id); !moved) {
      publish_asset_failure(id, moved.error());
      return std::unexpected(std::move(moved.error()));
    }
  }
  return asset;
}

std::expected<std::shared_ptr<Assets::AssetData>, StarException> Assets::load_json(AssetPath const& path) const {
  if (path.sub_path) {
    auto top = load_asset({.type = AssetType::Json, .path = {path.base_path, {}, {}}});
    if (!top)
      return std::unexpected(std::move(top.error()));
    if (!*top)
      return std::shared_ptr<AssetData>{};
    auto value = cached_data_ref<JsonData>(**top)->get().json.query(*path.sub_path);
    if (!value)
      return std::unexpected(AssetException(Star::format("Could not read JSON value {}", path), value.error()));
    auto data = std::make_shared<JsonData>();
    data->json = std::move(*value);
    return data;
  }
  return unlock_during([&]() -> std::expected<std::shared_ptr<AssetData>, StarException> {
    auto value = read_json(path.base_path);
    if (!value)
      return std::unexpected(AssetException(Star::format("Could not read JSON asset {}", path), value.error()));
    auto data = std::make_shared<JsonData>();
    data->json = std::move(*value);
    return data;
  });
}

std::expected<std::shared_ptr<Assets::AssetData>, StarException> Assets::load_image(AssetPath const& path) const {
  if (auto valid = validate_path(path, true, true); !valid)
    return std::unexpected(std::move(valid.error()));
  if (!path.directives.empty()) {
    auto loaded = load_asset({.type = AssetType::Image, .path = {path.base_path, path.sub_path, {}}});
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
    if (!*loaded)
      return std::shared_ptr<AssetData>{};
    // Keep loaded owning the wrapper throughout the unlocked image operation.
    auto& source = cached_data_ref<ImageData>(**loaded)->get();
    StringMap<std::shared_ptr<Image const>> references;
    StringList reference_paths;
    for (auto const& directives : path.directives.list())
      for (auto const& operation : directives.operations())
        add_image_operation_references(operation, reference_paths);
    for (auto const& ref : reference_paths) {
      auto components = AssetPath::split(ref);
      if (auto valid = validate_path(components, true, false); !valid)
        return std::unexpected(std::move(valid.error()));
      auto reference = load_asset({.type = AssetType::Image, .path = std::move(components)});
      if (!reference)
        return std::unexpected(std::move(reference.error()));
      if (!*reference)
        return std::shared_ptr<AssetData>{};
      auto& image = cached_data_ref<ImageData>(**reference)->get();
      image.is_reference = true;
      freshen(*reference);
      references[ref] = image.image;
    }
    return unlock_during([&]() -> std::expected<std::shared_ptr<AssetData>, StarException> {
      auto image = path.directives.apply_new_image(
        *source.image,
        [&](String const& ref) -> std::expected<std::optional<std::reference_wrapper<Image const>>, StarException> {
          if (auto reference = references.find_ref(ref))
            return std::optional{std::cref(*reference->get())};
          return std::unexpected(AssetException(Star::format("No such image directive reference '{}'", ref)));
        });
      if (!image)
        return std::unexpected(std::move(image.error()));
      auto data = std::make_shared<ImageData>();
      data->image = std::make_shared<Image>(std::move(*image));
      return data;
    });
  }
  if (path.sub_path) {
    auto loaded = load_asset({.type = AssetType::Image, .path = {path.base_path, {}, {}}});
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
    if (!*loaded)
      return std::shared_ptr<AssetData>{};
    auto& source = cached_data_ref<ImageData>(**loaded)->get();
    if (!source.frames)
      return std::unexpected(AssetException::format(
        "No associated frames file found for image '{}' while resolving image frame '{}'", path.base_path, path));
    if (auto alias = source.frames->aliases.find_ref(*path.sub_path)) {
      auto target = load_asset({.type = AssetType::Image, .path = {path.base_path, alias->get(), path.directives}});
      if (!target)
        return std::unexpected(std::move(target.error()));
      if (!*target)
        return std::shared_ptr<AssetData>{};
      auto data = std::make_shared<ImageData>();
      data->image = cached_data_ref<ImageData>(**target)->get().image;
      data->alias = true;
      return data;
    }
    auto frame = source.frames->frames.find_ref(*path.sub_path);
    if (!frame)
      return std::unexpected(AssetException(
        Star::format("No such frame {} in frames spec {}", *path.sub_path, source.frames->frames_file)));
    return unlock_during([&]() -> std::expected<std::shared_ptr<AssetData>, StarException> {
      auto image = source.image->sub_image(Vec2U(frame->get().x_min(), source.image->height() - frame->get().y_max()),
                                           frame->get().size());
      if (!image)
        return std::unexpected(std::move(image.error()));
      auto data = std::make_shared<ImageData>();
      data->image = std::make_shared<Image>(std::move(*image));
      return data;
    });
  }
  if (AssetPath::is_embedded(path.base_path)) {
    return unlock_during([&]() -> std::expected<std::shared_ptr<AssetData>, StarException> {
      auto embedded = read_embedded(path.base_path, m_settings.allow_remote_assets);
      if (!embedded)
        return std::unexpected(std::move(embedded.error()));
      if (embedded->type != EmbeddedAssetType::Binary && embedded->type != EmbeddedAssetType::Image)
        return std::unexpected(AssetException("Embedded image requires PNG binary data"));
      auto device = std::make_shared<ExternalBuffer>(embedded->bytes.span());
      auto metadata = Image::read_png_metadata(m_logger, device);
      if (!metadata)
        return std::unexpected(std::move(metadata.error()));
      auto size = std::get<0>(*metadata);
      if (std::uint64_t(size[0]) * size[1] > max_embedded_data_size / 4)
        return std::unexpected(AssetException("Embedded PNG exceeds the 64 MiB decoded pixel limit"));
      auto rewound = device->seek(0);
      if (!rewound)
        return std::unexpected(std::move(rewound.error()));
      auto image = Image::read_png(m_logger, std::move(device));
      if (!image)
        return std::unexpected(std::move(image.error()));
      auto data = std::make_shared<ImageData>();
      data->image = std::make_shared<Image>(std::move(*image));
      std::optional<Json> frame_config;
      String frame_name;
      if (std::holds_alternative<ExternalFramesPath>(embedded->frame_source)) {
        frame_name = std::move(std::get<ExternalFramesPath>(embedded->frame_source).path);
        auto frames = json(frame_name);
        if (!frames)
          return std::unexpected(std::move(frames.error()));
        frame_config = std::move(*frames);
      } else if (std::holds_alternative<EmbeddedFramesPath>(embedded->frame_source)) {
        frame_name = "<embedded frames>";
        auto frames = read_embedded(std::get<EmbeddedFramesPath>(embedded->frame_source).path, m_settings.allow_remote_assets);
        if (!frames)
          return std::unexpected(std::move(frames.error()));
        auto config = embedded_data_to_json(*frames);
        if (!config)
          return std::unexpected(std::move(config.error()));
        frame_config = std::move(*config);
      }
      if (frame_config) {
        auto frames = parse_frames_specification(*frame_config, std::move(frame_name));
        if (!frames)
          return std::unexpected(std::move(frames.error()));
        data->frames = std::make_shared<FramesSpecification>(std::move(*frames));
      }
      return data;
    });
  }
  auto image = unlock_during([&]() { return read_image(path.base_path); });
  if (!image)
    return std::unexpected(std::move(image.error()));
  auto frames = best_frames_specification(path.base_path);
  if (!frames)
    return std::unexpected(std::move(frames.error()));
  auto data = std::make_shared<ImageData>();
  data->image = std::move(*image);
  data->frames = std::move(*frames);
  return data;
}

std::expected<std::shared_ptr<Assets::AssetData>, StarException> Assets::load_audio(AssetPath const& path) const {
  return unlock_during([&]() -> std::expected<std::shared_ptr<AssetData>, StarException> {
    auto device = [&]() -> std::expected<std::shared_ptr<IODevice>, StarException> {
      if (!AssetPath::is_embedded(path.base_path))
        return open(path.base_path);
      auto embedded = read_embedded(path.base_path, m_settings.allow_remote_assets);
      if (!embedded)
        return std::unexpected(std::move(embedded.error()));
      if (embedded->type != EmbeddedAssetType::Binary)
        return std::unexpected(AssetException("Embedded audio requires binary data"));
      return std::make_shared<Buffer>(std::move(embedded->bytes));
    }();
    if (!device)
      return std::unexpected(std::move(device.error()));
    auto audio = Audio::create(std::move(*device), path.base_path);
    if (!audio)
      return std::unexpected(std::move(audio.error()));
    auto data = std::make_shared<AudioData>();
    data->audio = std::make_shared<Audio>(std::move(*audio));
    data->needs_post_processing = data->audio->compressed();
    return data;
  });
}

std::expected<std::shared_ptr<Assets::AssetData>, StarException> Assets::load_font(AssetPath const& path) const {
  return unlock_during([&]() -> std::expected<std::shared_ptr<AssetData>, StarException> {
    auto bytes = [&]() -> std::expected<ByteArray, StarException> {
      if (!AssetPath::is_embedded(path.base_path))
        return read(path.base_path);
      auto embedded = read_embedded(path.base_path, m_settings.allow_remote_assets);
      if (!embedded)
        return std::unexpected(std::move(embedded.error()));
      if (embedded->type != EmbeddedAssetType::Binary)
        return std::unexpected(AssetException("Embedded font requires binary data"));
      return std::move(embedded->bytes);
    }();
    if (!bytes)
      return std::unexpected(std::move(bytes.error()));
    auto font = Font::load(*m_font_library, std::make_shared<ByteArray>(std::move(*bytes)));
    if (!font)
      return std::unexpected(std::move(font.error()));
    auto data = std::make_shared<FontData>();
    data->font = std::move(*font);
    return data;
  });
}

std::expected<std::shared_ptr<Assets::AssetData>, StarException> Assets::load_bytes(AssetPath const& path) const {
  return unlock_during([&]() -> std::expected<std::shared_ptr<AssetData>, StarException> {
    auto bytes = read(path.base_path);
    if (!bytes)
      return std::unexpected(std::move(bytes.error()));
    auto data = std::make_shared<BytesData>();
    data->bytes = std::make_shared<ByteArray>(std::move(*bytes));
    return data;
  });
}

std::expected<std::shared_ptr<Assets::AssetData>, StarException> Assets::post_process_audio(PostTask const& task) const {
  LoadEpochScope scope(*this, task.epoch);
  auto audio_data = cached_data_ref<AudioData>(*task.original);
  if (!audio_data)
    return std::unexpected(AssetException("Expected cached audio for post-processing"));
  auto duration = audio_data->get().audio->total_time();
  if (!duration)
    return std::unexpected(std::move(duration.error()));
  if (*duration >= m_settings.audio_decompress_limit)
    return task.original;
  auto audio = audio_data->get().audio->clone();
  if (!audio)
    return std::unexpected(std::move(audio.error()));
  if (auto decompressed = audio->uncompress(); !decompressed)
    return std::unexpected(std::move(decompressed.error()));
  auto data = std::make_shared<AudioData>();
  data->audio = std::make_shared<Audio>(std::move(*audio));
  return data;
}

void Assets::freshen(std::shared_ptr<AssetData> const& asset) const {
  asset->time = std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now());
}

}
