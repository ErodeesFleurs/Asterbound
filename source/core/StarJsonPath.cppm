module;
import std;
import star.string;
import star.unicode;
import star.format;
import star.json;
import star.json_extra;

import star.lexical_cast;

namespace Star {

namespace JsonPath {
  enum class TypeHint {
    Array,
    Object
  };

  using PathParser = std::function<std::expected<TypeHint, StarException>(String&, String const&, Utf8Cursor&)>;

  struct ParsingExceptionTag {
    static constexpr std::string_view name() { return "ParsingException"; }
  };
  using ParsingException = StarError<ParsingExceptionTag, JsonException>;
  struct TraversalExceptionTag {
    static constexpr std::string_view name() { return "TraversalException"; }
  };
  using TraversalException = StarError<TraversalExceptionTag, JsonException>;

  // Parses RFC 6901 JSON Pointers, e.g. /foo/bar/4/baz
  std::expected<TypeHint, StarException> parse_pointer(String& buffer, String const& path, Utf8Cursor& cursor);

  // Parses JavaScript-like paths, e.g. foo.bar[4].baz
  std::expected<TypeHint, StarException> parse_query_path(String& buffer, String const& path, Utf8Cursor& cursor);

  template <typename JsonLike>
  concept json_path_readable = std::copy_constructible<JsonLike> && std::assignable_from<JsonLike&, JsonLike>
    && requires(JsonLike const& value, String const& key, std::size_t index) {
         { value.type() } -> std::same_as<Json::Type>;
         { value.size() } -> std::same_as<std::expected<std::size_t, StarException>>;
         { value.contains(key) } -> std::same_as<std::expected<bool, StarException>>;
         { value.get(key) } -> std::same_as<std::expected<JsonLike, StarException>>;
         { value.get(index) } -> std::same_as<std::expected<JsonLike, StarException>>;
       };

  template <typename JsonLike>
  concept json_path_rebuildable = json_path_readable<JsonLike>
    && requires(JsonLike const& value, String key, std::size_t index, JsonLike replacement) {
         { value.set(std::move(key), std::move(replacement)) } -> std::same_as<std::expected<JsonLike, StarException>>;
         { value.set(index, std::move(replacement)) } -> std::same_as<std::expected<JsonLike, StarException>>;
       };

  template <typename JsonLike>
  concept json_path_settable = json_path_rebuildable<JsonLike>
    && requires(JsonLike const& value, String const& key, std::size_t index, JsonLike const& replacement) {
         { value.set(key, replacement) } -> std::same_as<std::expected<JsonLike, StarException>>;
         { value.set(index, replacement) } -> std::same_as<std::expected<JsonLike, StarException>>;
         { value.append(replacement) } -> std::same_as<std::expected<JsonLike, StarException>>;
       };

  template <typename JsonLike>
  concept json_path_removable = json_path_rebuildable<JsonLike> && std::default_initializable<JsonLike>
    && requires(JsonLike const& value, String const& key, std::size_t index) {
         { value.erase(key) } -> std::same_as<std::expected<JsonLike, StarException>>;
         { value.erase(index) } -> std::same_as<std::expected<JsonLike, StarException>>;
       };

  template <typename JsonLike>
  concept json_path_addable =
    json_path_settable<JsonLike> && requires(JsonLike const& value, std::size_t index, JsonLike const& replacement) {
      { value.insert(index, replacement) } -> std::same_as<std::expected<JsonLike, StarException>>;
    };

  template <typename Parser>
  concept json_path_parser = requires(Parser& parser, String& buffer, String const& path, Utf8Cursor& cursor) {
    { std::invoke(parser, buffer, path, cursor) } -> std::same_as<std::expected<TypeHint, StarException>>;
  };

  template <typename Operation, typename JsonLike>
  concept json_parent_operation =
    requires(Operation& operation, JsonLike const& value, std::optional<String> const& key) {
      { std::invoke(operation, value, key) } -> std::same_as<std::expected<JsonLike, StarException>>;
    };

