import std;
import star.format;
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
import star.button_group;
import star.button_widget;

namespace Star {

void ButtonGroup::setCallback(WidgetCallbackFunc callback) {
  m_callback = std::move(callback);
}

std::optional<std::reference_wrapper<ButtonWidget>> ButtonGroup::button(int id) const {
  auto link = m_buttons.find_ref(id);
  if (!link)
    return {};
  auto owner = link->get().lock();
  if (!owner)
    return {};
  return asRef<ButtonWidget>(*owner);
}

List<std::reference_wrapper<ButtonWidget>> ButtonGroup::buttons() const {
  List<std::reference_wrapper<ButtonWidget>> result;
  for (auto const& entry : m_buttons) {
    if (auto owner = entry.second.lock()) {
      if (auto button = asRef<ButtonWidget>(*owner))
        result.push_back(*button);
    }
  }
  return result;
}

std::size_t ButtonGroup::buttonCount() const {
  return std::ranges::count_if(m_buttons, [](auto const& entry) { return !entry.second.expired(); });
}

int ButtonGroup::addButton(ButtonWidget& button, int id) {
  auto existingId = this->id(button);
  if (existingId != NoButton)
    return existingId;

  // If we are auto-generating an id, start at the last id and work forward.
  if (id == NoButton && !m_buttons.empty())
    id = (std::prev(m_buttons.end()))->first;

  while (m_buttons.contains(id))
    ++id;

  m_buttons[id] = button.weak_from_this();
  m_buttonIds[button.weak_from_this()] = id;
  return id;
}

void ButtonGroup::removeButton(ButtonWidget& button) {
  auto link = button.weak_from_this();
  if (auto id = m_buttonIds.find_ref(link)) {
    m_buttons.remove(id->get());
    m_buttonIds.remove(link);
  }
}

int ButtonGroup::id(ButtonWidget const& button) const {
  if (auto id = m_buttonIds.find_ref(button.weak_from_this()))
    return id->get();
  return NoButton;
}

std::optional<std::reference_wrapper<ButtonWidget>> ButtonGroup::checkedButton() const {
  for (auto const& entry : m_buttons) {
    if (auto button = this->button(entry.first); button && button->get().isChecked())
      return button;
  }
  return {};
}

int ButtonGroup::checkedId() const {
  auto checked = checkedButton();
  return checked ? id(checked->get()) : NoButton;
}

std::expected<void, StarException> ButtonGroup::select(int id) {
  auto b = button(id);
  if (!b)
    return std::unexpected(GuiException("Unknown button group id"));
  if (!b->get().isChecked())
    return b->get().check();
  return {};
}

std::expected<void, StarException> ButtonGroup::wasChecked(ButtonWidget& self) {
  for (auto const& entry : m_buttons) {
    if (auto button = this->button(entry.first); button && &button->get() != &self)
      button->get().setChecked(false);
  }
  if (m_callback)
    return m_callback(self);
  return {};
}

bool ButtonGroup::toggle() const {
  return m_toggle;
}

void ButtonGroup::setToggle(bool toggleMode) {
  m_toggle = toggleMode;
}

}
