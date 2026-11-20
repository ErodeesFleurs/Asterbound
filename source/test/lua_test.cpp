extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}
import std;
import star.format;
import star.string;
import star.list;
import star.json;
import star.lua;
import star.text_formatter;
import star.lua_converters;
import star.lexical_cast;
import star.rpc_promise;
import star.logging;
import star.random;
import star.utility_lua_bindings;

#include "gtest/gtest.h"

import star.vector;
import star.rect;
import star.poly;
import star.math_common;
import star.algorithm;
import star.matrix3;
import star.color;
import star.unicode;
using namespace Star;

struct BorrowedCrossStateProbe {
  LuaContext& target;
};

namespace Star {
template <> struct LuaConverter<BorrowedCrossStateProbe> {
  static std::expected<void, StarException> from(LuaStack source, BorrowedCrossStateProbe probe) {
    auto top = source.top();
    if (!lua_checkstack(source.state(), 64))
      return std::unexpected(LuaException("Cannot reserve probe stack"));
    for (int i = 0; i < 64; ++i)
      lua_pushnil(source.state());
    auto result = probe.target.invoke_path<int>("consume", source.value(1));
    lua_settop(source.state(), top);
    if (!result)
      return std::unexpected(std::move(result.error()));
    return source.push(*result);
  }
};
}// namespace Star

namespace {
struct NoMatrixLuaConverter {};

template <typename T>
concept matrix_lua_writable = requires(LuaStack stack, Matrix3<T> const& matrix) {
  LuaConverter<Matrix3<T>>::from(stack, matrix);
};

template <typename T>
concept matrix_lua_readable = requires(LuaStackValue value) { LuaConverter<Matrix3<T>>::to(value); };

static_assert(matrix_lua_writable<float> && matrix_lua_readable<float>);
static_assert(!matrix_lua_writable<NoMatrixLuaConverter> && !matrix_lua_readable<NoMatrixLuaConverter>);

template <typename T>
concept readable_path = requires(LuaContext const& context) { context.get_path<T>(""); };
template <typename T>
concept writable_path = requires(LuaContext& context, T&& value) { context.set_path("", std::forward<T>(value)); };
static_assert(readable_path<LuaTable> && !readable_path<LuaStackValue>);
static_assert(writable_path<std::string_view> && !writable_path<NoMatrixLuaConverter>);

template <typename T, typename U>
void expectLuaAlternative(std::expected<LuaValue, StarException> result, U const& expected) {
  ASSERT_TRUE(result) << result.error().what();
  ASSERT_TRUE(std::holds_alternative<T>(*result));
  EXPECT_EQ(std::get<T>(*result), expected);
}

void expectLuaNil(std::expected<LuaValue, StarException> result) {
  ASSERT_TRUE(result) << result.error().what();
  if (!std::holds_alternative<LuaNilType>(*result)) {
    auto rendered = print_lua_value(*result);
    ASSERT_TRUE(rendered) << rendered.error().what();
    ADD_FAILURE() << "Expected Lua nil; actual result: " << rendered->utf8();
  }
}
}

TEST(LuaTest, EvalParsesCompleteSourceBeforeExecuting) {
  using namespace std::string_view_literals;
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  EXPECT_FALSE(context->eval<int>(String("42\0 invalid"sv)));
  EXPECT_FALSE(context->eval<void>(String("changed = true\0 invalid"sv)));
  EXPECT_EQ(context->eval<bool>("changed == nil"), true);
  ASSERT_TRUE(context->eval<void>("changed = true"));
  EXPECT_EQ(context->eval<bool>("changed"), true);
  EXPECT_EQ(context->eval<String>("'a\\0b'"), String("a\0b"sv));
}

TEST(LuaTest, ColorCodecRetainsByteArraysCoercionAndOptionalAlpha) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->set("translucent", Color::rgba_f(0.5f, 0.0f, 1.0f, 0.5f)));
  EXPECT_EQ(context->eval<Json>("return translucent"), (Json(JsonArray{128, 0, 255, 127})));
  ASSERT_TRUE(context->set("opaque", Color::rgba_f(0.5f, 0.0f, 1.0f, 1.0f)));
  EXPECT_EQ(context->eval<Json>("return opaque"), (Json(JsonArray{128, 0, 255})));

  auto coercion = context->eval<Color>("return {12.9, '34', 255.9, 'bad-alpha', 'ignored'}");
  ASSERT_TRUE(coercion);
  EXPECT_EQ(coercion->to_rgba(), Vec4B(12, 34, 255, 255));
  auto extra = context->eval<Color>("return {1, 2, 3, 64, 99}");
  ASSERT_TRUE(extra);
  EXPECT_EQ(extra->to_rgba(), Vec4B(1, 2, 3, 64));
  EXPECT_EQ(context->eval<Color>("return 'RED\\0ignored'"), Color::Red);
}

TEST(LuaTest, ColorCodecDistinguishesMalformedColorAndUnicodeFailures) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  auto malformed = context->eval<Color>("return '#ggg'");
  ASSERT_FALSE(malformed);
  EXPECT_TRUE(malformed.error().is<LuaConversionException>());
  auto malformed_utf8 = context->eval<Color>("return 'red\\xff'");
  ASSERT_FALSE(malformed_utf8);
  EXPECT_TRUE(malformed_utf8.error().is<UnicodeException>());
  EXPECT_EQ(context->eval<Color>("return '#1234'"), Color::rgba(0x11, 0x22, 0x33, 0x44));
}

TEST(LuaTest, RandomCallbacksPreserveNamesAndPropagateCheckedFailures) {
  RandomService random(31415926);
  RandomSource untouched(31415926);
  Logger logger;
  LogMap logMap;
  auto callbacks = LuaBindings::makeUtilityCallbacks(random, logger, logMap);
  ASSERT_TRUE(callbacks) << callbacks.error().what();
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->set_callbacks("sb", *callbacks));

  auto checked = context->eval<bool>(R"LUA(
    local source = sb.makeRandomSource(31415926)
    local control = sb.makeRandomSource(31415926)
    assert(not pcall(function() source:randInt(-1) end))
    assert(not pcall(function() source:randInt(4, 3) end))
    assert(not pcall(function() source:randUInt(4, 3) end))
    assert(not pcall(function() source:randf(2, 1) end))
    assert(not pcall(function() source:randd(0, math.huge) end))
    assert(source:randu32() == 2950892229)
    assert(control:randu32() == 2950892229)
    assert(source:randu64() == control:randu64())
    source:init(27182818)
    control:init(27182818)
    source:addEntropy(1234)
    control:addEntropy(1234)
    assert(source:randu32() == control:randu32())
    assert(not pcall(sb.nrand, -1, 0))
    assert(not pcall(sb.nrand, 1, math.huge))
    assert(not pcall(sb.staticRandomI32Range, 2, 1, "seed"))
    assert(not pcall(sb.staticRandomDoubleRange, 2, 1, "seed"))
    assert(not pcall(sb.staticRandomDoubleRange, 0, math.huge, "seed"))
    local full = sb.staticRandomI32Range(-2147483648, 2147483647, "seed")
    assert(full >= -2147483648 and full <= 2147483647)
    local ordinary = sb.staticRandomI32Range(-4, 7, "seed")
    assert(ordinary >= -4 and ordinary <= 7)
    local wide = sb.staticRandomDoubleRange(-1.7976931348623157e308, 1.7976931348623157e308, "seed")
    assert(wide > -math.huge and wide < math.huge)
    return true
  )LUA");
  ASSERT_TRUE(checked) << checked.error().what();
  EXPECT_TRUE(*checked);
  EXPECT_EQ(random.next_u64(), untouched.next_u64());

  auto normal = context->eval<double>("return sb.nrand(0, 3)");
  ASSERT_TRUE(normal) << normal.error().what();
  EXPECT_EQ(*normal, 3);
  auto expected = untouched.normal_double(0, 3);
  ASSERT_TRUE(expected);
  EXPECT_EQ(random.next_u64(), untouched.next_u64());
}

TEST(LuaTest, MatrixCodecRetainsRowsAndRejectsIncompleteTables) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  Mat3D const matrix(1, 2, 3, 4, 5, 6, 7, 8, 9);
  ASSERT_TRUE(context->set("matrix", matrix));
  auto rows = context->eval<Json>("return matrix");
  ASSERT_TRUE(rows) << rows.error().what();
  EXPECT_EQ(*rows, (Json(JsonArray{JsonArray{1, 2, 3}, JsonArray{4, 5, 6}, JsonArray{7, 8, 9}})));
  auto decoded = context->eval<Mat3D>("return matrix");
  ASSERT_TRUE(decoded) << decoded.error().what();
  EXPECT_EQ(*decoded, matrix);
  auto incomplete = context->eval<std::optional<Mat3D>>("return {{1,2,3},{4,5,6},{7,8}}");
  EXPECT_FALSE(incomplete);
  auto invalid = context->eval<std::optional<Mat3D>>("return {{1,2,3},{4,'bad',6},{7,8,9}}");
  EXPECT_FALSE(invalid);
  auto absent = context->eval<std::optional<Mat3D>>("return nil");
  ASSERT_TRUE(absent) << absent.error().what();
  EXPECT_FALSE(*absent);
}

TEST(LuaTest, CheckedTextPreservesDiagnosticSpellings) {
  for (auto const& [value, expected] : List<std::pair<LuaValue, std::string_view>>{
           {LuaValue(LuaNil), "nil"}, {LuaValue(LuaBoolean(true)), "true"},
           {LuaValue(LuaBoolean(false)), "false"}, {LuaValue(LuaInt(-42)), "-42"},
           {LuaValue(LuaFloat(1.23456789)), "1.23457"},
           {LuaValue(LuaFloat(-0.0)), "-0"}}) {
    TextBuffer output;
    auto written = write_text(output, value);
    static_assert(std::same_as<decltype(written), std::expected<void, StarException>>);
    ASSERT_TRUE(written);
    EXPECT_EQ(output.view(), expected);
    auto printed = print_lua_value(value);
    ASSERT_TRUE(printed);
    EXPECT_EQ(printed->utf8(), expected);
  }
}

TEST(LuaTest, CheckedTextRetainsStringNulAndNestedTableConventions) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto string = (*engine)->create_string(std::string_view("é\0suffix", 9));
  ASSERT_TRUE(string);
  TextBuffer text;
  ASSERT_TRUE(write_text(text, LuaValue(*string)));
  EXPECT_EQ(text.view(), "é");

  auto table = (*engine)->create_table();
  ASSERT_TRUE(table);
  ASSERT_TRUE(table->set("key", LuaInt(42)));
  TextBuffer output;
  ASSERT_TRUE(write_text(output, LuaValue(*table)));
  EXPECT_EQ(output.view(), "{key: 42}");
}

TEST(LuaTest, ImGuiRejectsSimulationCallsWithoutAbortingOrPoisoningLua) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  ASSERT_TRUE((*engine)->add_imgui());
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  auto rejected = context->eval("imgui.Begin('simulation must not draw')");
  ASSERT_FALSE(rejected);
  EXPECT_TRUE(rejected.error().is<LuaException>());
  ASSERT_TRUE(context->load("afterRejectedDrawing = 6 * 7"));
  EXPECT_EQ(context->get<int>("afterRejectedDrawing"), 42);
}

TEST(LuaTest, BasicGetSet) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;
  auto luaContextResult = luaEngine->create_context();
  ASSERT_TRUE(luaContextResult);
  auto luaContext = std::move(*luaContextResult);
  ASSERT_TRUE((luaContext.load(R"SCRIPT(
      data1 = 1.0
      data2 = 3.0 > 2.0
      data3 = "hello"
    )SCRIPT")));

  ASSERT_TRUE((luaContext.set("data4", 4.0)));

  EXPECT_EQ(luaContext.get<double>("data1"), 1.0);
  EXPECT_EQ(luaContext.get<bool>("data2"), true);
  EXPECT_EQ(luaContext.get<String>("data3"), "hello");
  EXPECT_EQ(luaContext.get<double>("data4"), 4.0);
}

TEST(LuaTest, EscapedLoadstringKeepsOnlyItsLuaEnvironment) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto observer = (*engine)->create_context();
  ASSERT_TRUE(observer);
  auto weak = observer->eval<LuaTable>("setmetatable({}, {__mode = 'v'})");
  ASSERT_TRUE(weak);
  ASSERT_TRUE(observer->set("seen", *weak));
  int retired_slot;
  {
    auto origin = (*engine)->create_context();
    ASSERT_TRUE(origin);
    retired_slot = origin->handle_index();
    ASSERT_TRUE(origin->set("seen", *weak));
    ASSERT_TRUE(origin->load("marker = 41; seen[1], seen[2] = _ENV, loadstring"));
    auto loader = origin->get<LuaFunction>("loadstring");
    ASSERT_TRUE(loader);
    ASSERT_TRUE(observer->set("escaped", *loader));
  }

  auto unrelated = (*engine)->create_table();
  ASSERT_TRUE(unrelated);
  ASSERT_EQ(unrelated->handle_index(), retired_slot);
  ASSERT_TRUE(unrelated->set("marker", 99));
  ASSERT_TRUE((*engine)->collect_garbage());
  EXPECT_EQ(observer->eval<bool>("seen[1] ~= nil and escaped('return _ENV')() == seen[1]"), true);
  EXPECT_EQ(observer->eval<int>("escaped('return marker', nil, nil)()"), 41);
  EXPECT_EQ(observer->eval<int>("escaped('return marker', 'explicit', {marker=7}, false)()"), 7);
  EXPECT_EQ(observer->eval<bool>("coroutine.wrap(function() return escaped('return marker')() == 41 end)()"), true);

  ASSERT_TRUE(observer->load("escaped = nil"));
  ASSERT_TRUE((*engine)->collect_garbage());
  EXPECT_EQ(observer->eval<bool>("seen[1] == nil and seen[2] == nil"), true);
}

TEST(LuaTest, LoadstringKeepsTypedArgumentAndTextOnlyContract) {
  auto engine = LuaEngine::create(false);
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  auto unsupported = (*engine)->create_raw_function(+[](lua_State* state) {
    lua_pushlightuserdata(state, state);
    return 1;
  });
  ASSERT_TRUE(unsupported);
  ASSERT_TRUE(context->set("unsupported", *unsupported));
  auto checked = context->load(R"(
    assert(not pcall(loadstring))
    assert(not pcall(loadstring, false))
    assert(not pcall(loadstring, "return 1", false))
    assert(not pcall(loadstring, "return 1", nil, false))
    local ok, message = pcall(loadstring, "return 1", nil, nil, unsupported())
    assert(not ok and message:find("Unsupported type", 1, true))
    ok, message = pcall(loadstring, 42, 17)
    assert(not ok and message:find("loadstring: 17", 1, true))
    assert(not pcall(loadstring, "return 1\0trailing"))
    assert(not pcall(loadstring, string.dump(function() return 1 end)))
    assert(select("#", loadstring("return 2", 3.5)) == 1)
  )");
  ASSERT_TRUE(checked) << checked.error().what();
}

