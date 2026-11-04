#include "GL/glew.h"
#include "SDL3/SDL.h"
#ifdef STAR_ENABLE_STEAM_INTEGRATION
#include "steam/steam_api.h"
#endif
#ifdef STAR_ENABLE_DISCORD_INTEGRATION
#include "discord/discord.h"
#endif
#ifdef STAR_SYSTEM_WINDOWS
#include <dwmapi.h>
#include <objidl.h>
#include <shlobj.h>
#endif
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
import std;
import star.optional;
import star.format;
import star.output_proxy;
import star.json;
import star.string;
import star.map;
import star.file;
import star.io_device;
import star.input_event;
import star.application;
import star.vector;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.image;
import star.rect;
import star.application_controller;
import star.bi_map;
import star.ref_ptr;
import star.renderer;
import star.main_application;
import star.native_ui_context;
import star.logging;
import star.casting;
import star.texture_atlas;

import star.renderer_opengl;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.list;
import star.image_processing;



import star.platform_services_pc;



import star.signal_handler;
import star.tick_rate_monitor;

import star.byte_array;
import star.math_common;
import star.algorithm;
namespace Star {

#ifdef STAR_SYSTEM_WINDOWS
static bool copyDibToClipboard(std::span<std::uint8_t const> buf, unsigned int width, unsigned int height) {
  std::size_t size = (std::size_t)(width * height * 4u);

  BITMAPV5HEADER hV5{};
  hV5.bV5Size = sizeof(hV5);
  hV5.bV5SizeImage = size;
  hV5.bV5Width = width;
  hV5.bV5Height = height;
  hV5.bV5Planes = 1;
  hV5.bV5BitCount = 32;
  hV5.bV5Compression = BI_RGB;
  hV5.bV5RedMask   = 0x00FF0000;
  hV5.bV5GreenMask = 0x0000FF00;
  hV5.bV5BlueMask  = 0x000000FF;
  hV5.bV5AlphaMask = 0xFF000000;
  hV5.bV5CSType = LCS_WINDOWS_COLOR_SPACE;
  hV5.bV5Intent = LCS_GM_GRAPHICS;
  if (std::unique_ptr<std::remove_pointer_t<HGLOBAL>, decltype(&GlobalFree)> handle(GlobalAlloc(GMEM_MOVEABLE, sizeof(hV5) + size), GlobalFree); handle) {
    if (auto dst = static_cast<char*>(GlobalLock(handle.get()))) {
      std::memcpy(dst, &hV5, sizeof(hV5));
      unsigned int sizeI = width * height;
      auto data = reinterpret_cast<unsigned int const*>(buf.data());
      unsigned int* rgba = (unsigned int*)(dst + sizeof(hV5));
      for (unsigned int x = 0; x != sizeI; ++x) {
        unsigned int c = data[x];
        unsigned int v = c & 0xFF00FF00; // a and g
        v |= c << 16 & 0x00FF0000;       // r
        v |= c >> 16 & 0x000000FF;       // b
        rgba[x] = v;
      }
      GlobalUnlock(handle.get());
    } else {
      return false;
    }

    if (!SetClipboardData(CF_DIBV5, handle.get()))
      return false;
    handle.release();
    return true;
  }

  return false;
}

static bool copyPngToClipboard(std::span<char const> buf) {
  if (std::unique_ptr<std::remove_pointer_t<HGLOBAL>, decltype(&GlobalFree)> handle(GlobalAlloc(GMEM_MOVEABLE, buf.size()), GlobalFree); handle) {
    if (auto dst = static_cast<char*>(GlobalLock(handle.get()))) {
      std::memcpy(dst, buf.data(), buf.size());
      GlobalUnlock(handle.get());
    } else {
      return false;
    }
    if (!SetClipboardData(RegisterClipboardFormatA("PNG"), handle.get()))
      return false;
    handle.release();
    return true;
  }
  return false;
}

[[nodiscard]] static std::expected<bool, StarException> duringClipboard(SDL_Window* window, std::function<std::expected<void, StarException>()> task) {
  auto props = SDL_GetWindowProperties(window);
  auto handle = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
  if (!handle || !OpenClipboard(handle))
    return false;
  EmptyClipboard();
  auto result = task();
  CloseClipboard();
  if (!result)
    return std::unexpected(std::move(result.error()));
  return true;
}

[[nodiscard]] static std::expected<bool, StarException> copyFileToClipboard(String const& path) {
  DROPFILES drop{};
  drop.pFiles = sizeof(DROPFILES);
  drop.pt = {0, 0};
  drop.fNC = false;
  drop.fWide = true;
  auto converted = path.wstring();
  if (!converted)
    return std::unexpected(std::move(converted.error()));
  auto& wide = *converted;
  wide.push_back(0);
  wide.push_back(0);
  std::size_t wideSize = wide.size() * sizeof(std::wstring::value_type);
  if (std::unique_ptr<std::remove_pointer_t<HGLOBAL>, decltype(&GlobalFree)> handle(GlobalAlloc(GMEM_MOVEABLE, sizeof(DROPFILES) + wideSize), GlobalFree); handle) {
    if (auto dst = static_cast<char*>(GlobalLock(handle.get()))) {
      std::memcpy(dst, &drop, sizeof(drop));
      std::memcpy(dst + sizeof(DROPFILES), wide.data(), wideSize);
      GlobalUnlock(handle.get());
    } else {
      return false;
    }
    if (!SetClipboardData(CF_HDROP, handle.get()))
      return false;
    handle.release();
    return true;
  }
  return false;
}
#endif

std::optional<Key> keyFromSdlKeyCode(SDL_Keycode sym) {
  static HashMap<int, Key> KeyCodeMap{
    {SDLK_BACKSPACE, Key::Backspace},
    {SDLK_TAB, Key::Tab},
    {SDLK_CLEAR, Key::Clear},
    {SDLK_RETURN, Key::Return},
    {SDLK_PAUSE, Key::Pause},
    {SDLK_ESCAPE, Key::Escape},
    {SDLK_SPACE, Key::Space},
    {SDLK_EXCLAIM, Key::Exclaim},
    {SDLK_DBLAPOSTROPHE, Key::QuotedBL},
    {SDLK_HASH, Key::Hash},
    {SDLK_DOLLAR, Key::Dollar},
    {SDLK_AMPERSAND, Key::Ampersand},
    {SDLK_APOSTROPHE, Key::Quote},
    {SDLK_LEFTPAREN, Key::LeftParen},
    {SDLK_RIGHTPAREN, Key::RightParen},
    {SDLK_ASTERISK, Key::Asterisk},
    {SDLK_PLUS, Key::Plus},
    {SDLK_COMMA, Key::Comma},
    {SDLK_MINUS, Key::Minus},
    {SDLK_PERIOD, Key::Period},
    {SDLK_SLASH, Key::Slash},
    {SDLK_0, Key::Zero},
    {SDLK_1, Key::One},
    {SDLK_2, Key::Two},
    {SDLK_3, Key::Three},
    {SDLK_4, Key::Four},
    {SDLK_5, Key::Five},
    {SDLK_6, Key::Six},
    {SDLK_7, Key::Seven},
    {SDLK_8, Key::Eight},
    {SDLK_9, Key::Nine},
    {SDLK_COLON, Key::Colon},
    {SDLK_SEMICOLON, Key::Semicolon},
    {SDLK_LESS, Key::Less},
    {SDLK_EQUALS, Key::Equals},
    {SDLK_GREATER, Key::Greater},
    {SDLK_QUESTION, Key::Question},
    {SDLK_AT, Key::At},
    {SDLK_LEFTBRACKET, Key::LeftBracket},
    {SDLK_BACKSLASH, Key::Backslash},
    {SDLK_RIGHTBRACKET, Key::RightBracket},
    {SDLK_CARET, Key::Caret},
    {SDLK_UNDERSCORE, Key::Underscore},
    {SDLK_GRAVE, Key::Backquote},
    {SDLK_A, Key::A},
    {SDLK_B, Key::B},
    {SDLK_C, Key::C},
    {SDLK_D, Key::D},
    {SDLK_E, Key::E},
    {SDLK_F, Key::F},
    {SDLK_G, Key::G},
    {SDLK_H, Key::H},
    {SDLK_I, Key::I},
    {SDLK_J, Key::J},
    {SDLK_K, Key::K},
    {SDLK_L, Key::L},
    {SDLK_M, Key::M},
    {SDLK_N, Key::N},
    {SDLK_O, Key::O},
    {SDLK_P, Key::P},
    {SDLK_Q, Key::Q},
    {SDLK_R, Key::R},
    {SDLK_S, Key::S},
    {SDLK_T, Key::T},
    {SDLK_U, Key::U},
    {SDLK_V, Key::V},
    {SDLK_W, Key::W},
    {SDLK_X, Key::X},
    {SDLK_Y, Key::Y},
    {SDLK_Z, Key::Z},
    {SDLK_DELETE, Key::Delete},
    {SDLK_KP_0, Key::Keypad0},
    {SDLK_KP_1, Key::Keypad1},
    {SDLK_KP_2, Key::Keypad2},
    {SDLK_KP_3, Key::Keypad3},
    {SDLK_KP_4, Key::Keypad4},
    {SDLK_KP_5, Key::Keypad5},
    {SDLK_KP_6, Key::Keypad6},
    {SDLK_KP_7, Key::Keypad7},
    {SDLK_KP_8, Key::Keypad8},
    {SDLK_KP_9, Key::Keypad9},
    {SDLK_KP_PERIOD, Key::KeypadPeriod},
    {SDLK_KP_DIVIDE, Key::KeypadDivide},
    {SDLK_KP_MULTIPLY, Key::KeypadMultiply},
    {SDLK_KP_MINUS, Key::KeypadMinus},
    {SDLK_KP_PLUS, Key::KeypadPlus},
    {SDLK_KP_ENTER, Key::KeypadEnter},
    {SDLK_KP_EQUALS, Key::KeypadEquals},
    {SDLK_UP, Key::Up},
    {SDLK_DOWN, Key::Down},
    {SDLK_RIGHT, Key::Right},
    {SDLK_LEFT, Key::Left},
    {SDLK_INSERT, Key::Insert},
    {SDLK_HOME, Key::Home},
    {SDLK_END, Key::End},
    {SDLK_PAGEUP, Key::PageUp},
    {SDLK_PAGEDOWN, Key::PageDown},
    {SDLK_F1, Key::F1},
    {SDLK_F2, Key::F2},
    {SDLK_F3, Key::F3},
    {SDLK_F4, Key::F4},
    {SDLK_F5, Key::F5},
    {SDLK_F6, Key::F6},
    {SDLK_F7, Key::F7},
    {SDLK_F8, Key::F8},
    {SDLK_F9, Key::F9},
    {SDLK_F10, Key::F10},
    {SDLK_F11, Key::F11},
    {SDLK_F12, Key::F12},
    {SDLK_F13, Key::F13},
    {SDLK_F14, Key::F14},
    {SDLK_F15, Key::F15},
    {SDLK_F16, Key::F16},
    {SDLK_F17, Key::F17},
    {SDLK_F18, Key::F18},
    {SDLK_F19, Key::F19},
    {SDLK_F20, Key::F20},
    {SDLK_F21, Key::F21},
    {SDLK_F22, Key::F22},
    {SDLK_F23, Key::F23},
    {SDLK_F24, Key::F24},
    {SDLK_NUMLOCKCLEAR, Key::NumLock},
    {SDLK_CAPSLOCK, Key::CapsLock},
    {SDLK_SCROLLLOCK, Key::ScrollLock},
    {SDLK_RSHIFT, Key::RShift},
    {SDLK_LSHIFT, Key::LShift},
    {SDLK_RCTRL, Key::RCtrl},
    {SDLK_LCTRL, Key::LCtrl},
    {SDLK_RALT, Key::RAlt},
    {SDLK_LALT, Key::LAlt},
    {SDLK_RGUI, Key::RGui},
    {SDLK_LGUI, Key::LGui},
    {SDLK_MODE, Key::AltGr},
    {SDLK_APPLICATION, Key::Compose},
    {SDLK_HELP, Key::Help},
    {SDLK_PRINTSCREEN, Key::PrintScreen},
    {SDLK_SYSREQ, Key::SysReq},
    {SDLK_PAUSE, Key::Pause},
    {SDLK_MENU, Key::Menu},
    {SDLK_POWER, Key::Power}
  };

  return KeyCodeMap.maybe(sym);
}

std::optional<Key> keyFromSdlScancode(SDL_Scancode scancode) {
  static HashMap<int, Key> ScanCodeMap{
  {SDL_SCANCODE_A, Key::A},
  {SDL_SCANCODE_B, Key::B},
  {SDL_SCANCODE_C, Key::C},
  {SDL_SCANCODE_D, Key::D},
  {SDL_SCANCODE_E, Key::E},
  {SDL_SCANCODE_F, Key::F},
  {SDL_SCANCODE_G, Key::G},
  {SDL_SCANCODE_H, Key::H},
  {SDL_SCANCODE_I, Key::I},
  {SDL_SCANCODE_J, Key::J},
  {SDL_SCANCODE_K, Key::K},
  {SDL_SCANCODE_L, Key::L},
  {SDL_SCANCODE_M, Key::M},
  {SDL_SCANCODE_N, Key::N},
  {SDL_SCANCODE_O, Key::O},
  {SDL_SCANCODE_P, Key::P},
  {SDL_SCANCODE_Q, Key::Q},
  {SDL_SCANCODE_R, Key::R},
  {SDL_SCANCODE_S, Key::S},
  {SDL_SCANCODE_T, Key::T},
  {SDL_SCANCODE_U, Key::U},
  {SDL_SCANCODE_V, Key::V},
  {SDL_SCANCODE_W, Key::W},
  {SDL_SCANCODE_X, Key::X},
  {SDL_SCANCODE_Y, Key::Y},
  {SDL_SCANCODE_Z, Key::Z},
  {SDL_SCANCODE_0, Key::Zero},
  {SDL_SCANCODE_1, Key::One},
  {SDL_SCANCODE_2, Key::Two},
  {SDL_SCANCODE_3, Key::Three},
  {SDL_SCANCODE_4, Key::Four},
  {SDL_SCANCODE_5, Key::Five},
  {SDL_SCANCODE_6, Key::Six},
  {SDL_SCANCODE_7, Key::Seven},
  {SDL_SCANCODE_8, Key::Eight},
  {SDL_SCANCODE_9, Key::Nine},
  {SDL_SCANCODE_MINUS, Key::Minus},
  {SDL_SCANCODE_EQUALS, Key::Equals},
  {SDL_SCANCODE_LEFTBRACKET, Key::LeftBracket},
  {SDL_SCANCODE_RIGHTBRACKET, Key::RightBracket},
  {SDL_SCANCODE_BACKSLASH, Key::Backslash},
  {SDL_SCANCODE_SEMICOLON, Key::Semicolon},
  {SDL_SCANCODE_APOSTROPHE, Key::Quote},
  {SDL_SCANCODE_GRAVE, Key::Backquote},
  {SDL_SCANCODE_COMMA, Key::Comma},
  {SDL_SCANCODE_PERIOD, Key::Period},
  {SDL_SCANCODE_SLASH, Key::Slash},
  {SDL_SCANCODE_BACKSPACE, Key::Backspace},
  {SDL_SCANCODE_TAB, Key::Tab},
  {SDL_SCANCODE_RETURN, Key::Return},
  {SDL_SCANCODE_ESCAPE, Key::Escape},
  {SDL_SCANCODE_SPACE, Key::Space},
  {SDL_SCANCODE_DELETE, Key::Delete},
  {SDL_SCANCODE_INSERT, Key::Insert},
  {SDL_SCANCODE_HOME, Key::Home},
  {SDL_SCANCODE_END, Key::End},
  {SDL_SCANCODE_PAGEUP, Key::PageUp},
  {SDL_SCANCODE_PAGEDOWN, Key::PageDown},
  {SDL_SCANCODE_UP, Key::Up},
  {SDL_SCANCODE_DOWN, Key::Down},
  {SDL_SCANCODE_LEFT, Key::Left},
  {SDL_SCANCODE_RIGHT, Key::Right},
  {SDL_SCANCODE_F1, Key::F1},
  {SDL_SCANCODE_F2, Key::F2},
  {SDL_SCANCODE_F3, Key::F3},
  {SDL_SCANCODE_F4, Key::F4},
  {SDL_SCANCODE_F5, Key::F5},
  {SDL_SCANCODE_F6, Key::F6},
  {SDL_SCANCODE_F7, Key::F7},
  {SDL_SCANCODE_F8, Key::F8},
  {SDL_SCANCODE_F9, Key::F9},
  {SDL_SCANCODE_F10, Key::F10},
  {SDL_SCANCODE_F11, Key::F11},
  {SDL_SCANCODE_F12, Key::F12},
  {SDL_SCANCODE_F13, Key::F13},
  {SDL_SCANCODE_F14, Key::F14},
  {SDL_SCANCODE_F15, Key::F15},
  {SDL_SCANCODE_F16, Key::F16},
  {SDL_SCANCODE_F17, Key::F17},
  {SDL_SCANCODE_F18, Key::F18},
  {SDL_SCANCODE_F19, Key::F19},
  {SDL_SCANCODE_F20, Key::F20},
  {SDL_SCANCODE_F21, Key::F21},
  {SDL_SCANCODE_F22, Key::F22},
  {SDL_SCANCODE_F23, Key::F23},
  {SDL_SCANCODE_F24, Key::F24},
  {SDL_SCANCODE_KP_0, Key::Keypad0},
  {SDL_SCANCODE_KP_1, Key::Keypad1},
  {SDL_SCANCODE_KP_2, Key::Keypad2},
  {SDL_SCANCODE_KP_3, Key::Keypad3},
  {SDL_SCANCODE_KP_4, Key::Keypad4},
  {SDL_SCANCODE_KP_5, Key::Keypad5},
  {SDL_SCANCODE_KP_6, Key::Keypad6},
  {SDL_SCANCODE_KP_7, Key::Keypad7},
  {SDL_SCANCODE_KP_8, Key::Keypad8},
  {SDL_SCANCODE_KP_9, Key::Keypad9},
  {SDL_SCANCODE_KP_PERIOD, Key::KeypadPeriod},
  {SDL_SCANCODE_KP_DIVIDE, Key::KeypadDivide},
  {SDL_SCANCODE_KP_MULTIPLY, Key::KeypadMultiply},
  {SDL_SCANCODE_KP_MINUS, Key::KeypadMinus},
  {SDL_SCANCODE_KP_PLUS, Key::KeypadPlus},
  {SDL_SCANCODE_KP_ENTER, Key::KeypadEnter},
  {SDL_SCANCODE_KP_EQUALS, Key::KeypadEquals},
  {SDL_SCANCODE_LCTRL, Key::LCtrl},
  {SDL_SCANCODE_RCTRL, Key::RCtrl},
  {SDL_SCANCODE_LSHIFT, Key::LShift},
  {SDL_SCANCODE_RSHIFT, Key::RShift},
  {SDL_SCANCODE_LALT, Key::LAlt},
  {SDL_SCANCODE_RALT, Key::RAlt},
  {SDL_SCANCODE_LGUI, Key::LGui},
  {SDL_SCANCODE_RGUI, Key::RGui},
  {SDL_SCANCODE_MODE, Key::AltGr},
  {SDL_SCANCODE_CAPSLOCK, Key::CapsLock},
  {SDL_SCANCODE_NUMLOCKCLEAR, Key::NumLock},
  {SDL_SCANCODE_SCROLLLOCK, Key::ScrollLock},
  {SDL_SCANCODE_PRINTSCREEN, Key::PrintScreen},
  {SDL_SCANCODE_PAUSE, Key::Pause},
  {SDL_SCANCODE_MENU, Key::Menu},
  {SDL_SCANCODE_APPLICATION, Key::Compose},
  {SDL_SCANCODE_POWER, Key::Power},
  {SDL_SCANCODE_HELP, Key::Help},
  {SDL_SCANCODE_SYSREQ, Key::SysReq},
  };

  return ScanCodeMap.maybe(scancode);
}

KeyMod keyModsFromSdlKeyMods(std::uint16_t mod) {
  return static_cast<KeyMod>(mod);
}

MouseButton mouseButtonFromSdlMouseButton(std::uint8_t button) {
  switch (button) {
    case SDL_BUTTON_LEFT: return MouseButton::Left;
    case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
    case SDL_BUTTON_RIGHT: return MouseButton::Right;
    case SDL_BUTTON_X1: return MouseButton::FourthButton;
    default: return MouseButton::FifthButton;
  }
}

ControllerAxis controllerAxisFromSdlControllerAxis(std::uint8_t axis) {
  switch (axis) {
    case SDL_GAMEPAD_AXIS_LEFTX : return ControllerAxis::LeftX;
    case SDL_GAMEPAD_AXIS_LEFTY : return ControllerAxis::LeftY;
    case SDL_GAMEPAD_AXIS_RIGHTX : return ControllerAxis::RightX;
    case SDL_GAMEPAD_AXIS_RIGHTY : return ControllerAxis::RightY;
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER : return ControllerAxis::TriggerLeft;
    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER : return ControllerAxis::TriggerRight;
    default: return ControllerAxis::Invalid;
  }
}

ControllerButton controllerButtonFromSdlControllerButton(std::uint8_t button) {
  switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH : return ControllerButton::A;
    case SDL_GAMEPAD_BUTTON_EAST : return ControllerButton::B;
    case SDL_GAMEPAD_BUTTON_WEST : return ControllerButton::X;
    case SDL_GAMEPAD_BUTTON_NORTH : return ControllerButton::Y;
    case SDL_GAMEPAD_BUTTON_BACK : return ControllerButton::Back;
    case SDL_GAMEPAD_BUTTON_GUIDE : return ControllerButton::Guide;
    case SDL_GAMEPAD_BUTTON_START : return ControllerButton::Start;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK : return ControllerButton::LeftStick;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK : return ControllerButton::RightStick;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER : return ControllerButton::LeftShoulder;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER : return ControllerButton::RightShoulder;
    case SDL_GAMEPAD_BUTTON_DPAD_UP : return ControllerButton::DPadUp;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN : return ControllerButton::DPadDown;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT : return ControllerButton::DPadLeft;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT : return ControllerButton::DPadRight;
    case SDL_GAMEPAD_BUTTON_MISC1 : return ControllerButton::Misc1;
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1 : return ControllerButton::Paddle1;
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1 : return ControllerButton::Paddle2;
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2 : return ControllerButton::Paddle3;
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2 : return ControllerButton::Paddle4;
    case SDL_GAMEPAD_BUTTON_TOUCHPAD : return ControllerButton::Touchpad;
    default: return ControllerButton::Invalid;
  }
}

