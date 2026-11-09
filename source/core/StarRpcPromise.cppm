module;
import std;
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

  std::shared_ptr<typename RpcPromise<Result, Error>::State> m_state;
};

// Wraps an RpcPromiseKeeper, best used as a shared_ptr. Fails the promise on destruction.
template <typename Result, typename Error = String>
class AutoFailRpcPromiseKeeper {
public:
  [[nodiscard]] std::expected<void, StarException> chain(RpcPromise<Result, Error> promise);
  [[nodiscard]] std::expected<void, StarException> fulfill(Result result);
  [[nodiscard]] std::expected<void, StarException> fail(Error error);
  [[nodiscard]] std::expected<bool, StarException> cancel(Error reason);

  AutoFailRpcPromiseKeeper(RpcPromiseKeeper<Result, Error> keeper);
  ~AutoFailRpcPromiseKeeper();
private:
  RpcPromiseKeeper<Result, Error> m_keeper;
  bool m_done = false;
};

// A generic promise for the result of a remote procedure call.  It has
// reference semantics and is implicitly shared. Thread safe via mutexes. Can be chained.
template <typename Result, typename Error = String>
class RpcPromise {
public:
  static std::pair<RpcPromise, RpcPromiseKeeper<Result, Error>> createPair();
  static RpcPromise createFulfilled(Result result);
  static RpcPromise createFailed(Error error);

  // Query diagnostics are distinct from the remote call's Error outcome.
  [[nodiscard]] std::expected<RpcPromiseStatus, StarException> status() const;
  [[nodiscard]] std::expected<bool, StarException> finished() const;
  [[nodiscard]] std::expected<bool, StarException> succeeded() const;
  [[nodiscard]] std::expected<bool, StarException> failed() const;
  [[nodiscard]] std::expected<bool, StarException> cancelled() const;
  [[nodiscard]] std::expected<bool, StarException> cancel(Error reason) const;

  // Observe independently within a lifetime; cancellation never affects the
  // shared source unless an explicitly supplied Progress hook does so.
  template <class Progress = std::monostate>
  [[nodiscard]] std::expected<RpcPromise, StarException> withCancellation(std::stop_token lifetime, Error reason,
                                                                          Progress progress = {}) const;

  // A successful query contains a result only when the RPC succeeded.
  [[nodiscard]] std::expected<std::optional<Result>, StarException> result() const;

  // A successful query contains an Error only on failure or cancellation.
  [[nodiscard]] std::expected<std::optional<Error>, StarException> error() const;

  // Evaluation is lazy and successful values are cached for every observer.
  // The callback returns expected<T, StarException>. A diagnostic leaves the
  // wrapper unevaluated, so a later query retries it.
  template <typename Function>
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

  struct State {
    virtual ~State() = default;
    [[nodiscard]] virtual std::expected<void, StarException> poll() { return {}; }
    virtual bool scopedTo(std::stop_token const&) const { return false; }
    [[nodiscard]] virtual std::expected<void, StarException> onCancelled() { return {}; }

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
      auto notified = onCancelled();
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

    std::mutex mutex;
    std::variant<Pending, Fulfilled, Failed, Cancelled, Chained> outcome;
  };

  template <typename SourceResult, typename Function>
  struct WrappedState : State {
    WrappedState(Function function, RpcPromise<SourceResult, Error> other)
      : function(std::move(function)), other(std::move(other)) {}

    [[nodiscard]] std::expected<void, StarException> onCancelled() override {
      evaluated.notify_all();
      return {};
    }

