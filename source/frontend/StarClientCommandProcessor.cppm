module;
import std;
import star.json;
import star.string;
import star.byte_array;
import star.encode;
import star.bytes;
import star.format;
import star.thread;
import star.time;
import star.data_stream;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.input_event;
import star.bi_map;
import star.text;
import star.audio;
import star.map;
import star.set;
import star.rpc_promise;
import star.game_types;
import star.casting;
import star.listener;
import star.ordered_map;

import star.lua_components;
import star.lua_root;

import star.drawable;
import star.font_texture_group;
import star.anchor_types;
import star.text_painter;
import star.mixer;

import star.cinematic;

import star.asset_texture_group;
import star.drawable_painter;
import star.gui_types;
import star.key_bindings;
import star.gui_context;
import star.widget;
import star.pane;
import star.game_timers;
import star.pane_manager;
import star.registered_pane_manager;

import star.main_interface_types;

namespace Star {
class UniverseClient;
class Cinematic;
}

namespace Star {

class Quest;

class PendingClientCommands {
public:
  void push(RpcPromise<Json> promise) { m_pending.push_back(std::move(promise)); }
  [[nodiscard]] std::expected<void, StarException> poll(StringList& replies);

private:
  std::vector<RpcPromise<Json>> m_pending;
  bool m_polling = false;
};

class ClientCommandProcessor;
template <typename Function>
concept client_builtin_command = requires(Function function, ClientCommandProcessor& self, String const& arguments) {
  { (self.*function)(arguments) } -> std::same_as<std::expected<String, StarException>>;
} || requires(Function function, ClientCommandProcessor& self) {
  { (self.*function)() } -> std::same_as<std::expected<String, StarException>>;
};

class ClientCommandProcessor {
public:
  ClientCommandProcessor(GuiContext& context, std::shared_ptr<UniverseClient> universeClient, std::shared_ptr<Cinematic> cinematicOverlay,
      MainInterfacePaneManager& paneManager, StringMap<StringList> macroCommands);
  void detachInterface() { m_paneManager.reset(); }

  [[nodiscard]] std::expected<StringList, StarException> handle_command(String const& command_line);
  [[nodiscard]] std::expected<void, StarException> poll_replies(StringList& replies) { return m_pending.poll(replies); }

  bool debugDisplayEnabled() const;
  bool debugHudEnabled() const;
  bool fixedCameraEnabled() const;

private:
  [[nodiscard]] std::expected<bool, StarException> adminCommandAllowed() const;
  [[nodiscard]] std::expected<String, StarException> previewQuestDialog(StringList const& arguments, String const& kind);
  using MacroPath = std::vector<std::string_view>;
  [[nodiscard]] std::expected<void, StarException> append_command(String const& command_line, StringList& output, MacroPath& path);
  [[nodiscard]] std::expected<void, StarException> execute_command(String const& command_line, StringList& output, MacroPath& path);

  [[nodiscard]] std::expected<String, StarException> reload();
  [[nodiscard]] std::expected<String, StarException> hotReload();
  [[nodiscard]] std::expected<String, StarException> whoami();
  [[nodiscard]] std::expected<String, StarException> gravity();
  [[nodiscard]] std::expected<String, StarException> debug(String const& argumentsString);
  [[nodiscard]] std::expected<String, StarException> boxes();
  [[nodiscard]] std::expected<String, StarException> fullbright();
  [[nodiscard]] std::expected<String, StarException> asyncLighting();
  [[nodiscard]] std::expected<String, StarException> setGravity(String const& argumentsString);
  [[nodiscard]] std::expected<String, StarException> resetGravity();
  [[nodiscard]] std::expected<String, StarException> fixedCamera();
  [[nodiscard]] std::expected<String, StarException> monochromeLighting();
  [[nodiscard]] std::expected<String, StarException> radio_message(String const& argumentsString);
  [[nodiscard]] std::expected<String, StarException> clear_radio_messages();
  [[nodiscard]] std::expected<String, StarException> clearCinematics();
  [[nodiscard]] std::expected<String, StarException> startQuest(String const& argumentsString);
  [[nodiscard]] std::expected<String, StarException> completeQuest(String const& argumentsString);
  [[nodiscard]] std::expected<String, StarException> failQuest(String const& argumentsString);
  [[nodiscard]] std::expected<String, StarException> previewNewQuest(String const& argumentsString);
  [[nodiscard]] std::expected<String, StarException> previewQuestComplete(String const& argumentsString);
  [[nodiscard]] std::expected<String, StarException> previewQuestFailed(String const& argumentsString);
  [[nodiscard]] std::expected<String, StarException> clearScannedObjects();
  [[nodiscard]] std::expected<String, StarException> playTime();
  [[nodiscard]] std::expected<String, StarException> deathCount();
  [[nodiscard]] std::expected<String, StarException> cinema(String const& argumentsString);
  [[nodiscard]] std::expected<String, StarException> suicide();
  [[nodiscard]] std::expected<String, StarException> naked();
  [[nodiscard]] std::expected<String, StarException> resetAchievements();
  [[nodiscard]] std::expected<String, StarException> statistic(String const& argumentsString);
  [[nodiscard]] std::expected<String, StarException> giveEssentialItem(String const& argumentsString);
  [[nodiscard]] std::expected<String, StarException> makeTechAvailable(String const& argumentsString);
  [[nodiscard]] std::expected<String, StarException> enableTech(String const& argumentsString);
  [[nodiscard]] std::expected<String, StarException> upgradeShip(String const& argumentsString);
  [[nodiscard]] std::expected<String, StarException> swap(String const& argumentsString);
  [[nodiscard]] std::expected<String, StarException> respawnInWorld(String const& argumentsString);
  [[nodiscard]] std::expected<String, StarException> render(String const& imagePath);

  using BuiltinCommand = std::reference_wrapper<std::expected<String, StarException>(ClientCommandProcessor&, String const&)>;
  template <client_builtin_command auto command>
  [[nodiscard]] static std::expected<String, StarException> invoke_builtin(ClientCommandProcessor& self, String const& arguments) {
    if constexpr (std::invocable<decltype(command), ClientCommandProcessor&, String const&>) {
      return (self.*command)(arguments);
    } else {
      return (self.*command)();
    }
  }
  [[nodiscard]] static std::expected<CaseInsensitiveStringMap<BuiltinCommand>, StarException> makeBuiltinCommands();
  static const std::expected<CaseInsensitiveStringMap<BuiltinCommand>, StarException> s_builtinCommands;
  GuiContext& m_context;


  std::shared_ptr<UniverseClient> m_universeClient;
  std::shared_ptr<Cinematic> m_cinematicOverlay;
  std::optional<std::reference_wrapper<MainInterfacePaneManager>> m_paneManager;
  StringMap<StringList> const m_macro_commands;
  PendingClientCommands m_pending;
  bool m_debugDisplayEnabled = false;
  bool m_debugHudEnabled = true;
  bool m_fixedCameraEnabled = false;
};

}

export module star.client_command_processor;

export namespace Star {
  using ::Star::Quest;
  using ::Star::PendingClientCommands;
  using ::Star::ClientCommandProcessor;
}
