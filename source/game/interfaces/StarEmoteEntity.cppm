module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.id_map;
import star.casting;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;

import star.light_source;

import star.entity;

namespace Star {
enum class HumanoidEmote;
}

namespace Star {

class EmoteEntity : public virtual Entity {
public:
  void const* query_type(TypeKey key) const noexcept override { return query_type_of<EmoteEntity, Entity>(this, key); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<EmoteEntity>(); }

  [[nodiscard]] virtual std::expected<void, StarException> playEmote(HumanoidEmote emote) = 0;
};

}

export module star.emote_entity;

export namespace Star {
  using ::Star::EmoteEntity;
}
