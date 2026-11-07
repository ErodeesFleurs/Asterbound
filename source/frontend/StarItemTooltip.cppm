module;
import std;
import star.json;
import star.json_extra;
import star.algorithm;
import star.id_map;
import star.string;
import star.strong_typedef;
import star.data_stream;
import star.casting;
import star.input_event;
import star.color;
import star.font;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.parametric_function;
import star.multi_array_interpolator;
import star.multi_table;
import star.rpc_promise;
import star.a_star;
import star.logging;
import star.output_proxy;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.format;
import star.version;
import star.random;
import star.periodic;
import star.periodic_function;
import star.lua;


import star.lua_root;
import star.lua_components;
import star.lua_animation_component;
import star.status_types;
import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.image;
import star.application_controller;
import star.renderer;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.font_texture_group;
import star.anchor_types;
import star.text_painter;
import star.drawable;
import star.asset_texture_group;
import star.drawable_painter;
import star.gui_types;
import star.key_bindings;
import star.mixer;
import star.gui_context;
import star.widget;
import star.widget_parsing;
import star.gui_reader;
import star.pane;
import star.list_widget;
import star.label_widget;
import star.image_widget;
import star.progress_widget;
import star.animation;
import star.item_slot_widget;
import star.previewable_item;
import star.item;
import star.effect_source_item;
import star.swingable_item;
import star.armors;
import star.damage_types;
import star.world_geometry;
import star.damage;
import star.light_source;
import star.entity;
import star.particle;
import star.interaction_types;
import star.tile_damage;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;
import star.game_timers;
import star.actor_movement_controller;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.tool_user_item;
import star.status_effect_item;
import star.fireable_item;
import star.status_effect_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.animated_part_set;
import star.networked_animator;
import star.entity_rendering_types;
import star.entity_rendering;
import star.object;

export module star.item_tooltip;
import star.stored_functions;

import star.object_item;
import star.status_effect_database;
import star.object_database;


export namespace Star {
namespace ItemTooltipBuilder {
  [[nodiscard]] std::expected<std::shared_ptr<Pane>, StarException> buildItemTooltip(GuiContext& context, std::shared_ptr<Item> const& item, std::shared_ptr<Player> const& viewer = {});

  [[nodiscard]] std::expected<void, StarException> buildItemDescription(GuiContext& context, std::shared_ptr<Widget> const& container, std::shared_ptr<Item> const& item);
  [[nodiscard]] std::expected<void, StarException> buildItemDescriptionInner(
      GuiContext& context, std::shared_ptr<Widget> const& container, std::shared_ptr<Item> const& item, String const& tooltipKind, String& title, String& subtitle, std::shared_ptr<Player> const& viewer = {});

  [[nodiscard]] std::expected<void, StarException> describePersistentEffect(GuiContext& context, std::shared_ptr<ListWidget> const& container, PersistentStatusEffect const& effect);
};
}

