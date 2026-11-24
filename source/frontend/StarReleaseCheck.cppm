module;
import std;
import star.http_client;
import star.string;
import star.worker_pool;

export module star.release_check;

export namespace Star {

class ReleaseCheck {
public:
  enum class State { Idle, Checking, Current, Available, Failed };

  void start();
  // Publishes a completed check exactly once, without waiting for the network.
  bool poll();
  State state() const { return state_; }
  // Four decimal components without the tag prefix; empty unless a check succeeded.
  String const& latest_version() const { return latest_; }

private:
  std::optional<HttpClient> client_;
  std::optional<WorkerPoolPromise<HttpResponse>> pending_;
  State state_ = State::Idle;
  String latest_;
};

}
