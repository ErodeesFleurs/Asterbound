module;
import std;
import star.format;
import star.json;

import star.game_types;
import star.entity_rendering_types;

import star.color;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;
import star.string;
import star.vector;
import star.text;
import star.map;






import star.world_geometry;
import star.chat_action;
import star.font_texture_group;
import star.anchor_types;
import star.text_painter;
import star.chat_bubble_separation;
import star.world_camera;

namespace Star {

class GuiContext;
class AssetTextureGroup;
class WorldClient;
class Entity;
class StoredFunction;

class ChatBubbleManager {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  ChatBubbleManager(ConstructionKey, GuiContext& context);
  [[nodiscard]] static std::expected<std::unique_ptr<ChatBubbleManager>, StarException> create(GuiContext& context);

  [[nodiscard]] std::expected<void, StarException> set_camera(WorldCamera const& camera);

  [[nodiscard]] std::expected<void, StarException> add_chat_actions(List<ChatAction> chat_actions, bool silent = false);

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::shared_ptr<WorldClient> const& world);
  [[nodiscard]] std::expected<void, StarException> render(WorldClient const& world, WorldCamera const& camera, PresentationContext presentation);

private:
  [[nodiscard]] std::expected<void, StarException> initialize();
  using BubbleImage = std::tuple<String, Vec2F>;
  using BubbleText = std::tuple<String, TextStyle, bool, Vec2F>;

  struct Bubble {
    EntityId entity;
    String text;
    Json config;
    float age;
    List<BubbleImage> background_images;
    List<BubbleText> bubble_text;
    bool onscreen;
    std::optional<std::weak_ptr<Entity>> instance;
    std::uint64_t presentation_epoch = 0;
  };

  struct PortraitBubble {
    EntityId entity;
    String portrait;
    String text;
    Vec2F position;
    Json config;
    float age;
    List<BubbleImage> background_images;
    List<BubbleText> bubble_text;
    bool onscreen;
    std::optional<std::weak_ptr<Entity>> instance;
  };

  // Calculate the alpha for a speech bubble based on distance from player to
  // edge of screen
  [[nodiscard]] std::expected<std::uint8_t, StarException> calc_distance_fade_alpha(Vec2F bubble_screen_position, StoredFunction const& fade_function, WorldCamera const& camera) const;

  // Wrap widths are stored in unscaled interface pixels for measurement and drawing.
  TextPositioning scaled_positioning(TextPositioning positioning, float pixel_ratio) const;

  [[nodiscard]] std::expected<RectF, StarException> bubble_image_rect(Vec2F screen_pos, BubbleImage const& bubble_image, float pixel_ratio);
  [[nodiscard]] std::expected<void, StarException> draw_bubble_image(Vec2F screen_pos, BubbleImage const& bubble_image, float pixel_ratio, int alpha);
  [[nodiscard]] std::expected<void, StarException> draw_bubble_text(Vec2F screen_pos, BubbleText const& bubble_text, float pixel_ratio, int alpha, bool is_portrait);

  std::reference_wrapper<GuiContext> m_gui_context;

  WorldCamera m_camera;
  std::weak_ptr<WorldClient> m_world;
  std::optional<std::uint64_t> m_presentation_epoch;

  TextPositioning m_text_template;
  TextPositioning m_portrait_text_template;
  TextStyle m_text_style;
  Vec2F m_text_padding;

  BubbleSeparator<Bubble> m_bubbles;
  float m_part_size;
  Map<EntityId, int> m_entity_bubble_counts;
  float m_cached_interface_scale;
  Vec2F m_bubble_offset;
  float m_max_age;
  float m_portrait_max_age;
  float m_inter_bubble_margin;
  int m_max_message_per_entity;

  Deque<PortraitBubble> m_portrait_bubbles;
  String m_portrait_background_image;
  String m_portrait_more_image;
  Vec2I m_portrait_more_position;
  Vec2I m_portrait_background_size;
  Vec2I m_portrait_position;
  Vec2I m_portrait_size;
  Vec2I m_portrait_text_position;
  unsigned m_portrait_text_width;
  float m_portrait_chatter_framerate;
  float m_portrait_chatter_duration;

  float m_furthest_visible_text_distance; // 0.0 is directly over the player, 1.0
  // is the edge of the window
  std::shared_ptr<StoredFunction> m_text_fade_function;
  std::shared_ptr<StoredFunction> m_bubble_fade_function;
};

}

export module star.chat_bubble_manager;

export namespace Star {
  using ::Star::GuiContext;
  using ::Star::AssetTextureGroup;
  using ::Star::WorldClient;
  using ::Star::ChatBubbleManager;
  using ::Star::StoredFunction;
}
