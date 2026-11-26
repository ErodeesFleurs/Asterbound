import std;
import star.armor_wearer;
import star.armors;
import star.array;
import star.casting;
import star.data_stream;
import star.effect_emitter;
import star.format;
import star.game_types;
import star.humanoid;
import star.item;
import star.item_database;
import star.item_descriptor;
import star.json;
import star.list;
import star.net_element_basic_fields;
import star.optional;
import star.root;
import star.set;
import star.status_controller;
import star.status_types;
import star.string;
import star.tool_user;

namespace Star {

namespace {
constexpr auto slot_keys = std::to_array<std::string_view>({
    "headItem", "chestItem", "legsItem", "backItem",
    "headCosmeticItem", "chestCosmeticItem", "legsCosmeticItem", "backCosmeticItem",
    "cosmetic1Item", "cosmetic2Item", "cosmetic3Item", "cosmetic4Item",
    "cosmetic5Item", "cosmetic6Item", "cosmetic7Item", "cosmetic8Item",
    "cosmetic9Item", "cosmetic10Item", "cosmetic11Item", "cosmetic12Item"});
}

ArmorWearer::ArmorWearer(ConstructionKey, Root& root) : m_root(root) {}

std::expected<std::shared_ptr<ArmorWearer>, StarException> ArmorWearer::create(Root& root) {
  static_assert(slot_keys.size() == slot_count);
  auto wearer = std::make_shared<ArmorWearer>(ConstructionKey{}, root);
  for (std::size_t i = 0; i != wearer->m_armors.size(); ++i) {
    auto& armor = wearer->m_armors[i];
    if (i >= extra_cosmetic_begin)
      armor.net_state.set_compatibility_version(9);
    auto added = wearer->add_net_element(armor.net_state);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  return wearer;
}

std::expected<bool, StarException> ArmorWearer::setup_humanoid(Humanoid& humanoid, bool force_nude) {
  if (m_appearance_dirty) {
    // Cosmetic blockers are counted even when force_nude hides their own wearable.
    m_worn_cosmetic_types.fill(0);
    for (std::uint8_t i = cosmetic_begin; i != slot_count; ++i) {
      auto const& armor = m_armors[i];
      if (armor.visible && armor.item && armor.item->visible(i >= extra_cosmetic_begin))
        for (auto type : armor.item->armorTypesToHide())
          ++m_worn_cosmetic_types[static_cast<std::uint8_t>(type)];
    }
  }

  if (m_appearance_dirty || m_last_nude != force_nude) {
    auto previous_slots = m_visible_slots;
    m_visible_slots = 0;
    m_flipping_slots = 0;
    for (std::uint8_t i = 0; i != slot_count; ++i) {
      auto& armor = m_armors[i];
      auto const& item = armor.item;
      if (armor.visible && item && item->visible(i >= extra_cosmetic_begin)
          && (!force_nude || item->bypassNude())
          && (i >= cosmetic_begin || !m_worn_cosmetic_types[static_cast<std::uint8_t>(item->armorType())])) {
        auto bit = std::uint32_t{1} << i;
        m_visible_slots |= bit;
        if (item->flipping())
          m_flipping_slots |= bit;
        if (armor.needs_sync) {
          m_pending_wearables |= bit;
          m_config_dirty = true;
        }
      }
      armor.needs_sync = false;
    }
    auto changed_slots = previous_slots ^ m_visible_slots;
    m_pending_wearables |= changed_slots;
    m_config_dirty |= changed_slots != 0;
    m_last_nude = force_nude;
    m_appearance_dirty = false;
  }

  auto gender = humanoid.identity().gender;
  if (gender != m_last_gender) {
    m_pending_wearables |= m_visible_slots;
    m_last_gender = gender;
  }
  auto direction = humanoid.facing_direction();
  if (direction != m_last_direction) {
    m_pending_wearables |= m_flipping_slots;
    m_last_direction = direction;
  }

  while (m_pending_wearables) {
    auto slot = static_cast<std::uint8_t>(std::countr_zero(m_pending_wearables));
    std::expected<void, StarException> applied;
    if (m_visible_slots & (std::uint32_t{1} << slot)) {
      auto const& item = *m_armors[slot].item;
      if (auto head = as_ref<HeadArmor>(item))
        applied = humanoid.set_wearable_from_head(slot, head->get(), gender);
      else if (auto chest = as_ref<ChestArmor>(item))
        applied = humanoid.set_wearable_from_chest(slot, chest->get(), gender);
      else if (auto legs = as_ref<LegsArmor>(item))
        applied = humanoid.set_wearable_from_legs(slot, legs->get(), gender);
      else if (auto back = as_ref<BackArmor>(item))
        applied = humanoid.set_wearable_from_back(slot, back->get(), gender);
    } else {
      applied = humanoid.remove_wearable(slot);
    }
    if (!applied)
      return std::unexpected(std::move(applied.error()));
    m_pending_wearables &= m_pending_wearables - 1;
  }

  if (!m_config_dirty)
    return false;
  bool body_hidden = false;
  Json merged_config;
  for (auto slots = m_visible_slots; slots; slots &= slots - 1) {
    auto const& item = *m_armors[std::countr_zero(slots)].item;
    body_hidden |= item.hideBody();
    auto config = item.instance_value("humanoidConfig");
    if (!config)
      return std::unexpected(std::move(config.error()));
    if (config->is_type(Json::Type::Object))
      merged_config = merged_config ? json_merge(merged_config, *config) : std::move(*config);
  }
  auto loaded = humanoid.load_config(std::move(merged_config), body_hidden);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  m_config_dirty = false;
  return *loaded;
}

std::expected<void, StarException> ArmorWearer::effects(EffectEmitter& effect_emitter) const {
  static constexpr std::array<std::string_view, 4> positions{"headArmor", "chestArmor", "legsArmor", "backArmor"};
  std::array<StringSet, positions.size()> groups;
  for (std::size_t i = 0; i != m_armors.size(); ++i) {
    auto const& armor = m_armors[i];
    if (!armor.visible || !armor.item)
      continue;
    auto item = armor.item;
    auto type = std::to_underlying(item->armorType());
    if (i < cosmetic_begin && type < groups.size() && m_worn_cosmetic_types[type] != 0)
      continue;
    auto sources = item->effectSources();
    if (!sources)
      return std::unexpected(std::move(sources.error()));
    if (type >= groups.size() || sources->empty())
      continue;
    auto& group = groups[type];
    if (group.empty())
      group = std::move(*sources);
    else
      group.insert_range(*sources);
  }

  // Do not emit a partial tick if a later item's virtual query fails.
  for (std::size_t i = 0; i != groups.size(); ++i)
    if (!groups[i].empty())
      effect_emitter.add_effect_sources(positions[i], std::move(groups[i]));
  return {};
}

void ArmorWearer::reset() {
  for (auto& armor : m_armors) {
    armor.item.reset();
    armor.needs_store = armor.needs_sync = true;
  }
  m_worn_cosmetic_types.fill(0);
  m_visible_slots = 0;
  m_flipping_slots = 0;
  m_pending_wearables = all_slots;
  m_appearance_dirty = m_config_dirty = true;
}

std::expected<Json, StarException> ArmorWearer::disk_store() const {
  JsonObject res;
  std::size_t first = 0;
  while (first != m_armors.size() && !m_armors[first].item)
    ++first;
  if (first == m_armors.size())
    return res;
  auto versioning = m_root.versioningDatabase();
  if (!versioning)
    return std::unexpected(std::move(versioning.error()));
  for (std::size_t i = first; i != m_armors.size(); ++i) {
    auto const& armor = m_armors[i];
    if (!armor.item)
      continue;
    auto stored = armor.item->descriptor().disk_store(**versioning);
    if (!stored)
      return std::unexpected(std::move(stored.error()));
    res[String(slot_keys[i])] = std::move(*stored);
  }
  return res;
}

std::expected<void, StarException> ArmorWearer::disk_load(Json const& store) {
  auto database = m_root.itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  for (std::size_t i = 0; i != m_armors.size(); ++i) {
    auto entry = store.find(slot_keys[i]);
    if (!entry)
      return std::unexpected(std::move(entry.error()));
    if (!*entry)
      continue;
    auto loaded = (*database)->disk_load((*entry)->get());
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
    if (auto item = as<ArmorItem>(std::move(*loaded)))
      set_item(i, std::move(item));
  }
  return {};
}

std::expected<List<PersistentStatusEffect>, StarException> ArmorWearer::status_effects(bool cosmetic_only) const {
  List<PersistentStatusEffect> result;
  for (std::size_t i = 0; i != m_armors.size(); ++i) {
    auto const& item = m_armors[i].item;
    if (!item)
      continue;
    if (!cosmetic_only && (i < cosmetic_begin || item->status_effects_in_cosmetic_slot())) {
      auto effects = item->statusEffects();
      if (!effects)
        return std::unexpected(std::move(effects.error()));
      if (result.empty())
        result = std::move(*effects);
      else
        result.append_range(std::move(*effects));
    }
    if (m_visible_slots & (std::uint32_t{1} << i))
      result.append_range(item->cosmetic_status_effects());
  }
  return result;
}

std::expected<void, StarException> synchronize_equipment_status_effects(
    ArmorWearer const& armor, ToolUser const& tools, StatusController& status_controller, bool cosmetic_only) {
  auto armor_effects = armor.status_effects(cosmetic_only);
  if (!armor_effects)
    return std::unexpected(std::move(armor_effects.error()));
  if (auto result = status_controller.setPersistentEffects("armor", *armor_effects); !result)
    return std::unexpected(std::move(result.error()));

  auto tool_effects = tools.statusEffects();
  if (!tool_effects)
    return std::unexpected(std::move(tool_effects.error()));
  return status_controller.setPersistentEffects("tools", *tool_effects);
}

bool ArmorWearer::set_item(std::size_t slot, std::shared_ptr<ArmorItem> item, bool visible) {
  if (slot >= m_armors.size())
    return false;
  auto& armor = m_armors[slot];

  bool const item_changed = armor.item != item && !Item::items_equal(optional_ref(armor.item), optional_ref(item));
  if (!item_changed && armor.visible == visible)
    return false;

  if (item_changed)
    armor.item = std::move(item);
  armor.visible = visible;
  armor.needs_store = true;
  armor.needs_sync = true;
  m_appearance_dirty = true;
  return true;
}

ItemDescriptor ArmorWearer::item_descriptor(std::size_t slot) const {
  if (slot < m_armors.size() && m_armors[slot].item)
    return m_armors[slot].item->descriptor();
  return {};
}

bool ArmorWearer::set_cosmetic_item(std::size_t slot, std::shared_ptr<ArmorItem> item) {
  if (slot >= slot_count - extra_cosmetic_begin)
    return false;
  return set_item(extra_cosmetic_begin + slot, std::move(item), true);
}

std::expected<void, StarException> ArmorWearer::net_elements_need_load(bool) {
  auto database = m_root.itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  for (auto& armor : m_armors) {
    if (!armor.net_state.pull_updated())
      continue;
    auto loaded = (*database)->load_item(armor.net_state.get(), armor.item);
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
    if (*loaded) {
      armor.needs_store = true;
      armor.needs_sync = true;
      m_appearance_dirty = true;
    }
  }
  return {};
}

std::expected<void, StarException> ArmorWearer::net_elements_need_store() {
  for (auto& armor : m_armors) {
    if (armor.needs_store) {
      armor.net_state.set(armor.visible && armor.item ? armor.item->descriptor() : ItemDescriptor());
      armor.needs_store = false;
    }
  }
  return {};
}

}
