module;
import std;
import star.drawable;
import star.format;
import star.item;
import star.json;
import star.list;
import star.string;
import star.swingable_item;
import star.type_identity;

namespace Star {

class Root;
class TileDrawer;

class CodexItem : public Item, public SwingableItem {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  CodexItem(ConstructionKey, Root& root);
  void query_type(TypeQuery& query) const noexcept override;
  std::string_view dynamic_type_name() const noexcept override;

  [[nodiscard]] static std::expected<std::shared_ptr<CodexItem>, StarException> create(
      Root& root, Json const& config, String const& directory, Json const& data);
  [[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> clone() const override;
  [[nodiscard]] std::expected<void, StarException> fire_triggered() override;
  [[nodiscard]] std::expected<List<Drawable>, StarException> drawables() const override;
  [[nodiscard]] std::expected<List<Drawable>, StarException> icon_drawables() const override;
  [[nodiscard]] std::expected<List<Drawable>, StarException> drop_drawables(
      std::optional<std::reference_wrapper<TileDrawer>>) const override;

private:
  String m_codex_id;
  Drawable m_icon;
  Drawable m_world_drawable;
};

}

export module star.codex_item;

export namespace Star {
  using ::Star::CodexItem;
}
