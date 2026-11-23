module;
import std;
import star.type_identity;
import star.format;
import star.string;
import star.list;
import star.audio;

namespace Star {
class UniverseClient;
class AiDatabase;
class LabelWidget;
class ImageWidget;
class ImageStretchWidget;
class CanvasWidget;
class ListWidget;
class ButtonWidget;
class StackWidget;
class Companion;
}

import star.ai_types;
import star.gui_context;
import star.pane;
import star.animation;

namespace Star {


struct AiInterfaceExceptionTag {
  static constexpr std::string_view name() { return "AiInterfaceException"; }
};
using AiInterfaceException = StarError<AiInterfaceExceptionTag, StarException>;

class AiInterface : public Pane {
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  void query_type(TypeQuery& query) const noexcept override { query_type_of<AiInterface, Pane>(*this, query); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<AiInterface>(); }

  AiInterface(ConstructionKey, GuiContext& gui_context, std::shared_ptr<UniverseClient> client,
      std::shared_ptr<AiDatabase const> database, Animation static_animation, Animation scanline_animation, Animation face_animation);
  [[nodiscard]] static std::expected<std::shared_ptr<AiInterface>, StarException> create(GuiContext& gui_context, std::shared_ptr<UniverseClient> client);

  [[nodiscard]] std::expected<void, StarException> update(float dt) override;

  [[nodiscard]] std::expected<void, StarException> displayed() override;
  [[nodiscard]] std::expected<void, StarException> dismissed() override;

private:
  [[nodiscard]] std::expected<void, StarException> initialize();

  enum class AiPages : std::uint8_t {
    StatusPage,
    MissionList,
    MissionPage,
    CrewList,
    CrewPage
  };

  enum class ListAction { Open, Refresh };

  struct MissionRows {
    StringList available;
    StringList completed;
  };

  struct SpeechPlayback {
    std::shared_ptr<LabelWidget> label;
    String animation;
    float speed_modifier;
    unsigned length;
    float progress = 0;
  };

  [[nodiscard]] std::expected<void, StarException> update_breadcrumbs();
  [[nodiscard]] std::expected<void, StarException> show_status();

  [[nodiscard]] std::expected<void, StarException> populate_missions(AiState const& state);
  [[nodiscard]] std::expected<void, StarException> show_missions(ListAction action);
  [[nodiscard]] std::expected<void, StarException> select_mission();
  [[nodiscard]] std::expected<void, StarException> start_mission();

  [[nodiscard]] std::expected<void, StarException> populate_crew(List<std::shared_ptr<Companion>> const& crew);
  [[nodiscard]] std::expected<void, StarException> show_crew(ListAction action);
  [[nodiscard]] std::expected<void, StarException> select_recruit();
  [[nodiscard]] std::expected<void, StarException> dismiss_recruit();

  [[nodiscard]] std::expected<void, StarException> go_back();

  [[nodiscard]] std::expected<void, StarException> set_face_animation(String const& name);
  [[nodiscard]] std::expected<void, StarException> set_current_speech(String const& text_widget, AiSpeech speech);

  AiPages m_current_page;

  std::shared_ptr<UniverseClient> m_client;
  std::shared_ptr<AiDatabase const> m_ai_database;

  Animation m_static_animation;
  Animation m_scanline_animation;
  std::pair<String, Animation> m_face_animation;

  std::shared_ptr<AudioInstance> m_chatter_sound;

  std::shared_ptr<StackWidget> m_main_stack;
  std::shared_ptr<StackWidget> m_mission_stack;
  std::shared_ptr<StackWidget> m_crew_stack;

  std::shared_ptr<ButtonWidget> m_show_missions_button;
  std::shared_ptr<ButtonWidget> m_show_crew_button;
  std::shared_ptr<ButtonWidget> m_back_button;

  int m_breadcrumb_left_padding;
  int m_breadcrumb_right_padding;
  std::shared_ptr<ImageStretchWidget> m_home_breadcrumb_background;
  std::shared_ptr<ImageStretchWidget> m_page_breadcrumb_background;
  std::shared_ptr<ImageStretchWidget> m_item_breadcrumb_background;
  std::shared_ptr<LabelWidget> m_home_breadcrumb_widget;
  std::shared_ptr<LabelWidget> m_page_breadcrumb_widget;
  std::shared_ptr<LabelWidget> m_item_breadcrumb_widget;

  std::shared_ptr<CanvasWidget> m_ai_face_canvas_widget;

  std::shared_ptr<ListWidget> m_mission_list_widget;
  std::shared_ptr<LabelWidget> m_mission_name_label;
  std::shared_ptr<ImageWidget> m_mission_icon;

  std::shared_ptr<ListWidget> m_crew_list_widget;
  std::shared_ptr<LabelWidget> m_recruit_name_label;
  std::shared_ptr<ImageWidget> m_recruit_icon;

  String m_species;

  String m_mission_breadcrumb_text;
  String m_mission_deploy_text;
  String m_crew_breadcrumb_text;
  String m_default_recruit_name;
  String m_default_recruit_description;

  std::optional<MissionRows> m_mission_rows;
  std::optional<String> m_selected_mission;

  std::optional<List<std::shared_ptr<Companion>>> m_crew_rows;
  std::shared_ptr<Companion> m_selected_recruit;

  std::optional<SpeechPlayback> m_speech;
};

}

export module star.ai_interface;

export namespace Star {
  using ::Star::UniverseClient;
  using ::Star::AiDatabase;
  using ::Star::LabelWidget;
  using ::Star::ImageWidget;
  using ::Star::ImageStretchWidget;
  using ::Star::CanvasWidget;
  using ::Star::ListWidget;
  using ::Star::ButtonWidget;
  using ::Star::StackWidget;
  using ::Star::Companion;
  using ::Star::AiInterface;
  using ::Star::AiInterfaceExceptionTag;
  using ::Star::AiInterfaceException;
}
