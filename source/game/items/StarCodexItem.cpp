import std;
import star.asset_path;
import star.assets;
import star.casting;
import star.codex_item;
import star.drawable;
import star.format;
import star.game_types;
import star.item;
import star.json;
import star.list;
import star.player;
import star.player_codexes;
import star.root;
import star.string;
import star.swingable_item;
import star.tool_user_entity;
import star.type_identity;
import star.vector;

namespace Star {

CodexItem::CodexItem(ConstructionKey, Root& root) : Item(root) {}

void CodexItem::query_type(TypeQuery& query) const noexcept {
  query_type_of<CodexItem, Item, SwingableItem>(*this, query);
}

std::string_view CodexItem::dynamic_type_name() const noexcept {
  return Star::type_name<CodexItem>();
}

std::expected<std::shared_ptr<CodexItem>, StarException> CodexItem::create(
    Root& root, Json const& config, String const& directory, Json const& data) {
  auto item = std::make_shared<CodexItem>(ConstructionKey{}, root);
  auto initialized = item->initialize(config, directory, data);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto swingable = item->initializeSwingable(config);
  if (!swingable)
    return std::unexpected(std::move(swingable.error()));
  item->setWindupTime(0.2f);
  item->setCooldownTime(0.5f);
  item->m_requireEdgeTrigger = true;
  auto codex_id = item->instanceValue("codexId").and_then([](Json const& value) { return value.to_string(); });
  if (!codex_id)
    return std::unexpected(std::move(codex_id.error()));
  item->m_codex_id = std::move(*codex_id);
  auto icon_path = item->instanceValue("codexIcon").and_then([](Json const& value) { return value.to_string(); });
  if (!icon_path)
    return std::unexpected(std::move(icon_path.error()));
  auto metadata = root.imageMetadataDatabase();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto icon = Drawable::make_image(**metadata, *icon_path, 1.0f, true, Vec2F());
  if (!icon)
    return std::unexpected(std::move(icon.error()));
  item->m_icon = std::move(*icon);
  item->m_world_drawable = item->m_icon;
  item->m_world_drawable.scale(1.0f / tile_pixels);
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> CodexItem::clone() const {
  return std::make_shared<CodexItem>(*this);
}

std::expected<void, StarException> CodexItem::fire_triggered() {
  auto item_owner = owner();
  if (!item_owner)
    return std::unexpected(std::move(item_owner.error()));
  auto player = as_ref<Player>(item_owner->get());
  if (!player)
    return {};

  auto codexes = player->get().codexes();
  bool const known = codexes->contains(m_codex_id);
  auto assets = root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto message = (known ? (*assets)->json("/codex.config:messages.alreadyKnown")
                        : (*assets)->json("/codex.config:messages.learned"))
      .and_then([](Json const& value) { return value.to_string(); });
  if (!message)
    return std::unexpected(std::move(message.error()));

  if (known) {
    codexes->set_read(m_codex_id, false);
  } else {
    auto learned = codexes->learn(m_codex_id);
    if (!learned)
      return std::unexpected(std::move(learned.error()));
    if (*learned == CodexLearnResult::NotFound)
      return {};
  }
  return player->get().queueUIMessage(*message);
}

std::expected<List<Drawable>, StarException> CodexItem::drawables() const {
  return List<Drawable>(1, m_world_drawable);
}

std::expected<List<Drawable>, StarException> CodexItem::icon_drawables() const {
  return List<Drawable>(1, m_icon);
}

std::expected<List<Drawable>, StarException> CodexItem::drop_drawables(std::optional<std::reference_wrapper<TileDrawer>>) const {
  return drawables();
}

}
