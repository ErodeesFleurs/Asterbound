import std;
import star.ai_database;
import star.ai_interface;
import star.ai_types;
import star.animation;
import star.assets;
import star.audio;
import star.button_widget;
import star.canvas_widget;
import star.client_context;
import star.drawable;
import star.format;
import star.gui_context;
import star.gui_reader;
import star.image_stretch_widget;
import star.image_widget;
import star.json;
import star.label_widget;
import star.list;
import star.list_widget;
import star.mixer;
import star.ordered_set;
import star.pane;
import star.player;
import star.player_companions;
import star.player_types;
import star.root;
import star.stack_widget;
import star.string;
import star.text;
import star.universe_client;
import star.uuid;
import star.vector;
import star.warping;
import star.widget;
import star.widget_parsing;

namespace Star {

AiInterface::AiInterface(ConstructionKey, GuiContext& gui_context, std::shared_ptr<UniverseClient> client,
    std::shared_ptr<AiDatabase const> database, Animation static_animation, Animation scanline_animation, Animation face_animation)
  : Pane(gui_context), m_current_page(AiPages::StatusPage), m_client(std::move(client)), m_ai_database(std::move(database)),
    m_static_animation(std::move(static_animation)), m_scanline_animation(std::move(scanline_animation)),
    m_face_animation("", std::move(face_animation)) {}

std::expected<std::shared_ptr<AiInterface>, StarException> AiInterface::create(GuiContext& gui_context, std::shared_ptr<UniverseClient> client) {
  auto database = gui_context.root().ai_database();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto static_animation = (*database)->static_animation(client->mainPlayer()->shipSpecies());
  if (!static_animation)
    return std::unexpected(std::move(static_animation.error()));
  auto face_animation = Animation::create(gui_context.root());
  if (!face_animation)
    return std::unexpected(std::move(face_animation.error()));
  auto scanline_animation = (*database)->scanline_animation();
  auto pane = std::make_shared<AiInterface>(ConstructionKey{}, gui_context, std::move(client), std::move(*database),
      std::move(*static_animation), std::move(scanline_animation), std::move(*face_animation));
  auto initialized = pane->initializePane();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  initialized = pane->initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return pane;
}

std::expected<void, StarException> AiInterface::initialize() {
  auto assets_result = context().root().assets();
  if (!assets_result)
    return std::unexpected(std::move(assets_result.error()));
  auto const& assets = *assets_result;
  auto config = assets->json("/interface/ai/ai.config");
  if (!config)
    return std::unexpected(std::move(config.error()));
  GuiReader reader(context());
  reader.registerCallback("close", [this](Widget&) -> std::expected<void, StarException> { dismiss(); return {}; });
  reader.registerCallback("missionItemList", [this](Widget&) { return select_mission(); });
  reader.registerCallback("startMission", [this](Widget&) { return start_mission(); });
  reader.registerCallback("crewItemList", [this](Widget&) { return select_recruit(); });
  reader.registerCallback("dismissRecruit", [this](Widget&) { return dismiss_recruit(); });
  reader.registerCallback("showMissions", [this](Widget&) { return show_missions(ListAction::Open); });
  reader.registerCallback("showCrew", [this](Widget&) { return show_crew(ListAction::Open); });
  reader.registerCallback("goBack", [this](Widget&) { return go_back(); });
  auto layout = config->get("guiConfig");
  if (!layout)
    return std::unexpected(std::move(layout.error()));
  auto constructed = reader.construct(*layout, *this);
  if (!constructed)
    return std::unexpected(std::move(constructed.error()));

  auto main_stack = fetchChild<StackWidget>("mainStack");
  if (!main_stack) return std::unexpected(std::move(main_stack.error()));
  m_main_stack = std::move(*main_stack);
  auto mission_stack = findChild<StackWidget>("missionStack");
  if (!mission_stack) return std::unexpected(std::move(mission_stack.error()));
  m_mission_stack = std::move(*mission_stack);
  auto crew_stack = findChild<StackWidget>("crewStack");
  if (!crew_stack) return std::unexpected(std::move(crew_stack.error()));
  m_crew_stack = std::move(*crew_stack);
  auto left_padding = config->get_int("breadcrumbLeftPadding");
  if (!left_padding) return std::unexpected(std::move(left_padding.error()));
  m_breadcrumb_left_padding = *left_padding;
  auto right_padding = config->get_int("breadcrumbRightPadding");
  if (!right_padding) return std::unexpected(std::move(right_padding.error()));
  m_breadcrumb_right_padding = *right_padding;
  auto home_bg = fetchChild<ImageStretchWidget>("homeBreadcrumbBg");
  if (!home_bg) return std::unexpected(std::move(home_bg.error()));
  m_home_breadcrumb_background = std::move(*home_bg);
  auto page_bg = fetchChild<ImageStretchWidget>("pageBreadcrumbBg");
  if (!page_bg) return std::unexpected(std::move(page_bg.error()));
  m_page_breadcrumb_background = std::move(*page_bg);
  auto item_bg = fetchChild<ImageStretchWidget>("itemBreadcrumbBg");
  if (!item_bg) return std::unexpected(std::move(item_bg.error()));
  m_item_breadcrumb_background = std::move(*item_bg);
  auto home = fetchChild<LabelWidget>("homeBreadcrumb");
  if (!home) return std::unexpected(std::move(home.error()));
  m_home_breadcrumb_widget = std::move(*home);
  auto page = fetchChild<LabelWidget>("pageBreadcrumb");
  if (!page) return std::unexpected(std::move(page.error()));
  m_page_breadcrumb_widget = std::move(*page);
  auto item = fetchChild<LabelWidget>("itemBreadcrumb");
  if (!item) return std::unexpected(std::move(item.error()));
  m_item_breadcrumb_widget = std::move(*item);
  auto canvas = findChild<CanvasWidget>("aiFaceCanvas");
  if (!canvas) return std::unexpected(std::move(canvas.error()));
  m_ai_face_canvas_widget = std::move(*canvas);
  auto missions = findChild<ListWidget>("missionItemList");
  if (!missions) return std::unexpected(std::move(missions.error()));
  m_mission_list_widget = std::move(*missions);
  auto crew = findChild<ListWidget>("crewItemList");
  if (!crew) return std::unexpected(std::move(crew.error()));
  m_crew_list_widget = std::move(*crew);
  auto show_missions = findChild<ButtonWidget>("showMissions");
  if (!show_missions) return std::unexpected(std::move(show_missions.error()));
  m_show_missions_button = std::move(*show_missions);
  auto show_crew = findChild<ButtonWidget>("showCrew");
  if (!show_crew) return std::unexpected(std::move(show_crew.error()));
  m_show_crew_button = std::move(*show_crew);
  auto back = findChild<ButtonWidget>("backButton");
  if (!back) return std::unexpected(std::move(back.error()));
  m_back_button = std::move(*back);
  auto mission_name = findChild<LabelWidget>("missionName");
  if (!mission_name) return std::unexpected(std::move(mission_name.error()));
  m_mission_name_label = std::move(*mission_name);
  auto mission_icon = findChild<ImageWidget>("missionIcon");
  if (!mission_icon) return std::unexpected(std::move(mission_icon.error()));
  m_mission_icon = std::move(*mission_icon);
  auto start = findChild<ButtonWidget>("startMission");
  if (!start) return std::unexpected(std::move(start.error()));
  auto recruit_name = findChild<LabelWidget>("recruitName");
  if (!recruit_name) return std::unexpected(std::move(recruit_name.error()));
  m_recruit_name_label = std::move(*recruit_name);
  auto recruit_icon = findChild<ImageWidget>("recruitIcon");
  if (!recruit_icon) return std::unexpected(std::move(recruit_icon.error()));
  m_recruit_icon = std::move(*recruit_icon);
  auto dismiss_recruit = findChild<ButtonWidget>("dismissRecruit");
  if (!dismiss_recruit) return std::unexpected(std::move(dismiss_recruit.error()));
  m_species = m_client->mainPlayer()->shipSpecies();
  auto mission_breadcrumb = config->get_string("missionBreadcrumbText");
  if (!mission_breadcrumb) return std::unexpected(std::move(mission_breadcrumb.error()));
  m_mission_breadcrumb_text = std::move(*mission_breadcrumb);
  auto mission_deploy = config->get_string("missionDeployText");
  if (!mission_deploy) return std::unexpected(std::move(mission_deploy.error()));
  m_mission_deploy_text = std::move(*mission_deploy);
  auto crew_breadcrumb = config->get_string("crewBreadcrumbText");
  if (!crew_breadcrumb) return std::unexpected(std::move(crew_breadcrumb.error()));
  m_crew_breadcrumb_text = std::move(*crew_breadcrumb);
  auto recruit_name_text = config->get_string("defaultRecruitName");
  if (!recruit_name_text) return std::unexpected(std::move(recruit_name_text.error()));
  m_default_recruit_name = std::move(*recruit_name_text);
  auto description = config->get_string("defaultRecruitDescription");
  if (!description) return std::unexpected(std::move(description.error()));
  m_default_recruit_description = std::move(*description);
  return {};
}

std::expected<void, StarException> AiInterface::update(float dt) {
  if (!m_client->playerOnOwnShip())
    dismiss();
  auto result = Pane::update(dt);
  if (!result) return std::unexpected(std::move(result.error()));
  m_static_animation.update(dt);
  m_scanline_animation.update(dt);

  static String const idle("idle");
  if (m_speech) {
    auto& speech = *m_speech;
    if (speech.progress < speech.length)
      speech.progress += speech.speed_modifier * m_ai_database->characters_per_second() * dt;
    bool speaking = speech.progress < speech.length;
    unsigned visible = speaking ? static_cast<unsigned>(std::max(0.0f, speech.progress)) : speech.length;
    if (speech.label->getTextCharLimit() != visible) {
      result = speech.label->setTextCharLimit(visible);
      if (!result) return std::unexpected(std::move(result.error()));
    }
    result = set_face_animation(speaking ? speech.animation : idle);
    if (!result) return std::unexpected(std::move(result.error()));
    if (speaking) {
      if (!m_chatter_sound || m_chatter_sound->finished()) {
        auto assets = context().root().assets();
        if (!assets) return std::unexpected(std::move(assets.error()));
        auto sound_config = (*assets)->json("/interface/ai/ai.config:chatterSound");
        if (!sound_config) return std::unexpected(std::move(sound_config.error()));
        auto sound_name = sound_config->to_string();
        if (!sound_name) return std::unexpected(std::move(sound_name.error()));
        auto sound = (*assets)->audio(*sound_name);
        if (!sound) return std::unexpected(std::move(sound.error()));
        auto chatter = AudioInstance::create(**sound);
        if (!chatter) return std::unexpected(std::move(chatter.error()));
        m_chatter_sound = std::move(*chatter);
        m_chatter_sound->set_loops(-1);
        auto played = context().playAudio(m_chatter_sound);
        if (!played)
          return std::unexpected(std::move(played.error()));
      }
    } else if (m_chatter_sound) {
      m_chatter_sound->stop();
      m_chatter_sound.reset();
    }
    m_face_animation.second.update(dt * speech.speed_modifier);
  } else {
    result = set_face_animation(idle);
    if (!result) return std::unexpected(std::move(result.error()));
    m_face_animation.second.update(dt);
    if (m_chatter_sound) {
      m_chatter_sound->stop();
      m_chatter_sound.reset();
    }
  }

  if (m_current_page == AiPages::MissionList || m_current_page == AiPages::MissionPage)
    result = show_missions(ListAction::Refresh);
  else if (m_current_page == AiPages::CrewList || m_current_page == AiPages::CrewPage)
    result = show_crew(ListAction::Refresh);
  if (!result) return std::unexpected(std::move(result.error()));
  m_ai_face_canvas_widget->clear();
  auto face = m_face_animation.second.drawable(1.0f);
  if (!face) return std::unexpected(std::move(face.error()));
  m_ai_face_canvas_widget->drawDrawable(std::move(*face), Vec2F(0, 0));
  auto static_drawable = m_static_animation.drawable(1.0f);
  if (!static_drawable) return std::unexpected(std::move(static_drawable.error()));
  m_ai_face_canvas_widget->drawDrawable(std::move(*static_drawable), Vec2F(0, 0));
  auto scanline = m_scanline_animation.drawable(1.0f);
  if (!scanline) return std::unexpected(std::move(scanline.error()));
  m_ai_face_canvas_widget->drawDrawable(std::move(*scanline), Vec2F(0, 0));
  return {};
}

std::expected<void, StarException> AiInterface::update_breadcrumbs() {
  // Home breadcrumb
  auto width = m_home_breadcrumb_widget->size()[0];
  width += m_breadcrumb_left_padding + m_breadcrumb_right_padding;
  m_home_breadcrumb_background->setSize(Vec2I(width, m_home_breadcrumb_background->size()[1]));

  // Middle breadcrumb
  if (m_current_page != AiPages::StatusPage) {
    Vec2I page_position = m_home_breadcrumb_background->position() + Vec2I(width, 0);
    m_page_breadcrumb_widget->setPosition(Vec2I(page_position[0] + m_breadcrumb_left_padding, m_home_breadcrumb_widget->position()[1]));
    m_page_breadcrumb_background->setPosition(page_position - Vec2I(m_breadcrumb_right_padding, 0));
    width = m_page_breadcrumb_widget->size()[0] + (2 * m_breadcrumb_right_padding) + m_breadcrumb_left_padding; // Add right padding twice because of the overlap to the left
    m_page_breadcrumb_background->setSize(Vec2I(width, m_page_breadcrumb_background->size()[1]));

    // End breadcrumb
    if (m_current_page == AiPages::MissionPage || m_current_page == AiPages::CrewPage) {
      Vec2I item_position = m_page_breadcrumb_background->position() + Vec2I(width, 0);
      m_item_breadcrumb_widget->setPosition(Vec2I(item_position[0] + m_breadcrumb_left_padding, m_home_breadcrumb_widget->position()[1]));
      m_item_breadcrumb_background->setPosition(item_position - Vec2I(m_breadcrumb_right_padding, 0));
      width = m_item_breadcrumb_widget->size()[0] + (2 * m_breadcrumb_right_padding) + m_breadcrumb_left_padding;
      m_item_breadcrumb_background->setSize(Vec2I(width, m_item_breadcrumb_background->size()[1]));
    }
  }

  bool home = m_current_page == AiPages::StatusPage;
  auto result = m_show_crew_button->setVisibility(home);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_show_missions_button->setVisibility(home);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_back_button->setVisibility(!home);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_page_breadcrumb_background->setVisibility(!home);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_page_breadcrumb_widget->setVisibility(m_current_page != AiPages::StatusPage);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_item_breadcrumb_background->setVisibility(m_current_page == AiPages::MissionPage || m_current_page == AiPages::CrewPage);
  if (!result) return std::unexpected(std::move(result.error()));
  return m_item_breadcrumb_widget->setVisibility(m_current_page == AiPages::MissionPage || m_current_page == AiPages::CrewPage);
}

std::expected<void, StarException> AiInterface::displayed() {
  if (!m_client->playerOnOwnShip())
    return {};
  auto result = Pane::displayed();
  if (!result) return std::unexpected(std::move(result.error()));
  return show_status();
}

std::expected<void, StarException> AiInterface::dismissed() {
  if (m_chatter_sound) {
    m_chatter_sound->stop();
    m_chatter_sound.reset();
  }
  m_speech.reset();
  m_selected_mission = {};
  m_selected_recruit = {};
  return Pane::dismissed();
}

std::expected<void, StarException> AiInterface::show_status() {
  auto speech = m_ai_database->ship_status(m_client->client_context()->ship_level());
  if (!speech) return std::unexpected(std::move(speech.error()));
  m_current_page = AiPages::StatusPage;
  auto result = set_current_speech("shipStatusText", std::move(*speech));
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_main_stack->showPage(0);
  if (!result) return std::unexpected(std::move(result.error()));
  return update_breadcrumbs();
}

std::expected<void, StarException> AiInterface::show_missions(ListAction action) {
  auto const& state = m_client->mainPlayer()->ai_state();
  if (action == ListAction::Refresh && m_current_page == AiPages::MissionPage && m_selected_mission
      && (state.available_missions.contains(*m_selected_mission) || state.completed_missions.contains(*m_selected_mission)))
    return {};
  if (!m_mission_rows || !std::ranges::equal(state.available_missions, m_mission_rows->available)
      || !std::ranges::equal(state.completed_missions, m_mission_rows->completed)) {
    m_mission_rows.reset();
    auto populated = populate_missions(state);
    if (!populated) return std::unexpected(std::move(populated.error()));
    m_mission_rows.emplace(state.available_missions.values(), state.completed_missions.values());
  } else if (action == ListAction::Refresh && m_current_page == AiPages::MissionList) {
    return {};
  }
  m_current_page = AiPages::MissionList;
  m_speech.reset();
  auto result = m_page_breadcrumb_widget->setText(m_mission_breadcrumb_text);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_mission_list_widget->clearSelected();
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_main_stack->showPage(1);
  if (!result) return std::unexpected(std::move(result.error()));
  if (state.available_missions.empty() && state.completed_missions.empty()) {
    result = m_mission_stack->showPage(0);
    if (!result) return std::unexpected(std::move(result.error()));
    result = set_current_speech("noMissionsText", m_ai_database->no_missions_speech());
  } else {
    result = m_mission_stack->showPage(1);
  }
  if (!result) return std::unexpected(std::move(result.error()));
  return update_breadcrumbs();
}

std::expected<void, StarException> AiInterface::select_mission() {
  std::size_t selected_item = m_mission_list_widget->selectedItem();
  m_selected_mission = {};
  if (!m_mission_rows)
    return {};
  if (selected_item < m_mission_rows->available.size())
    m_selected_mission = m_mission_rows->available[selected_item];
  else if (selected_item - m_mission_rows->available.size() < m_mission_rows->completed.size())
    m_selected_mission = m_mission_rows->completed[selected_item - m_mission_rows->available.size()];
  if (!m_selected_mission)
    return {};
  auto mission = m_ai_database->mission(*m_selected_mission);
  if (!mission) return std::unexpected(std::move(mission.error()));
  auto const& mission_text = m_ai_database->mission_text(mission->get(), m_species);
  m_current_page = AiPages::MissionPage;
  auto result = set_current_speech("missionText", mission_text.select_speech);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_mission_name_label->setText(mission_text.button_text);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_mission_icon->setImage(mission->get().icon);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_item_breadcrumb_widget->setText(m_mission_deploy_text);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_mission_stack->showPage(2);
  if (!result) return std::unexpected(std::move(result.error()));
  return update_breadcrumbs();
}

std::expected<void, StarException> AiInterface::show_crew(ListAction action) {
  auto crew = m_client->mainPlayer()->companions()->getCompanions("crew");
  if (action == ListAction::Refresh && m_current_page == AiPages::CrewPage && m_selected_recruit
      && std::ranges::find(crew, m_selected_recruit) != crew.end())
    return {};
  if (!m_crew_rows || crew != *m_crew_rows) {
    m_crew_rows.reset();
    auto populated = populate_crew(crew);
    if (!populated) return std::unexpected(std::move(populated.error()));
    m_crew_rows.emplace(std::move(crew));
  } else if (action == ListAction::Refresh && m_current_page == AiPages::CrewList) {
    return {};
  }
  m_current_page = AiPages::CrewList;
  m_speech.reset();
  auto result = m_page_breadcrumb_widget->setText(m_crew_breadcrumb_text);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_crew_list_widget->clearSelected();
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_main_stack->showPage(2);
  if (!result) return std::unexpected(std::move(result.error()));
  if (m_crew_rows->empty()) {
    result = m_crew_stack->showPage(0);
    if (!result) return std::unexpected(std::move(result.error()));
    result = set_current_speech("noCrewText", m_ai_database->no_crew_speech());
  } else {
    result = m_crew_stack->showPage(1);
  }
  if (!result) return std::unexpected(std::move(result.error()));
  return update_breadcrumbs();
}

std::expected<void, StarException> AiInterface::select_recruit() {
  std::size_t selected_item = m_crew_list_widget->selectedItem();
  m_selected_recruit.reset();
  if (m_crew_rows && selected_item < m_crew_rows->size())
    m_selected_recruit = (*m_crew_rows)[selected_item];
  if (!m_selected_recruit)
    return {};
  m_current_page = AiPages::CrewPage;
  auto description = m_selected_recruit->description();
  if (!description) return std::unexpected(std::move(description.error()));
  auto name = m_selected_recruit->name();
  if (!name) return std::unexpected(std::move(name.error()));
  auto const& recruit_name = *name ? **name : m_default_recruit_name;
  auto result = set_current_speech("recruitText", {.animation = m_ai_database->default_animation(),
      .text = std::move(*description).value_or(m_default_recruit_description), .speed_modifier = 1.0f});
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_recruit_name_label->setText(recruit_name);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_recruit_icon->setDrawables(m_selected_recruit->portrait());
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_item_breadcrumb_widget->setText(recruit_name);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_crew_stack->showPage(2);
  if (!result) return std::unexpected(std::move(result.error()));
  return update_breadcrumbs();
}

std::expected<void, StarException> AiInterface::populate_missions(AiState const& state) {
  auto result = m_mission_list_widget->clear();
  if (!result) return std::unexpected(std::move(result.error()));
  auto add_mission = [this](String const& mission_name, bool completed) -> std::expected<void, StarException> {
    auto widget = m_mission_list_widget->addItem();
    if (!widget) return std::unexpected(std::move(widget.error()));
    auto label = (*widget)->fetchChild<LabelWidget>("itemName");
    if (!label) return std::unexpected(std::move(label.error()));
    auto icon = (*widget)->fetchChild<ImageWidget>("itemIcon");
    if (!icon) return std::unexpected(std::move(icon.error()));
    auto mission = m_ai_database->mission(mission_name);
    if (!mission) return std::unexpected(std::move(mission.error()));
    auto image = (*icon)->setImage(mission->get().icon);
    if (!image) return std::unexpected(std::move(image.error()));
    auto const& mission_text = m_ai_database->mission_text(mission->get(), m_species);
    return (*label)->setText(completed ? mission_text.repeat_button_text : mission_text.button_text);
  };
  for (auto const& mission_name : state.available_missions) {
    result = add_mission(mission_name, false);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  for (auto const& mission_name : state.completed_missions) {
    result = add_mission(mission_name, true);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> AiInterface::start_mission() {
  if (!m_selected_mission)
    return {};
  auto mission = m_ai_database->mission(*m_selected_mission);
  if (!mission) return std::unexpected(std::move(mission.error()));
  auto result = m_client->warpPlayer(
      WarpToWorld{InstanceWorldId(mission->get().mission_unique_world, m_client->teamUuid()), {}},
      true, mission->get().warp_animation.value_or("default"), mission->get().warp_deploy.value_or(false));
  if (!result) return std::unexpected(std::move(result.error()));
  dismiss();
  return {};
}

std::expected<void, StarException> AiInterface::populate_crew(List<std::shared_ptr<Companion>> const& crew) {
  auto result = m_crew_list_widget->clear();
  if (!result) return std::unexpected(std::move(result.error()));
  for (auto const& recruit : crew) {
    auto widget = m_crew_list_widget->addItem();
    if (!widget) return std::unexpected(std::move(widget.error()));
    auto label = (*widget)->fetchChild<LabelWidget>("itemName");
    if (!label) return std::unexpected(std::move(label.error()));
    auto icon = (*widget)->fetchChild<ImageWidget>("itemIcon");
    if (!icon) return std::unexpected(std::move(icon.error()));
    result = (*icon)->setDrawables(recruit->portrait());
    if (!result) return std::unexpected(std::move(result.error()));
    auto name = recruit->name();
    if (!name) return std::unexpected(std::move(name.error()));
    result = (*label)->setText(std::move(*name).value_or(m_default_recruit_name));
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> AiInterface::dismiss_recruit() {
  if (!m_selected_recruit)
    return {};
  auto pod_uuid = m_selected_recruit->podUuid();
  if (!pod_uuid) return std::unexpected(std::move(pod_uuid.error()));
  auto result = m_client->mainPlayer()->companions()->dismissCompanion("crew", *pod_uuid);
  if (!result) return std::unexpected(std::move(result.error()));
  return show_crew(ListAction::Open);
}

std::expected<void, StarException> AiInterface::go_back() {
  if (m_current_page == AiPages::MissionPage)
    return show_missions(ListAction::Open);
  if (m_current_page == AiPages::CrewPage)
    return show_crew(ListAction::Open);
  if (m_current_page == AiPages::MissionList || m_current_page == AiPages::CrewList)
    return show_status();
  return {};
}

std::expected<void, StarException> AiInterface::set_face_animation(String const& name) {
  if (m_face_animation.first != name) {
    auto animation = m_ai_database->animation(m_species, name);
    if (!animation) return std::unexpected(std::move(animation.error()));
    m_face_animation = {name, std::move(*animation)};
  }
  return {};
}

std::expected<void, StarException> AiInterface::set_current_speech(String const& text_widget, AiSpeech speech) {
  auto widget = findChild<LabelWidget>(text_widget);
  if (!widget) return std::unexpected(std::move(widget.error()));
  auto length = Text::visible_length(speech.text);
  if (!length) return std::unexpected(std::move(length.error()));
  if ((*widget)->getTextCharLimit() != 0u) {
    auto limited = (*widget)->setTextCharLimit(0u);
    if (!limited) return std::unexpected(std::move(limited.error()));
  }
  auto result = (*widget)->setText(std::move(speech.text));
  if (!result) return std::unexpected(std::move(result.error()));
  auto limit = static_cast<unsigned>(std::min<std::size_t>(*length, std::numeric_limits<unsigned>::max()));
  m_speech.emplace(std::move(*widget), std::move(speech.animation), speech.speed_modifier, limit);
  return {};
}

}
