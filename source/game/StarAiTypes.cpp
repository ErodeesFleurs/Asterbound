import std;
import star.ai_types;
import star.format;
import star.json;
import star.json_extra;
import star.ordered_set;
import star.string;

namespace Star {

std::expected<AiState, StarException> AiState::create(Json const& config) {
  auto read_missions = [&](String const& key) -> std::expected<OrderedHashSet<String>, StarException> {
    auto field = config.find(key);
    if (!field)
      return std::unexpected(std::move(field.error()));
    if (!*field)
      return OrderedHashSet<String>{};
    return json_to_set<OrderedHashSet<String>>((*field)->get(), [](Json const& entry) { return entry.to_string(); });
  };
  auto available = read_missions("availableMissions");
  if (!available)
    return std::unexpected(std::move(available.error()));
  auto completed = read_missions("completedMissions");
  if (!completed)
    return std::unexpected(std::move(completed.error()));
  return AiState{std::move(*available), std::move(*completed)};
}

Json AiState::to_json() const {
  auto encode = [](OrderedHashSet<String> const& missions) {
    JsonArray result;
    result.reserve(missions.size());
    for (auto const& mission : missions)
      result.emplace_back(mission);
    return result;
  };
  return JsonObject{{"availableMissions", encode(available_missions)},
      {"completedMissions", encode(completed_missions)}};
}

}
