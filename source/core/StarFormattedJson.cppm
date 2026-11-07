module;
import star.format;
import star.string;
import star.list;

import std;

import star.json;

namespace Star {

class FormattedJson;

template <typename Jsonlike> class JsonStreamer;

struct ObjectElement;
struct ObjectKeyElement;
struct ValueElement;
struct WhitespaceElement;
struct ColonElement;
struct CommaElement;


struct ValueElement {
  ValueElement(FormattedJson json);

  std::shared_ptr<FormattedJson> value;

  bool operator==(ValueElement const& v) const;
};

struct ObjectKeyElement {
  String key;

  bool operator==(ObjectKeyElement const& v) const;
};

struct WhitespaceElement {
  String whitespace;

  bool operator==(WhitespaceElement const& v) const;
};

struct ColonElement {
  bool operator==(ColonElement const&) const;
};

struct CommaElement {
  bool operator==(CommaElement const&) const;
};
using JsonElement = std::variant<ValueElement, ObjectKeyElement, WhitespaceElement, ColonElement, CommaElement>;


std::expected<std::reference_wrapper<std::ostream>, StarException> operator<<(std::ostream& os, JsonElement const& elem);

// Class representing formatted JSON data. Preserves whitespace and comments.
class FormattedJson {
public:
  using ElementList = List<JsonElement>;
  using ElementLocation = std::size_t;

  static std::expected<FormattedJson, StarException> parse(String const& string);
  static std::expected<FormattedJson, StarException> parseJson(String const& string);
  static std::expected<FormattedJson, StarException> create(Json const& json);
  // Builds an unspaced array without discarding each value's original formatting.
  static std::expected<FormattedJson, StarException> createArray(std::span<FormattedJson const> values);

  static FormattedJson ofType(Json::Type type);

  FormattedJson();
  FormattedJson(FormattedJson const&) = default;
  FormattedJson& operator=(FormattedJson const&) = default;
  FormattedJson(FormattedJson&& other);
  FormattedJson& operator=(FormattedJson&& other);

  Json const& toJson() const;

  std::expected<FormattedJson, StarException> get(String const& key) const;
  std::expected<FormattedJson, StarException> get(std::size_t index) const;

  // Returns a new FormattedJson with the given values added or erased.
  // Prepend, insert and append update the value in-place if the key already
  // exists.
  std::expected<FormattedJson, StarException> prepend(String const& key, FormattedJson const& value) const;
  std::expected<FormattedJson, StarException> insertBefore(String const& key, FormattedJson const& value, String const& beforeKey) const;
  std::expected<FormattedJson, StarException> insertAfter(String const& key, FormattedJson const& value, String const& afterKey) const;
  std::expected<FormattedJson, StarException> append(String const& key, FormattedJson const& value) const;
  std::expected<FormattedJson, StarException> set(String const& key, FormattedJson const& value) const;
  std::expected<FormattedJson, StarException> eraseKey(String const& key) const;

  std::expected<FormattedJson, StarException> insert(std::size_t index, FormattedJson const& value) const;
  std::expected<FormattedJson, StarException> append(FormattedJson const& value) const;
  std::expected<FormattedJson, StarException> set(std::size_t index, FormattedJson const& value) const;
  std::expected<FormattedJson, StarException> eraseIndex(std::size_t index) const;

  // Returns the number of elements in a Json array, or entries in an object.
  std::expected<std::size_t, StarException> size() const;

  std::expected<bool, StarException> contains(String const& key) const;

  Json::Type type() const;
  bool isType(Json::Type type) const;
  String typeName() const;

  std::expected<String, StarException> toFormattedDouble() const;
  std::expected<String, StarException> toFormattedInt() const;

  std::expected<String, StarException> repr() const;
  std::expected<String, StarException> printJson() const;

  ElementList const& elements() const;

  // Equality ignores whitespace and formatting. It just compares the Json
  // values.
  bool operator==(FormattedJson const& v) const;
  bool operator!=(FormattedJson const& v) const;

private:
  friend class FormattedJsonBuilderStream;
  friend class JsonStreamer<FormattedJson>;

  class Builder;
  static std::expected<FormattedJson, StarException> object(ElementList elements);
  static std::expected<FormattedJson, StarException> array(ElementList elements);

  std::expected<FormattedJson, StarException> objectInsert(String const& key, FormattedJson const& value, ElementLocation loc) const;

  std::expected<std::reference_wrapper<FormattedJson const>, StarException> getFormattedJson(ElementLocation loc) const;

  Json m_jsonValue;
  ElementList m_elements;
  // Used to preserve the formatting of numbers, i.e. -0 vs 0, 1.0 vs 1:
  std::optional<String> m_formatting;

  // Only this token lookup index uses the standard byte hash. Semantic object
  // enumeration and wire order continue to use JsonObject's original hash.
  struct ObjectKeyHash {
    std::size_t operator()(String const& key) const { return std::hash<std::string_view>{}(key.utf8()); }
  };
  StringMap<std::pair<ElementLocation, ElementLocation>, ObjectKeyHash> m_objectEntryLocations;
  List<ElementLocation> m_arrayElementLocations;
};

std::expected<std::reference_wrapper<std::ostream>, StarException> operator<<(std::ostream& os, FormattedJson const& json);

}

export module star.formatted_json;


export namespace Star {
  using ::Star::FormattedJson;
  using ::Star::JsonElement;
  using ::Star::ObjectElement;
  using ::Star::ObjectKeyElement;
  using ::Star::ValueElement;
  using ::Star::WhitespaceElement;
  using ::Star::ColonElement;
  using ::Star::CommaElement;
  using ::Star::operator<<;
}
