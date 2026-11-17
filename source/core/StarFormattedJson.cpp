import std;
import star.memory;
import star.string;
import star.list;
import star.format;
import star.text_formatter;
import star.json;
import star.lexical_cast;
import star.unicode;

import star.json_parser;
import star.json_builder;
import star.formatted_json;

namespace Star {

// Mutable state is confined to construction. Every completed FormattedJson owns
// one frozen semantic container, while duplicate object tokens remain intact and
// the semantic payload and lookup index retain their last occurrence.
class FormattedJson::Builder {
public:
  explicit Builder(bool object, std::size_t values = 0, std::size_t tokens = 0,
                   std::optional<std::reference_wrapper<Json const>> original = {})
      : m_object(object) {
    if (original) {
      m_result.m_value = original->get();
    } else if (object) {
      m_values.emplace(std::in_place_type<JsonObject>);
      std::get<JsonObject>(*m_values).reserve(values);
    } else {
      m_values.emplace(std::in_place_type<JsonArray>);
      std::get<JsonArray>(*m_values).reserve(values);
    }
    m_result.m_elements.reserve(tokens);
    if (object)
      m_result.m_object_entry_locations.reserve(values);
    else
      m_result.m_array_element_locations.reserve(values);
  }
  Builder(Builder&&) = default;
  Builder& operator=(Builder&&) = default;
  Builder(Builder const&) = delete;
  Builder& operator=(Builder const&) = delete;

  std::expected<void, StarException> append(JsonElement elem) {
    auto indexed = index(elem, m_result.m_elements.size());
    if (!indexed)
      return indexed;
    m_result.m_elements.push_back(std::move(elem));
    return {};
  }

  static std::expected<FormattedJson, StarException> rebuild(bool object, ElementList elements,
                                                             std::optional<std::size_t> array_value_count = {}) {
    std::size_t values;
    if (!object && array_value_count)
      values = *array_value_count;
    else
      values = std::ranges::count_if(
        elements, [](JsonElement const& elem) { return std::holds_alternative<ValueElement>(elem); });
    Builder builder(object, values);
    builder.m_result.m_elements = std::move(elements);
    for (std::size_t loc = 0; loc < builder.m_result.m_elements.size(); ++loc) {
      auto indexed = builder.index(builder.m_result.m_elements[loc], loc);
      if (!indexed)
        return std::unexpected(std::move(indexed.error()));
    }
    return builder.finish();
  }

  FormattedJson finish() {
    if (m_values)
      m_result.m_value = std::visit([](auto& values) { return Json(std::move(values)); }, *m_values);
    return std::move(m_result);
  }

private:
  std::expected<void, StarException> index(JsonElement const& elem, ElementLocation loc) {
    if (!m_object && (std::holds_alternative<ColonElement>(elem) || std::holds_alternative<ObjectKeyElement>(elem)))
      return std::unexpected(JsonException("Invalid FormattedJson element in Json array"));
    if (std::holds_alternative<ObjectKeyElement>(elem)) {
      if (!m_object)
        return std::unexpected(JsonException("Invalid FormattedJson object key outside object"));
      m_last_key = loc;
    } else if (std::holds_alternative<ValueElement>(elem)) {
      auto const& value = std::get<ValueElement>(elem);
      if (!value.m_value)
        return std::unexpected(JsonException("Invalid empty FormattedJson value element"));
      if (m_last_key) {
        if (!m_object || *m_last_key >= m_result.m_elements.size())
          return std::unexpected(JsonException("Invalid FormattedJson object entry"));
        auto const& key_element = m_result.m_elements[*m_last_key];
        if (!std::holds_alternative<ObjectKeyElement>(key_element))
          return std::unexpected(JsonException("Invalid FormattedJson object key element"));
        auto const& key = std::get<ObjectKeyElement>(key_element);
        if (m_values)
          std::get<JsonObject>(*m_values).insert_or_assign(key.m_key, value.m_value->toJson());
        m_result.m_object_entry_locations.insert_or_assign(key.m_key, std::pair{*m_last_key, loc});
        m_last_key.reset();
      } else {
        if (m_object)
          return std::unexpected(JsonException("Invalid FormattedJson array entry"));
        if (m_values)
          std::get<JsonArray>(*m_values).push_back(value.m_value->toJson());
        m_result.m_array_element_locations.push_back(loc);
      }
    }
    return {};
  }

