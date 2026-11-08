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
#include "imgui_lua_bindings.hpp"
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
import star.random;
import star.ttl_cache;
import star.list;
import star.image_processing;



import star.platform_services_pc;



import star.signal_handler;
import star.tick_rate_monitor;
import star.fixed_scheduler;
import star.main_thread_dispatcher;
import star.threaded_renderer;
import star.main_thread_services;
import star.thread;

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
        platformArguments.push_back(std::move(*it));
        it = cmdLineArgs.erase(it);
      } else {
        ++it;
      }
    }
    if (auto result = m_cursorRandom.init(); !result)
      return std::unexpected(std::move(result.error()));

    auto nativeBase = m_dispatcher.invoke([&]() -> std::expected<void, StarException> {
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
        auto result = File::change_directory(String(std::string_view(basePath)));
        if (!result)
          return std::unexpected(std::move(result.error()));
      }

      return contextScope->restore();
    });
    if (!nativeBase)
      return std::unexpected(std::move(nativeBase.error()));

    if (auto result = m_signalHandler.setHandleInterrupt(true); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_signalHandler.setHandleFatal(true); !result)
      return std::unexpected(std::move(result.error()));

    EmergencyLog::info("Application: startup...");
    if (auto result = m_application->startup(cmdLineArgs); !result)
      return std::unexpected(ApplicationException("Application failed during startup", ExceptionDiagnostic(result.error())));
    m_applicationStarted = true;
    m_logger = &m_application->logger();

    return m_dispatcher.invoke([&] { return initializeNative(applicationPath, platformArguments); });
  }

  [[nodiscard]] std::expected<void, StarException> initializeNative(String const& applicationPath, StringList const& platformArguments) {
    auto contextScope = NativeUiContextScope::create(nullptr, nullptr, nullptr);
    if (!contextScope)
      return std::unexpected(std::move(contextScope.error()));
    m_nativeContextScope.emplace(std::move(*contextScope));

#ifdef STAR_SYSTEM_LINUX
    SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_SCALE_TO_DISPLAY, "1");
#endif
    
    if (auto result = (*m_logger).info("Application: Initializing SDL Video"); !result)
      return std::unexpected(std::move(result.error()));
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
      return std::unexpected(ApplicationException(strf("Couldn't initialize SDL Video: {}", SDL_GetError())));

    if (auto result = (*m_logger).info("Application: using Video Driver '{}'", SDL_GetCurrentVideoDriver()); !result)
      return std::unexpected(std::move(result.error()));

    if (auto result = (*m_logger).info("Application: Initializing SDL Controller"); !result)
      return std::unexpected(std::move(result.error()));
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
      return std::unexpected(ApplicationException(strf("Couldn't initialize SDL Controller: {}", SDL_GetError())));

    if (auto result = (*m_logger).info("Application: Initializing SDL Audio"); !result)
      return std::unexpected(std::move(result.error()));
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
      return std::unexpected(ApplicationException(strf("Couldn't initialize SDL Audio: {}", SDL_GetError())));

    if (auto result = (*m_logger).info("Application: using Audio Driver '{}'", SDL_GetCurrentAudioDriver()); !result)
      return std::unexpected(std::move(result.error()));

    SDL_SetJoystickEventsEnabled(true);

    auto platformServices = PcPlatformServices::create((*m_logger), applicationPath, platformArguments);
    if (!platformServices)
      return std::unexpected(std::move(platformServices.error()));
    m_platformServices = std::move(*platformServices);
    if (m_platformServices) {
      m_mainThreadServices.emplace(MainThreadServices::create(m_dispatcher,
        m_platformServices->statisticsService(), m_platformServices->p2pNetworkingService(),
        m_platformServices->userGeneratedContentService(), m_platformServices->desktopService()));
      m_steamIsFlatpak = m_platformServices->steamIsFlatpak();
    }
    if (!m_platformServices)
      if (auto result = (*m_logger).info("Application: No platform services available"); !result)
        return std::unexpected(std::move(result.error()));

    if (auto result = (*m_logger).info("Application: Creating SDL window"); !result)
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
    auto iconExists = File::is_regular_file(".icon/openstarbound.png");
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
        auto img = Image::readPng((*m_logger), *device);
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
          if (auto result = (*m_logger).warn("Could not create SDL surface for icon '{}': {}", ".icon/openstarbound.png", SDL_GetError()); !result)
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
        iconsDirectoryExists = File::is_directory(XdgIconsPath);
      if (!iconsDirectoryExists)
        return std::unexpected(std::move(iconsDirectoryExists.error()));
      if (*iconsDirectoryExists) {
        auto icon = String::joinWith("", XdgIconsPath, "/openstarbound.png");
        if (!icon)
          return std::unexpected(std::move(icon.error()));
        const String XdgIcon = std::move(*icon);
        auto installed = File::is_regular_file(XdgIcon);
        if (!installed)
          return std::unexpected(std::move(installed.error()));
        if (!*installed) {
          if (auto result = File::copy(".icon/openstarbound.png", XdgIcon); !result)
            return std::unexpected(std::move(result.error()));
          if (auto result = (*m_logger).info("Application: Installed Icon to XDG Icons Directory"); !result)
            return std::unexpected(std::move(result.error()));
        }
      }
    }

    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_APP_ICON_NAME, "openstarbound");
