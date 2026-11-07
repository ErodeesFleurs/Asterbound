import std;
import star.format;
import star.memory;
import star.json;
import star.casting;
import star.input_event;
import star.color;
import star.font;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;
import star.ordered_map;
import star.io_device;
import star.file;
import star.ref_ptr;
import star.logging;
import star.version;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;


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
import star.pane;


import star.mods_menu;
import star.widget_parsing;
import star.gui_reader;
import star.label_widget;
import star.button_group;
import star.button_widget;
import star.list_widget;
import star.image_widget;

namespace Star {

ModsMenu::ModsMenu(GuiContext& guiContext) : Pane(guiContext) {}

std::expected<std::shared_ptr<ModsMenu>, StarException> ModsMenu::create(GuiContext& guiContext) {
  auto menu = std::shared_ptr<ModsMenu>(new ModsMenu(guiContext));
  if (auto result = menu->initializePane(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = menu->init(); !result)
    return std::unexpected(std::move(result.error()));
  return menu;
}

std::expected<void, StarException> ModsMenu::init() {
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));

  GuiReader reader(context());
  reader.registerCallback("linkbutton", [this](Widget&) { return openLink(); });
  reader.registerCallback("workshopbutton", [this](Widget&) { return openWorkshop(); });
  auto layout = (*assets)->json("/interface/modsmenu/modsmenu.config:paneLayout");
  if (!layout)
    return std::unexpected(std::move(layout.error()));
  if (auto result = reader.construct(*layout, *this); !result)
    return std::unexpected(std::move(result.error()));

  m_assetsSources = (*assets)->assetSources();
  auto modList = requireChild<ListWidget>("mods.list");
  if (!modList)
    return std::unexpected(std::move(modList.error()));
  m_modList = std::move(*modList);
  for (auto const& assetsSource : m_assetsSources) {
    auto metadata = (*assets)->assetSourceMetadata(assetsSource);
    if (!metadata)
      return std::unexpected(std::move(metadata.error()));
    auto listItem = m_modList->addItem();
    if (!listItem)
      return std::unexpected(std::move(listItem.error()));
    auto modName = (*listItem)->requireChild<LabelWidget>("name");
    if (!modName)
      return std::unexpected(std::move(modName.error()));
    auto name = bestModName(*metadata, assetsSource);
    if (!name)
      return std::unexpected(std::move(name.error()));
    if (auto result = (*modName)->setText(*name); !result)
      return std::unexpected(std::move(result.error()));
    if (auto iconImage = metadata->find_ref("icon")) {
      auto image = iconImage->get().toString();
      if (!image)
        return std::unexpected(std::move(image.error()));
      auto icon = (*listItem)->requireChild<ImageWidget>("icon");
      if (!icon)
        return std::unexpected(std::move(icon.error()));
      if (auto result = (*icon)->setImage(*image); !result)
        return std::unexpected(std::move(result.error()));
    }
  }

  for (auto const& [name, member] : {
      std::pair{String("modname"), &ModsMenu::m_modName},
      std::pair{String("modauthor"), &ModsMenu::m_modAuthor},
      std::pair{String("modversion"), &ModsMenu::m_modVersion},
      std::pair{String("modpath"), &ModsMenu::m_modPath},
      std::pair{String("moddescription"), &ModsMenu::m_modDescription}}) {
    auto label = findChild<LabelWidget>(name);
    if (!label)
      return std::unexpected(std::move(label.error()));
    if (!*label)
      return std::unexpected(GuiException::format("Required mod label '{}' was not found", name));
    this->*member = std::move(*label);
  }
  auto linkButton = requireChild<ButtonWidget>("linkbutton");
  if (!linkButton)
    return std::unexpected(std::move(linkButton.error()));
  m_linkButton = std::move(*linkButton);
  auto copyLinkButton = requireChild<ButtonWidget>("copylinkbutton");
  if (!copyLinkButton)
    return std::unexpected(std::move(copyLinkButton.error()));
  m_copyLinkButton = std::move(*copyLinkButton);
  auto linkLabel = requireChild<LabelWidget>("linklabel");
  if (!linkLabel)
    return std::unexpected(std::move(linkLabel.error()));
  auto copyLinkLabel = requireChild<LabelWidget>("copylinklabel");
  if (!copyLinkLabel)
    return std::unexpected(std::move(copyLinkLabel.error()));
  auto workshopLinkButton = requireChild<ButtonWidget>("workshopbutton");
  if (!workshopLinkButton)
    return std::unexpected(std::move(workshopLinkButton.error()));
  bool hasDesktopService = static_cast<bool>(context().applicationController()->desktopService());
  (*workshopLinkButton)->setEnabled(hasDesktopService);
  if (auto result = m_linkButton->setVisibility(hasDesktopService); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_copyLinkButton->setVisibility(!hasDesktopService); !result)
    return std::unexpected(std::move(result.error()));
  m_linkButton->setEnabled(false);
  m_copyLinkButton->setEnabled(false);
  if (auto result = (*linkLabel)->setVisibility(hasDesktopService); !result)
    return std::unexpected(std::move(result.error()));
  return (*copyLinkLabel)->setVisibility(!hasDesktopService);
}

