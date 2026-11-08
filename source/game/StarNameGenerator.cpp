import star.memory;
import star.json;
import star.json_extra;
import star.ordered_map;
import star.bi_map;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.string;
import star.format;
import star.version;
import star.random;

import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.name_generator;

namespace Star {

PatternedNameGenerator::PatternedNameGenerator(Root& root) : m_root(root) {}

std::expected<std::shared_ptr<PatternedNameGenerator>, StarException> PatternedNameGenerator::create(Root& root) {
  auto result = std::shared_ptr<PatternedNameGenerator>(new PatternedNameGenerator(root));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scanExtension("namesource");
  if (!files)
    return std::unexpected(std::move(files.error()));
  auto queued = (*assets)->queueJsons(files->get());
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  for (auto const& assetFile : files->get()) {
    auto const& file = assetFile.string();
    auto loaded = [&]() -> std::expected<void, StarException> {
      auto config = (*assets)->json(file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      auto name = config->getString("name");
      if (!name)
        return std::unexpected(std::move(name.error()));
      if (result->m_markovSources.contains(*name))
        return std::unexpected(NameGeneratorException::format("Duplicate name source '{}', config file '{}'", *name, file));
      auto prefixSize = config->getUInt("prefixSize", 1);
      if (!prefixSize)
        return std::unexpected(std::move(prefixSize.error()));
      auto endSize = config->getUInt("endSize", 1);
      if (!endSize)
        return std::unexpected(std::move(endSize.error()));
      auto namesConfig = config->get("sourceNames");
      if (!namesConfig)
        return std::unexpected(std::move(namesConfig.error()));
      auto names = jsonToStringList(*namesConfig);
      if (!names)
        return std::unexpected(std::move(names.error()));
      auto source = result->makeMarkovSource(*prefixSize, *endSize, std::move(*names));
      if (!source)
        return std::unexpected(std::move(source.error()));
      result->m_markovSources[*name] = std::move(*source);
      return {};
    }();
    if (!loaded)
      return std::unexpected(NameGeneratorException(strf("Error reading name source config {}", file), loaded.error()));
  }
  auto profanityConfig = (*assets)->json("/names/profanityfilter.config");
  if (!profanityConfig)
    return std::unexpected(std::move(profanityConfig.error()));
  auto profanity = profanityConfig->iterateArray();
  if (!profanity)
    return std::unexpected(std::move(profanity.error()));
  for (auto const& entry : *profanity) {
    auto word = entry.toString();
    if (!word)
      return std::unexpected(std::move(word.error()));
    auto lower = word->toLower();
    if (!lower)
      return std::unexpected(std::move(lower.error()));
    result->m_profanityFilter.add(std::move(*lower));
  }
  return result;
}

std::expected<String, StarException> PatternedNameGenerator::generateName(String const& rulesAsset) const {
  RandomSource random(m_root.random().randu64());
  return generateName(rulesAsset, random);
}

std::expected<String, StarException> PatternedNameGenerator::generateName(String const& rulesAsset, std::uint64_t seed) const {
  RandomSource random(seed);
  return generateName(rulesAsset, random);
}

std::expected<String, StarException> PatternedNameGenerator::generateName(String const& rulesAsset, RandomSource& random) const {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json(rulesAsset);
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto rules = config->arrayPtr();
  if (!rules)
    return std::unexpected(std::move(rules.error()));
  String result;
  for (int tries = 100; tries > 0; --tries) {
    if (!result.empty()) {
      auto profane = isProfane(result);
      if (!profane)
        return std::unexpected(std::move(profane.error()));
      if (!*profane)
        break;
    }
    auto generated = processRule(**rules, random);
    if (!generated)
      return std::unexpected(std::move(generated.error()));
    result = std::move(*generated);
  }
  return result;
}

std::expected<String, StarException> PatternedNameGenerator::processRule(JsonArray const& rule, RandomSource& random) const {
  if (rule.empty())
    return String();
  Json meta;
  String result;
  String mode = "alts";
  std::size_t index = 0;
  bool titleCase = false;
  if (rule[0].isType(Json::Type::Object)) {
    meta = rule[0];
    auto configuredMode = meta.getString("mode", mode);
    if (!configuredMode)
      return std::unexpected(std::move(configuredMode.error()));
    mode = std::move(*configuredMode);
    auto configuredTitleCase = meta.getBool("titleCase", false);
    if (!configuredTitleCase)
      return std::unexpected(std::move(configuredTitleCase.error()));
    titleCase = *configuredTitleCase;
    ++index;
  }
  auto evaluate = [&](Json const& entry) -> std::expected<String, StarException> {
    if (!entry.isType(Json::Type::Array))
      return entry.toString();
    auto nested = entry.arrayPtr();
    if (!nested)
      return std::unexpected(std::move(nested.error()));
    return processRule(**nested, random);
  };
  if (mode == "serie") {
    for (; index < rule.size(); ++index) {
      auto part = evaluate(rule[index]);
      if (!part)
        return std::unexpected(std::move(part.error()));
      result += *part;
    }
  } else if (mode == "alts") {
    if (index == rule.size())
      return std::unexpected(NameGeneratorException("Name rule has no alternatives"));
    auto selected = random.randInt(rule.size() - 1 - index);
    auto part = evaluate(rule[index + selected]);
    if (!part)
      return std::unexpected(std::move(part.error()));
    result += *part;
  } else if (mode == "markov") {
    auto sourceName = meta.getString("source");
    if (!sourceName)
      return std::unexpected(std::move(sourceName.error()));
    auto source = m_markovSources.find_ref(*sourceName);
    if (!source)
      return std::unexpected(NameGeneratorException::format("Unknown name source '{}'", *sourceName));
    auto range = meta.get("targetLength").and_then([](Json const& value) { return value.arrayPtr(); });
    if (!range)
      return std::unexpected(std::move(range.error()));
    if ((*range)->size() < 2)
      return std::unexpected(NameGeneratorException("Name targetLength must contain two bounds"));
    auto minimum = (**range)[0].toUInt();
    if (!minimum)
      return std::unexpected(std::move(minimum.error()));
    auto maximum = (**range)[1].toUInt();
    if (!maximum)
      return std::unexpected(std::move(maximum.error()));
    auto target = random.randUInt(*minimum, *maximum);
    if (!target)
      return std::unexpected(std::move(target.error()));
    String piece;
    for (std::size_t tries = 0; tries < 10; ++tries) {
      auto start = random.randFrom(source->get().starts);
      if (!start)
        return std::unexpected(std::move(start.error()));
      piece = start->get();
      while (true) {
        auto length = piece.length();
        if (!length)
          return std::unexpected(std::move(length.error()));
        if (*length >= *target) {
          auto ending = piece.slice(*length - source->get().endSize, *length);
          if (!ending)
            return std::unexpected(std::move(ending.error()));
          if (source->get().ends.contains(*ending))
            break;
        }
        auto link = piece.slice(*length - source->get().prefixSize, *length);
        if (!link)
          return std::unexpected(std::move(link.error()));
        auto chain = source->get().chains.find_ref(*link);
        if (!chain)
          break;
        auto next = random.randFrom(chain->get());
        if (!next)
          return std::unexpected(std::move(next.error()));
        piece += next->get();
      }
      auto length = piece.length();
      if (!length)
        return std::unexpected(std::move(length.error()));
      if (*length <= *maximum)
        break;
    }
    result += piece;
  } else {
    return std::unexpected(StarException::format("Unknown mode: {}", mode));
  }
  if (titleCase)
    return result.titleCase();
  return result;
}

std::expected<bool, StarException> PatternedNameGenerator::isProfane(String const& name) const {
  auto lower = name.toLower();
  if (!lower)
    return std::unexpected(std::move(lower.error()));
  auto rotated = lower->rot13();
  if (!rotated)
    return std::unexpected(std::move(rotated.error()));
  auto matches = rotated->splitAny(" -");
  if (!matches)
    return std::unexpected(std::move(matches.error()));
  for (auto const& word : m_profanityFilter) {
    for (auto const& match : *matches) {
      auto found = match.find(word);
      if (!found)
        return std::unexpected(std::move(found.error()));
      if (*found == NPos)
        continue;
      if (*found == 0)
        return true;
      auto wordSize = word.size();
      if (!wordSize)
        return std::unexpected(std::move(wordSize.error()));
      auto matchSize = match.size();
      if (!matchSize)
        return std::unexpected(std::move(matchSize.error()));
      if (*wordSize + 1 >= *matchSize)
        return true;
    }
  }
  return false;
}

std::expected<MarkovSource, StarException> PatternedNameGenerator::makeMarkovSource(std::size_t prefixSize, std::size_t endSize, StringList sourceNames) {
  MarkovSource source;
  source.prefixSize = prefixSize;
  source.endSize = endSize;
  for (auto const& original : sourceNames) {
    auto length = original.length();
    if (!length)
      return std::unexpected(std::move(length.error()));
    if (*length < prefixSize || *length < endSize)
      continue;
    auto name = original.toLower();
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto lowerLength = name->length();
    if (!lowerLength)
      return std::unexpected(std::move(lowerLength.error()));
    auto ending = name->slice(*lowerLength - endSize, *lowerLength);
    if (!ending)
      return std::unexpected(std::move(ending.error()));
    source.ends.add(std::move(*ending));
    for (std::size_t i = 0; i + prefixSize <= *lowerLength; ++i) {
      auto prefix = name->slice(i, i + prefixSize);
      if (!prefix)
        return std::unexpected(std::move(prefix.error()));
      if (i == 0)
        source.starts.push_back(*prefix);
      if (i + prefixSize < *lowerLength) {
        auto next = name->slice(i + prefixSize, i + prefixSize + 1);
        if (!next)
          return std::unexpected(std::move(next.error()));
        source.chains[*prefix].push_back(std::move(*next));
      }
    }
  }
  return source;
}

}