  bool m_object;
  FormattedJson m_result;
  std::optional<std::variant<JsonArray, JsonObject>> m_values;
  std::optional<ElementLocation> m_last_key;
};

class FormattedJsonBuilderStream : public JsonStream {
public:
  std::expected<void, StarException> begin_object() override;
  std::expected<void, StarException> object_key(std::span<String::Char const> s) override;
  std::expected<void, StarException> object_key_utf8(std::string_view s) override;
  std::expected<void, StarException> end_object() override;
  std::expected<void, StarException> begin_array() override;
  std::expected<void, StarException> end_array() override;
  std::expected<void, StarException> put_string(std::span<String::Char const> s) override;
  std::expected<void, StarException> put_string_utf8(std::string_view s) override;
  std::expected<void, StarException> put_double(std::string_view s) override;
  std::expected<void, StarException> put_integer(std::string_view s) override;
  std::expected<void, StarException> put_boolean(bool b) override;
  std::expected<void, StarException> put_null() override;
  std::expected<void, StarException> put_whitespace(std::span<String::Char const> s) override;
  std::expected<void, StarException> put_whitespace_utf8(std::string_view s) override;
  std::expected<void, StarException> put_comma() override;
  std::expected<void, StarException> put_colon() override;
  std::expected<FormattedJson, StarException> take_top();

private:
  void push(bool object);
  std::expected<FormattedJson, StarException> pop();
  std::expected<std::reference_wrapper<FormattedJson::Builder>, StarException> current();
  std::expected<void, StarException> put_value(Json const& value, std::optional<String> formatting = {});
  std::optional<FormattedJson> m_root;
  List<FormattedJson::Builder> m_stack;
};

template <>
class JsonStreamer<FormattedJson> {
public:
  static std::expected<void, StarException> to_json_stream(FormattedJson const& val, JsonStream& stream, bool sort);
};

ValueElement::ValueElement(FormattedJson json) : m_value(std::make_shared<FormattedJson>(std::move(json))) {}

bool ValueElement::operator==(ValueElement const& v) const { return *m_value == *v.m_value; }

bool ObjectKeyElement::operator==(ObjectKeyElement const& v) const { return m_key == v.m_key; }

bool WhitespaceElement::operator==(WhitespaceElement const& v) const { return m_whitespace == v.m_whitespace; }

bool ColonElement::operator==(ColonElement const&) const {
  return true;
}

bool CommaElement::operator==(CommaElement const&) const {
  return true;
}

std::expected<FormattedJson, StarException> FormattedJson::parse(String const& string) {
  auto const& bytes = string.utf8();
  return input_utf8_json<std::string::const_iterator, FormattedJsonBuilderStream, FormattedJson>(
    bytes.begin(), bytes.end(), JsonParseType::Value);
}

std::expected<FormattedJson, StarException> FormattedJson::parseJson(String const& string) {
  auto const& bytes = string.utf8();
  return input_utf8_json<std::string::const_iterator, FormattedJsonBuilderStream, FormattedJson>(
    bytes.begin(), bytes.end(), JsonParseType::Top);
}

FormattedJson FormattedJson::ofType(Json::Type type) {
  FormattedJson json;
  json.m_value = Json::ofType(type);
  return json;
}

FormattedJson::FormattedJson() = default;

FormattedJson::FormattedJson(FormattedJson&& other)
    : m_value(std::move(other.m_value)), m_elements(std::move(other.m_elements)),
      m_formatting(std::move(other.m_formatting)),
      m_object_entry_locations((other.m_formatting.reset(), std::move(other.m_object_entry_locations))),
      m_array_element_locations(std::move(other.m_array_element_locations)) {}

FormattedJson& FormattedJson::operator=(FormattedJson&& other) {
  if (this != &other) {
    m_value = std::move(other.m_value);
    m_elements = std::move(other.m_elements);
    m_formatting = std::move(other.m_formatting);
    other.m_formatting.reset();
    m_object_entry_locations = std::move(other.m_object_entry_locations);
    m_array_element_locations = std::move(other.m_array_element_locations);
  }
  return *this;
}

std::expected<FormattedJson, StarException> FormattedJson::create(Json const& json) {
  auto create_value = [](auto&& self, Json const& value, bool nested) -> std::expected<FormattedJson, StarException> {
    bool object = value.isType(Json::Type::Object);
    bool array = value.isType(Json::Type::Array);
    if (!object && !array) {
      if (nested && value.isType(Json::Type::String)) {
        auto text = value.string_ptr();
        if (!text)
          return std::unexpected(std::move(text.error()));
        auto valid = utf8_length((*text)->utf8());
        if (!valid)
          return std::unexpected(std::move(valid.error()));
      }
      if (nested && value.isType(Json::Type::Float)) {
        auto number = value.toDouble();
        if (!number)
          return std::unexpected(std::move(number.error()));
        if (!std::isfinite(*number)) {
          // Keep the builder's existing rejection of unparseable numeric text
          // when converting a container, without formatting ordinary numbers.
          FormattedJsonBuilderStream stream;
          auto written = JsonStreamer<Json>::to_json_stream(value, stream, false);
          if (!written)
            return std::unexpected(std::move(written.error()));
        }
      }
      FormattedJson result;
      result.m_value = value;
      return result;
    }
    auto size = value.size();
    if (!size)
      return std::unexpected(std::move(size.error()));
    Builder builder(object, *size, *size * (object ? 4 : 2), std::cref(value));
    auto append_value = [&](Json const& child, bool first) -> std::expected<void, StarException> {
      if (!first) {
        auto comma = builder.append(CommaElement{});
        if (!comma)
          return comma;
      }
      auto formatted = self(self, child, true);
      if (!formatted)
        return std::unexpected(std::move(formatted.error()));
      return builder.append(ValueElement{std::move(*formatted)});
    };
    bool first = true;
    if (object) {
      auto entries = value.iterate_object();
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      for (auto const& [key, child] : *entries) {
        if (!first) {
          auto comma = builder.append(CommaElement{});
          if (!comma)
            return std::unexpected(std::move(comma.error()));
        }
        auto valid = utf8_length(key.utf8());
        if (!valid)
          return std::unexpected(std::move(valid.error()));
        auto added = builder.append(ObjectKeyElement{key});
        if (!added)
          return std::unexpected(std::move(added.error()));
        added = builder.append(ColonElement{});
        if (!added)
          return std::unexpected(std::move(added.error()));
        added = append_value(child, true);
        if (!added)
          return std::unexpected(std::move(added.error()));
        first = false;
      }
    } else {
      auto children = value.iterate_array();
      if (!children)
        return std::unexpected(std::move(children.error()));
      for (auto const& child : *children) {
        auto added = append_value(child, first);
        if (!added)
          return std::unexpected(std::move(added.error()));
        first = false;
      }
    }
    return builder.finish();
  };
  return create_value(create_value, json, false);
}

std::expected<FormattedJson, StarException> FormattedJson::createArray(std::span<FormattedJson const> values) {
  Builder builder(false, values.size(), values.size() * 2);
  bool first = true;
  for (auto const& value : values) {
    if (!first) {
      auto comma = builder.append(CommaElement{});
      if (!comma)
        return std::unexpected(std::move(comma.error()));
    }
    auto appended = builder.append(ValueElement{value});
    if (!appended)
      return std::unexpected(std::move(appended.error()));
    first = false;
  }
  return builder.finish();
}

Json const& FormattedJson::toJson() const { return m_value; }

std::expected<FormattedJson, StarException> FormattedJson::get(String const& key) const {
  if (type() != Json::Type::Object)
    return std::unexpected(JsonException::format("Cannot call get with key on FormattedJson type {}, must be Object type", typeName()));
  auto entry = m_object_entry_locations.maybe(key);
  if (!entry)
    return std::unexpected(JsonException::format("No such key in FormattedJson::get(\"{}\")", key));
  auto value = get_formatted_json(entry->second);
  if (!value)
    return std::unexpected(std::move(value.error()));
  return value->get();
}

std::expected<FormattedJson, StarException> FormattedJson::get(std::size_t index) const {
  if (type() != Json::Type::Array)
    return std::unexpected(JsonException::format("Cannot call get with index on FormattedJson type {}, must be Array type", typeName()));
  if (index >= m_array_element_locations.size())
    return std::unexpected(JsonException::format("FormattedJson::get({}) out of range", index));
  auto value = get_formatted_json(m_array_element_locations[index]);
  if (!value)
    return std::unexpected(std::move(value.error()));
  return value->get();
}

struct WhitespaceStyle {
  String m_before_key;
  String m_before_colon;
  String m_before_value;
  String m_before_comma;
};

template <class Element>
concept formatted_json_element = std::same_as<Element, ValueElement> || std::same_as<Element, ObjectKeyElement>
  || std::same_as<Element, WhitespaceElement> || std::same_as<Element, ColonElement>
  || std::same_as<Element, CommaElement>;

template <formatted_json_element ElementType>
FormattedJson::ElementLocation index_of(FormattedJson::ElementList const& elements,
                                        FormattedJson::ElementLocation pos) {
  for (; pos < elements.size(); ++pos) {
    if (std::holds_alternative<ElementType>(elements[pos]))
      return pos;
  }
  return NPos;
}

template <formatted_json_element ElementType>
FormattedJson::ElementLocation last_index_of(FormattedJson::ElementList const& elements,
                                             FormattedJson::ElementLocation pos) {
  while (pos > 0) {
    --pos;
    if (std::holds_alternative<ElementType>(elements[pos]))
      return pos;
  }
  return NPos;
}

String concat_whitespace(FormattedJson::ElementList const& elements, FormattedJson::ElementLocation from,
                         FormattedJson::ElementLocation to) {
  String whitespace;
  for (auto i = from; i < to; ++i) {
    if (std::holds_alternative<WhitespaceElement>(elements[i]))
      whitespace += std::get<WhitespaceElement>(elements[i]).m_whitespace;
  }
  return whitespace;
}

std::expected<WhitespaceStyle, StarException> detect_whitespace(FormattedJson::ElementList const& elements,
                                                                FormattedJson::ElementLocation insert_loc, bool array) {
  WhitespaceStyle style;

  // Find a nearby value as a reference location to learn whitespace from.
  FormattedJson::ElementLocation value_loc = last_index_of<ValueElement>(elements, insert_loc);
  if (value_loc == NPos)
    value_loc = index_of<ValueElement>(elements, insert_loc);

  if (value_loc == NPos) {
    // This object/array is empty. Pre-key/value whitespace will be the total of
    // the whitespace already present, plus some guessed indentation if it
    // contained a newline.
    String before_value = concat_whitespace(elements, 0, elements.size());
    auto newline = before_value.find('\n');
    if (!newline)
      return std::unexpected(std::move(newline.error()));
    if (*newline != NPos)
      before_value += "  ";
    if (array)
      return WhitespaceStyle{.m_before_key = "",
                             .m_before_colon = "",
                             .m_before_value = before_value,
                             .m_before_comma = ""};
    return WhitespaceStyle{.m_before_key = before_value,
                           .m_before_colon = "",
                           .m_before_value = "",
                           .m_before_comma = ""};
  }

  FormattedJson::ElementLocation comma_loc = index_of<CommaElement>(elements, value_loc);
  if (comma_loc != NPos) {
    style.m_before_comma = concat_whitespace(elements, value_loc + 1, comma_loc);
  }

  FormattedJson::ElementLocation colon_loc = last_index_of<ColonElement>(elements, value_loc);
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed(((colon_loc == NPos) == array));
  }
  if (colon_loc != NPos) {
    style.m_before_value = concat_whitespace(elements, colon_loc + 1, value_loc);

    FormattedJson::ElementLocation key_loc = last_index_of<ObjectKeyElement>(elements, colon_loc);
    if constexpr (Star::DebugEnabled) {
      Star::star_assert_failed((key_loc != NPos));
    }
    style.m_before_colon = concat_whitespace(elements, key_loc + 1, colon_loc);

    FormattedJson::ElementLocation prev_value_loc = last_index_of<ValueElement>(elements, key_loc);
    if (prev_value_loc == NPos)
      prev_value_loc = 0;
    style.m_before_key = concat_whitespace(elements, prev_value_loc, key_loc);

  } else {
    FormattedJson::ElementLocation prev_value_loc = last_index_of<ValueElement>(elements, value_loc);
    if (prev_value_loc == NPos)
      prev_value_loc = 0;
    style.m_before_value = concat_whitespace(elements, prev_value_loc, value_loc);
  }

