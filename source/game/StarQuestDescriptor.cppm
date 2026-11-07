module;
import std;
import star.format;
import star.string;
import star.list;
import star.json;

import star.game_types;
import star.strong_typedef;
import star.data_stream;
import star.bi_map;
import star.item_descriptor;
import star.celestial_coordinate;

import star.rect;
namespace Star {

class Root;

// Item name - always one single item. QuestItem and QuestItemList are
// distinct due to how the surrounding text interacts with the parameter
// in the quest text. For a single item we might want to say "the <bandage>" or
// "any <bandage>", whereas the text for QuestItemList is always a list, e.g.
// "<1 bandage, 3 apple>."
struct QuestItem {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestItem& param);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestItem const& param);

  bool operator==(QuestItem const& rhs) const;
  [[nodiscard]] std::expected<ItemDescriptor, StarException> descriptor() const;

  String itemName;
  Json parameters;
};

// An item itemTag, indicating a set of possible items
template <typename BaseType>
struct QuestItemTagWrapper : BaseType {
  using BaseType::BaseType;

  QuestItemTagWrapper() : BaseType() {}

  QuestItemTagWrapper(QuestItemTagWrapper const& nt) : BaseType(nt) {}

  QuestItemTagWrapper(QuestItemTagWrapper&& nt) : BaseType(std::move(nt)) {}

  explicit QuestItemTagWrapper(BaseType const& bt) : BaseType(bt) {}

  explicit QuestItemTagWrapper(BaseType&& bt) : BaseType(std::move(bt)) {}

  QuestItemTagWrapper& operator=(QuestItemTagWrapper const& rhs) {
    BaseType::operator=(rhs);
    return *this;
  }

  QuestItemTagWrapper& operator=(QuestItemTagWrapper&& rhs) {
    BaseType::operator=(std::move(rhs));
    return *this;
  }

  template <class Arg>
  QuestItemTagWrapper& operator=(Arg&& other) {
    static_assert(std::is_base_of_v<BaseType, std::decay_t<Arg>> == false
                    || std::is_same_v<QuestItemTagWrapper, std::decay_t<Arg>>,
                  "QuestItemTag can not implicitly be assigned from String-derived classes or strong String typedefs");

    BaseType::operator=(std::forward<Arg>(other));
    return *this;
  }
};
using QuestItemTag = QuestItemTagWrapper<String>;

// A collection of items
template <typename BaseType>
struct QuestItemListWrapper : BaseType {
  using BaseType::BaseType;

  QuestItemListWrapper() : BaseType() {}

  QuestItemListWrapper(QuestItemListWrapper const& nt) : BaseType(nt) {}

  QuestItemListWrapper(QuestItemListWrapper&& nt) : BaseType(std::move(nt)) {}

  explicit QuestItemListWrapper(BaseType const& bt) : BaseType(bt) {}

  explicit QuestItemListWrapper(BaseType&& bt) : BaseType(std::move(bt)) {}

  QuestItemListWrapper& operator=(QuestItemListWrapper const& rhs) {
    BaseType::operator=(rhs);
    return *this;
  }

  QuestItemListWrapper& operator=(QuestItemListWrapper&& rhs) {
    BaseType::operator=(std::move(rhs));
    return *this;
  }

  template <class Arg>
  QuestItemListWrapper& operator=(Arg&& other) {
    static_assert(std::is_base_of_v<BaseType, std::decay_t<Arg>> == false
                    || std::is_same_v<QuestItemListWrapper, std::decay_t<Arg>>,
                  "QuestItemList can not implicitly be assigned from List<ItemDescriptor>-derived classes or strong "
                  "List<ItemDescriptor> typedefs");

    BaseType::operator=(std::forward<Arg>(other));
    return *this;
  }
};
using QuestItemList = QuestItemListWrapper<List<ItemDescriptor>>;

// The uniqueId of a specific entity
struct QuestEntity {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestEntity& param);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestEntity const& param);

  bool operator==(QuestEntity const& rhs) const;

  std::optional<String> uniqueId;
  std::optional<String> species;
  std::optional<Gender> gender;
};

// A location within the world, which could represent a spawn point or a dungeon
struct QuestLocation {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestLocation& param);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestLocation const& param);
  bool operator==(QuestLocation const& rhs) const;

  std::optional<String> uniqueId;
  RectF region;
};

struct QuestMonsterType {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestMonsterType& param);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestMonsterType const& param);

  bool operator==(QuestMonsterType const& rhs) const;

  String typeName;
  JsonObject parameters;
};

