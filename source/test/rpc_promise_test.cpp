import std;
import star.rpc_promise;
import star.string;
import star.json;
import star.json_rpc;
import star.byte_array;
import star.data_stream_devices;
import star.list;
import star.logging;
import star.format;

#include "gtest/gtest.h"

using namespace Star;

namespace {
struct NonDefaultValue {
  explicit NonDefaultValue(int value) : value(value) {}
  int value;
};
}

TEST(RpcPromiseTest, SharedObservationAndSingleResolution) {
  auto [promise, keeper] = RpcPromise<int>::createPair();
  auto observer = promise;
  auto otherKeeper = keeper;
  EXPECT_EQ(observer.finished(), false);
  EXPECT_EQ(observer.succeeded(), false);
  EXPECT_EQ(observer.failed(), false);
  auto pendingResult = observer.result();
  ASSERT_TRUE(pendingResult);
  EXPECT_EQ(*pendingResult, std::optional<int>{});
  auto pendingError = observer.error();
  ASSERT_TRUE(pendingError);
  EXPECT_EQ(*pendingError, std::optional<String>{});

  ASSERT_TRUE(keeper.fulfill(42));
  EXPECT_EQ(observer.finished(), true);
  EXPECT_EQ(observer.succeeded(), true);
  EXPECT_EQ(observer.failed(), false);
  auto observedResult = observer.result();
  ASSERT_TRUE(observedResult);
  EXPECT_EQ(*observedResult, std::optional<int>{42});
  auto observedError = observer.error();
  ASSERT_TRUE(observedError);
  EXPECT_EQ(*observedError, std::optional<String>{});
  auto fulfilledAgain = otherKeeper.fulfill(9);
  ASSERT_FALSE(fulfilledAgain);
  EXPECT_TRUE(fulfilledAgain.error().is<RpcPromiseException>());
  auto failedAfterFulfill = otherKeeper.fail("late failure");
  ASSERT_FALSE(failedAfterFulfill);
  EXPECT_TRUE(failedAfterFulfill.error().is<RpcPromiseException>());
  auto chainedAfterFulfill = otherKeeper.chain(RpcPromise<int>::createFulfilled(1));
  ASSERT_FALSE(chainedAfterFulfill);
  EXPECT_TRUE(chainedAfterFulfill.error().is<RpcPromiseException>());
  auto resolvedResult = promise.result();
  ASSERT_TRUE(resolvedResult);
  EXPECT_EQ(*resolvedResult, std::optional<int>{42});

  auto [failure, failureKeeper] = RpcPromise<int>::createPair();
  ASSERT_TRUE(failureKeeper.fail("failed"));
  EXPECT_EQ(failure.finished(), true);
  EXPECT_EQ(failure.succeeded(), false);
  EXPECT_EQ(failure.failed(), true);
  auto failureResult = failure.result();
  ASSERT_TRUE(failureResult);
  EXPECT_EQ(*failureResult, std::optional<int>{});
  auto failureError = failure.error();
  ASSERT_TRUE(failureError);
  EXPECT_EQ(*failureError, std::optional<String>{"failed"});
  auto fulfilledAfterFailure = failureKeeper.fulfill(1);
  ASSERT_FALSE(fulfilledAfterFailure);
  EXPECT_TRUE(fulfilledAfterFailure.error().is<RpcPromiseException>());
  auto failedAgain = failureKeeper.fail("again");
  ASSERT_FALSE(failedAgain);
  EXPECT_TRUE(failedAgain.error().is<RpcPromiseException>());
}

TEST(RpcPromiseTest, DelegationIsResolutionEvenBeforeTargetFinishes) {
  auto [outer, outerKeeper] = RpcPromise<int>::createPair();
  auto [middle, middleKeeper] = RpcPromise<int>::createPair();
  auto [inner, innerKeeper] = RpcPromise<int>::createPair();
  ASSERT_TRUE(outerKeeper.chain(middle));
  ASSERT_TRUE(middleKeeper.chain(inner));
  EXPECT_EQ(outer.finished(), false);
  EXPECT_EQ(outer.succeeded(), false);
  EXPECT_EQ(outer.failed(), false);
  auto pendingResult = outer.result();
  ASSERT_TRUE(pendingResult);
  EXPECT_EQ(*pendingResult, std::optional<int>{});
  auto pendingError = outer.error();
  ASSERT_TRUE(pendingError);
  EXPECT_EQ(*pendingError, std::optional<String>{});
  auto fulfilled = outerKeeper.fulfill(1);
  ASSERT_FALSE(fulfilled);
  EXPECT_TRUE(fulfilled.error().is<RpcPromiseException>());
  auto failed = outerKeeper.fail("late");
  ASSERT_FALSE(failed);
  EXPECT_TRUE(failed.error().is<RpcPromiseException>());
  auto chained = outerKeeper.chain(inner);
  ASSERT_FALSE(chained);
  EXPECT_TRUE(chained.error().is<RpcPromiseException>());
  ASSERT_TRUE(innerKeeper.fulfill(73));
  EXPECT_EQ(outer.finished(), true);
  EXPECT_EQ(outer.succeeded(), true);
  auto completedResult = outer.result();
  ASSERT_TRUE(completedResult);
  EXPECT_EQ(*completedResult, std::optional<int>{73});

  auto [failedOuter, failedKeeper] = RpcPromise<int>::createPair();
  ASSERT_TRUE(failedKeeper.chain(RpcPromise<int>::createFailed("delegated error")));
  EXPECT_EQ(failedOuter.finished(), true);
  EXPECT_EQ(failedOuter.failed(), true);
  auto failedResult = failedOuter.result();
  ASSERT_TRUE(failedResult);
  EXPECT_EQ(*failedResult, std::optional<int>{});
  auto failedError = failedOuter.error();
  ASSERT_TRUE(failedError);
  EXPECT_EQ(*failedError, std::optional<String>{"delegated error"});
}

