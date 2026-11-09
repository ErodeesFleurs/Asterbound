module;
import std;
import star.format;
import star.json;
import star.thread;
import star.time;
import star.string;
import star.data_stream;
import star.color;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.input_event;
import star.bi_map;
import star.vector;
import star.text;
import star.audio;
import star.map;
import star.set;

import star.drawable;
import star.font_texture_group;
import star.anchor_types;
import star.text_painter;
import star.mixer;

namespace Star {

class Player;
class GuiContext;

class Cinematic {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<Cinematic>, StarException> create(GuiContext& context);

  [[nodiscard]] std::expected<void, StarException> load(Json const& definition);

  void setPlayer(std::shared_ptr<Player> player);

  void update(float dt);
  [[nodiscard]] std::expected<void, StarException> render();

  bool completed() const;
  bool completable() const;

  // this won't synchronize audio, so it should only be used for testing
  void setTime(float timecode);

  void stop();

  [[nodiscard]] std::expected<bool, StarException> handleInputEvent(InputEvent const& event);
  bool suppressInput() const;

  bool muteSfx() const;
  bool muteMusic() const;

private:
  explicit Cinematic(GuiContext& context);
  [[nodiscard]] std::expected<void, StarException> loadDefinition(Json const& definition);

  struct TimeSkip {
    float availableTime;
    float skipToTime;
  };

  struct KeyFrame {
    float timecode;
    std::optional<bool> complete;
    std::optional<float> zoom;
    std::optional<float> alpha;
    std::optional<Vec2F> position;
    std::optional<float> frame;
    std::optional<float> textPercentage;
  };

  struct CameraKeyFrame {
    float timecode;
    float zoom;
    Vec2F pan;
  };

  struct Panel {
    bool useCamera;
    String avatar;
    JsonArray drawables;
    int animationFrames;
    String text;
    TextPositioning textPosition;
    TextStyle textStyle;
    List<KeyFrame> keyFrames;
    float startTime;
    float endTime;
    float loopTime;
  };

  struct PanelValues {
    float zoom;
    float alpha;
    Vec2F position;
    float frame;
    float textPercentage;
    bool completable;
  };

  struct AudioCue {
    AudioCue() {}
    String resource;
    int loops;
    float timecode = 0;
    float endTimecode = 0;
  };

  [[nodiscard]] std::expected<void, StarException> drawDrawable(Drawable const& drawable, float drawableScale, Vec2F const& drawableTranslation);

  void updateCamera(float timecode);

  // since the clock time includes the background fade in/out time, this function gives the adjusted
  // timecode to use for events within the cinematic
  float currentTimecode() const;

  PanelValues determinePanelValues(std::shared_ptr<Panel> panel, float timecode);

  std::reference_wrapper<GuiContext> m_guiContext;

  List<TimeSkip> m_timeSkips;
  std::optional<TimeSkip> m_currentTimeSkip;

  List<CameraKeyFrame> m_cameraKeyFrames;
  List<std::shared_ptr<Panel>> m_panels;
  List<AudioCue> m_audioCues;
  std::vector<std::shared_ptr<AudioInstance>> m_activeAudio;

  // these include the time for background fades so they may not reflect the completion timecode
  Clock m_timer;
  float m_completionTime{};

  std::optional<Vec4B> m_backgroundColor;
  float m_backgroundFadeTime{};

  float m_cameraZoom{1.0f};
  Vec2F m_cameraPan{};

  float m_drawableScale{1.0f};
  Vec2F m_drawableTranslation{};
  Vec2F m_windowSize{};
  RectI m_scissorRect{};

  bool m_scissor{true};
  bool m_letterbox{true};

  std::shared_ptr<Player> m_player;

  Vec2F m_offset{};

  bool m_skippable{true};
  bool m_suppressInput{false};

  bool m_muteSfx{false};
  bool m_muteMusic{false};

  bool m_completable{false};
};

}

export module star.cinematic;

export namespace Star {
  using ::Star::Cinematic;
  using ::Star::Player;
}
