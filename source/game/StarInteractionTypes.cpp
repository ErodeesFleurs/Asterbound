import star.format;
import std;
import star.string;
import star.data_stream;
import star.bi_map;
import star.json;
import star.game_types;
import star.data_stream_extra;
import star.interaction_types;

namespace Star {

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, InteractRequest& ir) {
  if (auto r = ds >> ir.sourceId; !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds >> ir.sourcePosition; !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds >> ir.targetId; !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds >> ir.interactPosition; !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, InteractRequest const& ir) {
  if (auto r = ds << ir.sourceId; !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds << ir.sourcePosition; !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds << ir.targetId; !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds << ir.interactPosition; !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

extern std::expected<EnumMap<InteractActionType>, StarException> const InteractActionTypeNames = EnumMap<InteractActionType>::create({{InteractActionType::None, "None"},
    {InteractActionType::OpenContainer, "OpenContainer"},
    {InteractActionType::SitDown, "SitDown"},
    {InteractActionType::OpenCraftingInterface, "OpenCraftingInterface"},
    {InteractActionType::OpenSongbookInterface, "OpenSongbookInterface"},
    {InteractActionType::OpenNpcCraftingInterface, "OpenNpcCraftingInterface"},
    {InteractActionType::OpenMerchantInterface, "OpenMerchantInterface"},
    {InteractActionType::OpenAiInterface, "OpenAiInterface"},
    {InteractActionType::OpenTeleportDialog, "OpenTeleportDialog"},
    {InteractActionType::ShowPopup, "ShowPopup"},
    {InteractActionType::ScriptPane, "ScriptPane"},
    {InteractActionType::Message, "Message"}});

InteractAction::InteractAction() {
  type = InteractActionType::None;
  entityId = NullEntityId;
}

InteractAction::InteractAction(InteractActionType type, EntityId entityId, Json data)
    : type(type), entityId(entityId), data(std::move(data)) {}

std::expected<InteractAction, StarException> InteractAction::create(String const& typeName, EntityId entityId, Json data) {
  if (!InteractActionTypeNames) return std::unexpected(InteractActionTypeNames.error());
  auto type = InteractActionTypeNames->getLeft(typeName);
  if (!type) return std::unexpected(std::move(type.error()));
  return InteractAction(type->get(), entityId, std::move(data));
}

InteractAction::operator bool() const {
  return type != InteractActionType::None;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, InteractAction& ir) {
  if (auto r = ds.read(ir.type); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(ir.entityId); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(ir.data); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, InteractAction const& ir) {
  if (auto r = ds.write(ir.type); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(ir.entityId); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(ir.data); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

}
