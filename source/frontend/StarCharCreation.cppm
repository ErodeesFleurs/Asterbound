module;
import std;
import star.type_identity;
import star.format;
import star.json;
import star.vector;
import star.casting;
import star.input_event;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.random;
import star.periodic_function;
import star.ordered_map;


import star.animation;
import star.particle;
import star.animated_part_set;
import star.light_source;
import star.networked_animator;
import star.humanoid;


import star.font_texture_group;
import star.anchor_types;
import star.text_painter;
import star.drawable;
import star.asset_texture_group;
import star.drawable_painter;
import star.gui_types;
import star.key_bindings;
import star.mixer;
import star.gui_context;
import star.widget;
import star.pane;

namespace Star {

class Player;

struct CharCreationExceptionTag {
  static constexpr std::string_view name() { return "CharCreationException"; }
};
using CharCreationException = StarError<CharCreationExceptionTag, StarException>;

class CharCreationPane : public Pane {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<CharCreationPane, Pane>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<CharCreationPane>(); }

  // The callback here is either called with null (when the user hits the
  // cancel button) or the newly created player (when the user hits the save
  // button).
  [[nodiscard]] static std::expected<std::shared_ptr<CharCreationPane>, StarException> create(GuiContext& guiContext, std::function<std::expected<void, StarException>(std::shared_ptr<Player>)> requestCloseFunc);

  [[nodiscard]] std::expected<void, StarException> randomize();
  [[nodiscard]] std::expected<void, StarException> randomizeName();

  [[nodiscard]] std::expected<void, StarException> tick(float dt) override;
  [[nodiscard]] std::expected<bool, StarException> sendEvent(InputEvent const& event) override;

  [[nodiscard]] std::expected<std::shared_ptr<Pane>, StarException> createTooltip(Vec2I const&) override;

private:
  explicit CharCreationPane(GuiContext& guiContext);
  [[nodiscard]] std::expected<void, StarException> init(std::function<std::expected<void, StarException>(std::shared_ptr<Player>)> requestCloseFunc);
  [[nodiscard]] std::expected<void, StarException> nameBoxCallback(Widget& object);

  [[nodiscard]] std::expected<void, StarException> changed();

  [[nodiscard]] std::expected<void, StarException> createPlayer();

  std::shared_ptr<Player> m_previewPlayer;

  StringList m_speciesList;

  std::size_t m_speciesChoice;
  std::size_t m_genderChoice;
  std::size_t m_modeChoice;
  std::size_t m_bodyColor;
  std::size_t m_alty;
  std::size_t m_hairChoice;
  std::size_t m_heady;
  std::size_t m_shirtChoice;
  std::size_t m_shirtColor;
  std::size_t m_pantsChoice;
  std::size_t m_pantsColor;
  std::size_t m_personality;
};

}

export module star.char_creation;

export namespace Star {
  using ::Star::Player;
  using ::Star::CharCreationExceptionTag;
  using ::Star::CharCreationException;
  using ::Star::CharCreationPane;
}