TEST(LuaTest, TableReferences) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;
  auto luaContextResult = luaEngine->create_context();
  ASSERT_TRUE(luaContextResult);
  auto luaContext = std::move(*luaContextResult);
  ASSERT_TRUE((luaContext.load(R"SCRIPT(
      table = {foo=1, bar=2}
      tableRef = table
    )SCRIPT")));

  auto tableResult = luaContext.get<LuaTable>("table");
  ASSERT_TRUE(tableResult);
  auto table = std::move(*tableResult);
  auto tableRef1Result = luaContext.get<LuaTable>("tableRef");
  ASSERT_TRUE(tableRef1Result);
  auto tableRef1 = std::move(*tableRef1Result);
  auto tableRef2 = table;

  EXPECT_EQ(table.get<double>("foo"), 1.0);
  EXPECT_EQ(table.get<double>("bar"), 2.0);

  ASSERT_TRUE((table.set("baz", 3.0)));
  EXPECT_EQ(tableRef1.get<double>("baz"), 3.0);

  ASSERT_TRUE((tableRef1.set("baf", 4.0)));
  EXPECT_EQ(table.get<double>("baf"), 4.0);
  EXPECT_EQ(tableRef2.get<double>("baf"), 4.0);
}

TEST(LuaTest, FunctionCallTest) {
  std::weak_ptr<int> destructionObserver;

  {
    auto luaEngineResult = LuaEngine::create();
    ASSERT_TRUE(luaEngineResult);
    auto& luaEngine = *luaEngineResult;
    auto luaContextResult = luaEngine->create_context();
    ASSERT_TRUE(luaContextResult);
    auto luaContext = std::move(*luaContextResult);
    ASSERT_TRUE((luaContext.load(R"SCRIPT(
        function testFunc(arg1, arg2)
          return callback(3) + arg1 + arg2
        end

        function testEmpty()
          return emptyCallback()
        end
      )SCRIPT")));

    auto toDestruct = std::make_shared<int>();
    destructionObserver = toDestruct;
    auto callbackResult1 = luaEngine->create_function([toDestruct](double n) { return n * 2; });
    ASSERT_TRUE(callbackResult1);
    ASSERT_TRUE((luaContext.set("callback", *callbackResult1)));

    auto callbackResult2 = luaEngine->create_function([]() { return "heyooo"; });
    ASSERT_TRUE(callbackResult2);
    ASSERT_TRUE((luaContext.set("emptyCallback", *callbackResult2)));

    EXPECT_EQ(luaContext.invoke_path<double>("testFunc", 5.0, 10.0), 21.0);
    EXPECT_EQ(luaContext.invoke_path<String>("emptyCallback"), "heyooo");
  }

  EXPECT_TRUE(destructionObserver.expired());
}

TEST(LuaTest, CoroutineTest) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;
  auto luaContextResult = luaEngine->create_context();
  ASSERT_TRUE(luaContextResult);
  auto luaContext = std::move(*luaContextResult);

  ASSERT_TRUE((luaContext.load(R"SCRIPT(
      function accumulate(sum)
        return sum + callback(coroutine.yield(sum))
      end

      function run()
          local sum = 0
          for i=1,4 do
            sum = accumulate(sum)
          end
          return sum
      end

      co = coroutine.create(run)
    )SCRIPT")));

  auto callbackResult3 = luaEngine->create_function([](double num) { return num * 2; });
  ASSERT_TRUE(callbackResult3);
  ASSERT_TRUE((luaContext.set("callback", *callbackResult3)));

  auto threadResult = luaEngine->create_thread();
  ASSERT_TRUE(threadResult);
  auto thread = std::move(*threadResult);
  EXPECT_EQ(thread.status(), LuaThread::Status::Dead);
  auto funcResult = luaContext.get<LuaFunction>("run");
  ASSERT_TRUE(funcResult);
  auto func = std::move(*funcResult);
  ASSERT_TRUE((thread.push_function(func)));
  EXPECT_EQ(thread.status(), LuaThread::Status::Active);
  EXPECT_EQ(thread.resume<double>(), 0.0);
  EXPECT_EQ(thread.resume<double>(1.0), 2.0);
  EXPECT_EQ(thread.resume<double>(3.0), 8.0);
  EXPECT_EQ(thread.resume<double>(5.0), 18.0);
  EXPECT_EQ(thread.resume<double>(7.0), 32.0);
  // manually created threads are empty after execution is finished
  EXPECT_EQ(thread.status(), LuaThread::Status::Dead);

  ASSERT_TRUE((thread.push_function(func)));
  EXPECT_EQ(thread.resume<double>(), 0.0);
  EXPECT_EQ(thread.resume<double>(1.0), 2.0);
  {
    auto failure = thread.push_function(func);
    ASSERT_FALSE(failure);
    EXPECT_TRUE(failure.error().is<LuaException>());
  }; // pushing function to suspended or errored thread

  auto coroutineResult = luaContext.get<LuaThread>("co");
  ASSERT_TRUE(coroutineResult);
  auto coroutine = std::move(*coroutineResult);
  EXPECT_EQ(coroutine.status(), LuaThread::Status::Active);
  EXPECT_EQ(coroutine.resume<double>(), 0.0);
  EXPECT_EQ(coroutine.resume<double>(1.0), 2.0);
  EXPECT_EQ(coroutine.resume<double>(3.0), 8.0);
  EXPECT_EQ(coroutine.resume<double>(5.0), 18.0);
  EXPECT_EQ(coroutine.resume<double>(7.0), 32.0);
  EXPECT_EQ(coroutine.status(), LuaThread::Status::Dead);
  {
    auto failure = coroutine.resume();
    ASSERT_FALSE(failure);
    EXPECT_TRUE(failure.error().is<LuaException>());
  };
  EXPECT_EQ(coroutine.status(), LuaThread::Status::Dead);

  auto thread2Result = luaEngine->create_thread();
  ASSERT_TRUE(thread2Result);
  auto thread2 = std::move(*thread2Result);
  EXPECT_EQ(thread2.status(), LuaThread::Status::Dead);
  ASSERT_TRUE((thread2.push_function(func)));
  EXPECT_EQ(thread2.status(), LuaThread::Status::Active);
  EXPECT_EQ(thread2.resume<double>(), 0.0);
  EXPECT_EQ(thread2.resume<double>(1.0), 2.0);
  {
    auto failure = thread2.resume<String>("not_a_number");
    ASSERT_FALSE(failure);
    EXPECT_TRUE(failure.error().is<LuaException>());
  };
  EXPECT_EQ(thread2.status(), LuaThread::Status::Error);
  {
    auto failure = thread2.resume();
    ASSERT_FALSE(failure);
    EXPECT_TRUE(failure.error().is<LuaException>());
  };
  EXPECT_EQ(thread2.status(), LuaThread::Status::Error);
}

template <typename T>
testing::AssertionResult roundTripEqual(LuaContext const& context, T t) {
  auto result = context.invoke_path<T>("roundTrip", t);
  if (!result)
    return testing::AssertionFailure() << result.error().what();
  if (*result != t)
    return testing::AssertionFailure() << "Lua round trip returned a different value";
  return testing::AssertionSuccess();
}

TEST(LuaTest, Converters) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;
  auto luaContextResult = luaEngine->create_context();
  ASSERT_TRUE(luaContextResult);
  auto luaContext = std::move(*luaContextResult);

  ASSERT_TRUE((luaContext.load( R"SCRIPT(
      function makeVec()
        return {1, 2}
      end

      function makePoly()
        return {{1, 2}, {3, 4}, {5, 6}}
      end

      function roundTrip(ret)
        return ret
      end
    )SCRIPT")));

  auto vecCompare = Vec2F(1.0f, 2.0f);
  auto polyCompare = PolyF({Vec2F(1.0f, 2.0f), Vec2F(3.0f, 4.0f), Vec2F(5.0f, 6.0f)});

  EXPECT_TRUE(luaContext.invoke_path<Vec2F>("makeVec") == vecCompare);
  EXPECT_TRUE(luaContext.invoke_path<PolyF>("makePoly") == polyCompare);
  EXPECT_TRUE(luaContext.invoke_path<Vec2F>("roundTrip", vecCompare) == vecCompare);
  EXPECT_TRUE(luaContext.invoke_path<PolyF>("roundTrip", polyCompare) == polyCompare);

  EXPECT_TRUE(roundTripEqual(luaContext, PolyF()));
  EXPECT_TRUE(roundTripEqual(luaContext, List<int>{1, 2, 3, 4}));
  EXPECT_TRUE(roundTripEqual(luaContext, List<PolyF>{PolyF(), PolyF()}));
  EXPECT_TRUE(roundTripEqual(luaContext, std::optional<int>(1)));
  EXPECT_TRUE(roundTripEqual(luaContext, std::optional<int>()));

  auto listCompare = List<int>{1, 2, 3, 4};
  EXPECT_TRUE(luaContext.invoke_path<List<int>>("roundTrip", JsonArray{1, 2, 3, 4}) == listCompare);
  auto mapCompare = StringMap<String>{{"one", "two"}, {"three", "four"}};
  EXPECT_TRUE(luaContext.invoke_path<StringMap<String>>("roundTrip", JsonObject{{"one", "two"}, {"three", "four"}}) == mapCompare);
}

TEST(LuaTest, OptionalConversionDistinguishesNilFromFailure) {
  auto engineResult = LuaEngine::create();
  ASSERT_TRUE(engineResult);
  auto& engine = *engineResult;
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  auto nilValue = context.eval<LuaValue>("nil");
  ASSERT_TRUE(nilValue);
  auto nil = engine->lua_maybe_to<std::optional<int>>(*nilValue);
  ASSERT_TRUE(nil);
  ASSERT_TRUE(*nil);
  EXPECT_FALSE(**nil);
  auto numberValue = context.eval<LuaValue>("17");
  ASSERT_TRUE(numberValue);
  auto number = engine->lua_maybe_to<std::optional<int>>(*numberValue);
  ASSERT_TRUE(number);
  ASSERT_TRUE(number);
  ASSERT_TRUE(*number);
  ASSERT_TRUE(**number);
  EXPECT_EQ(***number, 17);
  auto booleanValue = context.eval<LuaValue>("false");
  ASSERT_TRUE(booleanValue);
  auto boolean = engine->lua_maybe_to<std::optional<bool>>(*booleanValue);
  ASSERT_TRUE(boolean);
  ASSERT_TRUE(boolean);
  ASSERT_TRUE(*boolean);
  ASSERT_TRUE(**boolean);
  EXPECT_FALSE(***boolean);
  auto invalidValue = context.eval<LuaValue>("{}");
  ASSERT_TRUE(invalidValue);
  auto invalid = engine->lua_maybe_to<std::optional<int>>(*invalidValue);
  ASSERT_TRUE(invalid);
  EXPECT_FALSE(*invalid);
  {
    auto failure = context.eval<std::optional<int>>("{}");
    ASSERT_FALSE(failure);
    EXPECT_TRUE(failure.error().is<LuaConversionException>());
  };
}

TEST(LuaTest, OptionalRvalueIsConsumedBeforeForeignEnginePushFailure) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  auto function = context->eval<LuaFunction>("function() return 42 end");
  ASSERT_TRUE(function);
  auto foreignEngine = LuaEngine::create();
  ASSERT_TRUE(foreignEngine);

  std::optional<LuaFunction> value(*function);
  auto borrowedFailure = (*foreignEngine)->lua_from(value);
  ASSERT_FALSE(borrowedFailure);
  EXPECT_TRUE(borrowedFailure.error().is<LuaException>());
  EXPECT_TRUE(value);

  auto consumedFailure = (*foreignEngine)->lua_from(std::move(value));
  ASSERT_FALSE(consumedFailure);
  EXPECT_TRUE(consumedFailure.error().is<LuaException>());
  EXPECT_FALSE(value);

  std::optional<LuaFunction> successfulValue(*function);
  auto pushed = (*engine)->lua_from(std::move(successfulValue));
  ASSERT_TRUE(pushed);
  EXPECT_FALSE(successfulValue);
  auto roundTrip = (*engine)->lua_to<LuaFunction>(*pushed);
  ASSERT_TRUE(roundTrip);
  EXPECT_EQ(roundTrip->invoke<int>(), 42);
}

struct TestUserData1 {
  int field;
};

struct TestUserData2 {
  int field;
};

namespace Star {
template <>
struct LuaConverter<TestUserData1> : LuaUserDataConverter<TestUserData1> {};

template <>
struct LuaConverter<TestUserData2> : LuaUserDataConverter<TestUserData2> {};
}

TEST(LuaTest, UserDataTest) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;

  auto luaContextResult = luaEngine->create_context();
  ASSERT_TRUE(luaContextResult);
  auto luaContext = std::move(*luaContextResult);
  ASSERT_TRUE((luaContext.load(
      R"SCRIPT(
        function doit(ref)
          heldValue = ref
        end
      )SCRIPT")));

  auto userdata1Result = luaEngine->create_userdata(TestUserData1{1});
  ASSERT_TRUE(userdata1Result);
  auto userdata1 = std::move(*userdata1Result);
  auto userdata2Result = luaEngine->create_userdata(TestUserData2{2});
  ASSERT_TRUE(userdata2Result);
  auto userdata2 = std::move(*userdata2Result);

  ASSERT_TRUE((luaContext.invoke_path("doit", userdata1)));
  auto userdata3Result = luaContext.get<LuaUserData>("heldValue");
  ASSERT_TRUE(userdata3Result);
  auto userdata3 = std::move(*userdata3Result);

  EXPECT_EQ(userdata2.is<TestUserData2>(), true);
  EXPECT_EQ(userdata2.is<TestUserData1>(), false);

  EXPECT_EQ(userdata1.is<TestUserData1>(), true);
  EXPECT_EQ(userdata1.is<TestUserData2>(), false);

  EXPECT_EQ(userdata3.is<TestUserData1>(), true);
  EXPECT_EQ(userdata3.is<TestUserData2>(), false);

  auto data1 = userdata1.get<TestUserData1>();
  auto data2 = userdata2.get<TestUserData2>();
  auto data3 = userdata3.get<TestUserData1>();
  ASSERT_TRUE(data1);
  ASSERT_TRUE(data2);
  ASSERT_TRUE(data3);
  EXPECT_EQ(data1->get().field, 1);
  EXPECT_EQ(data2->get().field, 2);
  EXPECT_EQ(data3->get().field, 1);

  data1->get().field = 3;
  EXPECT_EQ(data1->get().field, 3);
  EXPECT_EQ(data3->get().field, 3);

  ASSERT_TRUE((luaContext.invoke_path("doit", TestUserData1())));
  auto userdata4Result = luaContext.get("heldValue");
  ASSERT_TRUE(userdata4Result);
  auto userdata4 = std::move(*userdata4Result);
  auto convertedUserdata = luaEngine->lua_maybe_to<TestUserData1>(userdata4);
  ASSERT_TRUE(convertedUserdata);
  EXPECT_TRUE(*convertedUserdata);

  ASSERT_TRUE((luaContext.invoke_path("doit", "notuserdata")));
  auto notuserdataResult = luaContext.get("heldValue");
  ASSERT_TRUE(notuserdataResult);
  auto notuserdata = std::move(*notuserdataResult);
  auto rejectedUserdata = luaEngine->lua_maybe_to<TestUserData1>(notuserdata);
  ASSERT_TRUE(rejectedUserdata);
  EXPECT_FALSE(*rejectedUserdata);
}

namespace Star {
template <>
struct LuaConverter<Vec3F> : LuaUserDataConverter<Vec3F> {};

template <>
struct LuaUserDataMethods<Vec3F> {
  [[nodiscard]] static std::expected<LuaMethods<Vec3F>, StarException> make() {
    LuaMethods<Vec3F> methods;
    auto registered = methods.register_method_with_signature<float, Vec3F const&>("magnitude", std::mem_fn(&Vec3F::magnitude));
    if (!registered)
      return std::unexpected(std::move(registered.error()));
    return methods;
  }
};
}

TEST(LuaTest, UserMethodTest) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;
  auto callbackResult4 = luaEngine->create_function_with_signature<Vec3F, float, float, float>(Construct<Vec3F>());
  ASSERT_TRUE(callbackResult4);
  ASSERT_TRUE((luaEngine->set_global("vec3", *callbackResult4)));

  auto luaContextResult = luaEngine->create_context();
  ASSERT_TRUE(luaContextResult);
  auto luaContext = std::move(*luaContextResult);
  ASSERT_TRUE((luaContext.load(R"SCRIPT(
      v = vec3(3, 2, 1)
      function testMagnitude(v2)
        return v:magnitude() + v2:magnitude()
      end
    )SCRIPT")));

  auto magnitudeResult = luaContext.invoke_path<float>("testMagnitude", Vec3F(5, 5, 5));
  ASSERT_TRUE(magnitudeResult);
  auto magnitude = std::move(*magnitudeResult);
  EXPECT_TRUE(std::fabs(magnitude - (Vec3F(3, 2, 1).magnitude() + Vec3F(5, 5, 5).magnitude())) < 0.00001f);
}

TEST(LuaTest, GlobalTest) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;
  ASSERT_TRUE((luaEngine->set_global("globalfoo", LuaInt(42))));
  expectLuaAlternative<LuaInt>(luaEngine->get_global("globalfoo"), 42);

  auto luaContextResult = luaEngine->create_context();
  ASSERT_TRUE(luaContextResult);
  auto luaContext = std::move(*luaContextResult);
  ASSERT_TRUE((luaContext.load(R"SCRIPT(
      function test()
        return globalfoo
      end
    )SCRIPT")));

  expectLuaAlternative<LuaInt>(luaContext.invoke_path("test"), 42);
}

TEST(LuaTest, ArgTest) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;

  auto luaContextResult = luaEngine->create_context();
  ASSERT_TRUE(luaContextResult);
  auto luaContext = std::move(*luaContextResult);
  ASSERT_TRUE((luaContext.load(R"SCRIPT(
      function test()
        callback("2", 3, nil)
      end
    )SCRIPT")));

  auto callbackResult5 = luaEngine->create_function([](LuaFloat n, LuaString s, LuaBoolean b, LuaValue o) {
        EXPECT_EQ(n, 2.0);
        EXPECT_EQ(s.to_string(), String("3"));
        EXPECT_EQ(b, false);
        EXPECT_TRUE(std::holds_alternative<LuaNilType>(o));
      });
  ASSERT_TRUE(callbackResult5);
  ASSERT_TRUE((luaContext.set("callback",
      *callbackResult5)));

  ASSERT_TRUE((luaContext.invoke_path("test")));
}

TEST(LuaTest, ArrayTest) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;
  auto luaContextResult = luaEngine->create_context();
  ASSERT_TRUE(luaContextResult);
  auto luaContext = std::move(*luaContextResult);
  ASSERT_TRUE((luaContext.load(R"SCRIPT(
      function test()
        return {2, 4, 6, 8, 10}
      end
    )SCRIPT")));

  auto arrayValue = luaContext.invoke_path("test");
  ASSERT_TRUE(arrayValue);
  ASSERT_TRUE(std::holds_alternative<LuaTable>(*arrayValue));
  auto& arrayTable = std::get<LuaTable>(*arrayValue);

  EXPECT_EQ(arrayTable.length(), 5);
  expectLuaAlternative<LuaInt>(arrayTable.get(2), 4);
  expectLuaAlternative<LuaInt>(arrayTable.get(5), 10);

  List<std::pair<int, int>> values;
  ASSERT_TRUE((arrayTable.iterate([&values](LuaValue const& key, LuaValue const& value) -> std::expected<void, StarException> {
    if (!std::holds_alternative<LuaInt>(key) || !std::holds_alternative<LuaInt>(value))
      return std::unexpected(LuaConversionException("Expected integer array key and value"));
    values.push_back({std::get<LuaInt>(key), std::get<LuaInt>(value)});
    return {};
  })));
  List<std::pair<int, int>> compare = {{1, 2}, {2, 4}, {3, 6}, {4, 8}, {5, 10}};
  EXPECT_EQ(values, compare);
}

TEST(LuaTest, PathTest) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;
  auto luaContextResult = luaEngine->create_context();
  ASSERT_TRUE(luaContextResult);
  auto luaContext = std::move(*luaContextResult);
  ASSERT_TRUE((luaContext.load(R"SCRIPT(
      foo = {
          bar = {
            baz = 1
          }
        }

      function test()
        return foo.bar.baf
      end
    )SCRIPT")));

  EXPECT_EQ(luaContext.contains_path("foo.bar.baz"), true);
  expectLuaAlternative<LuaInt>(luaContext.get_path("foo.bar.baz"), 1);
  EXPECT_EQ(luaContext.contains_path("foo.nothing.at.all"), false);

  ASSERT_TRUE((luaContext.set_path("foo.bar.baf", LuaInt(5))));
  expectLuaAlternative<LuaInt>(luaContext.invoke_path("test"), 5);

  ASSERT_TRUE((luaContext.set_path("new.table.value", LuaInt(5))));
  expectLuaAlternative<LuaInt>(luaContext.get_path("new.table.value"), 5);
}

TEST(LuaTest, DottedPathsUseCompleteByteKeysAndTableMetamethods) {
  using namespace std::string_view_literals;
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->load(R"LUA(
    backing = {part = {leaf = 7}}
    backing["call\0me"] = function(n) return n + 1 end
    proxy = setmetatable({}, {
      __index = function(_, k)
        if k == "read_error" then error("read marker") end
        return backing[k]
      end,
      __newindex = function(_, k, v)
        if k == "write_error" then error("write marker") end
        backing[k] = v
      end,
    })
  )LUA"));
  ASSERT_TRUE(context->set_path("proxy.part\0more.leaf\0tail"sv, 19));
  EXPECT_EQ(context->eval<int>(R"(backing["part\0more"]["leaf\0tail"])"), 19);
  EXPECT_EQ(context->get_path<int>("proxy.part.leaf"), 7);
  EXPECT_EQ(context->get_path<int>("proxy.part\0more.leaf\0tail"sv), 19);
  EXPECT_EQ(context->contains_path("proxy.part\0more.leaf"sv), false);
  EXPECT_EQ(context->invoke_path<int>("proxy.call\0me"sv, 41), 42);
  auto called = context->invoke_path_if_present<int>("proxy.call\0me"sv, 42);
  ASSERT_TRUE(called);
  EXPECT_EQ(*called, 43);
  ASSERT_TRUE(context->set_path("proxy.\0"sv, 23));
  EXPECT_EQ(context->eval<int>(R"(backing["\0"])"), 23);

  ASSERT_TRUE(context->set_path("..", 29));
  EXPECT_EQ(context->eval<int>(R"(_ENV[""][""][""])"), 29);
  EXPECT_EQ(context->get_path<int>(".."), 29);
  ASSERT_TRUE(context->set_path(std::string_view{}, false));
  EXPECT_EQ(context->contains_path(""), true);
  EXPECT_EQ(context->get_path<bool>(""), false);
  expectLuaNil(context->get_path(".."));
  EXPECT_FALSE(context->set_path("..", 0));
  EXPECT_FALSE(context->get_path<int>("proxy.read_error.x"));
  EXPECT_FALSE(context->set_path("proxy.write_error.x", 1));
  EXPECT_FALSE(context->set_path("proxy.write_error", 1));
  EXPECT_EQ(context->get_path<int>("proxy.part.leaf"), 7);
}

TEST(LuaTest, TypedPathReadsBorrowUntilAnOwnedResultIsRequested) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  // Vector conversion reads both coordinates through __index. Its input must
  // be rooted on the stack, but not promoted to a temporary registry handle.
  auto coordinate = (*engine)->create_raw_function(+[](lua_State* state) {
    lua_pushnil(state);
    while (lua_next(state, LUA_REGISTRYINDEX)) {
      if (lua_rawequal(state, 1, -1))
        return luaL_error(state, "path lookup promoted its vector to the registry");
      lua_pop(state, 1);
    }
    lua_pushvalue(state, 2);
    return 1;
  });
  ASSERT_TRUE(coordinate);
  ASSERT_TRUE(context->set("coordinate", *coordinate));
  auto unsupported = (*engine)->create_raw_function(+[](lua_State* state) {
    lua_pushlightuserdata(state, state);
    lua_newuserdatauv(state, 1, 0); // Full userdata without a metatable is unsupported too.
    return 2;
  });
  ASSERT_TRUE(unsupported);
  ASSERT_TRUE(context->set("unsupported", *unsupported));
  ASSERT_TRUE(context->load(R"LUA(
    result = {vector = setmetatable({}, {__index = coordinate}), count = 31,
              f = function() return 37 end}
    result.light, result.bare = unsupported()
  )LUA"));
  EXPECT_EQ(context->get_path<Vec2I>("result.vector"), Vec2I(1, 2));
  auto missing = context->get_path<std::optional<int>>("result.absent");
  ASSERT_TRUE(missing);
  EXPECT_FALSE(*missing);
  auto wrong = context->get_path<LuaFunction>("result.count");
  ASSERT_FALSE(wrong);
  EXPECT_TRUE(wrong.error().is<LuaConversionException>());
  for (auto path : {"result.light", "result.bare"}) {
    auto native = context->get_path<bool>(path);
    ASSERT_FALSE(native);
    EXPECT_FALSE(native.error().is<LuaConversionException>());
    EXPECT_FALSE(context->get_path(path));
    EXPECT_FALSE(context->contains_path(path));
  }
  auto owned = context->get_path<LuaFunction>("result.f");
  ASSERT_TRUE(owned);
  ASSERT_TRUE(context->set_path("result", LuaNil));
  ASSERT_TRUE((*engine)->collect_garbage());
  EXPECT_EQ(owned->invoke<int>(), 37);
}

TEST(LuaTest, CoroutineInstallsRetainedAndRevocableCallbacks) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto& engine = **created;
  auto context = engine.create_context();
  ASSERT_TRUE(context);
  LuaCallbacks values;
  ASSERT_TRUE(values.register_callback("vector", [] { return Vec3F(3, 4, 0); }));
  LuaCallbacks factory;
  ASSERT_TRUE(factory.register_callback("make", [&values]() -> LuaCallbacks const& { return values; }));
  std::stop_source lifetime;
  ASSERT_TRUE(context->set_callbacks("factory", factory.with_lifetime(lifetime.get_token())));
  ASSERT_TRUE(context->set_callbacks("factory", values));
  ASSERT_TRUE(context->set("occupied", false));
  ASSERT_TRUE(context->set_callbacks("occupied", values));
  auto installed = context->load(R"LUA(
    assert(occupied == false)
    local co = coroutine.create(function()
      local group = factory.make()
      local v = group.vector()
      assert(v:magnitude() == 5)
      return group.vector, v.magnitude
    end)
    local ok, vector, magnitude = coroutine.resume(co)
    assert(ok, vector)
    saved_vector, saved_method = vector, magnitude
    factory = nil
  )LUA");
  ASSERT_TRUE(installed) << installed.error().what();
  ASSERT_TRUE(engine.collect_garbage());
  EXPECT_EQ(context->eval<int>("saved_method(saved_vector())"), 5);
  ASSERT_TRUE(lifetime.request_stop());
  auto revoked = context->eval<LuaValue>("saved_vector()");
  ASSERT_FALSE(revoked);
  EXPECT_NE(std::string_view(revoked.error().what()).find("scope is retired"), std::string_view::npos);
  EXPECT_EQ(context->eval<int>("6 * 7"), 42);
}

TEST(LuaTest, CallbackTest) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;

  LuaCallbacks callbacks;
  ASSERT_TRUE((callbacks.register_callback("add", [](LuaInt a, LuaInt b) { return a + b; })));
  ASSERT_TRUE((callbacks.register_callback_with_signature<LuaInt, LuaInt, LuaInt>("subtract", [](int a, int b) { return a - b; })));
  ASSERT_TRUE((callbacks.register_callback_with_signature<LuaInt, LuaInt>("multiply2", std::bind([](int a, int b) { return a * b; }, 2, std::placeholders::_1))));
  ASSERT_TRUE((callbacks.register_callback_with_signature<void, LuaValue>("nothing", [](LuaValue v) { return v; })));

  auto luaContextResult = luaEngine->create_context();
  ASSERT_TRUE(luaContextResult);
  auto luaContext = std::move(*luaContextResult);
  ASSERT_TRUE((luaContext.set_callbacks("callbacks", callbacks)));
  ASSERT_TRUE((luaContext.load(R"SCRIPT(
      function test1()
        return callbacks.multiply2(callbacks.add(5, 10) + callbacks.subtract(3, 10))
      end

      function test2()
        return callbacks.nothing(1)
      end
    )SCRIPT")));

  expectLuaAlternative<LuaInt>(luaContext.invoke_path("test1"), 16);
  expectLuaNil(luaContext.invoke_path("test2"));
}

struct CallbackReturnProbe {
  int value;

  explicit operator int() const& { return value; }
  explicit operator int() && { return -std::exchange(value, 0); }
};

namespace Star {
template <> struct LuaConverter<CallbackReturnProbe> {
  static std::expected<void, StarException> from(LuaStack stack, CallbackReturnProbe const& probe) {
    return stack.push(static_cast<int>(probe));
  }
  static std::expected<void, StarException> from(LuaStack stack, CallbackReturnProbe&& probe) {
    return stack.push(static_cast<int>(std::move(probe)));
  }
};
}

TEST(LuaTest, CallbackReturnsForwardOnlyOwnedValues) {
  using Probe = CallbackReturnProbe;
  using Result = std::expected<Probe, StarException>;
  Probe borrowed{2};
  Result checked{Probe{4}};
  Result failure = std::unexpected(StarException("borrowed-error-marker", false));
  LuaVariadic<Probe> values{Probe{8}, Probe{9}};
  auto tuple = lua_tuple_return(Probe{10}, Probe{11});

  LuaCallbacks callbacks;
  ASSERT_TRUE(callbacks.register_callback("owned", [] { return Probe{1}; }));
  ASSERT_TRUE(callbacks.register_callback("borrowed", [&]() -> Probe& { return borrowed; }));
  ASSERT_TRUE(callbacks.register_callback("owned_result", []() -> Result { return Probe{3}; }));
  ASSERT_TRUE(callbacks.register_callback("borrowed_result", [&]() -> Result& { return checked; }));
  ASSERT_TRUE(callbacks.register_callback("const_result", [&]() -> Result const& { return checked; }));
  ASSERT_TRUE(callbacks.register_callback_with_signature<int>("coerce_owned", []() -> Result { return Probe{5}; }));
  ASSERT_TRUE(callbacks.register_callback_with_signature<int>("coerce_borrowed", [&]() -> Result& { return checked; }));
  ASSERT_TRUE(callbacks.register_callback("mixed", [&] {
    return LuaTupleReturn<Probe, Probe&>(Probe{6}, borrowed);
  }));
  ASSERT_TRUE(callbacks.register_callback("tuple", [&]() -> auto const& { return tuple; }));
  ASSERT_TRUE(callbacks.register_callback("variadic", [] { return LuaVariadic<Probe>{Probe{7}, Probe{8}}; }));
  ASSERT_TRUE(callbacks.register_callback("borrowed_variadic", [&]() -> auto& { return values; }));
  ASSERT_TRUE(callbacks.register_callback("flags", [] { return LuaVariadic<bool>{true, false}; }));
  ASSERT_TRUE(callbacks.register_callback("failure", [&]() -> Result& { return failure; }));
  ASSERT_TRUE(callbacks.register_callback("nothing", []() -> std::expected<void, StarException> { return {}; }));

  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->set_callbacks("r", callbacks));
  auto result = context->load(R"LUA(
    assert(r.owned() == -1 and r.borrowed() == 2)
    assert(r.owned_result() == -3 and r.borrowed_result() == 4 and r.const_result() == 4)
    assert(r.coerce_owned() == -5 and r.coerce_borrowed() == 4)
    local x, y = r.mixed()
    assert(x == -6 and y == 2)
    x, y = r.tuple()
    assert(x == 10 and y == 11)
    x, y = r.variadic()
    assert(x == -7 and y == -8)
    x, y = r.borrowed_variadic()
    assert(x == 8 and y == 9)
    x, y = r.flags()
    assert(x == true and y == false)
    assert(select('#', r.nothing()) == 1 and r.nothing() == nil)
  )LUA");
  ASSERT_TRUE(result) << result.error().what();
  EXPECT_EQ(borrowed.value, 2);
  EXPECT_EQ(checked->value, 4);
  EXPECT_EQ(values.front().value, 8);
  auto failed = context->eval<int>("r.failure()");
  ASSERT_FALSE(failed);
  EXPECT_NE(std::string_view(failed.error().what()).find("borrowed-error-marker"), std::string_view::npos);
  EXPECT_NE(std::string_view(failure.error().what()).find("borrowed-error-marker"), std::string_view::npos);
}

TEST(LuaTest, VariableParameters) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;
  auto context1Result = luaEngine->create_context();
  ASSERT_TRUE(context1Result);
  auto context1 = std::move(*context1Result);

  ASSERT_TRUE((context1.load(R"SCRIPT(
      function variableArgsCount(...)
        local arg = {...}
        return #arg
      end
    )SCRIPT")));

  auto res1Result = context1.invoke_path<int>("variableArgsCount");
  ASSERT_TRUE(res1Result);
  auto res1 = std::move(*res1Result);
  auto res2Result = context1.invoke_path<int>("variableArgsCount", 1);
  ASSERT_TRUE(res2Result);
  auto res2 = std::move(*res2Result);
  auto res3Result = context1.invoke_path<int>("variableArgsCount", 1, 1);
  ASSERT_TRUE(res3Result);
  auto res3 = std::move(*res3Result);
  auto res4Result = context1.invoke_path<int>("variableArgsCount", 1, 1, 1);
  ASSERT_TRUE(res4Result);
  auto res4 = std::move(*res4Result);

  EXPECT_EQ(res1, 0);
  EXPECT_EQ(res2, 1);
  EXPECT_EQ(res3, 2);
  EXPECT_EQ(res4, 3);
}

TEST(LuaTest, Scope) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;
  auto script1Result = luaEngine->compile(R"SCRIPT(
      function create(param)
        local self = {}
        local foo = param

        local getValue = function()
          return foo
        end

        function self.get()
          return getValue()
        end

        return self
      end
    )SCRIPT");
  ASSERT_TRUE(script1Result);
  auto script1 = std::move(*script1Result);

  auto script2Result = luaEngine->compile(R"SCRIPT(
      function init()
        obj = create(param)
      end

      function produce()
        return obj.get()
      end
    )SCRIPT");
  ASSERT_TRUE(script2Result);
  auto script2 = std::move(*script2Result);

  auto context1Result = luaEngine->create_context();
  ASSERT_TRUE(context1Result);
  auto context1 = std::move(*context1Result);
  ASSERT_TRUE((context1.load(script1)));
  ASSERT_TRUE((context1.load(script2)));

  auto context2Result = luaEngine->create_context();
  ASSERT_TRUE(context2Result);
  auto context2 = std::move(*context2Result);
  ASSERT_TRUE((context2.load(script1)));
  ASSERT_TRUE((context2.load(script2)));

  ASSERT_TRUE((context1.set_path("param", 1)));
  ASSERT_TRUE((context1.invoke_path("init")));

  ASSERT_TRUE((context2.set_path("param", 2)));
  ASSERT_TRUE((context2.invoke_path("init")));

  EXPECT_EQ(context1.invoke_path<int>("produce"), 1);
  EXPECT_EQ(context2.invoke_path<int>("produce"), 2);

  ASSERT_TRUE((context1.set_path("param", 2)));
  ASSERT_TRUE((context1.invoke_path("init")));

  ASSERT_TRUE((context2.set_path("param", 1)));
  ASSERT_TRUE((context2.invoke_path("init")));

  EXPECT_EQ(context1.invoke_path<int>("produce"), 2);
  EXPECT_EQ(context2.invoke_path<int>("produce"), 1);
}

TEST(LuaTest, Scope2) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;

  auto context1Result = luaEngine->create_context();
  ASSERT_TRUE(context1Result);
  auto context1 = std::move(*context1Result);
  ASSERT_TRUE((context1.load(R"SCRIPT(
      function init1()
        scopeState = {}
        scopeState.val = 10
      end
    )SCRIPT")));

  auto context2Result = luaEngine->create_context();
  ASSERT_TRUE(context2Result);
  auto context2 = std::move(*context2Result);
  ASSERT_TRUE((context2.load(R"SCRIPT(
      function init2()
        scopeState = {}
        scopeState.val = 20
      end
    )SCRIPT")));

  EXPECT_EQ(context1.contains("init1"), true);
  EXPECT_EQ(context2.contains("init2"), true);
  EXPECT_EQ(context1.contains("init2"), false);
  EXPECT_EQ(context2.contains("init1"), false);

  ASSERT_TRUE((context1.invoke_path("init1")));
  EXPECT_EQ(context1.get_path<int>("scopeState.val"), 10);

  expectLuaNil(context2.get_path("scopeState"));

  ASSERT_TRUE((context2.invoke_path("init2")));
  EXPECT_EQ(context2.get_path<int>("scopeState.val"), 20);

  EXPECT_EQ(context1.get_path<int>("scopeState.val"), 10);
}

TEST(LuaTest, MetaTable) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;

  auto contextResult = luaEngine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  ASSERT_TRUE((context.load(R"SCRIPT(
      function add(a, b)
        return a + b
      end
    )SCRIPT")));

  auto mtResult = luaEngine->create_table();
  ASSERT_TRUE(mtResult);
  auto mt = std::move(*mtResult);
  auto callbackResult6 = luaEngine->create_function([](LuaEngine& engine, LuaTable const& a, LuaTable const& b) -> std::expected<LuaTable, StarException> {
        auto ax = a.get<double>(1);
        if (!ax) return std::unexpected(std::move(ax.error()));
        auto ay = a.get<double>(2);
        if (!ay) return std::unexpected(std::move(ay.error()));
        auto bx = b.get<double>(1);
        if (!bx) return std::unexpected(std::move(bx.error()));
        auto by = b.get<double>(2);
        if (!by) return std::unexpected(std::move(by.error()));
        return engine.create_array_table(std::initializer_list<double>{*ax + *bx, *ay + *by});
      });
  ASSERT_TRUE(callbackResult6);
  ASSERT_TRUE((mt.set("__add",
      *callbackResult6)));
  ASSERT_TRUE((mt.set("test", "hello")));

  auto t1Result = luaEngine->create_array_table(std::initializer_list<double>{1, 2});
  ASSERT_TRUE(t1Result);
  auto t1 = std::move(*t1Result);
  ASSERT_TRUE(t1.set_metatable(mt));

  auto t2Result = luaEngine->create_array_table(std::initializer_list<double>{5, 6});
  ASSERT_TRUE(t2Result);
  auto t2 = std::move(*t2Result);
  ASSERT_TRUE(t2.set_metatable(mt));

  auto trResult = context.invoke_path<LuaTable>("add", t1, t2);
  ASSERT_TRUE(trResult);
  auto tr = std::move(*trResult);
  EXPECT_EQ(tr.get<double>(1), 6);
  EXPECT_EQ(tr.get<double>(2), 8);
  auto metatable1 = t1.get_metatable();
  ASSERT_TRUE(metatable1);
  ASSERT_TRUE(*metatable1);
  EXPECT_EQ((*metatable1)->get<String>("test"), "hello");
  auto metatable2 = t2.get_metatable();
  ASSERT_TRUE(metatable2);
  ASSERT_TRUE(*metatable2);
  EXPECT_EQ((*metatable2)->get<String>("test"), "hello");
}

TEST(LuaTest, Integers) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;
  auto contextResult = luaEngine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  ASSERT_TRUE((context.load(R"SCRIPT(
      n1 = 0
      n2 = 1
      n3 = 1.0
      n4 = 1.1
      n5 = 5.0
      n6 = 5
    )SCRIPT")));

  expectLuaAlternative<LuaInt>(context.get("n1"), 0);
  expectLuaAlternative<LuaInt>(context.get("n2"), 1);
  expectLuaAlternative<LuaFloat>(context.get("n3"), 1.0);
  expectLuaAlternative<LuaFloat>(context.get("n4"), 1.1);
  expectLuaAlternative<LuaFloat>(context.get("n5"), 5.0);
  expectLuaAlternative<LuaInt>(context.get("n6"), 5);
}

TEST(LuaTest, Require) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;
  auto contextResult = luaEngine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  ASSERT_TRUE(
    (context.set_require_function([](LuaStackValue environment, StringView arg) -> std::expected<void, StarException> {
      auto callback = environment.stack().engine().create_function([arg = String(arg.utf8())]() { return arg; });
      if (!callback)
        return std::unexpected(std::move(callback.error()));
      return environment.set(arg.utf8(), *callback);
    })));

  ASSERT_TRUE((context.load(R"SCRIPT(
      require "a"
      require "b"
      require "c"
      requireResultCount = select("#", require "c")

      function res()
        return a() .. b() .. c()
      end
    )SCRIPT")));

  auto required = context.invoke_path<LuaString>("res");
  ASSERT_TRUE(required);
  EXPECT_EQ(required->to_string(), String("abc"));
  EXPECT_EQ(context.get<int>("requireResultCount"), 0);
  auto invalid = context.load("require {}");
  ASSERT_FALSE(invalid);
  EXPECT_TRUE(invalid.error().is<LuaException>());
}

TEST(LuaTest, SourceProvidersUseFullLiveStackAndDeduplicate) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto engine = *created;
  String helperPath("/scripts/" + std::string(240, 'x') + "/helper.lua");
  int entryLookups = 0;
  int helperLookups = 0;
  engine->set_source_resolver([&](String const& source) -> std::expected<String, StarException> {
    if (source == "/entry.lua") {
      ++entryLookups;
      return String("friendly-entry / internal-entry / /mods/entry");
    }
    if (source == helperPath) {
      ++helperLookups;
      return String("friendly-helper / internal-helper / /mods/override");
    }
    return String();
  });
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  ASSERT_TRUE((context.set_require_function([&](LuaStackValue environment, StringView) {
    constexpr std::string_view helper_source =
      "function helper(n) if n == 0 then error('original-provider-marker') end "
      "local result = helper(n - 1); return result end";
    return environment.stack().load(environment, std::span(helper_source), helperPath);
  })));
  ASSERT_TRUE((context.load(
    "require '/helper.lua'; function entry() local result = helper(2); return result end; "
    "function normal() return 42 end", "/entry.lua")));
  EXPECT_EQ(context.invoke_path<int>("normal"), 42);
  EXPECT_EQ(entryLookups, 0);
  EXPECT_EQ(helperLookups, 0);
  auto failure = context.invoke_path<int>("entry");
  ASSERT_FALSE(failure);
  EXPECT_TRUE(failure.error().is<LuaException>());
  std::string message = failure.error().what();
  EXPECT_NE(message.find("original-provider-marker"), std::string::npos);
  EXPECT_NE(message.find("friendly-entry"), std::string::npos);
  EXPECT_NE(message.find("internal-entry"), std::string::npos);
  EXPECT_NE(message.find("/mods/entry"), std::string::npos);
  EXPECT_NE(message.find("friendly-helper"), std::string::npos);
  EXPECT_NE(message.find("internal-helper"), std::string::npos);
  EXPECT_NE(message.find("/mods/override"), std::string::npos);
  EXPECT_NE(message.find(helperPath.utf8()), std::string::npos);
  EXPECT_EQ(entryLookups, 1);
  EXPECT_EQ(helperLookups, 1);
  auto const& summary = failure.error().diagnostic_summary();
  ASSERT_TRUE(summary);
  EXPECT_EQ(summary->kind, ExceptionSummaryKind::ScriptRuntime);
  EXPECT_NE(summary->reason.find("original-provider-marker"), std::string::npos);
  EXPECT_EQ(summary->reason.find('\n'), std::string::npos);
  ASSERT_EQ(summary->locations.size(), 2u);
  EXPECT_EQ(summary->locations[0].source, helperPath.utf8());
  EXPECT_EQ(summary->locations[0].line, 1);
  EXPECT_EQ(summary->locations[0].provider, "friendly-helper / internal-helper / /mods/override");
  EXPECT_EQ(summary->locations[1].source, "/entry.lua");
  EXPECT_EQ(summary->locations[1].line, 1);
  EXPECT_EQ(summary->locations[1].provider, "friendly-entry / internal-entry / /mods/entry");
}

TEST(LuaTest, SourceProviderFailuresPreserveOriginalError) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto engine = *created;
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  ASSERT_TRUE((context.load("function broken() error('resolver-original-marker') end", "/broken.lua")));
  ASSERT_TRUE((context.load("function entry() local result = broken(); return result end", "/entry.lua")));
  engine->set_source_resolver([](String const& source) -> std::expected<String, StarException> {
    if (source == "/entry.lua")
      return String("surviving-provider");
    return std::unexpected(LuaException("resolver-failure-marker"));
  });
  auto failure = context.invoke_path<int>("entry");
  ASSERT_FALSE(failure);
  EXPECT_TRUE(failure.error().is<LuaException>());
  std::string message = failure.error().what();
  EXPECT_NE(message.find("resolver-original-marker"), std::string::npos);
  EXPECT_NE(message.find("surviving-provider"), std::string::npos);
  EXPECT_EQ(message.find("resolver-failure-marker"), std::string::npos);
}

TEST(LuaTest, SourceProvidersPreserveCompilationAndLimitCategories) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto engine = *created;
  engine->set_source_resolver([](String const& source) -> std::expected<String, StarException> {
    if (source == "/syntax.lua")
      return String("syntax-provider /mods/syntax");
    return String();
  });
  auto compiled = engine->compile("function unfinished()", "/syntax.lua");
  ASSERT_FALSE(compiled);
  EXPECT_TRUE(compiled.error().is<LuaIncompleteStatementException>());
  EXPECT_NE(std::string(compiled.error().what()).find("syntax-provider"), std::string::npos);
  ASSERT_TRUE(compiled.error().diagnostic_summary());
  EXPECT_EQ(compiled.error().diagnostic_summary()->kind, ExceptionSummaryKind::ScriptSyntax);
  EXPECT_NE(compiled.error().diagnostic_summary()->reason.find("<eof>"), std::string::npos);
  ASSERT_FALSE(compiled.error().diagnostic_summary()->locations.empty());
  EXPECT_EQ(compiled.error().diagnostic_summary()->locations.front().source, "/syntax.lua");
  EXPECT_EQ(compiled.error().diagnostic_summary()->locations.front().line, -1);
  EXPECT_EQ(compiled.error().diagnostic_summary()->locations.front().provider, "syntax-provider /mods/syntax");
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  auto loaded = context.load("local = 'syntax-original-marker'", "/syntax.lua");
  ASSERT_FALSE(loaded);
  EXPECT_TRUE(loaded.error().is<LuaException>());
  EXPECT_NE(std::string(loaded.error().what()).find("syntax-provider"), std::string::npos);
  ASSERT_TRUE(loaded.error().diagnostic_summary());
  EXPECT_EQ(loaded.error().diagnostic_summary()->kind, ExceptionSummaryKind::ScriptSyntax);
  engine->set_instruction_measure_interval(1);
  engine->set_instruction_limit(20);
  auto limited = context.load("while true do end", "/syntax.lua");
  ASSERT_FALSE(limited);
  EXPECT_TRUE(limited.error().is<LuaInstructionLimitReached>());
  ASSERT_TRUE(limited.error().diagnostic_summary());
  EXPECT_EQ(limited.error().diagnostic_summary()->kind, ExceptionSummaryKind::ScriptRuntime);
  EXPECT_EQ(limited.error().diagnostic_summary()->category, "LuaInstructionLimitReached");
  EXPECT_EQ(limited.error().diagnostic_summary()->reason, "Lua instruction limit reached");
}

TEST(LuaTest, SourceProviderFailureDoesNotReplaceSyntaxError) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto engine = *created;
  engine->set_source_resolver([](String const&) -> std::expected<String, StarException> {
    return std::unexpected(LuaException("metadata-failure-marker"));
  });
  auto compiled = engine->compile("function unfinished()", "/syntax.lua");
  ASSERT_FALSE(compiled);
  EXPECT_TRUE(compiled.error().is<LuaIncompleteStatementException>());
  std::string message = compiled.error().what();
  EXPECT_EQ(message.find("metadata-failure-marker"), std::string::npos);
}

TEST(LuaTest, CoroutineSourceProvidersKeepFailedFrames) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto engine = *created;
  engine->set_source_resolver([](String const& source) -> std::expected<String, StarException> {
    if (source == "/helper.lua")
      return String("coroutine-helper-provider");
    if (source == "/entry.lua")
      return String("coroutine-entry-provider");
    return String();
  });
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  ASSERT_TRUE((context.load("function helper() error('coroutine-original-marker') end", "/helper.lua")));
  ASSERT_TRUE((context.load(
    "function entry() local result = helper(); return result end; "
    "function run() local ok, message = coroutine.resume(coroutine.create(entry)); "
    "if not ok then error(message) end end; co = coroutine.create(entry)", "/entry.lua")));
  auto invoked = context.invoke_path<int>("run");
  ASSERT_FALSE(invoked);
  std::string message = invoked.error().what();
  EXPECT_NE(message.find("coroutine-original-marker"), std::string::npos);
  EXPECT_NE(message.find("coroutine-helper-provider"), std::string::npos);
  EXPECT_NE(message.find("coroutine-entry-provider"), std::string::npos);
  ASSERT_TRUE(invoked.error().diagnostic_summary());
  EXPECT_EQ(invoked.error().diagnostic_summary()->kind, ExceptionSummaryKind::ScriptRuntime);
  EXPECT_NE(invoked.error().diagnostic_summary()->reason.find("coroutine-original-marker"), std::string::npos);
  EXPECT_EQ(invoked.error().diagnostic_summary()->reason.find('\n'), std::string::npos);
  ASSERT_FALSE(invoked.error().diagnostic_summary()->locations.empty());
  EXPECT_EQ(invoked.error().diagnostic_summary()->locations.front().source, "/helper.lua");
  EXPECT_EQ(invoked.error().diagnostic_summary()->locations.front().line, 1);
  auto thread = context.get<LuaThread>("co");
  ASSERT_TRUE(thread);
  auto resumed = thread->resume<int>();
  ASSERT_FALSE(resumed);
  message = resumed.error().what();
  EXPECT_NE(message.find("coroutine-original-marker"), std::string::npos);
  EXPECT_NE(message.find("coroutine-helper-provider"), std::string::npos);
  EXPECT_NE(message.find("coroutine-entry-provider"), std::string::npos);
  ASSERT_TRUE(resumed.error().diagnostic_summary());
  EXPECT_EQ(resumed.error().diagnostic_summary()->kind, ExceptionSummaryKind::ScriptRuntime);
  EXPECT_NE(resumed.error().diagnostic_summary()->reason.find("coroutine-original-marker"), std::string::npos);
  EXPECT_EQ(resumed.error().diagnostic_summary()->reason.find('\n'), std::string::npos);
  ASSERT_FALSE(resumed.error().diagnostic_summary()->locations.empty());
  EXPECT_EQ(resumed.error().diagnostic_summary()->locations.front().source, "/helper.lua");
  EXPECT_EQ(resumed.error().diagnostic_summary()->locations.front().line, 1);
}

TEST(LuaTest, RuntimeSummaryUsesLiveFailureAndCallerLinesBeforeTraceback) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto engine = *created;
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  ASSERT_TRUE(context.load(
      "function fail()\n"
      "  local unavailable\n"
      "  return unavailable.field\n"
      "end\n"
      "function entry()\n"
      "  local result = fail()\n"
      "  return result\n"
      "end", "/live-lines.lua"));
  auto failure = context.invoke_path("entry");
  ASSERT_FALSE(failure);
  auto const& summary = failure.error().diagnostic_summary();
  ASSERT_TRUE(summary);
  EXPECT_EQ(summary->kind, ExceptionSummaryKind::ScriptRuntime);
  EXPECT_NE(summary->reason.find("unavailable"), std::string::npos);
  EXPECT_EQ(summary->reason.find('\n'), std::string::npos);
  ASSERT_EQ(summary->locations.size(), 2u);
  EXPECT_EQ(summary->locations[0].source, "/live-lines.lua");
  EXPECT_EQ(summary->locations[0].line, 3);
  EXPECT_EQ(summary->locations[1].source, "/live-lines.lua");
  EXPECT_EQ(summary->locations[1].line, 6);
}

TEST(LuaTest, SummaryPreservesTypedNativeCauseThroughNestedCaughtStringErrors) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto engine = *created;
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  auto native = engine->create_function([]() -> std::expected<void, StarException> {
    return std::unexpected(StarException("outer-native-context",
        ExceptionDiagnostic(LuaConversionException("deepest-native-marker", false))));
  });
  ASSERT_TRUE(native);
  ASSERT_TRUE(context.set("nativeFailure", *native));
  auto nested = engine->create_function([&]() -> std::expected<void, StarException> {
    auto result = context.invoke_path("inner");
    if (!result)
      return std::unexpected(std::move(result.error()));
    return {};
  });
  ASSERT_TRUE(nested);
  ASSERT_TRUE(context.set("nativeNested", *nested));
  ASSERT_TRUE(context.load(
      "function inner() local result = nativeFailure(); return result end\n"
      "function entry() local ok, message = pcall(nativeNested); "
      "assert(not ok and type(message) == 'string'); error(message) end\n"
      "function identityRethrow() local ok, message = pcall(nativeFailure); "
      "assert(not ok and type(message) == 'string'); error(message, 0) end",
      "/native-summary.lua"));
  for (String const& entry : {String("entry"), String("identityRethrow")}) {
    auto failure = context.invoke_path(entry.utf8());
    ASSERT_FALSE(failure);
    EXPECT_TRUE(failure.error().is<LuaException>());
    auto const& summary = failure.error().diagnostic_summary();
    ASSERT_TRUE(summary);
    EXPECT_EQ(summary->kind, ExceptionSummaryKind::Native);
    EXPECT_EQ(summary->reason, "deepest-native-marker");
    EXPECT_NE(std::string(failure.error().what()).find("outer-native-context"), std::string::npos);
    EXPECT_NE(std::string(failure.error().what()).find("deepest-native-marker"), std::string::npos);
    EXPECT_TRUE(std::ranges::any_of(summary->locations, [](auto const& location) {
      return location.source == "/native-summary.lua" && location.line > 0;
    }));
  }
}

TEST(LuaTest, HostErrorsPreserveTypeThroughNestedCallbacksAndIdentityRethrows) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto engine = *created;
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  auto native = engine->create_function([](bool host) -> std::expected<void, StarException> {
    if (host)
      return std::unexpected(LuaHostError("explicit-host-marker", false));
    return std::unexpected(LuaConversionException("ordinary-native-marker", false));
  });
  ASSERT_TRUE(native);
  ASSERT_TRUE(context.set("nativeFailure", *native));
  auto nested = engine->create_function([&](bool host, bool identity) -> std::expected<void, StarException> {
    auto result = context.invoke_path("inner", host, identity);
    if (!result)
      return std::unexpected(std::move(result.error()));
    return {};
  });
  ASSERT_TRUE(nested);
  ASSERT_TRUE(context.set("nativeNested", *nested));
  ASSERT_TRUE(context.load(
      "function inner(host, identity)\n"
      "  local ok, message = pcall(nativeFailure, host)\n"
      "  assert(not ok and type(message) == 'string')\n"
      "  if identity then error(message, 0) else error(message) end\n"
      "end\n"
      "function entry(host, identity)\n"
      "  local ok, message = pcall(nativeNested, host, identity)\n"
      "  assert(not ok and type(message) == 'string')\n"
      "  if identity then error(message, 0) else error(message) end\n"
      "end\n"
      "function caughtSuccess() local ok, message = pcall(nativeFailure, true); "
      "return not ok and type(message) == 'string' end\n"
      "function unrelated() error('unrelated-host-marker') end\n"
      "function caughtThenUnrelated() pcall(nativeFailure, true); unrelated() end\n"
      "function transformed() local ok, message = pcall(nativeFailure, true); "
      "assert(not ok); error('rewritten:' .. message, 0) end",
      "/host-identity.lua"));
  EXPECT_EQ(context.invoke_path<bool>("caughtSuccess"), true);
  for (bool identity : {false, true}) {
    auto failure = context.invoke_path("entry", true, identity);
    ASSERT_FALSE(failure);
    EXPECT_TRUE(failure.error().is<LuaHostError>());
    EXPECT_FALSE(failure.error().is<LuaException>());
    auto const& summary = failure.error().diagnostic_summary();
    ASSERT_TRUE(summary);
    EXPECT_EQ(summary->kind, ExceptionSummaryKind::Native);
    EXPECT_EQ(summary->reason, "explicit-host-marker");
    EXPECT_TRUE(std::ranges::any_of(summary->locations, [](auto const& location) {
      return location.source == "/host-identity.lua" && location.line > 0;
    }));
    EXPECT_NE(std::string(failure.error().what()).find("explicit-host-marker"), std::string::npos);
  }
  auto ordinary = context.invoke_path("entry", false, false);
  ASSERT_FALSE(ordinary);
  EXPECT_TRUE(ordinary.error().is<LuaException>());
  EXPECT_FALSE(ordinary.error().is<LuaHostError>());
  ASSERT_TRUE(ordinary.error().diagnostic_summary());
  EXPECT_EQ(ordinary.error().diagnostic_summary()->kind, ExceptionSummaryKind::Native);
  EXPECT_EQ(ordinary.error().diagnostic_summary()->reason, "ordinary-native-marker");
  for (String const& entry : {String("caughtThenUnrelated"), String("transformed"), String("unrelated")}) {
    auto failure = context.invoke_path(entry.utf8());
    ASSERT_FALSE(failure);
    EXPECT_TRUE(failure.error().is<LuaException>());
    EXPECT_FALSE(failure.error().is<LuaHostError>());
    ASSERT_TRUE(failure.error().diagnostic_summary());
    EXPECT_EQ(failure.error().diagnostic_summary()->kind, ExceptionSummaryKind::ScriptRuntime);
    if (entry == "transformed")
      EXPECT_TRUE(failure.error().diagnostic_summary()->reason.starts_with("rewritten:"));
    else
      EXPECT_EQ(std::string(failure.error().what()).find("explicit-host-marker"), std::string::npos);
  }
  EXPECT_EQ(context.invoke_path<bool>("caughtSuccess"), true);
}

TEST(LuaTest, CaughtNativeSummariesDoNotLeakToOtherErrorsOrLaterCalls) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto engine = *created;
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  auto native = engine->create_function([]() -> std::expected<void, StarException> {
    return std::unexpected(LuaConversionException("caught-native-marker", false));
  });
  ASSERT_TRUE(native);
  ASSERT_TRUE(context.set("nativeFailure", *native));
  ASSERT_TRUE(context.load(
      "function caughtSuccess() local ok, message = pcall(nativeFailure); "
      "return not ok and type(message) == 'string' end\n"
      "function unrelated() error('unrelated-lua-marker') end\n"
      "function caughtThenUnrelated() for i = 1, 100 do pcall(nativeFailure) end; unrelated() end\n"
      "function transformed() local ok, message = pcall(nativeFailure); "
      "error('not-a-location-prefix:' .. message, 0) end",
      "/caught-summary.lua"));
  EXPECT_EQ(context.invoke_path<bool>("caughtSuccess"), true);
  for (String const& entry : {String("unrelated"), String("caughtThenUnrelated"), String("unrelated")}) {
    auto failure = context.invoke_path(entry.utf8());
    ASSERT_FALSE(failure);
    auto const& summary = failure.error().diagnostic_summary();
    ASSERT_TRUE(summary);
    EXPECT_EQ(summary->kind, ExceptionSummaryKind::ScriptRuntime);
    EXPECT_NE(summary->reason.find("unrelated-lua-marker"), std::string::npos);
    EXPECT_EQ(summary->reason.find("caught-native-marker"), std::string::npos);
    EXPECT_EQ(summary->reason.find('\n'), std::string::npos);
    EXPECT_EQ(std::string(failure.error().what()).find("caught-native-marker"), std::string::npos);
  }
  auto transformed = context.invoke_path("transformed");
  ASSERT_FALSE(transformed);
  ASSERT_TRUE(transformed.error().diagnostic_summary());
  EXPECT_EQ(transformed.error().diagnostic_summary()->kind, ExceptionSummaryKind::ScriptRuntime);
  EXPECT_TRUE(transformed.error().diagnostic_summary()->reason.starts_with("not-a-location-prefix:"));
  EXPECT_EQ(context.invoke_path<bool>("caughtSuccess"), true);
}

TEST(LuaTest, CoroutineNativeSummariesRemainIdentityBoundAcrossResumeAndYield) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto engine = *created;
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  auto native = engine->create_function([]() -> std::expected<void, StarException> {
    return std::unexpected(LuaConversionException("coroutine-native-marker", false));
  });
  ASSERT_TRUE(native);
  ASSERT_TRUE(context.set("nativeFailure", *native));
  ASSERT_TRUE(context.load(
      "function coEntry() nativeFailure() end\n"
      "function rethrow() local ok, message = coroutine.resume(coroutine.create(coEntry)); "
      "assert(not ok and type(message) == 'string'); error(message) end\n"
      "function unrelated() local ok, message = coroutine.resume(coroutine.create(coEntry)); "
      "assert(not ok and type(message) == 'string'); error('coroutine-unrelated-marker') end\n"
      "co = coroutine.create(coEntry)\n"
      "yielding = coroutine.create(function() pcall(nativeFailure); coroutine.yield('caught'); "
      "error('after-yield-unrelated-marker') end)",
      "/coroutine-native.lua"));
  auto luaFailure = context.invoke_path("rethrow");
  ASSERT_FALSE(luaFailure);
  ASSERT_TRUE(luaFailure.error().diagnostic_summary());
  EXPECT_EQ(luaFailure.error().diagnostic_summary()->kind, ExceptionSummaryKind::Native);
  EXPECT_EQ(luaFailure.error().diagnostic_summary()->reason, "coroutine-native-marker");
  auto thread = context.get<LuaThread>("co");
  ASSERT_TRUE(thread);
  auto hostFailure = thread->resume<LuaValue>();
  ASSERT_FALSE(hostFailure);
  ASSERT_TRUE(hostFailure.error().diagnostic_summary());
  EXPECT_EQ(hostFailure.error().diagnostic_summary()->kind, ExceptionSummaryKind::Native);
  EXPECT_EQ(hostFailure.error().diagnostic_summary()->reason, "coroutine-native-marker");
  ASSERT_FALSE(hostFailure.error().diagnostic_summary()->locations.empty());
  EXPECT_EQ(hostFailure.error().diagnostic_summary()->locations.front().source, "/coroutine-native.lua");
  EXPECT_EQ(hostFailure.error().diagnostic_summary()->locations.front().line, 1);
  auto unrelated = context.invoke_path("unrelated");
  ASSERT_FALSE(unrelated);
  ASSERT_TRUE(unrelated.error().diagnostic_summary());
  EXPECT_EQ(unrelated.error().diagnostic_summary()->kind, ExceptionSummaryKind::ScriptRuntime);
  EXPECT_NE(unrelated.error().diagnostic_summary()->reason.find("coroutine-unrelated-marker"), std::string::npos);
  auto yielding = context.get<LuaThread>("yielding");
  ASSERT_TRUE(yielding);
  auto yielded = yielding->resume<String>();
  ASSERT_TRUE(yielded);
  ASSERT_TRUE(*yielded);
  EXPECT_EQ(**yielded, "caught");
  auto afterYield = yielding->resume<LuaValue>();
  ASSERT_FALSE(afterYield);
  ASSERT_TRUE(afterYield.error().diagnostic_summary());
  EXPECT_EQ(afterYield.error().diagnostic_summary()->kind, ExceptionSummaryKind::ScriptRuntime);
  EXPECT_NE(afterYield.error().diagnostic_summary()->reason.find("after-yield-unrelated-marker"), std::string::npos);
  EXPECT_EQ(afterYield.error().diagnostic_summary()->reason.find("coroutine-native-marker"), std::string::npos);
}

TEST(LuaTest, CoroutineHostErrorsPreserveTypeWithoutLeakingAcrossYield) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto engine = *created;
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  auto native = engine->create_function([]() -> std::expected<void, StarException> {
    return std::unexpected(LuaHostError("coroutine-host-marker", false));
  });
  ASSERT_TRUE(native);
  ASSERT_TRUE(context.set("nativeFailure", *native));
  ASSERT_TRUE(context.load(
      "function coEntry() nativeFailure() end\n"
      "function rethrow() local ok, message = coroutine.resume(coroutine.create(coEntry)); "
      "assert(not ok and type(message) == 'string'); error(message, 0) end\n"
      "co = coroutine.create(coEntry)\n"
      "yielding = coroutine.create(function() local ok, message = pcall(nativeFailure); "
      "assert(not ok and type(message) == 'string'); coroutine.yield('caught'); "
      "error('after-host-yield-marker') end)",
      "/coroutine-host.lua"));
  auto luaFailure = context.invoke_path("rethrow");
  ASSERT_FALSE(luaFailure);
  EXPECT_TRUE(luaFailure.error().is<LuaHostError>());
  EXPECT_FALSE(luaFailure.error().is<LuaException>());
  ASSERT_TRUE(luaFailure.error().diagnostic_summary());
  EXPECT_EQ(luaFailure.error().diagnostic_summary()->kind, ExceptionSummaryKind::Native);
  EXPECT_EQ(luaFailure.error().diagnostic_summary()->reason, "coroutine-host-marker");
  auto thread = context.get<LuaThread>("co");
  ASSERT_TRUE(thread);
  auto hostFailure = thread->resume<LuaValue>();
  ASSERT_FALSE(hostFailure);
  EXPECT_TRUE(hostFailure.error().is<LuaHostError>());
  EXPECT_FALSE(hostFailure.error().is<LuaException>());
  ASSERT_TRUE(hostFailure.error().diagnostic_summary());
  EXPECT_EQ(hostFailure.error().diagnostic_summary()->kind, ExceptionSummaryKind::Native);
  EXPECT_EQ(hostFailure.error().diagnostic_summary()->reason, "coroutine-host-marker");
  ASSERT_FALSE(hostFailure.error().diagnostic_summary()->locations.empty());
  EXPECT_EQ(hostFailure.error().diagnostic_summary()->locations.front().source, "/coroutine-host.lua");
  EXPECT_EQ(hostFailure.error().diagnostic_summary()->locations.front().line, 1);
  auto yielding = context.get<LuaThread>("yielding");
  ASSERT_TRUE(yielding);
  auto yielded = yielding->resume<String>();
  ASSERT_TRUE(yielded);
  ASSERT_TRUE(*yielded);
  EXPECT_EQ(**yielded, "caught");
  auto afterYield = yielding->resume<LuaValue>();
  ASSERT_FALSE(afterYield);
  EXPECT_TRUE(afterYield.error().is<LuaException>());
  EXPECT_FALSE(afterYield.error().is<LuaHostError>());
  ASSERT_TRUE(afterYield.error().diagnostic_summary());
  EXPECT_EQ(afterYield.error().diagnostic_summary()->kind, ExceptionSummaryKind::ScriptRuntime);
  EXPECT_NE(afterYield.error().diagnostic_summary()->reason.find("after-host-yield-marker"), std::string::npos);
  EXPECT_EQ(std::string(afterYield.error().what()).find("coroutine-host-marker"), std::string::npos);
}

TEST(LuaTest, CoroutineNumericErrorsKeepTheirOriginalReason) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto context = (*created)->create_context();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->load("numericError = coroutine.create(function() error(42, 0) end)"));
  auto coroutine = context->get<LuaThread>("numericError");
  ASSERT_TRUE(coroutine);
  auto result = coroutine->resume<LuaValue>();
  ASSERT_FALSE(result);
  EXPECT_TRUE(result.error().is<LuaException>());
  ASSERT_TRUE(result.error().diagnostic_summary());
  EXPECT_EQ(result.error().diagnostic_summary()->kind, ExceptionSummaryKind::ScriptRuntime);
  EXPECT_EQ(result.error().diagnostic_summary()->reason, "42");
}

TEST(LuaTest, Eval) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;
  auto contextResult = luaEngine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);

  ASSERT_TRUE((context.eval("i = 3")));
  // Make sure statements and expressions both work in eval.
  EXPECT_EQ(context.eval<int>("i + 1"), 4);
  EXPECT_EQ(context.eval<int>("return i + 1"), 4);
}

