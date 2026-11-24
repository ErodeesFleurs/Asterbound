module;
import std;
import star.string;
import star.list;
import star.small_vector;
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

struct ControllerSource {
  ControllerId controller = 0;
  ControllerButton button = ControllerButton::Invalid;
  bool operator==(ControllerSource const&) const = default;
};

using InputVariant = std::variant<Key, MouseButton, ControllerSource>;

template <>
struct ValueHash<InputVariant> {
  std::size_t operator()(InputVariant const& v) const;
};

class Input {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  Input(ConstructionKey, Root& root);

  [[nodiscard]] static std::expected<Json, StarException> input_event_to_json(InputEvent const& event, std::optional<bool> processed = {});

  struct KeyBind {
    Key key = Key::Zero;
    KeyMod mods = KeyMod::NoMod;
    std::uint8_t priority = 0;
  };

  struct MouseBind {
    MouseButton button = MouseButton::Left;
    KeyMod mods = KeyMod::NoMod;
    std::uint8_t priority = 0;
  };

  using ControllerBind = ControllerSource;

  using Bind = std::variant<std::monostate, KeyBind, MouseBind, ControllerBind>;

  [[nodiscard]] static std::expected<Bind, StarException> bind_from_json(Json const& json);
  [[nodiscard]] static std::expected<Json, StarException> bind_to_json(Bind const& bind);

  struct BindEntry {
    StringList tags;
    List<Bind> default_binds;
    // Missing uses defaults; an engaged empty list explicitly unbinds the action.
    std::optional<List<Bind>> custom_binds;
    std::size_t state_id = 0;

    List<Bind> const& binds() const { return custom_binds ? *custom_binds : default_binds; }
  };

  struct BindRef {
    KeyMod mods;
    std::uint8_t priority;
    std::reference_wrapper<BindEntry const> entry;
  };

  struct BindMapping {
    List<BindRef> candidates;
    SmallVector<std::size_t, 1> held_entries;
    bool held = false;
  };

  using BindCategory = StableHashMap<String, BindEntry>;


  [[nodiscard]] static std::expected<std::shared_ptr<Input>, StarException> create(Root& root);
  ~Input();

  Input(Input const&) = delete;
  Input& operator=(Input const&) = delete;

  List<std::pair<InputEvent, bool>> const& input_events_this_frame() const;

  // Retire transients once per logic tick, never for a render-only iteration.
  void finish_tick();
  // Rebinding invalidates action attribution, not physical keyboard modifiers or pending event history.
  void clear_binding_state();
  void clear_input_state();

  // Handles an input event.
  void handle_input(InputEvent const& input, bool game_processed = false);

  // Loads input categories and their binds from Assets.
  [[nodiscard]] std::expected<void, StarException> reload();

  void set_text_input_active(bool active);

  std::optional<unsigned> bind_down(String const& category_id, String const& bind_id);
  bool bind_held(String const& category_id, String const& bind_id);
  std::optional<unsigned> bind_up(String const& category_id, String const& bind_id);

  std::optional<unsigned> key_down(Key key, std::optional<KeyMod> key_mod) const;
  bool key_held(Key key) const;
  std::optional<unsigned> key_up(Key key) const;

  // Borrowed until the next input or state transition. Empty means no such event this tick.
  std::span<Vec2F const> mouse_down(MouseButton button) const;
  bool mouse_held(MouseButton button) const;
  std::span<Vec2F const> mouse_up(MouseButton button) const;
  Vec2F mouse_position() const;

  [[nodiscard]] std::expected<void, StarException> reset_binds(String const& category_id, String const& bind_id);
  [[nodiscard]] std::expected<void, StarException> set_binds(String const& category_id, String const& bind_id, Json const& binds);
  [[nodiscard]] std::expected<Json, StarException> get_default_binds(String const& category_id, String const& bind_id);
  [[nodiscard]] std::expected<Json, StarException> get_binds(String const& category_id, String const& bind_id);

  unsigned get_tag(String const& tag_name) const;

  class ClipboardUnlock {
  public:
    ClipboardUnlock(Input& input);
    ClipboardUnlock(ClipboardUnlock const&) = delete;
    ClipboardUnlock(ClipboardUnlock&&) noexcept;
    ~ClipboardUnlock();

  private:
    std::optional<std::reference_wrapper<Input>> m_input;
  };

  ClipboardUnlock unlock_clipboard();
  bool clipboard_allowed() const;
private:
  void press_source(InputVariant source, KeyMod mods);
  void release_source(InputVariant source);

  std::optional<std::reference_wrapper<BindEntry>> maybe_bind_entry(String const& category_id, String const& bind_id);
  [[nodiscard]] std::expected<std::reference_wrapper<BindEntry>, StarException> bind_entry(String const& category_id, String const& bind_id);
  [[nodiscard]] std::expected<void, StarException> write_binds(String const& category_id, String const& bind_id,
      BindEntry& entry, std::optional<List<Bind>> replacement);

  struct BindInputState {
    std::reference_wrapper<BindEntry const> entry;
    unsigned sources = 0;
    unsigned presses = 0;
    unsigned releases = 0;
    explicit BindInputState(BindEntry const& entry) : entry(entry) {}
  };
  std::optional<std::reference_wrapper<BindInputState>> maybe_bind_state(String const& category_id, String const& bind_id);

  Root& m_root;

  using BindCategories = StringMap<BindCategory>;
  using BindMappings = HashMap<InputVariant, BindMapping>;
  struct Replacement {
    std::size_t state_id;
    List<Bind> const& binds;
  };
  static BindMappings rebuild_mappings(BindCategories const& categories, std::optional<Replacement> replacement);

  // Publish these together: mappings borrow the stable entry nodes in the catalog.
  BindCategories m_bind_categories;
  BindMappings m_bind_mappings;

  std::shared_ptr<Listener> m_root_reload_listener;
  ListenerSubscription m_root_reload_subscription;

  // Per-frame input event storage for Lua.
  List<std::pair<InputEvent, bool>> m_input_events;

  struct ModifierPresses {
    KeyMod mods;
    unsigned count;
  };
  struct KeyInputState {
    SmallVector<ModifierPresses, 1> presses;
    unsigned releases = 0;
    bool held = false;
  };
  struct MouseInputState {
    SmallVector<Vec2F, 1> press_positions;
    SmallVector<Vec2F, 1> release_positions;
    bool held = false;
  };
  HashMap<Key, KeyInputState> m_key_states;
  HashMap<MouseButton, MouseInputState> m_mouse_states;
  HashMap<std::size_t, BindInputState> m_bind_states;
  StringMap<unsigned> m_active_tags;

  KeyMod m_pressed_mods = KeyMod::NoMod;
  bool m_text_input_active = false;
  Vec2F m_mouse_position;

  unsigned m_clipboard_allowed = 0;
};

}

export module star.input;

export namespace Star {
  template <>
  struct ValueHash<InputVariant>;
}


export namespace Star {
  using ::Star::ControllerSource;
  using ::Star::Input;
  using ::Star::InputExceptionTag;
  using ::Star::InputException;
  using ::Star::InputVariant;
}
