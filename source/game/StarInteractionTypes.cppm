module;
import std;
import star.string_view;
import star.data_stream;
import star.format;
import star.json;
import star.vector;
import star.game_types;

namespace Star {

struct InteractRequest {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream&, InteractRequest&);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream&, InteractRequest const&);
  EntityId source_id = null_entity_id;
  Vec2F source_position;
  EntityId target_id = null_entity_id;
  Vec2F interact_position;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, InteractRequest& request);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, InteractRequest const& request);

// These signed 32-bit values are part of the packet format.
enum class InteractActionType : std::int32_t {
  None = 0,
  OpenContainer = 1,
  SitDown = 2,
  OpenCraftingInterface = 3,
  OpenSongbookInterface = 4,
  OpenNpcCraftingInterface = 5,
  OpenMerchantInterface = 6,
  OpenAiInterface = 7,
  OpenTeleportDialog = 8,
  ShowPopup = 9,
  ScriptPane = 10,
  Message = 11
};
[[nodiscard]] std::expected<InteractActionType, StarException> parse_interact_action_type(StringView name);
[[nodiscard]] bool can_open_from_inventory(InteractActionType type) noexcept;

struct InteractAction {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream&, InteractAction&);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream&, InteractAction const&);
  [[nodiscard]] static std::expected<InteractAction, StarException> create(StringView type_name, EntityId entity_id, Json data);

  explicit operator bool() const noexcept { return type != InteractActionType::None; }

  InteractActionType type = InteractActionType::None;
  EntityId entity_id = null_entity_id;
  Json data;
};

// Null and absent script results have entity-specific fallback semantics.
[[nodiscard]] std::expected<InteractAction, StarException> interact_action_from_script(Json const& result, EntityId entity_id);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, InteractAction& action);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, InteractAction const& action);

}

export module star.interaction_types;

export namespace Star {
  using ::Star::InteractRequest;
  using ::Star::InteractActionType;
  using ::Star::parse_interact_action_type;
  using ::Star::can_open_from_inventory;
  using ::Star::interact_action_from_script;
  using ::Star::InteractAction;
}
