import star.string;
import star.chat_action;

namespace Star {

SayChatAction::operator bool() const {
  return !text.empty();
}

PortraitChatAction::operator bool() const {
  return !text.empty();
}

}
