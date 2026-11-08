import star.json;
import star.json_extra;
import star.set;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.strong_typedef;
import star.bi_map;
import star.game_types;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.version;

import star.lua_components;
import star.drawable;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.item;
import star.item_descriptor;
import star.item_recipe;
import star.augment_item;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.config_lua_bindings;
import star.item_database;


namespace Star {

std::expected<std::shared_ptr<AugmentItem>, StarException> AugmentItem::create(Root& root, Json const& config, String const& directory, Json const& parameters) {
  auto item = std::shared_ptr<AugmentItem>(new AugmentItem(root));
  auto initialized = item->initialize(config, directory, parameters);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> AugmentItem::clone() const {
  return create(root(), config(), directory(), parameters()).transform([](std::shared_ptr<AugmentItem> item) -> std::shared_ptr<Item> { return item; });
}

std::expected<StringList, StarException> AugmentItem::augmentScripts() const {
  auto scriptsValue = instanceValue("scripts");
  if (!scriptsValue)
    return std::unexpected(std::move(scriptsValue.error()));
  auto scripts = jsonToStringList(*scriptsValue);
  if (!scripts)
    return std::unexpected(std::move(scripts.error()));
  StringList result;
  for (auto const& script : *scripts) {
    result.push_back(AssetPath::relative_to(directory(), script));
  }
  return result;
}

std::expected<std::shared_ptr<Item>, StarException> AugmentItem::applyTo(std::shared_ptr<Item> const item) {
  auto database = root().itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  return (*database)->applyAugment(item, *this);
}

}