class SdlPlatform {
public:
  explicit SdlPlatform(std::unique_ptr<Application> application)
    : m_cursorCache(m_cursorRandom), m_application(std::move(application)) {}

  [[nodiscard]] std::expected<void, StarException> initialize(StringList cmdLineArgs) {
    if (cmdLineArgs.empty())
      return std::unexpected(ApplicationException("Application command line is missing its executable path"));
    String applicationPath = cmdLineArgs[0];
    cmdLineArgs.erase(cmdLineArgs.begin());

    StringList platformArguments;
    for (auto it = cmdLineArgs.begin(); it != cmdLineArgs.end();) {
      auto platformArgument = it->beginsWith("+platform");
      if (!platformArgument)
        return std::unexpected(std::move(platformArgument.error()));
      if (*platformArgument) {
        platformArguments.append(std::move(*it));
        it = cmdLineArgs.erase(it);
      } else {
        ++it;
      }
    }
    if (auto result = m_cursorRandom.init(); !result)
      return std::unexpected(std::move(result.error()));

    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, "OpenStarbound");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING, "io.github.openstarbound.openstarbound");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_URL_STRING, "https://github.com/OpenStarbound/OpenStarbound");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_TYPE_STRING, "game");

    EmergencyLog::info("Application: Initializing SDL");
    if (!SDL_Init(0))
      return std::unexpected(ApplicationException(strf("Couldn't initialize SDL: {}", SDL_GetError())));
    m_sdlInitialized = true;
    auto contextScope = NativeUiContextScope::create(nullptr, nullptr, nullptr);
    if (!contextScope)
      return std::unexpected(std::move(contextScope.error()));

    if (auto basePath = SDL_GetBasePath()) {
      auto result = File::changeDirectory(String(std::string_view(basePath)));
      if (!result)
        return std::unexpected(std::move(result.error()));
    }

    if (auto result = m_signalHandler.setHandleInterrupt(true); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_signalHandler.setHandleFatal(true); !result)
      return std::unexpected(std::move(result.error()));

    EmergencyLog::info("Application: startup...");
    if (auto result = m_application->startup(cmdLineArgs); !result)
      return std::unexpected(ApplicationException("Application failed during startup", ExceptionDiagnostic(result.error())));
    m_applicationStarted = true;

