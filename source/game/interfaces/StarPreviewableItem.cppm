module;
import std;
import star.format;
import star.type_identity;
import star.json;
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

class Player;
class TileDrawer;

class PreviewableItem {
public:
  virtual void const* query_type(TypeKey key) const noexcept { return query_type_of<PreviewableItem>(this, key); }
  virtual std::string_view dynamic_type_name() const noexcept { return Star::type_name<PreviewableItem>(); }

  virtual ~PreviewableItem() = default;
  [[nodiscard]]
  virtual std::expected<List<Drawable>, StarException> preview(std::optional<std::reference_wrapper<TileDrawer>> tileDrawer, std::shared_ptr<Player> const& viewer = {}) const = 0;
};

}

export module star.previewable_item;

export namespace Star {
  using ::Star::Player;
  using ::Star::TileDrawer;
  using ::Star::PreviewableItem;
}
