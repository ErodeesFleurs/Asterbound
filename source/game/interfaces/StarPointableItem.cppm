module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.game_types;
import star.string;
import star.data_stream;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;

import star.drawable;

namespace Star {

class PointableItem {
public:
  virtual void const* queryType(TypeKey key) const noexcept {
    return queryTypeOf<PointableItem>(this, key);
  }
  virtual std::string_view dynamicTypeName() const noexcept {
    return Star::typeName<PointableItem>();
  }

  virtual ~PointableItem() = default;

  [[nodiscard]]
  virtual std::expected<float, StarException> getAngleDir(float aimAngle, Direction facingDirection);
  [[nodiscard]]
  virtual std::expected<float, StarException> getAngle(float angle);
  [[nodiscard]]
  virtual std::expected<List<Drawable>, StarException> drawables() const = 0;
};

}

export module star.pointable_item;

export namespace Star {
  using ::Star::PointableItem;
}
