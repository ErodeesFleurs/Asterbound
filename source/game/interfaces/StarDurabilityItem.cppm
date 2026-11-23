module;
import std;
import star.type_identity;
import star.format;


namespace Star {

class DurabilityItem {
public:
  virtual void query_type(TypeQuery& query) const noexcept { query_type_of<DurabilityItem>(*this, query); }
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
