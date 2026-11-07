module;
import std;
import star.format;
import star.type_identity;
import star.string;


namespace Star {

class EffectSourceItem {
public:
  virtual void const* queryType(TypeKey key) const noexcept {
    return queryTypeOf<EffectSourceItem>(this, key);
  }
  virtual std::string_view dynamicTypeName() const noexcept {
    return Star::typeName<EffectSourceItem>();
  }

  virtual ~EffectSourceItem() = default;
  [[nodiscard]]
  virtual std::expected<StringSet, StarException> effectSources() const = 0;
};

}

export module star.effect_source_item;

export namespace Star {
  using ::Star::EffectSourceItem;
}
