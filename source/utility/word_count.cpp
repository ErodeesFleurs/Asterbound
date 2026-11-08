#include <cstdio>
import std;
import star.format;
import star.output_proxy;
import star.json;
import star.native_runtime;
import star.string;
import star.list;
import star.set;
import star.map;
import star.logging;
import star.option_parser;
import star.assets;
import star.root;
import star.root_loader;

using namespace Star;

[[nodiscard]]
std::expected<int, StarException> countWords(String const& text) {
  auto words = text.split(" ");
  if (!words)
    return std::unexpected(std::move(words.error()));
  return static_cast<int>(words->size());
}

[[nodiscard]]
std::expected<int, StarException> countFields(Json const& json, std::initializer_list<char const*> fields) {
  int count = 0;
  for (auto field : fields) {
    auto text = json.getString(field, "");
    if (!text)
      return std::unexpected(std::move(text.error()));
    auto words = countWords(*text);
    if (!words)
      return std::unexpected(std::move(words.error()));
    count += *words;
  }
  return count;
}

[[nodiscard]]
std::expected<int, StarException> countOnlyStrings(Json const& json) {
  int count = 0;
  if (json.isType(Json::Type::Object)) {
    auto entries = json.iterateObject();
    if (!entries)
      return std::unexpected(std::move(entries.error()));
    for (auto const& entry : *entries) {
      auto words = countOnlyStrings(entry.second);
      if (!words)
        return std::unexpected(std::move(words.error()));
      count += *words;
    }
  } else if (json.isType(Json::Type::Array)) {
    auto entries = json.iterateArray();
    if (!entries)
      return std::unexpected(std::move(entries.error()));
    for (auto const& entry : *entries) {
      auto words = countOnlyStrings(entry);
      if (!words)
        return std::unexpected(std::move(words.error()));
      count += *words;
    }
  } else if (json.isType(Json::Type::String)) {
    auto text = json.toString();
    if (!text)
      return std::unexpected(std::move(text.error()));
    auto assetPath = text->starts_with("/");
    if (!assetPath)
      return std::unexpected(std::move(assetPath.error()));
    if (!*assetPath)
      return countWords(*text);
  }
  return count;
}