TEST(LuaTest, Multi) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;
  auto scriptResult = luaEngine->compile(R"SCRIPT(
      function entry()
        return callbacks.func(2, 4)
      end

      function sum(...)
        local sum = 0
        for i,v in ipairs(arg) do
          sum = sum + v
        end
        return sum
      end
    )SCRIPT");
  ASSERT_TRUE(scriptResult);
  auto script = std::move(*scriptResult);

  auto context1Result = luaEngine->create_context();
  ASSERT_TRUE(context1Result);
  auto context1 = std::move(*context1Result);
  auto context2Result = luaEngine->create_context();
  ASSERT_TRUE(context2Result);
  auto context2 = std::move(*context2Result);
  auto context3Result = luaEngine->create_context();
  ASSERT_TRUE(context3Result);
  auto context3 = std::move(*context3Result);

  ASSERT_TRUE((context1.load(script)));
  ASSERT_TRUE((context2.load(script)));
  ASSERT_TRUE((context3.load(script)));

  LuaCallbacks addCallbacks;
  ASSERT_TRUE((addCallbacks.register_callback("func",
      [](LuaVariadic<int> const& args) -> int {
        int sum = 0.0;
        for (auto arg : args)
          sum += arg;
        return sum;
      })));

  LuaCallbacks multCallbacks;
  ASSERT_TRUE((multCallbacks.register_callback("func",
      [](LuaVariadic<int> const& args) -> int {
        int mult = 1.0;
        for (auto arg : args)
          mult *= arg;
        return mult;
      })));

  ASSERT_TRUE((context1.set_callbacks("callbacks", addCallbacks)));
  ASSERT_TRUE((context2.set_callbacks("callbacks", multCallbacks)));
  ASSERT_TRUE((context3.set_callbacks("callbacks", addCallbacks)));

  EXPECT_EQ(context1.invoke_path<int>("entry"), 6);
  EXPECT_EQ(context2.invoke_path<int>("entry"), 8);
  EXPECT_EQ(context3.invoke_path<int>("entry"), 6);
  EXPECT_EQ(context1.invoke_path<int>("entry"), 6);
  EXPECT_EQ(context2.invoke_path<int>("entry"), 8);
  EXPECT_EQ(context3.invoke_path<int>("entry"), 6);
  EXPECT_EQ(context1.invoke_path<int>("entry"), 6);

  auto context4Result = luaEngine->create_context();
  ASSERT_TRUE(context4Result);
  auto context4 = std::move(*context4Result);
  ASSERT_TRUE((context4.load(R"SCRIPT(
      function sum(...)
        local args = {...}
        local sum = 0
        for i = 1, #args do
          sum = sum + args[i]
        end
        return sum
      end

      function mreturn(...)
        return ...
      end

      function callbacktest(...)
        local x, y = callback()
        return x, y
      end

      function emptycallbacktest(...)
        return emptycallback()
      end
    )SCRIPT")));
  EXPECT_EQ(context4.invoke_path<int>("sum", LuaVariadic<int>{1, 2, 3}), 6);
  EXPECT_EQ(context4.invoke_path<int>("sum", 5, LuaVariadic<int>{1, 2, 3}, 10), 21);
  EXPECT_EQ(context4.invoke_path<LuaVariadic<int>>("mreturn", 1, 2, 3), LuaVariadic<int>({1, 2, 3}));

  int a;
  float b;
  String c;
  auto tupleResult1 = context4.invoke_path<LuaTupleReturn<int, float, String>>("mreturn", 1, 2.0f, "foo");
  ASSERT_TRUE(tupleResult1);
  lua_tie(a, b, c) = std::move(*tupleResult1);
  EXPECT_EQ(a, 1);
  EXPECT_EQ(b, 2.0f);
  EXPECT_EQ(c, "foo");

  auto callbackResult7 = context4.create_function([]() { return lua_tuple_return(5, 10); });
  ASSERT_TRUE(callbackResult7);
  ASSERT_TRUE((context4.set("callback", *callbackResult7)));

  auto callbackResult8 = context4.create_function([]() { return lua_tuple_return(); });
  ASSERT_TRUE(callbackResult8);
  ASSERT_TRUE((context4.set("emptycallback", *callbackResult8)));

  int d;
  int e;
  auto tupleResult2 = context4.invoke_path<LuaTupleReturn<int, int>>("callbacktest");
  ASSERT_TRUE(tupleResult2);
  lua_tie(d, e) = std::move(*tupleResult2);
  EXPECT_EQ(d, 5);
  EXPECT_EQ(e, 10);

  expectLuaNil(context4.invoke_path("emptycallbacktest"));
}

TEST(LuaTest, Limits) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;
  luaEngine->set_instruction_limit(50000);
  luaEngine->set_recursion_limit(8);
  auto contextResult = luaEngine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  ASSERT_TRUE((context.load(R"SCRIPT(
      function toinfinityandbeyond()
        while true do
        end
      end

      function toabignumberandthenstop()
        for i = 0, 5000 do
        end
      end
    )SCRIPT")));

  // Make sure infinite loops trigger the instruction limit.
  {
    auto failure = context.invoke_path("toinfinityandbeyond");
    ASSERT_FALSE(failure);
    EXPECT_TRUE(failure.error().is<LuaInstructionLimitReached>());
  };

  // Make sure the instruction count is reset after each call.
  ASSERT_TRUE((context.invoke_path("toabignumberandthenstop")));

  std::string_view infLoop = R"SCRIPT(
        while true do
        end
      )SCRIPT";

  // Make sure loading code into context with infinite loops in their
  // evaluation triggers instruction limit.
  {
    auto failure = context.load(std::span<char const>(infLoop));
    ASSERT_FALSE(failure);
    EXPECT_TRUE(failure.error().is<LuaInstructionLimitReached>());
  };

  // And the same for eval
  {
    auto failure = context.eval(String(infLoop));
    ASSERT_FALSE(failure);
    EXPECT_TRUE(failure.error().is<LuaInstructionLimitReached>());
  };

  auto call1 = [&context]() { return context.invoke_path("call2"); };

  auto call2 = [&context]() { return context.invoke_path("call1"); };

  auto callbackResult9 = context.create_function(call1);
  ASSERT_TRUE(callbackResult9);
  ASSERT_TRUE((context.set("call1", *callbackResult9)));
  auto callbackResult10 = context.create_function(call2);
  ASSERT_TRUE(callbackResult10);
  ASSERT_TRUE((context.set("call2", *callbackResult10)));

  {
    auto failure = context.invoke_path("call1");
    ASSERT_FALSE(failure);
    EXPECT_TRUE(failure.error().is<LuaRecursionLimitReached>());
    ASSERT_TRUE(failure.error().diagnostic_summary());
    EXPECT_EQ(failure.error().diagnostic_summary()->kind, ExceptionSummaryKind::ScriptRuntime);
    EXPECT_EQ(failure.error().diagnostic_summary()->category, "LuaRecursionLimitReached");
    EXPECT_EQ(failure.error().diagnostic_summary()->reason, "Lua recursion limit reached");
  };

  // Make sure the context still functions properly after these previous
  // errors.
  EXPECT_EQ(context.eval<int>("1 + 1"), 2);
}

TEST(LuaTest, Errors) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;
  auto contextResult = luaEngine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);

  {
    auto failure = context.eval("while true do");
    ASSERT_FALSE(failure);
    EXPECT_TRUE(failure.error().is<LuaIncompleteStatementException>());
  };
  ASSERT_TRUE((context.set_path("val", 1.0)));
  {
    auto failure = context.get_path<Vec2D>("val");
    ASSERT_FALSE(failure);
    EXPECT_TRUE(failure.error().is<LuaConversionException>());
  };
  auto val = context.get("val");
  ASSERT_TRUE(val);
  auto rect = luaEngine->lua_maybe_to<RectF>(*val);
  ASSERT_TRUE(rect);
  EXPECT_FALSE(*rect);

  auto callbackResult11 = luaEngine->create_function([]() -> std::expected<void, StarException> {
      return std::unexpected(StarException("lua caught the error!"));
    });
  ASSERT_TRUE(callbackResult11);
  ASSERT_TRUE((context.set("throwException", *callbackResult11)));

  ASSERT_TRUE((context.load(R"SCRIPT(
      function throwError()
        return throwException()
      end
      function catchError()
        return pcall(throwException)
      end
    )SCRIPT")));

  {
    auto failure = context.invoke_path("throwError");
    ASSERT_FALSE(failure);
    EXPECT_TRUE(failure.error().is<LuaException>());
  };

  bool status;
  LuaValue error;
  auto tupleResult3 = context.invoke_path<LuaTupleReturn<bool, LuaValue>>("catchError");
  ASSERT_TRUE(tupleResult3);
  lua_tie(status, error) = std::move(*tupleResult3);
  EXPECT_EQ(status, false);
  auto errorText = luaEngine->lua_to<String>(error);
  ASSERT_TRUE(errorText);
  EXPECT_EQ(errorText->contains("lua caught the error"), true);
}

TEST(LuaTest, QualifiedCallbacksPreserveStateAndCheckedVoidFailures) {
  struct DepositTotal {
    int total = 0;

    std::expected<int, StarException> operator()(int amount) & noexcept {
      if (amount < 0)
        return std::unexpected(StarException("negative-deposit"));
      total += amount;
      return total;
    }
  };
  struct Fee {
    int operator()(int cents) const noexcept { return cents / 10; }
    double operator()(double amount) const noexcept { return amount / 10.0; }
  };

  auto engineResult = LuaEngine::create();
  ASSERT_TRUE(engineResult);
  auto& engine = *engineResult;
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);

  auto twice = context.create_function([](int amount) noexcept { return amount * 2; });
  ASSERT_TRUE(twice);
  ASSERT_TRUE(context.set("twice", *twice));

  int charged = 0;
  LuaCallbacks callbacks;
  ASSERT_TRUE(callbacks.register_callback("deposit", DepositTotal{}));
  ASSERT_TRUE((callbacks.register_callback_with_signature<int, int>("fee", Fee{})));
  // The declared void return must not discard this generic callback's error.
  ASSERT_TRUE((callbacks.register_callback_with_signature<void, int>("charge",
      [&](auto amount) -> std::expected<void, StarException> {
        if (amount < 0)
          return std::unexpected(StarException("negative-charge"));
        charged += amount;
        return {};
      })));
  ASSERT_TRUE(context.set_callbacks("account", callbacks));
  ASSERT_TRUE(context.load(R"SCRIPT(
    function checkout(amount)
      account.charge(amount)
      return twice(account.deposit(amount - account.fee(amount)))
    end
    function rejectDeposit()
      return account.deposit(-5)
    end
  )SCRIPT"));

  EXPECT_EQ(context.invoke_path<int>("checkout", 20), 36);
  EXPECT_EQ(context.invoke_path<int>("checkout", 10), 54);
  EXPECT_EQ(charged, 30);

  auto chargeFailure = context.invoke_path<int>("checkout", -5);
  ASSERT_FALSE(chargeFailure);
  EXPECT_NE(std::string(chargeFailure.error().what()).find("negative-charge"), std::string::npos);
  EXPECT_EQ(charged, 30);

  auto depositFailure = context.invoke_path<int>("rejectDeposit");
  ASSERT_FALSE(depositFailure);
  EXPECT_NE(std::string(depositFailure.error().what()).find("negative-deposit"), std::string::npos);

  EXPECT_EQ(context.invoke_path<int>("checkout", 5), 64);
  EXPECT_EQ(charged, 35);
}

TEST(LuaTest, ErrorDiagnosticsLeaveBothCBoundariesClean) {
  auto engineResult = LuaEngine::create();
  ASSERT_TRUE(engineResult);
  auto& engine = *engineResult;
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  std::weak_ptr<int> wrappedLifetime;
  std::weak_ptr<int> requireLifetime;
  auto callbackResult12 = engine->create_function([&]() -> std::expected<void, StarException> {
    auto lifetime = std::make_shared<int>(1);
    wrappedLifetime = lifetime;
    return std::unexpected(IOException("wrapped callback failure",
        ExceptionDiagnostic(StarException("Lua callback cause"))));
  });
  ASSERT_TRUE(callbackResult12);
  ASSERT_TRUE((context.set("failWrapped", *callbackResult12)));
  ASSERT_TRUE((context.set_require_function([&](LuaStackValue, StringView) -> std::expected<void, StarException> {
    auto lifetime = std::make_shared<int>(2);
    requireLifetime = lifetime;
    return std::unexpected(IOException("require callback failure",
        ExceptionDiagnostic(StarException("Lua callback cause"))));
  })));
  ASSERT_TRUE((context.load(R"SCRIPT(
    function captureWrapped() return pcall(failWrapped) end
    function captureRequire() return pcall(require, "broken") end
    function healthy(value) return value * 3 end
  )SCRIPT")));

  bool status;
  LuaValue error;
  auto tupleResult4 = context.invoke_path<LuaTupleReturn<bool, LuaValue>>("captureWrapped");
  ASSERT_TRUE(tupleResult4);
  lua_tie(status, error) = std::move(*tupleResult4);
  EXPECT_FALSE(status);
  auto wrappedText = engine->lua_to<String>(error);
  ASSERT_TRUE(wrappedText);
  EXPECT_EQ(wrappedText->contains("(IOException) wrapped callback failure"), true);
  EXPECT_EQ(wrappedText->contains("Caused by:"), true);
  EXPECT_EQ(wrappedText->contains("Lua callback cause"), true);
  EXPECT_TRUE(wrappedLifetime.expired());
  EXPECT_EQ(context.invoke_path<int>("healthy", 7), 21);

  auto tupleResult5 = context.invoke_path<LuaTupleReturn<bool, LuaValue>>("captureRequire");
  ASSERT_TRUE(tupleResult5);
  lua_tie(status, error) = std::move(*tupleResult5);
  EXPECT_FALSE(status);
  auto requireText = engine->lua_to<String>(error);
  ASSERT_TRUE(requireText);
  EXPECT_EQ(requireText->contains("(IOException) require callback failure"), true);
  EXPECT_EQ(requireText->contains("Caused by:"), true);
  EXPECT_EQ(requireText->contains("Lua callback cause"), true);
  EXPECT_TRUE(requireLifetime.expired());
  EXPECT_EQ(context.invoke_path<int>("healthy", 9), 27);

  // Lua errors not caught by pcall still return the project's error value.
  {
    auto failure = context.load("failWrapped()");
    ASSERT_FALSE(failure);
    EXPECT_TRUE(failure.error().is<LuaException>());
  };
  EXPECT_TRUE(wrappedLifetime.expired());
  EXPECT_EQ(context.invoke_path<int>("healthy", 10), 30);
}

TEST(LuaTest, GarbageCollection) {
  auto engineResult = LuaEngine::create();
  ASSERT_TRUE(engineResult);
  auto& engine = *engineResult;
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);

  auto ptr = std::make_shared<int>(5);
  engine->set_auto_garbage_collection(false);
  {
    auto userdata = context.create_userdata(ptr);
    ASSERT_TRUE(userdata);
    ASSERT_TRUE(context.set_path("ref", *userdata));
  }
  EXPECT_EQ(ptr.use_count(), 2);
  ASSERT_TRUE((context.set_path("ref", LuaNil)));
  EXPECT_EQ(ptr.use_count(), 2);
  ASSERT_TRUE(engine->collect_garbage());
  EXPECT_EQ(ptr.use_count(), 1);
}

TEST(LuaTest, AutomaticCollectionCanBeStoppedAndRestarted) {
  auto engine = LuaEngine::create(false);
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  (*engine)->set_auto_garbage_collection(false);
  EXPECT_EQ(context->eval<bool>("collectgarbage('isrunning')"), false);
  (*engine)->set_auto_garbage_collection(true);
  EXPECT_EQ(context->eval<bool>("collectgarbage('isrunning')"), true);
  (*engine)->set_auto_garbage_collection(false);
  ASSERT_TRUE((*engine)->collect_garbage(1));
  EXPECT_EQ(context->eval<bool>("collectgarbage('isrunning')"), false);
}

TEST(LuaTest, OrdinaryCallsReleaseExpiredReferencesWithoutHostCollection) {
  auto engine = LuaEngine::create(false);
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->load(R"SCRIPT(
    observed = setmetatable({}, {__mode = "v"})
    function remember(index)
      local object = {index = index}
      observed[index] = object
      return object
    end
    function alive()
      collectgarbage("collect")
      local count = 0
      for _ in pairs(observed) do count = count + 1 end
      return count
    end
  )SCRIPT"));
  std::optional<LuaTable> retained;
  {
    auto first = context->invoke_path<LuaTable>("remember", 1);
    ASSERT_TRUE(first);
    retained = std::move(*first);
  }
  for (int i = 2; i <= 4096; ++i) {
    auto temporary = context->invoke_path<LuaTable>("remember", i);
    ASSERT_TRUE(temporary);
  }
  EXPECT_EQ(context->invoke_path<int>("alive"), 1);
  EXPECT_EQ(retained->get<int>("index"), 1);
  retained.reset();
  EXPECT_EQ(context->invoke_path<int>("alive"), 0);
}

TEST(LuaTest, FinalizerWarningPreservesContextAndReleasesUserData) {
  auto engineResult = LuaEngine::create();
  ASSERT_TRUE(engineResult);
  auto& engine = *engineResult;
  engine->set_auto_garbage_collection(false);
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);

  auto resource = std::make_shared<int>(7);
  std::weak_ptr<int> lifetime = resource;
  {
    auto userdata = context.create_userdata(resource);
    ASSERT_TRUE(userdata);
    ASSERT_TRUE(context.set_path("trackedResource", *userdata));
  }
  ASSERT_TRUE(context.load(R"SCRIPT(
    finalized = false
    do
      local doomed = {resource = trackedResource}
      setmetatable(doomed, {
        __gc = function(object)
          finalized = true
          object.resource = nil
          error("lua-gc-finalizer-recovery-marker")
        end
      })
    end
    trackedResource = nil
  )SCRIPT"));
  resource.reset();
  EXPECT_FALSE(lifetime.expired());

  ASSERT_TRUE(engine->collect_garbage());
  EXPECT_EQ(context.get<bool>("finalized"), true);
  EXPECT_EQ(context.eval<int>("6 * 7"), 42);

  // Finalized userdata can retain objects until a subsequent GC cycle.
  ASSERT_TRUE(engine->collect_garbage());
  ASSERT_TRUE(engine->collect_garbage());
  EXPECT_TRUE(lifetime.expired());
}

TEST(LuaTest, IntTest) {
  auto engineResult = LuaEngine::create();
  ASSERT_TRUE(engineResult);
  auto& engine = *engineResult;
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  ASSERT_TRUE((context.set_path("test", (std::uint64_t)-1)));
  EXPECT_EQ(context.get_path<std::uint64_t>("test"), (std::uint64_t)-1);
}

TEST(LuaTest, VariantTest) {
  auto engineResult = LuaEngine::create();
  ASSERT_TRUE(engineResult);
  auto& engine = *engineResult;
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);

  using IntOrString = std::variant<int, String>;

  EXPECT_EQ(context.eval<IntOrString>("'foo'"), IntOrString(String("foo")));
  EXPECT_EQ(context.eval<IntOrString>("'1'"), IntOrString(1));

  using StringOrInt = std::variant<String, int>;
  EXPECT_EQ(context.eval<StringOrInt>("'1'"), StringOrInt(String("1")));

  using MIntOrString = std::variant<std::monostate, std::optional<int>, String>;

  EXPECT_EQ(context.eval<MIntOrString>("'foo'"), MIntOrString(String("foo")));
  EXPECT_EQ(context.eval<MIntOrString>("'1'"), MIntOrString(std::optional<int>(1)));
  auto nilValue = context.eval<LuaValue>("nil");
  ASSERT_TRUE(nilValue);
  auto nil = engine->lua_maybe_to<MIntOrString>(*nilValue);
  ASSERT_TRUE(nil);
  ASSERT_TRUE(*nil);
  EXPECT_TRUE(std::holds_alternative<std::monostate>(**nil));
  ASSERT_TRUE((context.set("variantNil", MIntOrString{})));
  EXPECT_EQ(context.eval<bool>("variantNil == nil"), true);

  using OptionalOrString = std::variant<std::optional<int>, String>;
  auto optionalNilValue = context.eval<LuaValue>("nil");
  ASSERT_TRUE(optionalNilValue);
  auto optionalNil = engine->lua_maybe_to<OptionalOrString>(*optionalNilValue);
  ASSERT_TRUE(optionalNil);
  ASSERT_TRUE(*optionalNil);
  ASSERT_TRUE(std::holds_alternative<std::optional<int>>(**optionalNil));
  EXPECT_FALSE(std::get<std::optional<int>>(**optionalNil));
}

TEST(LuaTest, JsonContainerVariantMismatchReachesFunctionAlternative) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  auto value = context->eval<LuaValue>("function() return false end");
  ASSERT_TRUE(value);

  auto array = (*engine)->lua_maybe_to<std::variant<JsonArray, LuaFunction>>(*value);
  ASSERT_TRUE(array);
  ASSERT_TRUE(*array);
  ASSERT_TRUE(std::holds_alternative<LuaFunction>(**array));
  EXPECT_EQ(std::get<LuaFunction>(**array).invoke<bool>(), false);

  auto object = (*engine)->lua_maybe_to<std::variant<JsonObject, LuaFunction>>(*value);
  ASSERT_TRUE(object);
  ASSERT_TRUE(*object);
  ASSERT_TRUE(std::holds_alternative<LuaFunction>(**object));
  EXPECT_EQ(std::get<LuaFunction>(**object).invoke<bool>(), false);
}