#endif

    if (auto result = (*m_logger).info("Application: Creating SDL OpenGL context"); !result)
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

    if (auto result = (*m_logger).info("Application: Opening audio device"); !result)
      return std::unexpected(std::move(result.error()));
    m_audioOutputData.clear();
    SDL_AudioSpec desired = {.format = SDL_AUDIO_S16, .channels = 2, .freq = 44100};
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
      if (auto result = (*m_logger).error("Application: Could not open audio device, no sound available!"); !result)
        return std::unexpected(std::move(result.error()));
    } else {
      if (auto result = (*m_logger).info("Application: Opened default audio device with 44.1khz / 16 bit stereo audio"); !result)
        return std::unexpected(std::move(result.error()));
      SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(m_sdlAudioOutputStream.get()));
    }

    auto renderer = OpenGlRenderer::create((*m_logger));
    if (!renderer)
      return std::unexpected(std::move(renderer.error()));
    m_glRenderer = std::move(*renderer);
    if (auto result = m_glRenderer->setScreenSize(m_windowSize); !result)
      return std::unexpected(std::move(result.error()));
    m_renderer = std::make_shared<ThreadedRenderer>(m_glRenderer, m_dispatcher);

    m_cursorCache.setTimeToLive(std::chrono::milliseconds(30000));

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
    installImguiPlatformCallbacks();
    m_imguiOpenGlInitialized = ImGui_ImplOpenGL3_Init(nullptr);
    if (!m_imguiOpenGlInitialized)
      return std::unexpected(ApplicationException("Application: Could not initialize ImGui OpenGL backend"));

    m_renderTicker.reset();
    m_focused.store((SDL_GetWindowFlags(m_sdlWindow.get()) & SDL_WINDOW_INPUT_FOCUS) != 0, std::memory_order_relaxed);
    m_pollingEnabled = true;
    return {};
  }

  ~SdlPlatform() {
    if (auto result = releaseResources(); !result)
      EmergencyLog::error("Application: cleanup failed: {}", outputException(ExceptionDiagnostic(result.error()), true));
  }

  using AudioCallback = ApplicationController::AudioCallback;
  [[nodiscard]] std::expected<bool, StarException> openAudioInputDevice(SDL_AudioDeviceID deviceId, int freq, int channels, AudioCallback callback) {
    closeAudioInputDevice();
    if (!callback) {
      if (auto result = (*m_logger).error("Cannot open audio input with an empty callback"); !result)
        return std::unexpected(std::move(result.error()));
      return false;
    }
    m_audioInputCallback = std::move(callback);
    SDL_AudioSpec desired = {.format = SDL_AUDIO_S16, .channels = channels, .freq = freq};
    m_sdlAudioInputStream.reset(SDL_OpenAudioDeviceStream(deviceId, &desired,
    [](void* userdata, SDL_AudioStream* stream, int len, int) {
      if (len > 0) {
        auto& sdlPlatform = *static_cast<SdlPlatform*>(userdata);
        sdlPlatform.m_audioInputData.resize(len);
        int received = SDL_GetAudioStreamData(stream, sdlPlatform.m_audioInputData.data(), len);
        if (received > 0)
          sdlPlatform.m_audioInputCallback(std::span(sdlPlatform.m_audioInputData).first(received));
        else if (received < 0)
          if (auto result = sdlPlatform.m_logger->error("Could not read audio input stream: {}", SDL_GetError()); !result)
            sdlPlatform.recordAudioError(std::move(result.error()));
      }
    }, this));

    if (m_sdlAudioInputStream) {
      if (auto result = (*m_logger).info("Opened audio input device '{}'", SDL_GetAudioDeviceName(SDL_GetAudioStreamDevice(m_sdlAudioInputStream.get()))); !result) {
        closeAudioInputDevice();
        return std::unexpected(std::move(result.error()));
      }
      SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(m_sdlAudioInputStream.get()));
    }
    else {
      m_audioInputCallback = {};
      if (auto result = (*m_logger).info("Failed to open audio input device: {}", SDL_GetError()); !result)
        return std::unexpected(std::move(result.error()));
    }

    return static_cast<bool>(m_sdlAudioInputStream);
  }

  bool closeAudioInputDevice() {
    if (m_sdlAudioInputStream) {
      if (auto result = (*m_logger).info("Closing audio input device"); !result)
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
    if (auto result = m_cursorCache.find_ref(m_currentCursor); !result)
      return std::unexpected(std::move(result.error()));
    m_cursorCache.cleanup();
    return {};
  }

  bool failureReported() const { return m_failureReported; }

  [[nodiscard]] std::expected<void, StarException> run(StringList cmdLineArgs) {
    auto worker = Thread::invoke("ClientLogic", [this, arguments = std::move(cmdLineArgs)]() mutable -> std::expected<void, StarException> {
      auto result = initialize(std::move(arguments));
      if (result)
        result = runOwner();
      // Cleanup participates in the same owned error channel; log only once
      // after all native owners that borrow the Root logger have been joined.

      // Detach and join audio callbacks before the mixer/application disappear.
      m_dispatcher.invoke([&] {
        m_pollingEnabled = false;
        closeAudioOutputDevice();
        closeAudioInputDevice();
      });
      if (auto error = takeAudioError(); error && result)
        result = std::unexpected(std::move(*error));
      if (m_applicationStarted) {
        if (auto stopped = m_application->shutdown(); !stopped && result)
          result = std::unexpected(std::move(stopped.error()));
      }
      // Application destruction releases its last frontend handles on this
      // owner. Drain those retirements while main still pumps native calls.
      if (m_renderer) {
        if (auto drained = m_renderer->drain(); !drained && result)
          result = std::unexpected(std::move(drained.error()));
      }
      // SDK callbacks and GL diagnostics borrow the Root logger. Join/release
      // their native owners while the worker is paused and Root is still alive.
      auto nativeReleased = m_dispatcher.invoke([&]() -> std::expected<void, StarException> {
        m_mainThreadServices.reset();
        m_platformServices.reset();
        auto released = m_renderer ? m_renderer->releaseResources() : std::expected<void, StarException>{};
        m_glRenderer.reset();
        return released;
      });
      if (!nativeReleased && result)
        result = std::unexpected(std::move(nativeReleased.error()));
      if (!result && m_logger) {
        if (auto logged = m_logger->errorException("Application: failed", ExceptionDiagnostic(result.error())); !logged) {
          EmergencyLog::error("{}", exceptionReport(result.error()));
          EmergencyLog::error("Application: failure logging failed: {}", exceptionReport(logged.error()));
        }
        m_failureReported = true;
      }
      m_application.reset();
      m_logger = nullptr;
      if (m_renderer) {
        if (auto drained = m_renderer->drain(); !drained && result)
          result = std::unexpected(std::move(drained.error()));
      }
      m_ownerDone.store(true, std::memory_order_release);
      m_dispatcher.wake();
      return result;
    });
    if (!worker)
      return std::unexpected(std::move(worker.error()));

    while (!m_ownerDone.load(std::memory_order_acquire)) {
      auto observed = m_dispatcher.generation();
      if (m_pollingEnabled)
        pollNativeEvents();
      // A worker may already have queued a getter for N+1. Consume N before
      // draining it, so partial command flushes cannot overtake this frame.
      consumePendingFrame();
      {
        // Publication and native dispatch share only this short boundary, not
        // a world/update lock. A frame published after our first check wins
        // over any N+1 synchronous getter queued immediately afterward.
        std::lock_guard boundary(m_pipelineMutex);
        if (!m_pendingFrame)
          m_dispatcher.drain();
      }
      m_dispatcher.wait(observed, std::chrono::milliseconds(2));
    }
    consumePendingFrame();
    m_dispatcher.drain();
    auto result = worker->finish();
    m_renderer.reset();
    if (auto released = releaseResources(); !released) {
      if (result)
        result = std::unexpected(std::move(released.error()));
      else
        EmergencyLog::error("Application: native cleanup failed: {}", exceptionReport(released.error()));
    }
    return result;
  }

private:
  struct ImguiPlatformCallbacks {
    decltype(ImGuiPlatformIO::Platform_GetClipboardTextFn) getClipboardText = nullptr;
    decltype(ImGuiPlatformIO::Platform_SetClipboardTextFn) setClipboardText = nullptr;
    decltype(ImGuiPlatformIO::Platform_SetImeDataFn) setImeData = nullptr;
    decltype(ImGuiPlatformIO::Platform_OpenInShellFn) openInShell = nullptr;
  };

  // SDL owns BackendPlatformUserData and callers may own every public UserData
  // slot. Keep our context association separately instead of replacing either.
  static SdlPlatform& imguiPlatform(ImGuiContext* context) {
    std::lock_guard lock(s_imguiPlatformMutex);
    auto* platform = s_imguiPlatforms;
    while (platform && platform->m_imguiCallbackContext != context)
      platform = platform->m_nextImguiPlatform;
    IM_ASSERT(platform != nullptr && "ImGui platform callback outlived its owner");
    return *platform;
  }

  template <typename Function>
  auto dispatchImguiCallback(ImGuiContext* context, Function&& function) -> std::invoke_result_t<Function&> {
    return m_dispatcher.invoke([&]() -> std::invoke_result_t<Function&> {
      // The core caller is synchronously paused. SDL's callbacks use the current
      // context's backend data rather than their ctx argument, so preserve both.
      // This scope deliberately does not inspect or bind an OpenGL context.
      struct ContextScope {
        ImGuiContext* previous;
        explicit ContextScope(ImGuiContext* target) : previous(ImGui::GetCurrentContext()) {
          if (previous != target)
            ImGui::SetCurrentContext(target);
        }
        ~ContextScope() {
          if (ImGui::GetCurrentContext() != previous)
            ImGui::SetCurrentContext(previous);
        }
      } scope(context);
      return std::invoke(function);
    });
  }

  void installImguiPlatformCallbacks() {
    auto& platformIo = ImGui::GetPlatformIO();
    m_originalImguiCallbacks = {platformIo.Platform_GetClipboardTextFn, platformIo.Platform_SetClipboardTextFn,
      platformIo.Platform_SetImeDataFn, platformIo.Platform_OpenInShellFn};
    {
      std::lock_guard lock(s_imguiPlatformMutex);
      m_imguiCallbackContext = m_imguiContext.get();
      m_nextImguiPlatform = s_imguiPlatforms;
      s_imguiPlatforms = this;
    }
    if (m_originalImguiCallbacks.getClipboardText)
      platformIo.Platform_GetClipboardTextFn = [](ImGuiContext* context) -> const char* {
        auto& platform = imguiPlatform(context);
        return platform.dispatchImguiCallback(context, [&] { return platform.m_originalImguiCallbacks.getClipboardText(context); });
      };
    if (m_originalImguiCallbacks.setClipboardText)
      platformIo.Platform_SetClipboardTextFn = [](ImGuiContext* context, const char* text) {
        auto& platform = imguiPlatform(context);
        platform.dispatchImguiCallback(context, [&] { platform.m_originalImguiCallbacks.setClipboardText(context, text); });
      };
    if (m_originalImguiCallbacks.setImeData)
      platformIo.Platform_SetImeDataFn = [](ImGuiContext* context, ImGuiViewport* viewport, ImGuiPlatformImeData* data) {
        auto& platform = imguiPlatform(context);
        platform.dispatchImguiCallback(context, [&] { platform.m_originalImguiCallbacks.setImeData(context, viewport, data); });
      };
    if (m_originalImguiCallbacks.openInShell)
      platformIo.Platform_OpenInShellFn = [](ImGuiContext* context, const char* path) {
        auto& platform = imguiPlatform(context);
        return platform.dispatchImguiCallback(context, [&] { return platform.m_originalImguiCallbacks.openInShell(context, path); });
      };
  }

  void uninstallImguiPlatformCallbacks() {
    if (!m_imguiCallbackContext)
      return;
    // The owner has joined. Restore SDL's callbacks before Shutdown clears the
    // platform handlers and destroys their backend data/clipboard allocation.
    auto& platformIo = ImGui::GetPlatformIO();
    platformIo.Platform_GetClipboardTextFn = m_originalImguiCallbacks.getClipboardText;
    platformIo.Platform_SetClipboardTextFn = m_originalImguiCallbacks.setClipboardText;
    platformIo.Platform_SetImeDataFn = m_originalImguiCallbacks.setImeData;
    platformIo.Platform_OpenInShellFn = m_originalImguiCallbacks.openInShell;
    {
      std::lock_guard lock(s_imguiPlatformMutex);
      auto** link = &s_imguiPlatforms;
      while (*link != this) {
        IM_ASSERT(*link != nullptr);
        link = &(*link)->m_nextImguiPlatform;
      }
      *link = m_nextImguiPlatform;
      m_imguiCallbackContext = nullptr;
      m_nextImguiPlatform = nullptr;
    }
    m_originalImguiCallbacks = {};
  }

  struct PresentedFrame {
    ThreadedRenderer::FrameTicket ticket;
    ImDrawData* drawData = nullptr;
    Application::FrameTimings timings;
    std::chrono::time_point<std::chrono::steady_clock, std::chrono::microseconds> started{};
    bool captureKeyboard = false;
    bool captureText = false;
    bool captureMouse = false;
  };

  void consumePendingFrame() {
    std::optional<PresentedFrame> frame;
    {
      std::lock_guard lock(m_pipelineMutex);
      if (!m_pendingFrame)
        return;
      frame = std::move(m_pendingFrame);
      m_pendingFrame.reset();
    }
    auto finishStart = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
    auto result = m_renderer->consumeFrame(frame->ticket);
    auto guiStart = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
    if (result)
      ImGui_ImplOpenGL3_RenderDrawData(frame->drawData);
    auto presentStart = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
    if (result && !SDL_GL_SwapWindow(m_sdlWindow.get()))
      result = std::unexpected(ApplicationException::format("Application: Could not present OpenGL frame: {}", SDL_GetError()));
    auto presentEnd = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
    if (result) {
      m_captureKeyboard = frame->captureKeyboard;
      m_captureText = frame->captureText;
      m_captureMouse = frame->captureMouse;
    }
    frame->timings.finish += (guiStart - finishStart).count();
    frame->timings.gui += (presentStart - guiStart).count();
    frame->timings.present = (presentEnd - presentStart).count();
    frame->timings.total = (presentEnd - frame->started).count();
    float rate = result ? m_renderTicker.tick() : 0.0f;
    {
      std::lock_guard lock(m_pipelineMutex);
      m_completedTimings = frame->timings;
      m_completedRenderRate = rate;
      if (!result)
        m_pipelineError.emplace(std::move(result.error()));
      m_frameCompleted = true;
    }
    m_dispatcher.wake();
  }

  [[nodiscard]] std::expected<void, StarException> acknowledgeFrame(bool wait) {
    while (m_ownerFrame) {
      auto observed = m_dispatcher.generation();
      bool completed;
      std::optional<StarException> error;
      {
        std::lock_guard lock(m_pipelineMutex);
        completed = m_frameCompleted;
        if (completed) {
          m_frameCompleted = false;
          m_renderRate = m_completedRenderRate;
          m_application->recordFrameTimings(m_completedTimings);
          error = std::move(m_pipelineError);
          m_pipelineError.reset();
        }
      }
      if (!completed) {
        if (!wait)
          return {};
        m_dispatcher.wait(observed, std::chrono::milliseconds(10));
        continue;
      }
      auto ticket = *m_ownerFrame;
      m_ownerFrame.reset();
      auto retired = m_renderer->retire(ticket);
      if (error)
        return std::unexpected(std::move(*error));
      if (!retired)
        return std::unexpected(std::move(retired.error()));
      if (auto presented = m_application->framePresented(); !presented)
        return std::unexpected(std::move(presented.error()));
    }
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> runOwner() {
    bool imguiFrameActive = false;
    auto result = [&]() -> std::expected<void, StarException> {
      if (auto initialized = m_application->applicationInit(std::make_shared<Controller>(*this)); !initialized)
        return std::unexpected(std::move(initialized.error()));
      if (auto initialized = m_application->renderInit(m_renderer); !initialized)
        return std::unexpected(std::move(initialized.error()));
      m_updateTicker.reset();
      m_scheduler.reset(std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500)));
      while (!m_quitRequested) {
        auto observed = m_dispatcher.generation();
        auto frameStart = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
        if (auto acknowledged = acknowledgeFrame(false); !acknowledged)
          return std::unexpected(std::move(acknowledged.error()));
        if (auto error = takeAudioError())
          return std::unexpected(std::move(*error));
        if (auto events = processOwnerEvents(); !events)
          return std::unexpected(std::move(events.error()));
        auto updateStart = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
        unsigned updatesDue = m_scheduler.updatesDue(updateStart, m_maxFrameSkip);
        auto epoch = m_scheduler.updateEpoch();
        unsigned updateCount = 0;
        for (; updateCount < updatesDue; ++updateCount) {
          if (auto updated = m_application->update(); !updated)
            return std::unexpected(std::move(updated.error()));
          m_updateRate = m_updateTicker.tick();
          if (m_scheduler.updateEpoch() != epoch) {
            ++updateCount;
            break;
          }
        }
        // Void native state is coalesced by the main event pump.
        auto renderStart = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
        bool prepared = false;
        if (!m_ownerFrame && !m_quitRequested && m_scheduler.beginRender(renderStart)) {
          m_renderInterpolation = m_scheduler.interpolation(renderStart);
          // Only backend setup touches SDL/GL. Main owns IO until this call
          // returns; afterward it does not touch ImGui until consuming us.
          m_dispatcher.invoke([&] {
            processImguiEvents();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
          });
          ImGui::NewFrame();
          imguiFrameActive = true;
          if (auto started = m_renderer->startFrame(); !started)
            return std::unexpected(std::move(started.error()));
          setImguiPresentationActive(true);
          auto rendered = m_application->render();
          setImguiPresentationActive(false);
          if (!rendered)
            return std::unexpected(std::move(rendered.error()));
          auto finishStart = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
          if (auto finished = m_renderer->finishFrame(); !finished)
            return std::unexpected(std::move(finished.error()));
          auto guiStart = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
          ImGui::Render();
          imguiFrameActive = false;
          auto& io = ImGui::GetIO();
          auto ticket = m_renderer->frameTicket();
          m_ownerFrame = ticket;
          PresentedFrame frame{ticket, ImGui::GetDrawData(),
            {(updateStart - frameStart).count(), (renderStart - updateStart).count(), (finishStart - renderStart).count(),
             (guiStart - finishStart).count(), (std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500)) - guiStart).count(), 0, 0, 0, updateCount},
            frameStart, io.WantCaptureKeyboard, io.WantTextInput, io.WantCaptureMouse};
          {
            std::lock_guard lock(m_pipelineMutex);
            m_pendingFrame.emplace(std::move(frame));
          }
          m_dispatcher.wake();
          prepared = true;
        }
        if (m_signalHandler.interruptCaught())
          m_quitRequested = true;
        auto throttleStart = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
        if (!m_quitRequested) {
          auto sleep = m_ownerFrame ? m_scheduler.updateSleepTime(throttleStart) : m_scheduler.sleepTime(throttleStart);
          if (sleep > std::chrono::duration<double>::zero())
            m_dispatcher.wait(observed, sleep);
        }
        if (!prepared) {
          auto end = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
          m_application->recordFrameTimings({(updateStart - frameStart).count(), (renderStart - updateStart).count(),
            0, 0, 0, 0, (end - throttleStart).count(), (end - frameStart).count(), updateCount});
        }
      }
      return {};
    }();
    setImguiPresentationActive(false);
    if (imguiFrameActive)
      ImGui::EndFrame();
    if (auto acknowledged = acknowledgeFrame(true); !acknowledged && result)
      result = std::unexpected(std::move(acknowledged.error()));
    return result;
  }

