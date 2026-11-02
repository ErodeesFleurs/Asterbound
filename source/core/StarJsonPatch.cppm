module;
import std;
import star.string;
import star.map;
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
std::expected<Json, StarException> jsonPatch(Json const& base, JsonArray const& patch);
namespace JsonPatching {
  std::expected<Json, StarException> applyOperation(Json const& base, Json const& op, std::optional<Json> const& external = {});
  std::expected<Json, StarException> applyTestOperation(Json const& base, Json const& op);
  std::expected<Json, StarException> applyRemoveOperation(Json const& base, Json const& op);
  std::expected<Json, StarException> applyAddOperation(Json const& base, Json const& op);
  std::expected<Json, StarException> applyReplaceOperation(Json const& base, Json const& op);
  std::expected<Json, StarException> applyMoveOperation(Json const& base, Json const& op);
  std::expected<Json, StarException> applyCopyOperation(Json const& base, Json const& op);
  std::expected<Json, StarException> applyMergeOperation(Json const& base, Json const& op);
}
}

namespace Star {
namespace {
StarException patchFailure(StarException error, std::string_view context) {
  if (error.is<JsonException>())
    return JsonPatchException(strf("{} {}", context, error.what()), false);
  return error;
}
}// namespace

std::expected<Json, StarException> jsonPatch(Json const& base, JsonArray const& patch) {
  Json result = base;
  for (auto const& operation : patch) {
    auto applied = JsonPatching::applyOperation(result, operation);
    if (!applied)
      return std::unexpected(patchFailure(std::move(applied.error()), "Could not apply patch to base."));
    result = std::move(*applied);
  }
  return result;
}

// Zero means absent; otherwise the result is the matching index plus one.
std::expected<std::size_t, StarException> findJsonMatch(Json const& searchable, Json const& value, JsonPath::Pointer const& pointer) {
  if (!searchable.isType(Json::Type::Array))
    return std::unexpected(JsonPatchException(strf("Search operation failure, value at '{}' is not an array.", pointer.path()), false));
  auto array = searchable.arrayPtr();
  if (!array)
    return std::unexpected(std::move(array.error()));
  for (std::size_t i = 0; i < (*array)->size(); ++i)
    if (jsonPartialMatch((**array)[i], value))
      return i + 1;
  return 0;
}

namespace JsonPatching {
  using Operation = std::reference_wrapper<std::expected<Json, StarException>(Json const&, Json const&)>;
  using OperationEntry = std::pair<std::string_view, Operation>;
  static constexpr std::array<OperationEntry, 7> functionMap{{
      {"test", std::ref(applyTestOperation)}, {"remove", std::ref(applyRemoveOperation)},
      {"add", std::ref(applyAddOperation)}, {"replace", std::ref(applyReplaceOperation)},
      {"move", std::ref(applyMoveOperation)}, {"copy", std::ref(applyCopyOperation)},
      {"merge", std::ref(applyMergeOperation)},
  }};

  std::expected<Json, StarException> applyOperation(Json const& base, Json const& op, std::optional<Json> const&) {
    auto operation = op.getString("op");
    if (!operation)
      return std::unexpected(patchFailure(std::move(operation.error()), "Could not apply operation to base."));
    auto function = std::ranges::find(functionMap, operation->utf8(), &OperationEntry::first);
    if (function == functionMap.end())
      return std::unexpected(JsonPatchException(strf("Invalid operation: {}", *operation), false));
    return function->second(base, op).transform_error(
      [](StarException error) { return patchFailure(std::move(error), "Could not apply operation to base."); });
  }

  std::expected<Json, StarException> applyTestOperation(Json const& base, Json const& op) {
    auto path = op.getString("path");
    if (!path)
      return std::unexpected(std::move(path.error()));
    auto inverse = op.getBool("inverse", false);
    if (!inverse)
      return std::unexpected(std::move(inverse.error()));
    JsonPath::Pointer pointer(*path);
    auto tested = [&]() -> std::expected<Json, StarException> {
      auto search = op.contains("search");
      if (!search)
        return std::unexpected(std::move(search.error()));
      if (*search) {
        auto searchable = JsonPath::pathGetRef(base, JsonPath::parsePointer, *path);
        if (!searchable)
          return std::unexpected(std::move(searchable.error()));
        auto value = op.get("search");
        if (!value)
          return std::unexpected(std::move(value.error()));
        auto found = findJsonMatch(searchable->get(), *value, pointer);
        if (!found)
          return std::unexpected(std::move(found.error()));
        if ((*found != 0) == *inverse) {
          auto text = value->repr();
          if (!text)
            return std::unexpected(std::move(text.error()));
          if (*inverse)
            return std::unexpected(JsonPatchTestFail(strf("Test operation failure, expected {} to be missing.", *text), false));
          return std::unexpected(JsonPatchTestFail(strf("Test operation failure, could not find {}.", *text), false));
        }
        return base;
      }
      auto value = op.opt("value");
      if (!value)
        return std::unexpected(std::move(value.error()));
      auto actual = JsonPath::pathGetRef(base, JsonPath::parsePointer, *path);
      if (!actual)
        return std::unexpected(std::move(actual.error()));
      if (!*value) {
        if (*inverse)
          return std::unexpected(JsonPatchTestFail(strf("Test operation failure, expected {} to be missing.", *path), false));
        return base;
      }
      if ((**value == actual->get()) != *inverse)
        return base;
      auto expectedText = (**value).repr();
      if (!expectedText)
        return std::unexpected(std::move(expectedText.error()));
      auto actualText = actual->get().repr();
      if (!actualText)
        return std::unexpected(std::move(actualText.error()));
      return std::unexpected(JsonPatchTestFail(strf("Test operation failure, expected Just ({}) found {}.", *expectedText, *actualText), false));
    }();
    if (!tested && tested.error().is<JsonPath::TraversalException>()) {
      if (*inverse)
        return base;
      return std::unexpected(JsonPatchTestFail(strf("Test operation failure: {}", tested.error().what()), false));
    }
    return tested;
  }

