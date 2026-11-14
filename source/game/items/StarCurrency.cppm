module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.json_extra;
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
import star.random;


import star.drawable;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.item;
import star.root;


export module star.currency_item;

export namespace Star {

class CurrencyItem : public Item {
public:
  void const* query_type(TypeKey key) const noexcept override { return query_type_of<CurrencyItem, Item>(this, key); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<CurrencyItem>(); }

  [[nodiscard]]
  static std::expected<std::shared_ptr<CurrencyItem>, StarException> create(Root& root, Json const& config, String const& directory);

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]]
  std::expected<String, StarException> pickupSound() const override;

  String currencyType();

  // Value of a single instance of this currency
  std::uint64_t currencyValue();

  // Total value of all currencies (so currencyValue * count)
  std::uint64_t totalValue();

private:
  explicit CurrencyItem(Root& root) : Item(root) {}
  String m_currency;
  std::uint64_t m_value;
};

}

namespace Star {

std::expected<std::shared_ptr<CurrencyItem>, StarException> CurrencyItem::create(Root& root, Json const& config, String const& directory) {
  auto item = std::shared_ptr<CurrencyItem>(new CurrencyItem(root));
  auto initialized = item->initialize(config, directory);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto currency = config.getString("currency");
  if (!currency)
    return std::unexpected(std::move(currency.error()));
  item->m_currency = std::move(*currency);
  auto value = config.getUInt("value");
  if (!value)
    return std::unexpected(std::move(value.error()));
  item->m_value = *value;
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> CurrencyItem::clone() const {
  return std::make_shared<CurrencyItem>(*this);
}

std::expected<String, StarException> CurrencyItem::pickupSound() const {
  auto smallLimitValue = instanceValue("smallStackLimit", 100);
  if (!smallLimitValue)
    return std::unexpected(std::move(smallLimitValue.error()));
  auto smallLimit = smallLimitValue->toUInt();
  if (!smallLimit)
    return std::unexpected(std::move(smallLimit.error()));
  String soundsKey;
  if (count() <= *smallLimit) {
    soundsKey = "pickupSoundsSmall";
  } else {
    auto mediumLimitValue = instanceValue("mediumStackLimit", 10000);
    if (!mediumLimitValue)
      return std::unexpected(std::move(mediumLimitValue.error()));
    auto mediumLimit = mediumLimitValue->toUInt();
    if (!mediumLimit)
      return std::unexpected(std::move(mediumLimit.error()));
    soundsKey = count() <= *mediumLimit ? std::string_view("pickupSoundsMedium") : std::string_view("pickupSoundsLarge");
  }
  auto soundsValue = instanceValue(soundsKey, {});
  if (!soundsValue)
    return std::unexpected(std::move(soundsValue.error()));
  if (!soundsValue->isNull()) {
    auto sounds = jsonToStringSet(*soundsValue);
    if (!sounds)
      return std::unexpected(std::move(sounds.error()));
    auto sound = root().random().randFrom(*sounds);
    if (!sound)
      return std::unexpected(std::move(sound.error()));
    return sound->get();
  }
  return Item::pickupSound();
}

String CurrencyItem::currencyType() {
  return m_currency;
}

std::uint64_t CurrencyItem::currencyValue() {
  return m_value;
}

std::uint64_t CurrencyItem::totalValue() {
  return m_value * count();
}

}
