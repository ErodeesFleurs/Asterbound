module;
import std;
import star.format;
import star.string;
import star.bi_map;
import star.vector;

namespace Star {

enum class Key : std::uint16_t {
  Backspace,
  Tab,
  Clear,
  Return,
  Escape,
  Space,
  Exclaim,
  QuotedBL,
  Hash,
  Dollar,
  Ampersand,
  Quote,
  LeftParen,
  RightParen,
  Asterisk,
  Plus,
  Comma,
  Minus,
  Period,
  Slash,
  Zero,
  One,
  Two,
  Three,
  Four,
  Five,
  Six,
  Seven,
  Eight,
  Nine,
  Colon,
  Semicolon,
  Less,
  Equals,
  Greater,
  Question,
  At,
  LeftBracket,
  Backslash,
  RightBracket,
  Caret,
  Underscore,
  Backquote,
  A,
  B,
  C,
  D,
  E,
  F,
  G,
  H,
  I,
  J,
  K,
  L,
  M,
  N,
  O,
  P,
  Q,
  R,
  S,
  T,
  U,
  V,
  W,
  X,
  Y,
  Z,
  Delete,
  Keypad0,
  Keypad1,
  Keypad2,
  Keypad3,
  Keypad4,
  Keypad5,
  Keypad6,
  Keypad7,
  Keypad8,
  Keypad9,
  KeypadPeriod,
  KeypadDivide,
  KeypadMultiply,
  KeypadMinus,
  KeypadPlus,
  KeypadEnter,
  KeypadEquals,
  Up,
  Down,
  Right,
  Left,
  Insert,
  Home,
  End,
  PageUp,
  PageDown,
  F1,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12,
  F13,
  F14,
  F15,
  NumLock,
  CapsLock,
  ScrollLock,
  RShift,
  LShift,
  RCtrl,
  LCtrl,
  RAlt,
  LAlt,
  RGui,
  LGui,
  AltGr,
  Compose,
  Help,
  PrintScreen,
  SysReq,
  Pause,
  Menu,
  Power,
  // can't have this where I want because canvases
  // pass keycodes to Lua as a numeric code >:[
  F16,
  F17,
  F18,
  F19,
  F20,
  F21,
  F22,
  F23,
  F24
};
inline constexpr auto KeyNames = sortedEnumNames(std::to_array<std::pair<Key, std::string_view>>({
  {Key::Backspace, "Backspace"},
  {Key::Tab, "Tab"},
  {Key::Clear, "Clear"},
  {Key::Return, "Return"},
  {Key::Escape, "Esc"},
  {Key::Space, "Space"},
  {Key::Exclaim, "!"},
  {Key::QuotedBL, "\""},
  {Key::Hash, "#"},
  {Key::Dollar, "$"},
  {Key::Ampersand, "&"},
  {Key::Quote, "\'"},
  {Key::LeftParen, "("},
  {Key::RightParen, ")"},
  {Key::Asterisk, "*"},
  {Key::Plus, "+"},
  {Key::Comma, ","},
  {Key::Minus, "-"},
  {Key::Period, "."},
  {Key::Slash, "/"},
  {Key::Zero, "0"},
  {Key::One, "1"},
  {Key::Two, "2"},
  {Key::Three, "3"},
  {Key::Four, "4"},
  {Key::Five, "5"},
  {Key::Six, "6"},
  {Key::Seven, "7"},
  {Key::Eight, "8"},
  {Key::Nine, "9"},
  {Key::Colon, ":"},
  {Key::Semicolon, ";"},
  {Key::Less, "<"},
  {Key::Equals, "="},
  {Key::Greater, ">"},
  {Key::Question, "?"},
  {Key::At, "@"},
  {Key::LeftBracket, "["},
  {Key::Backslash, "\\"},
  {Key::RightBracket, "]"},
  {Key::Caret, "^"},
  {Key::Underscore, "_"},
  {Key::Backquote, "`"},
  {Key::A, "A"},
  {Key::B, "B"},
  {Key::C, "C"},
  {Key::D, "D"},
  {Key::E, "E"},
  {Key::F, "F"},
  {Key::G, "G"},
  {Key::H, "H"},
  {Key::I, "I"},
  {Key::J, "J"},
  {Key::K, "K"},
  {Key::L, "L"},
  {Key::M, "M"},
  {Key::N, "N"},
  {Key::O, "O"},
  {Key::P, "P"},
  {Key::Q, "Q"},
  {Key::R, "R"},
  {Key::S, "S"},
  {Key::T, "T"},
  {Key::U, "U"},
  {Key::V, "V"},
  {Key::W, "W"},
  {Key::X, "X"},
  {Key::Y, "Y"},
  {Key::Z, "Z"},
  {Key::Delete, "Del"},
  {Key::Keypad0, "Keypad 0"},
  {Key::Keypad1, "Keypad 1"},
  {Key::Keypad2, "Keypad 2"},
  {Key::Keypad3, "Keypad 3"},
  {Key::Keypad4, "Keypad 4"},
  {Key::Keypad5, "Keypad 5"},
  {Key::Keypad6, "Keypad 6"},
  {Key::Keypad7, "Keypad 7"},
  {Key::Keypad8, "Keypad 8"},
  {Key::Keypad9, "Keypad 9"},
  {Key::KeypadPeriod, "Keypad ."},
  {Key::KeypadDivide, "Keypad /"},
  {Key::KeypadMultiply, "Keypad *"},
  {Key::KeypadMinus, "Keypad -"},
  {Key::KeypadPlus, "Keypad +"},
  {Key::KeypadEnter, "Keypad Enter"},
  {Key::KeypadEquals, "Keypad ="},
  {Key::Up, "Up"},
  {Key::Down, "Down"},
  {Key::Right, "Right"},
  {Key::Left, "Left"},
  {Key::Insert, "Ins"},
  {Key::Home, "Home"},
  {Key::End, "End"},
  {Key::PageUp, "PageUp"},
  {Key::PageDown, "PageDown"},
  {Key::F1, "F1"},
  {Key::F2, "F2"},
  {Key::F3, "F3"},
  {Key::F4, "F4"},
  {Key::F5, "F5"},
  {Key::F6, "F6"},
  {Key::F7, "F7"},
  {Key::F8, "F8"},
  {Key::F9, "F9"},
  {Key::F10, "F10"},
  {Key::F11, "F11"},
  {Key::F12, "F12"},
  {Key::F13, "F13"},
  {Key::F14, "F14"},
  {Key::F15, "F15"},
  {Key::F16, "F16"},
  {Key::F17, "F17"},
  {Key::F18, "F18"},
  {Key::F19, "F19"},
  {Key::F20, "F20"},
  {Key::F21, "F21"},
  {Key::F22, "F22"},
  {Key::F23, "F23"},
  {Key::F24, "F24"},
  {Key::NumLock, "NumLock"},
  {Key::CapsLock, "CapsLock"},
  {Key::ScrollLock, "ScrollLock"},
  {Key::RShift, "RShift"},
  {Key::LShift, "LShift"},
  {Key::RCtrl, "RCtrl"},
  {Key::LCtrl, "LCtrl"},
  {Key::RAlt, "RAlt"},
  {Key::LAlt, "LAlt"},
  {Key::RGui, "RGui"},
  {Key::LGui, "LGui"},
  {Key::AltGr, "AltGr"},
  {Key::Compose, "Compose"},
  {Key::Help, "Help"},
  {Key::PrintScreen, "PrintScreen"},
  {Key::SysReq, "SysReq"},
  {Key::Pause, "Pause"},
  {Key::Menu, "Menu"},
  {Key::Power, "Power"}
}));
static_assert(validEnumNames<KeyNames>());

enum class KeyMod : std::uint16_t {
  NoMod = 0x0000,
  LShift = 0x0001,
  RShift = 0x0002,
  LCtrl = 0x0040,
  RCtrl = 0x0080,
  LAlt = 0x0100,
  RAlt = 0x0200,
  LGui = 0x0400,
  RGui = 0x0800,
  Num = 0x1000,
  Caps = 0x2000,
  AltGr = 0x4000,
  Scroll = 0x8000
};
inline constexpr auto KeyModNames = sortedEnumNames(std::to_array<std::pair<KeyMod, std::string_view>>({
  {KeyMod::NoMod, "NoMod"},
  {KeyMod::LShift, "LShift"},
  {KeyMod::RShift, "RShift"},
  {KeyMod::LCtrl, "LCtrl"},
  {KeyMod::RCtrl, "RCtrl"},
  {KeyMod::LAlt, "LAlt"},
  {KeyMod::RAlt, "RAlt"},
  {KeyMod::LGui, "LMeta"},
  {KeyMod::RGui, "RMeta"},
  {KeyMod::Num, "Num"},
  {KeyMod::Caps, "Caps"},
  {KeyMod::AltGr, "AltGr"}
}));
static_assert(validEnumNames<KeyModNames>());

KeyMod operator|(KeyMod a, KeyMod b);
KeyMod operator&(KeyMod a, KeyMod b);
KeyMod operator~(KeyMod a);
KeyMod& operator|=(KeyMod& a, KeyMod b);
KeyMod& operator&=(KeyMod& a, KeyMod b);

enum class MouseButton : std::uint8_t {
  Left,
  Middle,
  Right,
  FourthButton,
  FifthButton
};
inline constexpr auto MouseButtonNames = sortedEnumNames(std::to_array<std::pair<MouseButton, std::string_view>>({
  {MouseButton::Left, "MouseLeft"},
  {MouseButton::Middle, "MouseMiddle"},
  {MouseButton::Right, "MouseRight"},
  {MouseButton::FourthButton, "MouseFourth"},
  {MouseButton::FifthButton, "MouseFifth"}
}));
static_assert(validEnumNames<MouseButtonNames>());

enum class MouseWheel : std::uint8_t {
  Up,
  Down
};
inline constexpr auto MouseWheelNames = sortedEnumNames(std::to_array<std::pair<MouseWheel, std::string_view>>({
  {MouseWheel::Up, "MouseWheelUp"},
  {MouseWheel::Down, "MouseWheelDown"}
}));
static_assert(validEnumNames<MouseWheelNames>());

using ControllerId = std::uint32_t;

enum class ControllerAxis : std::uint8_t {
  LeftX,
  LeftY,
  RightX,
  RightY,
  TriggerLeft,
  TriggerRight,
  Invalid = 255
};
inline constexpr auto ControllerAxisNames = sortedEnumNames(std::to_array<std::pair<ControllerAxis, std::string_view>>({
  {ControllerAxis::LeftX, "LeftX"},
  {ControllerAxis::LeftY, "LeftY"},
  {ControllerAxis::RightX, "RightX"},
  {ControllerAxis::RightY, "RightY"},
  {ControllerAxis::TriggerLeft, "TriggerLeft"},
  {ControllerAxis::TriggerRight, "TriggerRight"},
}));
static_assert(validEnumNames<ControllerAxisNames>());

enum class ControllerButton : std::uint8_t {
  A,
  B,
  X,
  Y,
  Back,
  Guide,
  Start,
  LeftStick,
  RightStick,
  LeftShoulder,
  RightShoulder,
  DPadUp,
  DPadDown,
  DPadLeft,
  DPadRight,
  Misc1,
  Paddle1,
  Paddle2,
  Paddle3,
  Paddle4,
  Touchpad,
  Invalid = 255
};
inline constexpr auto ControllerButtonNames = sortedEnumNames(std::to_array<std::pair<ControllerButton, std::string_view>>({
  {ControllerButton::A, "A"},
  {ControllerButton::B, "B"},
  {ControllerButton::X, "X"},
  {ControllerButton::Y, "Y"},
  {ControllerButton::Back, "Back"},
  {ControllerButton::Guide, "Guide"},
  {ControllerButton::Start, "Start"},
  {ControllerButton::LeftStick, "LeftStick"},
  {ControllerButton::RightStick, "RightStick"},
  {ControllerButton::LeftShoulder, "LeftShoulder"},
  {ControllerButton::RightShoulder, "RightShoulder"},
  {ControllerButton::DPadUp, "DPadUp"},
  {ControllerButton::DPadDown, "DPadDown"},
  {ControllerButton::DPadLeft, "DPadLeft"},
  {ControllerButton::DPadRight, "DPadRight"},
  {ControllerButton::Misc1, "Misc1"},
  {ControllerButton::Paddle1, "Paddle1"},
  {ControllerButton::Paddle2, "Paddle2"},
  {ControllerButton::Paddle3, "Paddle3"},
  {ControllerButton::Paddle4, "Paddle4"},
  {ControllerButton::Touchpad, "Touchpad"},
  {ControllerButton::Invalid, "Invalid"}
}));
static_assert(validEnumNames<ControllerButtonNames>());

struct KeyDownEvent {
  Key key;
  KeyMod mods;
};

struct KeyUpEvent {
  Key key;
};

struct TextInputEvent {
  String text;
};

struct MouseMoveEvent {
  Vec2F mouseMove;
  Vec2F mousePosition;
};

struct MouseButtonDownEvent {
  MouseButton mouseButton;
  Vec2F mousePosition;
};

struct MouseButtonUpEvent {
  MouseButton mouseButton;
  Vec2F mousePosition;
};

struct MouseWheelEvent {
  MouseWheel mouseWheel;
  Vec2F mousePosition;
};

struct ControllerAxisEvent {
  ControllerId controller;
  ControllerAxis controllerAxis;
  float controllerAxisValue;
};

struct ControllerButtonDownEvent {
  ControllerId controller;
  ControllerButton controllerButton;
};

struct ControllerButtonUpEvent {
  ControllerId controller;
  ControllerButton controllerButton;
};

using InputEvent =
  std::variant<KeyDownEvent, KeyUpEvent, TextInputEvent, MouseMoveEvent, MouseButtonDownEvent, MouseButtonUpEvent,
               MouseWheelEvent, ControllerAxisEvent, ControllerButtonDownEvent, ControllerButtonUpEvent>;

inline KeyMod operator|(KeyMod a, KeyMod b) {
  return static_cast<KeyMod>(std::to_underlying(a) | std::to_underlying(b));
}

inline KeyMod operator&(KeyMod a, KeyMod b) {
  return static_cast<KeyMod>(std::to_underlying(a) & std::to_underlying(b));
}

inline KeyMod operator~(KeyMod a) {
  return static_cast<KeyMod>(~std::to_underlying(a));
}

inline KeyMod& operator|=(KeyMod& a, KeyMod b) {
  a = a | b;
  return a;
}

inline KeyMod& operator&=(KeyMod& a, KeyMod b) {
  a = a & b;
  return a;
}

}

export module star.input_event;

export namespace Star {
  using ::Star::Key;
  using ::Star::KeyNames;
  using ::Star::KeyMod;
  using ::Star::KeyModNames;
  using ::Star::operator|;
  using ::Star::operator&;
  using ::Star::operator~;
  using ::Star::operator|=;
  using ::Star::operator&=;
  using ::Star::MouseButton;
  using ::Star::MouseButtonNames;
  using ::Star::MouseWheel;
  using ::Star::MouseWheelNames;
  using ::Star::ControllerId;
  using ::Star::ControllerAxis;
  using ::Star::ControllerAxisNames;
  using ::Star::ControllerButton;
  using ::Star::ControllerButtonNames;
  using ::Star::KeyDownEvent;
  using ::Star::KeyUpEvent;
  using ::Star::TextInputEvent;
  using ::Star::MouseMoveEvent;
  using ::Star::MouseButtonDownEvent;
  using ::Star::MouseButtonUpEvent;
  using ::Star::MouseWheelEvent;
  using ::Star::ControllerAxisEvent;
  using ::Star::ControllerButtonDownEvent;
  using ::Star::ControllerButtonUpEvent;
  using ::Star::InputEvent;
}
