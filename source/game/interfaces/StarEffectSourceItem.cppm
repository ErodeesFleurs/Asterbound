module;
import std;
import star.format;
import star.type_identity;
import star.string;


namespace Star {

class EffectSourceItem {
public:
  virtual void query_type(TypeQuery& query) const noexcept { query_type_of<EffectSourceItem>(*this, query); }
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