  return style;
}

String const& whitespace_before(WhitespaceStyle const& style, JsonElement const& element) {
  if (std::holds_alternative<ValueElement>(element))
    return style.m_before_value;
  if (std::holds_alternative<ObjectKeyElement>(element))
    return style.m_before_key;
  if (std::holds_alternative<ColonElement>(element))
    return style.m_before_colon;
  return style.m_before_comma;
}

void append_with_whitespace(FormattedJson::ElementList& result, WhitespaceStyle const& style,
                            JsonElement const& element) {
  auto const& whitespace = whitespace_before(style, element);
  if (!whitespace.empty())
    result.push_back(WhitespaceElement{whitespace});
  result.push_back(element);
}

std::expected<FormattedJson::ElementList, StarException>
insert_with_comma_and_formatting(FormattedJson::ElementList const& source, FormattedJson::ElementLocation at,
                                 bool array, std::initializer_list<JsonElement> elements) {
  // Resolve the original token boundary before constructing the replacement.
  at = last_index_of<ValueElement>(source, at);
  if (at == NPos)
    at = 0;
  else
    at += 1;
  bool empty = last_index_of<ValueElement>(source, source.size()) == NPos;
  bool append_comma = at == 0 && !empty;
  bool prepend_comma = !append_comma && !empty;
  auto style = detect_whitespace(source, at, array);
  if (!style)
    return std::unexpected(std::move(style.error()));

  auto inserted_count = elements.size();
  for (auto const& element : elements)
    inserted_count += !whitespace_before(*style, element).empty();
  if (!empty)
    inserted_count += 1 + !style->m_before_comma.empty();

  FormattedJson::ElementList result;
  result.reserve(source.size() + inserted_count);
  std::span<JsonElement const> original(source);
  result.append_range(original.first(at));
  if (prepend_comma)
    append_with_whitespace(result, *style, CommaElement{});
  for (auto const& element : elements)
    append_with_whitespace(result, *style, element);
  if (append_comma)
    append_with_whitespace(result, *style, CommaElement{});
  result.append_range(original.subspan(at));
  return result;
}

