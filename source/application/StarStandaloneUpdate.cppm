module;
#include "SDL3/SDL_process.h"
import std;
import star.application_controller;
import star.string;

export module star.standalone_update;

export namespace Star {

// Owned exclusively by the client logic thread, including all pipe operations.
class StandaloneUpdate {
public:
  void captureLaunch(StringList const& arguments);
  StandaloneUpdateState state() const { return m_state; }
  void start(String const& targetVersion);
  // True only after a complete COMMIT was written; the caller must then quit.
  bool poll();
  void cancel();
  void shutdown() { m_process.reset(); }

private:
  void closeInput(StandaloneUpdateState afterExit);

  StandaloneUpdateState m_state = StandaloneUpdateState::Unsupported;
#ifdef STAR_SYSTEM_WINDOWS
  std::filesystem::path m_tempDirectory;
#endif
  StandaloneUpdateState m_afterExit = StandaloneUpdateState::Failed;
  std::filesystem::path m_client;
  String m_workingDirectory;
  StringList m_arguments;
  std::unique_ptr<SDL_Process, decltype(&SDL_DestroyProcess)> m_process{nullptr, SDL_DestroyProcess};
  std::size_t m_readyBytes = 0;
};

}
