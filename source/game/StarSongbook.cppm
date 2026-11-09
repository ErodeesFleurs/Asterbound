module;
import star.format;
import std;
import star.string;
import star.list;
import star.map;
import star.json;

import star.thread;
import star.vector;
import star.game_types;

import star.net_element_basic_fields;
import star.net_element_sync_group;

namespace Star {

class Audio;
class AudioInstance;
class World;
class RenderCallback;
class Root;

class SongbookSynchronization {
private:
  friend class Songbook;
  struct TimeSource {
    std::int64_t keepalive;
    std::int64_t epoch;
  };
  StringMap<std::shared_ptr<TimeSource>> timeSources;
  std::mutex mutex;
};

class Songbook : public NetElementSyncGroup {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<Songbook>, StarException> create(Root& root, String const& species);
  ~Songbook() override;

  [[nodiscard]] std::expected<void, StarException> update(EntityMode mode, World& world);
  [[nodiscard]] std::expected<void, StarException> collectRenderEvents(RenderCallback& renderCallback);

  // instrument needs to tell the songbook what type it is, and needs to keep
  // calling it to signal
  // the instrument is still equiped
  void keepAlive(String const& instrument, Vec2F const& position);

  void stop();
  void play(Json const& song, String const& timeSource);
  bool active() const;
  bool instrumentPlaying() const;

  std::optional<String> timeSource() const;
  std::optional<String> instrument() const;
  Json song() const;

private:
  Songbook(Root& root, String const& species);
  struct Note {
    String instrument;
    String file;
    double timecode;
    double duration;
    double fadeout;
    double velocity;
  };

  struct HeldNote {
    std::shared_ptr<AudioInstance> audio;
    double start;
    double end;
  };

  struct NoteMapping {
    List<String> files;
    double frequency;
    double velocity;
    double fadeout;
  };

  using TimeSource = SongbookSynchronization::TimeSource;

  static double fundamentalFrequency(double p);
  static double fundamentalPitch(double p);

  [[nodiscard]] std::expected<void, StarException> netElementsNeedLoad(bool full) override;
  [[nodiscard]] std::expected<void, StarException> netElementsNeedStore() override;

  Root& m_root;
  SongbookSynchronization& m_synchronization;

  String m_species;
  Vec2F m_position;

  bool m_serverMode;

  std::int64_t m_globalNowDelta;
  int m_activeCooldown;
  bool m_dataUpdated;
  bool m_dataChanged;

  String m_timeSource;
  std::int64_t m_timeSourceEpoch;
  bool m_epochUpdated;
  String m_instrument;
  Json m_song;
  bool m_stopped;

  Deque<Note> m_track;

  List<HeldNote> m_heldNotes;
  std::shared_ptr<TimeSource> m_timeSourceInstance;

  List<std::shared_ptr<AudioInstance>> m_pendingAudio;

  [[nodiscard]] std::expected<List<Note>, StarException> parseABC(String const& abc);

  StringMap<Map<int, NoteMapping>> m_noteMapping;

  [[nodiscard]] std::expected<std::reference_wrapper<NoteMapping>, StarException> noteMapping(String const& instrument, String const& species, int note);

  StringMap<std::shared_ptr<Audio const>> m_uncompressedSamples;

  [[nodiscard]] std::expected<void, StarException> playback();

  NetElementData<Json> m_songNetState;
  NetElementInt m_timeSourceEpochNetState;
  NetElementString m_timeSourceNetState;
  NetElementBool m_activeNetState;
  NetElementString m_instrumentNetState;
};

}

export module star.songbook;

export namespace Star {
  using ::Star::Audio;
  using ::Star::AudioInstance;
  using ::Star::World;
  using ::Star::Songbook;
  using ::Star::SongbookSynchronization;
  using ::Star::RenderCallback;
}
