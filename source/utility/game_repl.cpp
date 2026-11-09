#include <cstdio>
import std;
import star.json;
import star.native_runtime;
import star.lua;
import star.string;
import star.format;
import star.io_device;
import star.output_proxy;
import star.logging;
import star.option_parser;
import star.assets;
import star.image;
import star.asset_path;
import star.root;
import star.root_loader;
import star.utility_lua_bindings;
import star.root_lua_bindings;
import star.lua_root;

using namespace Star;

int main(int argc, char** argv) {
  auto run = [&]() -> std::expected<int, StarException> {
    auto nativeRuntime = NativeRuntime::create();
    if (!nativeRuntime)
      return std::unexpected(std::move(nativeRuntime.error()));
    auto rootLoader = RootLoader::create({{}, {}, {}, LogLevel::Error, false, {}});
    if (!rootLoader)
      return std::unexpected(std::move(rootLoader.error()));
    auto initialized = rootLoader->commandInitOrDie(argc, argv);
    if (!initialized)
      return std::unexpected(std::move(initialized.error()));
    auto& root = initialized->first;
    auto engine = LuaEngine::create(true);
    if (!engine)
      return std::unexpected(std::move(engine.error()));
    (*engine)->setExecutionHost(std::make_unique<LuaExecutionScope>(*root));
    (*engine)->setImageResolver([&root](String const& path) -> std::expected<std::shared_ptr<Image const>, StarException> {
      auto assets = root->assets();
      if (!assets)
        return std::unexpected(std::move(assets.error()));
      return (*assets)->image(path);
    });
    auto context = (*engine)->createContext();
    if (!context)
      return std::unexpected(std::move(context.error()));
    auto utilityCallbacks = LuaBindings::makeUtilityCallbacks(root->random(), root->logger(), root->logMap());
    if (!utilityCallbacks)
      return std::unexpected(std::move(utilityCallbacks.error()));
    auto registered = context->setCallbacks("sb", *utilityCallbacks);
    if (!registered)
      return std::unexpected(std::move(registered.error()));
    auto rootCallbacks = LuaBindings::makeRootCallbacks(*root);
    if (!rootCallbacks)
      return std::unexpected(std::move(rootCallbacks.error()));
    registered = context->setCallbacks("root", *rootCallbacks);
    if (!registered)
      return std::unexpected(std::move(registered.error()));

    String code;
    bool continuation = false;
    while (!std::feof(stdin)) {
      auto read_line = []() -> std::expected<String, StarException> {
        std::string line;
        for (int byte; (byte = std::fgetc(stdin)) != EOF && byte != '\n';)
          line.push_back(static_cast<char>(byte));
        if (std::ferror(stdin))
          return std::unexpected(IOException("Failed reading code from standard input"));
        return String(std::move(line));
      };
      auto prompted = Star::print("{}", continuation ? ">> " : "> ");
      if (!prompted)
        return std::unexpected(std::move(prompted.error()));
      auto line = read_line();
      if (!line)
        return std::unexpected(std::move(line.error()));
      if (continuation)
        code += *line;
      else
        code = std::move(*line);
      code += "\n";

      std::expected<void, StarException> output;
      auto evaluated = [&]() -> std::expected<void, StarException> {
        auto values = context->eval<LuaVariadic<LuaValue>>(code);
        if (!values)
          return std::unexpected(std::move(values.error()));
        for (auto const& value : *values) {
          auto text = Star::print_lua_value(value);
          if (!text)
            return std::unexpected(std::move(text.error()));
          output = Star::print("{}\n", *text);
          if (!output)
            return std::unexpected(std::move(output.error()));
        }
        return {};
      }();
      if (!output)
        return std::unexpected(std::move(evaluated.error()));
      if (evaluated) {
        continuation = false;
      } else if (evaluated.error().is<LuaIncompleteStatementException>()) {
        continuation = true;
      } else {
        if (auto written = Star::print("Error: {}\n", Star::output_exception(ExceptionDiagnostic(evaluated.error()), false)); !written)
          return std::unexpected(std::move(written.error()));
        continuation = false;
      }
    }
    return 0;
  };
  auto result = run();
  if (!result) {
    if (auto written = Star::print(stderr, "exception caught: {}\n", Star::output_exception(ExceptionDiagnostic(result.error()), true)); !written)
      return 1;
    return 1;
  }
  return *result;
}
