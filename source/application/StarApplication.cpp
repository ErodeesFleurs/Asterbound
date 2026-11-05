import std;
import star.format;
import star.vector;
import star.input_event;
import star.application;
import star.thread;
import star.time;
import star.logging;
import star.string;

namespace Star {

std::expected<void, StarException> Application::startup(StringList const&) { return {}; }

std::expected<void, StarException> Application::applicationInit(std::shared_ptr<ApplicationController> appController) {
  m_appController = std::move(appController);
  return {};
}

std::expected<void, StarException> Application::renderInit(std::shared_ptr<Renderer> renderer) {
  m_renderer = std::move(renderer);
  return {};
}

std::expected<void, StarException> Application::windowChanged(WindowMode, Vec2U) { return {}; }

std::expected<void, StarException> Application::processInput(InputEvent const&) { return {}; }

std::expected<void, StarException> Application::update() { return {}; }

unsigned Application::framesSkipped() const { return 0; }

std::expected<void, StarException> Application::render() { return {}; }

std::expected<void, StarException> Application::framePresented() { return {}; }

std::expected<void, StarException> Application::getAudioData(std::span<std::int16_t> samples, std::size_t) {
  std::ranges::fill(samples, 0);
  return {};
}

std::expected<void, StarException> Application::shutdown() { return {}; }
}