std::expected<FormattedJson, StarException> FormattedJson::prepend(String const& key, FormattedJson const& value) const {
  return insert_object(key, value, 0);
}

std::expected<FormattedJson, StarException> FormattedJson::insert_before(String const& key, FormattedJson const& value,
                                                                         String const& before_key) const {
  auto entry = m_object_entry_locations.find_ref(before_key);
  if (!entry)
    return std::unexpected(JsonException::format("Cannot insert before key \"{}\", which does not exist", before_key));
  return insert_object(key, value, entry->get().first);
}

std::expected<FormattedJson, StarException> FormattedJson::insert_after(String const& key, FormattedJson const& value,
                                                                        String const& after_key) const {
  auto entry = m_object_entry_locations.find_ref(after_key);
  if (!entry)
    return std::unexpected(JsonException::format("Cannot insert after key \"{}\", which does not exist", after_key));
  return insert_object(key, value, entry->get().second + 1);
}

std::expected<FormattedJson, StarException> FormattedJson::append(String const& key, FormattedJson const& value) const {
  return insert_object(key, value, m_elements.size());
}

std::expected<FormattedJson, StarException> FormattedJson::set(String const& key, FormattedJson const& value) const {
  return insert_object(key, value, m_elements.size());
}

void remove_value(FormattedJson::ElementList& elements, std::size_t prefix_loc, std::size_t value_loc) {
  // Calculate the complete interval in the original token coordinates. A
  // following comma owns subsequent whitespace; a final value owns its prefix.
  auto comma_loc = index_of<CommaElement>(elements, value_loc);
  std::size_t begin = prefix_loc;
  std::size_t end = value_loc + 1;
  if (comma_loc != NPos) {
    end = comma_loc + 1;
    while (end < elements.size() && std::holds_alternative<WhitespaceElement>(elements[end]))
      ++end;
  } else {
    comma_loc = last_index_of<CommaElement>(elements, prefix_loc);
    begin = comma_loc == NPos ? 0 : comma_loc;
  }
  elements.erase_at(begin, end);
}

