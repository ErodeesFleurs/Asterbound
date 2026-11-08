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
import star.version;

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
  auto bindings = cfg->getObject("emoteBindings");
  if (!bindings) return std::unexpected(std::move(bindings.error()));
  if (!HumanoidEmoteNames) return std::unexpected(HumanoidEmoteNames.error());
  for (auto const& binding : *bindings) {
    auto array = binding.second.toArray();
    if (!array) return std::unexpected(std::move(array.error()));
    for (auto const& text : *array) {
      auto emote = HumanoidEmoteNames->get_left(binding.first);
      if (!emote) return std::unexpected(std::move(emote.error()));
      auto string = text.toString();
      if (!string) return std::unexpected(std::move(string.error()));
      EmoteBinding value;
      value.emote = emote->get();
      value.text = std::move(*string);
      result->m_emoteBindings.push_back(std::move(value));
    }
  }
  return result;
}

std::expected<HumanoidEmote, StarException> EmoteProcessor::detectEmotes(String const& chatter) const {
  auto isShouty = [](String const& text) -> std::expected<bool, StarException> {
    int caps = 0;
    int noCaps = 0;
    auto begin = text.begin();
    if (!begin) return std::unexpected(std::move(begin.error()));
    for (auto it = *begin; it != text.end(); ++it) {
      auto c = *it;
      if (String::toUpper(c) != String::toLower(c)) {
        if (String::toUpper(c) == c)
          caps++;
        else
          noCaps++;
      }
    }
    return caps > noCaps;
  };

  HumanoidEmote result = HumanoidEmote::Idle;
  if (!chatter.empty()) {
    auto shouty = isShouty(chatter);
    if (!shouty) return std::unexpected(std::move(shouty.error()));
    if (*shouty)
      result = HumanoidEmote::Shouting;
    else
      result = HumanoidEmote::Blabbering;
  }

  float bestMatch = -1;

  for (auto const& option : m_emoteBindings) {
    auto p = chatter.find(option.text);
    if (!p) return std::unexpected(std::move(p.error()));
    if (*p == NPos)
      continue;
    auto length = option.text.length();
    if (!length) return std::unexpected(std::move(length.error()));
    float r = *p + static_cast<float>(*length) * 0.01f;
    if (r > bestMatch) {
      bestMatch = r;
      result = option.emote;
    }
  }
  return result;
}

}
