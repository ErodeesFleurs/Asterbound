module;
import std;
import star.algorithm;
import star.format;
import star.output_proxy;
import star.string;
import star.logging;

namespace Star {

struct RpcPromiseExceptionTag {
  static constexpr std::string_view name() { return "RpcPromiseException"; }
};
using RpcPromiseException = StarError<RpcPromiseExceptionTag, StarException>;

enum class RpcPromiseStatus { Pending, Succeeded, Failed, Cancelled };

template <class T>
inline constexpr bool rpc_transform_result = false;

template <class T>
inline constexpr bool rpc_transform_result<std::expected<T, StarException>> = std::constructible_from<T, T>;

template <class Function, class Result>
concept rpc_transform = std::is_object_v<Function> && std::constructible_from<Function, Function>
    && std::invocable<Function const&, Result> && rpc_transform_result<std::invoke_result_t<Function const&, Result>>;

template <class Progress>
concept rpc_progress = std::same_as<Progress, std::monostate>
    || (std::is_object_v<Progress> && std::constructible_from<Progress, Progress> && requires(Progress& progress) {
          { progress.poll() } -> std::same_as<std::expected<void, StarException>>;
          { progress.cancel() } -> std::same_as<std::expected<void, StarException>>;
        });

// Wrapping can change the result type. The ownership graph, unlike observations,
// must be traversable across those type boundaries without polling any state.
struct RpcPromiseNode {
  virtual ~RpcPromiseNode() = default;
  // Called under mutex; a nonempty edge can disappear but is never retargeted.
  virtual std::shared_ptr<RpcPromiseNode> dependency() const = 0;
  std::mutex mutex;
};

template <typename Result, typename Error>
class RpcPromise;

// The other side of an RpcPromise. Completion is allowed exactly once except
// that late producer decisions after cancellation are harmless discards.
template <typename Result, typename Error = String>
class RpcPromiseKeeper {
public:
  [[nodiscard]] std::expected<void, StarException> chain(RpcPromise<Result, Error> promise);
  [[nodiscard]] std::expected<void, StarException> fulfill(Result result);
  [[nodiscard]] std::expected<void, StarException> fail(Error error);
  [[nodiscard]] std::expected<bool, StarException> cancel(Error reason);

private:
  template <typename ResultT, typename ErrorT>
  friend class RpcPromise;
  template <typename, typename>
  friend class AutoFailRpcPromiseKeeper;

  std::shared_ptr<typename RpcPromise<Result, Error>::State> m_state;
};

// Share the wrapper, not its destructor responsibility. Explicit decisions and
// abandonment use the same keeper state, including decisions made by other keepers.
template <typename Result, typename Error = String>
class AutoFailRpcPromiseKeeper {
public:
  [[nodiscard]] std::expected<void, StarException> chain(RpcPromise<Result, Error> promise);
  [[nodiscard]] std::expected<void, StarException> fulfill(Result result);
  [[nodiscard]] std::expected<void, StarException> fail(Error error);
  [[nodiscard]] std::expected<bool, StarException> cancel(Error reason);

  AutoFailRpcPromiseKeeper(RpcPromiseKeeper<Result, Error> keeper)
    requires std::constructible_from<Error, decltype("Keeper destroyed")>;
  AutoFailRpcPromiseKeeper(AutoFailRpcPromiseKeeper const&) = delete;
  AutoFailRpcPromiseKeeper& operator=(AutoFailRpcPromiseKeeper const&) = delete;
  AutoFailRpcPromiseKeeper(AutoFailRpcPromiseKeeper&&) = delete;
  AutoFailRpcPromiseKeeper& operator=(AutoFailRpcPromiseKeeper&&) = delete;
  ~AutoFailRpcPromiseKeeper();

private:
  RpcPromiseKeeper<Result, Error> m_keeper;
};

// A generic promise for the result of a remote procedure call.  It has
// reference semantics and is implicitly shared. Thread safe via mutexes. Can be chained.
template <typename Result, typename Error = String>
class RpcPromise {
public:
  static std::pair<RpcPromise, RpcPromiseKeeper<Result, Error>> make_pair();
  static RpcPromise make_fulfilled(Result result);
  static RpcPromise make_failed(Error error);

