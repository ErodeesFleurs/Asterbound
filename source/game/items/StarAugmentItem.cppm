module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.set;
import star.string;
import star.data_stream;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.bi_map;
import star.game_types;
import star.strong_typedef;

import star.drawable;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.item;

namespace Star {

class AugmentItem : public Item {
public:
  void const* query_type(TypeKey key) const noexcept override { return query_type_of<AugmentItem, Item>(this, key); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<AugmentItem>(); }

  [[nodiscard]]
  static std::expected<std::shared_ptr<AugmentItem>, StarException> create(Root& root, Json const& config, String const& directory, Json const& parameters = JsonObject());

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]]
  std::expected<StringList, StarException> augmentScripts() const;

  // Makes no change to the given item if the augment can't be applied.
  // Consumes itself and returns true if the augment is applied.
  // Has no effect if augmentation fails.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> applyTo(std::shared_ptr<Item> const item);
private:
  explicit AugmentItem(Root& root) : Item(root) {}
};

}

export module star.augment_item;

export namespace Star {
  using ::Star::AugmentItem;
}
