module;
import std;
import star.string;
import star.json;
import star.vector;
import star.game_types;

namespace Star {

struct SayChatAction {
  explicit operator bool() const;

  EntityId entity = null_entity_id;
  String text;
  Vec2F position;
  Json config{};
};

struct PortraitChatAction {
  explicit operator bool() const;

  EntityId entity = null_entity_id;
  String portrait;
  String text;
  Vec2F position;
  Json config{};
};

using ChatAction = std::variant<std::monostate, SayChatAction, PortraitChatAction>;
}

export module star.chat_action;

export namespace Star {
  using ::Star::SayChatAction;
  using ::Star::PortraitChatAction;
  using ::Star::ChatAction;
}
