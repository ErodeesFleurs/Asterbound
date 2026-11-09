#include "steam/steam_api.h"
import std;
import star.format;
import star.output_proxy;

#include <QApplication>
#include <QMessageBox>


import star.mod_uploader;
import star.string_conversion;
import star.string;
import star.logging;
import star.random;
import star.native_runtime;

using namespace Star;

int main(int argc, char** argv) {
  auto nativeRuntime = NativeRuntime::create();
  QApplication app(argc, argv);
  auto reportError = [](StarException const& error) {
    auto diagnostic = Star::format("{}", Star::output_exception(ExceptionDiagnostic(error), true));
    QMessageBox::critical(nullptr, "Error", toQString(String(diagnostic)));
    if (!Star::print("{}\n", diagnostic))
      std::_Exit(1);
  };
  if (!nativeRuntime) {
    reportError(nativeRuntime.error());
    return 1;
  }

  if (!SteamAPI_Init()) {
    QMessageBox::critical(nullptr, "Error", "Could not initialize Steam API");
    return 1;
  }
  struct SteamShutdown {
    ~SteamShutdown() { SteamAPI_Shutdown(); }
  } steamShutdown;

  Logger logger;
  auto random = RandomService::create();
  if (!random) {
    reportError(random.error());
    return 1;
  }
  auto modUploader = ModUploader::create(logger, **random);
  if (!modUploader) {
    reportError(modUploader.error());
    return 1;
  }
  (*modUploader)->show();
  return app.exec();
}