TEST(RpcPromiseTest, LazyWrapperCachesAcrossAllObservers) {
  auto [source, keeper] = RpcPromise<int>::createPair();
  int calls = 0;
  auto wrapping = source.wrap([&calls](int value) -> std::expected<String, StarException> {
    ++calls;
    return String(std::to_string(value));
  });
  ASSERT_TRUE(wrapping);
  auto wrapped = *wrapping;
  auto observer = wrapped;
  EXPECT_EQ(calls, 0);
  EXPECT_EQ(wrapped.finished(), false);
  auto pendingResult = wrapped.result();
  ASSERT_TRUE(pendingResult);
  EXPECT_EQ(*pendingResult, std::optional<String>{});
  auto pendingError = wrapped.error();
  ASSERT_TRUE(pendingError);
  EXPECT_EQ(*pendingError, std::optional<String>{});
  EXPECT_EQ(calls, 0);
  ASSERT_TRUE(keeper.fulfill(21));
  EXPECT_EQ(calls, 0);
  EXPECT_EQ(observer.finished(), true);
  EXPECT_EQ(wrapped.succeeded(), true);
  EXPECT_EQ(observer.failed(), false);
  auto wrappedResult = wrapped.result();
  ASSERT_TRUE(wrappedResult);
  EXPECT_EQ(*wrappedResult, std::optional<String>{"21"});
  auto observerError = observer.error();
  ASSERT_TRUE(observerError);
  EXPECT_EQ(*observerError, std::optional<String>{});
  auto observerResult = observer.result();
  ASSERT_TRUE(observerResult);
  EXPECT_EQ(*observerResult, std::optional<String>{"21"});
  EXPECT_EQ(calls, 1);
}

TEST(RpcPromiseTest, FailingWrapperRemainsQueryable) {
  auto source = RpcPromise<int>::createFulfilled(8);
  int calls = 0;
  auto wrapping = source.wrap([&calls](int value) -> std::expected<int, StarException> {
    if (++calls < 3)
      return std::unexpected(StarException("wrapper failed"));
    return value * 2;
  });
  ASSERT_TRUE(wrapping);
  auto wrapped = *wrapping;
  auto observer = wrapped;
  auto finished = wrapped.finished();
  ASSERT_FALSE(finished);
  EXPECT_TRUE(finished.error().is<StarException>());
  EXPECT_NE(std::string_view(finished.error().what()).find("wrapper failed"), std::string_view::npos);
  auto error = observer.error();
  ASSERT_FALSE(error);
  EXPECT_TRUE(error.error().is<StarException>());
  EXPECT_NE(std::string_view(error.error().what()).find("wrapper failed"), std::string_view::npos);
  EXPECT_EQ(calls, 2);
  auto recoveredResult = observer.result();
  ASSERT_TRUE(recoveredResult);
  EXPECT_EQ(*recoveredResult, std::optional<int>{16});
  EXPECT_EQ(wrapped.succeeded(), true);
  EXPECT_EQ(wrapped.failed(), false);
  EXPECT_EQ(calls, 3);
}

TEST(RpcPromiseTest, WrappedFailureAndWrappedDelegation) {
  int calls = 0;
  auto [source, keeper] = RpcPromise<int, int>::createPair();
  auto wrapped = source.wrap([&calls](int value) -> std::expected<int, StarException> {
    ++calls;
    return value + 1;
  });
  ASSERT_TRUE(wrapped);
  auto [outer, outerKeeper] = RpcPromise<int, int>::createPair();
  ASSERT_TRUE(outerKeeper.chain(*wrapped));
  EXPECT_EQ(outer.finished(), false);
  ASSERT_TRUE(keeper.fail(19));
  EXPECT_EQ(outer.finished(), true);
  EXPECT_EQ(outer.failed(), true);
  auto failedResult = outer.result();
  ASSERT_TRUE(failedResult);
  EXPECT_EQ(*failedResult, std::optional<int>{});
  auto failedError = outer.error();
  ASSERT_TRUE(failedError);
  EXPECT_EQ(*failedError, std::optional<int>{19});
  EXPECT_EQ(calls, 0);

  auto [success, successKeeper] = RpcPromise<int, int>::createPair();
  auto transformed = success.wrap([&calls](int value) -> std::expected<int, StarException> {
    ++calls;
    return value + 1;
  });
  ASSERT_TRUE(transformed);
  auto [delegated, delegatedKeeper] = RpcPromise<int, int>::createPair();
  ASSERT_TRUE(delegatedKeeper.chain(*transformed));
  ASSERT_TRUE(successKeeper.fulfill(10));
  EXPECT_EQ(calls, 0);
  auto delegatedResult = delegated.result();
  ASSERT_TRUE(delegatedResult);
  EXPECT_EQ(*delegatedResult, std::optional<int>{11});
  EXPECT_EQ(delegated.finished(), true);
  EXPECT_EQ(calls, 1);
}

