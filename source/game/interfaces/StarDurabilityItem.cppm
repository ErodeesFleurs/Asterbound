module;
import std;
import star.type_identity;
import star.format;


namespace Star {

class DurabilityItem {
public:
  virtual void const* query_type(TypeKey key) const noexcept { return query_type_of<DurabilityItem>(this, key); }
  virtual std::string_view dynamic_type_name() const noexcept { return Star::type_name<DurabilityItem>(); }

  virtual ~DurabilityItem() = default;
  [[nodiscard]]
  virtual std::expected<float, StarException> durabilityStatus() = 0;
};

}

export module star.durability_item;

export namespace Star {
  using ::Star::DurabilityItem;
}
