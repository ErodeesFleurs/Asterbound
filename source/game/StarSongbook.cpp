import star.output_proxy;
import star.json;
import star.id_map;
import star.thread;
import star.game_types;
import star.lexical_cast;
import star.random;
import star.casting;
import star.strong_typedef;
import star.rpc_promise;
import star.logging;
import star.audio;
import star.list;
import star.map;
import star.set;
import star.vector;
import star.bi_map;
import star.color;
import star.string;
import star.data_stream;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.time;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.listener;
import std;
import star.format;
import star.version;

import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;

import star.light_source;
import star.entity_rendering;

import star.songbook;

import star.matrix3;
import star.math_common;
import star.algorithm;
namespace Star {

Songbook::Songbook(Root& root, String const& species)
  : m_root(root), m_synchronization(root.songbookSynchronization()) {
  m_activeCooldown = 0;
  m_dataUpdated = false;
  m_dataChanged = false;
  m_timeSourceEpoch = 0;
  m_epochUpdated = false;
  m_globalNowDelta = 0;
  m_species = species;
  m_stopped = true;
  m_serverMode = true;

}

std::expected<std::shared_ptr<Songbook>, StarException> Songbook::create(Root& root, String const& species) {
  auto songbook = std::shared_ptr<Songbook>(new Songbook(root, species));
  for (auto element : std::initializer_list<std::reference_wrapper<NetElement>>{
      songbook->m_songNetState, songbook->m_timeSourceEpochNetState,
      songbook->m_timeSourceNetState, songbook->m_activeNetState,
      songbook->m_instrumentNetState}) {
    auto added = songbook->addNetElement(element);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  return songbook;
}

Songbook::~Songbook() {
  stop();
}

std::expected<std::reference_wrapper<Songbook::NoteMapping>, StarException> Songbook::noteMapping(String const& instrument, String const& species, int note) {
  if (!m_noteMapping.contains(instrument)) {
    Map<int, NoteMapping> notemap;
    auto assets = m_root.assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto tuning = (*assets)->json(strf("/sfx/instruments/{}/tuning.config", instrument));
    if (!tuning)
      return std::unexpected(std::move(tuning.error()));
    auto mapping = tuning->getObject("mapping");
    if (!mapping)
      return std::unexpected(std::move(mapping.error()));
    for (auto const& entry : *mapping) {
      auto keyNumber = lexicalCast<int>(entry.first);
      if (!keyNumber)
        return std::unexpected(std::move(keyNumber.error()));
      NoteMapping nm;
      auto addFile = [&](String file) -> std::expected<void, StarException> {
        auto instrumentFile = file.replace("$instrument$", instrument);
        if (!instrumentFile)
          return std::unexpected(std::move(instrumentFile.error()));
        auto speciesFile = instrumentFile->replace("$species$", species);
        if (!speciesFile)
          return std::unexpected(std::move(speciesFile.error()));
        nm.files.append(std::move(*speciesFile));
        return {};
      };
      auto hasFile = entry.second.contains("file");
      if (!hasFile)
        return std::unexpected(std::move(hasFile.error()));
      if (*hasFile) {
        auto file = entry.second.getString("file", "");
        if (!file)
          return std::unexpected(std::move(file.error()));
        auto added = addFile(std::move(*file));
        if (!added)
          return std::unexpected(std::move(added.error()));
      } else {
        auto hasFiles = entry.second.contains("files");
        if (!hasFiles)
          return std::unexpected(std::move(hasFiles.error()));
        if (*hasFiles) {
          auto files = entry.second.getArray("files");
          if (!files)
            return std::unexpected(std::move(files.error()));
          for (auto const& fileJson : *files) {
            auto file = fileJson.toString();
            if (!file)
              return std::unexpected(std::move(file.error()));
            auto added = addFile(std::move(*file));
            if (!added)
              return std::unexpected(std::move(added.error()));
          }
        }
      }
      auto frequency = entry.second.getDouble("f");
      if (!frequency)
        return std::unexpected(std::move(frequency.error()));
      nm.frequency = *frequency;
      nm.velocity = 1;
      auto fallbackFadeout = tuning->getDouble("fadeout");
      if (!fallbackFadeout)
        return std::unexpected(std::move(fallbackFadeout.error()));
      auto fadeout = entry.second.getDouble("fadeOut", *fallbackFadeout);
      if (!fadeout)
        return std::unexpected(std::move(fadeout.error()));
      nm.fadeout = *fadeout;
      notemap[*keyNumber] = std::move(nm);
    }
    for (int key = 21; key <= 108; ++key) {
      NoteMapping& nm = notemap[key];
      if (nm.files.empty()) {
        auto prev = notemap[key - 1];
        nm.files = prev.files;
        nm.velocity = prev.velocity * nm.frequency / prev.frequency;
      }
    }
    m_noteMapping[instrument] = std::move(notemap);
  }
  return std::ref(m_noteMapping[instrument][note]);
}

std::expected<void, StarException> Songbook::update(EntityMode mode, World& world) {
  m_serverMode = world.isServer();

  if (m_serverMode)
    return {};

  m_globalNowDelta = world.epochTime() * 1000 - Time::millisecondsSinceEpoch();
  if (m_epochUpdated) {
    m_epochUpdated = false;
    m_timeSourceEpoch -= m_globalNowDelta;
  }
  if (m_dataUpdated) {
    m_dataUpdated = false;
    if (!m_song.isNull()) {
      auto parsed = [&]() -> std::expected<List<Note>, StarException> {
        {
          std::lock_guard lock(m_synchronization.mutex);
          if (!m_synchronization.timeSources.contains(m_timeSource)) {
            m_timeSourceInstance = std::make_shared<TimeSource>();
            m_synchronization.timeSources[m_timeSource] = m_timeSourceInstance;
            m_timeSourceInstance->epoch = m_timeSourceEpoch;
            m_timeSourceInstance->keepalive = m_timeSourceEpoch;
          } else
            m_timeSourceInstance = m_synchronization.timeSources[m_timeSource];
        }
        m_track.clear();
        m_stopped = false;
        auto abc = m_song.getString("abc");
        if (!abc)
          return std::unexpected(std::move(abc.error()));
        return parseABC(*abc);
      }();
      if (!parsed) {
        m_stopped = true;
        auto logged = m_root.logger().errorException("Failed to handle abc", ExceptionDiagnostic(parsed.error()));
        if (!logged)
          return std::unexpected(std::move(logged.error()));
      } else {
        m_track.appendAll(std::move(*parsed));
      }
    }
  }
  if (mode == EntityMode::Master) {
    if (active())
      m_activeCooldown--;
  }
  return playback();
}

std::expected<void, StarException> Songbook::playback() {
  if (!active() || (m_track.empty() && m_heldNotes.empty())) {
    stop();
    return {};
  }
  m_timeSourceInstance->keepalive = Time::millisecondsSinceEpoch();
  auto now = (Time::millisecondsSinceEpoch() - m_timeSourceInstance->epoch) / 1000.0;
  if (!m_heldNotes.empty()) {
    for (auto& note : m_heldNotes)
      note.audio->setPosition(m_position);
    m_heldNotes.filter([](HeldNote const& note) { return !note.audio->finished(); });
  }

  while (!m_track.empty() && (m_track.begin()->timecode <= (now + 0.5))) {
    auto taken = m_track.takeFirst();
    if (!taken)
      return std::unexpected(std::move(taken.error()));
    auto note = std::move(*taken);
    auto delta = now - note.timecode;
    if (delta > 1)
      continue; // skip notes that are more than a second behind
    if (!m_uncompressedSamples.contains(note.file)) {
      auto assets = m_root.assets();
      if (!assets)
        return std::unexpected(std::move(assets.error()));
      auto sample = (*assets)->audio(note.file);
      if (!sample)
        return std::unexpected(std::move(sample.error()));
      if ((*sample)->compressed()) {
        auto cloned = (*sample)->clone();
        if (!cloned)
          return std::unexpected(std::move(cloned.error()));
        auto copy = std::make_shared<Audio>(std::move(*cloned));
        auto uncompressed = copy->uncompress();
        if (!uncompressed)
          return std::unexpected(std::move(uncompressed.error()));
        m_uncompressedSamples[note.file] = std::move(copy);
      } else {
        m_uncompressedSamples[note.file] = std::move(*sample);
      }
    }
    auto created = AudioInstance::create(*m_uncompressedSamples[note.file]);
    if (!created)
      return std::unexpected(std::move(created.error()));
    auto audioInstance = std::move(*created);
    audioInstance->setMixerGroup(MixerGroup::Instruments);
    audioInstance->setPitchMultiplier(note.velocity);

    auto start = m_timeSourceInstance->epoch + static_cast<std::int64_t>(note.timecode * 1000.0);
    audioInstance->setClockStart(start);
    audioInstance->setClockStop(start + static_cast<std::int64_t>(note.duration * 1000.0),
                                static_cast<std::int64_t>(note.fadeout * 1000.0));

    audioInstance->setPosition(m_position);

    m_pendingAudio.append(audioInstance);
    m_heldNotes.append(HeldNote{audioInstance, note.timecode, note.duration + note.timecode});
  }
  return {};
}

std::expected<void, StarException> Songbook::collectRenderEvents(RenderCallback& renderCallback) {
  for (auto& audio : m_pendingAudio) {
    auto added = renderCallback.addAudio(audio);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  m_pendingAudio.clear();
  return {};
}

void Songbook::keepAlive(String const& instrument, Vec2F const& position) {
  if (instrument != m_instrument) {
    m_instrument = instrument;
    m_dataUpdated = true;
  }
  m_position = position;
  if (active())
    m_activeCooldown = 3;
}

std::expected<List<Songbook::Note>, StarException> Songbook::parseABC(String const& abc) {
  List<Songbook::Note> result;

  StringMap<String> fields;

  auto fraction = [](String const& text) -> std::expected<Vec2I, StarException> {
    auto parts = text.split("/", 1);
    if (!parts)
      return std::unexpected(std::move(parts.error()));
    if (parts->size() != 2)
      return std::unexpected(StarException("Invalid ABC fraction"));
    auto numerator = lexicalCast<int>((*parts)[0]);
    if (!numerator)
      return std::unexpected(std::move(numerator.error()));
    auto denominator = lexicalCast<int>((*parts)[1]);
    if (!denominator)
      return std::unexpected(std::move(denominator.error()));
    if (*denominator == 0)
      return std::unexpected(StarException("ABC fraction has a zero denominator"));
    return Vec2I{*numerator, *denominator};
  };
  auto meter = [&]() -> std::expected<Vec2I, StarException> {
    auto text = fields.value("M", "C");
    auto common = text.equalsIgnoreCase("C");
    if (!common)
      return std::unexpected(std::move(common.error()));
    if (*common)
      text = "4/4";
    auto cut = text.equalsIgnoreCase("C|");
    if (!cut)
      return std::unexpected(std::move(cut.error()));
    if (*cut)
      text = "2/2";
    return fraction(text);
  };
  auto noteLength = [&]() -> std::expected<Vec2I, StarException> {
    auto text = fields.value("L", "C");
    auto common = text.equalsIgnoreCase("C");
    if (!common)
      return std::unexpected(std::move(common.error()));
    if (*common) {
      auto value = meter();
      if (!value)
        return std::unexpected(std::move(value.error()));
      return Vec2I{1, (*value)[1]};
    }
    return fraction(text);
  };
  auto secondsPerBeat = [&]() -> std::expected<double, StarException> {
    auto parts = fields.value("Q", "120").split("=", 1);
    if (!parts)
      return std::unexpected(std::move(parts.error()));
    auto last = parts->last();
    if (!last)
      return std::unexpected(std::move(last.error()));
    auto tempo = lexicalCast<double>(last->get());
    if (!tempo)
      return std::unexpected(std::move(tempo.error()));
    if (*tempo == 0 || !std::isfinite(*tempo))
      return std::unexpected(StarException("ABC tempo must be finite and nonzero"));
    double seconds = 60.0 / *tempo;
    auto duration = parts->size() > 1 ? fraction((*parts)[0]) : noteLength();
    if (!duration)
      return std::unexpected(std::move(duration.error()));
    if ((*duration)[0] == 0)
      return std::unexpected(StarException("ABC tempo has a zero note length"));
    return (*duration)[1] * seconds / (*duration)[0];
  };
  auto transpose = [&]() -> std::expected<int, StarException> {
    return lexicalCast<int>(fields.value("Transpose", "0"));
  };
  auto fetchKeySignatureMapping = [&]() -> std::expected<List<int>, StarException> {
    auto cleanupKey = [](String const& key) -> std::expected<String, StarException> {
      auto result = key.toLower();
      if (!result)
        return std::unexpected(std::move(result.error()));
      for (auto const& replacement : std::initializer_list<std::pair<String, String>>{
          {" ", ""}, {"minor", "m"},
          {"min", "m"}, {"major", "maj"}}) {
        result = result->replace(replacement.first, replacement.second);
        if (!result)
          return std::unexpected(std::move(result.error()));
      }
      return std::move(*result);
    };
    auto key = cleanupKey(fields.value("K", "c"));
    if (!key)
      return std::unexpected(std::move(key.error()));
    auto assets = m_root.assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto keys = (*assets)->json("/songbook.config:keys");
    if (!keys)
      return std::unexpected(std::move(keys.error()));
    while (true) {
      auto contains = keys->contains(*key);
      if (!contains)
        return std::unexpected(std::move(contains.error()));
      if (!*contains) {
        auto logged = m_root.logger().info("Failed to find key {}, falling back to C", *key);
        if (!logged)
          return std::unexpected(std::move(logged.error()));
        key = String("c");
      }
      auto signature = keys->get(*key);
      if (!signature)
        return std::unexpected(std::move(signature.error()));
      if (signature->isType(Json::Type::String)) {
        auto text = signature->toString();
        if (!text)
          return std::unexpected(std::move(text.error()));
        key = cleanupKey(*text);
        if (!key)
          return std::unexpected(std::move(key.error()));
        continue;
      }
      auto entries = signature->iterateArray();
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      List<int> mapping;
      for (auto const& entry : *entries) {
        auto value = entry.toInt();
        if (!value)
          return std::unexpected(std::move(value.error()));
        mapping.append(*value);
      }
      if (mapping.size() < 7)
        return std::unexpected(StarException("ABC key signature needs seven notes"));
      return mapping;
    }
  };

  double now = 0;
  int accidentals = 0;
  bool accidentalSpecified = false;
  double lastBarNow = now;

  Map<int, int> impliedAccidentals;
  bool grouped = false;
  double groupDuration = 0;
  bool dirtyFlags = true;

  List<int> keySignatureMapping;
  List<int> tupleMapping;
  double fullNoteDuration = 0;
  double noteDuration = 0;
  double barDuration = 0;
  Map<int, std::size_t> pendingTies;
  int groupStartIndex = 0;

  int tupleCount = 0;
  double tupleDurationFactor = 0;
  bool staccato = false;

  auto normalized = abc.replace("\t", " ");
  if (!normalized)
    return std::unexpected(std::move(normalized.error()));
  auto lines = normalized->splitLines();
  if (!lines)
    return std::unexpected(std::move(lines.error()));
  auto setField = [&](std::basic_string_view<String::Char> text) -> std::expected<void, StarException> {
    auto encoded = String::create(text);
    if (!encoded)
      return std::unexpected(std::move(encoded.error()));
    auto parts = encoded->split(":", 1);
    if (!parts)
      return std::unexpected(std::move(parts.error()));
    if (parts->size() > 1) {
      auto key = (*parts)[0].trim();
      if (!key)
        return std::unexpected(std::move(key.error()));
      auto value = (*parts)[1].trim();
      if (!value)
        return std::unexpected(std::move(value.error()));
      fields[std::move(*key)] = std::move(*value);
      dirtyFlags = true;
    }
    return {};
  };
  for (auto const& text : *lines) {
    auto begin = text.begin();
    if (!begin)
      return std::unexpected(std::move(begin.error()));
    std::basic_string<String::Char> line(*begin, text.end());
    if (line.empty())
      continue;
    if (line[0] == '%') {
      // The extended Transpose field uses a percent sign followed by two spaces.
      if (line.size() >= 3 && line[1] == ' ' && line[2] == ' ') {
        auto stored = setField(std::basic_string_view<String::Char>(line).substr(3));
        if (!stored)
          return std::unexpected(std::move(stored.error()));
      }
      continue;
    }
    // Truncate comments before interpreting fields and notes.
    line.erase(std::find(line.begin(), line.end(), '%'), line.end());
    if (line.size() > 1 && line[1] == ':' && line[0] != '|') {
      auto stored = setField(line);
      if (!stored)
        return std::unexpected(std::move(stored.error()));
    } else {
      if (dirtyFlags) {
        auto mapping = fetchKeySignatureMapping();
        if (!mapping)
          return std::unexpected(std::move(mapping.error()));
        keySignatureMapping = std::move(*mapping);
        auto beat = secondsPerBeat();
        if (!beat)
          return std::unexpected(std::move(beat.error()));
        fullNoteDuration = *beat;
        auto length = noteLength();
        if (!length)
          return std::unexpected(std::move(length.error()));
        noteDuration = (*length)[0] * fullNoteDuration / (*length)[1];
        auto measured = meter();
        if (!measured)
          return std::unexpected(std::move(measured.error()));
        auto m = *measured;
        barDuration = m[0] * fullNoteDuration / m[1];
        dirtyFlags = false;

        int tdt = 2;

        if ((m[1] == 8) && ((m[0] == 6) || (m[0] == 9) || (m[0] == 12)))
          tdt = 3;

        tupleMapping.clear();
        tupleMapping.append(0);
        tupleMapping.append(0);
        tupleMapping.append(3);
        tupleMapping.append(2);
        tupleMapping.append(3);
        tupleMapping.append(tdt);
        tupleMapping.append(2);
        tupleMapping.append(tdt);
        tupleMapping.append(3);
        tupleMapping.append(tdt);
      }

      std::size_t cursor = 0;
      auto peek = [&]() -> String::Char {
        return cursor == line.size() ? '\0' : line[cursor];
      };
      // Every read below is dominated by a nonempty/peek check.
      auto readChar = [&]() {
        return line[cursor++];
      };
      auto readDuration = [&]() -> std::expected<double, StarException> {
        double duration = 1;
        if (String::isAsciiNumber(peek())) {
          std::string digits;
          while (String::isAsciiNumber(peek()))
            digits += static_cast<char>(readChar());
          auto multiplier = lexicalCast<int>(std::string_view(digits));
          if (!multiplier)
            return std::unexpected(std::move(multiplier.error()));
          duration *= *multiplier;
        }
        if (peek() == '/') {
          readChar();
          double divisor = 2;
          if (String::isAsciiNumber(peek())) {
            std::string digits;
            while (String::isAsciiNumber(peek()))
              digits += static_cast<char>(readChar());
            auto parsed = lexicalCast<int>(std::string_view(digits));
            if (!parsed)
              return std::unexpected(std::move(parsed.error()));
            divisor = *parsed;
          }
          if (divisor == 0)
            return std::unexpected(StarException("ABC duration has a zero divisor"));
          duration /= divisor;
        }
        return duration;
      };

      while (cursor != line.size()) {
        auto head = readChar();
        if (String::isSpace(head))
          continue;

        if (head == '|') {
          now = lastBarNow + barDuration;
          lastBarNow = now;
          // section/repetition artifact, nor supported
          if (peek() == ':')
            readChar();
          else if (peek() == ']')
            readChar();
          else {
            if (peek() == '[')
              readChar();
            while (String::isAsciiNumber(peek()))
              readChar();
          }
          accidentals = 0;
          accidentalSpecified = false;
          impliedAccidentals.clear();
          continue;
        }
        if (head == '~') {
          // ornament ?
          continue;
        }
        if (head == ':') {
          // section/repetition artifact, nor supported
          if (cursor == line.size())
            return std::unexpected(StarException("Truncated ABC repetition marker"));
          readChar();
          continue;
        }
        if (head == '^') {
          accidentals += 1;
          accidentalSpecified = true;
          continue;
        }
        if (head == '_') {
          accidentals -= 1;
          accidentalSpecified = true;
          continue;
        }
        if (head == '=') {
          accidentals = 0;
          accidentalSpecified = true;
          continue;
        }
        if (head == '[') {
          grouped = true;
          groupStartIndex = result.size();
          continue;
        }
        if (head == ']') {
          grouped = false;
          if (tupleCount > 0)
            tupleCount--;
          auto parsedDuration = readDuration();
          if (!parsedDuration)
            return std::unexpected(std::move(parsedDuration.error()));
          auto duration = *parsedDuration;
          if (duration != 1) {
            for (int index = groupStartIndex; index < static_cast<int>(result.size()); index++) {
              auto& entry = result[index];
              entry.duration *= duration;
            }
          }
          now += groupDuration * duration;
          groupDuration = 0;
          staccato = false;
          continue;
        }
        if (head == '(') {
          if (String::isAsciiNumber(peek())) {
            int p = 0;
            int q = 0;
            int r = 0;
            p = static_cast<int>(readChar()) - static_cast<int>('0');
            if (peek() == ':') {
              readChar();
              if (String::isAsciiNumber(peek()))
                q = static_cast<int>(readChar()) - static_cast<int>('0');
              if (peek() == ':') {
                readChar();
                if (String::isAsciiNumber(peek()))
                  r = static_cast<int>(readChar()) - static_cast<int>('0');
              }
            }

            if (r == 0)
              r = p;
            if (q == 0)
              q = tupleMapping[p];
            if (p == 0)
              return std::unexpected(StarException("ABC tuplet has a zero count"));

            tupleCount = p;
            tupleDurationFactor = static_cast<float>(q) / static_cast<float>(p);
          }

          continue;
        }

        if (head == '+') {
          while (cursor != line.size()) {
            if (readChar() == '+')
              break;
          }
          continue;
        }
        if (head == '!') {
          while (cursor != line.size()) {
            if (readChar() == '!')
              break;
          }
          continue;
        }
        if (head == '.') {
          staccato = true;
          continue;
        }

        if (String::isAsciiLetter(head)) {
          int note = (String::toLower(head) != head) ? 60 : 72;
          while (peek() == ',') {
            readChar();
            note -= 12;
          }
          while (peek() == '\'') {
            readChar();
            note += 12;
          }
          switch (String::toLower(head)) {
            case 'c': {
              note += 0;
              break;
            }
            case 'd': {
              note += 2;
              break;
            }
            case 'e': {
              note += 4;
              break;
            }
            case 'f': {
              note += 5;
              break;
            }
            case 'g': {
              note += 7;
              break;
            }
            case 'a': {
              note += 9;
              break;
            }
            case 'b': {
              note += 11;
              break;
            }
            case 'x': {
              note = 0;
              break;
            }
            case 'z': {
              note = 0;
              break;
            }
            default: return std::unexpected(StarException(strf("Unrecognized note {}", static_cast<char>(head))));
            }
          if (note != 0) {
            bool accidentalActive = accidentalSpecified;
            if (!accidentalSpecified) {
              if (impliedAccidentals.contains(note)) {
                accidentals = impliedAccidentals.value(note);
                accidentalActive = true;
              }
            } else {
              impliedAccidentals[note] = accidentals;
            }
            note += accidentals;

            // int base = note;

            if (!accidentalActive) {
              switch (String::toLower(head)) {
                case 'c': {
                  note += keySignatureMapping[0];
                  break;
                }
                case 'd': {
                  note += keySignatureMapping[1];
                  break;
                }
                case 'e': {
                  note += keySignatureMapping[2];
                  break;
                }
                case 'f': {
                  note += keySignatureMapping[3];
                  break;
                }
                case 'g': {
                  note += keySignatureMapping[4];
                  break;
                }
                case 'a': {
                  note += keySignatureMapping[5];
                  break;
                }
                case 'b': {
                  note += keySignatureMapping[6];
                  break;
                }
                case 'x': {
                  break;
                }
                case 'z': {
                  break;
                }
                default: return std::unexpected(StarException(strf("Unrecognized note {}", static_cast<char>(head))));
                }
            }

            // std::cerr << ":" << note << ":" << (int)accidentalSpecified <<
            // ":" <<
            // (int)accidentalActive << ":" << accidentals << ":" << (note-base)
            // << ":" << head <<
            // "\n";
          }
          accidentals = 0;
          accidentalSpecified = false;
          auto parsedDuration = readDuration();
          if (!parsedDuration)
            return std::unexpected(std::move(parsedDuration.error()));
          double duration = *parsedDuration;
          duration *= noteDuration;

          if (tupleCount > 0)
            duration *= tupleDurationFactor;

          double noteDuration = duration;
          if (staccato)
            noteDuration *= 0.5;

          if (note != 0) {
            auto transposition = transpose();
            if (!transposition)
              return std::unexpected(std::move(transposition.error()));
            auto transposed = static_cast<std::int64_t>(note) + *transposition;
            if (transposed < std::numeric_limits<int>::min() || transposed > std::numeric_limits<int>::max())
              return std::unexpected(StarException("ABC transposition is out of range"));
            note = static_cast<int>(transposed);
            bool tied = peek() == '-';
            if (auto pending = pendingTies.find_ref(note)) {
              result[pending->get()].duration += noteDuration;
              if (!tied)
                pendingTies.remove(note);
            } else {
              auto mapping = noteMapping(m_instrument, m_species, note);
              if (!mapping)
                return std::unexpected(std::move(mapping.error()));
              auto file = m_root.random().randFrom(mapping->get().files);
              if (!file)
                return std::unexpected(std::move(file.error()));
              result.append(Note{m_instrument, file->get(), now, noteDuration,
                  mapping->get().fadeout, mapping->get().velocity});
              if (tied)
                pendingTies[note] = result.size() - 1;
            }
          }
          if (!grouped) {
            if (tupleCount > 0)
              tupleCount--;
            now += duration;
            staccato = false;
          } else if (groupDuration == 0)
            groupDuration = duration;
          else
            groupDuration = std::min(groupDuration, duration);
        }
      }
    }
  }
  sortByComputedValue(result, [](Note const& note) { return note.timecode; });
  return result;
}

void Songbook::stop() {
  if (m_stopped)
    return;
  m_stopped = true;
  m_track.clear();
  m_heldNotes.clear();
  m_pendingAudio.clear();
  m_noteMapping.clear();
  m_uncompressedSamples.clear();
  m_activeCooldown = 0;
  m_song = {};
  m_dataUpdated = true;
  m_dataChanged = true;
}

void Songbook::play(Json const& song, String const& timeSource) {
  stop();

  m_song = song;
  m_timeSource = timeSource;
  if (m_timeSource.empty())
    m_timeSource = toString(m_root.random().randu64());

  {
    m_epochUpdated = false;
    m_timeSourceEpoch = Time::millisecondsSinceEpoch();
    std::lock_guard lock(m_synchronization.mutex);
    if (!m_synchronization.timeSources.contains(m_timeSource)) {
      m_timeSourceInstance = std::make_shared<TimeSource>();
      m_synchronization.timeSources[m_timeSource] = m_timeSourceInstance;
      m_timeSourceInstance->epoch = m_timeSourceEpoch;
      m_timeSourceInstance->keepalive = m_timeSourceEpoch;
    } else {
      m_timeSourceInstance = m_synchronization.timeSources[m_timeSource];
      if ((Time::millisecondsSinceEpoch() - m_timeSourceInstance->keepalive) > 5000) {
        m_timeSourceInstance->epoch = m_timeSourceEpoch;
        m_timeSourceInstance->keepalive = m_timeSourceEpoch;
      }
    }
    m_timeSourceEpoch = m_timeSourceInstance->epoch;
  }

  m_dataUpdated = true;
  m_dataChanged = true;
  m_activeCooldown = 3;
}

bool Songbook::active() const {
  return m_activeCooldown > 0;
}

bool Songbook::instrumentPlaying() const {
  if (!active())
    return false;
  if (m_timeSourceInstance) {
    auto now = (Time::millisecondsSinceEpoch() - m_timeSourceInstance->epoch) / 1000.0;
    for (auto const& n : m_heldNotes) {
      if ((n.start <= now) && (now <= n.end))
        return true;
    }
  }
  return false;
}

std::optional<String> Songbook::timeSource() const {
  return m_timeSource;
}

std::optional<String> Songbook::instrument() const {
  return m_instrument;
}

Json Songbook::song() const {
  return m_song;
}

double Songbook::fundamentalFrequency(double p) {
  return 55.0 * std::pow(2.0, (p - 69.0) / 12.0 + 3.0);
}

double Songbook::fundamentalPitch(double f) { return 69.0 + 12 * std::log2(f / 440.0); }

std::expected<void, StarException> Songbook::netElementsNeedLoad(bool) {
  if (m_songNetState.pullUpdated()) {
    m_song = m_songNetState.get();
    m_timeSourceEpoch = m_timeSourceEpochNetState.get();
    m_epochUpdated = true;
    m_dataUpdated = true;
  }
  m_timeSource = m_timeSourceNetState.get();
  if (m_activeNetState.get())
    m_activeCooldown = 3;
  else
    m_activeCooldown = 0;
  m_instrument = m_instrumentNetState.get();
  return {};
}

std::expected<void, StarException> Songbook::netElementsNeedStore() {
  if (m_serverMode)
    return {};
  if (m_dataChanged) {
    m_songNetState.set(m_song);
    m_timeSourceEpochNetState.set(m_globalNowDelta + m_timeSourceEpoch);
    m_dataChanged = false;
  }
  m_activeNetState.set(active());
  m_instrumentNetState.set(m_instrument);
  m_timeSourceNetState.set(m_timeSource);
  return {};
}

}
