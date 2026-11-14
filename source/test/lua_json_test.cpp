import std;
import star.format;
import star.lua;
import star.lua_converters;
import star.string;
import star.string_view;
import star.json;

#include "gtest/gtest.h"

using namespace Star;

TEST(LuaJsonTest, Scope) {
  auto engineResult = LuaEngine::create();
  ASSERT_TRUE(engineResult);
  auto& engine = *engineResult;
  auto script1Result = engine->compile(
      R"SCRIPT(
        function increment()
          self.called = self.called + 1
          return self.called
        end
      )SCRIPT");
  ASSERT_TRUE(script1Result);
  auto script1 = std::move(*script1Result);

  auto script2Result = engine->compile(
      R"SCRIPT(
        scopeValue = 42

        function increment()
          self.called = self.called + 1
          return self.called
        end
      )SCRIPT");
  ASSERT_TRUE(script2Result);
  auto script2 = std::move(*script2Result);

  auto context1Result = engine->createContext();
  ASSERT_TRUE(context1Result);
  auto context1 = std::move(*context1Result);
  auto context2Result = engine->createContext();
  ASSERT_TRUE(context2Result);
  auto context2 = std::move(*context2Result);

  ASSERT_TRUE((context1.load(script1)));
  ASSERT_TRUE((context2.load(script2)));

  ASSERT_TRUE((context1.setPath("self", JsonObject{{"called", 0}})));
  ASSERT_TRUE((context2.setPath("self", JsonObject{{"called", 0}})));

  EXPECT_EQ(context1.contains("self"), true);
  EXPECT_EQ(context1.contains("increment"), true);

  EXPECT_EQ(context1.invokePath<Json>("increment"), 1);
  EXPECT_EQ(context1.invokePath<Json>("increment"), 2);
  EXPECT_EQ(context1.invokePath<Json>("increment"), 3);
  EXPECT_EQ(context2.invokePath<Json>("increment"), 1);
  EXPECT_EQ(context1.invokePath<Json>("increment"), 4);
  EXPECT_EQ(context2.invokePath<Json>("increment"), 2);

  auto context3Result = engine->createContext();
  ASSERT_TRUE(context3Result);
  auto context3 = std::move(*context3Result);
  ASSERT_TRUE((context3.load(script2)));
  ASSERT_TRUE((context3.setPath("self", JsonObject{{"called", 0}})));

  EXPECT_EQ(context2.invokePath<Json>("increment"), 3);
  EXPECT_EQ(context3.invokePath<Json>("increment"), 1);
  EXPECT_EQ(context1.invokePath<Json>("increment"), 5);

  EXPECT_EQ(context1.contains("scopeValue"), false);
  EXPECT_EQ(context3.contains("scopeValue"), true);
}