  template <typename Operation, typename JsonLike>
  concept json_root_operation = requires(Operation& operation, JsonLike const& value) {
    { std::invoke(operation, value) } -> std::same_as<std::expected<JsonLike, StarException>>;
  };

  template <typename Operation, typename JsonLike>
  concept json_object_operation = requires(Operation& operation, JsonLike const& value, String const& key) {
    { std::invoke(operation, value, key) } -> std::same_as<std::expected<JsonLike, StarException>>;
  };

  template <typename Operation, typename JsonLike>
  concept json_array_operation =
    requires(Operation& operation, JsonLike const& value, std::optional<std::size_t> index) {
      { std::invoke(operation, value, std::move(index)) } -> std::same_as<std::expected<JsonLike, StarException>>;
    };

  template <typename Operation>
  concept json_update_operation = requires(Operation& operation, Json const& value) {
    { std::invoke(operation, value) } -> std::same_as<std::expected<std::optional<Json>, StarException>>;
  };

  namespace Detail {
  template <typename Callable>
  concept nullable_callable =
    !std::is_function_v<std::remove_reference_t<Callable>> && requires(Callable const& callable) {
      { static_cast<bool>(callable) } -> std::same_as<bool>;
    };

  template <typename... Arguments, typename Callable>
    requires std::invocable<Callable&, Arguments...>
  bool callable_available(Callable& callable) {
    if constexpr (nullable_callable<Callable>)
      return static_cast<bool>(callable);
    else
      return true;
  }

  template <bool optional_misses, json_path_parser Parser>
  std::expected<std::optional<std::reference_wrapper<Json const>>, StarException>
  find_path(Json const& base, Parser& parser, String const& path) {
    if (!callable_available<String&, String const&, Utf8Cursor&>(parser))
      return std::unexpected(ParsingException("Cannot invoke an empty Json path parser"));
    String buffer;
    buffer.reserve(path.utf8_size());
    Utf8Cursor cursor(path.utf8());
    auto value = std::cref(base);
    while (!cursor.at_end()) {
      auto parsed = std::invoke(parser, buffer, path, cursor);
      if (!parsed)
        return std::unexpected(std::move(parsed.error()));
      if (value.get().type() == Json::Type::Array) {
        if (buffer == "-") {
          if constexpr (optional_misses)
            return std::nullopt;
          return std::unexpected(
            TraversalException::format("Tried to get key '{}' in non-object type in pathGet(\"{}\")", buffer, path));
        }
        auto index = maybe_lexical_cast<std::size_t>(buffer);
        if (!index) {
          if constexpr (optional_misses)
            return std::nullopt;
          return std::unexpected(
            TraversalException::format("Cannot parse '{}' as index in pathGet(\"{}\")", buffer, path));
        }
        auto child = value.get().find(*index);
        if (!child)
          return std::unexpected(std::move(child.error()));
        if (!*child) {
          if constexpr (optional_misses)
            return std::nullopt;
          return std::unexpected(TraversalException::format("Index {} out of range in pathGet(\"{}\")", buffer, path));
        }
        value = **child;
      } else if (value.get().type() == Json::Type::Object) {
        auto child = value.get().find(buffer);
        if (!child)
          return std::unexpected(std::move(child.error()));
        if (!*child) {
          if constexpr (optional_misses)
            return std::nullopt;
          return std::unexpected(TraversalException::format("No such key '{}' in pathGet(\"{}\")", buffer, path));
        }
        value = **child;
      } else {
        if constexpr (optional_misses)
          return std::nullopt;
        return std::unexpected(
          TraversalException::format("Tried to get key '{}' in non-object type in pathGet(\"{}\")", buffer, path));
      }
    }
    return value;
  }
  }// namespace Detail