TEST(RpcPromiseTest, ArbitraryPayloadTypesDoNotNeedDefaultConstruction) {
  auto [promise, keeper] = RpcPromise<NonDefaultValue, NonDefaultValue>::createPair();
  ASSERT_TRUE(keeper.fulfill(NonDefaultValue(7)));
  auto result = promise.result();
  ASSERT_TRUE(result);
  ASSERT_TRUE(*result);
  EXPECT_EQ((*result)->value, 7);
  auto noError = promise.error();
  ASSERT_TRUE(noError);
  EXPECT_FALSE(*noError);
  auto failed = RpcPromise<NonDefaultValue, NonDefaultValue>::createFailed(NonDefaultValue(12));
  EXPECT_EQ(failed.failed(), true);
  auto error = failed.error();
  ASSERT_TRUE(error);
  ASSERT_TRUE(*error);
  EXPECT_EQ((*error)->value, 12);

  auto [moveOnly, moveOnlyKeeper] = RpcPromise<std::unique_ptr<int>, std::unique_ptr<int>>::createPair();
  ASSERT_TRUE(moveOnlyKeeper.fulfill(std::make_unique<int>(3)));
  EXPECT_EQ(moveOnly.succeeded(), true);
  auto moveOnlyFailure = RpcPromise<std::unique_ptr<int>, std::unique_ptr<int>>::createFailed(std::make_unique<int>(4));
  EXPECT_EQ(moveOnlyFailure.failed(), true);
}

TEST(RpcPromiseTest, ConcurrentKeepersChooseExactlyOneOutcome) {
  auto [promise, keeper] = RpcPromise<int, int>::createPair();
  std::barrier start(3);
  std::atomic<int> rejected = 0;
  auto fulfillKeeper = keeper;
  auto failKeeper = keeper;
  std::thread fulfill([&] {
    start.arrive_and_wait();
    auto decision = fulfillKeeper.fulfill(5);
    if (!decision) {
      EXPECT_TRUE(decision.error().is<RpcPromiseException>());
      ++rejected;
    }
  });
  std::thread fail([&] {
    start.arrive_and_wait();
    auto decision = failKeeper.fail(6);
    if (!decision) {
      EXPECT_TRUE(decision.error().is<RpcPromiseException>());
      ++rejected;
    }
  });
  start.arrive_and_wait();
  fulfill.join();
  fail.join();
  EXPECT_EQ(rejected.load(), 1);
  EXPECT_EQ(promise.finished(), true);
  auto succeeded = promise.succeeded();
  auto failed = promise.failed();
  ASSERT_TRUE(succeeded);
  ASSERT_TRUE(failed);
  EXPECT_NE(*succeeded, *failed);
  if (*succeeded) {
    auto result = promise.result();
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, std::optional<int>{5});
  } else {
    auto error = promise.error();
    ASSERT_TRUE(error);
    EXPECT_EQ(*error, std::optional<int>{6});
  }
}

TEST(RpcPromiseTest, AutoFailOwnershipAndExplicitDecisions) {
  auto [abandoned, keeper] = RpcPromise<int>::createPair();
  auto automatic = std::make_shared<AutoFailRpcPromiseKeeper<int>>(keeper);
  auto alias = automatic;
  automatic.reset();
  EXPECT_EQ(abandoned.finished(), false);
  alias.reset();
  EXPECT_EQ(abandoned.failed(), true);
  auto abandonedError = abandoned.error();
  ASSERT_TRUE(abandonedError);
  EXPECT_EQ(*abandonedError, std::optional<String>{"Keeper destroyed"});

  auto [fulfilled, fulfilledKeeper] = RpcPromise<int>::createPair();
  {
    AutoFailRpcPromiseKeeper<int> automaticKeeper(fulfilledKeeper);
    ASSERT_TRUE(automaticKeeper.fulfill(10));
    auto second = automaticKeeper.fail("second decision");
    ASSERT_FALSE(second);
    EXPECT_TRUE(second.error().is<RpcPromiseException>());
  }
  auto fulfilledResult = fulfilled.result();
  ASSERT_TRUE(fulfilledResult);
  EXPECT_EQ(*fulfilledResult, std::optional<int>{10});

  auto [failed, failedKeeper] = RpcPromise<int>::createPair();
  {
    AutoFailRpcPromiseKeeper<int> automaticKeeper(failedKeeper);
    ASSERT_TRUE(automaticKeeper.fail("explicit failure"));
  }
  auto failedError = failed.error();
  ASSERT_TRUE(failedError);
  EXPECT_EQ(*failedError, std::optional<String>{"explicit failure"});

  auto [delegated, delegatedKeeper] = RpcPromise<int>::createPair();
  auto [target, targetKeeper] = RpcPromise<int>::createPair();
  {
    AutoFailRpcPromiseKeeper<int> automaticKeeper(delegatedKeeper);
    ASSERT_TRUE(automaticKeeper.chain(target));
  }
  EXPECT_EQ(delegated.finished(), false);
  ASSERT_TRUE(targetKeeper.fulfill(17));
  auto delegatedResult = delegated.result();
  ASSERT_TRUE(delegatedResult);
  EXPECT_EQ(*delegatedResult, std::optional<int>{17});
}