TEST(LuaJsonTest, FunkyRecursion) {
  auto engineResult = LuaEngine::create();
  ASSERT_TRUE(engineResult);
  auto& engine = *engineResult;

  auto context1Result = engine->createContext();
  ASSERT_TRUE(context1Result);
  auto context1 = std::move(*context1Result);
  ASSERT_TRUE((context1.load(
      R"SCRIPT(
        mine = 1

        function util()
          mine = 1
          return mine
        end

        function util3()
          mine = 1
          return callbacks.util2()
        end
      )SCRIPT")));

  auto context2Result = engine->createContext();
  ASSERT_TRUE(context2Result);
  auto context2 = std::move(*context2Result);
  ASSERT_TRUE((context2.load(
      R"SCRIPT(
        mine = 2

        function util2()
          return 4
        end

        function entry()
          local other = callbacks.util()
          return {other, mine}
        end

        function entry2()
          local other = callbacks.util2()
          return {other, mine}
        end

        function entry3()
          local other = callbacks.util3()
          return {other, mine}
        end
      )SCRIPT")));

  LuaCallbacks callbacks;
  ASSERT_TRUE((callbacks.registerCallback("util", [&context1]() -> std::expected<Json, StarException> { return context1.invokePath<Json>("util"); })));
  ASSERT_TRUE((callbacks.registerCallback("util2", [&context2]() -> std::expected<Json, StarException> { return context2.invokePath<Json>("util2"); })));
  ASSERT_TRUE((callbacks.registerCallback("util3", [&context1]() -> std::expected<Json, StarException> { return context1.invokePath<Json>("util3"); })));

  ASSERT_TRUE((context1.setCallbacks("callbacks", callbacks)));
  ASSERT_TRUE((context2.setCallbacks("callbacks", callbacks)));

  auto resResult = context2.invokePath<Json>("entry");
  ASSERT_TRUE(resResult);
  auto res = std::move(*resResult);
  EXPECT_EQ(res.get(0), 1);
  EXPECT_EQ(res.get(1), 2);

  auto res2Result = context2.invokePath<Json>("entry2");
  ASSERT_TRUE(res2Result);
  auto res2 = std::move(*res2Result);
  EXPECT_EQ(res2.get(0), 4);
  EXPECT_EQ(res2.get(1), 2);

  auto res3Result = context2.invokePath<Json>("entry3");
  ASSERT_TRUE(res3Result);
  auto res3 = std::move(*res3Result);
  EXPECT_EQ(res3.get(0), 4);
  EXPECT_EQ(res3.get(1), 2);
}

TEST(LuaJsonTest, TypeConversion) {
  auto engineResult = LuaEngine::create();
  ASSERT_TRUE(engineResult);
  auto& engine = *engineResult;
  auto contextResult = engine->createContext();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);
  ASSERT_TRUE((context.load(
      R"SCRIPT(
        var1 = "1"
        var2 = {}
      )SCRIPT")));

  EXPECT_EQ(context.getPath<String>("var1"), "1");
  EXPECT_EQ(context.getPath<double>("var1"), 1.0);
  EXPECT_EQ(context.getPath<JsonArray>("var2"), JsonArray());
  EXPECT_EQ(context.getPath<JsonObject>("var2"), JsonObject());
  EXPECT_EQ(context.eval<JsonObject>("jarray()"), JsonObject());
  EXPECT_EQ(context.eval<JsonArray>("jobject()"), JsonArray());
}

TEST(LuaJsonTest, ChunkBoundaries) {
  auto engineResult = LuaEngine::create();
  ASSERT_TRUE(engineResult);
  auto& engine = *engineResult;
  auto contextResult = engine->createContext();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);

  ASSERT_TRUE((context.load(
      R"SCRIPT(
        function func()
          return env.thing()
        end
      )SCRIPT")));

  ASSERT_TRUE((context.load(
      R"SCRIPT(
        env = {}

        function env.thing()
          local temp = {
            foo = extern.var
          }

          return temp.foo
        end
      )SCRIPT")));

  ASSERT_TRUE((context.setPath("extern", JsonObject{{"var", 1}})));

  EXPECT_EQ(context.invokePath<int>("func"), 1);
}

TEST(LuaJsonTest, CustomObjectType) {
  auto engineResult = LuaEngine::create();
  ASSERT_TRUE(engineResult);
  auto& engine = *engineResult;
  auto contextResult = engine->createContext();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);

  ASSERT_TRUE((context.load(
      R"SCRIPT(
        function createObject()
          map = jobject()
          map.foo = 'hello'
          map.bar = nil
          map.baz = nil
          return map
        end

        function handleObject(arg)
          arg.bar = 'noodles'
          arg.test1 = jarray()
          arg.test2 = jobject()
          return arg
        end

        function iteratePairs()
          local map = jobject()
          map.foo = 1
          map.foo = nil
          map.bar = 1

          local keys = {}
          for key, val in pairs(map) do
            table.insert(keys, key)
          end
          return keys
        end

        function nilsRemoved()
          local map = jobject()
          map.foo = 1
          map.foo = nil
          return rawget(map, "foo")
        end

        function removeObject(arg, key)
          jremove(arg, key)
          return arg
        end
      )SCRIPT")));

  auto testResult = context.invokePath<JsonObject>("createObject");
  ASSERT_TRUE(testResult);
  auto test = std::move(*testResult);
  JsonObject comp = {{"foo", "hello"}, {"bar", Json()}, {"baz", Json()}};
  EXPECT_EQ(test, comp);

  auto updatedTest = context.invokePath<JsonObject>("handleObject", JsonObject{
      {"foo", JsonArray()},
      {"bar", Json()},
      {"baz", "hunky dory"},
      {"baf", Json()}
    });
  ASSERT_TRUE(updatedTest);
  test = std::move(*updatedTest);
  comp = {
    {"foo", JsonArray()},
    {"bar", "noodles"},
    {"baz", "hunky dory"},
    {"baf", Json()},
    {"test1", JsonArray()},
    {"test2", JsonObject()}
  };
  EXPECT_EQ(test, comp);

  auto testArrayResult = context.invokePath<JsonArray>("iteratePairs");
  ASSERT_TRUE(testArrayResult);
  auto testArray = std::move(*testArrayResult);
  JsonArray compArray = {"bar"};
  EXPECT_EQ(testArray, compArray);

  auto testValueResult = context.invokePath<Json>("nilsRemoved");
  ASSERT_TRUE(testValueResult);
  auto testValue = std::move(*testValueResult);
  EXPECT_EQ(testValue, Json());

  auto testValue2Result =
      context.invokePath<Json>("removeObject", JsonObject{{"foo", 1}, {"bar", Json()}, {"baz", Json()}}, "bar");
  ASSERT_TRUE(testValue2Result);
  auto testValue2 = std::move(*testValue2Result);
  Json compValue2 = JsonObject{{"foo", 1}, {"baz", Json()}};
  EXPECT_EQ(testValue2, compValue2);
}

TEST(LuaJsonTest, CustomArrayType) {
  auto engineResult = LuaEngine::create();
  ASSERT_TRUE(engineResult);
  auto& engine = *engineResult;
  auto contextResult = engine->createContext();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);

  ASSERT_TRUE((context.load(
      R"SCRIPT(
        function createArray()
          list = jarray()
          list[1] = 1
          list[2] = 1
          list[3] = 1
          list[7] = 1
          list[12] = nil
          return list
        end

        function handleArray(arg)
          arg[1] = 'noodles'
          arg[4] = jarray()
          arg[9] = jobject()
          arg[10] = nil
          return arg
        end

        function iteratePairs()
          local list = jarray()
          list[1] = 1
          list[5] = nil
          list[4] = 1
          list[9] = nil

          local keys = {}
          for key, val in pairs(list) do
            table.insert(keys, key)
          end
          return keys
        end

        function resizeArray(arg, size)
          jresize(arg, size)
          return arg
        end

        function listSize(list)
          return jsize(list)
        end

        function listSize2()
          return jsize({1, 1, 1, 1, 1})
        end
      )SCRIPT")));

  auto testResult = context.invokePath<JsonArray>("createArray");
  ASSERT_TRUE(testResult);
  auto test = std::move(*testResult);
  JsonArray comp = {
      1, 1, 1, {}, {}, {}, 1, {}, {}, {}, {}, {},
  };
  EXPECT_EQ(test, comp);

  auto updatedTest = context.invokePath<JsonArray>("handleArray",
      JsonArray{
          2, {}, 5, 6, {}, {}, "omg",
      });
  ASSERT_TRUE(updatedTest);
  test = std::move(*updatedTest);
  comp = {
      "noodles", {}, 5, JsonArray{}, {}, {}, "omg", {}, JsonObject{}, {},
  };
  EXPECT_EQ(test, comp);

  auto testArrayResult = context.invokePath<JsonArray>("iteratePairs");
  ASSERT_TRUE(testArrayResult);
  auto testArray = std::move(*testArrayResult);
  JsonArray compArray = {1, 4};
  EXPECT_EQ(testArray, compArray);

  auto testArray2Result = context.invokePath<JsonArray>("resizeArray", JsonArray{1, 2, 3, 4, 5, Json(), Json(), 8}, 4);
  ASSERT_TRUE(testArray2Result);
  auto testArray2 = std::move(*testArray2Result);
  JsonArray compArray2 = {1, 2, 3, 4};
  EXPECT_EQ(testArray2, compArray2);

  auto testArray3Result = context.invokePath<JsonArray>("resizeArray", JsonArray{1, 2, 3, 4}, 6);
  ASSERT_TRUE(testArray3Result);
  auto testArray3 = std::move(*testArray3Result);
  JsonArray compArray3 = {1, 2, 3, 4, Json(), Json()};
  EXPECT_EQ(testArray3, compArray3);

  auto test4Result = context.invokePath<Json>("listSize", JsonArray{1, 2, 3, 4, Json(), Json(), Json()});
  ASSERT_TRUE(test4Result);
  auto test4 = std::move(*test4Result);
  EXPECT_EQ(test4, 7);

  EXPECT_EQ(context.invokePath<Json>("listSize2"), Json(5));
}

TEST(LuaJsonTest, CustomArrayType2) {
  auto engineResult = LuaEngine::create();
  ASSERT_TRUE(engineResult);
  auto& engine = *engineResult;
  auto contextResult = engine->createContext();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);

  ASSERT_TRUE((context.load(
      R"SCRIPT(
        function doTest()
          sampleTable = jobject()
          sampleTable[18] = 0
          sampleTable[37] = 0

          targetTable = jobject()

          for k, v in pairs(sampleTable) do
            targetTable[k] = v
          end

          return targetTable
        end

        function arrayLogic1()
          l = {}
          l[1] = "foo"
          l[2] = "bar"
          return l
        end

        function arrayLogic2()
          l = {}
          l[1] = "foo"
          l[2.1] = "bar"
          return l
        end

        function arrayLogic3()
          l = jarray()
          l[1] = "foo"
          l[2] = "bar"
          return l
        end
      )SCRIPT")));

  auto testResult = context.invokePath<JsonObject>("doTest");
  ASSERT_TRUE(testResult);
  auto test = std::move(*testResult);
  JsonObject comp = {{"18", 0}, {"37", 0}};
  EXPECT_EQ(test, comp);

  auto arrayTest1Result = context.invokePath<Json>("arrayLogic1");
  ASSERT_TRUE(arrayTest1Result);
  auto arrayTest1 = std::move(*arrayTest1Result);
  Json arrayComp1 = JsonArray{"foo", "bar"};
  EXPECT_EQ(arrayTest1, arrayComp1);

  auto arrayTest2Result = context.invokePath<Json>("arrayLogic2");
  ASSERT_TRUE(arrayTest2Result);
  auto arrayTest2 = std::move(*arrayTest2Result);
  Json arrayComp2 = JsonObject{{"1", "foo"}, {"2.1", "bar"}};
  EXPECT_EQ(arrayTest2, arrayComp2);

  auto arrayTest3Result = context.invokePath<Json>("arrayLogic3");
  ASSERT_TRUE(arrayTest3Result);
  auto arrayTest3 = std::move(*arrayTest3Result);
  Json arrayComp3 = JsonArray{"foo", "bar"};
  EXPECT_EQ(arrayTest3, arrayComp3);
}

TEST(LuaJsonTest, IntFloat) {
  auto engineResult = LuaEngine::create();
  ASSERT_TRUE(engineResult);
  auto& engine = *engineResult;
  auto contextResult = engine->createContext();
  ASSERT_TRUE(contextResult);
  auto context = std::move(*contextResult);

  ASSERT_TRUE((context.load(
      R"SCRIPT(
        function returnFloat()
          return 1.0
        end

        function returnInt()
          return 1
        end

        function printNumber(n)
          return tostring(n)
        end
      )SCRIPT")));

  auto floatResult = context.invokePath<Json>("returnFloat");
  ASSERT_TRUE(floatResult);
  EXPECT_TRUE(floatResult->isType(Json::Type::Float));
  auto intResult = context.invokePath<Json>("returnInt");
  ASSERT_TRUE(intResult);
  EXPECT_TRUE(intResult->isType(Json::Type::Int));
  EXPECT_EQ(context.invokePath<String>("printNumber", 1.0), "1.0");
  EXPECT_EQ(context.invokePath<String>("printNumber", 1), "1");
}

TEST(LuaJsonTest, MixedKeysPreserveNestedJsonKinds) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->createContext();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->load(R"SCRIPT(
    function mixedKeys()
      local result = jobject()
      result[1] = jarray()
      result[2.5] = jobject()
      result.named = nil
      result.nested = {jarray(), jobject()}
      return result
    end
  )SCRIPT"));
  auto result = context->invokePath<Json>("mixedKeys");
  ASSERT_TRUE(result);
  Json expected = JsonObject{{"1", JsonArray{}}, {"2.5", JsonObject{}},
      {"named", Json()}, {"nested", JsonArray{JsonArray{}, JsonObject{}}}};
  EXPECT_EQ(*result, expected);
}

TEST(LuaJsonTest, ActualValuesOverrideNullTracking) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->createContext();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->load(R"SCRIPT(
    function overwrittenNulls()
      local object = jobject()
      object.value = nil
      rawset(object, "value", 42)
      local array = jarray()
      array[1] = nil
      rawset(array, 1, 7)
      return {object, array}
    end
  )SCRIPT"));
  auto result = context->invokePath<Json>("overwrittenNulls");
  ASSERT_TRUE(result);
  Json expected = JsonArray{JsonObject{{"value", 42}}, JsonArray{7}};
  EXPECT_EQ(*result, expected);
}

TEST(LuaJsonTest, InvalidArrayIndexDoesNotPoisonContext) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->createContext();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->load(R"SCRIPT(
    function invalidArray()
      local array = jarray()
      array[0] = 42
      return array
    end
    function validArray()
      return {42}
    end
  )SCRIPT"));
  auto invalid = context->invokePath<Json>("invalidArray");
  EXPECT_FALSE(invalid);
  auto valid = context->invokePath<Json>("validArray");
  ASSERT_TRUE(valid);
  EXPECT_EQ(*valid, Json(JsonArray{42}));
}

TEST(LuaJsonTest, UnsupportedFunctionsAreConversionMisses) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->createContext();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->load(R"SCRIPT(
    function unsupportedValue()
      return {value = function() end}
    end
    function unsupportedKey()
      return {[function() end] = 42}
    end
  )SCRIPT"));
  for (std::string_view name : {"unsupportedValue", "unsupportedKey"}) {
    auto value = context->invokePath<LuaValue>(name);
    ASSERT_TRUE(value);
    auto converted = (*engine)->luaMaybeTo<Json>(*value);
    ASSERT_TRUE(converted);
    EXPECT_FALSE(*converted);
    auto array = (*engine)->luaMaybeTo<JsonArray>(*value);
    ASSERT_TRUE(array);
    EXPECT_FALSE(*array);
    auto object = (*engine)->luaMaybeTo<JsonObject>(*value);
    ASSERT_TRUE(object);
    EXPECT_FALSE(*object);
  }
}

TEST(LuaJsonTest, UnsupportedNativeValuesRemainErrorsBeforeConversionMisses) {
  auto engine = LuaEngine::create(false);
  ASSERT_TRUE(engine);
  auto context = (*engine)->createContext();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->load(R"SCRIPT(
    local captured = {}
    local function holder() return captured end
    local token = debug.upvalueid(holder, 1)
    function invalidValue() return {value = token} end
    function invalidKey() return {[token] = function() end} end
    function invalidValueWithFunctionKey() return {[function() end] = token} end
    function invalidNullTracking()
      local result = jobject()
      getmetatable(result).__nils.ignored = token
      return result
    end
  )SCRIPT"));
  for (std::string_view name : {"invalidValue", "invalidKey", "invalidValueWithFunctionKey", "invalidNullTracking"}) {
    auto value = context->invokePath<LuaValue>(name);
    ASSERT_TRUE(value);
    auto converted = (*engine)->luaMaybeTo<Json>(*value);
    ASSERT_FALSE(converted);
    EXPECT_TRUE(converted.error().is<LuaException>());
    auto array = (*engine)->luaMaybeTo<std::variant<JsonArray, LuaTable>>(*value);
    ASSERT_FALSE(array);
    EXPECT_TRUE(array.error().is<LuaException>());
    auto object = (*engine)->luaMaybeTo<std::variant<JsonObject, LuaTable>>(*value);
    ASSERT_FALSE(object);
    EXPECT_TRUE(object.error().is<LuaException>());
  }
  EXPECT_EQ(context->eval<int>("6 * 7"), 42);
}

TEST(LuaJsonTest, EmbeddedNullStringsRetainBytesAcrossLuaOperations) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->createContext();
  ASSERT_TRUE(context);
  for (auto bytes : {std::string_view{}, std::string_view("\0", 1), std::string_view("\0a\0\0b\0", 6),
                     std::string_view("before\0after", 12), std::string_view("你\0好", 7)}) {
    auto value = (*engine)->createString(bytes);
    ASSERT_TRUE(value);
    EXPECT_EQ(value->length(), bytes.size());
    EXPECT_EQ(value->toString().utf8(), bytes);
    EXPECT_EQ(value->view().utf8(), bytes);
    ASSERT_TRUE(context->set("value", *value));
    auto concatenated = context->eval<LuaString>("value .. '\\0tail'");
    ASSERT_TRUE(concatenated);
    std::string expected(bytes);
    expected.append("\0tail", 5);
    EXPECT_EQ(concatenated->length(), expected.size());
    EXPECT_EQ(concatenated->toString().utf8(), expected);
    EXPECT_EQ(concatenated->view().utf8(), expected);
  }
}

TEST(LuaJsonTest, EmbeddedNullObjectKeysRemainDistinctDuringRoundTrip) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->createContext();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->load(R"SCRIPT(
    function embeddedNull()
      return {
        key = "plain",
        ["key\0tail"] = "extended",
        ["\0key"] = "leading",
        nested = {"before\0after", "\0a\0\0b\0", "你\0好"}
      }
    end
  )SCRIPT"));
  Json expected =
    JsonObject{{"key", "plain"},
               {String(std::string_view("key\0tail", 8)), "extended"},
               {String(std::string_view("\0key", 4)), "leading"},
               {"nested",
                JsonArray{String(std::string_view("before\0after", 12)), String(std::string_view("\0a\0\0b\0", 6)),
                          String(std::string_view("你\0好", 7))}}};
  auto converted = context->invokePath<Json>("embeddedNull");
  ASSERT_TRUE(converted);
  EXPECT_EQ(*converted, expected);
  ASSERT_TRUE(context->set("input", expected));
  auto selected = context->eval<Json>(R"SCRIPT(
    return {input.key, input["key\0tail"], input["\0key"], input.nested[1], input.nested[2], input.nested[3]}
  )SCRIPT");
  ASSERT_TRUE(selected);
  EXPECT_EQ(*selected,
            Json(JsonArray{"plain", "extended", "leading", String(std::string_view("before\0after", 12)),
                           String(std::string_view("\0a\0\0b\0", 6)), String(std::string_view("你\0好", 7))}));
}