std::expected<FormattedJson, StarException> FormattedJson::erase(String const& key) const {
  if (type() != Json::Type::Object)
    return std::unexpected(JsonException::format("Cannot call erase with key on FormattedJson type {}, must be Object type", typeName()));

  std::optional<std::pair<ElementLocation, ElementLocation>> maybe_entry = m_object_entry_locations.maybe(key);
  if (!maybe_entry.has_value())
    return *this;

  ElementList elements = m_elements;
  remove_value(elements, maybe_entry->first, maybe_entry->second);
  return object(std::move(elements));
}

std::expected<FormattedJson, StarException> FormattedJson::insert(std::size_t index, FormattedJson const& value) const {
  if (type() != Json::Type::Array)
    return std::unexpected(JsonException::format("Cannot call insert with index on FormattedJson type {}, must be Array type", typeName()));
  if (index > m_array_element_locations.size())
    return std::unexpected(JsonException::format("FormattedJson::insert({}) out of range", index));
  ElementLocation insert_position =
    index < m_array_element_locations.size() ? m_array_element_locations[index] : m_elements.size();
  auto inserted = insert_with_comma_and_formatting(m_elements, insert_position, true, {ValueElement{value}});
  if (!inserted)
    return std::unexpected(std::move(inserted.error()));
  return array(std::move(*inserted), m_array_element_locations.size() + 1);
}

std::expected<FormattedJson, StarException> FormattedJson::append(FormattedJson const& value) const {
  if (type() != Json::Type::Array)
    return std::unexpected(JsonException::format("Cannot call append on FormattedJson type {}, must be Array type", typeName()));
  auto inserted = insert_with_comma_and_formatting(m_elements, m_elements.size(), true, {ValueElement{value}});
  if (!inserted)
    return std::unexpected(std::move(inserted.error()));
  return array(std::move(*inserted), m_array_element_locations.size() + 1);
}