std::expected<void, StarException> ModsMenu::update(float dt) {
  if (auto result = Pane::update(dt); !result)
    return std::unexpected(std::move(result.error()));

  std::size_t selectedItem = m_modList->selectedItem();
  if (selectedItem == NPos) {
    for (auto label : {std::ref(*m_modName), std::ref(*m_modAuthor), std::ref(*m_modVersion), std::ref(*m_modPath), std::ref(*m_modDescription)}) {
      if (auto result = label.get().setText(""); !result)
        return std::unexpected(std::move(result.error()));
    }
  } else {
    auto source = m_assetsSources.at(selectedItem);
    if (!source)
      return std::unexpected(std::move(source.error()));
    auto assets = context().root().assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto metadata = (*assets)->assetSourceMetadata(source->get());
    if (!metadata)
      return std::unexpected(std::move(metadata.error()));
    auto name = bestModName(*metadata, source->get());
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto author = metadata->value("author", "No Author Set").toString();
    if (!author)
      return std::unexpected(std::move(author.error()));
    auto version = metadata->value("version", "No Version Set").printString();
    if (!version)
      return std::unexpected(std::move(version.error()));
    auto description = metadata->value("description", "").toString();
    if (!description)
      return std::unexpected(std::move(description.error()));
    auto link = metadata->value("link", "").toString();
    if (!link)
      return std::unexpected(std::move(link.error()));
    if (auto result = m_modName->setText(*name); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_modAuthor->setText(*author); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_modVersion->setText(*version); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_modPath->setText(source->get()); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_modDescription->setText(*description); !result)
      return std::unexpected(std::move(result.error()));
    m_linkButton->setEnabled(!link->empty());
    m_copyLinkButton->setEnabled(!link->empty());
  }
  return {};
}

std::expected<String, StarException> ModsMenu::bestModName(JsonObject const& metadata, String const& sourcePath) {
  if (auto value = metadata.find_ref("friendlyName"))
    return value->get().toString();
  if (auto value = metadata.find_ref("name"))
    return value->get().toString();
  String baseName = File::base_name(sourcePath);
  auto contains = baseName.contains(".");
  if (!contains)
    return std::unexpected(std::move(contains.error()));
  if (*contains) {
    if (auto result = baseName.rextract("."); !result)
      return std::unexpected(std::move(result.error()));
  }
  return baseName;
}

std::expected<void, StarException> ModsMenu::openLink() {
  std::size_t selectedItem = m_modList->selectedItem();
  if (selectedItem == NPos)
    return {};
  auto source = m_assetsSources.at(selectedItem);
  if (!source)
    return std::unexpected(std::move(source.error()));
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto metadata = (*assets)->assetSourceMetadata(source->get());
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto link = metadata->value("link", "").toString();
  if (!link)
    return std::unexpected(std::move(link.error()));
  if (link->empty())
    return {};
  if (auto desktopService = context().applicationController()->desktopService())
    desktopService->openUrl(*link);
  else
    context().setClipboard(*link);
  return {};
}

std::expected<void, StarException> ModsMenu::openWorkshop() {
  if (auto desktopService = context().applicationController()->desktopService()) {
    auto assets = context().root().assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto config = (*assets)->json("/interface/modsmenu/modsmenu.config:workshopLink");
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto link = config->toString();
    if (!link)
      return std::unexpected(std::move(link.error()));
    desktopService->openUrl(*link);
  }
  return {};
}

}