  std::expected<Json, StarException> applyRemoveOperation(Json const& base, Json const& op) {
    auto path = op.getString("path");
    if (!path)
      return std::unexpected(std::move(path.error()));
    JsonPath::Pointer pointer(*path);
    auto search = op.contains("search");
    if (!search)
      return std::unexpected(std::move(search.error()));
    if (!*search)
      return pointer.remove(base);
    auto searchable = JsonPath::pathGetRef(base, JsonPath::parsePointer, *path);
    if (!searchable)
      return std::unexpected(std::move(searchable.error()));
    auto value = op.get("search");
    if (!value)
      return std::unexpected(std::move(value.error()));
    auto index = findJsonMatch(searchable->get(), *value, pointer);
    if (!index)
      return std::unexpected(std::move(index.error()));
    if (!*index)
      return base;
    auto changed = searchable->get().eraseIndex(*index - 1);
    if (!changed)
      return std::unexpected(std::move(changed.error()));
    return pointer.set(base, *changed);
  }

  std::expected<Json, StarException> applyAddOperation(Json const& base, Json const& op) {
    auto path = op.getString("path");
    if (!path)
      return std::unexpected(std::move(path.error()));
    auto value = op.get("value");
    if (!value)
      return std::unexpected(std::move(value.error()));
    JsonPath::Pointer pointer(*path);
    auto search = op.contains("search");
    if (!search)
      return std::unexpected(std::move(search.error()));
    if (!*search)
      return pointer.add(base, *value);
    auto searchable = JsonPath::pathGetRef(base, JsonPath::parsePointer, *path);
    if (!searchable)
      return std::unexpected(std::move(searchable.error()));
    auto searchValue = op.get("search");
    if (!searchValue)
      return std::unexpected(std::move(searchValue.error()));
    auto index = findJsonMatch(searchable->get(), *searchValue, pointer);
    if (!index)
      return std::unexpected(std::move(index.error()));
    if (!*index)
      return base;
    auto changed = searchable->get().insert(*index - 1, *value);
    if (!changed)
      return std::unexpected(std::move(changed.error()));
    return pointer.set(base, *changed);
  }

  std::expected<Json, StarException> applyReplaceOperation(Json const& base, Json const& op) {
    auto path = op.getString("path");
    if (!path)
      return std::unexpected(std::move(path.error()));
    auto value = op.get("value");
    if (!value)
      return std::unexpected(std::move(value.error()));
    JsonPath::Pointer pointer(*path);
    auto search = op.contains("search");
    if (!search)
      return std::unexpected(std::move(search.error()));
    if (*search) {
      auto searchable = JsonPath::pathGetRef(base, JsonPath::parsePointer, *path);
      if (!searchable)
        return std::unexpected(std::move(searchable.error()));
      auto searchValue = op.get("search");
      if (!searchValue)
        return std::unexpected(std::move(searchValue.error()));
      auto index = findJsonMatch(searchable->get(), *searchValue, pointer);
      if (!index)
        return std::unexpected(std::move(index.error()));
      if (!*index)
        return base;
      auto changed = searchable->get().set(*index - 1, *value);
      if (!changed)
        return std::unexpected(std::move(changed.error()));
      return pointer.set(base, *changed);
    }
    return pointer.apply<Json>(
      base, [&](Json const&) -> std::expected<Json, StarException> { return *value; },
      [&](Json const& object, String const& key) -> std::expected<Json, StarException> {
        auto present = object.find(key);
        if (!present)
          return std::unexpected(std::move(present.error()));
        if (!*present)
          return std::unexpected(JsonPath::TraversalException::format("Could not find \"{}\" to remove", key));
        return object.set(key, *value);
      },
      [&](Json const& array, std::optional<std::size_t> index) -> std::expected<Json, StarException> {
        if (!index)
          return std::unexpected(JsonPath::TraversalException("Could not remove element after end of array"));
        return array.set(*index, *value);
      });
  }