#ifdef STAR_SYSTEM_LINUX
    SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_SCALE_TO_DISPLAY, "1");
#endif
    
    if (auto result = m_application->logger().info("Application: Initializing SDL Video"); !result)
      return std::unexpected(std::move(result.error()));
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
      return std::unexpected(ApplicationException(strf("Couldn't initialize SDL Video: {}", SDL_GetError())));

    if (auto result = m_application->logger().info("Application: using Video Driver '{}'", SDL_GetCurrentVideoDriver()); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = m_application->logger().info("Application: Initializing SDL Controller"); !result)
      return std::unexpected(std::move(result.error()));
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
      return std::unexpected(ApplicationException(strf("Couldn't initialize SDL Controller: {}", SDL_GetError())));

    if (auto result = m_application->logger().info("Application: Initializing SDL Audio"); !result)
      return std::unexpected(std::move(result.error()));
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
      return std::unexpected(ApplicationException(strf("Couldn't initialize SDL Audio: {}", SDL_GetError())));

    if (auto result = m_application->logger().info("Application: using Audio Driver '{}'", SDL_GetCurrentAudioDriver()); !result)
      return std::unexpected(std::move(result.error()));

    SDL_SetJoystickEventsEnabled(true);

    auto platformServices = PcPlatformServices::create(m_application->logger(), applicationPath, platformArguments);
    if (!platformServices)
      return std::unexpected(std::move(platformServices.error()));
    m_platformServices = std::move(*platformServices);
    if (!m_platformServices)
      if (auto result = m_application->logger().info("Application: No platform services available"); !result)
        return std::unexpected(std::move(result.error()));

    if (auto result = m_application->logger().info("Application: Creating SDL window"); !result)
      return std::unexpected(std::move(result.error()));
#ifdef STAR_SYSTEM_MACOS
    m_sdlWindow.reset(SDL_CreateWindow(m_windowTitle.utf8Ptr(), m_windowSize[0], m_windowSize[1], SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE));
#else
    m_sdlWindow.reset(SDL_CreateWindow(m_windowTitle.utf8Ptr(), m_windowSize[0], m_windowSize[1], SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY));
#endif
    if (!m_sdlWindow)
      return std::unexpected(ApplicationException::format("Application: Could not create SDL Window: {}", SDL_GetError()));
    
#ifndef STAR_SYSTEM_MACOS
    m_displayScale = SDL_GetWindowDisplayScale(m_sdlWindow.get());
#endif
    