private:
  void recordAudioError(StarException error) {
    std::lock_guard lock(m_audioErrorMutex);
    if (!m_audioError)
      m_audioError.emplace(std::move(error));
    else
      if (auto logged = (*m_logger).errorException("Application: audio callback failed", ExceptionDiagnostic(error)); !logged) {
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
    closeAudioOutputDevice();
    closeAudioInputDevice();
    m_mainThreadServices.reset();
    m_platformServices.reset();
    m_renderer.reset();
    m_glRenderer.reset();
    if (std::exchange(m_imguiOpenGlInitialized, false))
      ImGui_ImplOpenGL3_Shutdown();
    if (std::exchange(m_imguiSdlInitialized, false)) {
      uninstallImguiPlatformCallbacks();
      ImGui_ImplSDL3_Shutdown();
    }
    if (m_imguiContext) {
      if (contextScope)
        contextScope->discardImguiContext(m_imguiContext.get());
      if (m_nativeContextScope)
        m_nativeContextScope->discardImguiContext(m_imguiContext.get());
      m_imguiContext.reset();
    }
    m_SdlControllers.clear();
    SDL_SetCursor(nullptr);
    m_cursorCache.clear();
    if (m_sdlGlContext) {
      if (contextScope)
        contextScope->discardGlContext(m_sdlGlContext.get());
      if (m_nativeContextScope)
        m_nativeContextScope->discardGlContext(m_sdlGlContext.get());
      m_sdlGlContext.reset();
    }
    m_sdlWindow.reset();
    std::expected<void, StarException> result;
    if (contextScope)
      result = contextScope->restore();
    else
      result = std::unexpected(std::move(contextScope.error()));
    if (m_nativeContextScope) {
      if (auto restored = m_nativeContextScope->restore(); !restored && result)
        result = std::unexpected(std::move(restored.error()));
      m_nativeContextScope.reset();
    }
    if (std::exchange(m_sdlInitialized, false))
      SDL_Quit();
    return result;
  }

  struct Controller : public ApplicationController {
    Controller(SdlPlatform& parent)
      : parent(parent) {}

    bool hasClipboard() override {
      return parent.m_dispatcher.invoke([&]() -> bool {
        return SDL_HasClipboardText();
      });
    }

    std::optional<String> getClipboard() override {
      return parent.m_dispatcher.invoke([&]() -> std::optional<String> {
        std::optional<String> string;
        if (SDL_HasClipboardText()) {
          if (auto text = SDL_GetClipboardText()) {
            if (*text != '\0')
              string.emplace(std::string_view(text));
            SDL_free(text);
          }
        }
        return string;
      });
    }

    bool setClipboard(String text) override {
      return parent.m_dispatcher.invoke([&]() -> bool {
        return SDL_SetClipboardText(text.utf8Ptr());
      });
    }

    bool setClipboardData(StringMap<ByteArray> data) override {
      return parent.m_dispatcher.invoke([&]() -> bool {
        return parent.setClipboardData(std::move(data));
      });
    }

    [[nodiscard]] std::expected<bool, StarException> setClipboardImage(Image const& image, std::optional<std::reference_wrapper<ByteArray>> png, std::optional<std::reference_wrapper<String const>> path) override {
      return parent.m_dispatcher.invoke([&]() -> std::expected<bool, StarException> {
        return parent.setClipboardImage(image, png, path);
      });
    }

    [[nodiscard]] std::expected<bool, StarException> setClipboardFile(String const& path) override {
      return parent.m_dispatcher.invoke([&]() -> std::expected<bool, StarException> {
        return parent.setClipboardFile(path);
      });
    }

    bool isFocused() const override {
      return parent.m_focused.load(std::memory_order_relaxed);
    }

    void setTargetUpdateRate(float targetUpdateRate) override {
      parent.m_scheduler.setUpdateRate(targetUpdateRate, std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500)));
    }

    void setTargetRenderRate(float rate) override {
      parent.m_scheduler.setRenderRate(rate, std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500)));
    }

    float renderInterpolation() const override {
      return parent.m_renderInterpolation;
    }

    std::uint64_t renderEpoch() const override {
      return parent.m_scheduler.epoch();
    }

    void setApplicationTitle(String title) override {
      parent.m_dispatcher.invoke([&]() {
        parent.m_windowTitle = std::move(title);
        if (parent.m_sdlWindow)
          SDL_SetWindowTitle(parent.m_sdlWindow.get(), parent.m_windowTitle.utf8Ptr());
      });
    }

    [[nodiscard]] std::expected<void, StarException> setFullscreenWindow(Vec2U fullScreenResolution) override {
      std::optional<std::pair<WindowMode, Vec2U>> changed;
      auto result = parent.m_dispatcher.invoke([&]() -> std::expected<void, StarException> {
        if (parent.m_windowMode != WindowMode::Fullscreen || parent.m_windowSize != fullScreenResolution) {
          int currentDisplayIndex = SDL_GetDisplayForWindow(parent.m_sdlWindow.get());

          SDL_DisplayMode closestDisplayMode;
          if (SDL_GetClosestFullscreenDisplayMode(currentDisplayIndex, static_cast<int>(fullScreenResolution[0]),
                                                  static_cast<int>(fullScreenResolution[1]), 0.f, true,
                                                  &closestDisplayMode)) {
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
              if (auto result = (*parent.m_logger)
                                  .warn("Failed to set resolution {}, {}", static_cast<unsigned>(closestDisplayMode.w),
                                        static_cast<unsigned>(closestDisplayMode.h));
                  !result)
                return std::unexpected(std::move(result.error()));
            }
          } else {
            if (auto result =
                  (*parent.m_logger)
                    .warn("Unable to set requested display resolution {}, {}",
                          static_cast<int>(fullScreenResolution[0]), static_cast<int>(fullScreenResolution[1]));
                !result)
              return std::unexpected(std::move(result.error()));
          }

          if (auto displayMode = SDL_GetWindowFullscreenMode(parent.m_sdlWindow.get())) {
            parent.m_windowSize = {static_cast<unsigned>(displayMode->w), static_cast<unsigned>(displayMode->h)};

            // call these manually since no SDL_WindowEvent is triggered when changing between fullscreen resolutions for some reason
            changed = std::pair{parent.m_windowMode, parent.m_windowSize};
          } else {
            if (auto result = (*parent.m_logger).error("Couldn't get window display mode!"); !result)
              return std::unexpected(std::move(result.error()));
          }
        }
        return {};
      });
      if (!result)
        return std::unexpected(std::move(result.error()));
      if (changed) {
        if (auto resized = parent.m_renderer->setScreenSize(changed->second); !resized)
          return std::unexpected(std::move(resized.error()));
        return parent.m_application->windowChanged(changed->first, changed->second);
      }
      return {};
    }

    void setNormalWindow(Vec2U windowSize) override {
      parent.m_dispatcher.invoke([&]() {
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
      });
    }

    void setMaximizedWindow() override {
      parent.m_dispatcher.invoke([&]() {
        if (parent.m_windowMode != WindowMode::Maximized) {
          if (parent.m_windowMode == WindowMode::Fullscreen)
            SDL_SetWindowFullscreen(parent.m_sdlWindow.get(), 0);
          else if (parent.m_windowMode == WindowMode::Borderless)
            SDL_SetWindowBordered(parent.m_sdlWindow.get(), true);

          SDL_RestoreWindow(parent.m_sdlWindow.get());
          SDL_MaximizeWindow(parent.m_sdlWindow.get());
          parent.m_windowMode = WindowMode::Maximized;
        }
      });
    }

    [[nodiscard]] std::expected<void, StarException> setBorderlessWindow() override {
      std::optional<std::pair<WindowMode, Vec2U>> changed;
      auto result = parent.m_dispatcher.invoke([&]() -> std::expected<void, StarException> {
        if (parent.m_windowMode != WindowMode::Borderless) {
          if (parent.m_windowMode == WindowMode::Fullscreen)
            SDL_SetWindowFullscreen(parent.m_sdlWindow.get(), 0);
          else if (parent.m_windowMode == WindowMode::Maximized)
            SDL_RestoreWindow(parent.m_sdlWindow.get());

          SDL_SetWindowBordered(parent.m_sdlWindow.get(), false);
          parent.m_windowMode = WindowMode::Borderless;

          if (auto displayMode = SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(parent.m_sdlWindow.get()))) {
            parent.m_windowSize = {static_cast<unsigned>(displayMode->w), static_cast<unsigned>(displayMode->h)};
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
            changed = std::pair{parent.m_windowMode, parent.m_windowSize};
          } else {
            if (auto result = (*parent.m_logger).error("Couldn't get desktop display mode!"); !result)
              return std::unexpected(std::move(result.error()));
          }
        }
        return {};
      });
      if (!result)
        return std::unexpected(std::move(result.error()));
      if (changed) {
        if (auto resized = parent.m_renderer->setScreenSize(changed->second); !resized)
          return std::unexpected(std::move(resized.error()));
        return parent.m_application->windowChanged(changed->first, changed->second);
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
      return parent.m_dispatcher.invoke([&]() -> std::expected<void, StarException> {
        if (parent.m_windowVSync != vSync) {
          if (auto result = parent.setVSyncEnabled(vSync); !result)
            return std::unexpected(std::move(result.error()));
          parent.m_windowVSync = vSync;
        }
        return {};
      });
    }

    void setMaxFrameSkip(unsigned maxFrameSkip) override {
      parent.m_maxFrameSkip = maxFrameSkip;
    }

    void setCursorVisible(bool cursorVisible) override {
      if (m_requestedCursorVisible == cursorVisible)
        return;
      m_requestedCursorVisible = cursorVisible;
      m_cursorDescriptor.reset();
      {
        std::lock_guard lock(parent.m_eventMutex);
        parent.m_nativeState.cursorVisible = cursorVisible;
      }
      parent.m_dispatcher.wake();
    }

    void setCursorPosition(Vec2I cursorPosition) override {
      {
        std::lock_guard lock(parent.m_eventMutex);
        parent.m_nativeState.cursorPosition = cursorPosition;
      }
      parent.m_dispatcher.wake();
    }

    void setCursorHardware(bool hardware) override {
      if (m_requestedHardware == hardware)
        return;
      m_requestedHardware = hardware;
      m_cursorDescriptor.reset();
      {
        std::lock_guard lock(parent.m_eventMutex);
        parent.m_nativeState.cursorHardware = hardware;
      }
      parent.m_dispatcher.wake();
    }

    [[nodiscard]] std::expected<bool, StarException> setCursorImage(const String& id, const std::shared_ptr<Image const>& image, unsigned scale, const Vec2I& offset) override {
      if (m_cursorDescriptor && *m_cursorDescriptor == std::tie(scale, offset, id))
        return m_cursorResult;
      auto result = parent.m_dispatcher.invoke([&]() {
        parent.applyNativeState();
        return parent.setCursorImage(id, image, scale, offset);
      });
      if (result) {
        m_cursorDescriptor.emplace(scale, offset, id);
        m_cursorResult = *result;
        m_requestedCursorVisible = *result;
      }
      return result;
    }

    void setAcceptingTextInput(bool acceptingTextInput) override {
      if (m_requestedTextInput == acceptingTextInput)
        return;
      m_requestedTextInput = acceptingTextInput;
      {
        std::lock_guard lock(parent.m_eventMutex);
        parent.m_nativeState.acceptingTextInput = acceptingTextInput;
      }
      parent.m_dispatcher.wake();
    }

    void setTextArea(std::optional<std::pair<RectI, int>> area) override {
      if (m_textAreaKnown && m_requestedTextArea == area)
        return;
      m_textAreaKnown = true;
      m_requestedTextArea = area;
      {
        std::lock_guard lock(parent.m_eventMutex);
        parent.m_nativeState.textInputArea.emplace(std::move(area));
      }
      parent.m_dispatcher.wake();
    }

    AudioFormat enableAudio() override {
      parent.m_audioEnabled.store(true, std::memory_order_release);
      parent.m_dispatcher.invoke([&] {
        if (parent.m_sdlAudioOutputStream)
          SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(parent.m_sdlAudioOutputStream.get()));
      });
      return AudioFormat{44100, 2};
    }

    void disableAudio() override {
      parent.m_audioEnabled.store(false, std::memory_order_release);
      parent.m_dispatcher.invoke([&] {
        if (parent.m_sdlAudioOutputStream)
          SDL_PauseAudioDevice(SDL_GetAudioStreamDevice(parent.m_sdlAudioOutputStream.get()));
      });
    }

    [[nodiscard]] std::expected<bool, StarException> openAudioInputDevice(std::uint32_t deviceId, int freq, int channels, AudioCallback callback) override {
      return parent.m_dispatcher.invoke([&]() -> std::expected<bool, StarException> {
        return parent.openAudioInputDevice(deviceId, freq, channels, callback);
      });
    };

    bool closeAudioInputDevice() override {
      return parent.m_dispatcher.invoke([&]() -> bool {
        return parent.closeAudioInputDevice();
      });
    };

    float updateRate() const override {
      return parent.m_updateRate;
    }

    float renderFps() const override {
      return parent.m_renderRate;
    }

  float getDisplayScale() const override {
    return parent.m_displayScale.load(std::memory_order_relaxed);
  }

    std::shared_ptr<StatisticsService> statisticsService() const override {
      return parent.m_mainThreadServices ? parent.m_mainThreadServices->statistics : nullptr;
    }

    std::shared_ptr<P2PNetworkingService> p2pNetworkingService() const override {
      return parent.m_mainThreadServices ? parent.m_mainThreadServices->p2p : nullptr;
    }

    std::shared_ptr<UserGeneratedContentService> userGeneratedContentService() const override {
      return parent.m_mainThreadServices ? parent.m_mainThreadServices->ugc : nullptr;
    }

    std::shared_ptr<DesktopService> desktopService() const override {
      return parent.m_mainThreadServices ? parent.m_mainThreadServices->desktop : nullptr;
    }

    bool steamIsFlatpak() const override {
      return parent.m_steamIsFlatpak;
    }

    void quit() override {
      parent.m_quitRequested = true;
    }

    std::optional<std::tuple<unsigned, Vec2I, String>> m_cursorDescriptor;
    bool m_cursorResult = false;
    bool m_requestedHardware = true;
    std::optional<bool> m_requestedCursorVisible;
    bool m_requestedTextInput = false;
    bool m_textAreaKnown = false;
    std::optional<std::pair<RectI, int>> m_requestedTextArea;
    SdlPlatform& parent;
  };

  void pollNativeEvents() {
    applyNativeState();
    if (m_platformServices)
      if (auto updated = m_platformServices->update(); !updated)
        recordNativeError(std::move(updated.error()));
    m_focused.store((SDL_GetWindowFlags(m_sdlWindow.get()) & SDL_WINDOW_INPUT_FOCUS) != 0, std::memory_order_relaxed);
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      SDL_ConvertEventToRenderCoordinates(SDL_GetRenderer(m_sdlWindow.get()), &event);
      queueImguiEvent(event);
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

        queueWindowChanged();
      } else if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        m_windowSize = Vec2U(event.window.data1, event.window.data2);
        queueWindowChanged();
      }
