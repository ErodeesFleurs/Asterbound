import std;
import star.format;
import star.string;
import star.bi_map;
import star.game_types;
import star.gui_types;

namespace Star {

bool ImageStretchSet::fullyPopulated() const {
  return !inner.empty() && !begin.empty() && !end.empty();
}

GuiDirection otherDirection(GuiDirection direction) {
  switch (direction) {
    case GuiDirection::Horizontal:
      return GuiDirection::Vertical;
    case GuiDirection::Vertical:
      return GuiDirection::Horizontal;
    default:
      if constexpr (Star::DebugEnabled) {
        Star::star_assert_failed((false));
      }
      return static_cast<GuiDirection>(-1);
  }
}

extern std::expected<EnumMap<GuiDirection>, StarException> const GuiDirectionNames = EnumMap<GuiDirection>::create({
    {GuiDirection::Horizontal, "horizontal"}, {GuiDirection::Vertical, "vertical"},
});

std::expected<String, StarException> rarityBorder(String rarity) {
  auto absolute = rarity.starts_with("/");
  if (!absolute)
    return std::unexpected(std::move(absolute.error()));
  return (*absolute ? rarity : Star::format("/interface/inventory/itemborder{}.png", rarity)).to_lower();
}

}
