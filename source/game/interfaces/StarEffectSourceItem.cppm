module;
import std;
import star.format;
import star.type_identity;
import star.string;


namespace Star {

class EffectSourceItem {
public:
  virtual void const* query_type(TypeKey key) const noexcept { return query_type_of<EffectSourceItem>(this, key); }
  virtual std::string_view dynamic_type_name() const noexcept { return Star::type_name<EffectSourceItem>(); }

  virtual ~EffectSourceItem() = default;
  [[nodiscard]]
  virtual std::expected<StringSet, StarException> effectSources() const = 0;
};

}

export module star.effect_source_item;

export namespace Star {
  using ::Star::EffectSourceItem;
}