  // Query diagnostics are distinct from the remote call's Error outcome.
  [[nodiscard]] std::expected<RpcPromiseStatus, StarException> status() const;
  [[nodiscard]] std::expected<bool, StarException> finished() const;
  [[nodiscard]] std::expected<bool, StarException> succeeded() const;
  [[nodiscard]] std::expected<bool, StarException> failed() const;
  [[nodiscard]] std::expected<bool, StarException> cancelled() const;
  [[nodiscard]] std::expected<bool, StarException> cancel(Error reason) const;

  // Observe independently within a lifetime; cancellation never affects the
  // shared source unless an explicitly supplied Progress hook does so.
  template <rpc_progress Progress = std::monostate>
  [[nodiscard]] std::expected<RpcPromise, StarException> with_cancellation(std::stop_token lifetime, Error reason,
                                                                          Progress progress = {}) const;

  // A successful query contains a result only when the RPC succeeded.
  [[nodiscard]] std::expected<std::optional<Result>, StarException> result() const
    requires std::constructible_from<Result, Result const&>;

  // A successful query contains an Error only on failure or cancellation.
  [[nodiscard]] std::expected<std::optional<Error>, StarException> error() const
    requires std::constructible_from<Error, Error const&>;

  // Evaluation is lazy and successful values are cached for every observer.
  // The callback returns expected<T, StarException>. A diagnostic leaves the
  // wrapper unevaluated, so a later query retries it.
  template <rpc_transform<Result> Function>
    requires std::constructible_from<Result, Result const&> && std::constructible_from<Error, Error const&>
        && std::constructible_from<Error, Error>
  [[nodiscard]] auto wrap(Function function)
      -> std::expected<RpcPromise<typename std::invoke_result_t<Function const&, Result>::value_type, Error>, StarException>;

private:
  template <typename ResultT, typename ErrorT>
  friend class RpcPromise;

  friend class RpcPromiseKeeper<Result, Error>;

  // Chaining is a keeper decision, even while the delegated call is pending.
  struct Pending {};
  struct Fulfilled { Result value; };
  struct Failed { Error value; };
  struct Cancelled {
    Error value;
  };
  struct Chained { RpcPromise promise; };

  using Outcome = std::variant<Pending, Fulfilled, Failed, Cancelled, Chained>;

  struct State : RpcPromiseNode {
    std::shared_ptr<RpcPromiseNode> dependency() const override {
      if (std::holds_alternative<Chained>(outcome))
        return std::get<Chained>(outcome).promise.m_state;
      return {};
    }

    // Enter and return with our state lock held. Implementations release it
    // before observing a dependency, waiting on user code or invoking a hook.
    [[nodiscard]] virtual std::expected<void, StarException> poll(std::unique_lock<std::mutex>&) { return {}; }
    virtual bool scoped_to(std::stop_token const&) const { return false; }
    [[nodiscard]] virtual std::expected<void, StarException> on_cancelled() { return {}; }

    bool pending() const {
      return std::holds_alternative<Pending>(outcome);
    }

    [[nodiscard]] std::expected<bool, StarException> cancel(Error&& reason) {
      {
        std::lock_guard lock(mutex);
        if (!pending())
          return false;
        outcome.template emplace<Cancelled>(std::move(reason));
      }
      auto notified = on_cancelled();
      if (!notified)
        return std::unexpected(std::move(notified.error()));
      return true;
    }

    void fulfill(Result&& result) {
      outcome.template emplace<Fulfilled>(std::move(result));
    }

    void fail(Error&& error) {
      outcome.template emplace<Failed>(std::move(error));
    }

    Outcome outcome;
  };

  template <typename SourceResult, rpc_transform<SourceResult> Function>
  struct WrappedState : State {
    using Source = RpcPromise<SourceResult, Error>;
    WrappedState(Function&& function, Source source) : function(std::move(function)), source(std::move(source)) {}

    std::shared_ptr<RpcPromiseNode> dependency() const override { return source.m_state; }