  // Lexical borrows into a retained immutable document. Present null is not a miss.
  template <json_path_parser Parser>
  std::expected<std::reference_wrapper<Json const>, StarException> path_get_ref(Json const& base, Parser&& parser,
                                                                                String const& path) {
    auto value = Detail::find_path<false>(base, parser, path);
    if (!value)
      return std::unexpected(std::move(value.error()));
    return **value;
  }

  template <json_path_parser Parser>
  std::expected<std::reference_wrapper<Json const>, StarException> path_get_ref(Json const&& base, Parser&& parser,
                                                                                String const& path) = delete;

  template <json_path_parser Parser>
  std::expected<std::optional<std::reference_wrapper<Json const>>, StarException>
  path_find_ref(Json const& base, Parser&& parser, String const& path) {
    return Detail::find_path<true>(base, parser, path);
  }

  template <json_path_parser Parser>
  std::expected<std::optional<std::reference_wrapper<Json const>>, StarException>
  path_find_ref(Json const&& base, Parser&& parser, String const& path) = delete;

  // Retrieves the portion of the Json document referred to by the given path.
  template <json_path_readable JsonLike, json_path_parser Parser>
  std::expected<JsonLike, StarException> path_get(JsonLike const& base, Parser&& parser, String const& path) {
    if constexpr (std::same_as<JsonLike, Json>) {
      auto value = path_get_ref(base, parser, path);
      if (!value)
        return std::unexpected(std::move(value.error()));
      return value->get();
    }
    JsonLike value = base;
    if (!Detail::callable_available<String&, String const&, Utf8Cursor&>(parser))
      return std::unexpected(ParsingException("Cannot invoke an empty Json path parser"));
    String buffer;
    buffer.reserve(path.utf8_size());
    Utf8Cursor cursor(path.utf8());
    while (!cursor.at_end()) {
      auto parsed = std::invoke(parser, buffer, path, cursor);
      if (!parsed)
        return std::unexpected(std::move(parsed.error()));
      if (value.type() == Json::Type::Array) {
        if (buffer == "-")
          return std::unexpected(TraversalException::format("Tried to get key '{}' in non-object type in pathGet(\"{}\")", buffer, path));
        auto index = maybe_lexical_cast<std::size_t>(buffer);
        if (!index)
          return std::unexpected(TraversalException::format("Cannot parse '{}' as index in pathGet(\"{}\")", buffer, path));
        auto size = value.size();
        if (!size)
          return std::unexpected(std::move(size.error()));
        if (*index >= *size)
          return std::unexpected(TraversalException::format("Index {} out of range in pathGet(\"{}\")", buffer, path));
        auto child = value.get(*index);
        if (!child)
          return std::unexpected(std::move(child.error()));
        value = std::move(*child);
      } else if (value.type() == Json::Type::Object) {
        auto contains = value.contains(buffer);
        if (!contains)
          return std::unexpected(std::move(contains.error()));
        if (!*contains)
          return std::unexpected(TraversalException::format("No such key '{}' in pathGet(\"{}\")", buffer, path));
        auto child = value.get(buffer);
        if (!child)
          return std::unexpected(std::move(child.error()));
        value = std::move(*child);
      } else {
        return std::unexpected(TraversalException::format("Tried to get key '{}' in non-object type in pathGet(\"{}\")", buffer, path));
      }
    }
    return value;
  }