#ifndef STAR_SYSTEM_MACOS
      else if (event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
        m_displayScale = SDL_GetWindowDisplayScale(m_sdlWindow.get());
      }
#endif
      else if (event.type == SDL_EVENT_KEY_DOWN && (!m_captureKeyboard || !m_captureText)) {
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
      } else if (event.type == SDL_EVENT_TEXT_INPUT && !m_captureText) {
        starEvent.emplace(TextInputEvent{String(std::string_view(event.text.text))});
      } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
        starEvent.emplace(MouseMoveEvent{{event.motion.xrel, -event.motion.yrel},
                                         {event.motion.x, static_cast<int>(m_windowSize[1]) - event.motion.y}});
      } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !m_captureMouse) {
        starEvent.emplace(MouseButtonDownEvent{mouseButtonFromSdlMouseButton(event.button.button),
                                               {event.button.x, static_cast<int>(m_windowSize[1]) - event.button.y}});
      } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && !m_captureMouse) {
        starEvent.emplace(MouseButtonUpEvent{mouseButtonFromSdlMouseButton(event.button.button),
                                             {event.button.x, static_cast<int>(m_windowSize[1]) - event.button.y}});
      } else if (event.type == SDL_EVENT_MOUSE_WHEEL && !m_captureMouse) {
        starEvent.emplace(
          MouseWheelEvent{event.wheel.y < 0 ? MouseWheel::Down : MouseWheel::Up,
                          {event.wheel.mouse_x, static_cast<int>(m_windowSize[1]) - event.wheel.mouse_y}});
      } else if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        starEvent.emplace(ControllerAxisEvent{static_cast<ControllerId>(event.gaxis.which),
                                              controllerAxisFromSdlControllerAxis(event.gaxis.axis),
                                              static_cast<float>(event.gaxis.value) / 32768.0f});
      } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        starEvent.emplace(ControllerButtonDownEvent{static_cast<ControllerId>(event.gbutton.which),
                                                    controllerButtonFromSdlControllerButton(event.gbutton.button)});
      } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        starEvent.emplace(ControllerButtonUpEvent{static_cast<ControllerId>(event.gbutton.which),
                                                  controllerButtonFromSdlControllerButton(event.gbutton.button)});
      } else if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        auto insertion = m_SdlControllers.insert_or_assign(event.gdevice.which, std::unique_ptr<SDL_Gamepad, decltype(&SDL_CloseGamepad)>(SDL_OpenGamepad(event.gdevice.which), SDL_CloseGamepad));
        if (SDL_Gamepad* controller = insertion.first->second.get())
          if (auto result = (*m_logger).info("Controller device '{}' added", SDL_GetGamepadName(controller)); !result)
            recordNativeError(std::move(result.error()));
      } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        auto find = m_SdlControllers.find(event.gdevice.which);
        if (find != m_SdlControllers.end()) {
          if (SDL_Gamepad* controller = find->second.get())
            if (auto result = (*m_logger).info("Controller device '{}' removed", SDL_GetGamepadName(controller)); !result)
              recordNativeError(std::move(result.error()));
          m_SdlControllers.erase(event.gdevice.which);
        }
      } else if (event.type == SDL_EVENT_QUIT) {
        std::lock_guard lock(m_eventMutex);
        m_ownerEvents.emplace_back(OwnerEvent{.quit = true});
        starEvent.reset();
      }

      if (starEvent) {
        std::lock_guard lock(m_eventMutex);
        m_ownerEvents.emplace_back(OwnerEvent{.input = std::move(starEvent)});
      }
    }

    if (m_cursorVisible || (m_platformServices && m_platformServices->overlayActive()))
      SDL_ShowCursor();
    else
      SDL_HideCursor();
    if (auto cleaned = cleanup(); !cleaned)
      recordNativeError(std::move(cleaned.error()));
    {
      std::lock_guard lock(m_eventMutex);
      if (!m_ownerEvents.empty() || m_nativeError)
        m_dispatcher.wake();
    }
  }

  struct OwnerEvent {
    std::optional<InputEvent> input = std::nullopt;
    std::optional<std::pair<WindowMode, Vec2U>> window = std::nullopt;
    bool quit = false;
  };

  struct ImguiEvent {
    SDL_Event event;
    std::string text;
  };

  void queueImguiEvent(SDL_Event const& event) {
    // Only events consumed by the SDL ImGui backend cross its deferred phase.
    // Text is the only pointer payload in this set and must outlive SDL_PollEvent.
    switch (event.type) {
      case SDL_EVENT_MOUSE_MOTION:
      case SDL_EVENT_MOUSE_WHEEL:
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
      case SDL_EVENT_MOUSE_BUTTON_UP:
      case SDL_EVENT_KEY_DOWN:
      case SDL_EVENT_KEY_UP:
      case SDL_EVENT_WINDOW_MOUSE_ENTER:
      case SDL_EVENT_WINDOW_MOUSE_LEAVE:
      case SDL_EVENT_WINDOW_FOCUS_GAINED:
      case SDL_EVENT_WINDOW_FOCUS_LOST:
      case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      case SDL_EVENT_GAMEPAD_ADDED:
      case SDL_EVENT_GAMEPAD_REMOVED:
        m_imguiEvents.emplace_back(ImguiEvent{event, {}});
        break;
      case SDL_EVENT_TEXT_INPUT:
        m_imguiEvents.emplace_back(ImguiEvent{event, event.text.text ? event.text.text : ""});
        break;
      default:
        break;
    }
  }

  void processImguiEvents() {
    for (auto& queued : m_imguiEvents) {
      if (queued.event.type == SDL_EVENT_TEXT_INPUT)
        queued.event.text.text = queued.text.c_str();
      ImGui_ImplSDL3_ProcessEvent(&queued.event);
    }
    m_imguiEvents.clear();
  }

  void queueWindowChanged() {
    std::lock_guard lock(m_eventMutex);
    m_ownerEvents.emplace_back(OwnerEvent{.window = std::pair{m_windowMode, m_windowSize}});
  }

  void recordNativeError(StarException error) {
    std::lock_guard lock(m_eventMutex);
    if (!m_nativeError)
      m_nativeError.emplace(std::move(error));
    m_dispatcher.wake();
  }

  [[nodiscard]] std::expected<void, StarException> processOwnerEvents() {
    {
      std::lock_guard lock(m_eventMutex);
      if (m_nativeError)
        return std::unexpected(std::move(*std::exchange(m_nativeError, {})));
      m_ownerEvents.swap(m_ownerEventScratch);
    }
    for (auto const& event : m_ownerEventScratch) {
      if (event.window) {
        auto const& [mode, size] = *event.window;
        if (auto resized = m_renderer->setScreenSize(size); !resized)
          return std::unexpected(std::move(resized.error()));
        if (auto changed = m_application->windowChanged(mode, size); !changed)
          return std::unexpected(std::move(changed.error()));
      }
      if (event.input)
        if (auto processed = m_application->processInput(*event.input); !processed)
          return std::unexpected(std::move(processed.error()));
      if (event.quit)
        m_quitRequested = true;
    }
    m_ownerEventScratch.clear();
    return {};
  }

  struct NativeState {
    std::optional<bool> cursorVisible;
    std::optional<bool> cursorHardware;
    std::optional<Vec2I> cursorPosition;
    std::optional<bool> acceptingTextInput;
    std::optional<std::optional<std::pair<RectI, int>>> textInputArea;
  };

  void applyNativeState() {
    NativeState state;
    {
      std::lock_guard lock(m_eventMutex);
      std::swap(state, m_nativeState);
    }
    if (state.cursorVisible)
      m_cursorVisible = *state.cursorVisible;
    if (state.cursorHardware)
      m_cursorHardware = *state.cursorHardware;
    if (state.cursorPosition)
      SDL_WarpMouseInWindow(m_sdlWindow.get(), (*state.cursorPosition)[0], (*state.cursorPosition)[1]);
    if (state.acceptingTextInput && *state.acceptingTextInput != m_acceptingTextInput) {
      m_acceptingTextInput = *state.acceptingTextInput;
      if (m_acceptingTextInput)
        SDL_StartTextInput(m_sdlWindow.get());
      else
        SDL_StopTextInput(m_sdlWindow.get());
    }
    if (state.textInputArea && *state.textInputArea != m_textInputArea) {
      m_textInputArea = std::move(*state.textInputArea);
      if (m_textInputArea) {
        auto const& [r, cursor] = *m_textInputArea;
        SDL_Rect rect{.x = r.xMin(),
                      .y = static_cast<int>(m_windowSize.y()) - r.yMax(),
                      .w = r.width(),
                      .h = r.height()};
        SDL_SetTextInputArea(m_sdlWindow.get(), &rect, cursor);
      } else {
        SDL_SetTextInputArea(m_sdlWindow.get(), nullptr, 0);
      }
    }
  }

  [[nodiscard]] std::expected<void, StarException> getAudioData(std::span<std::int16_t> samples) {
    if (m_audioEnabled.load(std::memory_order_acquire))
      return m_application->getAudioData(samples, samples.size() / 2);
    std::ranges::fill(samples, 0);
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> setVSyncEnabled(bool vsyncEnabled) {
    if (vsyncEnabled) {
      // If VSync is requested, try for late swap tearing first, then fall back
      // to regular VSync
      if (auto result = (*m_logger).info("Application: Enabling adaptive VSync"); !result)
        return std::unexpected(std::move(result.error()));
      if (!SDL_GL_SetSwapInterval(-1)) {
        if (auto result = (*m_logger).info("Application: Enabling adaptive VSync, falling back to full VSync"); !result)
          return std::unexpected(std::move(result.error()));
        SDL_GL_SetSwapInterval(1);
      }
    } else {
      if (auto result = (*m_logger).info("Application: Disabling VSync"); !result)
        return std::unexpected(std::move(result.error()));
      SDL_GL_SetSwapInterval(0);
    }
    return {};
  }

  inline static const std::size_t MaxCursorSize = 128;
  [[nodiscard]] std::expected<bool, StarException> setCursorImage(const String& id, const std::shared_ptr<Image const>& image, unsigned scale, const Vec2I& offset) {
    auto imageSize = image->size().piecewise_multiply(Vec2U::filled(scale));
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
          FlipImageOperation{FlipImageOperation::Mode::FlipY},// SDL wants an Australian cursor.
          BorderImageOperation{
            1, Vec4B(), Vec4B(), false,
            false},// Nearest scaling fucks up and clips half off the edges, work around this with border+crop for now.
          ScaleImageOperation{ScaleImageOperation::Mode::Nearest, Vec2F::filled(scale)},
          CropImageOperation{
            RectI::withSize(Vec2I::filled(std::ceilf(static_cast<float>(scale) / 2)), Vec2I(imageSize))}};
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
    for (auto&& entry : *heldData)
      types.push_back(entry.first.utf8Ptr());
    auto request = [](void* userdata, const char* mime_type, std::size_t* size) -> const void* {
      if (auto entry = static_cast<StringMap<ByteArray>*>(userdata)->find_ref(String(std::string_view(mime_type)))) {
        *size = entry->get().size();
        return entry->get().ptr();
      }
      *size = 0;
      return NULL;
    };
    auto cleanup = [](void* userdata) { delete static_cast<StringMap<ByteArray>*>(userdata); };
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

  FixedScheduler m_scheduler;
  float m_renderInterpolation = 1.0f;
  TickRateMonitor m_updateTicker = TickRateMonitor(1.0f);
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

  using CursorDescriptor = std::tuple<unsigned, Vec2I, String>;

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
  std::atomic<bool> m_audioEnabled = false;
  bool m_quitRequested = false;
  bool m_applicationStarted = false;
  bool m_failureReported = false;
  std::atomic<float> m_displayScale = 1.0f;

  std::shared_ptr<ThreadedRenderer> m_renderer;
  std::shared_ptr<OpenGlRenderer> m_glRenderer;
  std::optional<NativeUiContextScope> m_nativeContextScope;
  Logger* m_logger = nullptr;
  MainThreadDispatcher m_dispatcher;
  std::atomic<bool> m_ownerDone = false;
  bool m_pollingEnabled = false;
  bool m_steamIsFlatpak = false;
  std::atomic<bool> m_focused = false;
  // Main-owned capture policy of the last successfully presented ImGui frame.
  bool m_captureKeyboard = false;
  bool m_captureText = false;
  bool m_captureMouse = false;
  std::mutex m_pipelineMutex;
  std::optional<PresentedFrame> m_pendingFrame;
  std::optional<ThreadedRenderer::FrameTicket> m_ownerFrame;
  Application::FrameTimings m_completedTimings;
  float m_completedRenderRate = 0.0f;
  bool m_frameCompleted = false;
  std::optional<StarException> m_pipelineError;
  std::mutex m_eventMutex;
  std::vector<OwnerEvent> m_ownerEvents;
  std::vector<OwnerEvent> m_ownerEventScratch;
  std::vector<ImguiEvent> m_imguiEvents;
  std::optional<StarException> m_nativeError;
  NativeState m_nativeState;
  std::optional<MainThreadServices> m_mainThreadServices;
  ImguiPlatformCallbacks m_originalImguiCallbacks;
  ImGuiContext* m_imguiCallbackContext = nullptr;
  SdlPlatform* m_nextImguiPlatform = nullptr;
  inline static std::mutex s_imguiPlatformMutex;
  inline static SdlPlatform* s_imguiPlatforms = nullptr;
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
    auto result = platform.run(std::move(cmdLineArgs));
    diagnosticReported = platform.failureReported();
    return result;
  }();
  if (!result)
    return reportError(result.error(), diagnosticReported);
  return 0;
}

}
