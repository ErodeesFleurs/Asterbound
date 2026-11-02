#include "gtest/gtest-spi.h"
#include "gtest/gtest.h"

import std;
import star.format;
import star.algorithm;
import star.actor_movement_controller;
import star.file;
import star.game_test;
import star.game_types;
import star.json;
import star.input;
import star.input_event;
import star.map;
import star.unicode;
import star.lua;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_converters;
import star.lua_game_converters;
import star.lua_root;
import star.npc;
import star.npc_database;
import star.root;
import star.rpc_promise;
import star.scriptable_thread;
import star.string;
import star.vector;
import star.thread;
import star.world_server;
import star.world_lua_bindings;
import star.logging;

using namespace Star;
namespace Star {
struct SessionPromiseFactory {
  RpcPromise<Json> source;
};

template <> struct LuaUserDataMethods<SessionPromiseFactory> {
  static std::expected<LuaMethods<SessionPromiseFactory>, StarException> make() {
    LuaMethods<SessionPromiseFactory> methods;
    if (auto registered =
          methods.registerMethod("request", [](SessionPromiseFactory const& factory) { return factory.source; });
        !registered)
      return std::unexpected(std::move(registered.error()));
    return methods;
  }
};
}// namespace Star

namespace {

static_assert(!std::is_copy_constructible_v<LuaMessageScript>);
static_assert(!std::is_move_constructible_v<LuaMessageScript>);
static_assert(!std::is_move_constructible_v<LuaStoredEntityScript>);
static_assert(!std::is_move_constructible_v<LuaActorScript>);
static_assert(!std::is_move_constructible_v<LuaActionScript>);

void expectNoResponse(std::expected<std::optional<ChainableJsonMessageResponse>, StarException> response) {
  ASSERT_TRUE(response) << response.error().what();
  EXPECT_FALSE(*response);
}

void expectResponse(std::expected<std::optional<ChainableJsonMessageResponse>, StarException> response, Json const& expected) {
  ASSERT_TRUE(response) << response.error().what();
  ASSERT_TRUE(*response);
  ASSERT_TRUE(std::holds_alternative<Json>(**response));
  EXPECT_EQ(std::get<Json>(**response), expected);
}

std::expected<RpcPromise<Json>, std::string>
promiseResponse(std::expected<std::optional<ChainableJsonMessageResponse>, StarException> response) {
  if (!response)
    return std::unexpected(std::string(response.error().what()));
  if (!*response)
    return std::unexpected(std::string("Message returned no response"));
  auto promise = std::get_if<RpcPromise<Json>>(&**response);
  if (!promise)
    return std::unexpected(std::string("Message response is not a promise"));
  return std::move(*promise);
}

void expectPending(RpcPromise<Json> const& promise) {
  auto finished = promise.finished();
  ASSERT_TRUE(finished) << finished.error().what();
  EXPECT_FALSE(*finished);
}

void expectPromiseFailure(RpcPromise<Json> const& promise) {
  auto finished = promise.finished();
  ASSERT_TRUE(finished) << finished.error().what();
  EXPECT_TRUE(*finished);
  auto error = promise.error();
  ASSERT_TRUE(error) << error.error().what();
  ASSERT_TRUE(*error);
}

void expectPromiseStatus(RpcPromise<Json> const& promise, RpcPromiseStatus expected) {
  auto status = promise.status();
  ASSERT_TRUE(status) << status.error().what();
  EXPECT_EQ(*status, expected);
}

template <typename Function> auto expectLoggedFailure(Function&& function, std::string_view detail) {
  testing::TestPartResultArray errors;
  auto result = [&] {
    testing::ScopedFakeTestPartResultReporter capture(
      testing::ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD, &errors);
    return std::forward<Function>(function)();
  }();
  EXPECT_EQ(errors.size(), 1);
  for (int i = 0; i < errors.size(); ++i) {
    auto const& error = errors.GetTestPartResult(i);
    EXPECT_EQ(error.type(), testing::TestPartResult::kNonFatalFailure);
    auto message = std::string_view(error.message());
    EXPECT_NE(message.find(detail), std::string_view::npos);
  }
  return result;
}

template <typename Host>
void expectStorageLifecycle(Root& root, WorldServer& world) {
  Host component(root);
  auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
  JsonObject initial{{"total", 7}, {"saves", 0}};
  ASSERT_TRUE(component.setScriptStorage(initial));
  auto beforeInit = component.getScriptStorage();
  ASSERT_TRUE(beforeInit) << beforeInit.error().what();
  EXPECT_EQ(*beforeInit, initial);

  // Empty scripts still run the real host's context setup. In particular,
  // storage and self must exist before any script code accesses them.
  ASSERT_TRUE(component.init(world));
  ASSERT_TRUE(component.initialized());
  auto missing = component.template invoke<Json>("missingEntryPoint");
  ASSERT_TRUE(missing) << missing.error().what();
  EXPECT_FALSE(*missing);
  auto missingUpdate = component.template update<Json>(0.25f, 5);
  ASSERT_TRUE(missingUpdate) << missingUpdate.error().what();
  EXPECT_FALSE(*missingUpdate);

  auto defined = component.template eval<bool>(R"LUA(
    self.marker = "old context"
    function update(dt, amount)
      storage.total = storage.total + amount
      storage.lastDt = dt
      return {storage.total, dt}
    end
    function uninit()
      storage.saves = storage.saves + 1
    end
    return storage.total == 7 and storage.saves == 0
  )LUA");
  ASSERT_TRUE(defined) << defined.error().what();
  ASSERT_TRUE(*defined);
  EXPECT_TRUE(**defined);

  auto updated = component.template update<Json>(0.25f, 5);
  ASSERT_TRUE(updated) << updated.error().what();
  ASSERT_TRUE(*updated);
  EXPECT_EQ(**updated, Json(JsonArray{12, 0.25}));
  JsonObject live{{"total", 12}, {"saves", 0}, {"lastDt", 0.25}};
  auto liveStorage = component.getScriptStorage();
  ASSERT_TRUE(liveStorage) << liveStorage.error().what();
  EXPECT_EQ(*liveStorage, live);

  // uninit must execute before the storage snapshot and context teardown.
  ASSERT_TRUE(component.uninit());
  EXPECT_FALSE(component.initialized());
  JsonObject saved{{"total", 12}, {"saves", 1}, {"lastDt", 0.25}};
  auto stored = component.getScriptStorage();
  ASSERT_TRUE(stored) << stored.error().what();
  EXPECT_EQ(*stored, saved);
  auto stoppedUpdate = component.template update<Json>(0.25f, 5);
  ASSERT_TRUE(stoppedUpdate) << stoppedUpdate.error().what();
  EXPECT_FALSE(*stoppedUpdate);

  ASSERT_TRUE(component.init(world));
  auto restored = component.template eval<bool>(R"LUA(
    return storage.total == 12 and storage.saves == 1 and storage.lastDt == 0.25
      and self.marker == nil and update == nil and uninit == nil
  )LUA");
  ASSERT_TRUE(restored) << restored.error().what();
  ASSERT_TRUE(*restored);
  EXPECT_TRUE(**restored);
  ASSERT_TRUE(component.uninit());
  auto unchanged = component.getScriptStorage();
  ASSERT_TRUE(unchanged) << unchanged.error().what();
  EXPECT_EQ(*unchanged, saved);
}

const GameTestRegistration persistentStorage("LuaComponentLifecycle", "PersistentHostsPreserveStorageAcrossContexts", [](GameTest& fixture) {
  auto storage = File::ephemeralFile(fixture.root().random());
  ASSERT_TRUE(storage) << storage.error().what();
  auto created = WorldServer::create(fixture.root(), Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(created) << created.error().what();
  auto world = *created;
  auto close = finally([&] { EXPECT_TRUE(world->close()); });
  {
    SCOPED_TRACE("LuaStoredEntityScript");
    expectStorageLifecycle<LuaStoredEntityScript>(fixture.root(), *world);
  }
  {
    SCOPED_TRACE("LuaActorScript");
    expectStorageLifecycle<LuaActorScript>(fixture.root(), *world);
  }
  {
    SCOPED_TRACE("LuaActionScript");
    expectStorageLifecycle<LuaActionScript>(fixture.root(), *world);
  }
});

const GameTestRegistration messageLifecycle("LuaComponentLifecycle", "MessageArgumentsAndHandlersFollowContextLifetime", [](GameTest& fixture) {
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  unsigned calls = 0;
  LuaMessageScript component(fixture.root());
  auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
  ASSERT_TRUE(component.setLuaRoot(*luaRoot));
  LuaCallbacks observer;
  ASSERT_TRUE(observer.registerCallback("record", [&] { ++calls; }));
  ASSERT_TRUE(component.addCallbacks("observer", std::move(observer)));
  auto initialized = component.init();
  ASSERT_TRUE(initialized) << initialized.error().what();
  ASSERT_TRUE(*initialized);
  auto defined = component.eval<bool>(R"LUA(
    message.setHandler("legacy", function(name, localMessage, value, tail)
      observer.record()
      return {name, localMessage, value, tail}
    end)
    message.setHandler({name = "localUnnamed", localOnly = true}, function(value)
      observer.record()
      return value * 2
    end)
    message.setHandler({name = "localNamed", localOnly = true, passName = true}, function(name, value)
      observer.record()
      return {name, value}
    end)
    message.setHandler({name = "remoteUnnamed"}, function(localMessage, value)
      observer.record()
      return {localMessage, value}
    end)
    return true
  )LUA");
  ASSERT_TRUE(defined) << defined.error().what();
  ASSERT_TRUE(*defined);
  EXPECT_TRUE(**defined);

  expectNoResponse(component.handleMessage("localUnnamed", false, {7}));
  EXPECT_EQ(calls, 0u);
  expectResponse(component.handleMessage("localUnnamed", true, {7}), Json(14));
  expectResponse(component.handleMessage("localNamed", true, {9}), Json(JsonArray{"localNamed", 9}));
  expectResponse(component.handleMessage("remoteUnnamed", false, {11}), Json(JsonArray{false, 11}));
  expectResponse(component.handleMessage("legacy", true, {13, "tail"}), Json(JsonArray{"legacy", true, 13, "tail"}));
  EXPECT_EQ(calls, 4u);

  ASSERT_TRUE(component.uninit());
  expectNoResponse(component.handleMessage("legacy", true, {13, "tail"}));
  EXPECT_EQ(calls, 4u);
  auto reinitialized = component.init();
  ASSERT_TRUE(reinitialized) << reinitialized.error().what();
  ASSERT_TRUE(*reinitialized);
  // Checking after reinit catches retained LuaFunction handlers; checking only
  // while uninitialized would merely exercise the initialized() guard.
  for (std::string_view name : {"legacy", "localUnnamed", "localNamed", "remoteUnnamed"})
    expectNoResponse(component.handleMessage(name, true, {17, "unused"}));
  EXPECT_EQ(calls, 4u);
  auto registered = component.eval<bool>(R"LUA(
    message.setHandler({name = "replacement", localOnly = true}, function(value)
      observer.record()
      return value
    end)
    return true
  )LUA");
  ASSERT_TRUE(registered) << registered.error().what();
  ASSERT_TRUE(*registered);
  EXPECT_TRUE(**registered);
  expectResponse(component.handleMessage("replacement", true, {19}), Json(19));
  EXPECT_EQ(calls, 5u);
});

const GameTestRegistration threadMessages("LuaComponentLifecycle", "ThreadPromisesSettleOnUpdateUninitAndDestruction", [](GameTest& fixture) {
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  std::optional<RpcPromise<Json>> destroyed;
  {
    // Heap ownership fixes callback targets' addresses through queue teardown.
    auto component = std::make_unique<LuaMessageScript>(fixture.root());
    ASSERT_TRUE(component->setLuaRoot(*luaRoot));
    auto initialized = component->init();
    ASSERT_TRUE(initialized) << initialized.error().what();
    ASSERT_TRUE(*initialized);
    auto defined = component->eval<bool>(R"LUA(
      threads.setMessageHandler("echo", function(origin, name, value)
        return {origin, name, value}
      end)
      return true
    )LUA");
    ASSERT_TRUE(defined) << defined.error().what();
    ASSERT_TRUE(*defined);
    EXPECT_TRUE(**defined);
    auto handled = component->threadPassMessage("worker", "echo", {23});
    auto missing = component->threadPassMessage("worker", "missing", {});
    expectPending(handled);
    expectPending(missing);
    auto updated = component->update<Json>();
    ASSERT_TRUE(updated) << updated.error().what();
    EXPECT_FALSE(*updated);
    auto result = handled.result();
    ASSERT_TRUE(result) << result.error().what();
    ASSERT_TRUE(*result);
    EXPECT_EQ(**result, Json(JsonArray{"worker", "echo", 23}));
    expectPromiseFailure(missing);

    auto pending = component->threadPassMessage("worker", "echo", {29});
    expectPending(pending);
    ASSERT_TRUE(component->uninit());
    expectPromiseFailure(pending);
    auto reinitialized = component->init();
    ASSERT_TRUE(reinitialized) << reinitialized.error().what();
    ASSERT_TRUE(*reinitialized);
    auto oldHandler = component->threadPassMessage("worker", "echo", {31});
    auto afterReinit = component->update<Json>();
    ASSERT_TRUE(afterReinit) << afterReinit.error().what();
    EXPECT_FALSE(*afterReinit);
    expectPromiseFailure(oldHandler);
    destroyed.emplace(component->threadPassMessage("worker", "echo", {37}));
    expectPending(*destroyed);
  }
  ASSERT_TRUE(destroyed);
  expectPromiseFailure(*destroyed);
});

const GameTestRegistration reloadFirstMessage("LuaComponentLifecycle",
                                              "FirstMessageConsumesPendingReloadBeforeSelectingHandlers",
                                              [](GameTest& fixture) {
                                                auto luaRoot = LuaRoot::create(fixture.root());
                                                ASSERT_TRUE(luaRoot);
                                                LuaMessageScript component(fixture.root());
                                                auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
                                                ASSERT_TRUE(component.setLuaRoot(*luaRoot));
                                                ASSERT_TRUE(component.init());
                                                ASSERT_TRUE(component.eval<LuaValue>(R"LUA(
    self.retired = true
    message.setHandler("old", function() error("retired ordinary handler") end)
    threads.setMessageHandler("old", function() error("retired thread handler") end)
  )LUA"));
                                                auto queued = component.threadPassMessage("worker", "old", {});
                                                ASSERT_TRUE((*luaRoot)->restart());
                                                // No invoke/eval/update may be needed to make ordinary dispatch reload-safe.
                                                expectNoResponse(component.handleMessage("old", true));
                                                expectPromiseFailure(queued);
                                                auto fresh = component.eval<bool>("return self.retired == nil");
                                                ASSERT_TRUE(fresh);
                                                EXPECT_TRUE(fresh->value_or(false));
                                                auto afterReload = component.threadPassMessage("worker", "old", {});
                                                ASSERT_TRUE(component.update<LuaValue>());
                                                expectPromiseFailure(afterReload);
                                                ASSERT_TRUE(component.eval<LuaValue>(R"LUA(
    threads.setMessageHandler("old", function() error("retired first thread handler") end)
  )LUA"));
                                                auto firstThreadMessage =
                                                  component.threadPassMessage("worker", "old", {});
                                                ASSERT_TRUE((*luaRoot)->restart());
                                                ASSERT_TRUE(component.update<LuaValue>());
                                                expectPromiseFailure(firstThreadMessage);
                                              });

const GameTestRegistration
  reentrantMessageReload("LuaComponentLifecycle", "NativeReloadAndHandlerMutationRetainOnlyActiveFrame",
                         [](GameTest& fixture) {
                           for (bool threadMessage : {false, true}) {
                             SCOPED_TRACE(threadMessage ? "thread message" : "ordinary message");
                             auto luaRoot = LuaRoot::create(fixture.root());
                             ASSERT_TRUE(luaRoot);
                             LuaMessageScript component(fixture.root());
                             auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
                             ASSERT_TRUE(component.setLuaRoot(*luaRoot));
                             LuaCallbacks host;
                             ASSERT_TRUE(host.registerCallback("reload", [&]() -> std::expected<Json, StarException> {
                               if (auto restarted = (*luaRoot)->restart(); !restarted)
                                 return std::unexpected(std::move(restarted.error()));
                               auto nested = component.invoke<Json>("probe");
                               if (!nested)
                                 return std::unexpected(std::move(nested.error()));
                               if (!*nested)
                                 return std::unexpected(StarException("Reload retired the active message frame"));
                               return std::move(**nested);
                             }));
                             ASSERT_TRUE(component.addCallbacks("host", std::move(host)));
                             ASSERT_TRUE(component.init());
                             ASSERT_TRUE(component.eval<LuaValue>(R"LUA(
      self.marker = "active"
      function probe() return self.marker end
      message.setHandler("reload", function()
        message.setHandler("reload", nil)
        local nested = host.reload()
        return {nested, self.marker}
      end)
      threads.setMessageHandler("reload", function()
        threads.setMessageHandler("reload", nil)
        local nested = host.reload()
        return {nested, self.marker}
      end)
    )LUA"));
                             Json expected = JsonArray{"active", "active"};
                             if (threadMessage) {
                               auto response = component.threadPassMessage("worker", "reload", {});
                               ASSERT_TRUE(component.update<LuaValue>());
                               auto result = response.result();
                               ASSERT_TRUE(result);
                               ASSERT_TRUE(*result);
                               EXPECT_EQ(**result, expected);
                             } else {
                               expectResponse(component.handleMessage("reload", true), expected);
                             }
                             // The next top-level entry, not a nested native call, retires the frame.
                             expectNoResponse(component.handleMessage("reload", true));
                             auto fresh = component.eval<bool>("return self.marker == nil and probe == nil");
                             ASSERT_TRUE(fresh);
                             EXPECT_TRUE(fresh->value_or(false));
                           }
                         });

const GameTestRegistration threadBatchFailure(
  "LuaComponentLifecycle", "LocalMessageBatchSettlesAfterHandlerDisablesSession", [](GameTest& fixture) {
    auto luaRoot = LuaRoot::create(fixture.root());
    ASSERT_TRUE(luaRoot);
    LuaMessageScript component(fixture.root());
    auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
    ASSERT_TRUE(component.setLuaRoot(*luaRoot));
    ASSERT_TRUE(component.init());
    ASSERT_TRUE(component.eval<LuaValue>(R"LUA(
    threads.setMessageHandler("fail", function()
      threads.setMessageHandler("fail", nil)
      error("local batch failure")
    end)
    threads.setMessageHandler("later", function() error("stale batch handler") end)
  )LUA"));
    auto first = component.threadPassMessage("worker", "fail", {});
    auto second = component.threadPassMessage("worker", "later", {});
    auto third = component.threadPassMessage("worker", "missing", {});
    auto result = expectLoggedFailure([&] { return component.update<LuaValue>(); }, "local batch failure");
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().is<LuaException>());
    EXPECT_FALSE(component.initialized());
    EXPECT_TRUE(component.error());
    expectPromiseFailure(first);
    expectPromiseFailure(second);
    expectPromiseFailure(third);
    ASSERT_TRUE(component.init());
    auto retired = component.threadPassMessage("worker", "later", {});
    ASSERT_TRUE(component.update<LuaValue>());
    expectPromiseFailure(retired);
  });

const GameTestRegistration
  evalErrorPolicy("LuaComponentLifecycle", "EvalFailureKeepsContextButInvokeFailureDisablesIt", [](GameTest& fixture) {
    auto luaRoot = LuaRoot::create(fixture.root());
    ASSERT_TRUE(luaRoot);
    LuaMessageScript component(fixture.root());
    auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
    ASSERT_TRUE(component.setLuaRoot(*luaRoot));
    ASSERT_TRUE(component.init());
    ASSERT_TRUE(component.eval<LuaValue>("self.marker = 41; function fail() error('invoke policy failure') end"));
    auto evaluated =
      expectLoggedFailure([&] { return component.eval<Json>("error('eval policy failure')"); }, "eval policy failure");
    ASSERT_FALSE(evaluated);
    EXPECT_TRUE(evaluated.error().is<LuaException>());
    EXPECT_TRUE(component.initialized());
    EXPECT_FALSE(component.error());
    auto marker = component.eval<int>("return self.marker");
    ASSERT_TRUE(marker);
    EXPECT_EQ(marker->value_or(0), 41);
    auto invoked = expectLoggedFailure([&] { return component.invoke<LuaValue>("fail"); }, "invoke policy failure");
    ASSERT_FALSE(invoked);
    EXPECT_TRUE(invoked.error().is<LuaException>());
    EXPECT_FALSE(component.initialized());
    EXPECT_TRUE(component.error());
  });

const GameTestRegistration
  chainedMessageResult("LuaComponentLifecycle", "AsyncMessageResultsDistinguishPendingFailureAndSuccess",
                       [](GameTest& fixture) {
                         auto luaRoot = LuaRoot::create(fixture.root());
                         ASSERT_TRUE(luaRoot);
                         auto remote = RpcPromise<Json>::createPair();
                         LuaMessageScript component(fixture.root());
                         auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
                         ASSERT_TRUE(component.setLuaRoot(*luaRoot));
                         LuaCallbacks host;
                         ASSERT_TRUE(host.registerCallback("promise", [&] { return remote.first; }));
                         ASSERT_TRUE(component.addCallbacks("host", std::move(host)));
                         ASSERT_TRUE(component.init());
                         ASSERT_TRUE(component.eval<LuaValue>(R"LUA(
    threads.setMessageHandler("async", function() return host.promise() end)
  )LUA"));
                         auto response = component.threadPassMessage("worker", "async", {});
                         ASSERT_TRUE(component.update<LuaValue>());
                         expectPending(response);
                         auto pendingResult = response.result();
                         ASSERT_TRUE(pendingResult);
                         EXPECT_FALSE(*pendingResult);
                         auto pendingError = response.error();
                         ASSERT_TRUE(pendingError);
                         EXPECT_FALSE(*pendingError);
                         ASSERT_TRUE(remote.second.fail("remote failure"));
                         expectPromiseFailure(response);
                         auto failedResult = response.result();
                         ASSERT_TRUE(failedResult);
                         EXPECT_FALSE(*failedResult);
                         remote = RpcPromise<Json>::createPair();
                         auto success = component.threadPassMessage("worker", "async", {});
                         ASSERT_TRUE(component.update<LuaValue>());
                         expectPending(success);
                         ASSERT_TRUE(remote.second.fulfill(Json(53)));
                         auto successfulResult = success.result();
                         ASSERT_TRUE(successfulResult);
                         ASSERT_TRUE(*successfulResult);
                         EXPECT_EQ(**successfulResult, Json(53));
                       });

const GameTestRegistration workerStartupFailure(
  "LuaComponentLifecycle", "ActualFailedWorkerSettlesMessagesAndPublishesRecyclableOnExit", [](GameTest& fixture) {
    LuaMessageScript parent(fixture.root());
    auto created = ScriptableThread::create(JsonObject{{"name", "startup-failure"}, {"scripts", false}}, parent);
    ASSERT_TRUE(created) << created.error().what();
    auto worker = std::move(*created);
    auto cleanup = finally([&] { EXPECT_TRUE(worker->stop()); });
    auto accepted = RpcPromise<Json>::createPair();
    ASSERT_TRUE(worker->passMessage({"queued-before-start", {}, accepted.second}));
    expectPending(accepted.first);
    testing::TestPartResultArray errors;
    {
      testing::ScopedFakeTestPartResultReporter capture(
        testing::ScopedFakeTestPartResultReporter::INTERCEPT_ALL_THREADS, &errors);
      auto started = worker->start();
      ASSERT_TRUE(started);
      ASSERT_TRUE(*started);
      // Join waits for the actual OS worker, not a fabricated failure flag.
      auto joined = worker->join();
      ASSERT_TRUE(joined);
      EXPECT_TRUE(*joined);
    }
    EXPECT_EQ(errors.size(), 1);
    EXPECT_TRUE(worker->errorOccurred());
    EXPECT_TRUE(worker->shouldExpire());
    EXPECT_TRUE(worker->isJoined());
    expectPromiseFailure(accepted.first);
    expectPromiseStatus(accepted.first, RpcPromiseStatus::Failed);
    auto late = RpcPromise<Json>::createPair();
    ASSERT_TRUE(worker->passMessage({"after-failure", {}, late.second}));
    expectPromiseFailure(late.first);
    expectPromiseStatus(late.first, RpcPromiseStatus::Failed);
    ASSERT_TRUE(worker->stop());
    cleanup.cancel();
  });

const GameTestRegistration
  deferredWorldShutdown("LuaComponentLifecycle", "DeferredWorldShutdownKeepsCapabilitiesThroughLuaHooks",
                        [](GameTest& fixture) {
                          auto storage = File::ephemeralFile(fixture.root().random());
                          ASSERT_TRUE(storage);
                          auto world = WorldServer::create(fixture.root(), Vec2U(2048, 2048), *storage);
                          ASSERT_TRUE(world);
                          auto close = finally([&] { EXPECT_TRUE((*world)->close()); });
                          LuaStoredEntityScript component(fixture.root());
                          auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
                          LuaCallbacks host;
                          ASSERT_TRUE(host.registerCallback("retire", [&] { return component.uninit(); }));
                          ASSERT_TRUE(component.addCallbacks("host", std::move(host)));
                          ASSERT_TRUE(component.init(**world));
                          ASSERT_TRUE(component.eval<LuaValue>(R"LUA(
    function uninit() storage.width = world.size()[1] end
    message.setHandler("retire", function()
      host.retire()
      return world.size()[1]
    end)
  )LUA"));
                          expectResponse(component.handleMessage("retire", true), Json(2048));
                          EXPECT_FALSE(component.initialized());
                          auto saved = component.getScriptStorage();
                          ASSERT_TRUE(saved);
                          EXPECT_EQ(*saved, (JsonObject{{"width", 2048}}));
                        });

const GameTestRegistration deferredCallbackRemoval(
  "LuaComponentLifecycle", "DeferredUninitKeepsHostServicesUntilShutdownHookReturns", [](GameTest& fixture) {
    auto luaRoot = LuaRoot::create(fixture.root());
    ASSERT_TRUE(luaRoot);
    LuaMessageScript component(fixture.root());
    auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
    ASSERT_TRUE(component.setLuaRoot(*luaRoot));
    int cleanups = 0;
    LuaCallbacks capability;
    ASSERT_TRUE(capability.registerCallback("cleanup", [&] { ++cleanups; }));
    ASSERT_TRUE(component.addCallbacks("capability", capability));
    LuaCallbacks host;
    ASSERT_TRUE(host.registerCallback("close", [&]() -> std::expected<void, StarException> {
      auto stopped = component.uninit();
      if (!stopped)
        return stopped;
      auto removed = component.removeCallbacks("capability");
      if (!removed)
        return std::unexpected(std::move(removed.error()));
      return {};
    }));
    ASSERT_TRUE(component.addCallbacks("host", std::move(host)));
    ASSERT_TRUE(component.init());
    ASSERT_TRUE(component.eval<LuaValue>(R"LUA(
      function uninit() capability.cleanup() end
      message.setHandler("close", function()
        host.close()
        return "closed"
      end)
    )LUA"));
    auto saved = component.context()->getPath<LuaFunction>("capability.cleanup");
    ASSERT_TRUE(saved);
    expectResponse(component.handleMessage("close", true), Json("closed"));
    EXPECT_EQ(cleanups, 1);
    EXPECT_FALSE(component.initialized());
    EXPECT_FALSE(saved->invoke<LuaValue>());
    ASSERT_TRUE(component.addCallbacks("capability", capability));
    ASSERT_TRUE(component.init());
    ASSERT_TRUE(component.eval<LuaValue>("function uninit() capability.cleanup() end"));
    ASSERT_TRUE(component.uninit());
    EXPECT_EQ(cleanups, 2);
  });

struct FailingLuaLogTag {
  static constexpr std::string_view name() { return "FailingLuaLog"; }
};
using FailingLuaLog = StarError<FailingLuaLogTag, StarException>;

const GameTestRegistration
  loggingFailureCategory("LuaComponentLifecycle", "LoggingFailureRemainsInfrastructureFailure", [](GameTest& fixture) {
    class FailingSink final : public LogSink {
    public:
      FailingSink() { setLevel(LogLevel::Error); }
      std::expected<void, StarException> log(std::string_view, LogLevel) override {
        return std::unexpected(FailingLuaLog("log sink unavailable"));
      }
    };
    auto luaRoot = LuaRoot::create(fixture.root());
    ASSERT_TRUE(luaRoot);
    LuaMessageScript component(fixture.root());
    auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
    ASSERT_TRUE(component.setLuaRoot(*luaRoot));
    ASSERT_TRUE(component.init());
    ASSERT_TRUE(component.eval<LuaValue>("function fail() error('original script failure') end"));
    auto sink = std::make_shared<FailingSink>();
    fixture.root().logger().addSink(sink);
    auto removeSink = finally([&] { fixture.root().logger().removeSink(sink); });
    auto result = [&] {
      // Sink iteration may fail before the test reporter receives the log.
      testing::TestPartResultArray errors;
      testing::ScopedFakeTestPartResultReporter capture(
        testing::ScopedFakeTestPartResultReporter::INTERCEPT_ALL_THREADS, &errors);
      return component.invoke<LuaValue>("fail");
    }();
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().is<FailingLuaLog>());
    EXPECT_FALSE(result.error().is<LuaException>());
    EXPECT_FALSE(component.initialized());
  });

const GameTestRegistration
  deferredLifecycle("LuaComponentLifecycle", "ExplicitLifecycleReentryDefersRetirementUntilHandlerReturns",
                    [](GameTest& fixture) {
                      auto luaRoot = LuaRoot::create(fixture.root());
                      ASSERT_TRUE(luaRoot);
                      LuaMessageScript component(fixture.root());
                      auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
                      ASSERT_TRUE(component.setLuaRoot(*luaRoot));
                      LuaCallbacks host;
                      ASSERT_TRUE(host.registerCallback("reinit", [&] { return component.init(); }));
                      ASSERT_TRUE(host.registerCallback("uninit", [&] { return component.uninit(); }));
                      ASSERT_TRUE(component.addCallbacks("host", std::move(host)));
                      ASSERT_TRUE(component.init());
                      ASSERT_TRUE(component.eval<LuaValue>(R"LUA(
    self.marker = "before reinit"
    message.setHandler("reinit", function()
      host.reinit()
      return self.marker
    end)
  )LUA"));
                      expectResponse(component.handleMessage("reinit", true), Json("before reinit"));
                      expectNoResponse(component.handleMessage("reinit", true));
                      ASSERT_TRUE(component.eval<LuaValue>(R"LUA(
    self.marker = "before uninit"
    function uninit()
      host.uninit()
      host.reinit()
    end
    message.setHandler("uninit", function()
      host.uninit()
      return self.marker
    end)
  )LUA"));
                      expectResponse(component.handleMessage("uninit", true), Json("before uninit"));
                      EXPECT_FALSE(component.initialized());
                      expectNoResponse(component.handleMessage("uninit", true));
                      EXPECT_TRUE(component.initialized());
                    });

const GameTestRegistration
  actorParameterCodec("LuaComponentBindings", "ActorParametersRoundtripAndReachRealMovementControls",
                      [](GameTest& fixture) {
                        ActorMovementParameters parameters;
                        parameters.stopOnFirstBounce = true;
                        parameters.enableSurfaceSlopeCorrection = false;
                        parameters.pathExploreRate = 23.5f;
                        auto controller = ActorMovementController::create(fixture.root(), parameters);
                        ASSERT_TRUE(controller) << controller.error().what();
                        auto storage = File::ephemeralFile(fixture.root().random());
                        ASSERT_TRUE(storage);
                        auto world = WorldServer::create(fixture.root(), Vec2U(2048, 2048), *storage);
                        ASSERT_TRUE(world);
                        auto close = finally([&] { EXPECT_TRUE((*world)->close()); });
                        (*controller)->init(**world);
                        auto unbind = finally([&] { (*controller)->uninit(); });
                        LuaActorScript component(fixture.root());
                        auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
                        ASSERT_TRUE(component.addActorMovementCallbacks(**controller));
                        ASSERT_TRUE(component.init(**world));
                        ASSERT_TRUE(component.context()->set("roundtripInput", parameters));
                        auto decoded = component.eval<ActorMovementParameters>("return roundtripInput");
                        ASSERT_TRUE(decoded) << decoded.error().what();
                        ASSERT_TRUE(*decoded);
                        EXPECT_EQ((**decoded).stopOnFirstBounce, parameters.stopOnFirstBounce);
                        EXPECT_EQ((**decoded).enableSurfaceSlopeCorrection, parameters.enableSurfaceSlopeCorrection);
                        EXPECT_EQ((**decoded).pathExploreRate, parameters.pathExploreRate);
                        auto base = component.eval<Json>(R"LUA(
    local p = mcontroller.baseParameters()
    return {p.stopOnFirstBounce, p.enableSurfaceSlopeCorrection, p.pathExploreRate}
  )LUA");
                        ASSERT_TRUE(base);
                        ASSERT_TRUE(*base);
                        EXPECT_EQ(**base, (Json(JsonArray{true, false, 23.5})));
                        ASSERT_TRUE(component.eval<LuaValue>(R"LUA(
    function update()
      mcontroller.controlParameters({
        stopOnFirstBounce = false,
        enableSurfaceSlopeCorrection = true,
        pathExploreRate = 37.5,
        gravityEnabled = false,
        collisionEnabled = false
      })
    end
  )LUA"));
                        ASSERT_TRUE(component.update<LuaValue>());
                        ASSERT_TRUE((*controller)->tickMaster(1.0f / 60.0f));
                        auto active = component.eval<Json>(R"LUA(
    local p = mcontroller.parameters()
    return {p.stopOnFirstBounce, p.enableSurfaceSlopeCorrection}
  )LUA");
                        ASSERT_TRUE(active);
                        ASSERT_TRUE(*active);
                        EXPECT_EQ(**active, (Json(JsonArray{false, true})));
                      });

const GameTestRegistration inputEventNames("InputNameSerialization", "EventJsonPreservesCanonicalNamesAndUnknownEnumErrors", [](GameTest&) {
  auto key = Input::inputEventToJson(KeyDownEvent{Key::F16, KeyMod::LGui | KeyMod::AltGr});
  ASSERT_TRUE(key) << key.error().what();
  EXPECT_EQ(*key, Json(JsonObject{{"type", "KeyDown"}, {"data", JsonObject{
      {"key", "F16"}, {"mods", JsonArray{"LGui", "AltGr"}}}}}));
  auto mouse = Input::inputEventToJson(MouseButtonUpEvent{MouseButton::FourthButton, Vec2F(3, 4)});
  ASSERT_TRUE(mouse) << mouse.error().what();
  EXPECT_EQ(*mouse, Json(JsonObject{{"type", "MouseButtonUp"}, {"data", JsonObject{
      {"mouseButton", "MouseFourth"}, {"mousePosition", JsonArray{3.0, 4.0}}}}}));
  auto controller = Input::inputEventToJson(ControllerButtonDownEvent{7, ControllerButton::Invalid});
  ASSERT_TRUE(controller) << controller.error().what();
  EXPECT_EQ(*controller, Json(JsonObject{{"type", "ControllerButtonDown"}, {"data", JsonObject{
      {"controllerButton", "Invalid"}, {"controller", 7}}}}));
  auto axis = Input::inputEventToJson(ControllerAxisEvent{2, ControllerAxis::TriggerRight, 0.5f});
  ASSERT_TRUE(axis) << axis.error().what();
  EXPECT_EQ(*axis, Json(JsonObject{{"type", "ControllerAxis"}, {"data", JsonObject{
      {"controllerAxis", "TriggerRight"}, {"controllerAxisValue", 0.5}, {"controller", 2}}}}));
  auto unknownKey = Input::inputEventToJson(KeyUpEvent{static_cast<Key>(65535)});
  ASSERT_FALSE(unknownKey);
  EXPECT_TRUE(unknownKey.error().is<MapException>());
  auto unknownAxis = Input::inputEventToJson(ControllerAxisEvent{2, ControllerAxis::Invalid, 0.5f});
  ASSERT_FALSE(unknownAxis);
  EXPECT_TRUE(unknownAxis.error().is<MapException>());
});

const GameTestRegistration inputBindNames("InputNameSerialization", "BindJsonPreservesCaseFirstErrorsAndUnknownNames", [](GameTest&) {
  auto key = Input::bindFromJson(JsonObject{{"type", "key"}, {"value", "eSc"},
      {"mods", JsonArray{"rShIfT", "lCtRl", "RSHIFT"}}});
  ASSERT_TRUE(key) << key.error().what();
  ASSERT_TRUE(std::holds_alternative<Input::KeyBind>(*key));
  auto const& keyBind = std::get<Input::KeyBind>(*key);
  EXPECT_EQ(keyBind.key, Key::Escape);
  EXPECT_EQ(keyBind.mods, KeyMod::RShift | KeyMod::LCtrl);
  EXPECT_EQ(keyBind.priority, 2);
  auto serializedKey = Input::bindToJson(*key);
  ASSERT_TRUE(serializedKey) << serializedKey.error().what();
  EXPECT_EQ(*serializedKey, Json(JsonObject{{"type", "key"}, {"value", "Esc"},
      {"mods", JsonArray{"RShift", "LCtrl"}}}));
  auto reparsedKey = Input::bindFromJson(*serializedKey);
  ASSERT_TRUE(reparsedKey) << reparsedKey.error().what();
  ASSERT_TRUE(std::holds_alternative<Input::KeyBind>(*reparsedKey));
  auto const& reparsedKeyBind = std::get<Input::KeyBind>(*reparsedKey);
  EXPECT_EQ(reparsedKeyBind.key, keyBind.key);
  EXPECT_EQ(reparsedKeyBind.mods, keyBind.mods);
  EXPECT_EQ(reparsedKeyBind.priority, keyBind.priority);

  for (Json const& json : JsonArray{
      JsonObject{{"type", "mouse"}, {"value", "mOuSeFiFtH"}},
      JsonObject{{"type", "controller"}, {"value", "tOuChPaD"}, {"controller", 7}}}) {
    auto bind = Input::bindFromJson(json);
    ASSERT_TRUE(bind) << bind.error().what();
    auto serialized = Input::bindToJson(*bind);
    ASSERT_TRUE(serialized) << serialized.error().what();
    auto type = json.getString("type");
    ASSERT_TRUE(type);
    EXPECT_EQ(*serialized, *type == "mouse"
        ? Json(JsonObject{{"type", "mouse"}, {"value", "MouseFifth"}})
        : Json(JsonObject{{"type", "controller"}, {"value", "Touchpad"}, {"controller", 7}}));
  }
  String malformed(std::string_view("zzzz\xff", 5));
  auto unknown = Input::bindFromJson(JsonObject{{"type", "key"}, {"value", "not a key"},
      {"mods", JsonArray{malformed}}});
  ASSERT_TRUE(unknown) << unknown.error().what();
  EXPECT_TRUE(std::holds_alternative<std::monostate>(*unknown));
  auto invalidName = Input::bindFromJson(JsonObject{{"type", "key"}, {"value", malformed}});
  ASSERT_FALSE(invalidName);
  EXPECT_TRUE(invalidName.error().is<UnicodeException>());
  auto firstUnknown = Input::bindFromJson(JsonObject{{"type", "key"}, {"value", "A"},
      {"mods", JsonArray{"unknown modifier", malformed}}});
  ASSERT_FALSE(firstUnknown);
  EXPECT_TRUE(firstUnknown.error().is<MapException>());
  auto firstInvalid = Input::bindFromJson(JsonObject{{"type", "key"}, {"value", "A"},
      {"mods", JsonArray{malformed, "unknown modifier"}}});
  ASSERT_FALSE(firstInvalid);
  EXPECT_TRUE(firstInvalid.error().is<UnicodeException>());
  auto invalidValue = Input::bindToJson(Input::KeyBind{static_cast<Key>(65535)});
  ASSERT_FALSE(invalidValue);
  EXPECT_TRUE(invalidValue.error().is<MapException>());
});

const GameTestRegistration inputModifierPresence("InputNameSerialization",
                                                 "ModifierPresencePreservesTypeErrorsAndPriority", [](GameTest&) {
                                                   Json plain = JsonObject{{"type", "key"}, {"value", "A"}};
                                                   auto nullModifiers = plain.set("mods", Json());
                                                   ASSERT_TRUE(nullModifiers);
                                                   for (Json const& config : JsonArray{plain, *nullModifiers}) {
                                                     auto decoded = Input::bindFromJson(config);
                                                     ASSERT_TRUE(decoded);
                                                     ASSERT_TRUE(std::holds_alternative<Input::KeyBind>(*decoded));
                                                     auto const& binding = std::get<Input::KeyBind>(*decoded);
                                                     EXPECT_EQ(binding.mods, KeyMod::NoMod);
                                                     EXPECT_EQ(binding.priority, 0);
                                                   }
                                                   auto badModifiers = plain.set("mods", JsonObject{});
                                                   ASSERT_TRUE(badModifiers);
                                                   auto invalid = Input::bindFromJson(*badModifiers);
                                                   ASSERT_FALSE(invalid);
                                                   EXPECT_TRUE(invalid.error().is<JsonException>());
                                                 });
const GameTestRegistration luaEntityIdentifiers(
  "LuaEntityBindings", "EmbeddedNullIdentifiersSurviveFreshAndRestartedEngines", [](GameTest& fixture) {
    auto storage = File::ephemeralFile(fixture.root().random());
    ASSERT_TRUE(storage);
    auto world = WorldServer::create(fixture.root(), Vec2U(2048, 2048), *storage);
    ASSERT_TRUE(world);
    auto close = finally([&] { EXPECT_TRUE((*world)->close()); });
    auto database = fixture.root().npcDatabase();
    ASSERT_TRUE(database);
    auto variant = (*database)->generateNpcVariant("human", "villager", 1.0f, 1234, JsonObject());
    ASSERT_TRUE(variant);
    auto npc = (*database)->createNpc(*variant);
    ASSERT_TRUE(npc);
    ASSERT_TRUE((*world)->addEntity(*npc));
    String identifier(std::string_view("\0before\0\0after\0", 15));
    (*npc)->setUniqueId(identifier);
    auto luaRoot = LuaRoot::create(fixture.root());
    ASSERT_TRUE(luaRoot);
    for (bool restart : {false, true}) {
      SCOPED_TRACE(restart ? "restarted engine" : "fresh engine");
      if (restart)
        ASSERT_TRUE((*luaRoot)->restart());
      auto context = (*luaRoot)->luaEngine().createContext();
      ASSERT_TRUE(context);
      auto callbacks = LuaBindings::makeWorldCallbacks(fixture.root(), **world);
      ASSERT_TRUE(callbacks);
      ASSERT_TRUE(context->setCallbacks("world", *callbacks));
      ASSERT_TRUE(context->set("id", (*npc)->entityId()));
      auto identifiers = context->eval<Json>("{world.entityUniqueId(id), world.entity(id):uniqueId()}");
      ASSERT_TRUE(identifiers) << identifiers.error().what();
      EXPECT_EQ(*identifiers, Json(JsonArray{identifier, identifier}));
      auto lengths = context->eval<Json>("{#world.entityUniqueId(id), #world.entity(id):uniqueId()}");
      ASSERT_TRUE(lengths);
      EXPECT_EQ(*lengths, Json(JsonArray{15, 15}));
    }
  });

const GameTestRegistration
  chainedReplyShutdown("LuaComponentLifecycle", "ClosingSessionSettlesChainedReplyAndIgnoresLateCompletion",
                       [](GameTest& fixture) {
                         auto luaRoot = LuaRoot::create(fixture.root());
                         ASSERT_TRUE(luaRoot);
                         auto remote = RpcPromise<Json>::createPair();
                         LuaMessageScript component(fixture.root());
                         auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
                         ASSERT_TRUE(component.setLuaRoot(*luaRoot));
                         LuaCallbacks host;
                         ASSERT_TRUE(host.registerCallback("promise", [&] { return remote.first; }));
                         ASSERT_TRUE(component.addCallbacks("host", std::move(host)));
                         ASSERT_TRUE(component.init());
                         ASSERT_TRUE(component.eval<LuaValue>(R"LUA(
      threads.setMessageHandler("async", function() return host.promise() end)
    )LUA"));
                         auto response = component.threadPassMessage("worker", "async", {});
                         ASSERT_TRUE(component.update<LuaValue>());
                         expectPending(response);
                         ASSERT_TRUE(component.uninit());
                         EXPECT_EQ(response.finished(), true);
                         expectPromiseStatus(response, RpcPromiseStatus::Cancelled);
                         expectPromiseStatus(remote.first, RpcPromiseStatus::Pending);
                         EXPECT_EQ(response.succeeded(), false);
                         ASSERT_TRUE(remote.second.fulfill(Json(53)));
                         EXPECT_EQ(response.succeeded(), false);
                         auto result = response.result();
                         ASSERT_TRUE(result);
                         EXPECT_FALSE(*result);
                       });

const GameTestRegistration
  injectedReplyRetirement("LuaComponentLifecycle",
                          "InjectedRepliesRetireOnUninitReinitAndReloadWithoutCancellingSource", [](GameTest& fixture) {
                            enum class Retirement { Uninit, Reinit, Reload };
                            for (auto [retirement, name] :
                                 {std::pair{Retirement::Uninit, "uninit"}, std::pair{Retirement::Reinit, "reinit"},
                                  std::pair{Retirement::Reload, "reload"}}) {
                              SCOPED_TRACE(name);
                              auto luaRoot = LuaRoot::create(fixture.root());
                              ASSERT_TRUE(luaRoot);
                              LuaMessageScript component(fixture.root());
                              auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
                              ASSERT_TRUE(component.setLuaRoot(*luaRoot));
                              ASSERT_TRUE(component.init());
                              auto producer = RpcPromise<Json>::createPair();
                              ASSERT_TRUE(component.context()->set("injected", producer.first));
                              ASSERT_TRUE(component.eval<LuaValue>(R"LUA(
        message.setHandler("async", function() return injected end)
        threads.setMessageHandler("async", function() return injected end)
      )LUA"));
                              auto ordinary = promiseResponse(component.handleMessage("async", true));
                              ASSERT_TRUE(ordinary) << ordinary.error();
                              auto reply = std::move(*ordinary);
                              auto queued = component.threadPassMessage("worker", "async", {});
                              ASSERT_TRUE(component.update<LuaValue>());
                              expectPromiseStatus(reply, RpcPromiseStatus::Pending);
                              expectPromiseStatus(queued, RpcPromiseStatus::Pending);
                              if (retirement == Retirement::Uninit) {
                                ASSERT_TRUE(component.uninit());
                              } else if (retirement == Retirement::Reinit) {
                                ASSERT_TRUE(component.init());
                              } else {
                                ASSERT_TRUE((*luaRoot)->restart());
                                expectNoResponse(component.handleMessage("async", true));
                              }
                              expectPromiseStatus(reply, RpcPromiseStatus::Cancelled);
                              expectPromiseStatus(queued, RpcPromiseStatus::Cancelled);
                              expectPromiseStatus(producer.first, RpcPromiseStatus::Pending);
                              ASSERT_TRUE(producer.second.fulfill(Json(67)));
                              expectPromiseStatus(producer.first, RpcPromiseStatus::Succeeded);
                              expectPromiseStatus(reply, RpcPromiseStatus::Cancelled);
                              expectPromiseStatus(queued, RpcPromiseStatus::Cancelled);
                              auto result = producer.first.result();
                              ASSERT_TRUE(result);
                              ASSERT_TRUE(*result);
                              EXPECT_EQ(**result, Json(67));
                            }
                          });

const GameTestRegistration publishedNullReply("LuaComponentLifecycle",
                                              "PublishedAndDirectNullRepliesSurviveSessionClose",
                                              [](GameTest& fixture) {
                                                auto luaRoot = LuaRoot::create(fixture.root());
                                                ASSERT_TRUE(luaRoot);
                                                LuaMessageScript component(fixture.root());
                                                auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
                                                ASSERT_TRUE(component.setLuaRoot(*luaRoot));
                                                ASSERT_TRUE(component.init());
                                                for (bool completedBeforeDispatch : {true, false}) {
                                                  SCOPED_TRACE(completedBeforeDispatch ? "completed before dispatch"
                                                                                       : "completed after dispatch");
                                                  auto producer = RpcPromise<Json>::createPair();
                                                  if (completedBeforeDispatch)
                                                    ASSERT_TRUE(producer.second.fulfill(Json()));
                                                  ASSERT_TRUE(component.context()->set("injected", producer.first));
                                                  ASSERT_TRUE(component.eval<LuaValue>(R"LUA(
        message.setHandler("async", function() return injected end)
      )LUA"));
                                                  auto response =
                                                    promiseResponse(component.handleMessage("async", true));
                                                  ASSERT_TRUE(response) << response.error();
                                                  auto reply = std::move(*response);
                                                  if (!completedBeforeDispatch)
                                                    ASSERT_TRUE(producer.second.fulfill(Json()));
                                                  expectPromiseStatus(reply, RpcPromiseStatus::Succeeded);
                                                  ASSERT_TRUE(component.uninit());
                                                  expectPromiseStatus(reply, RpcPromiseStatus::Succeeded);
                                                  auto result = reply.result();
                                                  ASSERT_TRUE(result);
                                                  ASSERT_TRUE(*result);
                                                  EXPECT_EQ(**result, Json());
                                                  EXPECT_EQ(reply.failed(), false);
                                                  EXPECT_EQ(reply.cancelled(), false);
                                                  ASSERT_TRUE(component.init());
                                                }
                                              });

const GameTestRegistration
  independentSessionReplies("LuaComponentLifecycle", "SharedNativeSourceHasIndependentSessionObservers",
                            [](GameTest& fixture) {
                              auto luaRoot = LuaRoot::create(fixture.root());
                              ASSERT_TRUE(luaRoot);
                              auto producer = RpcPromise<Json>::createPair();
                              LuaMessageScript first(fixture.root());
                              LuaMessageScript second(fixture.root());
                              auto cleanup = finally([&] {
                                EXPECT_TRUE(first.uninit());
                                EXPECT_TRUE(second.uninit());
                              });
                              for (auto component : {&first, &second}) {
                                ASSERT_TRUE(component->setLuaRoot(*luaRoot));
                                ASSERT_TRUE(component->init());
                                ASSERT_TRUE(component->context()->set("injected", producer.first));
                                ASSERT_TRUE(component->eval<LuaValue>(R"LUA(
        message.setHandler("async", function() return injected end)
      )LUA"));
                              }
                              auto firstResponse = promiseResponse(first.handleMessage("async", true));
                              auto secondResponse = promiseResponse(second.handleMessage("async", true));
                              ASSERT_TRUE(firstResponse) << firstResponse.error();
                              ASSERT_TRUE(secondResponse) << secondResponse.error();
                              auto firstReply = std::move(*firstResponse);
                              auto secondReply = std::move(*secondResponse);
                              ASSERT_TRUE(first.uninit());
                              expectPromiseStatus(firstReply, RpcPromiseStatus::Cancelled);
                              expectPromiseStatus(secondReply, RpcPromiseStatus::Pending);
                              expectPromiseStatus(producer.first, RpcPromiseStatus::Pending);
                              ASSERT_TRUE(producer.second.fulfill(Json()));
                              expectPromiseStatus(secondReply, RpcPromiseStatus::Succeeded);
                              expectPromiseStatus(producer.first, RpcPromiseStatus::Succeeded);
                              expectPromiseStatus(firstReply, RpcPromiseStatus::Cancelled);
                            });

const GameTestRegistration
  methodPromiseLifetime("LuaComponentLifecycle", "UserdataMethodsInheritSessionScopeAndRestoreOuterEngineMarker",
                        [](GameTest& fixture) {
                          auto luaRoot = LuaRoot::create(fixture.root());
                          ASSERT_TRUE(luaRoot);
                          LuaMessageScript component(fixture.root());
                          auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
                          ASSERT_TRUE(component.setLuaRoot(*luaRoot));
                          ASSERT_TRUE(component.init());
                          auto producer = RpcPromise<Json>::createPair();
                          auto userdata = component.context()->createUserData(SessionPromiseFactory{producer.first});
                          ASSERT_TRUE(userdata);
                          ASSERT_TRUE(component.context()->set("factory", *userdata));
                          std::stop_source outer;
                          auto& engine = (*luaRoot)->luaEngine();
                          auto previous = engine.setPromiseLifetime(outer.get_token());
                          auto restore = finally([&] { engine.setPromiseLifetime(previous); });
                          ASSERT_TRUE(component.eval<LuaValue>("savedRequest = factory:request()"));
                          EXPECT_EQ(engine.promiseLifetime(), outer.get_token());
                          auto saved = component.context()->get<RpcPromise<Json>>("savedRequest");
                          ASSERT_TRUE(saved);
                          expectPromiseStatus(*saved, RpcPromiseStatus::Pending);
                          ASSERT_TRUE(component.uninit());
                          expectPromiseStatus(*saved, RpcPromiseStatus::Cancelled);
                          expectPromiseStatus(producer.first, RpcPromiseStatus::Pending);
                          EXPECT_EQ(engine.promiseLifetime(), outer.get_token());
                          ASSERT_TRUE(producer.second.fulfill(Json(79)));
                          expectPromiseStatus(*saved, RpcPromiseStatus::Cancelled);
                        });

const GameTestRegistration deferredReplyCancellation(
  "LuaComponentLifecycle", "DeferredCloseCancelsPendingReplyButPreservesDirectNull", [](GameTest& fixture) {
    auto luaRoot = LuaRoot::create(fixture.root());
    ASSERT_TRUE(luaRoot);
    for (bool completed : {false, true}) {
      SCOPED_TRACE(completed ? "completed before deferred close" : "pending at deferred close");
      LuaMessageScript component(fixture.root());
      auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
      ASSERT_TRUE(component.setLuaRoot(*luaRoot));
      auto producer = RpcPromise<Json>::createPair();
      if (completed)
        ASSERT_TRUE(producer.second.fulfill(Json()));
      LuaCallbacks host;
      ASSERT_TRUE(host.registerCallback("close", [&] { return component.uninit(); }));
      ASSERT_TRUE(component.addCallbacks("host", std::move(host)));
      ASSERT_TRUE(component.init());
      ASSERT_TRUE(component.context()->set("injected", producer.first));
      ASSERT_TRUE(component.eval<LuaValue>(R"LUA(
        message.setHandler("close", function()
          host.close()
          return injected
        end)
      )LUA"));
      auto response = promiseResponse(component.handleMessage("close", true));
      ASSERT_TRUE(response) << response.error();
      auto reply = std::move(*response);
      EXPECT_FALSE(component.initialized());
      expectPromiseStatus(reply, completed ? RpcPromiseStatus::Succeeded : RpcPromiseStatus::Cancelled);
      if (completed) {
        auto result = reply.result();
        ASSERT_TRUE(result);
        ASSERT_TRUE(*result);
        EXPECT_EQ(**result, Json());
      } else {
        expectPromiseStatus(producer.first, RpcPromiseStatus::Pending);
        ASSERT_TRUE(producer.second.fulfill(Json(83)));
        expectPromiseStatus(reply, RpcPromiseStatus::Cancelled);
      }
    }
  });

const GameTestRegistration closedWorkerMessages(
  "LuaComponentLifecycle", "ActualWorkerClosureCancelsLateMessagesAndPrestartQueue", [](GameTest& fixture) {
    LuaMessageScript parent(fixture.root());
    auto created = ScriptableThread::create(
      JsonObject{{"name", "clean-close"}, {"scripts", JsonObject{}}, {"logMapped", false}}, parent);
    ASSERT_TRUE(created);
    auto worker = *created;
    auto cleanup = finally([&] { EXPECT_TRUE(worker->stop()); });
    auto beforeStart = RpcPromise<Json>::createPair();
    ASSERT_TRUE(worker->passMessage({"queued", {}, beforeStart.second}));
    ASSERT_TRUE(worker->stop());
    expectPromiseStatus(beforeStart.first, RpcPromiseStatus::Cancelled);
    auto started = worker->start();
    ASSERT_TRUE(started);
    ASSERT_TRUE(*started);
    ASSERT_TRUE(worker->stop());
    EXPECT_TRUE(worker->isJoined());
    EXPECT_TRUE(worker->shouldExpire());
    EXPECT_FALSE(worker->errorOccurred());
    auto late = RpcPromise<Json>::createPair();
    ASSERT_TRUE(worker->passMessage({"after-close", {}, late.second}));
    expectPromiseStatus(late.first, RpcPromiseStatus::Cancelled);
  });

const GameTestRegistration workerChainedReplyClosure(
  "LuaComponentLifecycle", "ActualWorkerClosureCancelsDelegatedReplyWithoutCancellingProducer", [](GameTest& fixture) {
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto removeDirectory = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    auto overlay = File::relativeTo(*directory, "assets");
    ASSERT_TRUE(File::makeDirectory(overlay));
    ASSERT_TRUE(File::writeFile(String(R"LUA(
      function init()
        message.setHandler("pending", function() return host.promise() end)
      end
    )LUA"),
                                File::relativeTo(overlay, "p6-worker.lua")));
    auto settings = fixture.rootSettings();
    settings.assetSources.append(overlay);
    settings.storageDirectory = File::relativeTo(*directory, "storage");
    settings.runtimeConfigFile = String("p6-worker.config");
    settings.logDirectory = {};
    settings.logFile = {};
    settings.includeUGC = false;
    auto root = Root::create(std::move(settings));
    ASSERT_TRUE(root);
    LuaMessageScript parent(**root);
    auto created = ScriptableThread::create(JsonObject{{"name", "chained-close"},
                                                       {"scripts", JsonObject{{"worker", JsonArray{"/p6-worker.lua"}}}},
                                                       {"tickRate", 0.05},
                                                       {"logMapped", false}},
                                            parent);
    ASSERT_TRUE(created);
    auto worker = *created;
    auto producer = RpcPromise<Json>::createPair();
    std::promise<void> entered;
    auto handlerEntered = entered.get_future();
    LuaCallbacks host;
    ASSERT_TRUE(host.registerCallback("promise", [&] {
      entered.set_value();
      return producer.first;
    }));
    ASSERT_TRUE(worker->addCallbacks("host", host));
    auto response = RpcPromise<Json>::createPair();
    auto cleanup = finally([&] { EXPECT_TRUE(worker->stop()); });
    ASSERT_TRUE(worker->passMessage({"pending", {}, response.second}));
    auto started = worker->start();
    ASSERT_TRUE(started);
    ASSERT_TRUE(*started);
    ASSERT_EQ(handlerEntered.wait_for(std::chrono::seconds(10)), std::future_status::ready);
    // A low tick rate must not make session shutdown wait for the next tick.
    auto stopped = std::async(std::launch::async, [&] { return worker->stop(); });
    EXPECT_EQ(stopped.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    ASSERT_TRUE(stopped.get());
    EXPECT_TRUE(worker->isJoined());
    EXPECT_TRUE(worker->shouldExpire());
    EXPECT_FALSE(worker->errorOccurred());
    expectPromiseStatus(response.first, RpcPromiseStatus::Cancelled);
    expectPromiseStatus(producer.first, RpcPromiseStatus::Pending);
    ASSERT_TRUE(producer.second.fulfill(Json(89)));
    expectPromiseStatus(producer.first, RpcPromiseStatus::Succeeded);
    expectPromiseStatus(response.first, RpcPromiseStatus::Cancelled);
  });

const GameTestRegistration
  shutDownRootSetup("LuaComponentLifecycle", "ShutDownRootFailsSetupAndRestartRecovers", [](GameTest& fixture) {
    auto luaRoot = LuaRoot::create(fixture.root());
    ASSERT_TRUE(luaRoot);
    LuaMessageScript component(fixture.root());
    auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
    ASSERT_TRUE(component.setLuaRoot(*luaRoot));
    ASSERT_TRUE((*luaRoot)->shutdown());
    auto failed = [&] {
      testing::TestPartResultArray errors;
      testing::ScopedFakeTestPartResultReporter capture(
        testing::ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD, &errors);
      return component.init();
    }();
    ASSERT_FALSE(failed);
    EXPECT_TRUE(failed.error().is<LuaException>());
    EXPECT_FALSE(component.initialized());
    ASSERT_TRUE((*luaRoot)->restart());
    auto recovered = component.init();
    ASSERT_TRUE(recovered);
    EXPECT_TRUE(*recovered);
    EXPECT_TRUE(component.initialized());
  });

const GameTestRegistration removedCapability(
  "LuaComponentBindings", "RemovingGroupRevokesSavedFunctionsWithoutClosingOtherGroups", [](GameTest& fixture) {
    auto luaRoot = LuaRoot::create(fixture.root());
    ASSERT_TRUE(luaRoot);
    LuaMessageScript component(fixture.root());
    auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
    ASSERT_TRUE(component.setLuaRoot(*luaRoot));
    int oldCalls = 0;
    LuaCallbacks old;
    ASSERT_TRUE(old.registerCallback("read", [&] { return ++oldCalls; }));
    ASSERT_TRUE(component.addCallbacks("capability", std::move(old)));
    ASSERT_TRUE(component.init());
    auto saved = component.context()->getPath<LuaFunction>("capability.read");
    ASSERT_TRUE(saved);
    EXPECT_EQ(saved->invoke<int>(), 1);
    ASSERT_TRUE(component.removeCallbacks("capability"));
    EXPECT_FALSE(saved->invoke<int>());
    EXPECT_EQ(oldCalls, 1);
    EXPECT_EQ(component.eval<bool>("return capability == nil and type(sb.makePromise) == 'function'"), true);
    LuaCallbacks fresh;
    ASSERT_TRUE(fresh.registerCallback("read", [] { return 73; }));
    ASSERT_TRUE(component.addCallbacks("capability", std::move(fresh)));
    EXPECT_EQ(component.eval<int>("return capability.read()"), 73);
    EXPECT_FALSE(saved->invoke<int>());
    EXPECT_EQ(oldCalls, 1);
  });

const GameTestRegistration
  inheritedCapability("LuaComponentBindings", "InheritedCapabilitiesRetireWithTheirSession", [](GameTest& fixture) {
    auto luaRoot = LuaRoot::create(fixture.root());
    ASSERT_TRUE(luaRoot);
    int calls = 0;
    LuaCallbacks inherited;
    ASSERT_TRUE(inherited.registerCallback("read", [&] { return ++calls; }));
    (*luaRoot)->addCallbacks("inherited", inherited);
    LuaMessageScript component(fixture.root());
    auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
    ASSERT_TRUE(component.setLuaRoot(*luaRoot));
    ASSERT_TRUE(component.init());
    auto saved = component.context()->getPath<LuaFunction>("inherited.read");
    ASSERT_TRUE(saved);
    EXPECT_EQ(saved->invoke<int>(), 1);
    ASSERT_TRUE(component.uninit());
    EXPECT_FALSE(saved->invoke<int>());
    EXPECT_EQ(calls, 1);
    ASSERT_TRUE(component.init());
    EXPECT_EQ(component.eval<int>("return inherited.read()"), 2);
    EXPECT_FALSE(saved->invoke<int>());
    EXPECT_EQ(calls, 2);
  });

const GameTestRegistration
  nestedCapability("LuaComponentBindings", "ReturnedCallbackTablesInheritGroupRevocation", [](GameTest& fixture) {
    auto luaRoot = LuaRoot::create(fixture.root());
    ASSERT_TRUE(luaRoot);
    LuaMessageScript component(fixture.root());
    auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
    ASSERT_TRUE(component.setLuaRoot(*luaRoot));
    int calls = 0;
    LuaCallbacks host;
    ASSERT_TRUE(host.registerCallback("nested", [&]() -> std::expected<LuaCallbacks, StarException> {
      LuaCallbacks nested;
      auto result = nested.registerCallback("read", [&] { return ++calls; });
      if (!result)
        return std::unexpected(std::move(result.error()));
      return nested;
    }));
    ASSERT_TRUE(component.addCallbacks("host", std::move(host)));
    ASSERT_TRUE(component.init());
    ASSERT_TRUE(component.eval<LuaValue>("nested = host.nested()"));
    auto read = component.context()->getPath<LuaFunction>("nested.read");
    ASSERT_TRUE(read);
    EXPECT_EQ(read->invoke<int>(), 1);
    ASSERT_TRUE(component.removeCallbacks("host"));
    EXPECT_FALSE(read->invoke<int>());
    EXPECT_EQ(calls, 1);
  });

const GameTestRegistration scopedPathFinder("LuaComponentBindings",
                                            "PathFinderCannotExploreRetiredWorldButKeepsResultSnapshot",
                                            [](GameTest& fixture) {
                                              auto storage = File::ephemeralFile(fixture.root().random());
                                              ASSERT_TRUE(storage);
                                              auto world =
                                                WorldServer::create(fixture.root(), Vec2U(2048, 2048), *storage);
                                              ASSERT_TRUE(world);
                                              auto close = finally([&] { EXPECT_TRUE((*world)->close()); });
                                              LuaStoredEntityScript component(fixture.root());
                                              auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
                                              ASSERT_TRUE(component.init(**world));
                                              ASSERT_TRUE(component.eval<LuaValue>(R"LUA(
      finder = world.platformerPathStart({10, 10}, {10, 10}, {}, {boundBox = {-0.5, -0.5, 0.5, 0.5}})
      finder:explore(1)
      function exploreSaved() return finder:explore(1) end
      function resultSaved() return finder:result() end
    )LUA"));
                                              auto explore = component.context()->get<LuaFunction>("exploreSaved");
                                              auto result = component.context()->get<LuaFunction>("resultSaved");
                                              ASSERT_TRUE(explore);
                                              ASSERT_TRUE(result);
                                              auto before = result->invoke<Json>();
                                              ASSERT_TRUE(before);
                                              ASSERT_TRUE(component.uninit());
                                              ASSERT_TRUE((*world)->close());
                                              EXPECT_FALSE(explore->invoke<LuaValue>());
                                              auto after = result->invoke<Json>();
                                              ASSERT_TRUE(after);
                                              EXPECT_EQ(*after, *before);
                                            });

const GameTestRegistration
  failedCapabilityInstallation("LuaComponentBindings", "FailedInstallationRevokesFunctionsEscapedThroughMetamethod",
                               [](GameTest& fixture) {
                                 auto lua = LuaRoot::create(fixture.root());
                                 ASSERT_TRUE(lua);
                                 LuaMessageScript component(fixture.root());
                                 auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
                                 ASSERT_TRUE(component.setLuaRoot(*lua));
                                 ASSERT_TRUE(component.init());
                                 ASSERT_TRUE(component.eval<LuaValue>(R"LUA(
      setmetatable(_ENV, {__newindex = function(env, name, value)
        if name == "candidate" and value ~= nil then
          rawset(env, "escaped", value.read)
          error("installation rejected")
        end
        rawset(env, name, value)
      end})
    )LUA"));
                                 int calls = 0;
                                 LuaCallbacks callbacks;
                                 ASSERT_TRUE(callbacks.registerCallback("read", [&] { return ++calls; }));
                                 EXPECT_FALSE(component.addCallbacks("candidate", std::move(callbacks)));
                                 auto escaped = component.context()->get<LuaFunction>("escaped");
                                 ASSERT_TRUE(escaped);
                                 EXPECT_FALSE(escaped->invoke<int>());
                                 ASSERT_TRUE(component.uninit());
                                 EXPECT_FALSE(escaped->invoke<int>());
                                 EXPECT_EQ(calls, 0);
                               });

const GameTestRegistration reentrantCapabilityRemoval(
  "LuaComponentBindings", "ReentrantRemovalPreservesReplacementRegistration", [](GameTest& fixture) {
    auto lua = LuaRoot::create(fixture.root());
    ASSERT_TRUE(lua);
    LuaMessageScript component(fixture.root());
    auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
    ASSERT_TRUE(component.setLuaRoot(*lua));
    LuaCallbacks old;
    ASSERT_TRUE(old.registerCallback("read", [] { return 17; }));
    ASSERT_TRUE(component.addCallbacks("candidate", std::move(old)));
    LuaCallbacks control;
    ASSERT_TRUE(control.registerCallback("replace", [&]() -> std::expected<void, StarException> {
      auto removed = component.removeCallbacks("candidate");
      if (!removed)
        return std::unexpected(std::move(removed.error()));
      LuaCallbacks fresh;
      auto registered = fresh.registerCallback("read", [] { return 83; });
      if (!registered)
        return std::unexpected(std::move(registered.error()));
      return component.addCallbacks("candidate", std::move(fresh));
    }));
    ASSERT_TRUE(component.addCallbacks("control", std::move(control)));
    ASSERT_TRUE(component.init());
    auto oldRead = component.context()->getPath<LuaFunction>("candidate.read");
    ASSERT_TRUE(oldRead);
    ASSERT_TRUE(component.eval<LuaValue>(R"LUA(
      rawset(_ENV, "candidate", nil)
      local removing = false
      setmetatable(_ENV, {__newindex = function(env, name, value)
        if name == "candidate" and value == nil and not removing then
          removing = true
          control.replace()
          removing = false
          return
        end
        rawset(env, name, value)
      end})
    )LUA"));
    ASSERT_TRUE(component.removeCallbacks("candidate"));
    EXPECT_EQ(component.eval<int>("return candidate.read()"), 83);
    EXPECT_FALSE(oldRead->invoke<int>());
    auto freshRead = component.context()->getPath<LuaFunction>("candidate.read");
    ASSERT_TRUE(freshRead);
    ASSERT_TRUE(component.uninit());
    EXPECT_FALSE(freshRead->invoke<int>());
  });

const GameTestRegistration
  retiringCapabilityInstallation("LuaComponentBindings", "SessionRetirementDuringInstallationRevokesEscapedFunctions",
                                 [](GameTest& fixture) {
                                   auto lua = LuaRoot::create(fixture.root());
                                   ASSERT_TRUE(lua);
                                   LuaMessageScript component(fixture.root());
                                   auto cleanup = finally([&] { EXPECT_TRUE(component.uninit()); });
                                   ASSERT_TRUE(component.setLuaRoot(*lua));
                                   LuaCallbacks control;
                                   ASSERT_TRUE(control.registerCallback("close", [&] { return component.uninit(); }));
                                   ASSERT_TRUE(component.addCallbacks("control", std::move(control)));
                                   ASSERT_TRUE(component.init());
                                   auto retained = *component.context();
                                   ASSERT_TRUE(component.eval<LuaValue>(R"LUA(
      setmetatable(_ENV, {__newindex = function(env, name, value)
        if name == "candidate" and value ~= nil then
          rawset(env, "escaped", value.read)
          control.close()
        end
        rawset(env, name, value)
      end})
    )LUA"));
                                   int calls = 0;
                                   LuaCallbacks callbacks;
                                   ASSERT_TRUE(callbacks.registerCallback("read", [&] { return ++calls; }));
                                   EXPECT_FALSE(component.addCallbacks("candidate", std::move(callbacks)));
                                   EXPECT_FALSE(component.initialized());
                                   auto escaped = retained.get<LuaFunction>("escaped");
                                   ASSERT_TRUE(escaped);
                                   EXPECT_FALSE(escaped->invoke<int>());
                                   EXPECT_EQ(calls, 0);
                                 });
}