  // Optional misses are normal results, not diagnostics to construct and discard.
  template <json_path_readable JsonLike, json_path_parser Parser>
  std::expected<std::optional<JsonLike>, StarException> path_find(JsonLike const& base, Parser&& parser,
                                                                  String const& path) {
    if constexpr (std::same_as<JsonLike, Json>) {
      auto value = path_find_ref(base, parser, path);
      if (!value)
        return std::unexpected(std::move(value.error()));
      if (*value)
        return std::optional<JsonLike>((*value)->get());
      return std::optional<JsonLike>{};
    }
    JsonLike value = base;
    if (!Detail::callable_available<String&, String const&, Utf8Cursor&>(parser))
      return std::unexpected(ParsingException("Cannot invoke an empty Json path parser"));
    String buffer;
    buffer.reserve(path.utf8_size());
    Utf8Cursor cursor(path.utf8());
    while (!cursor.at_end()) {
      auto parsed = std::invoke(parser, buffer, path, cursor);
      if (!parsed)
        return std::unexpected(std::move(parsed.error()));
      if (value.type() == Json::Type::Array) {
        if (buffer == "-")
          return std::optional<JsonLike>{};
        auto index = maybe_lexical_cast<std::size_t>(buffer);
        if (!index)
          return std::optional<JsonLike>{};
        auto size = value.size();
        if (!size)
          return std::unexpected(std::move(size.error()));
        if (*index >= *size)
          return std::optional<JsonLike>{};
        auto child = value.get(*index);
        if (!child)
          return std::unexpected(std::move(child.error()));
        value = std::move(*child);
      } else if (value.type() == Json::Type::Object) {
        auto contains = value.contains(buffer);
        if (!contains)
          return std::unexpected(std::move(contains.error()));
        if (!*contains)
          return std::optional<JsonLike>{};
        auto child = value.get(buffer);
        if (!child)
          return std::unexpected(std::move(child.error()));
        value = std::move(*child);
      } else {
        return std::optional<JsonLike>{};
      }
    }
    return std::optional<JsonLike>(std::move(value));
  }

  namespace Detail {
  template <json_path_rebuildable JsonLike, json_path_parser Parser, typename Operation>
    requires json_parent_operation<Operation, JsonLike>
  std::expected<JsonLike, StarException> apply_path(String& buffer, JsonLike const& value, Parser& parser,
                                                    String const& path, Utf8Cursor cursor, Operation& operation) {
    if (cursor.at_end())
      return std::invoke(operation, value, std::optional<String>{});
    auto parsed = std::invoke(parser, buffer, path, cursor);
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    if (value.type() == Json::Type::Array) {
      if (cursor.at_end())
        return std::invoke(operation, value, std::optional<String>(std::move(buffer)));
      auto index = maybe_lexical_cast<std::size_t>(buffer);
      if (!index)
        return std::unexpected(
          TraversalException::format("Cannot parse '{}' as index in pathApply(\"{}\")", buffer, path));
      if constexpr (std::same_as<JsonLike, Json>) {
        auto child = value.find(*index);
        if (!child)
          return std::unexpected(std::move(child.error()));
        if (!*child)
          return std::unexpected(
            TraversalException::format("Index {} out of range in pathApply(\"{}\")", buffer, path));
        auto changed = apply_path(buffer, (**child).get(), parser, path, cursor, operation);
        if (!changed)
          return std::unexpected(std::move(changed.error()));
        return value.set(*index, std::move(*changed));
      } else {
        auto size = value.size();
        if (!size)
          return std::unexpected(std::move(size.error()));
        if (*index >= *size)
          return std::unexpected(
            TraversalException::format("Index {} out of range in pathApply(\"{}\")", buffer, path));
        auto child = value.get(*index);
        if (!child)
          return std::unexpected(std::move(child.error()));
        auto changed = apply_path(buffer, *child, parser, path, cursor, operation);
        if (!changed)
          return std::unexpected(std::move(changed.error()));
        return value.set(*index, std::move(*changed));
      }
    } else if (value.type() == Json::Type::Object) {
      if (cursor.at_end())
        return std::invoke(operation, value, std::optional<String>(std::move(buffer)));
      if constexpr (std::same_as<JsonLike, Json>) {
        auto child = value.find(buffer);
        if (!child)
          return std::unexpected(std::move(child.error()));
        if (!*child)
          return std::unexpected(TraversalException::format("No such key '{}' in pathApply(\"{}\")", buffer, path));
        String key = std::move(buffer);
        auto changed = apply_path(buffer, (**child).get(), parser, path, cursor, operation);
        if (!changed)
          return std::unexpected(std::move(changed.error()));
        return value.set(std::move(key), std::move(*changed));
      } else {
        auto contains = value.contains(buffer);
        if (!contains)
          return std::unexpected(std::move(contains.error()));
        if (!*contains)
          return std::unexpected(TraversalException::format("No such key '{}' in pathApply(\"{}\")", buffer, path));
        auto child = value.get(buffer);
        if (!child)
          return std::unexpected(std::move(child.error()));
        String key = std::move(buffer);
        auto changed = apply_path(buffer, *child, parser, path, cursor, operation);
        if (!changed)
          return std::unexpected(std::move(changed.error()));
        return value.set(std::move(key), std::move(*changed));
      }
    }
    return std::unexpected(
      TraversalException::format("Tried to get key '{}' in non-object type in pathApply(\"{}\")", buffer, path));
  }