TEST(LuaTest, ProfilingTest) {
  auto luaEngineResult = LuaEngine::create();
  ASSERT_TRUE(luaEngineResult);
  auto& luaEngine = *luaEngineResult;
  luaEngine->set_profiling_enabled(true);
  luaEngine->set_instruction_measure_interval(1000);

  auto contextResult = luaEngine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  ASSERT_TRUE((context.eval(R"SCRIPT(
      function function1()
        for i = 1, 1000 do
        end
      end

      function function2()
        for i = 1, 1000 do
        end
      end

      function function3()
        for i = 1, 1000 do
        end
      end

      for i = 1, 10 do
        function1()
        function2()
        function3()
      end
    )SCRIPT")));

  StringSet names;
  List<LuaProfileEntry> profile = luaEngine->get_profile();
  ASSERT_FALSE(profile.empty());
  for (auto const& p : profile[0].calls)
    names.add(p.second->name.value_or(String()));

  EXPECT_TRUE(names.contains("function1"));
  EXPECT_TRUE(names.contains("function2"));
  EXPECT_TRUE(names.contains("function3"));
}

TEST(LuaTest, PromiseUserdataObservesSharedOutcome) {
  auto engineResult = LuaEngine::create();
  ASSERT_TRUE(engineResult);
  auto& engine = *engineResult;
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  auto [promise, keeper] = RpcPromise<Json>::make_pair();
  ASSERT_TRUE((context.set_path("promise", promise)));
  ASSERT_TRUE((context.set_path("keeper", std::make_shared<AutoFailRpcPromiseKeeper<Json>>(keeper))));
  EXPECT_EQ(context.eval<bool>("promise:finished()"), false);
  EXPECT_EQ(context.eval<bool>("promise:succeeded()"), false);
  EXPECT_EQ(context.eval<bool>("promise:result() == nil and promise:error() == nil"), true);
  ASSERT_TRUE((context.load("keeper:fulfill({answer = 42})")));
  EXPECT_EQ(context.eval<bool>("promise:finished() and promise:succeeded()"), true);
  EXPECT_EQ(context.eval<Json>("promise:result()"), JsonObject({{"answer", 42}}));
  EXPECT_EQ(context.eval<bool>("promise:error() == nil"), true);
  auto promiseResult = promise.result();
  ASSERT_TRUE(promiseResult);
  ASSERT_TRUE(*promiseResult);
  EXPECT_EQ((**promiseResult).get_int("answer"), 42);
  {
    auto failure = context.load("keeper:fail('second resolution')");
    ASSERT_FALSE(failure);
    EXPECT_TRUE(failure.error().is<LuaException>());
  };
  ASSERT_TRUE((context.load("keeper = nil promise = nil")));
  ASSERT_TRUE(engine->collect_garbage());
  EXPECT_EQ(promise.succeeded(), true);
}

TEST(LuaTest, PromiseKeeperGarbageCollectionFailsOnlyLastOwner) {
  auto engineResult = LuaEngine::create();
  ASSERT_TRUE(engineResult);
  auto& engine = *engineResult;
  engine->set_auto_garbage_collection(false);
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  auto [promise, keeper] = RpcPromise<Json>::make_pair();
  auto automatic = std::make_shared<AutoFailRpcPromiseKeeper<Json>>(keeper);
  ASSERT_TRUE((context.set_path("keeper", automatic)));
  ASSERT_TRUE((context.set_path("promise", promise)));
  ASSERT_TRUE((context.load("alias = keeper keeper = nil")));
  automatic.reset();
  ASSERT_TRUE(engine->collect_garbage());
  EXPECT_EQ(promise.finished(), false);
  ASSERT_TRUE((context.load("alias = nil")));
  ASSERT_TRUE(engine->collect_garbage());
  EXPECT_EQ(promise.failed(), true);
  auto promiseError = promise.error();
  ASSERT_TRUE(promiseError);
  ASSERT_TRUE(*promiseError);
  EXPECT_EQ(**promiseError, "Keeper destroyed");
  EXPECT_EQ(context.eval<bool>("promise:finished() and not promise:succeeded()"), true);
  EXPECT_EQ(context.eval<String>("promise:error()"), "Keeper destroyed");
  ASSERT_TRUE((context.load("promise = nil")));
  ASSERT_TRUE(engine->collect_garbage());
}

TEST(LuaTest, PromiseKeeperCollectionPreservesChainAndFailure) {
  auto engineResult = LuaEngine::create();
  ASSERT_TRUE(engineResult);
  auto& engine = *engineResult;
  auto contextResult = engine->create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  auto [source, sourceKeeper] = RpcPromise<Json>::make_pair();
  auto [promise, keeper] = RpcPromise<Json>::make_pair();
  ASSERT_TRUE((context.set_path("source", source)));
  ASSERT_TRUE((context.set_path("promise", promise)));
  ASSERT_TRUE((context.set_path("keeper", std::make_shared<AutoFailRpcPromiseKeeper<Json>>(keeper))));
  ASSERT_TRUE((context.load("keeper:chain(source) keeper = nil")));
  ASSERT_TRUE(engine->collect_garbage());
  EXPECT_EQ(promise.finished(), false);
  ASSERT_TRUE((sourceKeeper.fulfill(JsonArray{1, 2, 3})));
  EXPECT_EQ(context.eval<Json>("promise:result()"), JsonArray({1, 2, 3}));

  auto [failed, failedKeeper] = RpcPromise<Json>::make_pair();
  ASSERT_TRUE((context.set_path("failedKeeper", std::make_shared<AutoFailRpcPromiseKeeper<Json>>(failedKeeper))));
  ASSERT_TRUE((context.load("failedKeeper:fail('from Lua') failedKeeper = nil")));
  ASSERT_TRUE(engine->collect_garbage());
  auto failedError = failed.error();
  ASSERT_TRUE(failedError);
  ASSERT_TRUE(*failedError);
  EXPECT_EQ(**failedError, "from Lua");
  ASSERT_TRUE((context.load("source = nil promise = nil")));
  ASSERT_TRUE(engine->collect_garbage());
}

TEST(LuaTest, TypedCallsResolveCurrentFunctionsAndOnlyDecodeRequestedResults) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto& engine = **created;
  auto contextResult = engine.create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  auto native = engine.create_raw_function(+[](lua_State* state) {
    lua_pushinteger(state, 43);
    lua_pushlightuserdata(state, state);
    return 2;
  });
  ASSERT_TRUE(native);
  ASSERT_TRUE(context.set("native", *native));
  ASSERT_TRUE(context.load(R"LUA(
    routes = {entry = function() return native() end}
    function nothing() return nil end
    function tuple() return 17, "payload" end
    function values() return 1, nil, 3 end
  )LUA"));
  EXPECT_EQ(context.invoke_path<int>("routes.entry"), 43);
  ASSERT_TRUE(context.invoke_path<void>("routes.entry"));
  auto unsupported = context.invoke_path<LuaVariadic<LuaValue>>("routes.entry");
  ASSERT_FALSE(unsupported);
  EXPECT_TRUE(unsupported.error().is<LuaException>());
  auto tuple = context.invoke_path<LuaTupleReturn<int, String, std::optional<int>>>("tuple");
  ASSERT_TRUE(tuple);
  EXPECT_EQ(std::get<0>(*tuple), 17);
  EXPECT_EQ(std::get<1>(*tuple), "payload");
  EXPECT_FALSE(std::get<2>(*tuple));
  auto values = context.invoke_path<LuaVariadic<std::optional<int>>>("values");
  ASSERT_TRUE(values);
  EXPECT_EQ(*values, (LuaVariadic<std::optional<int>>{1, std::nullopt, 3}));
  auto missing = context.invoke_path_if_present<LuaValue>("routes.missing");
  ASSERT_TRUE(missing);
  EXPECT_FALSE(*missing);
  auto nil = context.invoke_path_if_present<LuaValue>("nothing");
  ASSERT_TRUE(nil);
  ASSERT_TRUE(*nil);
  EXPECT_TRUE(std::holds_alternative<LuaNilType>(**nil));
  ASSERT_TRUE(context.load("routes.entry = function() return 59 end"));
  EXPECT_EQ(context.invoke_path<int>("routes.entry"), 59);
}

TEST(LuaTest, BorrowedCoroutineArgumentsSurviveReentryAndOnlyExplicitReferencesPersist) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto& engine = **created;
  auto contextResult = engine.create_context();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  std::optional<LuaFunction> retained;
  auto native = engine.create_function([&](LuaFunction function, Vec2F position) {
    retained = std::move(function);
    return position;
  });
  ASSERT_TRUE(native);
  ASSERT_TRUE(context.set("native", *native));
  auto nested = engine.create_function([&]() -> std::expected<int, StarException> {
    if (auto collected = engine.collect_garbage(); !collected)
      return std::unexpected(std::move(collected.error()));
    return context.eval<int>("6");
  });
  ASSERT_TRUE(nested);
  ASSERT_TRUE(context.set("nested", *nested));
  ASSERT_TRUE(context.load(R"LUA(
    function run()
      local input = setmetatable({}, {__index = function(_, key) return nested() + key end})
      return coroutine.yield(native(function() return 29 end, input))
    end
  )LUA"));
  {
    auto thread = engine.create_thread();
    ASSERT_TRUE(thread);
    auto function = context.get<LuaFunction>("run");
    ASSERT_TRUE(function);
    ASSERT_TRUE(thread->push_function(*function));
    auto yielded = thread->resume<Vec2F>();
    ASSERT_TRUE(yielded);
    ASSERT_TRUE(*yielded);
    EXPECT_EQ(**yielded, Vec2F(7, 8));
  }
  ASSERT_TRUE(engine.collect_garbage());
  ASSERT_TRUE(retained);
  EXPECT_EQ(retained->invoke<int>(), 29);
  ASSERT_TRUE(context.load(R"LUA(
    observed = setmetatable({}, {__mode = "v"})
    do
      local candidate = function() return 71 end
      observed[1] = candidate
      local ok = pcall(native, candidate, setmetatable({}, {
        __index = function() error("argument decode failed") end
      }))
      assert(not ok)
    end
  )LUA"));
  ASSERT_TRUE(engine.collect_garbage());
  EXPECT_EQ(context.eval<bool>("observed[1] == nil"), true);
  EXPECT_EQ(retained->invoke<int>(), 29);
}

TEST(LuaTest, MissingJsonArgumentsAndEmptyCoroutineResultsAreNil) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto& engine = **created;
  auto context = engine.create_context();
  ASSERT_TRUE(context);
  auto native = engine.create_function([](Json value) { return value; });
  ASSERT_TRUE(native);
  ASSERT_TRUE(context->set("native", *native));
  EXPECT_EQ(context->eval<Json>("native()"), Json());
  EXPECT_EQ(context->eval<Json>("native(nil)"), Json());
  ASSERT_TRUE(context->load("function empty() end"));
  auto function = context->get<LuaFunction>("empty");
  ASSERT_TRUE(function);
  auto thread = engine.create_thread();
  ASSERT_TRUE(thread);
  ASSERT_TRUE(thread->push_function(*function));
  auto result = thread->resume<Json>();
  ASSERT_TRUE(result);
  ASSERT_TRUE(*result);
  EXPECT_EQ(**result, Json());
}

TEST(LuaTest, NativeCoroutineResultsUseTopSliceAndPreserveContinuationArguments) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto& engine = **created;
  auto function = engine.create_raw_function(+[](lua_State* state) {
    lua_pushinteger(state, 42);
    return lua_yieldk(
      state, 1, 0, +[](lua_State* state, int, lua_KContext) {
        lua_pushinteger(state, lua_tointeger(state, 1) + lua_tointeger(state, 2));
        return 1;
      });
  });
  ASSERT_TRUE(function);
  auto thread = engine.create_thread();
  ASSERT_TRUE(thread);
  ASSERT_TRUE(thread->push_function(*function));
  auto yielded = thread->resume<int>(10);
  ASSERT_TRUE(yielded);
  EXPECT_EQ(*yielded, std::optional<int>(42));
  auto finished = thread->resume<int>(7);
  ASSERT_TRUE(finished);
  EXPECT_EQ(*finished, std::optional<int>(17));
  EXPECT_EQ(thread->status(), LuaThread::Status::Dead);
}

TEST(LuaTest, FixedReturnTuplesReserveCapacityAndPadMissingValues) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto context = (*created)->create_context();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->load("function one() return 7 end"));
  [&]<std::size_t... I>(std::index_sequence<I...>) {
    using Result = LuaTupleReturn<decltype((void)I, std::optional<int>{})...>;
    auto verify = [](Result const& result) {
      std::apply(
        [](auto const& first, auto const&... rest) {
          EXPECT_EQ(first, std::optional<int>(7));
          EXPECT_TRUE((!rest && ...));
        },
        static_cast<typename Result::Base const&>(result));
    };
    auto direct = context->invoke_path<Result>("one");
    ASSERT_TRUE(direct);
    verify(*direct);
    auto evaluated = context->eval<Result>("one()");
    ASSERT_TRUE(evaluated);
    verify(*evaluated);
  }(std::make_index_sequence<64>{});
}

