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
  virtual void const* queryType(TypeKey key) const noexcept {
    return queryTypeOf<StatusEffectItem>(this, key);
  }
  virtual std::string_view dynamicTypeName() const noexcept {
    return Star::typeName<StatusEffectItem>();
  }

  virtual ~StatusEffectItem() = default;
  [[nodiscard]]
  virtual std::expected<List<PersistentStatusEffect>, StarException> statusEffects() const = 0;
};

}

export module star.status_effect_item;

export namespace Star {
  using ::Star::StatusEffectItem;
}
