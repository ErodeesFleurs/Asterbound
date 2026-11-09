import star.format;
import star.json_extra;
import std;
import star.json;
import star.ordered_set;
import star.data_stream;
import star.bi_map;
import star.item_descriptor;
import star.game_types;
import star.strong_typedef;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.ai_types;

namespace Star {

AiState::AiState() = default;

std::expected<AiState, StarException> AiState::create(Json const& v) {
  AiState result;
  auto available = v.get("availableMissions", JsonArray());
  if (!available) return std::unexpected(std::move(available.error()));
  auto availableList = jsonToStringList(*available);
  if (!availableList) return std::unexpected(std::move(availableList.error()));
  result.availableMissions.insert_range(*availableList);
  auto completed = v.get("completedMissions", JsonArray());
  if (!completed) return std::unexpected(std::move(completed.error()));
  auto completedList = jsonToStringList(*completed);
  if (!completedList) return std::unexpected(std::move(completedList.error()));
  result.completedMissions.insert_range(*completedList);
  return result;
}

Json AiState::toJson() const {
  JsonArray available;
  available.reserve(availableMissions.size());
  for (auto const& mission : availableMissions)
    available.emplace_back(mission);
  JsonArray completed;
  completed.reserve(completedMissions.size());
  for (auto const& mission : completedMissions)
    completed.emplace_back(mission);
  return JsonObject{{"availableMissions", std::move(available)},
      {"completedMissions", std::move(completed)}};
}

}
