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

RectF InteractiveEntity::interactiveBoundBox() const {
  return meta_bound_box();
}

bool InteractiveEntity::isInteractive() const {
  return true;
}

std::expected<List<QuestArcDescriptor>, StarException> InteractiveEntity::offeredQuests() const {
  return List<QuestArcDescriptor>{};
}

std::expected<StringSet, StarException> InteractiveEntity::turnInQuests() const {
  return StringSet{};
}

std::expected<Vec2F, StarException> InteractiveEntity::questIndicatorPosition() const {
  return position();
}

}
