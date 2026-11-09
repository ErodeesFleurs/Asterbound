module;
import std;
import star.string;
import star.list;
import star.map;
import star.format;
import star.hash;
import star.json;
import star.vector;
import star.input_event;
import star.thread;
import star.listener;


namespace Star {
class Root;


struct InputExceptionTag {
  static constexpr std::string_view name() { return "InputException"; }
};
using InputException = StarError<InputExceptionTag, StarException>;

using InputVariant = std::variant<Key, MouseButton, ControllerButton>;

template <>
struct hash<InputVariant> {
  std::size_t operator()(InputVariant const& v) const;
};

class Input {
public:

  [[nodiscard]] static std::expected<Json, StarException> inputEventToJson(InputEvent const& event);

  struct KeyBind {
    Key key = Key::Zero;
    KeyMod mods = KeyMod::NoMod;
    std::uint8_t priority = 0;

    inline bool operator<(KeyBind const& rhs) const {
      return priority < rhs.priority;
    }

    inline bool operator>(KeyBind const& rhs) const {
      return priority > rhs.priority;
    }
  };

  struct MouseBind {
    MouseButton button = MouseButton::Left;
    KeyMod mods = KeyMod::NoMod;
    std::uint8_t priority = 0;
  };

  struct ControllerBind {
    unsigned int controller = 0;
    ControllerButton button = ControllerButton::Invalid;
  };

  using Bind = std::variant<std::monostate, KeyBind, MouseBind, ControllerBind>;

  [[nodiscard]] static std::expected<Bind, StarException> bindFromJson(Json const& json);
  [[nodiscard]] static std::expected<Json, StarException> bindToJson(Bind const& bind);

  struct BindCategory;

  struct BindEntry {
    Input& owner;
    // The internal ID of this entry.
    String id;
    // The user-facing name of this entry.
    String name;
    // The category this entry belongs to.
    std::reference_wrapper<BindCategory const> category;
    // Associated string tags that become active when this bind is pressed.
    StringList tags;

    // The default binds.
    List<Bind> defaultBinds;
    // The user-configured binds.
    List<Bind> customBinds;
    std::size_t stateId;

    explicit BindEntry(Input& owner, BindCategory const& category);
    [[nodiscard]] static std::expected<BindEntry, StarException> create(Input& owner, String entryId, Json const& config, BindCategory const& parentCategory);
    [[nodiscard]] std::expected<void, StarException> updated();
  };

  struct BindRef {
    KeyMod mods;
    std::uint8_t priority = 0;  
    std::reference_wrapper<BindEntry> entry; // Invalidated on reload; mappings are rebuilt together.

    BindRef(BindEntry& bindEntry, KeyBind& keyBind);
    BindRef(BindEntry& bindEntry, MouseBind& mouseBind);
    BindRef(BindEntry& bindEntry);
  };

  struct BindCategory {
    Input& owner;
    String id;
    String name;
    Json config = JsonObject{};

    StableHashMap<String, BindEntry> entries;

    explicit BindCategory(Input& owner);
    [[nodiscard]] std::expected<void, StarException> mergeJson(String const& categoryId, Json const& categoryConfig);
  };

  struct InputState {
    unsigned presses = 0;
    unsigned releases = 0;
    bool pressed = false;
    bool held = false;
    bool released = false;

    // Calls the passed functions for each press and release.
    template <typename PressFunction, typename ReleaseFunction>
    void forEach(PressFunction&& pressed, ReleaseFunction&& released) {
      for (std::size_t i = 0; i != releases; ++i) {
        pressed();
        released();
      }
    }

    inline void reset() {
      presses = releases = 0;
      pressed = released = false;
    }

    inline void press() { pressed = ++presses; held = true; }
    inline void release() { released = ++releases; held = false; }
  };

  struct KeyInputState : InputState {
    KeyMod mods = KeyMod::NoMod;
  };

  struct MouseInputState : InputState {
    List<Vec2F> pressPositions;
    List<Vec2F> releasePositions;
  };

  using ControllerInputState = InputState;

  [[nodiscard]] static std::expected<std::shared_ptr<Input>, StarException> create(Root& root);
  ~Input();

  Input(Input const&) = delete;
  Input& operator=(Input const&) = delete;