std::expected<FormattedJson, StarException> FormattedJson::set(std::size_t index, FormattedJson const& value) const {
  if (type() != Json::Type::Array)
    return std::unexpected(JsonException::format("Cannot call set with index on FormattedJson type {}, must be Array type", typeName()));
  if (index >= m_array_element_locations.size())
    return std::unexpected(JsonException::format("FormattedJson::set({}) out of range", index));
  ElementList elements = m_elements;
  elements[m_array_element_locations[index]] = ValueElement{value};
  return array(std::move(elements), m_array_element_locations.size());
}

std::expected<FormattedJson, StarException> FormattedJson::erase(std::size_t index) const {
  if (type() != Json::Type::Array)
    return std::unexpected(JsonException::format("Cannot call set with index on FormattedJson type {}, must be Array type", typeName()));
  if (index >= m_array_element_locations.size())
    return std::unexpected(JsonException::format("FormattedJson::eraseIndex({}) out of range", index));
  ElementList elements = m_elements;
  remove_value(elements, m_array_element_locations[index], m_array_element_locations[index]);
  return array(std::move(elements), m_array_element_locations.size() - 1);
}

std::expected<std::size_t, StarException> FormattedJson::size() const { return m_value.size(); }

std::expected<bool, StarException> FormattedJson::contains(String const& key) const { return m_value.contains(key); }

Json::Type FormattedJson::type() const { return m_value.type(); }

bool FormattedJson::isType(Json::Type type) const { return m_value.isType(type); }

String FormattedJson::typeName() const { return m_value.typeName(); }

std::expected<String, StarException> FormattedJson::toFormattedDouble() const {
  if (!isType(Json::Type::Float))
    return std::unexpected(JsonException::format("Cannot call toFormattedDouble on Json type {}, must be Float", typeName()));
  if (m_formatting.has_value())
    return *m_formatting;
  return toJson().repr();
}

std::expected<String, StarException> FormattedJson::toFormattedInt() const {
  if (!isType(Json::Type::Int))
    return std::unexpected(JsonException::format("Cannot call toFormattedInt on Json type {}, must be Int", typeName()));
  if (m_formatting.has_value())
    return *m_formatting;
  return toJson().repr();
}

std::expected<String, StarException> FormattedJson::repr() const {
  if (m_formatting.has_value())
    return *m_formatting;
  std::string result;
  JsonWriter<JsonUtf8StringOutputIterator, true> writer{JsonUtf8StringOutputIterator(result)};
  auto written = JsonStreamer<FormattedJson>::to_json_stream(*this, writer, false);
  if (!written)
    return std::unexpected(std::move(written.error()));
  return String(std::move(result));
}

std::expected<String, StarException> FormattedJson::printJson() const {
  if (type() != Json::Type::Object && type() != Json::Type::Array)
    return std::unexpected(JsonException("printJson called on non-top-level JSON type"));
  return repr();
}

FormattedJson::ElementList const& FormattedJson::elements() const { return m_elements; }

bool FormattedJson::operator==(FormattedJson const& v) const { return m_value == v.m_value; }

bool FormattedJson::operator!=(FormattedJson const& v) const {
  return !(*this == v);
}

std::expected<FormattedJson, StarException> FormattedJson::object(ElementList elements) {
  return Builder::rebuild(true, std::move(elements));
}

std::expected<FormattedJson, StarException> FormattedJson::array(ElementList elements,
                                                                 std::optional<std::size_t> value_count) {
  return Builder::rebuild(false, std::move(elements), value_count);
}

std::expected<FormattedJson, StarException> FormattedJson::insert_object(String const& key, FormattedJson const& value,
                                                                         ElementLocation loc) const {
  if (type() != Json::Type::Object)
    return std::unexpected(JsonException::format("Cannot call set with key on FormattedJson type {}, must be Object type", typeName()));
  auto entry = m_object_entry_locations.maybe(key);
  if (entry) {
    ElementList elements = m_elements;
    elements[entry->second] = ValueElement{value};
    return object(std::move(elements));
  }
  auto inserted = insert_with_comma_and_formatting(m_elements, loc, false,
                                                   {ObjectKeyElement{key}, ColonElement{}, ValueElement{value}});
  if (!inserted)
    return std::unexpected(std::move(inserted.error()));
  return object(std::move(*inserted));
}