TEST(LuaTest, OwningIdentityConversionsShareOriginalRegistryReferences) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto& engine = **created;
  auto table = engine.create_table();
  auto function = engine.create_function([] { return 1; });
  auto thread = engine.create_thread();
  auto userdata = engine.create_userdata(9);
  ASSERT_TRUE(table);
  ASSERT_TRUE(function);
  ASSERT_TRUE(thread);
  ASSERT_TRUE(userdata);
  auto verify = [&]<typename T>(T const& original) {
    auto encoded = engine.lua_from(original);
    ASSERT_TRUE(encoded);
    ASSERT_TRUE(std::holds_alternative<T>(*encoded));
    EXPECT_TRUE(std::get<T>(*encoded) == original);
    auto decoded = engine.lua_to<T>(*encoded);
    ASSERT_TRUE(decoded);
    EXPECT_TRUE(*decoded == original);
    auto moved = engine.lua_maybe_to<T>(std::move(*encoded));
    ASSERT_TRUE(moved);
    ASSERT_TRUE(*moved);
    EXPECT_TRUE(**moved == original);
  };
  verify(*table);
  verify(*function);
  verify(*thread);
  verify(*userdata);
}

TEST(LuaTest, BorrowedCrossStatePushReservesSourceCapacityAndKeepsValueRooted) {
  auto created = LuaEngine::create();
  ASSERT_TRUE(created);
  auto& engine = **created;
  auto context = engine.create_context();
  ASSERT_TRUE(context);
  auto collect = engine.create_function([&] { return engine.collect_garbage(); });
  ASSERT_TRUE(collect);
  ASSERT_TRUE(context->set("collect", *collect));
  auto callback = engine.create_function([&] { return BorrowedCrossStateProbe{*context}; });
  ASSERT_TRUE(callback);
  ASSERT_TRUE(context->set("probe", *callback));
  ASSERT_TRUE(context->load(R"LUA(
    function consume(value)
      collect()
      return value.counter + 1
    end
    function run() return probe({counter = 41}) end
  )LUA"));
  auto function = context->get<LuaFunction>("run");
  ASSERT_TRUE(function);
  auto thread = engine.create_thread();
  ASSERT_TRUE(thread);
  ASSERT_TRUE(thread->push_function(*function));
  auto result = thread->resume<int>();
  ASSERT_TRUE(result) << result.error().what();
  EXPECT_EQ(*result, std::optional<int>(42));
  EXPECT_EQ(thread->status(), LuaThread::Status::Dead);
}

TEST(LuaTest, RequireExecutesOnCallingCoroutine) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->set_require_function([](LuaStackValue environment, StringView) {
    constexpr std::string_view script = "moduleIsMain = select(2, coroutine.running())";
    return environment.stack().load(environment, std::span(script));
  }));
  ASSERT_TRUE(context->load("function loadModule() require 'module'; return moduleIsMain end"));
  auto function = context->get<LuaFunction>("loadModule");
  ASSERT_TRUE(function);
  auto thread = (*engine)->create_thread();
  ASSERT_TRUE(thread);
  ASSERT_TRUE(thread->push_function(*function));
  auto result = thread->resume<bool>();
  ASSERT_TRUE(result) << result.error().what();
  ASSERT_TRUE(*result);
  EXPECT_FALSE(**result);
}

