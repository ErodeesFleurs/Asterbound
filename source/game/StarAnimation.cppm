module;
import star.format;
import std;
import star.string;
import star.json;

import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;

import star.vector;
namespace Star {

struct Drawable;
class Root;

class Animation {
public:
  // A clock snapshot contains no assets, tags, or other animation payload.
  struct PlaybackState {
    float timer;
    bool completed;
  };

  PlaybackState playbackState() const;
  // Samples an owned display animation without advancing the source animation.
  void samplePlayback(PlaybackState const& previous, float elapsed);

  // config can be either a path to a config or a literal config.
  [[nodiscard]] static std::expected<Animation, StarException> create(Root& root, Json config = {}, String const& directory = {});

  void setAngle(float angle);

  void setProcessing(DirectivesGroup processing);

  void setColor(Color color);

  void setTag(String tagName, String tagValue);
  void clearTags();

  [[nodiscard]] std::expected<Drawable, StarException> drawable(float pixelSize) const;

  void update(float dt);

  bool isComplete() const;
  void reset();

private:
  explicit Animation(Root& root) : m_root(root) {}
  [[nodiscard]] std::expected<void, StarException> initialize(Json config, String const& directory);
  enum AnimationMode { Stop, EndAndDisappear, LoopForever };
  inline static constexpr auto AnimationModeNames = sortedEnumNames(
    std::to_array<std::pair<AnimationMode, std::string_view>>({{AnimationMode::Stop, "Stop"},
                                                               {AnimationMode::EndAndDisappear, "EndAndDisappear"},
                                                               {AnimationMode::LoopForever, "LoopForever"}}));
  std::reference_wrapper<Root> m_root;

  AnimationMode m_mode;
  String m_directory;
  String m_base;
  bool m_appendFrame;
  int m_frameNumber;
  float m_animationCycle;
  float m_animationTime;
  float m_angle;
  Vec2F m_offset;
  bool m_centered;
  DirectivesGroup m_processing;
  Color m_color;
  int m_variantOffset;

  StringMap<String> m_tagValues;
  int m_frame;
  float m_animationTimer;
  float m_timeToLive;
  bool m_completed;
};

}

export module star.animation;

export namespace Star {
  using ::Star::Drawable;
  using ::Star::Animation;
}
