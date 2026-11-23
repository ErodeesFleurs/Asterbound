module;
import std;
import star.format;
import star.type_identity;
import star.list;
import star.vector;
import star.entity;
import star.chat_action;

namespace Star {

class ChattyEntity : public virtual Entity {
public:
  void query_type(TypeQuery& query) const noexcept override { query_type_of<ChattyEntity, Entity>(*this, query); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ChattyEntity>(); }

  [[nodiscard]] virtual std::expected<Vec2F, StarException> mouth_position() const = 0;
  [[nodiscard]] virtual std::expected<Vec2F, StarException> mouth_position(bool) const = 0;
  virtual List<ChatAction> pull_pending_chat_actions() = 0;
};

}

export module star.chatty_entity;

export namespace Star {
  using ::Star::ChattyEntity;
}