TEST(RpcPromiseTest, WrapperRetainsCaptureUntilLastObserverDies) {
  std::weak_ptr<int> capture;
  {
    auto resource = std::make_shared<int>(9);
    capture = resource;
    auto wrapping = RpcPromise<int>::createFulfilled(2).wrap([resource](int value) -> std::expected<int, StarException> { return value + *resource; });
    ASSERT_TRUE(wrapping);
    auto wrapped = std::move(*wrapping);
    resource.reset();
    {
      auto observer = wrapped;
      auto result = observer.result();
      ASSERT_TRUE(result);
      EXPECT_EQ(*result, std::optional<int>{11});
    }
    EXPECT_FALSE(capture.expired());
  }
  EXPECT_TRUE(capture.expired());
}

TEST(RpcPromiseTest, EmptyHandlesReportDomainErrors) {
  RpcPromise<int> promise;
  RpcPromiseKeeper<int> keeper;
  auto finished = promise.finished();
  ASSERT_FALSE(finished);
  EXPECT_TRUE(finished.error().is<RpcPromiseException>());
  auto result = promise.result();
  ASSERT_FALSE(result);
  EXPECT_TRUE(result.error().is<RpcPromiseException>());
  auto fulfilled = keeper.fulfill(1);
  ASSERT_FALSE(fulfilled);
  EXPECT_TRUE(fulfilled.error().is<RpcPromiseException>());
  auto failed = keeper.fail("error");
  ASSERT_FALSE(failed);
  EXPECT_TRUE(failed.error().is<RpcPromiseException>());
}

TEST(JsonRpcTest, RemoteFailureStillFulfillsNull) {
  Logger logger;
  JsonRpc sender(logger);
  auto response = sender.invokeRemote("remote", JsonArray{1, 2});
  ASSERT_TRUE(response);
  EXPECT_EQ(response->finished(), false);
  auto request = sender.send();
  ASSERT_TRUE(request);
  DataStreamBuffer requestBuffer(std::move(*request));
  List<Json> requests;
  ASSERT_TRUE(requestBuffer.readContainer(requests));
  ASSERT_EQ(requests.size(), 1u);
  EXPECT_EQ(requests[0].getString("handler"), "remote");
  auto id = requests[0].get("id");
  ASSERT_TRUE(id);
  DataStreamBuffer failBuffer;
  ASSERT_TRUE(failBuffer.writeContainer(List<Json>{JsonObject{{"command", "fail"}, {"id", *id}}}));
  ASSERT_TRUE(sender.receive(failBuffer.takeData()));
  EXPECT_EQ(response->finished(), true);
  EXPECT_EQ(response->succeeded(), true);
  EXPECT_EQ(response->failed(), false);
  auto result = response->result();
  ASSERT_TRUE(result);
  ASSERT_TRUE(*result);
  EXPECT_TRUE((*result)->isNull());
  auto error = response->error();
  ASSERT_TRUE(error);
  EXPECT_EQ(*error, std::optional<String>{});
}

TEST(JsonRpcTest, SuccessfulTransportPreservesStructuredResult) {
  Logger logger;
  JsonRpc sender(logger);
  JsonRpc receiver(logger);
  ASSERT_TRUE(receiver.registerHandler("echo", [](Json const& arguments) -> std::expected<Json, StarException> { return arguments; }));
  Json arguments = JsonObject{{"message", "round trip"}, {"values", JsonArray{1, false, Json()}}};
  auto response = sender.invokeRemote("echo", arguments);
  ASSERT_TRUE(response);
  auto request = sender.send();
  ASSERT_TRUE(request);
  ASSERT_TRUE(receiver.receive(*request));
  EXPECT_EQ(response->finished(), false);
  auto reply = receiver.send();
  ASSERT_TRUE(reply);
  ASSERT_TRUE(sender.receive(*reply));
  EXPECT_EQ(response->succeeded(), true);
  auto result = response->result();
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, std::optional<Json>{arguments});
  auto error = response->error();
  ASSERT_TRUE(error);
  EXPECT_EQ(*error, std::optional<String>{});
}

TEST(JsonRpcTest, TruncatedContainerDoesNotDispatchItsCompleteFirstRequest) {
  Logger logger;
  JsonRpc sender(logger);
  JsonRpc receiver(logger);
  int calls = 0;
  ASSERT_TRUE(receiver.registerHandler("echo", [&calls](Json const& arguments) -> std::expected<Json, StarException> {
    ++calls;
    return arguments;
  }));
  Json firstArguments = JsonObject{{"message", "first"}, {"values", JsonArray{1, false, Json()}}};
  Json secondArguments = JsonObject{{"message", "second"}};
  auto firstResponse = sender.invokeRemote("echo", firstArguments);
  ASSERT_TRUE(firstResponse);
  auto secondResponse = sender.invokeRemote("echo", secondArguments);
  ASSERT_TRUE(secondResponse);
  auto request = sender.send();
  ASSERT_TRUE(request);
  ASSERT_FALSE(request->empty());
  auto truncated = *request;
  truncated.resize(truncated.size() - 1);

  EXPECT_FALSE(receiver.receive(truncated));
  EXPECT_EQ(calls, 0);
  EXPECT_FALSE(receiver.sendPending());
  EXPECT_EQ(firstResponse->finished(), false);
  EXPECT_EQ(secondResponse->finished(), false);

  ASSERT_TRUE(receiver.receive(*request));
  EXPECT_EQ(calls, 2);
  auto reply = receiver.send();
  ASSERT_TRUE(reply);
  ASSERT_TRUE(sender.receive(*reply));
  auto firstResult = firstResponse->result();
  ASSERT_TRUE(firstResult);
  EXPECT_EQ(*firstResult, std::optional<Json>{firstArguments});
  auto secondResult = secondResponse->result();
  ASSERT_TRUE(secondResult);
  EXPECT_EQ(*secondResult, std::optional<Json>{secondArguments});
}

