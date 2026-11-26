module;
import std;
import star.string;
import star.format;
import star.json;
import star.random;

namespace Star {
class Root;

struct NameGeneratorExceptionTag {
  static constexpr std::string_view name() { return "NameGeneratorException"; }
};
using NameGeneratorException = StarError<NameGeneratorExceptionTag, StarException>;

class PatternedNameGenerator {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  PatternedNameGenerator(ConstructionKey, Root& root);
  [[nodiscard]] static std::expected<std::shared_ptr<PatternedNameGenerator>, StarException> create(Root& root);

  [[nodiscard]] std::expected<String, StarException> generate_name(String const& rules_asset) const;
  [[nodiscard]] std::expected<String, StarException> generate_name(String const& rules_asset, std::uint64_t seed) const;
  [[nodiscard]] std::expected<String, StarException> generate_name(String const& rules_asset, RandomSource& random) const;

private:
  struct MarkovSource {
    std::size_t prefix_size;
    std::size_t end_size;
    StringList starts;
    StringMap<StringList> chains;
    StringSet ends;
  };

  [[nodiscard]] std::expected<String, StarException> process_rule(JsonArray const& rule, RandomSource& random) const;

  [[nodiscard]] std::expected<bool, StarException> is_profane(String const& name) const;

  [[nodiscard]] static std::expected<MarkovSource, StarException> make_markov_source(std::size_t prefix_size, std::size_t end_size, StringList source_names);

  Root& m_root;
  StringMap<MarkovSource> m_markov_sources;
  std::vector<std::string> m_profanity_filter;
};

}

export module star.name_generator;

export namespace Star {
  using ::Star::NameGeneratorExceptionTag;
  using ::Star::NameGeneratorException;
  using ::Star::PatternedNameGenerator;
}