  template <json_path_readable JsonLike, typename RootOperation, typename ObjectOperation, typename ArrayOperation>
    requires json_root_operation<RootOperation, JsonLike> && json_object_operation<ObjectOperation, JsonLike>
    && json_array_operation<ArrayOperation, JsonLike>
  auto combine_operations(String const& path, RootOperation& root_operation, ObjectOperation& object_operation,
                          ArrayOperation& array_operation) {
    return [&](JsonLike const& parent, std::optional<String> const& key) -> std::expected<JsonLike, StarException> {
      if (!key) {
        if (!callable_available<JsonLike const&>(root_operation))
          return std::unexpected(TraversalException("Cannot invoke an empty Json path operation"));
        return std::invoke(root_operation, parent);
      }
      if (parent.type() == Json::Type::Array) {
        if (!callable_available<JsonLike const&, std::optional<std::size_t>>(array_operation))
          return std::unexpected(TraversalException("Cannot invoke an empty Json array operation"));
        if (*key == "-")
          return std::invoke(array_operation, parent, std::optional<std::size_t>{});
        auto index = maybe_lexical_cast<std::size_t>(*key);
        if (!index)
          return std::unexpected(
            TraversalException::format("Cannot parse '{}' as index in Json path \"{}\"", *key, path));
        auto size = parent.size();
        if (!size)
          return std::unexpected(std::move(size.error()));
        if (*index > *size)
          return std::unexpected(TraversalException::format("Index {} out of range in Json path \"{}\"", *key, path));
        if (*index == *size)
          index = {};
        return std::invoke(array_operation, parent, std::move(index));
      } else if (parent.type() == Json::Type::Object) {
        if (!callable_available<JsonLike const&, String const&>(object_operation))
          return std::unexpected(TraversalException("Cannot invoke an empty Json object operation"));
        return std::invoke(object_operation, parent, *key);
      }
      return std::unexpected(
        TraversalException::format("Tried to set key '{}' in non-object type in pathSet(\"{}\")", *key, path));
    };
  }
  }// namespace Detail

  // Applies an operation to the parent and final key; root paths have no key.
  // Concrete callbacks are borrowed only for this synchronous traversal.
  template <json_path_rebuildable JsonLike, json_path_parser Parser, typename Operation>
    requires json_parent_operation<Operation, JsonLike>
  std::expected<JsonLike, StarException> path_apply(JsonLike const& base, Parser&& parser, String const& path,
                                                    Operation&& operation) {
    if (!Detail::callable_available<String&, String const&, Utf8Cursor&>(parser))
      return std::unexpected(ParsingException("Cannot invoke an empty Json path parser"));
    if (!Detail::callable_available<JsonLike const&, std::optional<String> const&>(operation))
      return std::unexpected(TraversalException("Cannot invoke an empty Json path operation"));
    String buffer;
    buffer.reserve(path.utf8_size());
    return Detail::apply_path(buffer, base, parser, path, Utf8Cursor(path.utf8()), operation);
  }