TEST(JsonRpcTest, HandlerRegistrationPreservesActiveCallableState) {
  Logger logger;
  JsonRpc sender(logger);
  JsonRpc receiver(logger);
  ASSERT_TRUE(receiver.registerHandler(
    "grow", [&receiver, visits = 0](Json const&) mutable -> std::expected<Json, StarException> {
      if (++visits == 1) {
        for (int i = 0; i < 256; ++i) {
          auto added = receiver.registerHandler(
            strf("generated-{}", i), [i](Json const&) -> std::expected<Json, StarException> { return Json(i); });
          if (!added)
            return std::unexpected(std::move(added.error()));
        }
      }
      return Json(++visits);
    }));

  for (int expected : {2, 4}) {
    auto response = sender.invokeRemote("grow", Json());
    ASSERT_TRUE(response);
    auto request = sender.send();
    ASSERT_TRUE(request);
    ASSERT_TRUE(receiver.receive(*request));
    auto reply = receiver.send();
    ASSERT_TRUE(reply);
    ASSERT_TRUE(sender.receive(*reply));
    auto result = response->result();
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, std::optional<Json>{Json(expected)});
  }

  auto response = sender.invokeRemote("generated-255", Json());
  ASSERT_TRUE(response);
  auto request = sender.send();
  ASSERT_TRUE(request);
  ASSERT_TRUE(receiver.receive(*request));
  auto reply = receiver.send();
  ASSERT_TRUE(reply);
  ASSERT_TRUE(sender.receive(*reply));
  auto result = response->result();
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, std::optional<Json>{Json(255)});
}

TEST(RpcPromiseTest, ConcurrentObserversShareOneLazyEvaluation) {
  std::atomic<int> calls = 0;
  auto wrapped = RpcPromise<int>::createFulfilled(7).wrap([&calls](int value) -> std::expected<int, StarException> {
    ++calls;
    return value * 3;
  });
  ASSERT_TRUE(wrapped);
  std::barrier start(5);
  std::atomic<int> observed = 0;
  std::vector<std::thread> observers;
  for (int i = 0; i != 4; ++i) {
    observers.emplace_back([observer = *wrapped, &start, &observed] {
      start.arrive_and_wait();
      auto result = observer.result();
      ASSERT_TRUE(result);
      ASSERT_TRUE(*result);
      EXPECT_EQ(**result, 21);
      if (**result == 21)
        ++observed;
    });
  }
  start.arrive_and_wait();
  for (auto& observer : observers)
    observer.join();
  EXPECT_EQ(observed.load(), 4);
  EXPECT_EQ(calls.load(), 1);
}

TEST(RpcPromiseTest, WrapperCachesOwnedValue) {
  int value = 13;
  auto wrapped = RpcPromise<int>::createFulfilled(1).wrap([&value](int) -> std::expected<int, StarException> {
    return value;
  });
  ASSERT_TRUE(wrapped);
  auto initialResult = wrapped->result();
  ASSERT_TRUE(initialResult);
  EXPECT_EQ(*initialResult, std::optional<int>{13});
  value = 24;
  auto cachedResult = wrapped->result();
  ASSERT_TRUE(cachedResult);
  EXPECT_EQ(*cachedResult, std::optional<int>{13});
}

TEST(RpcPromiseTest, CancellationIsTerminalAndDiscardsLateProducerDecisions) {
  auto [promise, keeper] = RpcPromise<int, int>::createPair();
  EXPECT_EQ(promise.status(), RpcPromiseStatus::Pending);
  EXPECT_EQ(keeper.cancel(17), true);
  EXPECT_EQ(promise.status(), RpcPromiseStatus::Cancelled);
  EXPECT_EQ(promise.finished(), true);
  EXPECT_EQ(promise.succeeded(), false);
  EXPECT_EQ(promise.failed(), true);
  EXPECT_EQ(promise.cancelled(), true);
  auto error = promise.error();
  ASSERT_TRUE(error);
  EXPECT_EQ(*error, (std::optional<int>{17}));
  auto result = promise.result();
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, (std::optional<int>{}));
  EXPECT_EQ(promise.cancel(19), false);
  EXPECT_EQ(keeper.cancel(20), false);
  ASSERT_TRUE(keeper.fulfill(1));
  ASSERT_TRUE(keeper.fail(2));
  ASSERT_TRUE(keeper.chain(RpcPromise<int, int>{}));
  error = promise.error();
  ASSERT_TRUE(error);
  EXPECT_EQ(*error, (std::optional<int>{17}));

  EXPECT_EQ((RpcPromise<int, int>::createFulfilled(3).cancel(9)), false);
  EXPECT_EQ((RpcPromise<int, int>::createFailed(4).cancel(9)), false);
  auto [delegated, delegatedKeeper] = RpcPromise<int, int>::createPair();
  auto [target, targetKeeper] = RpcPromise<int, int>::createPair();
  ASSERT_TRUE(delegatedKeeper.chain(target));
  EXPECT_EQ(delegated.cancel(8), false);
  EXPECT_EQ(targetKeeper.cancel(7), true);
  EXPECT_EQ(delegated.status(), RpcPromiseStatus::Cancelled);

  RpcPromise<int> empty;
  RpcPromiseKeeper<int> emptyKeeper;
  EXPECT_FALSE(empty.status());
  EXPECT_FALSE(empty.cancel("closed"));
  EXPECT_FALSE(emptyKeeper.cancel("closed"));
  EXPECT_FALSE(empty.withCancellation(std::stop_token{}, "closed"));
  auto [automaticPromise, automaticKeeper] = RpcPromise<int>::createPair();
  {
    AutoFailRpcPromiseKeeper<int> automatic(automaticKeeper);
    EXPECT_EQ(automatic.cancel("closed"), true);
  }
  EXPECT_EQ(automaticPromise.status(), RpcPromiseStatus::Cancelled);
  auto automaticError = automaticPromise.error();
  ASSERT_TRUE(automaticError);
  EXPECT_EQ(*automaticError, (std::optional<String>{"closed"}));
}

