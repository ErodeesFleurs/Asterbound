module;
#include "SDL3/SDL.h"
#include "imgui.h"

import std;
import star.format;
import star.output_proxy;
import star.logging;

namespace Star {

// Null targets capture the current state while a new native context is created.
class NativeUiContextScope {
public:
  [[nodiscard]] static std::expected<NativeUiContextScope, StarException> create(SDL_Window* window, SDL_GLContext glContext, ImGuiContext* imguiContext) {
    NativeUiContextScope scope;
    if (glContext && (window != scope.m_previousWindow || glContext != scope.m_previousGlContext)) {
      if (!SDL_GL_MakeCurrent(window, glContext))
        return std::unexpected(StarException(Star::format("Could not activate native OpenGL context: {}", SDL_GetError())));
    }
    if (imguiContext)
      ImGui::SetCurrentContext(imguiContext);
    return scope;
  }

  [[nodiscard]] std::expected<void, StarException> restore() {
    if (!m_active)
      return {};
    ImGui::SetCurrentContext(m_previousImguiContext);
    if (SDL_GL_GetCurrentWindow() != m_previousWindow || SDL_GL_GetCurrentContext() != m_previousGlContext) {
      if (!SDL_GL_MakeCurrent(m_previousWindow, m_previousGlContext))
        return std::unexpected(StarException(Star::format("Could not restore native OpenGL context: {}", SDL_GetError())));
    }
    m_active = false;
    return {};
  }

  ~NativeUiContextScope() {
    if (auto result = restore(); !result)
      EmergencyLog::error("{}", Star::output_exception(ExceptionDiagnostic(result.error()), true));
  }

  NativeUiContextScope(NativeUiContextScope const&) = delete;
  NativeUiContextScope& operator=(NativeUiContextScope const&) = delete;
  NativeUiContextScope(NativeUiContextScope&& other) noexcept
    : m_previousWindow(other.m_previousWindow),
      m_previousGlContext(other.m_previousGlContext),
      m_previousImguiContext(other.m_previousImguiContext),
      m_active(std::exchange(other.m_active, false)) {}
  NativeUiContextScope& operator=(NativeUiContextScope&&) = delete;

  void discardImguiContext(ImGuiContext* context) {
    if (m_previousImguiContext == context)
      m_previousImguiContext = nullptr;
  }

  void discardGlContext(SDL_GLContext context) {
    if (m_previousGlContext == context) {
      m_previousGlContext = nullptr;
      m_previousWindow = nullptr;
    }
  }

private:
  NativeUiContextScope()
    : m_previousWindow(SDL_GL_GetCurrentWindow()),
      m_previousGlContext(SDL_GL_GetCurrentContext()),
      m_previousImguiContext(ImGui::GetCurrentContext()) {}

  SDL_Window* m_previousWindow;
  SDL_GLContext m_previousGlContext;
  ImGuiContext* m_previousImguiContext;
  bool m_active = true;
};

}

export module star.native_ui_context;

export namespace Star {
  using ::Star::NativeUiContextScope;
}