  template <json_path_rebuildable JsonLike, json_path_parser Parser, typename RootOperation, typename ObjectOperation,
            typename ArrayOperation>
    requires json_root_operation<RootOperation, JsonLike> && json_object_operation<ObjectOperation, JsonLike>
    && json_array_operation<ArrayOperation, JsonLike>
  std::expected<JsonLike, StarException> path_apply(JsonLike const& base, Parser&& parser, String const& path,
                                                    RootOperation&& root_operation, ObjectOperation&& object_operation,
                                                    ArrayOperation&& array_operation) {
    auto combined_operation =
      Detail::combine_operations<JsonLike>(path, root_operation, object_operation, array_operation);
    return path_apply(base, parser, path, combined_operation);
  }

  template <json_path_settable JsonLike, json_path_parser Parser>
  std::expected<JsonLike, StarException> path_set(JsonLike const& base, Parser&& parser, String const& path,
                                                  JsonLike const& value) {
    auto root_operation = [&value](JsonLike const&) -> std::expected<JsonLike, StarException> { return value; };
    auto object_operation = [&value](JsonLike const& object, String const& key) { return object.set(key, value); };
    auto array_operation = [&value](JsonLike const& array, std::optional<std::size_t> index) {
      return index ? array.set(*index, value) : array.append(value);
    };
    return path_apply(base, parser, path,
                      Detail::combine_operations<JsonLike>(path, root_operation, object_operation, array_operation));
  }

  template <json_path_removable JsonLike, json_path_parser Parser>
  std::expected<JsonLike, StarException> path_remove(JsonLike const& base, Parser&& parser, String const& path) {
    auto root_operation = [](JsonLike const&) -> std::expected<JsonLike, StarException> { return JsonLike{}; };
    auto object_operation = [](JsonLike const& object, String const& key) -> std::expected<JsonLike, StarException> {
      auto contains = object.contains(key);
      if (!contains)
        return std::unexpected(std::move(contains.error()));
      if (!*contains)
        return std::unexpected(TraversalException::format("Could not find \"{}\" to remove", key));
      return object.erase(key);
    };
    auto array_operation = [](JsonLike const& array,
                              std::optional<std::size_t> index) -> std::expected<JsonLike, StarException> {
      if (index)
        return array.erase(*index);
      return std::unexpected(TraversalException("Could not remove element after end of array"));
    };
    return path_apply(base, parser, path,
                      Detail::combine_operations<JsonLike>(path, root_operation, object_operation, array_operation));
  }

  template <json_path_addable JsonLike, json_path_parser Parser>
  std::expected<JsonLike, StarException> path_add(JsonLike const& base, Parser&& parser, String const& path,
                                                  JsonLike const& value) {
    auto root_operation = [&value](JsonLike const& document) -> std::expected<JsonLike, StarException> {
      if (document.type() == Json::Type::Null)
        return value;
      return std::unexpected(JsonException("Cannot add a value to the entire document, it is not empty."));
    };
    auto object_operation = [&value](JsonLike const& object, String const& key) { return object.set(key, value); };
    auto array_operation = [&value](JsonLike const& array, std::optional<std::size_t> index) {
      return index ? array.insert(*index, value) : array.append(value);
    };
    return path_apply(base, parser, path,
                      Detail::combine_operations<JsonLike>(path, root_operation, object_operation, array_operation));
  }

