import star.format;
import star.memory;
import star.json;
import star.json_extra;
import star.data_stream;
import star.game_types;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.bi_map;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.audio;
import star.map;
import star.set;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;

import star.drawable;
import star.animation;
import star.particle;
import star.animated_part_set;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.humanoid;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.emote_processor;

namespace Star {

std::expected<std::shared_ptr<EmoteProcessor>, StarException> EmoteProcessor::create(std::shared_ptr<Assets const> const& assets) {
  auto result = std::shared_ptr<EmoteProcessor>(new EmoteProcessor());
  auto cfg = assets->json("/emotes.config");
  if (!cfg) return std::unexpected(std::move(cfg.error()));
  auto bindings = cfg->get_object("emoteBindings");
  if (!bindings) return std::unexpected(std::move(bindings.error()));
  if (!humanoid_emote_names) return std::unexpected(humanoid_emote_names.error());
  for (auto const& binding : *bindings) {
    auto array = binding.second.array_ptr();
    if (!array) return std::unexpected(std::move(array.error()));
    if ((*array)->empty()) continue;
    auto emote = humanoid_emote_names->get_left(binding.first);
    if (!emote) return std::unexpected(std::move(emote.error()));
    for (auto const& text : **array) {
      auto string = text.to_string();
      if (!string) return std::unexpected(std::move(string.error()));
      auto length = string->length();
      // Invalid needles must fail only if the old search actually inspects them.
      float bonus = length ? static_cast<float>(*length) * 0.01f : 0.f;
      bool ascii = length && *length == string->utf8().size();
      result->m_emote_bindings.push_back(EmoteBinding{bonus, ascii, std::move(*string), emote->get()});
    }
  }
  return result;
}

std::expected<HumanoidEmote, StarException> EmoteProcessor::detect_chat_emote(Root& root, String const& chatter) {
  auto processor = root.emoteProcessor();
  if (!processor) return std::unexpected(std::move(processor.error()));
  return (*processor)->detect_emotes(chatter);
}

std::expected<HumanoidEmote, StarException> EmoteProcessor::detect_emotes(String const& chatter) const {
  auto const& bytes = chatter.utf8();
  bool ascii = true;
  std::size_t caps = 0, lowercase = 0;
  for (unsigned char byte : bytes) {
    if (byte >= 128) {
      ascii = false;
      break;
    }
    caps += byte >= 'A' && byte <= 'Z';
    lowercase += byte >= 'a' && byte <= 'z';
  }
  if (!ascii) {
    auto begin = chatter.begin(); // Validates the entire message, including unmatched suffixes.
    if (!begin) return std::unexpected(std::move(begin.error()));
    caps = lowercase = 0;
    for (auto it = *begin; it != chatter.end(); ++it) {
      auto point = *it;
      caps += point >= 'A' && point <= 'Z';
      lowercase += point >= 'a' && point <= 'z';
    }
  }
  auto result = bytes.empty() ? HumanoidEmote::Idle
      : caps > lowercase ? HumanoidEmote::Shouting : HumanoidEmote::Blabbering;
  float best_match = -1;
  for (auto const& option : m_emote_bindings) {
    std::size_t position;
    if (ascii && option.ascii) {
      position = bytes.find(option.text.utf8());
    } else {
      auto found = chatter.find(option.text);
      if (!found) return std::unexpected(std::move(found.error()));
      position = *found;
    }
    if (position == npos) continue;
    float score = position + option.length_bonus;
    if (score > best_match) {
      best_match = score;
      result = option.emote;
    }
  }
  return result;
}

}