TEST(RpcPromiseTest, CancellationBeforeCompletionKeepsNativeDecision) {
  auto [promise, keeper] = RpcPromise<int, int>::createPair();
  EXPECT_EQ(promise.cancel(9), true);
  EXPECT_TRUE(keeper.fulfill(42));
  EXPECT_EQ(promise.status(), RpcPromiseStatus::Cancelled);
}

TEST(RpcPromiseTest, CompletionBeforeCancellationKeepsNativeDecision) {
  auto [promise, keeper] = RpcPromise<int, int>::createPair();
  EXPECT_TRUE(keeper.fulfill(42));
  EXPECT_EQ(promise.cancel(9), false);
  EXPECT_EQ(promise.status(), RpcPromiseStatus::Succeeded);
}

TEST(RpcPromiseTest, ScopedChainedViewCancelsWithoutChangingSourceObservers) {
  auto [source, sourceKeeper] = RpcPromise<int>::createPair();
  auto [chained, chainedKeeper] = RpcPromise<int>::createPair();
  ASSERT_TRUE(chainedKeeper.chain(source));
  std::stop_source lifetime;
  auto scoped = chained.withCancellation(lifetime.get_token(), "closed");
  ASSERT_TRUE(scoped);
  auto reused = scoped->withCancellation(lifetime.get_token(), "different reason");
  ASSERT_TRUE(reused);
  auto independent = chained.withCancellation(lifetime.get_token(), "independent");
  ASSERT_TRUE(independent);
  EXPECT_EQ(scoped->cancel("manual"), true);
  auto error = reused->error();
  ASSERT_TRUE(error);
  EXPECT_EQ(*error, (std::optional<String>{"manual"}));
  EXPECT_EQ(independent->status(), RpcPromiseStatus::Pending);
  EXPECT_EQ(chained.status(), RpcPromiseStatus::Pending);
  lifetime.request_stop();
  EXPECT_EQ(independent->status(), RpcPromiseStatus::Cancelled);
  EXPECT_EQ(source.status(), RpcPromiseStatus::Pending);
  ASSERT_TRUE(sourceKeeper.fulfill(51));
  auto result = source.result();
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, (std::optional<int>{51}));
  auto chainedResult = chained.result();
  ASSERT_TRUE(chainedResult);
  EXPECT_EQ(*chainedResult, (std::optional<int>{51}));
  EXPECT_EQ(scoped->status(), RpcPromiseStatus::Cancelled);
  EXPECT_EQ(independent->status(), RpcPromiseStatus::Cancelled);
}

TEST(RpcPromiseTest, ScopedCancellationWinsBeforePublicationButNotAfter) {
  struct Progress {
    std::latch& entered;
    std::latch& resume;
    std::atomic<int>& cancelled;
    std::expected<void, StarException> poll() {
      entered.count_down();
      resume.wait();
      return {};
    }
    std::expected<void, StarException> cancel() {
      ++cancelled;
      return {};
    }
  };
  auto [source, keeper] = RpcPromise<int>::createPair();
  std::stop_source lifetime;
  std::latch entered(1), resume(1);
  std::atomic<int> cancelled = 0;
  auto view = source.withCancellation(lifetime.get_token(), "closed", Progress{entered, resume, cancelled});
  ASSERT_TRUE(view);
  ASSERT_TRUE(keeper.fulfill(22));
  std::thread observer([&] { EXPECT_EQ(view->status(), RpcPromiseStatus::Cancelled); });
  entered.wait();
  lifetime.request_stop();
  EXPECT_EQ(cancelled.load(), 1);
  EXPECT_EQ(view->cancel("again"), false);
  resume.count_down();
  observer.join();
  auto result = source.result();
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, (std::optional<int>{22}));

  auto [completedSource, completedKeeper] = RpcPromise<int>::createPair();
  std::stop_source completedLifetime;
  auto completedView = completedSource.withCancellation(completedLifetime.get_token(), "closed");
  ASSERT_TRUE(completedView);
  ASSERT_TRUE(completedKeeper.fulfill(23));
  EXPECT_EQ(completedView->status(), RpcPromiseStatus::Succeeded);
  completedLifetime.request_stop();
  auto completedResult = completedView->result();
  ASSERT_TRUE(completedResult);
  EXPECT_EQ(*completedResult, (std::optional<int>{23}));
}

