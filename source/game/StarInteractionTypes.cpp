import star.format;
import std;
import star.string;
import star.string_view;
import star.data_stream;
import star.bi_map;
import star.json;
import star.game_types;
import star.data_stream_extra;
import star.interaction_types;

namespace Star {

namespace {
constexpr auto action_names = std::to_array<std::pair<InteractActionType, std::string_view>>({
    {InteractActionType::None, "None"},
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
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, InteractRequest& request) {
  InteractRequest decoded;
  auto status = ds >> decoded.source_id >> decoded.source_position >> decoded.target_id >> decoded.interact_position;
  if (status)
    request = decoded;
  return status;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, InteractRequest const& request) {
  return ds << request.source_id << request.source_position << request.target_id << request.interact_position;
}

std::expected<InteractActionType, StarException> parse_interact_action_type(StringView name) {
  return enum_value<action_names>(name);
}

bool can_open_from_inventory(InteractActionType type) noexcept {
  switch (type) {
    case InteractActionType::OpenCraftingInterface:
    case InteractActionType::OpenSongbookInterface:
    case InteractActionType::OpenNpcCraftingInterface:
    case InteractActionType::OpenMerchantInterface:
    case InteractActionType::OpenAiInterface:
    case InteractActionType::OpenTeleportDialog:
    case InteractActionType::ShowPopup:
    case InteractActionType::ScriptPane:
      return true;
    default:
      return false;
  }
}

std::expected<InteractAction, StarException> InteractAction::create(StringView type_name, EntityId entity_id, Json data) {
  auto type = parse_interact_action_type(type_name);
  if (!type)
    return std::unexpected(std::move(type.error()));
  return InteractAction{*type, entity_id, std::move(data)};
}

std::expected<InteractAction, StarException> interact_action_from_script(Json const& result, EntityId entity_id) {
  if (result.is_type(Json::Type::String)) {
    auto name = result.string_ptr();
    if (!name)
      return std::unexpected(std::move(name.error()));
    return InteractAction::create(**name, entity_id, {});
  }
  auto first = result.get(0);
  if (!first)
    return std::unexpected(std::move(first.error()));
  auto name = first->string_ptr();
  if (!name)
    return std::unexpected(std::move(name.error()));
  // A missing payload must be diagnosed before looking up even an unknown name.
  auto data = result.get(1);
  if (!data)
    return std::unexpected(std::move(data.error()));
  return InteractAction::create(**name, entity_id, std::move(*data));
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, InteractAction& action) {
  InteractAction decoded;
  auto status = ds >> decoded.type >> decoded.entity_id >> decoded.data;
  if (status)
    action = std::move(decoded);
  return status;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, InteractAction const& action) {
  return ds << action.type << action.entity_id << action.data;
}

}
