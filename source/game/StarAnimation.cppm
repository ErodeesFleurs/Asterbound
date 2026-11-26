module;
import star.format;
import std;
import star.string;
import star.json;

import star.asset_path;
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
struct Particle;

class Animation {
public:
  // A clock snapshot contains no assets, tags, or other animation payload.
  struct PlaybackState {
    float timer;
    bool completed;
  };

  [[nodiscard]] PlaybackState playback_state() const;
  // Samples an owned display animation without advancing the source animation.
  void sample_playback(PlaybackState const& previous, float elapsed);

  // config can be either a path to a config or a literal config.
  [[nodiscard]] static std::expected<Animation, StarException> create(Root& root, Json config = {}, String const& directory = {});

  void set_angle(float angle);

  void set_processing(DirectivesGroup processing);

  void set_color(Color color);

  void set_tag(String name, String value);
  void clear_tags();

  [[nodiscard]] std::expected<Drawable, StarException> drawable(float pixel_size) const;

  void update(float dt);

  [[nodiscard]] bool is_complete() const;
  void reset();

private:
  friend struct Particle;
  struct Preparation;
  [[nodiscard]] static std::expected<Preparation, StarException> prepare(Root& root, Json config, String const& directory = {});

  explicit Animation(Root& root) : m_root(root) {}
  enum class Mode { Stop, EndAndDisappear, LoopForever };
  inline static constexpr auto mode_names = sorted_enum_names(
    std::to_array<std::pair<Mode, std::string_view>>({{Mode::Stop, "Stop"},
                                                      {Mode::EndAndDisappear, "EndAndDisappear"},
                                                      {Mode::LoopForever, "LoopForever"}}));
  int frame_at(float timer) const;
  float advance_timer(float timer, float elapsed) const;
  void set_frame(int frame);
  void refresh_image();
  std::reference_wrapper<Root> m_root;

  Mode m_mode;
  int m_frame_count;
  int m_variant_offset = 0;
  float m_cycle;
  float m_frame_duration;
  float m_duration;
  float m_angle;
  Vec2F m_offset;
  bool m_centered;
  DirectivesGroup m_processing;
  Color m_color;

  // A plain numbered sheet needs only its base path and the current sub-path.
  // Other paths retain the original template for non-recursive tag expansion.
  String m_frame_template;
  AssetPath m_image;
  StringMap<String> m_tags;
  bool m_append_frame = false;
  bool m_automatic_index = true;
  int m_frame = -1;
  float m_timer = 0;
  bool m_completed = false;
};

struct Animation::Preparation {
  explicit Preparation(Root& root) : animation(root) {}

  Animation animation;
  std::uint64_t max_variant;

  void initialize_variant();
};

}

export module star.animation;

export namespace Star {
  using ::Star::Drawable;
  using ::Star::Animation;
}