#ifdef STAR_SYSTEM_LINUX
    auto iconExists = File::isFile(".icon/openstarbound.png");
    if (!iconExists)
      return std::unexpected(std::move(iconExists.error()));
    if (*iconExists) {
      auto device = File::open(".icon/openstarbound.png", IOMode::Read);
      if (!device)
        return std::unexpected(std::move(device.error()));
      auto isPng = Image::isPng(*device);
      if (!isPng)
        return std::unexpected(std::move(isPng.error()));
      if (*isPng) {
        auto img = Image::readPng(m_application->logger(), *device);
        if (!img)
          return std::unexpected(std::move(img.error()));
        auto conv = img->convert(PixelFormat::RGBA32);
        auto flippedResult = processImageOperations(
          List<ImageOperation>{ FlipImageOperation{ FlipImageOperation::Mode::FlipY } }, conv);
        if (!flippedResult)
          return std::unexpected(std::move(flippedResult.error()));
        auto& flipped = *flippedResult;

        SDL_PixelFormat sdlFmt = static_cast<SDL_PixelFormat>(SDL_PIXELFORMAT_ABGR8888);

        int w = static_cast<int>(flipped.width());
        int h = static_cast<int>(flipped.height());
        int pitch = static_cast<int>(flipped.bytesPerPixel() * flipped.width());

        std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> iconSurface(SDL_CreateSurfaceFrom(
          w,
          h,
          sdlFmt,
          flipped.data().data(),
          pitch
        ), SDL_DestroySurface);

        if (iconSurface) {
          SDL_SetWindowIcon(m_sdlWindow.get(), iconSurface.get());
        } else {
          if (auto result = m_application->logger().warn("Could not create SDL surface for icon '{}': {}", ".icon/openstarbound.png", SDL_GetError()); !result)
            return std::unexpected(std::move(result.error()));
        }
      }

      // Get XDG Icons Directory following XDG Base Directory Specification
      String XdgIconsPath;
      const char* XdgDataHome = std::getenv("XDG_DATA_HOME");
      const char* Home = std::getenv("HOME");

      if (XdgDataHome && XdgDataHome[0] != '\0') {
        auto path = String::joinWith("", String(std::string_view(XdgDataHome)), "/icons");
        if (!path)
          return std::unexpected(std::move(path.error()));
        XdgIconsPath = std::move(*path);
      } else if (Home && Home[0] != '\0') {
        auto path = String::joinWith("", String(std::string_view(Home)), "/.local/share/icons");
        if (!path)
          return std::unexpected(std::move(path.error()));
        XdgIconsPath = std::move(*path);
      }

      // Install Icon if not Installed
      std::expected<bool, StarException> iconsDirectoryExists = false;
      if (!XdgIconsPath.empty())
        iconsDirectoryExists = File::isDirectory(XdgIconsPath);
      if (!iconsDirectoryExists)
        return std::unexpected(std::move(iconsDirectoryExists.error()));
      if (*iconsDirectoryExists) {
        auto icon = String::joinWith("", XdgIconsPath, "/openstarbound.png");
        if (!icon)
          return std::unexpected(std::move(icon.error()));
        const String XdgIcon = std::move(*icon);
        auto installed = File::isFile(XdgIcon);
        if (!installed)
          return std::unexpected(std::move(installed.error()));
        if (!*installed) {
          if (auto result = File::copy(".icon/openstarbound.png", XdgIcon); !result)
            return std::unexpected(std::move(result.error()));
          if (auto result = m_application->logger().info("Application: Installed Icon to XDG Icons Directory"); !result)
            return std::unexpected(std::move(result.error()));
        }
      }
    }

    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_APP_ICON_NAME, "openstarbound");