struct QuestNpcType {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestNpcType& param);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestNpcType const& param);

  bool operator==(QuestNpcType const& rhs) const;

  String species;
  String typeName;
  JsonObject parameters;
  std::optional<std::uint64_t> seed;
};

struct QuestCoordinate {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestCoordinate& param);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestCoordinate const& param);

  bool operator==(QuestCoordinate const& rhs) const;

  CelestialCoordinate coordinate;
};

using QuestJson = Json;

using QuestParamDetail = std::variant<std::monostate, QuestItem, QuestItemTag, QuestItemList, QuestEntity,
                                      QuestLocation, QuestMonsterType, QuestNpcType, QuestCoordinate, QuestJson>;

struct QuestParam {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestParam& param);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestParam const& param);

  [[nodiscard]] static std::expected<QuestParam, StarException> fromJson(Json const& json);
  [[nodiscard]] static std::expected<QuestParam, StarException> diskLoad(Root& root, Json const& json);

  [[nodiscard]] std::expected<Json, StarException> toJson() const;
  [[nodiscard]] std::expected<Json, StarException> diskStore(Root& root) const;

  bool operator==(QuestParam const& rhs) const;

  QuestParamDetail detail;
  std::optional<String> name;
  std::optional<Json> portrait;
  std::optional<String> indicator;
};

struct QuestDescriptor {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestDescriptor& quest);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestDescriptor const& quest);

  [[nodiscard]] static std::expected<QuestDescriptor, StarException> fromJson(Root& root, Json const& json);
  [[nodiscard]] static std::expected<QuestDescriptor, StarException> diskLoad(Root& root, Json const& json);

  [[nodiscard]] std::expected<Json, StarException> toJson() const;
  [[nodiscard]] std::expected<Json, StarException> diskStore(Root& root) const;

  bool operator==(QuestDescriptor const& rhs) const;

  String questId;
  String templateId;
  StringMap<QuestParam> parameters;
  std::uint64_t seed;
};

struct QuestArcDescriptor {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestArcDescriptor& questArc);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestArcDescriptor const& questArc);

  [[nodiscard]] static std::expected<QuestArcDescriptor, StarException> fromJson(Root& root, Json const& json);
  [[nodiscard]] static std::expected<QuestArcDescriptor, StarException> diskLoad(Root& root, Json const& json);

  [[nodiscard]] std::expected<Json, StarException> toJson() const;
  [[nodiscard]] std::expected<Json, StarException> diskStore(Root& root) const;

  bool operator==(QuestArcDescriptor const& rhs) const;

  List<QuestDescriptor> quests;
  std::optional<String> stagehandUniqueId;
};

[[nodiscard]] std::expected<String, StarException> questParamText(Root& root, QuestParam const& param);
[[nodiscard]] std::expected<StringMap<String>, StarException> questParamTags(Root& root, StringMap<QuestParam> const& parameters);

[[nodiscard]] std::expected<StringMap<QuestParam>, StarException> questParamsFromJson(Json const& json);
[[nodiscard]] std::expected<StringMap<QuestParam>, StarException> questParamsDiskLoad(Root& root, Json const& json);
[[nodiscard]] std::expected<Json, StarException> questParamsToJson(StringMap<QuestParam> const& parameters);
[[nodiscard]] std::expected<Json, StarException> questParamsDiskStore(Root& root, StringMap<QuestParam> const& parameters);

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestItem& param);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestItem const& param);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestEntity& param);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestEntity const& param);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestLocation& param);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestLocation const& param);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestMonsterType& param);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestMonsterType const& param);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestNpcType& param);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestNpcType const& param);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestCoordinate& param);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestCoordinate const& param);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestParam& param);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestParam const& param);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestDescriptor& quest);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestDescriptor const& quest);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestArcDescriptor& questArc);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestArcDescriptor const& questArc);
}

export module star.quest_descriptor;

export namespace Star {
  using ::Star::QuestItem;
  using ::Star::QuestItemTagWrapper;
  using ::Star::QuestItemTag;
  using ::Star::QuestItemListWrapper;
  using ::Star::QuestItemList;
  using ::Star::QuestEntity;
  using ::Star::QuestLocation;
  using ::Star::QuestMonsterType;
  using ::Star::QuestNpcType;
  using ::Star::QuestCoordinate;
  using ::Star::QuestJson;
  using ::Star::QuestParamDetail;
  using ::Star::QuestParam;
  using ::Star::QuestDescriptor;
  using ::Star::QuestArcDescriptor;
  using ::Star::questParamText;
  using ::Star::questParamTags;
  using ::Star::questParamsFromJson;
  using ::Star::questParamsDiskLoad;
  using ::Star::questParamsToJson;
  using ::Star::questParamsDiskStore;
}
