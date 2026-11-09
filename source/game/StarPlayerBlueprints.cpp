import star.format;
import std;
import star.json;
import star.byte_array;
import star.set;
import star.data_stream;
import star.bi_map;
import star.item_descriptor;

import star.player_blueprints;

import star.algorithm;
namespace Star {

PlayerBlueprints::PlayerBlueprints() = default;

std::expected<PlayerBlueprints, StarException> PlayerBlueprints::create(Json const& json) {
  PlayerBlueprints blueprints;
  for (auto const& field : {
      std::pair{std::string_view{"knownBlueprints"}, std::ref(blueprints.m_knownBlueprints)},
      std::pair{std::string_view{"newBlueprints"}, std::ref(blueprints.m_newBlueprints)}}) {
    auto entries = json.getArray(field.first);
    if (!entries)
      return std::unexpected(std::move(entries.error()));
    for (auto const& entry : *entries) {
      auto descriptor = ItemDescriptor::create(entry);
      if (!descriptor)
        return std::unexpected(std::move(descriptor.error()));
      field.second.get().add(std::move(*descriptor));
    }
  }
  return blueprints;
}

Json PlayerBlueprints::toJson() const {
  return JsonObject{{"knownBlueprints", transform<JsonArray>(m_knownBlueprints, std::mem_fn(&ItemDescriptor::toJson))},
      {"newBlueprints", transform<JsonArray>(m_newBlueprints, std::mem_fn(&ItemDescriptor::toJson))}};
}

bool PlayerBlueprints::isKnown(ItemDescriptor const& itemDescriptor) const {
  return m_knownBlueprints.contains(itemDescriptor.singular());
}

bool PlayerBlueprints::isNew(ItemDescriptor const& itemDescriptor) const {
  return m_newBlueprints.contains(itemDescriptor.singular());
}

void PlayerBlueprints::add(ItemDescriptor const& itemDescriptor) {
  if (m_knownBlueprints.add(itemDescriptor.singular()))
    m_newBlueprints.add(itemDescriptor.singular());
}

void PlayerBlueprints::markAsRead(ItemDescriptor const& itemDescriptor) {
  m_newBlueprints.remove(itemDescriptor.singular());
}

}