TEST(LuaTest, StackLoadRejectsForeignEnvironment) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  auto otherEngine = LuaEngine::create();
  ASSERT_TRUE(otherEngine);
  auto otherContext = (*otherEngine)->create_context();
  ASSERT_TRUE(otherContext);
  LuaStackValue* callingEnvironment = nullptr;
  ASSERT_TRUE(otherContext->set_require_function([&](LuaStackValue environment, StringView) {
    constexpr std::string_view script = "foreignExecuted = true";
    return environment.stack().load(*callingEnvironment, std::span(script));
  }));
  ASSERT_TRUE(context->set_require_function([&](LuaStackValue environment, StringView) {
    callingEnvironment = &environment;
    auto result = otherContext->load("require 'foreign'");
    callingEnvironment = nullptr;
    return result;
  }));
  auto loaded = context->load("require 'foreign'");
  ASSERT_FALSE(loaded);
  EXPECT_TRUE(loaded.error().is<LuaException>());
  auto foreignExecuted = otherContext->contains("foreignExecuted");
  ASSERT_TRUE(foreignExecuted);
  EXPECT_FALSE(*foreignExecuted);
  ASSERT_TRUE(context->load("healthy = 42"));
  EXPECT_EQ(context->get<int>("healthy"), 42);
  ASSERT_TRUE(otherContext->load("healthy = 43"));
  EXPECT_EQ(otherContext->get<int>("healthy"), 43);
}

