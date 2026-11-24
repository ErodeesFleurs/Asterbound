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

  PhysicsCategoryFilter() = default;
  PhysicsCategoryFilter(Type type, StringSet categories = {});

  bool check(StringSet const& categories) const;

  bool operator==(PhysicsCategoryFilter const&) const = default;

  Type type = Blacklist;
  StringSet categories;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PhysicsCategoryFilter& rfr);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PhysicsCategoryFilter const& rfr);

[[nodiscard]] std::expected<PhysicsCategoryFilter, StarException> json_to_physics_category_filter(Json const& json);

struct DirectionalForceRegion {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DirectionalForceRegion& region);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DirectionalForceRegion const& region);
  [[nodiscard]] static std::expected<DirectionalForceRegion, StarException> from_json(Json const& json);

  RectF bounding_box() const;

  void translate(Vec2F const& pos);

  bool operator==(DirectionalForceRegion const&) const = default;

  PolyF region;
  std::optional<float> x_target_velocity;
  std::optional<float> y_target_velocity;
  float control_force = 0;
  PhysicsCategoryFilter category_filter;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DirectionalForceRegion& rfr);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DirectionalForceRegion const& rfr);

struct RadialForceRegion {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RadialForceRegion& region);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RadialForceRegion const& region);
  [[nodiscard]] static std::expected<RadialForceRegion, StarException> from_json(Json const& json);

  RectF bounding_box() const;

  void translate(Vec2F const& pos);

  bool operator==(RadialForceRegion const&) const = default;

  Vec2F center;
  float outer_radius = 0;
  float inner_radius = 0;
  float target_radial_velocity = 0;
  float control_force = 0;
  PhysicsCategoryFilter category_filter;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RadialForceRegion& rfr);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RadialForceRegion const& rfr);

struct GradientForceRegion {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, GradientForceRegion& region);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, GradientForceRegion const& region);
  [[nodiscard]] static std::expected<GradientForceRegion, StarException> from_json(Json const& json);

  RectF bounding_box() const;

  void translate(Vec2F const& pos);

  bool operator==(GradientForceRegion const&) const = default;

  PolyF region;
  Line2F gradient;
  float base_target_velocity = 0;
  float base_control_force = 0;
  PhysicsCategoryFilter category_filter;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, GradientForceRegion& rfr);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, GradientForceRegion const& rfr);

template <typename T>
concept physics_force_region = std::same_as<std::remove_cvref_t<T>, DirectionalForceRegion>
    || std::same_as<std::remove_cvref_t<T>, RadialForceRegion>
    || std::same_as<std::remove_cvref_t<T>, GradientForceRegion>;

using PhysicsForceRegion = std::variant<DirectionalForceRegion, RadialForceRegion, GradientForceRegion>;

[[nodiscard]] std::expected<PhysicsForceRegion, StarException> json_to_physics_force_region(Json const& json);

}

export module star.force_regions;

export namespace Star {
  using ::Star::PhysicsCategoryFilter;
  using ::Star::DirectionalForceRegion;
  using ::Star::RadialForceRegion;
  using ::Star::GradientForceRegion;
  using ::Star::PhysicsForceRegion;
  using ::Star::physics_force_region;
  using ::Star::json_to_physics_category_filter;
  using ::Star::json_to_physics_force_region;
}