  namespace Detail {
  template <json_path_parser Parser, json_update_operation Operation>
  std::expected<std::optional<Json>, StarException> update_path(String& buffer, Json const& value, Parser& parser,
                                                                String const& path, Utf8Cursor cursor,
                                                                Operation& operation) {
    if (cursor.at_end())
      return std::invoke(operation, value);
    auto parsed = std::invoke(parser, buffer, path, cursor);
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    if (value.type() == Json::Type::Array) {
      if (buffer == "-")
        return std::unexpected(
          TraversalException::format("Tried to get key '{}' in non-object type in pathGet(\"{}\")", buffer, path));
      auto index = maybe_lexical_cast<std::size_t>(buffer);
      if (!index)
        return std::unexpected(
          TraversalException::format("Cannot parse '{}' as index in pathGet(\"{}\")", buffer, path));
      auto child = value.find(*index);
      if (!child)
        return std::unexpected(std::move(child.error()));
      if (!*child)
        return std::unexpected(TraversalException::format("Index {} out of range in pathGet(\"{}\")", buffer, path));
      auto changed = update_path(buffer, (**child).get(), parser, path, cursor, operation);
      if (!changed || !*changed)
        return changed;
      auto rebuilt = value.set(*index, std::move(**changed));
      if (!rebuilt)
        return std::unexpected(std::move(rebuilt.error()));
      return std::optional<Json>(std::move(*rebuilt));
    } else if (value.type() == Json::Type::Object) {
      auto object = value.object_ptr();
      if (!object)
        return std::unexpected(std::move(object.error()));
      auto child = (*object)->find(buffer);
      if (child == (*object)->end())
        return std::unexpected(TraversalException::format("No such key '{}' in pathGet(\"{}\")", buffer, path));
      auto changed = update_path(buffer, child->second, parser, path, cursor, operation);
      if (!changed || !*changed)
        return changed;
      auto rebuilt = value.set(child->first, std::move(**changed));
      if (!rebuilt)
        return std::unexpected(std::move(rebuilt.error()));
      return std::optional<Json>(std::move(*rebuilt));
    }
    return std::unexpected(
      TraversalException::format("Tried to get key '{}' in non-object type in pathGet(\"{}\")", buffer, path));
  }

  struct PointerParser {
    std::expected<TypeHint, StarException> operator()(String& buffer, String const& path, Utf8Cursor& cursor) const {
      return parse_pointer(buffer, path, cursor);
    }
  };

  struct QueryParser {
    std::expected<TypeHint, StarException> operator()(String& buffer, String const& path, Utf8Cursor& cursor) const {
      return parse_query_path(buffer, path, cursor);
    }
  };

  template <typename Visitor>
  concept parser_visitor = std::invocable<Visitor, PointerParser const&> && std::invocable<Visitor, QueryParser const&>
    && std::invocable<Visitor, PathParser const&>
    && std::same_as<std::invoke_result_t<Visitor, PointerParser const&>,
                    std::invoke_result_t<Visitor, QueryParser const&>>
    && std::same_as<std::invoke_result_t<Visitor, PointerParser const&>,
                    std::invoke_result_t<Visitor, PathParser const&>>;
  }// namespace Detail

  // Borrows the parser, path and callback for one strict synchronous update.
  template <json_path_parser Parser, json_update_operation Operation>
  std::expected<std::optional<Json>, StarException> path_update(Json const& base, Parser&& parser, String const& path,
                                                                Operation&& operation) {
    if (!Detail::callable_available<String&, String const&, Utf8Cursor&>(parser))
      return std::unexpected(ParsingException("Cannot invoke an empty Json path parser"));
    if (!Detail::callable_available<Json const&>(operation))
      return std::unexpected(TraversalException("Cannot invoke an empty Json path operation"));
    String buffer;
    buffer.reserve(path.utf8_size());
    return Detail::update_path(buffer, base, parser, path, Utf8Cursor(path.utf8()), operation);
  }

  class Path {
  public:
    Path(PathParser parser, String const& path) : m_parser(std::move(parser)), m_path(path) {}

    template <json_path_readable JsonLike> std::expected<JsonLike, StarException> get(JsonLike const& base) const {
      return with_parser([&](auto const& parser) { return path_get(base, parser, m_path); });
    }

    template <json_path_rebuildable JsonLike, typename Operation>
      requires json_parent_operation<Operation, JsonLike>
    std::expected<JsonLike, StarException> apply(JsonLike const& base, Operation&& operation) const {
      return with_parser([&](auto const& parser) { return path_apply(base, parser, m_path, operation); });
    }