    [[nodiscard]] std::expected<void, StarException> poll() override {
      {
        std::unique_lock lock(this->mutex);
        evaluated.wait(lock, [&] { return !evaluating || !this->pending(); });
        if (!this->pending())
          return {};
        evaluating = true;
      }
      // Neither delegated queries nor user transforms may run under our lock.
      auto evaluation = [&]() -> std::expected<void, StarException> {
        auto sourceStatus = other.status();
        if (!sourceStatus)
          return std::unexpected(std::move(sourceStatus.error()));
        if (*sourceStatus == RpcPromiseStatus::Pending)
          return {};
        if (*sourceStatus == RpcPromiseStatus::Succeeded) {
          auto sourceResult = other.result();
          if (!sourceResult)
            return std::unexpected(std::move(sourceResult.error()));
          auto wrappedResult = function(std::move(**sourceResult));
          if (!wrappedResult)
            return std::unexpected(std::move(wrappedResult.error()));
          std::lock_guard lock(this->mutex);
          if (this->pending())
            this->fulfill(std::move(*wrappedResult));
        } else {
          auto sourceError = other.error();
          if (!sourceError)
            return std::unexpected(std::move(sourceError.error()));
          std::lock_guard lock(this->mutex);
          if (this->pending()) {
            if (*sourceStatus == RpcPromiseStatus::Cancelled)
              this->outcome.template emplace<Cancelled>(std::move(**sourceError));
            else
              this->fail(std::move(**sourceError));
          }
        }
        return {};
      }();
      {
        std::lock_guard lock(this->mutex);
        evaluating = false;
        // A reentrant or concurrent cancellation wins over late diagnostics too.
        if (!this->pending())
          evaluation = {};
      }
      evaluated.notify_all();
      return evaluation;
    }

    Function const function;
    RpcPromise<SourceResult, Error> other;
    std::condition_variable evaluated;
    bool evaluating = false;
  };