TEST(RpcPromiseTest, ScopedProgressMayReenterCancellationAndQueries) {
  struct Progress {
    RpcPromise<int>& view;
    int& cancellations;
    std::expected<void, StarException> poll() {
      auto cancelled = view.cancel("poll cancelled");
      if (!cancelled)
        return std::unexpected(std::move(cancelled.error()));
      return {};
    }
    std::expected<void, StarException> cancel() {
      ++cancellations;
      EXPECT_EQ(view.status(), RpcPromiseStatus::Cancelled);
      EXPECT_EQ(view.cancel("reentrant"), false);
      return {};
    }
  };
  int conversions = 0;
  auto source = RpcPromise<int>::createFulfilled(5).wrap([&](int value) -> std::expected<int, StarException> {
    ++conversions;
    return value;
  });
  ASSERT_TRUE(source);
  RpcPromise<int> view;
  int cancellations = 0;
  auto scoped = source->withCancellation(std::stop_token{}, "closed", Progress{view, cancellations});
  ASSERT_TRUE(scoped);
  view = *scoped;
  EXPECT_EQ(view.status(), RpcPromiseStatus::Cancelled);
  EXPECT_EQ(cancellations, 1);
  EXPECT_EQ(conversions, 0);
  EXPECT_EQ(source->status(), RpcPromiseStatus::Succeeded);
  EXPECT_EQ(conversions, 1);
}

TEST(RpcPromiseTest, StopCallbackPinsStateWhileHookDropsLastObservation) {
  struct Progress {
    RpcPromise<int>& observation;
    std::weak_ptr<int>& capture;
    std::shared_ptr<int> owned;
    int& cancellations;
    std::expected<void, StarException> poll() { return {}; }
    std::expected<void, StarException> cancel() {
      observation = {};
      EXPECT_FALSE(capture.expired());
      EXPECT_EQ(*owned, 7);
      ++cancellations;
      return {};
    }
  };
  auto [source, keeper] = RpcPromise<int>::createPair();
  std::stop_source lifetime;
  RpcPromise<int> observation;
  std::weak_ptr<int> capture;
  int cancellations = 0;
  {
    auto owned = std::make_shared<int>(7);
    capture = owned;
    auto scoped = source.withCancellation(lifetime.get_token(), "closed",
                                          Progress{observation, capture, std::move(owned), cancellations});
    ASSERT_TRUE(scoped);
    observation = std::move(*scoped);
  }
  EXPECT_FALSE(capture.expired());
  lifetime.request_stop();
  EXPECT_EQ(cancellations, 1);
  EXPECT_TRUE(capture.expired());
  EXPECT_EQ(source.status(), RpcPromiseStatus::Pending);
  ASSERT_TRUE(keeper.fulfill(6));
}

TEST(RpcPromiseTest, DroppingScopedObservationUnregistersWithoutCancellingOperation) {
  struct Progress {
    int& cancellations;
    std::expected<void, StarException> poll() { return {}; }
    std::expected<void, StarException> cancel() {
      ++cancellations;
      return {};
    }
  };
  auto [source, keeper] = RpcPromise<int>::createPair();
  std::stop_source lifetime;
  int cancellations = 0;
  {
    auto view = source.withCancellation(lifetime.get_token(), "closed", Progress{cancellations});
    ASSERT_TRUE(view);
  }
  lifetime.request_stop();
  EXPECT_EQ(cancellations, 0);
  EXPECT_EQ(source.status(), RpcPromiseStatus::Pending);
  ASSERT_TRUE(keeper.fulfill(3));
}

TEST(RpcPromiseTest, ScopedNullAndNonDefaultPayloadsRemainCachedWithoutStatusCopies) {
  std::stop_source stopped;
  stopped.request_stop();
  auto terminal = RpcPromise<Json>::createFulfilled(Json()).withCancellation(stopped.get_token(), "closed");
  ASSERT_TRUE(terminal);
  EXPECT_EQ(terminal->status(), RpcPromiseStatus::Succeeded);
  auto null = terminal->result();
  ASSERT_TRUE(null);
  ASSERT_TRUE(*null);
  EXPECT_TRUE((*null)->isNull());

  auto [source, keeper] = RpcPromise<Json>::createPair();
  std::stop_source lifetime;
  auto view = source.withCancellation(lifetime.get_token(), "closed");
  ASSERT_TRUE(view);
  ASSERT_TRUE(keeper.fulfill(Json()));
  auto result = view->result();
  ASSERT_TRUE(result);
  ASSERT_TRUE(*result);
  EXPECT_TRUE((*result)->isNull());
  lifetime.request_stop();
  EXPECT_EQ(view->status(), RpcPromiseStatus::Succeeded);

  auto [nonDefault, nonDefaultKeeper] = RpcPromise<NonDefaultValue, NonDefaultValue>::createPair();
  std::stop_source nonDefaultLifetime;
  auto nonDefaultView = nonDefault.withCancellation(nonDefaultLifetime.get_token(), NonDefaultValue(12));
  ASSERT_TRUE(nonDefaultView);
  nonDefaultLifetime.request_stop();
  EXPECT_EQ(nonDefaultView->status(), RpcPromiseStatus::Cancelled);
  auto reason = nonDefaultView->error();
  ASSERT_TRUE(reason);
  ASSERT_TRUE(*reason);
  EXPECT_EQ((**reason).value, 12);
  ASSERT_TRUE(nonDefaultKeeper.fulfill(NonDefaultValue(13)));
  EXPECT_EQ(nonDefault.status(), RpcPromiseStatus::Succeeded);
  auto completedNonDefault = nonDefault.withCancellation(stopped.get_token(), NonDefaultValue(20));
  ASSERT_TRUE(completedNonDefault);
  EXPECT_EQ(completedNonDefault->status(), RpcPromiseStatus::Succeeded);
  auto cachedNonDefault = completedNonDefault->result();
  ASSERT_TRUE(cachedNonDefault);
  ASSERT_TRUE(*cachedNonDefault);
  EXPECT_EQ((**cachedNonDefault).value, 13);

  auto [moveOnly, moveOnlyKeeper] = RpcPromise<std::unique_ptr<int>, std::unique_ptr<int>>::createPair();
  std::stop_source moveOnlyLifetime;
  auto moveOnlyView = moveOnly.withCancellation(moveOnlyLifetime.get_token(), std::make_unique<int>(9));
  ASSERT_TRUE(moveOnlyView);
  ASSERT_TRUE(moveOnlyKeeper.fulfill(std::make_unique<int>(2)));
  EXPECT_EQ(moveOnlyView->status(), RpcPromiseStatus::Succeeded);
  moveOnlyLifetime.request_stop();
  EXPECT_EQ(moveOnlyView->status(), RpcPromiseStatus::Succeeded);
}