int main(int argc, char** argv) {
  auto run = [&]() -> std::expected<int, StarException> {
    auto nativeRuntime = NativeRuntime::create();
    if (!nativeRuntime)
      return std::unexpected(std::move(nativeRuntime.error()));
    auto rootLoader = RootLoader::create({{}, {}, {}, LogLevel::Error, false, {}});
    if (!rootLoader)
      return std::unexpected(std::move(rootLoader.error()));
    rootLoader->setSummary("Calculate a (very approximate) word count of user-facing text in assets");
    auto initialized = rootLoader->commandInitOrDie(argc, argv);
    if (!initialized)
      return std::unexpected(std::move(initialized.error()));
    auto& root = initialized->first;
    auto assets = root->assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    StringMap<int> wordCounts;
    using CountFunction = std::function<std::expected<int, StarException>(Json const&)>;
    using FilterFunction = std::function<std::expected<bool, StarException>(String const&)>;
    auto countWordsInType = [&](String const& type, CountFunction countFunction, std::optional<FilterFunction> filterFunction = {}, std::optional<String> wordCountKey = {}) -> std::expected<void, StarException> {
      auto scanned = (*assets)->scanExtension(type);
      if (!scanned)
        return std::unexpected(std::move(scanned.error()));
      StringList files;
      for (auto const& key : scanned->get()) {
        auto const& path = key.string();
        if (filterFunction) {
          auto include = (*filterFunction)(path);
          if (!include)
            return std::unexpected(std::move(include.error()));
          if (!*include)
            continue;
        }
        files.push_back(path);
      }
      auto queued = (*assets)->queueJsons(files);
      if (!queued)
        return std::unexpected(std::move(queued.error()));
      for (auto const& path : files) {
        auto json = (*assets)->json(path);
        if (!json)
          return std::unexpected(std::move(json.error()));
        if (json->isNull())
          continue;
        auto count = countFunction(*json);
        if (!count)
          return std::unexpected(std::move(count.error()));
        String countKey = wordCountKey ? *wordCountKey : String(Star::format(".{} files", type));
        wordCounts[countKey] += *count;
      }
      return {};
    };

    StringList itemFileTypes = {
      "tech", "item", "liqitem", "matitem", "miningtool", "flashlight", "wiretool", "beamaxe", "tillingtool", "painttool", "harvestingtool", "head", "chest", "legs", "back", "currencyitem", "consumable", "blueprint", "inspectiontool", "instrument", "thrownitem", "unlock", "activeitem", "augment"
    };
    for (auto const& itemFileType : itemFileTypes) {
      auto counted = countWordsInType(itemFileType, [](Json const& json) { return countFields(json, {"shortdescription", "description"}); });
      if (!counted)
        return std::unexpected(std::move(counted.error()));
    }
    auto counted = countWordsInType("object", [](Json const& json) {
      return countFields(json, {"shortdescription", "description", "apexDescription", "avianDescription", "glitchDescription", "floranDescription", "humanDescription", "hylotlDescription", "novakidDescription"});
    });
    if (!counted)
      return std::unexpected(std::move(counted.error()));
    counted = countWordsInType("codex", [](Json const& json) -> std::expected<int, StarException> {
      auto count = countFields(json, {"title", "description"});
      if (!count)
        return std::unexpected(std::move(count.error()));
      auto pages = json.getArray("contentPages", JsonArray());
      if (!pages)
        return std::unexpected(std::move(pages.error()));
      for (auto const& page : *pages) {
        auto text = page.toString();
        if (!text)
          return std::unexpected(std::move(text.error()));
        auto words = countWords(*text);
        if (!words)
          return std::unexpected(std::move(words.error()));
        *count += *words;
      }
      return *count;
    });
    if (!counted)
      return std::unexpected(std::move(counted.error()));
    counted = countWordsInType("monstertype", [](Json const& json) { return countFields(json, {"description"}); });
    if (!counted)
      return std::unexpected(std::move(counted.error()));
    counted = countWordsInType("radiomessages", [](Json const& json) -> std::expected<int, StarException> {
      auto messages = json.iterateObject();
      if (!messages)
        return std::unexpected(std::move(messages.error()));
      int count = 0;
      for (auto const& message : *messages) {
        auto words = countFields(message.second, {"text"});
        if (!words)
          return std::unexpected(std::move(words.error()));
        count += *words;
      }
      return count;
    });
    if (!counted)
      return std::unexpected(std::move(counted.error()));
    FilterFunction dialogFilter = [](String const& path) { return path.starts_with("/dialog/"); };
    counted = countWordsInType("config", countOnlyStrings, dialogFilter, String("NPC dialog (.config files)"));
    if (!counted)
      return std::unexpected(std::move(counted.error()));
    counted = countWordsInType("npctype", [](Json const& json) -> std::expected<int, StarException> {
      auto config = json.get("scriptConfig", Json());
      if (!config)
        return std::unexpected(std::move(config.error()));
      if (!*config)
        return 0;
      auto dialog = config->get("dialog", Json());
      if (!dialog)
        return std::unexpected(std::move(dialog.error()));
      return countOnlyStrings(*dialog);
    }, {}, String("NPC dialog (.npctype files)"));
    if (!counted)
      return std::unexpected(std::move(counted.error()));
    counted = countWordsInType("questtemplate", [](Json const& json) -> std::expected<int, StarException> {
      auto count = countFields(json, {"title", "text", "completionText"});
      if (!count)
        return std::unexpected(std::move(count.error()));
      auto config = json.get("scriptConfig", Json());
      if (!config)
        return std::unexpected(std::move(config.error()));
      if (*config) {
        auto text = config->get("generatedText", Json());
        if (!text)
          return std::unexpected(std::move(text.error()));
        auto words = countOnlyStrings(*text);
        if (!words)
          return std::unexpected(std::move(words.error()));
        *count += *words;
      }
      return *count;
    });
    if (!counted)
      return std::unexpected(std::move(counted.error()));
    counted = countWordsInType("collection", [](Json const& json) -> std::expected<int, StarException> {
      auto collectables = json.get("collectables", Json());
      if (!collectables)
        return std::unexpected(std::move(collectables.error()));
      auto entries = collectables->iterateObject();
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      int count = 0;
      for (auto const& entry : *entries) {
        auto words = countFields(entry.second, {"description"});
        if (!words)
          return std::unexpected(std::move(words.error()));
        count += *words;
      }
      return count;
    });
    if (!counted)
      return std::unexpected(std::move(counted.error()));
    counted = countWordsInType("cinematic", [](Json const& json) -> std::expected<int, StarException> {
      auto panels = json.get("panels", Json());
      if (!panels)
        return std::unexpected(std::move(panels.error()));
      auto entries = panels->iterateArray();
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      int count = 0;
      for (auto const& panel : *entries) {
        auto text = panel.optString("text");
        if (!text)
          return std::unexpected(std::move(text.error()));
        if (!*text)
          continue;
        // Ignore the pipe-separated lists of backer names in the credits.
        auto backerNames = (**text).contains("|");
        if (!backerNames)
          return std::unexpected(std::move(backerNames.error()));
        if (!*backerNames) {
          auto words = countWords(**text);
          if (!words)
            return std::unexpected(std::move(words.error()));
          count += *words;
        }
      }
      return count;
    });
    if (!counted)
      return std::unexpected(std::move(counted.error()));
    counted = countWordsInType("aimission", [](Json const& json) -> std::expected<int, StarException> {
      auto species = json.get("speciesText", Json());
      if (!species)
        return std::unexpected(std::move(species.error()));
      auto entries = species->iterateObject();
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      int count = 0;
      for (auto const& entry : *entries) {
        auto words = countFields(entry.second, {"buttonText", "repeatButtonText"});
        if (!words)
          return std::unexpected(std::move(words.error()));
        count += *words;
        auto speech = entry.second.get("selectSpeech");
        if (!speech)
          return std::unexpected(std::move(speech.error()));
        if (*speech) {
          auto speechWords = countFields(*speech, {"text"});
          if (!speechWords)
            return std::unexpected(std::move(speechWords.error()));
          count += *speechWords;
        }
      }
      return count;
    });
    if (!counted)
      return std::unexpected(std::move(counted.error()));
    auto cockpit = (*assets)->json("/interface/cockpit/cockpit.config");
    if (!cockpit)
      return std::unexpected(std::move(cockpit.error()));
    int cockpitCount = 0;
    for (auto field : {"visitableTypeDescription", "worldTypeDescription"}) {
      auto descriptions = cockpit->get(field);
      if (!descriptions)
        return std::unexpected(std::move(descriptions.error()));
      auto words = countOnlyStrings(*descriptions);
      if (!words)
        return std::unexpected(std::move(words.error()));
      cockpitCount += *words;
    }
    wordCounts["planet descriptions (cockpit.config)"] = cockpitCount;
    int total = 0;
    for (auto const& count : wordCounts) {
      if (auto written = Star::print("{} words in {}\n", count.second, count.first); !written)
        return std::unexpected(std::move(written.error()));
      total += count.second;
    }
    if (auto written = Star::print("approximately {} words total\n", total); !written)
      return std::unexpected(std::move(written.error()));
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
