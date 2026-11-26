import std;
import star.assets;
import star.format;
import star.json;
import star.json_extra;
import star.name_generator;
import star.random;
import star.root;
import star.string;

namespace Star {

namespace {

// The initial strings are validated during training; transitions append one whole code point.
std::size_t next_utf8_boundary(std::string_view bytes, std::size_t offset) {
  do {
    ++offset;
  } while (offset < bytes.size() && (static_cast<unsigned char>(bytes[offset]) & 0xc0u) == 0x80u);
  return offset;
}

}

PatternedNameGenerator::PatternedNameGenerator(ConstructionKey, Root& root) : m_root(root) {}

std::expected<std::shared_ptr<PatternedNameGenerator>, StarException> PatternedNameGenerator::create(Root& root) {
  auto result = std::make_shared<PatternedNameGenerator>(ConstructionKey{}, root);
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scan_extension("namesource");
  if (!files)
    return std::unexpected(std::move(files.error()));
  auto queued = (*assets)->queue_jsons(files->get());
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  for (auto const& asset_file : files->get()) {
    auto const& file = asset_file.string();
    auto loaded = [&]() -> std::expected<void, StarException> {
      auto config = (*assets)->json(file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      auto name = config->get_string("name");
      if (!name)
        return std::unexpected(std::move(name.error()));
      if (result->m_markov_sources.contains(*name))
        return std::unexpected(NameGeneratorException::format("Duplicate name source '{}', config file '{}'", *name, file));
      auto prefix_size = config->get_uint("prefixSize", 1);
      if (!prefix_size)
        return std::unexpected(std::move(prefix_size.error()));
      auto end_size = config->get_uint("endSize", 1);
      if (!end_size)
        return std::unexpected(std::move(end_size.error()));
      auto names_config = config->get("sourceNames");
      if (!names_config)
        return std::unexpected(std::move(names_config.error()));
      auto names = json_to_string_list(*names_config);
      if (!names)
        return std::unexpected(std::move(names.error()));
      auto source = make_markov_source(*prefix_size, *end_size, std::move(*names));
      if (!source)
        return std::unexpected(std::move(source.error()));
      result->m_markov_sources.try_emplace(std::move(*name), std::move(*source));
      return {};
    }();
    if (!loaded)
      return std::unexpected(NameGeneratorException(Star::format("Error reading name source config {}", file), loaded.error()));
  }
  auto profanity_config = (*assets)->json("/names/profanityfilter.config");
  if (!profanity_config)
    return std::unexpected(std::move(profanity_config.error()));
  auto profanity = profanity_config->array_ptr();
  if (!profanity)
    return std::unexpected(std::move(profanity.error()));
  auto& filter = result->m_profanity_filter;
  filter.reserve((*profanity)->size());
  for (auto const& entry : **profanity) {
    auto word = entry.string_ptr();
    if (!word)
      return std::unexpected(std::move(word.error()));
    // ROT13 is its own inverse and commutes with ASCII case folding.
    auto decoded = (*word)->rot13();
    if (!decoded)
      return std::unexpected(std::move(decoded.error()));
    auto bytes = std::move(*decoded).utf8();
    for (auto& byte : bytes)
      byte = static_cast<char>(String::to_lower(static_cast<unsigned char>(byte)));
    filter.push_back(std::move(bytes));
  }
  std::ranges::sort(filter);
  filter.erase(std::ranges::unique(filter).begin(), filter.end());
  return result;
}

std::expected<String, StarException> PatternedNameGenerator::generate_name(String const& rules_asset) const {
  RandomSource random(m_root.random().next_u64());
  return generate_name(rules_asset, random);
}

std::expected<String, StarException> PatternedNameGenerator::generate_name(String const& rules_asset, std::uint64_t seed) const {
  RandomSource random(seed);
  return generate_name(rules_asset, random);
}

std::expected<String, StarException> PatternedNameGenerator::generate_name(String const& rules_asset, RandomSource& random) const {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json(rules_asset);
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto rules = config->array_ptr();
  if (!rules)
    return std::unexpected(std::move(rules.error()));
  constexpr int max_attempts = 100;
  for (int attempt = 0; attempt < max_attempts; ++attempt) {
    auto candidate = process_rule(**rules, random);
    if (!candidate)
      return std::unexpected(std::move(candidate.error()));
    if (candidate->empty())
      continue;
    auto profane = is_profane(*candidate);
    if (!profane)
      return std::unexpected(std::move(profane.error()));
    if (!*profane)
      return candidate;
  }
  return std::unexpected(NameGeneratorException::format("No acceptable name after {} attempts from '{}'", max_attempts, rules_asset));
}

std::expected<String, StarException> PatternedNameGenerator::process_rule(JsonArray const& rule, RandomSource& random) const {
  if (rule.empty())
    return String();
  auto const& meta = rule.front();
  String result;
  std::string_view mode = "alts";
  std::size_t index = 0;
  bool title_case = false;
  if (meta.is_type(Json::Type::Object)) {
    auto configured_mode = meta.find("mode");
    if (!configured_mode)
      return std::unexpected(std::move(configured_mode.error()));
    if (*configured_mode && !(**configured_mode).get().is_null()) {
      auto value = (**configured_mode).get().string_ptr();
      if (!value)
        return std::unexpected(std::move(value.error()));
      mode = (*value)->utf8();
    }
    auto configured_title_case = meta.get_bool("titleCase", false);
    if (!configured_title_case)
      return std::unexpected(std::move(configured_title_case.error()));
    title_case = *configured_title_case;
    ++index;
  }
  if (mode == "serie" || mode == "alts") {
    auto end_index = rule.size();
    if (mode == "alts") {
      if (index == end_index)
        return std::unexpected(NameGeneratorException("Name rule has no alternatives"));
      auto selected = random.uniform_int(end_index - 1 - index);
      if (!selected)
        return std::unexpected(std::move(selected.error()));
      index += *selected;
      end_index = index + 1;
    }
    for (; index < end_index; ++index) {
      auto const& entry = rule[index];
      if (entry.is_type(Json::Type::Array)) {
        auto nested = entry.array_ptr();
        if (!nested)
          return std::unexpected(std::move(nested.error()));
        auto part = process_rule(**nested, random);
        if (!part)
          return std::unexpected(std::move(part.error()));
        if (result.empty())
          result = std::move(*part);
        else
          result += *part;
      } else {
        auto part = entry.string_ptr();
        if (!part)
          return std::unexpected(std::move(part.error()));
        result += **part;
      }
    }
  } else if (mode == "markov") {
    auto source_name = meta.get_string("source");
    if (!source_name)
      return std::unexpected(std::move(source_name.error()));
    auto found_source = m_markov_sources.find_ref(*source_name);
    if (!found_source)
      return std::unexpected(NameGeneratorException::format("Unknown name source '{}'", *source_name));
    auto const& source = found_source->get();
    auto range = meta.get("targetLength").and_then([](Json const& value) { return value.array_ptr(); });
    if (!range)
      return std::unexpected(std::move(range.error()));
    if ((*range)->size() < 2)
      return std::unexpected(NameGeneratorException("Name targetLength must contain two bounds"));
    auto minimum = (**range)[0].to_uint();
    if (!minimum)
      return std::unexpected(std::move(minimum.error()));
    auto maximum = (**range)[1].to_uint();
    if (!maximum)
      return std::unexpected(std::move(maximum.error()));
    auto target = random.uniform_uint(*minimum, *maximum);
    if (!target)
      return std::unexpected(std::move(target.error()));
    constexpr std::size_t max_walk_steps = 4096;
    std::string piece;
    for (std::size_t tries = 0; tries < 10; ++tries) {
      auto start = random.sample_ref(source.starts);
      if (!start)
        return std::unexpected(std::move(start.error()));
      piece = start->get().utf8();
      auto length = source.prefix_size;
      std::size_t prefix_begin = 0;
      std::size_t ending_begin = 0;
      for (auto remaining = length; remaining > source.end_size; --remaining)
        ending_begin = next_utf8_boundary(piece, ending_begin);
      for (std::size_t steps = 0;; ++steps) {
        std::string_view bytes(piece);
        if (length >= *target && length >= source.end_size && source.ends.contains(bytes.substr(ending_begin)))
          break;
        auto chain = source.chains.find_ref(bytes.substr(prefix_begin));
        if (!chain)
          break;
        if (steps == max_walk_steps)
          return std::unexpected(NameGeneratorException::format("Name source '{}' exceeded the Markov walk limit of {} steps",
              *source_name, max_walk_steps));
        auto next = random.sample_ref(chain->get());
        if (!next)
          return std::unexpected(std::move(next.error()));
        piece += next->get().utf8();
        ++length;
        prefix_begin = next_utf8_boundary(piece, prefix_begin);
        if (length > source.end_size)
          ending_begin = next_utf8_boundary(piece, ending_begin);
      }
      if (length <= *maximum)
        break;
    }
    result = String(std::move(piece));
  } else {
    return std::unexpected(StarException::format("Unknown mode: {}", mode));
  }
  if (title_case)
    return result.title_case();
  return result;
}

std::expected<bool, StarException> PatternedNameGenerator::is_profane(String const& name) const {
  // Validate the whole candidate first, even if a word matches early.
  auto validated = name.size();
  if (!validated)
    return std::unexpected(std::move(validated.error()));
  if (m_profanity_filter.empty())
    return false;

  auto first_byte = [](std::string const& word) {
    return word.empty() ? -1 : static_cast<int>(static_cast<unsigned char>(word.front()));
  };
  auto matches = [&](std::string_view token, bool exact) {
    if (token.empty())
      return false;
    auto leading = static_cast<int>(String::to_lower(static_cast<unsigned char>(token.front())));
    auto word = std::ranges::lower_bound(m_profanity_filter, leading, {}, first_byte);
    for (; word != m_profanity_filter.end() && first_byte(*word) == leading; ++word) {
      if (word->size() > token.size() || (exact && word->size() != token.size()))
        continue;
      if (std::ranges::equal(*word, token.substr(0, word->size()), [](char expected, char actual) {
            return expected == static_cast<char>(String::to_lower(static_cast<unsigned char>(actual)));
          }))
        return true;
    }
    return false;
  };

  constexpr std::string_view delimiters = " -";
  std::string_view bytes = name.utf8();
  while (true) {
    auto first = bytes.find_first_not_of(delimiters);
    if (first == std::string_view::npos)
      return false;
    bytes.remove_prefix(first);
    auto end = bytes.find_first_of(delimiters);
    auto token = bytes.substr(0, end);
    // The legacy substring check only accepted prefixes and exact matches after the first code point.
    if (m_profanity_filter.front().empty() || matches(token, false) || matches(token.substr(next_utf8_boundary(token, 0)), true))
      return true;
    if (end == std::string_view::npos)
      return false;
    bytes.remove_prefix(end + 1);
  }
}

std::expected<PatternedNameGenerator::MarkovSource, StarException> PatternedNameGenerator::make_markov_source(
    std::size_t prefix_size, std::size_t end_size, StringList source_names) {
  MarkovSource source{prefix_size, end_size, {}, {}, {}};
  for (auto& original : source_names) {
    auto length = original.length();
    if (!length)
      return std::unexpected(std::move(length.error()));
    if (*length < prefix_size || *length < end_size)
      continue;
    auto name = std::move(original).utf8();
    for (auto& byte : name)
      byte = static_cast<char>(String::to_lower(static_cast<unsigned char>(byte)));
    std::string_view bytes(name);
    std::size_t prefix_begin = 0;
    std::size_t prefix_end = 0;
    std::size_t ending_begin = 0;
    for (std::size_t index = 0;; ++index) {
      if (index == *length - end_size)
        ending_begin = prefix_end;
      if (index == prefix_size)
        source.starts.emplace_back(std::string(bytes.substr(0, prefix_end)));
      if (prefix_end == bytes.size())
        break;
      auto next_end = next_utf8_boundary(bytes, prefix_end);
      if (index >= prefix_size) {
        auto prefix = bytes.substr(prefix_begin, prefix_end - prefix_begin);
        auto chain = source.chains.find(prefix);
        if (chain == source.chains.end())
          chain = source.chains.try_emplace(String(std::string(prefix))).first;
        chain->second.emplace_back(std::string(bytes.substr(prefix_end, next_end - prefix_end)));
        prefix_begin = next_utf8_boundary(bytes, prefix_begin);
      }
      prefix_end = next_end;
    }
    auto ending = bytes.substr(ending_begin);
    if (!source.ends.contains(ending))
      source.ends.add(String(std::string(ending)));
  }
  return source;
}

}