TEST(RpcPromiseTest, WrapperSupportsReentrantCancellationAndPropagatesCancelledSources) {
  RpcPromise<int> observation;
  int calls = 0;
  auto wrapper = RpcPromise<int>::createFulfilled(1).wrap([&](int) -> std::expected<int, StarException> {
    ++calls;
    EXPECT_EQ(observation.cancel("inside transform"), true);
    return std::unexpected(StarException("discarded diagnostic"));
  });
  ASSERT_TRUE(wrapper);
  observation = *wrapper;
  EXPECT_EQ(observation.status(), RpcPromiseStatus::Cancelled);
  auto error = observation.error();
  ASSERT_TRUE(error);
  EXPECT_EQ(*error, (std::optional<String>{"inside transform"}));
  EXPECT_EQ(calls, 1);

  auto [source, keeper] = RpcPromise<int, int>::createPair();
  auto propagated = source.wrap([](int value) -> std::expected<int, StarException> { return value; });
  ASSERT_TRUE(propagated);
  EXPECT_EQ(keeper.cancel(4), true);
  EXPECT_EQ(propagated->status(), RpcPromiseStatus::Cancelled);
  auto propagatedError = propagated->error();
  ASSERT_TRUE(propagatedError);
  EXPECT_EQ(*propagatedError, (std::optional<int>{4}));
}

TEST(RpcPromiseTest, SingleFlightCancellationWakesObserversAndDiscardsLateTransform) {
  std::latch entered(1), resume(1), observerStarted(1), observerFinished(1);
  std::atomic<int> calls = 0;
  auto wrapped = RpcPromise<int>::createFulfilled(7).wrap([&](int value) -> std::expected<int, StarException> {
    ++calls;
    entered.count_down();
    resume.wait();
    return value * 2;
  });
  ASSERT_TRUE(wrapped);
  std::thread evaluator([&] { EXPECT_EQ(wrapped->status(), RpcPromiseStatus::Cancelled); });
  entered.wait();
  std::thread observer([&] {
    observerStarted.count_down();
    EXPECT_EQ(wrapped->status(), RpcPromiseStatus::Cancelled);
    observerFinished.count_down();
  });
  observerStarted.wait();
  EXPECT_EQ(wrapped->cancel("closed"), true);
  observerFinished.wait();
  EXPECT_EQ(calls.load(), 1);
  resume.count_down();
  evaluator.join();
  observer.join();
  auto result = wrapped->result();
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, (std::optional<int>{}));
  auto error = wrapped->error();
  ASSERT_TRUE(error);
  EXPECT_EQ(*error, (std::optional<String>{"closed"}));
}

TEST(RpcPromiseTest, AlreadyStoppedLifetimeEagerlyCancelsOnlyPendingViews) {
  struct Progress {
    int& polls;
    int& cancellations;
    std::expected<void, StarException> poll() {
      ++polls;
      return {};
    }
    std::expected<void, StarException> cancel() {
      ++cancellations;
      return {};
    }
  };
  std::stop_source lifetime;
  lifetime.request_stop();
  int polls = 0, cancellations = 0;
  auto [source, keeper] = RpcPromise<int>::createPair();
  auto view = source.withCancellation(lifetime.get_token(), "closed", Progress{polls, cancellations});
  ASSERT_TRUE(view);
  EXPECT_EQ(cancellations, 1);
  EXPECT_EQ(polls, 0);
  EXPECT_EQ(view->status(), RpcPromiseStatus::Cancelled);
  EXPECT_EQ(source.status(), RpcPromiseStatus::Pending);
  ASSERT_TRUE(keeper.fulfill(8));
  auto terminal = source.withCancellation(lifetime.get_token(), "closed", Progress{polls, cancellations});
  ASSERT_TRUE(terminal);
  EXPECT_EQ(terminal->status(), RpcPromiseStatus::Succeeded);
  EXPECT_EQ(cancellations, 1);
  EXPECT_EQ(polls, 0);
}