  template <class Progress> struct ScopedState : State {
    ScopedState(RpcPromise source, std::stop_token lifetime, Error reason, Progress progress)
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

    void observeStop(std::shared_ptr<ScopedState> const& self) { stop.emplace(lifetime, Stop{self}); }

    bool scopedTo(std::stop_token const& token) const override { return lifetime == token; }

    [[nodiscard]] std::expected<void, StarException> onCancelled() override {
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

    [[nodiscard]] std::expected<void, StarException> poll() override {
      RpcPromise observed;
      {
        std::lock_guard lock(this->mutex);
        if (!this->pending())
          return {};
        observed = source;
      }
      if constexpr (!std::is_same_v<Progress, std::monostate>) {
        auto polled = progress.poll();
        std::lock_guard lock(this->mutex);
        if (!this->pending())
          return {};
        if (!polled)
          return std::unexpected(std::move(polled.error()));
      }
      auto sourceStatus = observed.status();
      std::lock_guard lock(this->mutex);
      if (!this->pending())
        return {};
      if (!sourceStatus)
        return std::unexpected(std::move(sourceStatus.error()));
      if (*sourceStatus != RpcPromiseStatus::Pending)
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

  [[nodiscard]] std::expected<std::shared_ptr<State>, StarException> state() const {
    if (!m_state)
      return std::unexpected(RpcPromiseException("Cannot query an empty RpcPromise"));
    return m_state;
  }

  std::shared_ptr<State> m_state;
};

template <typename Result, typename Error>
[[nodiscard]] std::expected<void, StarException> RpcPromiseKeeper<Result, Error>::chain(RpcPromise<Result, Error> promise) {
  if (!m_state)
    return std::unexpected(RpcPromiseException("Cannot chain an empty RpcPromiseKeeper"));
  std::lock_guard lock(m_state->mutex);
  if (std::holds_alternative<typename RpcPromise<Result, Error>::Cancelled>(m_state->outcome))
    return {};
  if (!promise.m_state)
    return std::unexpected(RpcPromiseException("Cannot chain an empty RpcPromise"));
  if (!m_state->pending())
    return std::unexpected(RpcPromiseException("chain called on already finished RpcPromise"));
  m_state->outcome.template emplace<typename RpcPromise<Result, Error>::Chained>(std::move(promise));
  return {};
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
  auto completed = m_keeper.chain(std::move(promise));
  if (!completed)
    return std::unexpected(std::move(completed.error()));
  m_done = true;
  return {};
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<void, StarException> AutoFailRpcPromiseKeeper<Result, Error>::fulfill(Result result) {
  auto completed = m_keeper.fulfill(std::move(result));
  if (!completed)
    return std::unexpected(std::move(completed.error()));
  m_done = true;
  return {};
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<void, StarException> AutoFailRpcPromiseKeeper<Result, Error>::fail(Error error) {
  auto completed = m_keeper.fail(std::move(error));
  if (!completed)
    return std::unexpected(std::move(completed.error()));
  m_done = true;
  return {};
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<bool, StarException> AutoFailRpcPromiseKeeper<Result, Error>::cancel(Error reason) {
  auto cancelled = m_keeper.cancel(std::move(reason));
  if (!cancelled)
    return std::unexpected(std::move(cancelled.error()));
  m_done = true;
  return *cancelled;
}

template <typename Result, typename Error>
AutoFailRpcPromiseKeeper<Result, Error>::AutoFailRpcPromiseKeeper(RpcPromiseKeeper<Result, Error> keeper)
  : m_keeper(std::move(keeper)) {}

template <typename Result, typename Error>
AutoFailRpcPromiseKeeper<Result, Error>::~AutoFailRpcPromiseKeeper() {
  if (!m_done) {
    auto completed = fail("Keeper destroyed");
    if (!completed)
      EmergencyLog::error("Failed to auto-fail RpcPromise keeper: {}", Star::output_exception(ExceptionDiagnostic(completed.error()), true));
  }
}

template <typename Result, typename Error>
std::pair<RpcPromise<Result, Error>, RpcPromiseKeeper<Result, Error>> RpcPromise<Result, Error>::createPair() {
  auto state = std::make_shared<State>();
  RpcPromise promise;
  promise.m_state = state;
  RpcPromiseKeeper<Result, Error> keeper;
  keeper.m_state = std::move(state);

  return {std::move(promise), std::move(keeper)};
}

template <typename Result, typename Error>
RpcPromise<Result, Error> RpcPromise<Result, Error>::createFulfilled(Result result) {
  RpcPromise promise;
  promise.m_state = std::make_shared<State>();
  promise.m_state->fulfill(std::move(result));
  return promise;
}

template <typename Result, typename Error>
RpcPromise<Result, Error> RpcPromise<Result, Error>::createFailed(Error error) {
  RpcPromise promise;
  promise.m_state = std::make_shared<State>();
  promise.m_state->fail(std::move(error));
  return promise;
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<RpcPromiseStatus, StarException> RpcPromise<Result, Error>::status() const {
  auto currentState = state();
  if (!currentState)
    return std::unexpected(std::move(currentState.error()));
  auto& val = **currentState;
  auto polled = val.poll();
  if (!polled)
    return std::unexpected(std::move(polled.error()));
  RpcPromise delegated;
  {
    std::lock_guard lock(val.mutex);
    if (auto chained = std::get_if<Chained>(&val.outcome))
      delegated = chained->promise;
    else if (std::holds_alternative<Fulfilled>(val.outcome))
      return RpcPromiseStatus::Succeeded;
    else if (std::holds_alternative<Failed>(val.outcome))
      return RpcPromiseStatus::Failed;
    else if (std::holds_alternative<Cancelled>(val.outcome))
      return RpcPromiseStatus::Cancelled;
    else
      return RpcPromiseStatus::Pending;
  }
  return delegated.status();
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
  auto currentState = state();
  if (!currentState)
    return std::unexpected(std::move(currentState.error()));
  return (*currentState)->cancel(std::move(reason));
}

template <typename Result, typename Error>
template <class Progress>
[[nodiscard]] std::expected<RpcPromise<Result, Error>, StarException>
RpcPromise<Result, Error>::withCancellation(std::stop_token lifetime, Error reason, Progress progress) const {
  auto currentState = state();
  if (!currentState)
    return std::unexpected(std::move(currentState.error()));
  RpcPromise source;
  source.m_state = *currentState;
  {
    std::lock_guard lock((*currentState)->mutex);
    auto const& outcome = (*currentState)->outcome;
    if (std::holds_alternative<Fulfilled>(outcome) || std::holds_alternative<Failed>(outcome)
        || std::holds_alternative<Cancelled>(outcome))
      return source;
    if constexpr (std::is_same_v<Progress, std::monostate>) {
      if (!lifetime.stop_possible() || (*currentState)->scopedTo(lifetime))
        return source;
    }
  }
  auto scoped =
    std::make_shared<ScopedState<Progress>>(std::move(source), lifetime, std::move(reason), std::move(progress));
  RpcPromise view;
  view.m_state = scoped;
  scoped->observeStop(scoped);
  return view;
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<std::optional<Result>, StarException> RpcPromise<Result, Error>::result() const {
  auto currentState = state();
  if (!currentState)
    return std::unexpected(std::move(currentState.error()));
  auto& val = **currentState;
  auto polled = val.poll();
  if (!polled)
    return std::unexpected(std::move(polled.error()));
  RpcPromise delegated;
  {
    std::lock_guard lock(val.mutex);
    if (auto chained = std::get_if<Chained>(&val.outcome))
      delegated = chained->promise;
    else if (auto fulfilled = std::get_if<Fulfilled>(&val.outcome))
      return std::optional<Result>(std::in_place, fulfilled->value);
    else
      return std::optional<Result>{};
  }
  return delegated.result();
}

template <typename Result, typename Error>
[[nodiscard]] std::expected<std::optional<Error>, StarException> RpcPromise<Result, Error>::error() const {
  auto currentState = state();
  if (!currentState)
    return std::unexpected(std::move(currentState.error()));
  auto& val = **currentState;
  auto polled = val.poll();
  if (!polled)
    return std::unexpected(std::move(polled.error()));
  RpcPromise delegated;
  {
    std::lock_guard lock(val.mutex);
    if (auto chained = std::get_if<Chained>(&val.outcome))
      delegated = chained->promise;
    else if (auto failed = std::get_if<Failed>(&val.outcome))
      return std::optional<Error>(std::in_place, failed->value);
    else if (auto cancelled = std::get_if<Cancelled>(&val.outcome))
      return std::optional<Error>(std::in_place, cancelled->value);
    else
      return std::optional<Error>{};
  }
  return delegated.error();
}

template <typename Result, typename Error>
template <typename Function>
[[nodiscard]] auto RpcPromise<Result, Error>::wrap(Function function)
    -> std::expected<RpcPromise<typename std::invoke_result_t<Function const&, Result>::value_type, Error>, StarException> {
  using WrappedResult = typename std::invoke_result_t<Function const&, Result>::value_type;
  static_assert(std::is_same_v<std::invoke_result_t<Function const&, Result>, std::expected<WrappedResult, StarException>>,
      "RpcPromise wrappers must return std::expected<T, StarException>");
  if (!m_state)
    return std::unexpected(RpcPromiseException("Cannot wrap an empty RpcPromise"));
  using WrappedPromise = RpcPromise<WrappedResult, Error>;
  WrappedPromise wrappedPromise;
  wrappedPromise.m_state = std::make_shared<typename WrappedPromise::template WrappedState<Result, Function>>(
      std::move(function), *this);
  return wrappedPromise;
}

}

export module star.rpc_promise;

export namespace Star {
using ::Star::AutoFailRpcPromiseKeeper;
using ::Star::RpcPromise;
using ::Star::RpcPromiseException;
using ::Star::RpcPromiseExceptionTag;
using ::Star::RpcPromiseKeeper;
using ::Star::RpcPromiseStatus;
}
