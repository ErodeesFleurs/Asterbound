module;
import std;
import star.format;
import star.json;

import star.game_types;
import star.casting;
import star.input_event;
import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.image;
import star.application_controller;
import star.color;
import star.font;
import star.bi_map;
import star.ref_ptr;
import star.renderer;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.set;
import star.audio;
import star.map;
import star.ordered_map;
import star.font_texture_group;
import star.anchor_types;
import star.text_painter;
import star.drawable;
import star.asset_texture_group;
import star.drawable_painter;
import star.gui_types;
import star.key_bindings;
import star.mixer;
import star.gui_context;
import star.widget;
import star.pane;
import star.game_timers;
import star.pane_manager;

import star.output_proxy;
namespace Star {

// This class inherits PaneManager to allow for registered panes that are kept
// internally by the class even when dismissed.  They can be displayed,
// dismissed, and toggled between the two without being lost.
template <typename KeyT>
class RegisteredPaneManager : public PaneManager {
public:
  using Key = KeyT;

  explicit RegisteredPaneManager(GuiContext& context) : PaneManager(context) {}

  [[nodiscard]] std::expected<void, StarException> initialize() {
    return initializePaneManager();
  }

  [[nodiscard]] static std::expected<std::shared_ptr<RegisteredPaneManager>, StarException> create(GuiContext& context) {
    auto manager = std::make_shared<RegisteredPaneManager>(context);
    auto initialized = manager->initialize();
    if (!initialized)
      return std::unexpected(std::move(initialized.error()));
    return manager;
  }

  [[nodiscard]] std::expected<void, StarException> registerPane(KeyT paneId, PaneLayer paneLayer, std::shared_ptr<Pane> pane, DismissCallback onDismiss = {});
  [[nodiscard]] std::expected<std::shared_ptr<Pane>, StarException> deregisterPane(KeyT const& paneId);
  [[nodiscard]] std::expected<void, StarException> deregisterAllPanes();

  template <typename T = Pane>
  [[nodiscard]] std::expected<std::shared_ptr<T>, StarException> registeredPane(KeyT const& paneId) const;
  template <typename T = Pane>
  [[nodiscard]] std::expected<std::shared_ptr<T>, StarException> maybeRegisteredPane(KeyT const& paneId) const;

  // Displays a registred pane if it is not already displayed.  Returns true
  // if it is newly displayed.
  [[nodiscard]] std::expected<bool, StarException> displayRegisteredPane(KeyT const& paneId);
  [[nodiscard]] std::expected<bool, StarException> registeredPaneIsDisplayed(KeyT const& paneId) const;

  // Dismisses a registred pane if it is displayed.  Returns true if it
  // has been dismissed.
  [[nodiscard]] std::expected<bool, StarException> dismissRegisteredPane(KeyT const& paneId);

  // Returns whether the pane is now displayed.
  [[nodiscard]] std::expected<bool, StarException> toggleRegisteredPane(KeyT const& paneId);

private:
  struct PaneInfo {
    PaneLayer layer;
    std::shared_ptr<Pane> pane;
    DismissCallback dismissCallback;
  };

  [[nodiscard]] std::expected<std::reference_wrapper<PaneInfo const>, StarException> getRegisteredPaneInfo(KeyT const& paneId) const;