TEST(LuaTest, StackLoadRejectsNonTableEnvironment) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->set_require_function([](LuaStackValue environment, StringView) {
    constexpr std::string_view script = "executed = true";
    return environment.stack().load(environment.stack().value(1), std::span(script));
  }));
  auto loaded = context->load("require 'not a table'");
  ASSERT_FALSE(loaded);
  EXPECT_TRUE(loaded.error().is<LuaException>());
  auto executed = context->contains("executed");
  ASSERT_TRUE(executed);
  EXPECT_FALSE(*executed);
  ASSERT_TRUE(context->load("healthy = 42"));
  EXPECT_EQ(context->get<int>("healthy"), 42);
}

TEST(LuaTest, StackLoadRejectsDifferentCoroutineEnvironment) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  LuaStackValue* callingEnvironment = nullptr;
  ASSERT_TRUE(context->set_require_function([&](LuaStackValue environment, StringView) {
    if (callingEnvironment) {
      constexpr std::string_view script = "executed = true";
      return environment.stack().load(*callingEnvironment, std::span(script));
    }
    callingEnvironment = &environment;
    auto result = context->load("require 'main'");
    callingEnvironment = nullptr;
    return result;
  }));
  ASSERT_TRUE(context->load("function loadModule() require 'coroutine' end"));
  auto function = context->get<LuaFunction>("loadModule");
  ASSERT_TRUE(function);
  auto thread = (*engine)->create_thread();
  ASSERT_TRUE(thread);
  ASSERT_TRUE(thread->push_function(*function));
  auto result = thread->resume<bool>();
  ASSERT_FALSE(result);
  EXPECT_TRUE(result.error().is<LuaException>());
  auto executed = context->contains("executed");
  ASSERT_TRUE(executed);
  EXPECT_FALSE(*executed);
  ASSERT_TRUE(context->load("healthy = 42"));
  EXPECT_EQ(context->get<int>("healthy"), 42);
}