    template <json_path_rebuildable JsonLike, typename RootOperation, typename ObjectOperation, typename ArrayOperation>
      requires json_root_operation<RootOperation, JsonLike> && json_object_operation<ObjectOperation, JsonLike>
      && json_array_operation<ArrayOperation, JsonLike>
    std::expected<JsonLike, StarException> apply(JsonLike const& base, RootOperation&& root_operation,
                                                 ObjectOperation&& object_operation,
                                                 ArrayOperation&& array_operation) const {
      return with_parser([&](auto const& parser) {
        return path_apply(base, parser, m_path, root_operation, object_operation, array_operation);
      });
    }

    // Resolves a strict target with path_get_ref diagnostics and lazy parsing.
    // nullopt propagates without rebuilding; a replacement rebuilds each ancestor once.
    template <json_update_operation Operation>
    std::expected<std::optional<Json>, StarException> update(Json const& base, Operation&& operation) const {
      return with_parser([&](auto const& parser) { return path_update(base, parser, m_path, operation); });
    }

    template <json_path_settable JsonLike>
    std::expected<JsonLike, StarException> set(JsonLike const& base, JsonLike const& value) const {
      return with_parser([&](auto const& parser) { return path_set(base, parser, m_path, value); });
    }

    template <json_path_removable JsonLike> std::expected<JsonLike, StarException> remove(JsonLike const& base) const {
      return with_parser([&](auto const& parser) { return path_remove(base, parser, m_path); });
    }

    template <json_path_addable JsonLike>
    std::expected<JsonLike, StarException> add(JsonLike const& base, JsonLike const& value) const {
      return with_parser([&](auto const& parser) { return path_add(base, parser, m_path, value); });
    }

    String const& path() const { return m_path; }

  protected:
    Path(Detail::PointerParser parser, String const& path) : m_parser(parser), m_path(path) {}
    Path(Detail::QueryParser parser, String const& path) : m_parser(parser), m_path(path) {}

  private:
    template <Detail::parser_visitor Visitor> decltype(auto) with_parser(Visitor&& visitor) const {
      return std::visit(std::forward<Visitor>(visitor), m_parser);
    }

    std::variant<Detail::PointerParser, Detail::QueryParser, PathParser> m_parser;
    String m_path;
  };

  class Pointer : public Path {
  public:
    Pointer(String const& path) : Path(Detail::PointerParser{}, path) {}
  };

  class QueryPath : public Path {
  public:
    QueryPath(String const& path) : Path(Detail::QueryParser{}, path) {}
  };
}

}

export module star.json_path;

export namespace Star::JsonPath {
using ::Star::JsonPath::json_array_operation;
using ::Star::JsonPath::json_object_operation;
using ::Star::JsonPath::json_parent_operation;
using ::Star::JsonPath::json_path_addable;
using ::Star::JsonPath::json_path_parser;
using ::Star::JsonPath::json_path_readable;
using ::Star::JsonPath::json_path_rebuildable;
using ::Star::JsonPath::json_path_removable;
using ::Star::JsonPath::json_path_settable;
using ::Star::JsonPath::json_root_operation;
using ::Star::JsonPath::json_update_operation;
using ::Star::JsonPath::parse_pointer;
using ::Star::JsonPath::parse_query_path;
using ::Star::JsonPath::ParsingException;
using ::Star::JsonPath::ParsingExceptionTag;
using ::Star::JsonPath::Path;
using ::Star::JsonPath::path_add;
using ::Star::JsonPath::path_apply;
using ::Star::JsonPath::path_find;
using ::Star::JsonPath::path_find_ref;
using ::Star::JsonPath::path_get;
using ::Star::JsonPath::path_get_ref;
using ::Star::JsonPath::path_remove;
using ::Star::JsonPath::path_set;
using ::Star::JsonPath::path_update;
using ::Star::JsonPath::PathParser;
using ::Star::JsonPath::Pointer;
using ::Star::JsonPath::QueryPath;
using ::Star::JsonPath::TraversalException;
using ::Star::JsonPath::TraversalExceptionTag;
using ::Star::JsonPath::TypeHint;
}
