module;
import std;
import star.type_identity;
import star.format;


namespace Star {

class DurabilityItem {
public:
  virtual void const* queryType(TypeKey key) const noexcept {
    return queryTypeOf<DurabilityItem>(this, key);
  }
  virtual std::string_view dynamicTypeName() const noexcept {
    return Star::typeName<DurabilityItem>();
  }

  virtual ~DurabilityItem() = default;
  [[nodiscard]]
  virtual std::expected<float, StarException> durabilityStatus() = 0;
};

}

export module star.durability_item;

export namespace Star {
  using ::Star::DurabilityItem;
}