    [[nodiscard]] std::expected<void, StarException> on_cancelled() override {
      evaluated.notify_all();
      return {};
    }

    [[nodiscard]] std::expected<void, StarException> poll(std::unique_lock<std::mutex>& lock) override {
      evaluated.wait(lock, [&] { return !evaluating || !this->pending(); });
      if (!this->pending())
        return {};
      evaluating = true;
      auto finish = make_scope_exit([&] {
        if (!lock.owns_lock())
          lock.lock();
        evaluating = false;
        evaluated.notify_all();
      });
      lock.unlock();
      auto evaluation = [&]() -> std::expected<void, StarException> {
        using Snapshot = typename Source::Outcome;
        auto observed = source.observe([](Snapshot const& outcome) {
          return std::expected<Snapshot, StarException>(std::in_place, outcome);
        });
        if (!observed)
          return std::unexpected(std::move(observed.error()));
        if (std::holds_alternative<typename Source::Fulfilled>(*observed)) {
          auto transformed = std::invoke(function, std::move(std::get<typename Source::Fulfilled>(*observed).value));
          if (!transformed)
            return std::unexpected(std::move(transformed.error()));
          std::lock_guard publish(this->mutex);
          if (this->pending())
            this->fulfill(std::move(*transformed));
        } else if (!std::holds_alternative<typename Source::Pending>(*observed)) {
          std::lock_guard publish(this->mutex);
          if (this->pending()) {
            if (std::holds_alternative<typename Source::Cancelled>(*observed))
              this->outcome.template emplace<Cancelled>(std::move(std::get<typename Source::Cancelled>(*observed).value));
            else
              this->fail(std::move(std::get<typename Source::Failed>(*observed).value));
          }
        }
        return {};
      }();
      lock.lock();
      // Cancellation wins over late values and diagnostics.
      if (!this->pending())
        return {};
      return evaluation;
    }

    Function const function;
    Source const source;
    std::condition_variable evaluated;
    bool evaluating = false;
  };

  template <rpc_progress Progress> struct ScopedState : State {
    ScopedState(RpcPromise source, std::stop_token lifetime, Error&& reason, Progress&& progress)
        : source(std::move(source)), lifetime(lifetime), reason(std::move(reason)), progress(std::move(progress)) {}

    struct Stop {
      std::weak_ptr<ScopedState> state;
      void operator()() const noexcept {
        // Hooks can erase the entry holding the last external strong reference.
        if (auto pinned = state.lock()) {
          auto cancelled = pinned->cancel(std::move(pinned->reason));
          if (!cancelled)
            EmergencyLog::error("Error cancelling RPC observation: {}",
                                Star::output_exception(ExceptionDiagnostic(cancelled.error()), true));
        }
      }
    };

    void observe_stop(std::shared_ptr<ScopedState> const& self) { stop.emplace(lifetime, Stop{self}); }

    bool scoped_to(std::stop_token const& token) const override { return lifetime == token; }

    std::shared_ptr<RpcPromiseNode> dependency() const override {
      return source.m_state ? source.m_state : State::dependency();
    }

    [[nodiscard]] std::expected<void, StarException> on_cancelled() override {
      RpcPromise released;
      {
        std::lock_guard lock(this->mutex);
        released = std::move(source);
      }
      released = {};
      if constexpr (!std::is_same_v<Progress, std::monostate>)
        return progress.cancel();
      return {};
    }

    [[nodiscard]] std::expected<void, StarException> poll(std::unique_lock<std::mutex>& lock) override {
      auto source_status = [&]() -> std::expected<RpcPromiseStatus, StarException> {
        auto observed = source;
        lock.unlock();
        if constexpr (!std::same_as<Progress, std::monostate>) {
          auto polled = progress.poll();
          std::lock_guard check(this->mutex);
          if (!this->pending())
            return RpcPromiseStatus::Pending; // The concurrent decision is already published.
          if (!polled)
            return std::unexpected(std::move(polled.error()));
        }
        return observed.status();
      }();
      lock.lock();
      if (!this->pending())
        return {};
      if (!source_status)
        return std::unexpected(std::move(source_status.error()));
      if (*source_status != RpcPromiseStatus::Pending)
        this->outcome.template emplace<Chained>(std::move(source));
      return {};
    }

    RpcPromise source;
    std::stop_token lifetime;
    Error reason;
    Progress progress;
    // Destroy/unregister before the source, reason, progress and base state.
    std::optional<std::stop_callback<Stop>> stop;
  };

