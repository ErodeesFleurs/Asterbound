module;
import std;
import star.string;
import star.format;
import star.json;
import star.json_path;

export module star.json_patch;

export namespace Star {
struct JsonPatchExceptionTag {
  static constexpr std::string_view name() { return "JsonPatchException"; }
};
using JsonPatchException = StarError<JsonPatchExceptionTag, JsonException>;
struct JsonPatchTestFailTag {
  static constexpr std::string_view name() { return "JsonPatchTestFail"; }
};
using JsonPatchTestFail = StarError<JsonPatchTestFailTag, StarException>;

// Applies RFC6902 operations, preserving patch diagnostics and test-failure categories.
std::expected<Json, StarException> json_patch(Json const& base, JsonArray const& patch);
namespace JsonPatching {
std::expected<Json, StarException> apply_operation(Json const& base, Json const& op);
// Raw merge preserves traversal/parsing errors without the dispatch failure wrapper.
std::expected<Json, StarException> apply_merge(Json const& base, Json const& op);
}
}

namespace Star {
namespace {
StarException patch_failure(StarException error, std::string_view context) {
  if (error.is<JsonException>())
    return JsonPatchException(Star::format("{} {}", context, error.what()), false);
  return error;
}
}// namespace

std::expected<Json, StarException> json_patch(Json const& base, JsonArray const& patch) {
  Json result = base;
  for (auto const& operation : patch) {
    auto applied = JsonPatching::apply_operation(result, operation);
    if (!applied)
      return std::unexpected(patch_failure(std::move(applied.error()), "Could not apply patch to base."));
    result = std::move(*applied);
  }
  return result;
}

namespace JsonPatching {
namespace {
std::expected<std::reference_wrapper<Json const>, StarException> require_field(Json const& op, String const& key) {
  auto value = op.find(key);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (!*value)
    return std::unexpected(JsonException::format("No such key in Json::get(\"{}\")", key));
  return **value;
}

std::expected<std::shared_ptr<String const>, StarException> string_field(Json const& op, String const& key) {
  auto value = require_field(op, key);
  if (!value)
    return std::unexpected(std::move(value.error()));
  return value->get().string_ptr();
}

std::expected<std::optional<std::size_t>, StarException> find_match(Json const& searchable, Json const& value,
                                                                    String const& path) {
  if (!searchable.isType(Json::Type::Array))
    return std::unexpected(
      JsonPatchException(Star::format("Search operation failure, value at '{}' is not an array.", path), false));
  auto array = searchable.array_ptr();
  if (!array)
    return std::unexpected(std::move(array.error()));
  for (std::size_t index = 0; index < (*array)->size(); ++index)
    if (jsonPartialMatch((**array)[index], value))
      return index;
  return std::nullopt;
}

template <typename Mutation>
concept search_mutation = requires(Mutation& mutation, Json const& searchable, std::size_t index) {
  { std::invoke(mutation, searchable, index) } -> std::same_as<std::expected<Json, StarException>>;
};

template <search_mutation Mutation>
std::expected<Json, StarException> update_search(Json const& base, String const& path, Json const& search,
                                                 Mutation&& mutation) {
  auto updated =
    JsonPath::path_update(base, JsonPath::parse_pointer, path,
                          [&](Json const& searchable) -> std::expected<std::optional<Json>, StarException> {
                            auto index = find_match(searchable, search, path);
                            if (!index)
                              return std::unexpected(std::move(index.error()));
                            if (!*index)
                              return std::nullopt;
                            auto changed = std::invoke(mutation, searchable, **index);
                            if (!changed)
                              return std::unexpected(std::move(changed.error()));
                            return std::optional<Json>(std::move(*changed));
                          });
  if (!updated)
    return std::unexpected(std::move(updated.error()));
  if (!*updated)
    return base;
  return std::move(**updated);
}

std::expected<Json, StarException> apply_test(Json const& base, Json const& op) {
  auto path = string_field(op, "path");
  if (!path)
    return std::unexpected(std::move(path.error()));
  auto inverse = op.getBool("inverse", false);
  if (!inverse)
    return std::unexpected(std::move(inverse.error()));
  auto tested = [&]() -> std::expected<Json, StarException> {
    auto search = op.find("search");
    if (!search)
      return std::unexpected(std::move(search.error()));
    if (*search) {
      auto searchable = JsonPath::path_get_ref(base, JsonPath::parse_pointer, **path);
      if (!searchable)
        return std::unexpected(std::move(searchable.error()));
      auto const& value = (**search).get();
      auto found = find_match(searchable->get(), value, **path);
      if (!found)
        return std::unexpected(std::move(found.error()));
      if (found->has_value() == *inverse) {
        auto text = value.repr();
        if (!text)
          return std::unexpected(std::move(text.error()));
        if (*inverse)
          return std::unexpected(
            JsonPatchTestFail(Star::format("Test operation failure, expected {} to be missing.", *text), false));
        return std::unexpected(
          JsonPatchTestFail(Star::format("Test operation failure, could not find {}.", *text), false));
      }
      return base;
    }
    auto value = op.find("value");
    if (!value)
      return std::unexpected(std::move(value.error()));
    auto actual = JsonPath::path_get_ref(base, JsonPath::parse_pointer, **path);
    if (!actual)
      return std::unexpected(std::move(actual.error()));
    if (!*value || (**value).get().isNull()) {
      if (*inverse)
        return std::unexpected(
          JsonPatchTestFail(Star::format("Test operation failure, expected {} to be missing.", **path), false));
      return base;
    }
    if (((**value).get() == actual->get()) != *inverse)
      return base;
    auto expected_text = (**value).get().repr();
    if (!expected_text)
      return std::unexpected(std::move(expected_text.error()));
    auto actual_text = actual->get().repr();
    if (!actual_text)
      return std::unexpected(std::move(actual_text.error()));
    return std::unexpected(JsonPatchTestFail(
      Star::format("Test operation failure, expected Just ({}) found {}.", *expected_text, *actual_text), false));
  }();
  if (!tested && tested.error().is<JsonPath::TraversalException>()) {
    if (*inverse)
      return base;
    return std::unexpected(JsonPatchTestFail(Star::format("Test operation failure: {}", tested.error().what()), false));
  }
  return tested;
}

std::expected<Json, StarException> apply_remove(Json const& base, Json const& op) {
  auto path = string_field(op, "path");
  if (!path)
    return std::unexpected(std::move(path.error()));
  auto search = op.find("search");
  if (!search)
    return std::unexpected(std::move(search.error()));
  if (!*search)
    return JsonPath::path_remove(base, JsonPath::parse_pointer, **path);
  return update_search(base, **path, (**search).get(),
                       [](Json const& searchable, std::size_t index) { return searchable.erase(index); });
}

std::expected<Json, StarException> apply_add(Json const& base, Json const& op) {
  auto path = string_field(op, "path");
  if (!path)
    return std::unexpected(std::move(path.error()));
  auto value = require_field(op, "value");
  if (!value)
    return std::unexpected(std::move(value.error()));
  auto search = op.find("search");
  if (!search)
    return std::unexpected(std::move(search.error()));
  if (!*search)
    return JsonPath::path_add(base, JsonPath::parse_pointer, **path, value->get());
  return update_search(base, **path, (**search).get(), [&](Json const& searchable, std::size_t index) {
    return searchable.insert(index, value->get());
  });
}

std::expected<Json, StarException> apply_replace(Json const& base, Json const& op) {
  auto path = string_field(op, "path");
  if (!path)
    return std::unexpected(std::move(path.error()));
  auto value = require_field(op, "value");
  if (!value)
    return std::unexpected(std::move(value.error()));
  auto search = op.find("search");
  if (!search)
    return std::unexpected(std::move(search.error()));
  if (*search)
    return update_search(base, **path, (**search).get(), [&](Json const& searchable, std::size_t index) {
      return searchable.set(index, value->get());
    });
  return JsonPath::path_apply<Json>(
    base, JsonPath::parse_pointer, **path,
    [&](Json const&) -> std::expected<Json, StarException> { return value->get(); },
    [&](Json const& object, String const& key) -> std::expected<Json, StarException> {
      auto present = object.find(key);
      if (!present)
        return std::unexpected(std::move(present.error()));
      if (!*present)
        return std::unexpected(JsonPath::TraversalException::format("Could not find \"{}\" to remove", key));
      return object.set(key, value->get());
    },
    [&](Json const& array, std::optional<std::size_t> index) -> std::expected<Json, StarException> {
      if (!index)
        return std::unexpected(JsonPath::TraversalException("Could not remove element after end of array"));
      return array.set(*index, value->get());
    });
}

std::expected<Json, StarException> apply_move(Json const& base, Json const& op) {
  auto path = string_field(op, "path");
  if (!path)
    return std::unexpected(std::move(path.error()));
  auto from = string_field(op, "from");
  if (!from)
    return std::unexpected(std::move(from.error()));
  auto search = op.find("search");
  if (!search)
    return std::unexpected(std::move(search.error()));
  if (*search) {
    auto searchable = JsonPath::path_get_ref(base, JsonPath::parse_pointer, **from);
    if (!searchable)
      return std::unexpected(std::move(searchable.error()));
    auto index = find_match(searchable->get(), (**search).get(), **from);
    if (!index)
      return std::unexpected(std::move(index.error()));
    if (!*index)
      return base;
    auto value = searchable->get().find(**index);
    if (!value)
      return std::unexpected(std::move(value.error()));
    // Search moves add first, then add the rewritten original source array.
    // Overlapping paths deliberately retain these non-RFC insertion semantics.
    auto added = JsonPath::path_add(base, JsonPath::parse_pointer, **path, (**value).get());
    if (!added)
      return std::unexpected(std::move(added.error()));
    auto changed = searchable->get().erase(**index);
    if (!changed)
      return std::unexpected(std::move(changed.error()));
    return JsonPath::path_add(*added, JsonPath::parse_pointer, **from, *changed);
  }
  auto value = JsonPath::path_get_ref(base, JsonPath::parse_pointer, **from);
  if (!value)
    return std::unexpected(std::move(value.error()));
  auto removed = JsonPath::path_remove(base, JsonPath::parse_pointer, **from);
  if (!removed)
    return std::unexpected(std::move(removed.error()));
  return JsonPath::path_add(*removed, JsonPath::parse_pointer, **path, value->get());
}

std::expected<Json, StarException> apply_copy(Json const& base, Json const& op) {
  auto path = string_field(op, "path");
  if (!path)
    return std::unexpected(std::move(path.error()));
  auto from = string_field(op, "from");
  if (!from)
    return std::unexpected(std::move(from.error()));
  auto search = op.find("search");
  if (!search)
    return std::unexpected(std::move(search.error()));
  if (*search) {
    auto searchable = JsonPath::path_get_ref(base, JsonPath::parse_pointer, **from);
    if (!searchable)
      return std::unexpected(std::move(searchable.error()));
    auto index = find_match(searchable->get(), (**search).get(), **from);
    if (!index)
      return std::unexpected(std::move(index.error()));
    if (!*index)
      return base;
    auto value = searchable->get().find(**index);
    if (!value)
      return std::unexpected(std::move(value.error()));
    return JsonPath::path_add(base, JsonPath::parse_pointer, **path, (**value).get());
  }
  auto value = JsonPath::path_get_ref(base, JsonPath::parse_pointer, **from);
  if (!value)
    return std::unexpected(std::move(value.error()));
  return JsonPath::path_add(base, JsonPath::parse_pointer, **path, value->get());
}
}// namespace

std::expected<Json, StarException> apply_merge(Json const& base, Json const& op) {
  auto path = string_field(op, "path");
  if (!path)
    return std::unexpected(std::move(path.error()));
  auto search = op.find("search");
  if (!search)
    return std::unexpected(std::move(search.error()));
  if (*search)
    return update_search(base, **path, (**search).get(),
                         [&](Json const& searchable, std::size_t index) -> std::expected<Json, StarException> {
                           auto old_value = searchable.find(index);
                           if (!old_value)
                             return std::unexpected(std::move(old_value.error()));
                           auto value = require_field(op, "value");
                           if (!value)
                             return std::unexpected(std::move(value.error()));
                           return searchable.set(index, jsonMerge((**old_value).get(), value->get()));
                         });
  auto merge = [&](Json const& target) -> std::expected<Json, StarException> {
    auto value = require_field(op, "value");
    if (!value)
      return std::unexpected(std::move(value.error()));
    return jsonMerge(target, value->get());
  };
  return JsonPath::path_apply<Json>(
    base, JsonPath::parse_pointer, **path, merge,
    [&](Json const& object, String const& key) -> std::expected<Json, StarException> {
      auto target = object.find(key);
      if (!target)
        return std::unexpected(std::move(target.error()));
      if (!*target)
        return std::unexpected(JsonPath::TraversalException::format("No such key '{}' in merge", key));
      auto changed = merge((**target).get());
      if (!changed)
        return std::unexpected(std::move(changed.error()));
      return object.set(key, std::move(*changed));
    },
    [&](Json const& array, std::optional<std::size_t> index) -> std::expected<Json, StarException> {
      if (!index)
        return std::unexpected(JsonPath::TraversalException("Index out of range in merge"));
      auto target = array.find(*index);
      if (!target)
        return std::unexpected(std::move(target.error()));
      auto changed = merge((**target).get());
      if (!changed)
        return std::unexpected(std::move(changed.error()));
      return array.set(*index, std::move(*changed));
    });
}

namespace {
using Operation = std::reference_wrapper<std::expected<Json, StarException>(Json const&, Json const&)>;
using OperationEntry = std::pair<std::string_view, Operation>;
static constexpr std::array<OperationEntry, 7> operations{{
  {"test", std::ref(apply_test)},
  {"remove", std::ref(apply_remove)},
  {"add", std::ref(apply_add)},
  {"replace", std::ref(apply_replace)},
  {"move", std::ref(apply_move)},
  {"copy", std::ref(apply_copy)},
  {"merge", std::ref(apply_merge)},
}};
}// namespace

std::expected<Json, StarException> apply_operation(Json const& base, Json const& op) {
  auto operation = string_field(op, "op");
  if (!operation)
    return std::unexpected(patch_failure(std::move(operation.error()), "Could not apply operation to base."));
  auto function = std::ranges::find(operations, (*operation)->utf8(), &OperationEntry::first);
  if (function == operations.end())
    return std::unexpected(JsonPatchException(Star::format("Invalid operation: {}", **operation), false));
  return function->second(base, op).transform_error(
    [](StarException error) { return patch_failure(std::move(error), "Could not apply operation to base."); });
}
}// namespace JsonPatching
}
