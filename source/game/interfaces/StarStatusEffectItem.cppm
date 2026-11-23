module;
import std;
import star.format;
import star.type_identity;
import star.list;
import star.json;
import star.id_map;
import star.strong_typedef;
import star.data_stream;

import star.status_types;

namespace Star {

class StatusEffectItem {
public:
  virtual void query_type(TypeQuery& query) const noexcept { query_type_of<StatusEffectItem>(*this, query); }
  virtual std::string_view dynamic_type_name() const noexcept { return Star::type_name<StatusEffectItem>(); }

  virtual ~StatusEffectItem() = default;
  [[nodiscard]]
  virtual std::expected<List<PersistentStatusEffect>, StarException> statusEffects() const = 0;
};

}

export module star.status_effect_item;

export namespace Star {
  using ::Star::StatusEffectItem;
}
