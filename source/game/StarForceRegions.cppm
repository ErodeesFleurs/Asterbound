module;
import star.format;
import std;
import star.string;
import star.data_stream;
import star.json;

import star.poly;

import star.vector;
import star.line;
import star.rect;
namespace Star {

struct PhysicsCategoryFilter {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PhysicsCategoryFilter& region);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PhysicsCategoryFilter const& region);
  enum Type { Whitelist, Blacklist };

  static PhysicsCategoryFilter whitelist(StringSet categories);
  static PhysicsCategoryFilter blacklist(StringSet categories);

  PhysicsCategoryFilter();
  PhysicsCategoryFilter(Type type, StringSet categories = {});

  bool check(StringSet const& categories) const;

  bool operator==(PhysicsCategoryFilter const& rhs) const;

  Type type;
  StringSet categories;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PhysicsCategoryFilter& rfr);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PhysicsCategoryFilter const& rfr);

[[nodiscard]] std::expected<PhysicsCategoryFilter, StarException> jsonToPhysicsCategoryFilter(Json const& json);

struct DirectionalForceRegion {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DirectionalForceRegion& region);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DirectionalForceRegion const& region);
  [[nodiscard]] static std::expected<DirectionalForceRegion, StarException> fromJson(Json const& json);

  RectF boundBox() const;

  void translate(Vec2F const& pos);

  bool operator==(DirectionalForceRegion const& rhs) const;

  PolyF region;
  std::optional<float> xTargetVelocity;
  std::optional<float> yTargetVelocity;
  float controlForce;
  PhysicsCategoryFilter categoryFilter;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DirectionalForceRegion& rfr);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DirectionalForceRegion const& rfr);

struct RadialForceRegion {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RadialForceRegion& region);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RadialForceRegion const& region);
  [[nodiscard]] static std::expected<RadialForceRegion, StarException> fromJson(Json const& json);

  RectF boundBox() const;

  void translate(Vec2F const& pos);

  bool operator==(RadialForceRegion const& rhs) const;

  Vec2F center;
  float outerRadius;
  float innerRadius;
  float targetRadialVelocity;
  float controlForce;
  PhysicsCategoryFilter categoryFilter;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RadialForceRegion& rfr);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RadialForceRegion const& rfr);

struct GradientForceRegion {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, GradientForceRegion& region);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, GradientForceRegion const& region);
  [[nodiscard]] static std::expected<GradientForceRegion, StarException> fromJson(Json const& json);

  RectF boundBox() const;

  void translate(Vec2F const& pos);

  bool operator==(GradientForceRegion const& rhs) const;

  PolyF region;
  Line2F gradient;
  float baseTargetVelocity;
  float baseControlForce;
  PhysicsCategoryFilter categoryFilter;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, GradientForceRegion& rfr);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, GradientForceRegion const& rfr);

using PhysicsForceRegion = std::variant<DirectionalForceRegion, RadialForceRegion, GradientForceRegion>;

[[nodiscard]] std::expected<PhysicsForceRegion, StarException> jsonToPhysicsForceRegion(Json const& json);

}

export module star.force_regions;

export namespace Star {
  using ::Star::PhysicsCategoryFilter;
  using ::Star::DirectionalForceRegion;
  using ::Star::RadialForceRegion;
  using ::Star::GradientForceRegion;
  using ::Star::PhysicsForceRegion;
  using ::Star::jsonToPhysicsCategoryFilter;
  using ::Star::jsonToPhysicsForceRegion;
}
