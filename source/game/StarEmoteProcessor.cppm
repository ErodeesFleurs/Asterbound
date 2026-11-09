module;

import std;
import star.list;
import star.string;
import star.format;

namespace Star {
class Assets;


// Keep the game-heavy Humanoid definition out of interface BMIs.
enum class HumanoidEmote;

class EmoteProcessor {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<EmoteProcessor>, StarException> create(std::shared_ptr<Assets const> const& assets);

  [[nodiscard]] std::expected<HumanoidEmote, StarException> detectEmotes(String const& chatter) const;

private:
  EmoteProcessor() = default;
  struct EmoteBinding {
    EmoteBinding() = default;
    String text;
    HumanoidEmote emote{};
  };
  List<EmoteBinding> m_emoteBindings;
};

}

export module star.emote_processor;

export namespace Star {
  using ::Star::HumanoidEmote;
  using ::Star::EmoteProcessor;
}