std::expected<std::reference_wrapper<FormattedJson const>, StarException>
FormattedJson::get_formatted_json(ElementLocation loc) const {
  if (loc >= m_elements.size())
    return std::unexpected(JsonException("FormattedJson element location out of range"));
  if (!std::holds_alternative<ValueElement>(m_elements[loc]))
    return std::unexpected(JsonException("Invalid FormattedJson value element"));
  auto const& value = std::get<ValueElement>(m_elements[loc]).m_value;
  if (!value)
    return std::unexpected(JsonException("Invalid FormattedJson value element"));
  return std::cref(*value);
}

std::expected<void, StarException> FormattedJsonBuilderStream::begin_object() {
  push(true);
  return {};
}

std::expected<void, StarException> FormattedJsonBuilderStream::object_key(std::span<String::Char const> s) {
  auto key = String::create(std::u32string_view(s.data(), s.size()));
  if (!key)
    return std::unexpected(std::move(key.error()));
  auto container = current();
  if (!container)
    return std::unexpected(std::move(container.error()));
  return container->get().append(ObjectKeyElement{std::move(*key)});
}

std::expected<void, StarException> FormattedJsonBuilderStream::object_key_utf8(std::string_view s) {
  auto valid = utf8_length(s);
  if (!valid)
    return std::unexpected(std::move(valid.error()));
  auto container = current();
  if (!container)
    return std::unexpected(std::move(container.error()));
  return container->get().append(ObjectKeyElement{String(s)});
}

std::expected<void, StarException> FormattedJsonBuilderStream::end_object() {
  auto value = pop();
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (!m_stack.empty())
    return m_stack.back().append(ValueElement{std::move(*value)});
  m_root = std::move(*value);
  return {};
}

std::expected<void, StarException> FormattedJsonBuilderStream::begin_array() {
  push(false);
  return {};
}

std::expected<void, StarException> FormattedJsonBuilderStream::end_array() { return end_object(); }

std::expected<void, StarException> FormattedJsonBuilderStream::put_string(std::span<String::Char const> s) {
  auto text = String::create(std::u32string_view(s.data(), s.size()));
  if (!text)
    return std::unexpected(std::move(text.error()));
  return put_value(Json(std::move(*text)));
}

std::expected<void, StarException> FormattedJsonBuilderStream::put_string_utf8(std::string_view s) {
  auto valid = utf8_length(s);
  if (!valid)
    return std::unexpected(std::move(valid.error()));
  return put_value(Json(String(s)));
}

std::expected<void, StarException> FormattedJsonBuilderStream::put_double(std::string_view s) {
  auto number = lexical_cast<double>(s);
  if (!number)
    return std::unexpected(JsonParserException("bad double", ExceptionDiagnostic(number.error())));
  return put_value(Json(*number), String(s));
}

std::expected<void, StarException> FormattedJsonBuilderStream::put_integer(std::string_view s) {
  auto number = lexical_cast<long long>(s);
  if (!number)
    return std::unexpected(JsonParserException("bad integer", ExceptionDiagnostic(number.error())));
  return put_value(Json(*number), String(s));
}

std::expected<void, StarException> FormattedJsonBuilderStream::put_boolean(bool b) { return put_value(Json(b)); }

std::expected<void, StarException> FormattedJsonBuilderStream::put_null() { return put_value(Json()); }

std::expected<void, StarException> FormattedJsonBuilderStream::put_whitespace(std::span<String::Char const> s) {
  if (m_stack.empty())
    return {};
  auto whitespace = String::create(std::u32string_view(s.data(), s.size()));
  if (!whitespace)
    return std::unexpected(std::move(whitespace.error()));
  return m_stack.back().append(WhitespaceElement{std::move(*whitespace)});
}

std::expected<void, StarException> FormattedJsonBuilderStream::put_whitespace_utf8(std::string_view s) {
  auto valid = utf8_length(s);
  if (!valid)
    return std::unexpected(std::move(valid.error()));
  if (m_stack.empty())
    return {};
  return m_stack.back().append(WhitespaceElement{String(s)});
}

std::expected<void, StarException> FormattedJsonBuilderStream::put_colon() {
  auto container = current();
  if (!container)
    return std::unexpected(std::move(container.error()));
  return container->get().append(ColonElement{});
}

std::expected<void, StarException> FormattedJsonBuilderStream::put_comma() {
  auto container = current();
  if (!container)
    return std::unexpected(std::move(container.error()));
  return container->get().append(CommaElement{});
}

