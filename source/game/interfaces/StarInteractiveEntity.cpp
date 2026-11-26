import std;
import star.format;
import star.string;
import star.list;
import star.json;
import star.id_map;
import star.game_types;
import star.casting;
import star.vector;
import star.data_stream;
import star.strong_typedef;
import star.bi_map;
import star.interaction_types;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;

import star.rect;
namespace Star {

RectF InteractiveEntity::interactive_bound_box() const {
  return meta_bound_box();
}

bool InteractiveEntity::is_interactive() const {
  return true;
}

std::expected<List<QuestArcDescriptor>, StarException> InteractiveEntity::offered_quests() const {
  return List<QuestArcDescriptor>{};
}

std::expected<StringSet, StarException> InteractiveEntity::turn_in_quests() const {
  return StringSet{};
}

std::expected<Vec2F, StarException> InteractiveEntity::quest_indicator_position() const {
  return position();
}

}