  List<std::pair<InputEvent, bool>> const& inputEventsThisFrame() const;

  // Retire transients once per logic tick, never for a render-only iteration.
  void reset(bool clear = false);

  void update();

  // Handles an input event.
  [[nodiscard]] std::expected<bool, StarException> handleInput(InputEvent const& input, bool gameProcessed);

  void rebuildMappings();

  // Loads input categories and their binds from Assets.
  [[nodiscard]] std::expected<void, StarException> reload();

  void setTextInputActive(bool active);

  std::optional<unsigned> bindDown(String const& categoryId, String const& bindId);
  bool            bindHeld(String const& categoryId, String const& bindId);
  std::optional<unsigned> bindUp  (String const& categoryId, String const& bindId);

  std::optional<unsigned> keyDown(Key key, std::optional<KeyMod> keyMod);
  bool            keyHeld(Key key);
  std::optional<unsigned> keyUp  (Key key);

  std::optional<List<Vec2F>> mouseDown(MouseButton button);
  bool               mouseHeld(MouseButton button);
  std::optional<List<Vec2F>> mouseUp  (MouseButton button);

  Vec2F mousePosition() const;

  [[nodiscard]] std::expected<void, StarException> resetBinds(String const& categoryId, String const& bindId);
  [[nodiscard]] std::expected<void, StarException> setBinds(String const& categoryId, String const& bindId, Json const& binds);
  [[nodiscard]] std::expected<Json, StarException> getDefaultBinds(String const& categoryId, String const& bindId); 
  [[nodiscard]] std::expected<Json, StarException> getBinds(String const& categoryId, String const& bindId);

  unsigned getTag(String const& tagName) const;

  class ClipboardUnlock {
  public:
    ClipboardUnlock(Input& input);
    ClipboardUnlock(ClipboardUnlock const&) = delete;
    ClipboardUnlock(ClipboardUnlock&&);
    ~ClipboardUnlock();

  private:
    std::optional<std::reference_wrapper<Input>> m_input;
  };

  ClipboardUnlock unlockClipboard();
  bool clipboardAllowed() const;
private:
  explicit Input(Root& root);
  List<std::reference_wrapper<BindEntry>> filterBindEntries(List<BindRef> const& binds, KeyMod mods) const;

  std::optional<std::reference_wrapper<BindEntry>> maybeBindEntry(String const& categoryId, String const& bindId);
  [[nodiscard]] std::expected<std::reference_wrapper<BindEntry>, StarException> bindEntry(String const& categoryId, String const& bindId);

  std::optional<std::reference_wrapper<InputState>> maybeBindState(String const& categoryId, String const& bindId);

  InputState& addBindState(BindEntry const& bindEntry);

  Root& m_root;

  // Regenerated on reload.
  StableHashMap<String, BindCategory> m_bindCategories;
  // Contains references to bind entries in categories, so also regenerated on reload.
  HashMap<InputVariant, List<BindRef>> m_bindMappings;

  std::shared_ptr<Listener> m_rootReloadListener;
  ListenerSubscription m_rootReloadSubscription;

  // Per-frame input event storage for Lua.
  List<std::pair<InputEvent, bool>> m_inputEvents;

  // Per-frame input state maps.
  //Input states
  HashMap<Key, KeyInputState> m_keyStates;
  HashMap<MouseButton, MouseInputState> m_mouseStates;
  HashMap<ControllerButton, ControllerInputState> m_controllerStates;
  //Bind states
  struct BindInputState : InputState {
    std::reference_wrapper<BindEntry const> entry;
    explicit BindInputState(BindEntry const& entry) : entry(entry) {}
  };
  HashMap<std::size_t, BindInputState> m_bindStates;
  std::size_t m_nextBindEntryId = 0;
  StringMap<unsigned> m_activeTags;

  KeyMod m_pressedMods;
  bool m_textInputActive = false;
  Vec2F m_mousePosition;

  unsigned m_clipboardAllowed = 0;
};

}

export module star.input;

export namespace Star {
  template <>
  struct hash<InputVariant>;
}


export namespace Star {
  using ::Star::Input;
  using ::Star::InputExceptionTag;
  using ::Star::InputException;
  using ::Star::InputVariant;
}
