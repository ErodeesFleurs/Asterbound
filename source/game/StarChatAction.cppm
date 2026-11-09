module;
import std;
import star.string;
import star.json;

import star.vector;
import star.game_types;

namespace Star {

struct SayChatAction {
  SayChatAction();
  SayChatAction(EntityId entity, String const& text, Vec2F const& position);
  SayChatAction(EntityId entity, String const& text, Vec2F const& position, Json const& config);

  explicit operator bool() const;

  EntityId entity;
  String text;
  Vec2F position;
  Json config;
};

struct PortraitChatAction {
  PortraitChatAction();
  PortraitChatAction(EntityId entity, String const& portrait, String const& text, Vec2F const& position);
  PortraitChatAction(EntityId entity, String const& portrait, String const& text, Vec2F const& position, Json const& config);

  explicit operator bool() const;

  EntityId entity;
  String portrait;
  String text;
  Vec2F position;
  Json config;
};

using ChatAction = std::variant<std::monostate, SayChatAction, PortraitChatAction>;
}

export module star.chat_action;

export namespace Star {
  using ::Star::SayChatAction;
  using ::Star::PortraitChatAction;
  using ::Star::ChatAction;
}