TEST(LuaTest, PromiseStatusDistinguishesNullFailureAndCancellation) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  auto [source, keeper] = RpcPromise<Json>::make_pair();
  std::stop_source lifetime;
  auto previous = (*engine)->set_promise_lifetime(lifetime.get_token());
  ASSERT_TRUE(context->set("pending", source));
  (*engine)->set_promise_lifetime(std::move(previous));
  ASSERT_TRUE(context->set("success", RpcPromise<Json>::make_fulfilled(Json())));
  ASSERT_TRUE(context->set("failure", RpcPromise<Json>::make_failed("remote failure")));
  EXPECT_EQ(context->eval<bool>(R"LUA(
    pending:status() == "pending" and not pending:finished() and
    success:status() == "succeeded" and success:succeeded() and success:result() == nil and
    failure:status() == "failed" and failure:failed() and not failure:cancelled()
  )LUA"),
            true);
  EXPECT_EQ(context->eval<bool>(R"LUA(
    pending:cancel() and pending:status() == "cancelled" and
    pending:finished() and pending:failed() and pending:cancelled() and
    not pending:succeeded() and pending:result() == nil and pending:error() ~= nil
  )LUA"),
            true);
  ASSERT_TRUE(keeper.fulfill(Json(77)));
  auto native = source.result();
  ASSERT_TRUE(native);
  ASSERT_TRUE(*native);
  EXPECT_EQ(**native, Json(77));
  EXPECT_EQ(context->eval<bool>(R"LUA(
    not pending:cancel() and pending:status() == "cancelled" and pending:result() == nil
  )LUA"),
            true);
  auto [producer, producerKeeper] = RpcPromise<Json>::make_pair();
  ASSERT_TRUE(context->set("producer", producer));
  ASSERT_TRUE(context->set("keeper", std::make_shared<AutoFailRpcPromiseKeeper<Json>>(producerKeeper)));
  EXPECT_EQ(context->eval<bool>("keeper:cancel() and producer:status() == 'cancelled'"), true);
}

TEST(LuaTest, PromiseScopeSeparatesObserversAndKeepsPublishedNull) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto first = (*engine)->create_context();
  auto second = (*engine)->create_context();
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  std::stop_source firstLifetime;
  std::stop_source secondLifetime;
  auto [source, keeper] = RpcPromise<Json>::make_pair();
  auto previous = (*engine)->set_promise_lifetime(firstLifetime.get_token());
  ASSERT_TRUE(first->set("promise", source));
  (*engine)->set_promise_lifetime(secondLifetime.get_token());
  ASSERT_TRUE(second->set("promise", source));
  (*engine)->set_promise_lifetime(std::move(previous));
  firstLifetime.request_stop();
  EXPECT_EQ(first->eval<String>("promise:status()"), String("cancelled"));
  EXPECT_EQ(second->eval<String>("promise:status()"), String("pending"));
  EXPECT_EQ(source.finished(), false);
  ASSERT_TRUE(keeper.fulfill(Json()));
  EXPECT_EQ(second->eval<bool>("promise:succeeded() and promise:result() == nil"), true);
  secondLifetime.request_stop();
  EXPECT_EQ(second->eval<String>("promise:status()"), String("succeeded"));
  EXPECT_EQ(first->eval<String>("promise:status()"), String("cancelled"));
}

TEST(LuaTest, ClosingPromiseScopeDoesNotEvaluateLazyConversion) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  int evaluations = 0;
  auto source = RpcPromise<Json>::make_fulfilled(Json(27));
  auto wrapped = source.wrap([&](Json value) -> std::expected<Json, StarException> {
    ++evaluations;
    return value;
  });
  ASSERT_TRUE(wrapped);
  std::stop_source lifetime;
  auto previous = (*engine)->set_promise_lifetime(lifetime.get_token());
  ASSERT_TRUE(context->set("promise", *wrapped));
  (*engine)->set_promise_lifetime(std::move(previous));
  lifetime.request_stop();
  EXPECT_EQ(context->eval<bool>("promise:cancelled() and promise:result() == nil"), true);
  EXPECT_EQ(evaluations, 0);
  auto independent = wrapped->result();
  ASSERT_TRUE(independent);
  ASSERT_TRUE(*independent);
  EXPECT_EQ(**independent, Json(27));
  EXPECT_EQ(evaluations, 1);
}