namespace Star {

std::expected<std::shared_ptr<Pane>, StarException> ItemTooltipBuilder::buildItemTooltip(GuiContext& context, std::shared_ptr<Item> const& item, std::shared_ptr<Player> const& viewer) {
  if (!item) {
    return {};
  } else {
    auto createResult770 = Pane::create(context);
    if (!createResult770)
      return std::unexpected(std::move(createResult770.error()));
    std::shared_ptr<Pane> tooltip = std::move(*createResult770);
    tooltip->removeAllChildren();

    String title;
    String subTitle;

    String tooltipKind = item->tooltipKind();

    if (tooltipKind.empty())
      tooltipKind = "base";
    {
      auto endsWithResult771 = tooltipKind.endsWith(".tooltip");
      if (!endsWithResult771)
        return std::unexpected(std::move(endsWithResult771.error()));
      if (!std::move(*endsWithResult771))
        tooltipKind = "/interface/tooltips/" + tooltipKind + ".tooltip";
    }

    auto buildItemDescriptionInnerResult772 = buildItemDescriptionInner(context, tooltip, item, tooltipKind, title, subTitle, viewer);
    if (!buildItemDescriptionInnerResult772)
      return std::unexpected(std::move(buildItemDescriptionInnerResult772.error()));

    auto createResult773 = ItemSlotWidget::create(context, item, "/interface/inventory/portrait.png");
    if (!createResult773)
      return std::unexpected(std::move(createResult773.error()));
    auto titleIcon = std::move(*createResult773);
    titleIcon->setBackingImageAffinity(true, true);
    titleIcon->showRarity(false);
    auto setTitleResult774 = tooltip->setTitle(titleIcon, title, subTitle);
    if (!setTitleResult774)
      return std::unexpected(std::move(setTitleResult774.error()));

    return tooltip;
  }
}

std::expected<void, StarException> ItemTooltipBuilder::buildItemDescription(GuiContext& context, std::shared_ptr<Widget> const& container, std::shared_ptr<Item> const& item) {
  String tooltipKind = item->tooltipKind();

  if (tooltipKind.empty())
    tooltipKind = "base";
  {
    auto endsWithResult775 = tooltipKind.endsWith(".itemdescription");
    if (!endsWithResult775)
      return std::unexpected(std::move(endsWithResult775.error()));
    if (!std::move(*endsWithResult775))
      tooltipKind = "/interface/itemdescriptions/" + tooltipKind + ".itemdescription";
  }

  String title;
  String subTitle;
  auto buildItemDescriptionInnerResult776 = buildItemDescriptionInner(context, container, item, tooltipKind, title, subTitle);
  if (!buildItemDescriptionInnerResult776)
    return std::unexpected(std::move(buildItemDescriptionInnerResult776.error()));

return {};
}

[[nodiscard]] std::expected<String, StarException> categoryDisplayName(GuiContext& context, String const& category) {
  auto assetsResult777 = context.root().assets();
  if (!assetsResult777)
    return std::unexpected(std::move(assetsResult777.error()));
  auto jsonResult778 = std::move(*assetsResult777)->json("/items/categories.config:labels");
  if (!jsonResult778)
    return std::unexpected(std::move(jsonResult778.error()));
  Json categories = std::move(*jsonResult778);
  auto getStringResult779 = categories.getString(category, category);
  if (!getStringResult779)
    return std::unexpected(std::move(getStringResult779.error()));
  return std::move(*getStringResult779);
}

std::expected<void, StarException> ItemTooltipBuilder::buildItemDescriptionInner(
    GuiContext& context, std::shared_ptr<Widget> const& container, std::shared_ptr<Item> const& item, String const& tooltipKind, String& title, String& subTitle, std::shared_ptr<Player> const& viewer) {
  GuiReader reader(context);
  auto& root = context.root();
  auto assetsResult780 = root.assets();
  if (!assetsResult780)
    return std::unexpected(std::move(assetsResult780.error()));
  auto assets = std::move(*assetsResult780);
  title = item->friendlyName();
  auto categoryDisplayNameResult781 = categoryDisplayName(context, item->category());
  if (!categoryDisplayNameResult781)
    return std::unexpected(std::move(categoryDisplayNameResult781.error()));
  subTitle = std::move(*categoryDisplayNameResult781);
  String description = item->description();

  auto jsonResult782 = assets->json(tooltipKind);
  if (!jsonResult782)
    return std::unexpected(std::move(jsonResult782.error()));
  auto constructResult783 = reader.construct(std::move(*jsonResult782), *container);
  if (!constructResult783)
    return std::unexpected(std::move(constructResult783.error()));

  {
    auto containsChildResult784 = container->containsChild("icon");
    if (!containsChildResult784)
      return std::unexpected(std::move(containsChildResult784.error()));
    if (std::move(*containsChildResult784))
      {
        auto fetchChildResult785 = container->fetchChild<ItemSlotWidget>("icon");
        if (!fetchChildResult785)
          return std::unexpected(std::move(fetchChildResult785.error()));
        std::move(*fetchChildResult785)->setItem(item);
      }
  }

  auto setLabelResult786 = container->setLabel("nameLabel", item->name());
  if (!setLabelResult786)
    return std::unexpected(std::move(setLabelResult786.error()));
  auto setLabelResult787 = container->setLabel("countLabel", toString(item->count()));
  if (!setLabelResult787)
    return std::unexpected(std::move(setLabelResult787.error()));

  auto rarityName = enumName<RarityNames>(item->rarity());
  if (!rarityName)
    return std::unexpected(std::move(rarityName.error()));
  auto rarityTitle = String(*rarityName).titleCase();
  if (!rarityTitle)
    return std::unexpected(std::move(rarityTitle.error()));
  auto rarityLabel = container->setLabel("rarityLabel", std::move(*rarityTitle));
  if (!rarityLabel)
    return std::unexpected(std::move(rarityLabel.error()));

  if (item->twoHanded())
    {
      auto setLabelResult792 = container->setLabel("handednessLabel", "2-Handed");
      if (!setLabelResult792)
        return std::unexpected(std::move(setLabelResult792.error()));
      
    }
  else
    {
      auto setLabelResult793 = container->setLabel("handednessLabel", "1-Handed");
      if (!setLabelResult793)
        return std::unexpected(std::move(setLabelResult793.error()));
      
    }

  auto instanceValueResult794 = item->instanceValue("fuelAmount", 0);
  if (!instanceValueResult794)
    return std::unexpected(std::move(instanceValueResult794.error()));
  auto toUIntResult795 = std::move(*instanceValueResult794).toUInt();
  if (!toUIntResult795)
    return std::unexpected(std::move(toUIntResult795.error()));
  auto setLabelResult796 = container->setLabel("countLabel", toString(std::move(*toUIntResult795) * item->count()));
  if (!setLabelResult796)
    return std::unexpected(std::move(setLabelResult796.error()));
  auto priceLabelUpdated = container->setLabel("priceLabel", toString(static_cast<int>(item->price())));
  if (!priceLabelUpdated)
    return std::unexpected(std::move(priceLabelUpdated.error()));

  if (auto objectItem = as<ObjectItem>(item)) {
    auto objectDescription = [&]() -> std::expected<void, StarException> {
      auto objectDatabaseResult798 = context.root().objectDatabase();
      if (!objectDatabaseResult798)
        return std::unexpected(std::move(objectDatabaseResult798.error()));
      auto objectName = objectItem->objectName();
      if (!objectName)
        return std::unexpected(std::move(objectName.error()));
      auto objectParameters = objectItem->objectParameters();
      if (!objectParameters)
        return std::unexpected(std::move(objectParameters.error()));
      auto createObjectResult799 = std::move(*objectDatabaseResult798)->createObject(*objectName, *objectParameters);
      if (!createObjectResult799)
        return std::unexpected(std::move(createObjectResult799.error()));
      auto object = std::move(*createObjectResult799);

      {
        auto containsChildResult800 = container->containsChild("objectImage");
        if (!containsChildResult800)
          return std::unexpected(std::move(containsChildResult800.error()));
        if (std::move(*containsChildResult800)) {
          auto cursorHintDrawablesResult801 = object->cursorHintDrawables();
          if (!cursorHintDrawablesResult801)
            return std::unexpected(std::move(cursorHintDrawablesResult801.error()));
          auto drawables = std::move(*cursorHintDrawablesResult801);
          auto fetchChildResult802 = container->fetchChild<ImageWidget>("objectImage");
          if (!fetchChildResult802)
            return std::unexpected(std::move(fetchChildResult802.error()));
          auto setDrawablesResult803 = std::move(*fetchChildResult802)->setDrawables(drawables);
          if (!setDrawablesResult803)
            return std::unexpected(std::move(setDrawablesResult803.error()));
        }
      }

      if (objectItem->tooltipKind() == "container")
        {
          auto instanceValueResult804 = objectItem->instanceValue("slotCount");
          if (!instanceValueResult804)
            return std::unexpected(std::move(instanceValueResult804.error()));
          auto reprResult805 = std::move(*instanceValueResult804).repr();
          if (!reprResult805)
            return std::unexpected(std::move(reprResult805.error()));
          auto setLabelResult806 = container->setLabel("slotCountLabel", strf("Holds {} Items", std::move(*reprResult805)));
          if (!setLabelResult806)
            return std::unexpected(std::move(setLabelResult806.error()));
          
        }

      auto shortDescriptionResult807 = object->shortDescription();
      if (!shortDescriptionResult807)
        return std::unexpected(std::move(shortDescriptionResult807.error()));
      title = std::move(*shortDescriptionResult807);
      auto categoryDisplayNameResult808 = categoryDisplayName(context, object->category());
      if (!categoryDisplayNameResult808)
        return std::unexpected(std::move(categoryDisplayNameResult808.error()));
      subTitle = std::move(*categoryDisplayNameResult808);
      auto descriptionResult809 = object->description();
      if (!descriptionResult809)
        return std::unexpected(std::move(descriptionResult809.error()));
      description = std::move(*descriptionResult809);
      return {};
    }();
    if (!objectDescription) {
      auto logged = context.root().logger().error("Failed to instantiate object for object item tooltip. {}", outputException(ExceptionDiagnostic(objectDescription.error()), false));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    }
  } else {
    {
      auto containsChildResult810 = container->containsChild("objectImage");
      if (!containsChildResult810)
        return std::unexpected(std::move(containsChildResult810.error()));
      if (std::move(*containsChildResult810)) {
        auto fetchChildResult811 = container->fetchChild<ImageWidget>("objectImage");
        if (!fetchChildResult811)
          return std::unexpected(std::move(fetchChildResult811.error()));
        auto objectImage = std::move(*fetchChildResult811);
        if (auto previewable = as<PreviewableItem>(item)) {
          if (is<ArmorItem>(previewable)) {
            objectImage->disableScissoring();
            std::shared_ptr<Player> armorViewer;
            {
              auto jsonResult812 = assets->json("/interface.config:tooltip.previewArmorWith");
              if (!jsonResult812)
                return std::unexpected(std::move(jsonResult812.error()));
              auto toStringResult813 = std::move(*jsonResult812).toString();
              if (!toStringResult813)
                return std::unexpected(std::move(toStringResult813.error()));
              if (std::move(*toStringResult813) == "player")
                armorViewer = viewer;
            }
            auto previewResult814 = previewable->preview(context.tileDrawer(), armorViewer);
            if (!previewResult814)
              return std::unexpected(std::move(previewResult814.error()));
            auto setDrawablesResult815 = objectImage->setDrawables(std::move(*previewResult814));
            if (!setDrawablesResult815)
              return std::unexpected(std::move(setDrawablesResult815.error()));
          } else {
            auto previewResult816 = previewable->preview(context.tileDrawer(), viewer);
            if (!previewResult816)
              return std::unexpected(std::move(previewResult816.error()));
            auto setDrawablesResult817 = objectImage->setDrawables(std::move(*previewResult816));
            if (!setDrawablesResult817)
              return std::unexpected(std::move(setDrawablesResult817.error()));
          }
        } else {
          auto iconDrawablesResult818 = item->iconDrawables();
          if (!iconDrawablesResult818)
            return std::unexpected(std::move(iconDrawablesResult818.error()));
          auto drawables = std::move(*iconDrawablesResult818);
          auto setDrawablesResult819 = objectImage->setDrawables(drawables);
          if (!setDrawablesResult819)
            return std::unexpected(std::move(setDrawablesResult819.error()));
        }
      }
    }
  }

  auto instanceValueResult820 = item->instanceValue("tooltipFields", JsonObject());
  if (!instanceValueResult820)
    return std::unexpected(std::move(instanceValueResult820.error()));
  auto tooltipFields = std::move(*instanceValueResult820);
  {
    auto iterateObjectResult821 = tooltipFields.iterateObject();
    if (!iterateObjectResult821)
      return std::unexpected(std::move(iterateObjectResult821.error()));
    for (auto const& pair : std::move(*iterateObjectResult821)) {
      {
        auto equalsIgnoreCaseResult822 = pair.first.equalsIgnoreCase("subtitle");
        if (!equalsIgnoreCaseResult822)
          return std::unexpected(std::move(equalsIgnoreCaseResult822.error()));
        if (std::move(*equalsIgnoreCaseResult822))
          {
            auto toStringResult823 = pair.second.toString();
            if (!toStringResult823)
              return std::unexpected(std::move(toStringResult823.error()));
            subTitle = std::move(*toStringResult823);
          }
      }
      {
        auto endsWithResult824 = pair.first.endsWith("Label");
        if (!endsWithResult824)
          return std::unexpected(std::move(endsWithResult824.error()));
        if (std::move(*endsWithResult824))
          {
            std::optional<String> conditional827;
            if (pair.second.type() == Json::Type::String) {
              auto toStringResult825 = pair.second.toString();
              if (!toStringResult825)
                return std::unexpected(std::move(toStringResult825.error()));
              conditional827.emplace(std::move(*toStringResult825));
            } else {
              auto reprResult826 = pair.second.repr();
              if (!reprResult826)
                return std::unexpected(std::move(reprResult826.error()));
              conditional827.emplace(std::move(*reprResult826));
            }
            auto setLabelResult828 = container->setLabel(pair.first, std::move(*conditional827));
            if (!setLabelResult828)
              return std::unexpected(std::move(setLabelResult828.error()));
            
          }
      }
      {
        auto endsWithResult829 = pair.first.endsWith("Image");
        if (!endsWithResult829)
          return std::unexpected(std::move(endsWithResult829.error()));
        bool condition831 = static_cast<bool>(std::move(*endsWithResult829));
        if (condition831) {
          auto containsChildResult830 = container->containsChild(pair.first);
          if (!containsChildResult830)
            return std::unexpected(std::move(containsChildResult830.error()));
          condition831 = static_cast<bool>(std::move(*containsChildResult830));
        }
        if (condition831) {
          if (pair.second.isType(Json::Type::String))
            {
              auto fetchChildResult832 = container->fetchChild<ImageWidget>(pair.first);
              if (!fetchChildResult832)
                return std::unexpected(std::move(fetchChildResult832.error()));
              auto toStringResult833 = pair.second.toString();
              if (!toStringResult833)
                return std::unexpected(std::move(toStringResult833.error()));
              auto setImageResult834 = std::move(*fetchChildResult832)->setImage(std::move(*toStringResult833));
              if (!setImageResult834)
                return std::unexpected(std::move(setImageResult834.error()));
              
            }
          else
            {
              List<Drawable> drawables;
              {
                auto toArrayResult835 = pair.second.toArray();
                if (!toArrayResult835)
                  return std::unexpected(std::move(toArrayResult835.error()));
                for (auto const& drawable : std::move(*toArrayResult835))
                  {
                    auto imageMetadataDatabaseResult836 = context.root().imageMetadataDatabase();
                    if (!imageMetadataDatabaseResult836)
                      return std::unexpected(std::move(imageMetadataDatabaseResult836.error()));
                    auto createResult837 = Drawable::create(*std::move(*imageMetadataDatabaseResult836), drawable);
                    if (!createResult837)
                      return std::unexpected(std::move(createResult837.error()));
                    drawables.append(std::move(*createResult837));
                  }
              }
              auto fetchChildResult838 = container->fetchChild<ImageWidget>(pair.first);
              if (!fetchChildResult838)
                return std::unexpected(std::move(fetchChildResult838.error()));
              auto setDrawablesResult839 = std::move(*fetchChildResult838)->setDrawables(drawables);
              if (!setDrawablesResult839)
                return std::unexpected(std::move(setDrawablesResult839.error()));
            }
        }
      }
    }
  }

  if (auto fireable = as<FireableItem>(item)) {
    auto setLabelResult840 = container->setLabel("cooldownTimeLabel", strf("{:.2f}", fireable->cooldownTime()));
    if (!setLabelResult840)
      return std::unexpected(std::move(setLabelResult840.error()));
    auto setLabelResult841 = container->setLabel("windupTimeLabel", strf("{:.2f}", fireable->windupTime()));
    if (!setLabelResult841)
      return std::unexpected(std::move(setLabelResult841.error()));
    auto setLabelResult842 = container->setLabel("speedLabel", strf("{:.2f}", 1.0f / (fireable->cooldownTime() + fireable->windupTime())));
    if (!setLabelResult842)
      return std::unexpected(std::move(setLabelResult842.error()));
  }

  {
    auto containsChildResult843 = container->containsChild("largeImage");
    if (!containsChildResult843)
      return std::unexpected(std::move(containsChildResult843.error()));
    if (std::move(*containsChildResult843)) {
      auto fetchChildResult844 = container->fetchChild<ImageWidget>("largeImage");
      if (!fetchChildResult844)
        return std::unexpected(std::move(fetchChildResult844.error()));
      auto setImageResult845 = std::move(*fetchChildResult844)->setImage(item->largeImage());
      if (!setImageResult845)
        return std::unexpected(std::move(setImageResult845.error()));
    }
  }

  auto setLabelResult846 = container->setLabel("descriptionLabel", description);
  if (!setLabelResult846)
    return std::unexpected(std::move(setLabelResult846.error()));
  auto setLabelResult847 = container->setLabel("friendlyNameLabel", title);
  if (!setLabelResult847)
    return std::unexpected(std::move(setLabelResult847.error()));

  {
    auto containsChildResult848 = container->containsChild("statusList");
    if (!containsChildResult848)
      return std::unexpected(std::move(containsChildResult848.error()));
    if (std::move(*containsChildResult848)) {
      auto fetchChildResult849 = container->fetchChild<ListWidget>("statusList");
      if (!fetchChildResult849)
        return std::unexpected(std::move(fetchChildResult849.error()));
      auto statusList = std::move(*fetchChildResult849);
      if (auto statusEffects = as<StatusEffectItem>(item)) {
        auto effects = statusEffects->statusEffects();
        if (!effects)
          return std::unexpected(std::move(effects.error()));
        for (auto const& effect : *effects) {
          auto effectDescription = describePersistentEffect(context, statusList, effect);
          if (!effectDescription)
            return std::unexpected(std::move(effectDescription.error()));
        }
      }
    }
  }

  {
    auto instanceValueResult851 = item->instanceValue("acceptsAugmentType", false);
    if (!instanceValueResult851)
      return std::unexpected(std::move(instanceValueResult851.error()));
    if (std::move(*instanceValueResult851)) {
      {
        auto fetchChildResult852 = container->fetchChild<LabelWidget>("augmentNameLabel");
        if (!fetchChildResult852)
          return std::unexpected(std::move(fetchChildResult852.error()));
        if (auto augmentLabel = std::move(*fetchChildResult852)) {
          {
            auto instanceValueResult853 = item->instanceValue("currentAugment");
            if (!instanceValueResult853)
              return std::unexpected(std::move(instanceValueResult853.error()));
            if (auto currentAugment = std::move(*instanceValueResult853)) {
              auto getStringResult854 = currentAugment.getString("displayName", "???");
              if (!getStringResult854)
                return std::unexpected(std::move(getStringResult854.error()));
              auto setLabelResult855 = container->setLabel("augmentNameLabel", std::move(*getStringResult854));
              if (!setLabelResult855)
                return std::unexpected(std::move(setLabelResult855.error()));
              {
                auto fetchChildResult856 = container->fetchChild<ImageWidget>("augmentIconImage");
                if (!fetchChildResult856)
                  return std::unexpected(std::move(fetchChildResult856.error()));
                if (auto augmentIcon = std::move(*fetchChildResult856))
                  {
                    auto getStringResult857 = currentAugment.getString("displayIcon", "");
                    if (!getStringResult857)
                      return std::unexpected(std::move(getStringResult857.error()));
                    auto setImageResult858 = augmentIcon->setImage(std::move(*getStringResult857));
                    if (!setImageResult858)
                      return std::unexpected(std::move(setImageResult858.error()));
                    
                  }
              }
              augmentLabel->setColor(Color::White);
            } else {
              auto setLabelResult859 = container->setLabel("augmentNameLabel", "NO AUGMENT INSERTED");
              if (!setLabelResult859)
                return std::unexpected(std::move(setLabelResult859.error()));
              {
                auto fetchChildResult860 = container->fetchChild<ImageWidget>("augmentIconImage");
                if (!fetchChildResult860)
                  return std::unexpected(std::move(fetchChildResult860.error()));
                if (auto augmentIcon = std::move(*fetchChildResult860))
                  {
                    auto setImageResult861 = augmentIcon->setImage("");
                    if (!setImageResult861)
                      return std::unexpected(std::move(setImageResult861.error()));
                    
                  }
              }
              augmentLabel->setColor(Color::Gray);
            }
          }
        }
      }
    }
  }

  auto setLabelResult862 = container->setLabel("title", title);
  if (!setLabelResult862)
    return std::unexpected(std::move(setLabelResult862.error()));
  auto setLabelResult863 = container->setLabel("subTitle", subTitle);
  if (!setLabelResult863)
    return std::unexpected(std::move(setLabelResult863.error()));
  {
    auto containsChildResult864 = container->containsChild("titleIcon");
    if (!containsChildResult864)
      return std::unexpected(std::move(containsChildResult864.error()));
    if (std::move(*containsChildResult864)) {
      auto fetchChildResult865 = container->fetchChild<ItemSlotWidget>("titleIcon");
      if (!fetchChildResult865)
        return std::unexpected(std::move(fetchChildResult865.error()));
      auto titleIcon = std::move(*fetchChildResult865);
      titleIcon->setItem(item);
    }
  }
  return {};
}


std::expected<void, StarException> ItemTooltipBuilder::describePersistentEffect(
    GuiContext& context, std::shared_ptr<ListWidget> const& container, PersistentStatusEffect const& effect) {
  if (std::holds_alternative<UniqueStatusEffect>(effect)) {
    auto statusEffectDatabaseResult866 = context.root().statusEffectDatabase();
    if (!statusEffectDatabaseResult866)
      return std::unexpected(std::move(statusEffectDatabaseResult866.error()));
    auto statusEffectDatabase = std::move(*statusEffectDatabaseResult866);
    auto uniqueEffectConfigResult867 = statusEffectDatabase->uniqueEffectConfig(std::get<UniqueStatusEffect>(effect));
    if (!uniqueEffectConfigResult867)
      return std::unexpected(std::move(uniqueEffectConfigResult867.error()));
    auto effectConfig = std::move(*uniqueEffectConfigResult867);
    if (effectConfig.icon) {
      auto addItemResult868 = container->addItem();
      if (!addItemResult868)
        return std::unexpected(std::move(addItemResult868.error()));
      auto listItem = std::move(*addItemResult868);
      auto setLabelResult869 = listItem->setLabel("statusLabel", effectConfig.label);
      if (!setLabelResult869)
        return std::unexpected(std::move(setLabelResult869.error()));
      auto fetchChildResult870 = listItem->fetchChild<ImageWidget>("statusImage");
      if (!fetchChildResult870)
        return std::unexpected(std::move(fetchChildResult870.error()));
      auto setImageResult871 = std::move(*fetchChildResult870)->setImage(*effectConfig.icon);
      if (!setImageResult871)
        return std::unexpected(std::move(setImageResult871.error()));
    }
  } else if (std::holds_alternative<StatModifier>(effect)) {
    auto const& modifierEffect = std::get<StatModifier>(effect);
    auto assetsResult872 = context.root().assets();
    if (!assetsResult872)
      return std::unexpected(std::move(assetsResult872.error()));
    auto jsonResult873 = std::move(*assetsResult872)->json("/interface/stats/stats.config");
    if (!jsonResult873)
      return std::unexpected(std::move(jsonResult873.error()));
    auto statsConfig = std::move(*jsonResult873);
    if (std::holds_alternative<StatBaseMultiplier>(modifierEffect)) {
      auto const& baseMultiplier = std::get<StatBaseMultiplier>(modifierEffect);
      {
        auto containsResult874 = statsConfig.contains(baseMultiplier.statName);
        if (!containsResult874)
          return std::unexpected(std::move(containsResult874.error()));
        if (std::move(*containsResult874)) {
          auto addItemResult875 = container->addItem();
          if (!addItemResult875)
            return std::unexpected(std::move(addItemResult875.error()));
          auto listItem = std::move(*addItemResult875);
          auto fetchChildResult876 = listItem->fetchChild<ImageWidget>("statusImage");
          if (!fetchChildResult876)
            return std::unexpected(std::move(fetchChildResult876.error()));
          auto getResult877 = statsConfig.get(baseMultiplier.statName);
          if (!getResult877)
            return std::unexpected(std::move(getResult877.error()));
          auto getStringResult878 = std::move(*getResult877).getString("icon");
          if (!getStringResult878)
            return std::unexpected(std::move(getStringResult878.error()));
          auto setImageResult879 = std::move(*fetchChildResult876)
                      ->setImage(std::move(*getStringResult878));
          if (!setImageResult879)
            return std::unexpected(std::move(setImageResult879.error()));
          auto setLabelResult880 = listItem->setLabel("statusLabel", strf("{:.1f}%", (baseMultiplier.baseMultiplier - 1) * 100));
          if (!setLabelResult880)
            return std::unexpected(std::move(setLabelResult880.error()));
        }
      }
    } else if (std::holds_alternative<StatValueModifier>(modifierEffect)) {
      auto const& valueModifier = std::get<StatValueModifier>(modifierEffect);
      {
        auto containsResult881 = statsConfig.contains(valueModifier.statName);
        if (!containsResult881)
          return std::unexpected(std::move(containsResult881.error()));
        if (std::move(*containsResult881)) {
          auto addItemResult882 = container->addItem();
          if (!addItemResult882)
            return std::unexpected(std::move(addItemResult882.error()));
          auto listItem = std::move(*addItemResult882);
          auto fetchChildResult883 = listItem->fetchChild<ImageWidget>("statusImage");
          if (!fetchChildResult883)
            return std::unexpected(std::move(fetchChildResult883.error()));
          auto getResult884 = statsConfig.get(valueModifier.statName);
          if (!getResult884)
            return std::unexpected(std::move(getResult884.error()));
          auto getStringResult885 = std::move(*getResult884).getString("icon");
          if (!getStringResult885)
            return std::unexpected(std::move(getStringResult885.error()));
          auto setImageResult886 = std::move(*fetchChildResult883)
                      ->setImage(std::move(*getStringResult885));
          if (!setImageResult886)
            return std::unexpected(std::move(setImageResult886.error()));
          auto setLabelResult887 = listItem->setLabel("statusLabel", strf("{}{:.2f}", valueModifier.value < 0 ? "-" : "", valueModifier.value));
          if (!setLabelResult887)
            return std::unexpected(std::move(setLabelResult887.error()));
        }
      }
    } else if (std::holds_alternative<StatEffectiveMultiplier>(modifierEffect)) {
      auto const& effectiveMultiplier = std::get<StatEffectiveMultiplier>(modifierEffect);
      {
        auto containsResult888 = statsConfig.contains(effectiveMultiplier.statName);
        if (!containsResult888)
          return std::unexpected(std::move(containsResult888.error()));
        if (std::move(*containsResult888)) {
          auto addItemResult889 = container->addItem();
          if (!addItemResult889)
            return std::unexpected(std::move(addItemResult889.error()));
          auto listItem = std::move(*addItemResult889);
          auto fetchChildResult890 = listItem->fetchChild<ImageWidget>("statusImage");
          if (!fetchChildResult890)
            return std::unexpected(std::move(fetchChildResult890.error()));
          auto getResult891 = statsConfig.get(effectiveMultiplier.statName);
          if (!getResult891)
            return std::unexpected(std::move(getResult891.error()));
          auto getStringResult892 = std::move(*getResult891).getString("icon");
          if (!getStringResult892)
            return std::unexpected(std::move(getStringResult892.error()));
          auto setImageResult893 = std::move(*fetchChildResult890)
                      ->setImage(std::move(*getStringResult892));
          if (!setImageResult893)
            return std::unexpected(std::move(setImageResult893.error()));
          auto setLabelResult894 = listItem->setLabel("statusLabel", strf("{:.1f}%", (effectiveMultiplier.effectiveMultiplier - 1) * 100));
          if (!setLabelResult894)
            return std::unexpected(std::move(setLabelResult894.error()));
        }
      }
    }
  }
  return {};
}


}