  // Map of registered panes by name.
  HashMap<KeyT, PaneInfo> m_registeredPanes;
};

template <typename KeyT>
template <typename T>
[[nodiscard]] std::expected<std::shared_ptr<T>, StarException> RegisteredPaneManager<KeyT>::registeredPane(KeyT const& paneId) const {
  if (auto v = m_registeredPanes.find_ref(paneId))
    return convert<T>(v->get().pane);
  return std::unexpected(GuiException(strf("No pane named '{}' found in RegisteredPaneManager", outputAny(paneId))));
}

template <typename KeyT>
template <typename T>
[[nodiscard]] std::expected<std::shared_ptr<T>, StarException> RegisteredPaneManager<KeyT>::maybeRegisteredPane(KeyT const& paneId) const {
  if (auto v = m_registeredPanes.find_ref(paneId))
    return convert<T>(v->get().pane);
  return std::shared_ptr<T>{};
}

template <typename KeyT>
[[nodiscard]] std::expected<void, StarException> RegisteredPaneManager<KeyT>::registerPane(
    KeyT paneId, PaneLayer paneLayer, std::shared_ptr<Pane> pane, DismissCallback onDismiss) {
  if (!m_registeredPanes.insert(std::move(paneId), {std::move(paneLayer), std::move(pane), std::move(onDismiss)}).second)
    return std::unexpected(GuiException(
        strf("Registered pane with name '{}' registered a second time in RegisteredPaneManager::registerPane",
            outputAny(paneId))));
  return {};
}

template <typename KeyT>
[[nodiscard]] std::expected<std::shared_ptr<Pane>, StarException> RegisteredPaneManager<KeyT>::deregisterPane(KeyT const& paneId) {
  if (auto v = m_registeredPanes.try_extract_value(paneId)) {
    if (isDisplayed(v->pane)) {
      auto dismissed = dismissPane(v->pane);
      if (!dismissed)
        return std::unexpected(std::move(dismissed.error()));
    }
    return v->pane;
  }
  return std::unexpected(GuiException(strf("No pane named '{}' found in RegisteredPaneManager::deregisterPane", outputAny(paneId))));
}

template <typename KeyT>
[[nodiscard]] std::expected<void, StarException> RegisteredPaneManager<KeyT>::deregisterAllPanes() {
  for (auto const& k : m_registeredPanes.keys()) {
    auto deregistered = deregisterPane(k);
    if (!deregistered)
      return std::unexpected(std::move(deregistered.error()));
  }
  return {};
}

template <typename KeyT>
[[nodiscard]] std::expected<bool, StarException> RegisteredPaneManager<KeyT>::displayRegisteredPane(KeyT const& paneId) {
  auto paneInfo = getRegisteredPaneInfo(paneId);
  if (!paneInfo)
    return std::unexpected(std::move(paneInfo.error()));
  if (!isDisplayed(paneInfo->get().pane)) {
    auto displayed = displayPane(paneInfo->get().layer, paneInfo->get().pane, paneInfo->get().dismissCallback);
    if (!displayed)
      return std::unexpected(std::move(displayed.error()));
    return true;
  }
  return false;
}

template <typename KeyT>
[[nodiscard]] std::expected<bool, StarException> RegisteredPaneManager<KeyT>::registeredPaneIsDisplayed(KeyT const& paneId) const {
  auto paneInfo = getRegisteredPaneInfo(paneId);
  if (!paneInfo)
    return std::unexpected(std::move(paneInfo.error()));
  return isDisplayed(paneInfo->get().pane);
}

template <typename KeyT>
[[nodiscard]] std::expected<bool, StarException> RegisteredPaneManager<KeyT>::dismissRegisteredPane(KeyT const& paneId) {
  auto paneInfo = getRegisteredPaneInfo(paneId);
  if (!paneInfo)
    return std::unexpected(std::move(paneInfo.error()));
  if (isDisplayed(paneInfo->get().pane)) {
    auto dismissed = dismissPane(paneInfo->get().pane);
    if (!dismissed)
      return std::unexpected(std::move(dismissed.error()));
    return true;
  }
  return false;
}

template <typename KeyT>
[[nodiscard]] std::expected<bool, StarException> RegisteredPaneManager<KeyT>::toggleRegisteredPane(KeyT const& paneId) {
  auto displayed = registeredPaneIsDisplayed(paneId);
  if (!displayed)
    return std::unexpected(std::move(displayed.error()));
  if (*displayed) {
    auto dismissed = dismissRegisteredPane(paneId);
    if (!dismissed)
      return std::unexpected(std::move(dismissed.error()));
    return false;
  }
  auto shown = displayRegisteredPane(paneId);
  if (!shown)
    return std::unexpected(std::move(shown.error()));
  return true;
}

template <typename KeyT>
[[nodiscard]] std::expected<std::reference_wrapper<typename RegisteredPaneManager<KeyT>::PaneInfo const>, StarException> RegisteredPaneManager<KeyT>::getRegisteredPaneInfo(
    KeyT const& paneId) const {
  if (auto p = m_registeredPanes.find_ref(paneId))
    return *p;
  return std::unexpected(GuiException(strf("No registered pane with name '{}' found in  RegisteredPaneManager", outputAny(paneId))));
}
}

export module star.registered_pane_manager;

export namespace Star {
  using ::Star::RegisteredPaneManager;
}