#endif

    if (auto result = m_application->logger().info("Application: Creating SDL OpenGL context"); !result)
      return std::unexpected(std::move(result.error()));
    static const std::pair<int,int> versions[] = {{4,6},{4,5},{4,4},{4,3},{4,2},{4,1},{4,0},{3,3},{3,2},{3,1}};
    for (auto& [majorVersion, minorVersion] : versions) {
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, majorVersion);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minorVersion);
      m_sdlGlContext.reset(SDL_GL_CreateContext(m_sdlWindow.get()));
      if (m_sdlGlContext) break;
    }
    if (!m_sdlGlContext)
      return std::unexpected(ApplicationException::format("Application: Could not create OpenGL context: {}", SDL_GetError()));

    SDL_ShowWindow(m_sdlWindow.get());
    SDL_RaiseWindow(m_sdlWindow.get());

    int width;
    int height;
    SDL_GetWindowSize(m_sdlWindow.get(), &width, &height);
    m_windowSize = Vec2U(width, height);

    SDL_GL_SwapWindow(m_sdlWindow.get());
    if (auto result = setVSyncEnabled(m_windowVSync); !result)
      return std::unexpected(std::move(result.error()));

    SDL_StopTextInput(m_sdlWindow.get());

    if (auto result = m_application->logger().info("Application: Opening audio device"); !result)
      return std::unexpected(std::move(result.error()));
    m_audioOutputData.clear();
    SDL_AudioSpec desired = {SDL_AUDIO_S16, 2, 44100};
    m_sdlAudioOutputStream.reset(SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired,
    [](void* userdata, SDL_AudioStream* stream, int len, int) {
      if (len > 0) {
        auto& sdlPlatform = *static_cast<SdlPlatform*>(userdata);
        sdlPlatform.m_audioOutputData.resize(len / sizeof(std::int16_t));
        auto result = sdlPlatform.getAudioData(sdlPlatform.m_audioOutputData);
        if (!result) {
          sdlPlatform.recordAudioError(std::move(result.error()));
          return;
        }
        SDL_PutAudioStreamData(stream, sdlPlatform.m_audioOutputData.data(), len);
      }
    }, this));
    if (!m_sdlAudioOutputStream) {
      if (auto result = m_application->logger().error("Application: Could not open audio device, no sound available!"); !result)
        return std::unexpected(std::move(result.error()));
    } else {
      if (auto result = m_application->logger().info("Application: Opened default audio device with 44.1khz / 16 bit stereo audio"); !result)
        return std::unexpected(std::move(result.error()));
      SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(m_sdlAudioOutputStream.get()));
    }

    auto renderer = OpenGlRenderer::create(m_application->logger());
    if (!renderer)
      return std::unexpected(std::move(renderer.error()));
    m_renderer = std::move(*renderer);
    if (auto result = m_renderer->setScreenSize(m_windowSize); !result)
      return std::unexpected(std::move(result.error()));

    m_cursorCache.setTimeToLive(30000);

    IMGUI_CHECKVERSION();
    m_imguiContext.reset(ImGui::CreateContext());
    if (!m_imguiContext)
      return std::unexpected(ApplicationException("Application: Could not create ImGui context"));
    ImGui::SetCurrentContext(m_imguiContext.get());
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigNavCaptureKeyboard = false;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    style.ScaleAllSizes(main_scale);

    m_imguiSdlInitialized = ImGui_ImplSDL3_InitForOpenGL(m_sdlWindow.get(), m_sdlGlContext.get());
    if (!m_imguiSdlInitialized)
      return std::unexpected(ApplicationException::format("Application: Could not initialize ImGui SDL backend: {}", SDL_GetError()));
    m_imguiOpenGlInitialized = ImGui_ImplOpenGL3_Init(nullptr);
    if (!m_imguiOpenGlInitialized)
      return std::unexpected(ApplicationException("Application: Could not initialize ImGui OpenGL backend"));

    return contextScope->restore();
  }

  bool logInitializationFailure(StarException const& error) {
    if (!m_applicationStarted)
      return false;
    if (auto logged = m_application->logger().errorException("Application: failed during initialization", ExceptionDiagnostic(error)); !logged) {
      EmergencyLog::error("{}", exceptionReport(error));
      EmergencyLog::error("Application: failure logging failed: {}", exceptionReport(logged.error()));
    }
    return true;
  }

  ~SdlPlatform() {
    if (auto result = releaseResources(); !result)
      EmergencyLog::error("Application: cleanup failed: {}", outputException(ExceptionDiagnostic(result.error()), true));
  }

  using AudioCallback = ApplicationController::AudioCallback;
  [[nodiscard]] std::expected<bool, StarException> openAudioInputDevice(SDL_AudioDeviceID deviceId, int freq, int channels, AudioCallback callback) {
    closeAudioInputDevice();
    if (!callback) {
      if (auto result = m_application->logger().error("Cannot open audio input with an empty callback"); !result)
        return std::unexpected(std::move(result.error()));
      return false;
    }
    m_audioInputCallback = std::move(callback);
    SDL_AudioSpec desired = {SDL_AUDIO_S16, channels, freq};
    m_sdlAudioInputStream.reset(SDL_OpenAudioDeviceStream(deviceId, &desired,
    [](void* userdata, SDL_AudioStream* stream, int len, int) {
      if (len > 0) {
        auto& sdlPlatform = *static_cast<SdlPlatform*>(userdata);
        sdlPlatform.m_audioInputData.resize(len);
        int received = SDL_GetAudioStreamData(stream, sdlPlatform.m_audioInputData.data(), len);
        if (received > 0)
          sdlPlatform.m_audioInputCallback(std::span(sdlPlatform.m_audioInputData).first(received));
        else if (received < 0)
          if (auto result = sdlPlatform.m_application->logger().error("Could not read audio input stream: {}", SDL_GetError()); !result)
            sdlPlatform.recordAudioError(std::move(result.error()));
      }
    }, this));

    if (m_sdlAudioInputStream) {
      if (auto result = m_application->logger().info("Opened audio input device '{}'", SDL_GetAudioDeviceName(SDL_GetAudioStreamDevice(m_sdlAudioInputStream.get()))); !result) {
        closeAudioInputDevice();
        return std::unexpected(std::move(result.error()));
      }
      SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(m_sdlAudioInputStream.get()));
    }
    else {
      m_audioInputCallback = {};
      if (auto result = m_application->logger().info("Failed to open audio input device: {}", SDL_GetError()); !result)
        return std::unexpected(std::move(result.error()));
    }

    return static_cast<bool>(m_sdlAudioInputStream);
  }

  bool closeAudioInputDevice() {
    if (m_sdlAudioInputStream) {
      if (auto result = m_application->logger().info("Closing audio input device"); !result)
        EmergencyLog::error("Application: cleanup logging failed: {}", outputException(ExceptionDiagnostic(result.error()), true));
      // Detaching takes the stream lock and waits for an in-flight callback.
      SDL_SetAudioStreamPutCallback(m_sdlAudioInputStream.get(), nullptr, nullptr);
      m_sdlAudioInputStream.reset();
      m_audioInputCallback = {};
      return true;
    }
    return false;
  }

  [[nodiscard]] std::expected<void, StarException> cleanup() {
    if (auto result = m_cursorCache.maybeRef(m_currentCursor); !result)
      return std::unexpected(std::move(result.error()));
    m_cursorCache.cleanup();
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> run() {
    auto contextScope = NativeUiContextScope::create(m_sdlWindow.get(), m_sdlGlContext.get(), m_imguiContext.get());
    bool imguiFrameActive = false;
    auto result = [&]() -> std::expected<void, StarException> {
      if (!contextScope)
        return std::unexpected(std::move(contextScope.error()));
      if (auto result = m_application->logger().info("Application: initialization..."); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = m_application->applicationInit(std::make_shared<Controller>(*this)); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = m_application->logger().info("Application: renderer initialization..."); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = m_application->renderInit(m_renderer); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = m_application->logger().info("Application: main update loop..."); !result)
        return std::unexpected(std::move(result.error()));
      m_updateTicker.reset();
      m_renderTicker.reset();
      bool quit = false;
      while (true) {
        auto frameStart = Time::monotonicMicroseconds();
        if (auto error = takeAudioError())
          return std::unexpected(std::move(*error));
        if (auto result = cleanup(); !result)
          return std::unexpected(std::move(result.error()));
        auto events = processEvents();
        if (!events)
          return std::unexpected(std::move(events.error()));
        for (auto const& event : *events) {
          if (auto result = m_application->processInput(event); !result)
            return std::unexpected(std::move(result.error()));
        }
        if (m_platformServices) {
          if (auto result = m_platformServices->update(); !result)
            return std::unexpected(std::move(result.error()));
        }
        if (m_cursorVisible || (m_platformServices && m_platformServices->overlayActive()))
          SDL_ShowCursor();
        else
          SDL_HideCursor();


        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();

        auto updateStart = Time::monotonicMicroseconds();
        int updatesBehind = std::max<int>(std::round(m_updateTicker.ticksBehind()), 1);
        updatesBehind = std::min<int>(updatesBehind, m_maxFrameSkip + 1);
        for (int i = 0; i < updatesBehind; ++i) {
          //since frame-skipping is a thing, we have to begin a new ImGui frame here to prevent duplicate elements made by updates
          if (i != 0)
            ImGui::EndFrame();
          ImGui::NewFrame();
          imguiFrameActive = true;
          if (auto result = m_application->update(); !result)
            return std::unexpected(std::move(result.error()));
          m_updateRate = m_updateTicker.tick();
        }

        auto renderStart = Time::monotonicMicroseconds();
        if (auto result = m_renderer->startFrame(); !result)
          return std::unexpected(std::move(result.error()));
        if (auto result = m_application->render(); !result)
          return std::unexpected(std::move(result.error()));
        auto finishStart = Time::monotonicMicroseconds();
        if (auto result = m_renderer->finishFrame(); !result)
          return std::unexpected(std::move(result.error()));

        auto guiStart = Time::monotonicMicroseconds();
        ImGui::Render();
        imguiFrameActive = false;
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        auto presentStart = Time::monotonicMicroseconds();
        SDL_GL_SwapWindow(m_sdlWindow.get());
        m_renderRate = m_renderTicker.tick();
        auto presentEnd = Time::monotonicMicroseconds();

        if (m_quitRequested) {
          if (auto result = m_application->logger().info("Application: quit requested"); !result)
            return std::unexpected(std::move(result.error()));
          quit = true;
        }

        if (m_signalHandler.interruptCaught()) {
          if (auto result = m_application->logger().info("Application: Interrupt caught"); !result)
            return std::unexpected(std::move(result.error()));
          quit = true;
        }

        if (quit) {
          if (auto result = m_application->logger().info("Application: quitting..."); !result)
            return std::unexpected(std::move(result.error()));
        }

        auto throttleStart = Time::monotonicMicroseconds();
        if (!quit) {
          std::int64_t spareMilliseconds = std::round(m_updateTicker.spareTime() * 1000);
          if (spareMilliseconds > 0)
            Thread::sleepPrecise(spareMilliseconds);
        }
        auto frameEnd = Time::monotonicMicroseconds();
        m_application->recordFrameTimings({updateStart - frameStart, renderStart - updateStart,
                                           finishStart - renderStart, guiStart - finishStart, presentStart - guiStart,
                                           presentEnd - presentStart, quit ? 0 : frameEnd - throttleStart,
                                           frameEnd - frameStart, static_cast<unsigned>(updatesBehind)});
        if (quit)
          break;
      }
      return {};
    }();
    if (imguiFrameActive)
      ImGui::EndFrame();
    if (!result)
      if (auto logged = m_application->logger().errorException("Application: failed", ExceptionDiagnostic(result.error())); !logged) {
        EmergencyLog::error("{}", exceptionReport(result.error()));
        EmergencyLog::error("Application: failure logging failed: {}", exceptionReport(logged.error()));
      }

    closeAudioOutputDevice();
    closeAudioInputDevice();
    if (auto error = takeAudioError()) {
      if (result) {
        result = std::unexpected(std::move(*error));
        if (auto logged = m_application->logger().errorException("Application: audio callback failed", ExceptionDiagnostic(result.error())); !logged) {
          EmergencyLog::error("Application: audio callback failed: {}", exceptionReport(result.error()));
          EmergencyLog::error("Application: cleanup logging failed: {}", exceptionReport(logged.error()));
        }
      } else
        if (auto logged = m_application->logger().errorException("Application: audio callback failed", ExceptionDiagnostic(*error)); !logged) {
          EmergencyLog::error("Application: audio callback failed: {}", outputException(ExceptionDiagnostic(*error), true));
          EmergencyLog::error("Application: cleanup logging failed: {}", outputException(ExceptionDiagnostic(logged.error()), true));
        }
    }
    if (auto logged = m_application->logger().info("Application: shutdown..."); !logged)
      EmergencyLog::error("Application: cleanup logging failed: {}", outputException(ExceptionDiagnostic(logged.error()), true));
    if (auto shutdown = m_application->shutdown(); !shutdown) {
      if (auto logged = m_application->logger().errorException("Application: failed during shutdown", ExceptionDiagnostic(shutdown.error())); !logged) {
        EmergencyLog::error("Application: failed during shutdown: {}", outputException(ExceptionDiagnostic(shutdown.error()), true));
        EmergencyLog::error("Application: cleanup logging failed: {}", outputException(ExceptionDiagnostic(logged.error()), true));
      }
    }

    m_SdlControllers.clear();

    SDL_SetCursor(NULL);
    m_cursorCache.clear();

    m_platformServices.reset();
    m_renderer.reset();
    if (result) {
      if (auto logged = m_application->logger().info("Application: stopped gracefully"); !logged) {
        result = std::unexpected(std::move(logged.error()));
        if (auto reported = m_application->logger().errorException("Application: final logging failed", ExceptionDiagnostic(result.error())); !reported)
          EmergencyLog::error("Application: final logging failed: {}", exceptionReport(result.error()));
      }
    }
    m_application.reset();
    if (contextScope) {
      if (auto restored = contextScope->restore(); !restored) {
        if (result) {
          result = std::unexpected(std::move(restored.error()));
          EmergencyLog::error("Application: context restoration failed: {}", exceptionReport(result.error()));
        } else
          EmergencyLog::error("Application: context restoration failed: {}", outputException(ExceptionDiagnostic(restored.error()), true));
      }
    }
    return result;
  }

private:
  void recordAudioError(StarException error) {
    std::lock_guard lock(m_audioErrorMutex);
    if (!m_audioError)
      m_audioError.emplace(std::move(error));
    else
      if (auto logged = m_application->logger().errorException("Application: audio callback failed", ExceptionDiagnostic(error)); !logged) {
        EmergencyLog::error("Application: audio callback failed: {}", outputException(ExceptionDiagnostic(error), true));
        EmergencyLog::error("Application: audio failure logging failed: {}", outputException(ExceptionDiagnostic(logged.error()), true));
      }
  }

  std::optional<StarException> takeAudioError() {
    std::lock_guard lock(m_audioErrorMutex);
    return std::exchange(m_audioError, {});
  }

  void closeAudioOutputDevice() {
    if (m_sdlAudioOutputStream) {
      SDL_SetAudioStreamGetCallback(m_sdlAudioOutputStream.get(), nullptr, nullptr);
      m_sdlAudioOutputStream.reset();
    }
  }

  [[nodiscard]] std::expected<void, StarException> releaseResources() {
    if (!m_sdlInitialized) {
      m_application.reset();
      return {};
    }
    auto contextScope = NativeUiContextScope::create(m_sdlWindow.get(), m_sdlGlContext.get(), m_imguiContext.get());
    if (!contextScope && m_imguiContext)
      ImGui::SetCurrentContext(m_imguiContext.get());
    // Stop callbacks before destroying their application/userdata and buffers.
    closeAudioOutputDevice();
    closeAudioInputDevice();
    m_platformServices.reset();
    m_renderer.reset();
    m_application.reset();
    if (std::exchange(m_imguiOpenGlInitialized, false))
      ImGui_ImplOpenGL3_Shutdown();
    if (std::exchange(m_imguiSdlInitialized, false))
      ImGui_ImplSDL3_Shutdown();
    if (m_imguiContext) {
      if (contextScope)
        contextScope->discardImguiContext(m_imguiContext.get());
      m_imguiContext.reset();
    }
    m_SdlControllers.clear();
    SDL_SetCursor(nullptr);
    m_cursorCache.clear();
    if (m_sdlGlContext) {
      if (contextScope)
        contextScope->discardGlContext(m_sdlGlContext.get());
      m_sdlGlContext.reset();
    }
    if (m_sdlWindow)
      m_sdlWindow.reset();
    if (std::exchange(m_sdlInitialized, false))
      SDL_Quit();
    if (!contextScope)
      return std::unexpected(std::move(contextScope.error()));
    return contextScope->restore();
  }

  struct Controller : public ApplicationController {
    Controller(SdlPlatform& parent)
      : parent(parent) {}

    bool hasClipboard() override {
      return SDL_HasClipboardText();
    }

    std::optional<String> getClipboard() override {
      std::optional<String> string;
      if (SDL_HasClipboardText()) {
        if (auto text = SDL_GetClipboardText()) {
          if (*text != '\0')
            string.emplace(std::string_view(text));
          SDL_free(text);
        }
      }
      return string;
    }

    bool setClipboard(String text) override {
      return SDL_SetClipboardText(text.utf8Ptr());
    }

    bool setClipboardData(StringMap<ByteArray> data) override {
      return parent.setClipboardData(std::move(data));
    }

    [[nodiscard]] std::expected<bool, StarException> setClipboardImage(Image const& image, std::optional<std::reference_wrapper<ByteArray>> png, std::optional<std::reference_wrapper<String const>> path) override {
      return parent.setClipboardImage(image, png, path);
    }

    [[nodiscard]] std::expected<bool, StarException> setClipboardFile(String const& path) override {
      return parent.setClipboardFile(path);
    }

    bool isFocused() const override {
      return (SDL_GetWindowFlags(parent.m_sdlWindow.get()) & SDL_WINDOW_INPUT_FOCUS) != 0;
    }

    void setTargetUpdateRate(float targetUpdateRate) override {
      parent.m_updateTicker.setTargetTickRate(targetUpdateRate);
    }

    void setUpdateTrackWindow(float updateTrackWindow) override {
      parent.m_updateTicker.setWindow(updateTrackWindow);
    }

    void setApplicationTitle(String title) override {
      parent.m_windowTitle = std::move(title);
      if (parent.m_sdlWindow)
        SDL_SetWindowTitle(parent.m_sdlWindow.get(), parent.m_windowTitle.utf8Ptr());
    }

    [[nodiscard]] std::expected<void, StarException> setFullscreenWindow(Vec2U fullScreenResolution) override {
      if (parent.m_windowMode != WindowMode::Fullscreen || parent.m_windowSize != fullScreenResolution) {
        int currentDisplayIndex = SDL_GetDisplayForWindow(parent.m_sdlWindow.get());

        SDL_DisplayMode closestDisplayMode;
        if (SDL_GetClosestFullscreenDisplayMode(currentDisplayIndex, (int)fullScreenResolution[0], (int)fullScreenResolution[1], 0.f, true, &closestDisplayMode)) {
          if (SDL_SetWindowFullscreenMode(parent.m_sdlWindow.get(), &closestDisplayMode)) {
            if (parent.m_windowMode == WindowMode::Fullscreen)
              SDL_SetWindowFullscreen(parent.m_sdlWindow.get(), 0);
            else if (parent.m_windowMode == WindowMode::Borderless)
              SDL_SetWindowBordered(parent.m_sdlWindow.get(), true);
            else if (parent.m_windowMode == WindowMode::Maximized)
              SDL_RestoreWindow(parent.m_sdlWindow.get());

            parent.m_windowMode = WindowMode::Fullscreen;
            SDL_SetWindowFullscreen(parent.m_sdlWindow.get(), SDL_WINDOW_FULLSCREEN);
          } else {
            if (auto result = parent.m_application->logger().warn("Failed to set resolution {}, {}", (unsigned)closestDisplayMode.w, (unsigned)closestDisplayMode.h); !result)
              return std::unexpected(std::move(result.error()));
          }
        } else {
          if (auto result = parent.m_application->logger().warn("Unable to set requested display resolution {}, {}", (int)fullScreenResolution[0], (int)fullScreenResolution[1]); !result)
            return std::unexpected(std::move(result.error()));
        }

        if (auto displayMode = SDL_GetWindowFullscreenMode(parent.m_sdlWindow.get())) {
          parent.m_windowSize = {(unsigned)displayMode->w, (unsigned)displayMode->h};

          // call these manually since no SDL_WindowEvent is triggered when changing between fullscreen resolutions for some reason
          if (auto result = parent.m_renderer->setScreenSize(parent.m_windowSize); !result)
            return std::unexpected(std::move(result.error()));
          if (auto result = parent.m_application->windowChanged(parent.m_windowMode, parent.m_windowSize); !result)
            return std::unexpected(std::move(result.error()));
        } else {
          if (auto result = parent.m_application->logger().error("Couldn't get window display mode!"); !result)
            return std::unexpected(std::move(result.error()));
        }
      }
      return {};
    }

    void setNormalWindow(Vec2U windowSize) override {
      auto const& window = parent.m_sdlWindow;
      if (parent.m_windowMode != WindowMode::Normal || parent.m_windowSize != windowSize) {
        if (parent.m_windowMode == WindowMode::Fullscreen)
          SDL_SetWindowFullscreen(window.get(), 0);
        else if (parent.m_windowMode == WindowMode::Borderless)
          SDL_SetWindowBordered(window.get(), true);
        else if (parent.m_windowMode == WindowMode::Maximized)
          SDL_RestoreWindow(window.get());

        SDL_SetWindowBordered(window.get(), true);
        SDL_SetWindowSize(window.get(), windowSize[0], windowSize[1]);
        SDL_SetWindowPosition(window.get(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

        parent.m_windowMode = WindowMode::Normal;
        parent.m_windowSize = windowSize;
      }
    }

    void setMaximizedWindow() override {
      if (parent.m_windowMode != WindowMode::Maximized) {
        if (parent.m_windowMode == WindowMode::Fullscreen)
          SDL_SetWindowFullscreen(parent.m_sdlWindow.get(), 0);
        else if (parent.m_windowMode == WindowMode::Borderless)
          SDL_SetWindowBordered(parent.m_sdlWindow.get(), true);

        SDL_RestoreWindow(parent.m_sdlWindow.get());
        SDL_MaximizeWindow(parent.m_sdlWindow.get());
        parent.m_windowMode = WindowMode::Maximized;
      }
    }

    [[nodiscard]] std::expected<void, StarException> setBorderlessWindow() override {
      if (parent.m_windowMode != WindowMode::Borderless) {
        if (parent.m_windowMode == WindowMode::Fullscreen)
          SDL_SetWindowFullscreen(parent.m_sdlWindow.get(), 0);
        else if (parent.m_windowMode == WindowMode::Maximized)
          SDL_RestoreWindow(parent.m_sdlWindow.get());

        SDL_SetWindowBordered(parent.m_sdlWindow.get(), false);
        parent.m_windowMode = WindowMode::Borderless;

        if (auto displayMode = SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(parent.m_sdlWindow.get()))) {
          parent.m_windowSize = {(unsigned)displayMode->w, (unsigned)displayMode->h};
          #ifdef STAR_SYSTEM_WINDOWS
          if (m_borderlessWorkaround) { // breaks brightness on some setups god what the fuck fuck microsoft fuck nvidia
            auto handle = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(parent.m_sdlWindow.get()), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
            SetWindowLongPtr(handle, GWL_STYLE, WS_OVERLAPPED);
            SetWindowLongPtr(handle, GWL_EXSTYLE, WS_EX_APPWINDOW);
            SetWindowPos(handle, HWND_TOP, 0, 0, displayMode->w, displayMode->h, SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
          } // fuck everything dude it's all so fucked. everything is so fucked
          #endif
          SDL_SetWindowPosition(parent.m_sdlWindow.get(), 0, 0);
          SDL_SetWindowSize(parent.m_sdlWindow.get(), parent.m_windowSize[0], parent.m_windowSize[1]);
          if (auto result = parent.m_renderer->setScreenSize(parent.m_windowSize); !result)
            return std::unexpected(std::move(result.error()));
          if (auto result = parent.m_application->windowChanged(parent.m_windowMode, parent.m_windowSize); !result)
            return std::unexpected(std::move(result.error()));
        } else {
          if (auto result = parent.m_application->logger().error("Couldn't get desktop display mode!"); !result)
            return std::unexpected(std::move(result.error()));
        }
      }
      return {};
    }

    #ifdef STAR_SYSTEM_WINDOWS
    bool m_borderlessWorkaround = false;
    void setBorderlessWorkaround(bool enabled) override {
      m_borderlessWorkaround = enabled;
    }
    #endif

    [[nodiscard]] std::expected<void, StarException> setVSyncEnabled(bool vSync) override {
      if (parent.m_windowVSync != vSync) {
        if (auto result = parent.setVSyncEnabled(vSync); !result)
          return std::unexpected(std::move(result.error()));
        parent.m_windowVSync = vSync;
      }
      return {};
    }

    void setMaxFrameSkip(unsigned maxFrameSkip) override {
      parent.m_maxFrameSkip = maxFrameSkip;
    }

    void setCursorVisible(bool cursorVisible) override {
      parent.m_cursorVisible = cursorVisible;
    }

    void setCursorPosition(Vec2I cursorPosition) override {
      SDL_WarpMouseInWindow(parent.m_sdlWindow.get(), cursorPosition[0], cursorPosition[1]);
    }

    void setCursorHardware(bool hardware) override {
      parent.m_cursorHardware = hardware;
    }

    [[nodiscard]] std::expected<bool, StarException> setCursorImage(const String& id, const std::shared_ptr<Image const>& image, unsigned scale, const Vec2I& offset) override {
      return parent.setCursorImage(id, image, scale, offset);
    }

    void setAcceptingTextInput(bool acceptingTextInput) override {
      if (acceptingTextInput != parent.m_acceptingTextInput) {
        if (acceptingTextInput)
          SDL_StartTextInput(parent.m_sdlWindow.get());
        else
          SDL_StopTextInput(parent.m_sdlWindow.get());

        parent.m_acceptingTextInput = acceptingTextInput;
      }
    }

    void setTextArea(std::optional<std::pair<RectI, int>> area) override {
      if (parent.m_textInputArea == area)
        return;
      parent.m_textInputArea = area;
      if (area) {
        RectI& r = area->first;
        SDL_Rect rect{
          r.xMin(), (int)parent.m_windowSize.y() - r.yMax(),
          r.width(), r.height()
        };
        SDL_SetTextInputArea(parent.m_sdlWindow.get(), &rect, area->second);
      } else {
        SDL_SetTextInputArea(parent.m_sdlWindow.get(), NULL, 0);
      }
    }

    AudioFormat enableAudio() override {
      parent.m_audioEnabled = true;
      SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(parent.m_sdlAudioOutputStream.get()));
      return AudioFormat{44100, 2};
    }

    void disableAudio() override {
      parent.m_audioEnabled = false;
      SDL_PauseAudioDevice(SDL_GetAudioStreamDevice(parent.m_sdlAudioOutputStream.get()));
    }

    [[nodiscard]] std::expected<bool, StarException> openAudioInputDevice(std::uint32_t deviceId, int freq, int channels, AudioCallback callback) override {
      return parent.openAudioInputDevice(deviceId, freq, channels, callback);
    };

    bool closeAudioInputDevice() override {
      return parent.closeAudioInputDevice();
    };

    float updateRate() const override {
      return parent.m_updateRate;
    }

    float renderFps() const override {
      return parent.m_renderRate;
    }

  float getDisplayScale() const override {
    return parent.m_displayScale;
  }

    std::shared_ptr<StatisticsService> statisticsService() const override {
      if (parent.m_platformServices)
        return parent.m_platformServices->statisticsService();
      return {};
    }

    std::shared_ptr<P2PNetworkingService> p2pNetworkingService() const override {
      if (parent.m_platformServices)
        return parent.m_platformServices->p2pNetworkingService();
      return {};
    }

    std::shared_ptr<UserGeneratedContentService> userGeneratedContentService() const override {
      if (parent.m_platformServices)
        return parent.m_platformServices->userGeneratedContentService();
      return {};
    }

    std::shared_ptr<DesktopService> desktopService() const override {
      if (parent.m_platformServices)
        return parent.m_platformServices->desktopService();
      return {};
    }

    bool steamIsFlatpak() const override {
      return parent.m_platformServices && parent.m_platformServices->steamIsFlatpak();
    }

    void quit() override {
      parent.m_quitRequested = true;
    }

    SdlPlatform& parent;
  };

  [[nodiscard]] std::expected<List<InputEvent>, StarException> processEvents() {
    List<InputEvent> inputEvents;

    ImGuiIO& io = ImGui::GetIO();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      SDL_ConvertEventToRenderCoordinates(SDL_GetRenderer(m_sdlWindow.get()), &event);
      ImGui_ImplSDL3_ProcessEvent(&event);
      std::optional<InputEvent> starEvent;
      if (event.type == SDL_EVENT_WINDOW_MAXIMIZED || event.type == SDL_EVENT_WINDOW_RESTORED) {
        auto windowFlags = SDL_GetWindowFlags(m_sdlWindow.get());

        if (windowFlags & SDL_WINDOW_MAXIMIZED)
          m_windowMode = WindowMode::Maximized;
        else if (windowFlags & SDL_WINDOW_FULLSCREEN)
          m_windowMode = WindowMode::Fullscreen;
        else if (windowFlags & SDL_WINDOW_BORDERLESS)
          m_windowMode = WindowMode::Borderless;
        else
          m_windowMode = WindowMode::Normal;

        if (auto result = m_application->windowChanged(m_windowMode, m_windowSize); !result)
          return std::unexpected(std::move(result.error()));
      } else if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        m_windowSize = Vec2U(event.window.data1, event.window.data2);
        if (auto result = m_renderer->setScreenSize(m_windowSize); !result)
          return std::unexpected(std::move(result.error()));
        if (auto result = m_application->windowChanged(m_windowMode, m_windowSize); !result)
          return std::unexpected(std::move(result.error()));
      }
#ifndef STAR_SYSTEM_MACOS
      else if (event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
        m_displayScale = SDL_GetWindowDisplayScale(m_sdlWindow.get());
      }
#endif
      else if (event.type == SDL_EVENT_KEY_DOWN && (!io.WantCaptureKeyboard || !io.WantTextInput)) {
        if (!event.key.repeat) {
          auto key = keyFromSdlKeyCode(event.key.key);
          if (!key)
            key = keyFromSdlScancode(event.key.scancode);
          if (key)
            starEvent.emplace(KeyDownEvent{*key, keyModsFromSdlKeyMods(event.key.mod)});
        }
      } else if (event.type == SDL_EVENT_KEY_UP) {
        auto key = keyFromSdlKeyCode(event.key.key);
        if (!key)
          key = keyFromSdlScancode(event.key.scancode);
        if (key)
          starEvent.emplace(KeyUpEvent{*key});
      } else if (event.type == SDL_EVENT_TEXT_INPUT && !io.WantTextInput) {
        starEvent.emplace(TextInputEvent{String(std::string_view(event.text.text))});
      } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
        starEvent.emplace(MouseMoveEvent{
          {event.motion.xrel, -event.motion.yrel},
          {event.motion.x, (int)m_windowSize[1] - event.motion.y}});
      } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !io.WantCaptureMouse) {
        starEvent.emplace(MouseButtonDownEvent{mouseButtonFromSdlMouseButton(event.button.button),
          {event.button.x, (int)m_windowSize[1] - event.button.y}});
      } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && !io.WantCaptureMouse) {
        starEvent.emplace(MouseButtonUpEvent{mouseButtonFromSdlMouseButton(event.button.button),
          {event.button.x, (int)m_windowSize[1] - event.button.y}});
      } else if (event.type == SDL_EVENT_MOUSE_WHEEL && !io.WantCaptureMouse) {
        starEvent.emplace(MouseWheelEvent{event.wheel.y < 0 ? MouseWheel::Down : MouseWheel::Up,
          {event.wheel.mouse_x, (int)m_windowSize[1] - event.wheel.mouse_y}});
      } else if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        starEvent.emplace(ControllerAxisEvent{
          (ControllerId)event.gaxis.which,
          controllerAxisFromSdlControllerAxis(event.gaxis.axis),
          (float)event.gaxis.value / 32768.0f
        });
      } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        starEvent.emplace(ControllerButtonDownEvent{ (ControllerId)event.gbutton.which, controllerButtonFromSdlControllerButton(event.gbutton.button) });
      } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        starEvent.emplace(ControllerButtonUpEvent{ (ControllerId)event.gbutton.which, controllerButtonFromSdlControllerButton(event.gbutton.button) });
      } else if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        auto insertion = m_SdlControllers.insert_or_assign(event.gdevice.which, std::unique_ptr<SDL_Gamepad, decltype(&SDL_CloseGamepad)>(SDL_OpenGamepad(event.gdevice.which), SDL_CloseGamepad));
        if (SDL_Gamepad* controller = insertion.first->second.get())
          if (auto result = m_application->logger().info("Controller device '{}' added", SDL_GetGamepadName(controller)); !result)
            return std::unexpected(std::move(result.error()));
      } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        auto find = m_SdlControllers.find(event.gdevice.which);
        if (find != m_SdlControllers.end()) {
          if (SDL_Gamepad* controller = find->second.get())
            if (auto result = m_application->logger().info("Controller device '{}' removed", SDL_GetGamepadName(controller)); !result)
              return std::unexpected(std::move(result.error()));
          m_SdlControllers.erase(event.gdevice.which);
        }
      } else if (event.type == SDL_EVENT_QUIT) {
        m_quitRequested = true;
        starEvent.reset();
      }

      if (starEvent)
        inputEvents.append(std::move(*starEvent));
    }

    return inputEvents;
  }

  [[nodiscard]] std::expected<void, StarException> getAudioData(std::span<std::int16_t> samples) {
    if (m_audioEnabled)
      return m_application->getAudioData(samples, samples.size() / 2);
    std::ranges::fill(samples, 0);
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> setVSyncEnabled(bool vsyncEnabled) {
    if (vsyncEnabled) {
      // If VSync is requested, try for late swap tearing first, then fall back
      // to regular VSync
      if (auto result = m_application->logger().info("Application: Enabling adaptive VSync"); !result)
        return std::unexpected(std::move(result.error()));
      if (!SDL_GL_SetSwapInterval(-1)) {
        if (auto result = m_application->logger().info("Application: Enabling adaptive VSync, falling back to full VSync"); !result)
          return std::unexpected(std::move(result.error()));
        SDL_GL_SetSwapInterval(1);
      }
    } else {
      if (auto result = m_application->logger().info("Application: Disabling VSync"); !result)
        return std::unexpected(std::move(result.error()));
      SDL_GL_SetSwapInterval(0);
    }
    return {};
  }

  inline static const std::size_t MaxCursorSize = 128;
  [[nodiscard]] std::expected<bool, StarException> setCursorImage(const String& id, const std::shared_ptr<Image const>& image, unsigned scale, const Vec2I& offset) {
    auto imageSize = image->size().piecewiseMultiply(Vec2U::filled(scale));
    if (!m_cursorHardware || !scale || imageSize.max() > MaxCursorSize || imageSize.product() > square(MaxCursorSize)) {
      if (auto defaultCursor = SDL_GetDefaultCursor()) {
        if (SDL_GetCursor() != defaultCursor && !SDL_SetCursor(defaultCursor))
          return std::unexpected(ApplicationException::format("Could not restore default cursor: {}", SDL_GetError()));
      }
      return m_cursorVisible = false;
    }

    auto entryResult = m_cursorCache.get(m_currentCursor = { scale, offset, id }, [&](auto const&) -> std::expected<std::shared_ptr<CursorEntry>, StarException> {
      auto entry = std::make_shared<CursorEntry>();
      List<ImageOperation> operations;
      if (scale != 1)
        operations = {
          FlipImageOperation{ FlipImageOperation::Mode::FlipY }, // SDL wants an Australian cursor.
          BorderImageOperation{ 1, Vec4B(), Vec4B(), false, false }, // Nearest scaling fucks up and clips half off the edges, work around this with border+crop for now.
          ScaleImageOperation{ ScaleImageOperation::Mode::Nearest, Vec2F::filled(scale) },
          CropImageOperation{ RectI::withSize(Vec2I::filled(std::ceilf((float)scale / 2)), Vec2I(imageSize)) }
        };
      else
        operations = { FlipImageOperation{ FlipImageOperation::Mode::FlipY } };

      auto processed = processImageOperations(operations, *image);
      if (!processed)
        return std::unexpected(std::move(processed.error()));
      auto newImage = std::make_shared<Image>(std::move(*processed));
      // Fix fully transparent pixels inverting the underlying display pixel on Windows (allowing this could be made configurable per cursor later!)
      newImage->forEachPixel([](unsigned /*x*/, unsigned /*y*/, Vec4B& pixel) { if (!pixel[3]) pixel[0] = pixel[1] = pixel[2] = 0; });
      entry->image = std::move(newImage);


      auto size = entry->image->size();
      SDL_PixelFormat pixelFormat;
      switch (entry->image->pixelFormat()) {
        case PixelFormat::RGB24: // I know this conversion looks wrong, but it's correct. I'm confused too.
          pixelFormat = SDL_PIXELFORMAT_XBGR8888;
          break;
        case PixelFormat::RGBA32:
          pixelFormat = SDL_PIXELFORMAT_ABGR8888;
          break;
        case PixelFormat::BGR24:
          pixelFormat = SDL_PIXELFORMAT_XRGB8888;
          break;
        case PixelFormat::BGRA32:
          pixelFormat = SDL_PIXELFORMAT_ARGB8888;
          break;
        default:
          pixelFormat = SDL_PIXELFORMAT_UNKNOWN;
      }

      entry->sdlSurface.reset(SDL_CreateSurfaceFrom(
        size[0], size[1],
        pixelFormat,
        const_cast<std::uint8_t*>(entry->image->data().data()),
        entry->image->bytesPerPixel() * size[0])
      );
      if (!entry->sdlSurface)
        return std::unexpected(ApplicationException::format("Could not create cursor surface: {}", SDL_GetError()));
      entry->sdlCursor.reset(SDL_CreateColorCursor(entry->sdlSurface.get(), offset[0] * scale, offset[1] * scale));
      if (!entry->sdlCursor)
        return std::unexpected(ApplicationException::format("Could not create color cursor: {}", SDL_GetError()));

      return entry;
    });

    if (!entryResult)
      return std::unexpected(std::move(entryResult.error()));
    auto& entry = entryResult->get();
    if (!SDL_SetCursor(entry->sdlCursor.get()))
      return std::unexpected(ApplicationException::format("Could not activate color cursor: {}", SDL_GetError()));
    return m_cursorVisible = true;
  }

  bool setClipboardData(StringMap<ByteArray> data) {
    auto heldData = std::make_unique<StringMap<ByteArray>>(std::move(data));
    std::vector<const char*> types;
    for (auto& entry : *heldData)
      types.push_back(entry.first.utf8Ptr());
    auto request = [](void* userdata, const char* mime_type, std::size_t* size) -> const void* {
      if (auto entry = static_cast<StringMap<ByteArray>*>(userdata)->maybeRef(String(std::string_view(mime_type)))) {
        *size = entry->get().size();
        return entry->get().ptr();
      }
      *size = 0;
      return NULL;
    };
    auto cleanup = [](void* userdata) { delete ((StringMap<ByteArray>*)userdata); };
    if (SDL_SetClipboardData(request, cleanup, heldData.get(), types.data(), types.size())) {
      heldData.release();
      return true;
    }
    return false;
  }

  [[nodiscard]] std::expected<bool, StarException> setClipboardImage(Image const& image, std::optional<std::reference_wrapper<ByteArray>> png, std::optional<std::reference_wrapper<String const>> path) {
    #ifdef STAR_SYSTEM_WINDOWS // wow, SDL3's implementation is so bad!!
    return duringClipboard(m_sdlWindow.get(), [&]() -> std::expected<void, StarException> {
      if (path) {
        auto result = copyFileToClipboard(path->get());
        if (!result)
          return std::unexpected(std::move(result.error()));
      }
      if (png)
        copyPngToClipboard(png->get().span());
      auto converted = image.convert(PixelFormat::RGBA32);
      copyDibToClipboard(converted.data(), converted.width(), converted.height());
      return {};
    });
    #else
    ((void)(image));
    ((void)(path));
    if (png) {
      StringMap<ByteArray> clipboardData = {{"image/png", std::move(png->get())}};
      return setClipboardData(std::move(clipboardData));
    }
    return false;
    #endif
  }

  [[nodiscard]] std::expected<bool, StarException> setClipboardFile(String const& path) {
    #ifdef STAR_SYSTEM_WINDOWS
    return duringClipboard(m_sdlWindow.get(), [&]() -> std::expected<void, StarException> {
      auto result = copyFileToClipboard(path);
      if (!result)
        return std::unexpected(std::move(result.error()));
      return {};
    });
    #else
    ((void)(path));
    return false;
    #endif
  }

  SignalHandler m_signalHandler;

  TickRateApproacher m_updateTicker = TickRateApproacher(60.0f, 1.0f);
  float m_updateRate = 0.0f;
  TickRateMonitor m_renderTicker = TickRateMonitor(1.0f);
  float m_renderRate = 0.0f;

  std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> m_sdlWindow{nullptr, SDL_DestroyWindow};
  std::unique_ptr<std::remove_pointer_t<SDL_GLContext>, decltype(&SDL_GL_DestroyContext)> m_sdlGlContext{nullptr, SDL_GL_DestroyContext};
  std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> m_imguiContext{nullptr, ImGui::DestroyContext};
  bool m_imguiSdlInitialized = false;
  bool m_imguiOpenGlInitialized = false;
  bool m_sdlInitialized = false;
  std::unique_ptr<SDL_AudioStream, decltype(&SDL_DestroyAudioStream)> m_sdlAudioOutputStream{nullptr, SDL_DestroyAudioStream};
  std::unique_ptr<SDL_AudioStream, decltype(&SDL_DestroyAudioStream)> m_sdlAudioInputStream{nullptr, SDL_DestroyAudioStream};
  AudioCallback m_audioInputCallback;
  std::vector<std::uint8_t> m_audioInputData;
  std::vector<std::int16_t> m_audioOutputData;
  std::mutex m_audioErrorMutex;
  std::optional<StarException> m_audioError;
  std::optional<std::pair<RectI, int>> m_textInputArea;

  StableHashMap<int, std::unique_ptr<SDL_Gamepad, decltype(&SDL_CloseGamepad)>> m_SdlControllers;

  struct CursorEntry {
    std::shared_ptr<Image const> image = nullptr;
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> sdlSurface;
    std::unique_ptr<SDL_Cursor, decltype(&SDL_DestroyCursor)> sdlCursor;

    CursorEntry() : image(nullptr), sdlSurface(nullptr, SDL_DestroySurface), sdlCursor(nullptr, SDL_DestroyCursor) {};
  };

  typedef std::tuple<unsigned, Vec2I, String> CursorDescriptor;

  // Native cursor-cache expiry jitter belongs to this platform, not gameplay.
  RandomService m_cursorRandom{0};
  HashTtlCache<CursorDescriptor, std::shared_ptr<CursorEntry>> m_cursorCache;
  CursorDescriptor m_currentCursor;

  Vec2U m_windowSize = {800, 600};
  WindowMode m_windowMode = WindowMode::Normal;

  String m_windowTitle = "Starbound";
  bool m_windowVSync = true;
  unsigned m_maxFrameSkip = 5;
  bool m_cursorVisible = true;
  bool m_cursorHardware = true;
  bool m_acceptingTextInput = false;
  bool m_audioEnabled = false;
  bool m_quitRequested = false;
  bool m_applicationStarted = false;
  float m_displayScale = 1.0f;

  std::shared_ptr<OpenGlRenderer> m_renderer;
  std::unique_ptr<Application> m_application;
  std::unique_ptr<PcPlatformServices> m_platformServices;
};

int runMainApplication(std::unique_ptr<Application> application, StringList cmdLineArgs) {
  auto reportError = [](StarException const& error, bool diagnosticReported) {
    auto diagnostic = exceptionSummary(error);
    EmergencyLog::error("{}", diagnosticReported ? diagnostic : exceptionReport(error));
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", diagnostic.c_str(), nullptr);
    return 1;
  };
  bool diagnosticReported = false;
  auto result = [&]() -> std::expected<void, StarException> {
    SdlPlatform platform(std::move(application));
    auto initialized = platform.initialize(std::move(cmdLineArgs));
    if (!initialized) {
      diagnosticReported = platform.logInitializationFailure(initialized.error());
      return std::unexpected(std::move(initialized.error()));
    }
    diagnosticReported = true;
    return platform.run();
  }();
  if (!result)
    return reportError(result.error(), diagnosticReported);
  return 0;
}

}