  // Project only a local outcome; never invoke user transforms or progress
  // hooks here. Pin the next node before unlocking, retire the old pin after.
  template <class Project>
    requires std::invocable<Project&, Outcome const&>
  [[nodiscard]] auto observe(Project project) const -> std::invoke_result_t<Project&, Outcome const&> {
    auto current = m_state;
    if (!current)
      return std::unexpected(RpcPromiseException("Cannot query an empty RpcPromise"));
    for (;;) {
      std::shared_ptr<State> next;
      {
        std::unique_lock lock(current->mutex);
        if (current->pending()) {
          if (auto polled = current->poll(lock); !polled)
            return std::unexpected(std::move(polled.error()));
        }
        auto const& outcome = current->outcome;
        if (!std::holds_alternative<Chained>(outcome))
          return project(outcome);
        next = std::get<Chained>(outcome).promise.m_state;
      }
      current = std::move(next);
    }
  }

  std::shared_ptr<State> m_state;
};

template <typename Result, typename Error>
[[nodiscard]] std::expected<void, StarException> RpcPromiseKeeper<Result, Error>::chain(RpcPromise<Result, Error> promise) {
  if (!m_state)
    return std::unexpected(RpcPromiseException("Cannot chain an empty RpcPromiseKeeper"));
  using Promise = RpcPromise<Result, Error>;
  // Called with the keeper locked. A rejected edge does not consume it.
  auto commit = [&](bool cycle) -> std::expected<void, StarException> {
    if (std::holds_alternative<typename Promise::Cancelled>(m_state->outcome))
      return {};
    if (!promise.m_state)
      return std::unexpected(RpcPromiseException("Cannot chain an empty RpcPromise"));
    if (!m_state->pending())
      return std::unexpected(RpcPromiseException("chain called on already finished RpcPromise"));
    if (cycle)
      return std::unexpected(RpcPromiseException("Cannot chain an RpcPromise in a cycle"));
    m_state->outcome.template emplace<typename Promise::Chained>(std::move(promise));
    return {};
  };

  // Already observed edges can only stay put or disappear. Holding the
  // keeper and the target path's leaf through publication excludes a racing
  // reverse edge (including one involving wrapped or scoped dependencies).
  std::shared_ptr<RpcPromiseNode> current = promise.m_state;
  while (current && current != m_state) {
    std::shared_ptr<RpcPromiseNode> next;
    {
      std::scoped_lock lock(m_state->mutex, current->mutex);
      if (!m_state->pending() || !(next = current->dependency()))
        return commit(false);
    }
    current = std::move(next);
  }
  std::lock_guard lock(m_state->mutex);
  return commit(true);
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<void, StarException> RpcPromiseKeeper<Result, Error>::fulfill(Result result) {
  if (!m_state)
    return std::unexpected(RpcPromiseException("Cannot fulfill an empty RpcPromiseKeeper"));
  std::lock_guard lock(m_state->mutex);
  if (std::holds_alternative<typename RpcPromise<Result, Error>::Cancelled>(m_state->outcome))
    return {};
  if (!m_state->pending())
    return std::unexpected(RpcPromiseException("fulfill called on already finished RpcPromise"));
  m_state->fulfill(std::move(result));
  return {};
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<void, StarException> RpcPromiseKeeper<Result, Error>::fail(Error error) {
  if (!m_state)
    return std::unexpected(RpcPromiseException("Cannot fail an empty RpcPromiseKeeper"));
  std::lock_guard lock(m_state->mutex);
  if (std::holds_alternative<typename RpcPromise<Result, Error>::Cancelled>(m_state->outcome))
    return {};
  if (!m_state->pending())
    return std::unexpected(RpcPromiseException("fail called on already finished RpcPromise"));
  m_state->fail(std::move(error));
  return {};
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<bool, StarException> RpcPromiseKeeper<Result, Error>::cancel(Error reason) {
  auto pinned = m_state;
  if (!pinned)
    return std::unexpected(RpcPromiseException("Cannot cancel an empty RpcPromiseKeeper"));
  return pinned->cancel(std::move(reason));
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<void, StarException> AutoFailRpcPromiseKeeper<Result, Error>::chain(RpcPromise<Result, Error> promise) {
  return m_keeper.chain(std::move(promise));
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<void, StarException> AutoFailRpcPromiseKeeper<Result, Error>::fulfill(Result result) {
  return m_keeper.fulfill(std::move(result));
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<void, StarException> AutoFailRpcPromiseKeeper<Result, Error>::fail(Error error) {
  return m_keeper.fail(std::move(error));
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<bool, StarException> AutoFailRpcPromiseKeeper<Result, Error>::cancel(Error reason) {
  return m_keeper.cancel(std::move(reason));
}

template <typename Result, typename Error>
AutoFailRpcPromiseKeeper<Result, Error>::AutoFailRpcPromiseKeeper(RpcPromiseKeeper<Result, Error> keeper)
  requires std::constructible_from<Error, decltype("Keeper destroyed")>
  : m_keeper(std::move(keeper)) {}

template <typename Result, typename Error>
AutoFailRpcPromiseKeeper<Result, Error>::~AutoFailRpcPromiseKeeper() {
  auto const& state = m_keeper.m_state;
  if (!state) {
    EmergencyLog::error("Cannot auto-fail an empty RpcPromiseKeeper");
    return;
  }
  {
    std::lock_guard lock(state->mutex);
    if (!state->pending())
      return;
  }
  // Construct user-defined reasons outside the lock; an independent keeper
  // can still win before we publish the fallback.
  Error reason("Keeper destroyed");
  std::lock_guard lock(state->mutex);
  if (state->pending())
    state->fail(std::move(reason));
}

template <typename Result, typename Error>
std::pair<RpcPromise<Result, Error>, RpcPromiseKeeper<Result, Error>> RpcPromise<Result, Error>::make_pair() {
  auto state = std::make_shared<State>();
  RpcPromise promise;
  promise.m_state = state;
  RpcPromiseKeeper<Result, Error> keeper;
  keeper.m_state = std::move(state);

  return {std::move(promise), std::move(keeper)};
}

template <typename Result, typename Error>
RpcPromise<Result, Error> RpcPromise<Result, Error>::make_fulfilled(Result result) {
  RpcPromise promise;
  promise.m_state = std::make_shared<State>();
  promise.m_state->fulfill(std::move(result));
  return promise;
}

template <typename Result, typename Error>
RpcPromise<Result, Error> RpcPromise<Result, Error>::make_failed(Error error) {
  RpcPromise promise;
  promise.m_state = std::make_shared<State>();
  promise.m_state->fail(std::move(error));
  return promise;
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<RpcPromiseStatus, StarException> RpcPromise<Result, Error>::status() const {
  return observe([](Outcome const& outcome) -> std::expected<RpcPromiseStatus, StarException> {
    if (std::holds_alternative<Fulfilled>(outcome))
      return RpcPromiseStatus::Succeeded;
    if (std::holds_alternative<Failed>(outcome))
      return RpcPromiseStatus::Failed;
    if (std::holds_alternative<Cancelled>(outcome))
      return RpcPromiseStatus::Cancelled;
    return RpcPromiseStatus::Pending;
  });
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<bool, StarException> RpcPromise<Result, Error>::finished() const {
  return status().transform([](RpcPromiseStatus value) { return value != RpcPromiseStatus::Pending; });
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<bool, StarException> RpcPromise<Result, Error>::succeeded() const {
  return status().transform([](RpcPromiseStatus value) { return value == RpcPromiseStatus::Succeeded; });
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<bool, StarException> RpcPromise<Result, Error>::failed() const {
  return status().transform(
    [](RpcPromiseStatus value) { return value == RpcPromiseStatus::Failed || value == RpcPromiseStatus::Cancelled; });
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<bool, StarException> RpcPromise<Result, Error>::cancelled() const {
  return status().transform([](RpcPromiseStatus value) { return value == RpcPromiseStatus::Cancelled; });
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<bool, StarException> RpcPromise<Result, Error>::cancel(Error reason) const {
  auto pinned = m_state;
  if (!pinned)
    return std::unexpected(RpcPromiseException("Cannot query an empty RpcPromise"));
  return pinned->cancel(std::move(reason));
}

template <typename Result, typename Error>
template <rpc_progress Progress>
[[nodiscard]] std::expected<RpcPromise<Result, Error>, StarException>
RpcPromise<Result, Error>::with_cancellation(std::stop_token lifetime, Error reason, Progress progress) const {
  RpcPromise source = *this;
  if (!source.m_state)
    return std::unexpected(RpcPromiseException("Cannot query an empty RpcPromise"));
  {
    std::lock_guard lock(source.m_state->mutex);
    auto const& outcome = source.m_state->outcome;
    if (std::holds_alternative<Fulfilled>(outcome) || std::holds_alternative<Failed>(outcome)
        || std::holds_alternative<Cancelled>(outcome))
      return source;
    if constexpr (std::is_same_v<Progress, std::monostate>) {
      if (!lifetime.stop_possible() || source.m_state->scoped_to(lifetime))
        return source;
    }
  }
  auto scoped =
    std::make_shared<ScopedState<Progress>>(std::move(source), lifetime, std::move(reason), std::move(progress));
  RpcPromise view;
  view.m_state = scoped;
  scoped->observe_stop(scoped);
  return view;
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<std::optional<Result>, StarException> RpcPromise<Result, Error>::result() const
  requires std::constructible_from<Result, Result const&> {
  using Query = std::expected<std::optional<Result>, StarException>;
  return observe([](Outcome const& outcome) -> Query {
    if (std::holds_alternative<Fulfilled>(outcome))
      return Query(std::in_place, std::in_place, std::get<Fulfilled>(outcome).value);
    return Query(std::in_place);
  });
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<std::optional<Error>, StarException> RpcPromise<Result, Error>::error() const
  requires std::constructible_from<Error, Error const&> {
  using Query = std::expected<std::optional<Error>, StarException>;
  return observe([](Outcome const& outcome) -> Query {
    if (std::holds_alternative<Failed>(outcome))
      return Query(std::in_place, std::in_place, std::get<Failed>(outcome).value);
    if (std::holds_alternative<Cancelled>(outcome))
      return Query(std::in_place, std::in_place, std::get<Cancelled>(outcome).value);
    return Query(std::in_place);
  });
}

template <typename Result, typename Error>
template <rpc_transform<Result> Function>
  requires std::constructible_from<Result, Result const&> && std::constructible_from<Error, Error const&>
      && std::constructible_from<Error, Error>
[[nodiscard]] auto RpcPromise<Result, Error>::wrap(Function function)
    -> std::expected<RpcPromise<typename std::invoke_result_t<Function const&, Result>::value_type, Error>, StarException> {
  using WrappedResult = typename std::invoke_result_t<Function const&, Result>::value_type;
  using WrappedPromise = RpcPromise<WrappedResult, Error>;
  if (!m_state)
    return std::unexpected(RpcPromiseException("Cannot wrap an empty RpcPromise"));
  WrappedPromise wrapped;
  wrapped.m_state = std::make_shared<typename WrappedPromise::template WrappedState<Result, Function>>(
      std::move(function), *this);
  return wrapped;
}

}

export module star.rpc_promise;

export namespace Star {
using ::Star::rpc_progress;
using ::Star::rpc_transform;
using ::Star::AutoFailRpcPromiseKeeper;
using ::Star::RpcPromise;
using ::Star::RpcPromiseException;
using ::Star::RpcPromiseExceptionTag;
using ::Star::RpcPromiseKeeper;
using ::Star::RpcPromiseStatus;
}