  std::expected<Json, StarException> applyMoveOperation(Json const& base, Json const& op) {
    auto path = op.getString("path");
    if (!path)
      return std::unexpected(std::move(path.error()));
    auto from = op.getString("from");
    if (!from)
      return std::unexpected(std::move(from.error()));
    JsonPath::Pointer toPointer(*path), fromPointer(*from);
    auto search = op.contains("search");
    if (!search)
      return std::unexpected(std::move(search.error()));
    if (*search) {
      auto searchable = JsonPath::pathGetRef(base, JsonPath::parsePointer, *from);
      if (!searchable)
        return std::unexpected(std::move(searchable.error()));
      auto searchValue = op.get("search");
      if (!searchValue)
        return std::unexpected(std::move(searchValue.error()));
      auto index = findJsonMatch(searchable->get(), *searchValue, fromPointer);
      if (!index)
        return std::unexpected(std::move(index.error()));
      if (!*index)
        return base;
      auto value = searchable->get().find(*index - 1);
      if (!value)
        return std::unexpected(std::move(value.error()));
      auto added = toPointer.add(base, (**value).get());
      if (!added)
        return std::unexpected(std::move(added.error()));
      auto changed = searchable->get().eraseIndex(*index - 1);
      if (!changed)
        return std::unexpected(std::move(changed.error()));
      return fromPointer.add(*added, *changed);
    }
    auto value = JsonPath::pathGetRef(base, JsonPath::parsePointer, *from);
    if (!value)
      return std::unexpected(std::move(value.error()));
    auto removed = fromPointer.remove(base);
    if (!removed)
      return std::unexpected(std::move(removed.error()));
    return toPointer.add(*removed, value->get());
  }

  std::expected<Json, StarException> applyCopyOperation(Json const& base, Json const& op) {
    auto path = op.getString("path");
    if (!path)
      return std::unexpected(std::move(path.error()));
    auto from = op.getString("from");
    if (!from)
      return std::unexpected(std::move(from.error()));
    JsonPath::Pointer toPointer(*path), fromPointer(*from);
    auto search = op.contains("search");
    if (!search)
      return std::unexpected(std::move(search.error()));
    if (*search) {
      auto searchable = JsonPath::pathGetRef(base, JsonPath::parsePointer, *from);
      if (!searchable)
        return std::unexpected(std::move(searchable.error()));
      auto searchValue = op.get("search");
      if (!searchValue)
        return std::unexpected(std::move(searchValue.error()));
      auto index = findJsonMatch(searchable->get(), *searchValue, fromPointer);
      if (!index)
        return std::unexpected(std::move(index.error()));
      if (!*index)
        return base;
      auto value = searchable->get().find(*index - 1);
      if (!value)
        return std::unexpected(std::move(value.error()));
      return toPointer.add(base, (**value).get());
    }
    auto value = JsonPath::pathGetRef(base, JsonPath::parsePointer, *from);
    if (!value)
      return std::unexpected(std::move(value.error()));
    return toPointer.add(base, value->get());
  }

  std::expected<Json, StarException> applyMergeOperation(Json const& base, Json const& op) {
    auto path = op.getString("path");
    if (!path)
      return std::unexpected(std::move(path.error()));
    JsonPath::Pointer pointer(*path);
    auto search = op.contains("search");
    if (!search)
      return std::unexpected(std::move(search.error()));
    if (*search) {
      auto searchable = JsonPath::pathGetRef(base, JsonPath::parsePointer, *path);
      if (!searchable)
        return std::unexpected(std::move(searchable.error()));
      auto searchValue = op.get("search");
      if (!searchValue)
        return std::unexpected(std::move(searchValue.error()));
      auto index = findJsonMatch(searchable->get(), *searchValue, pointer);
      if (!index)
        return std::unexpected(std::move(index.error()));
      if (!*index)
        return base;
      auto oldValue = searchable->get().find(*index - 1);
      if (!oldValue)
        return std::unexpected(std::move(oldValue.error()));
      auto value = op.get("value");
      if (!value)
        return std::unexpected(std::move(value.error()));
      auto changed = searchable->get().set(*index - 1, jsonMerge((**oldValue).get(), *value));
      if (!changed)
        return std::unexpected(std::move(changed.error()));
      return pointer.set(base, *changed);
    }
    auto merge = [&](Json const& target) -> std::expected<Json, StarException> {
      auto value = op.get("value");
      if (!value)
        return std::unexpected(std::move(value.error()));
      return jsonMerge(target, *value);
    };
    return pointer.apply<Json>(
      base, merge,
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
}
}