std::expected<FormattedJson, StarException> FormattedJsonBuilderStream::take_top() {
  if (!m_root || !m_stack.empty())
    return std::unexpected(JsonParsingException("Incomplete formatted JSON builder stream"));
  FormattedJson result = std::move(*m_root);
  m_root.reset();
  return result;
}

void FormattedJsonBuilderStream::push(bool object) { m_stack.emplace_back(object); }

std::expected<FormattedJson, StarException> FormattedJsonBuilderStream::pop() {
  if (m_stack.empty())
    return std::unexpected(JsonParsingException("Missing formatted JSON builder container"));
  FormattedJson result = m_stack.back().finish();
  m_stack.pop_back();
  return result;
}

std::expected<std::reference_wrapper<FormattedJson::Builder>, StarException> FormattedJsonBuilderStream::current() {
  if (m_stack.empty())
    return std::unexpected(JsonParsingException("Missing formatted JSON builder container"));
  return std::ref(m_stack.back());
}

std::expected<void, StarException> FormattedJsonBuilderStream::put_value(Json const& value,
                                                                         std::optional<String> formatting) {
  auto formatted = FormattedJson::create(value);
  if (!formatted)
    return std::unexpected(std::move(formatted.error()));
  if (formatting) {
    if constexpr (DebugEnabled) {
      auto parsed = Json::parse(*formatting);
      if (!parsed)
        return std::unexpected(std::move(parsed.error()));
      Star::star_assert_failed(*parsed == formatted->toJson());
    }
    formatted->m_formatting = std::move(*formatting);
  }
  if (!m_stack.empty())
    return m_stack.back().append(ValueElement{std::move(*formatted)});
  m_root = std::move(*formatted);
  return {};
}

std::expected<void, StarException> JsonStreamer<FormattedJson>::to_json_stream(FormattedJson const& val,
                                                                               JsonStream& stream, bool sort) {
  bool object = val.isType(Json::Type::Object);
  bool array = val.isType(Json::Type::Array);
  if (!object && !array) {
    if (val.m_formatting) {
      auto const& text = val.m_formatting->utf8();
      return val.isType(Json::Type::Float) ? stream.put_double(text) : stream.put_integer(text);
    }
    return JsonStreamer<Json>::to_json_stream(val.toJson(), stream, sort);
  }
  auto begun = object ? stream.begin_object() : stream.begin_array();
  if (!begun)
    return std::unexpected(std::move(begun.error()));
  for (auto const& elem : val.elements()) {
    std::expected<void, StarException> written;
    if (std::holds_alternative<ObjectKeyElement>(elem)) {
      written = stream.object_key_utf8(std::get<ObjectKeyElement>(elem).m_key.utf8());
    } else if (std::holds_alternative<WhitespaceElement>(elem)) {
      written = stream.put_whitespace_utf8(std::get<WhitespaceElement>(elem).m_whitespace.utf8());
    } else if (std::holds_alternative<ColonElement>(elem)) {
      written = stream.put_colon();
    } else if (std::holds_alternative<CommaElement>(elem)) {
      written = stream.put_comma();
    } else {
      auto const& value = std::get<ValueElement>(elem).m_value;
      if (!value)
        return std::unexpected(JsonException("Invalid empty FormattedJson value element"));
      written = to_json_stream(*value, stream, sort);
    }
    if (!written)
      return std::unexpected(std::move(written.error()));
  }
  return object ? stream.end_object() : stream.end_array();
}

std::expected<void, StarException> write_text(TextBuffer& output, JsonElement const& elem) {
  if (std::holds_alternative<ValueElement>(elem)) {
    output.append("ValueElement{");
    auto const& value = std::get<ValueElement>(elem).m_value;
    if (value)
      std::format_to(std::back_inserter(output), "{}", static_cast<void const*>(value.get()));
    else
      output.push_back('0');
    output.push_back('}');
  } else if (std::holds_alternative<ObjectKeyElement>(elem)) {
    output.append("ObjectKeyElement{");
    append_text(output, std::get<ObjectKeyElement>(elem).m_key);
    output.push_back('}');
  } else if (std::holds_alternative<WhitespaceElement>(elem)) {
    output.append("WhitespaceElement{");
    append_text(output, std::get<WhitespaceElement>(elem).m_whitespace);
    output.push_back('}');
  } else if (std::holds_alternative<ColonElement>(elem)) {
    output.append("ColonElement{}");
  } else {
    output.append("CommaElement{}");
  }
  return {};
}

std::expected<void, StarException> write_text(TextBuffer& output, FormattedJson const& json) {
  auto text = json.repr();
  if (!text)
    return std::unexpected(std::move(text.error()));
  append_text(output, *text);
  return {};
}

}
