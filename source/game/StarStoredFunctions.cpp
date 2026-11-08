import star.json;
import star.interpolation;
import star.parametric_function;
import star.multi_array;
import star.multi_array_interpolator;
import star.multi_table;
import star.ordered_map;
import star.bi_map;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.string;
import star.format;
import star.version;

import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.stored_functions;

namespace Star {

double const StoredFunction::DefaultSearchTolerance = 0.001;

StoredFunction::StoredFunction(ParametricFunction<double, double> data, Monotonicity monotonicity)
  : m_monotonicity(monotonicity), m_function(std::move(data)) {}

std::expected<std::shared_ptr<StoredFunction>, StarException> StoredFunction::create(ParametricFunction<double, double> data) {
  if (data.empty())
    return std::unexpected(StoredFunctionException("StoredFunction constructor called on function with no data points"));
  Monotonicity monotonicity = Monotonicity::Flat;
  for (std::size_t i = 0; i + 1 < data.size(); ++i) {
    auto current = data.value(i);
    if (!current)
      return std::unexpected(std::move(current.error()));
    auto next = data.value(i + 1);
    if (!next)
      return std::unexpected(std::move(next.error()));
    if (current->get() < next->get()) {
      if (monotonicity == Monotonicity::Flat)
        monotonicity = Monotonicity::Increasing;
      else if (monotonicity == Monotonicity::Decreasing)
        monotonicity = Monotonicity::None;
    } else if (next->get() < current->get()) {
      if (monotonicity == Monotonicity::Flat)
        monotonicity = Monotonicity::Decreasing;
      else if (monotonicity == Monotonicity::Increasing)
        monotonicity = Monotonicity::None;
    }
    if (monotonicity == Monotonicity::None)
      break;
  }
  return std::shared_ptr<StoredFunction>(new StoredFunction(std::move(data), monotonicity));
}

Monotonicity StoredFunction::monotonicity() const {
  return m_monotonicity;
}

std::expected<double, StarException> StoredFunction::evaluate(double value) const {
  return m_function.interpolate(value);
}

std::expected<StoredFunction::SearchResult, StarException> StoredFunction::search(double targetValue, double valueTolerance) const {
  auto firstIndex = m_function.index(0);
  if (!firstIndex)
    return std::unexpected(std::move(firstIndex.error()));
  auto first_value = m_function.value(0);
  if (!first_value)
    return std::unexpected(std::move(first_value.error()));
  auto lastIndex = m_function.index(m_function.size() - 1);
  if (!lastIndex)
    return std::unexpected(std::move(lastIndex.error()));
  auto lastValue = m_function.value(m_function.size() - 1);
  if (!lastValue)
    return std::unexpected(std::move(lastValue.error()));
  double minIndex = firstIndex->get();
  double minValue = first_value->get();
  double maxIndex = lastIndex->get();
  double maxValue = lastValue->get();
  if (maxValue < minValue) {
    std::swap(minIndex, maxIndex);
    std::swap(minValue, maxValue);
  }
  double index;
  double value;
  if (targetValue < minValue) {
    index = minIndex;
    value = minValue;
  } else if (targetValue > maxValue) {
    index = maxIndex;
    value = maxValue;
  } else {
    index = (minIndex + maxIndex) / 2;
    auto initial = m_function.interpolate(index);
    if (!initial)
      return std::unexpected(std::move(initial.error()));
    value = *initial;
    int searchDepth = 0;
    while (std::fabs(targetValue - value) > valueTolerance && searchDepth < 64) {
      ++searchDepth;
      if (value < targetValue) {
        minIndex = index;
        minValue = value;
      } else if (value > targetValue) {
        maxIndex = index;
        maxValue = value;
      }
      double newIndex = (minIndex + maxIndex) / 2;
      auto newValue = m_function.interpolate(newIndex);
      if (!newValue)
        return std::unexpected(std::move(newValue.error()));
      if (*newValue > maxValue || *newValue < minValue)
        return std::unexpected(StarException("StoredFunction is not monotonic."));
      index = newIndex;
      value = *newValue;
    }
  }
  return SearchResult{targetValue, valueTolerance, std::fabs(targetValue - value) <= valueTolerance, index, value};
}

StoredFunction2::StoredFunction2(MultiTable2D table) : table(std::move(table)) {}

std::expected<double, StarException> StoredFunction2::evaluate(double x, double y) const {
  return table.interpolate({x, y});
}

StoredConfigFunction::StoredConfigFunction(ParametricTable<int, Json> data) {
  m_data = std::move(data);
}

std::expected<Json, StarException> StoredConfigFunction::get(double value) const {
  auto result = m_data.get(value);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return result->get();
}

std::expected<std::shared_ptr<FunctionDatabase>, StarException> FunctionDatabase::create(std::shared_ptr<Assets const> const& assets) {
  auto result = std::shared_ptr<FunctionDatabase>(new FunctionDatabase());
  auto functions = assets->scanExtension("functions");
  if (!functions)
    return std::unexpected(std::move(functions.error()));
  auto functions2 = assets->scanExtension("2functions");
  if (!functions2)
    return std::unexpected(std::move(functions2.error()));
  auto configFunctions = assets->scanExtension("configfunctions");
  if (!configFunctions)
    return std::unexpected(std::move(configFunctions.error()));
  auto queuedFunctions = assets->queueJsons(functions->get());
  if (!queuedFunctions)
    return std::unexpected(std::move(queuedFunctions.error()));
  auto queuedFunctions2 = assets->queueJsons(functions2->get());
  if (!queuedFunctions2)
    return std::unexpected(std::move(queuedFunctions2.error()));
  auto queuedConfigFunctions = assets->queueJsons(configFunctions->get());
  if (!queuedConfigFunctions)
    return std::unexpected(std::move(queuedConfigFunctions.error()));
  for (auto const& assetFile : functions->get()) {
    auto const& file = assetFile.string();
    auto config = assets->json(file);
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto entries = config->iterateObject();
    if (!entries)
      return std::unexpected(std::move(entries.error()));
    for (auto const& entry : *entries) {
      if (result->m_functions.contains(entry.first))
        return std::unexpected(StarException(strf("Named Function '{}' defined twice, second time from {}", entry.first, file)));
      auto data = parametricFunctionFromConfig(entry.second);
      if (!data)
        return std::unexpected(std::move(data.error()));
      auto function = StoredFunction::create(std::move(*data));
      if (!function)
        return std::unexpected(std::move(function.error()));
      result->m_functions[entry.first] = std::move(*function);
    }
  }
  for (auto const& assetFile : functions2->get()) {
    auto const& file = assetFile.string();
    auto config = assets->json(file);
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto entries = config->iterateObject();
    if (!entries)
      return std::unexpected(std::move(entries.error()));
    for (auto const& entry : *entries) {
      if (result->m_functions2.contains(entry.first))
        return std::unexpected(StarException(strf("Named 2-ary Function '{}' defined twice, second time from {}", entry.first, file)));
      auto data = multiTable2DFromConfig(entry.second);
      if (!data)
        return std::unexpected(std::move(data.error()));
      result->m_functions2[entry.first] = std::make_shared<StoredFunction2>(std::move(*data));
    }
  }
  for (auto const& assetFile : configFunctions->get()) {
    auto const& file = assetFile.string();
    auto config = assets->json(file);
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto entries = config->iterateObject();
    if (!entries)
      return std::unexpected(std::move(entries.error()));
    for (auto const& entry : *entries) {
      if (result->m_configFunctions.contains(entry.first))
        return std::unexpected(StarException(strf("Named config function '{}' defined twice, second time from {}", entry.first, file)));
      auto data = parametricTableFromConfig(entry.second);
      if (!data)
        return std::unexpected(std::move(data.error()));
      result->m_configFunctions[entry.first] = std::make_shared<StoredConfigFunction>(std::move(*data));
    }
  }
  return result;
}

StringList FunctionDatabase::namedFunctions() const {
  return m_functions.keys();
}

StringList FunctionDatabase::namedFunctions2() const {
  return m_functions2.keys();
}

StringList FunctionDatabase::namedConfigFunctions() const {
  return m_configFunctions.keys();
}

std::expected<std::shared_ptr<StoredFunction>, StarException> FunctionDatabase::function(Json const& configOrName) const {
  if (configOrName.isType(Json::Type::String)) {
    auto name = configOrName.toString();
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto value = m_functions.get(*name);
    if (!value)
      return std::unexpected(std::move(value.error()));
    return value->get();
  }
  auto data = parametricFunctionFromConfig(configOrName);
  if (!data)
    return std::unexpected(std::move(data.error()));
  return StoredFunction::create(std::move(*data));
}

std::expected<std::shared_ptr<StoredFunction2>, StarException> FunctionDatabase::function2(Json const& configOrName) const {
  if (configOrName.isType(Json::Type::String)) {
    auto name = configOrName.toString();
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto value = m_functions2.get(*name);
    if (!value)
      return std::unexpected(std::move(value.error()));
    return value->get();
  }
  auto data = multiTable2DFromConfig(configOrName);
  if (!data)
    return std::unexpected(std::move(data.error()));
  return std::make_shared<StoredFunction2>(std::move(*data));
}

std::expected<std::shared_ptr<StoredConfigFunction>, StarException> FunctionDatabase::configFunction(Json const& configOrName) const {
  if (configOrName.isType(Json::Type::String)) {
    auto name = configOrName.toString();
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto value = m_configFunctions.get(*name);
    if (!value)
      return std::unexpected(std::move(value.error()));
    return value->get();
  }
  auto data = parametricTableFromConfig(configOrName);
  if (!data)
    return std::unexpected(std::move(data.error()));
  return std::make_shared<StoredConfigFunction>(std::move(*data));
}

namespace {

[[nodiscard]] std::expected<InterpolationMode, StarException> storedInterpolationMode(String const& name) {
  auto halfStep = name.equalsIgnoreCase("HalfStep");
  if (!halfStep)
    return std::unexpected(std::move(halfStep.error()));
  if (*halfStep)
    return InterpolationMode::HalfStep;
  auto linear = name.equalsIgnoreCase("Linear");
  if (!linear)
    return std::unexpected(std::move(linear.error()));
  if (*linear)
    return InterpolationMode::Linear;
  auto cubic = name.equalsIgnoreCase("Cubic");
  if (!cubic)
    return std::unexpected(std::move(cubic.error()));
  if (*cubic)
    return InterpolationMode::Cubic;
  return std::unexpected(StoredFunctionException(strf("Unrecognized InterpolationMode '{}'", name)));
}

[[nodiscard]] std::expected<BoundMode, StarException> storedBoundMode(String const& name) {
  auto clamp = name.equalsIgnoreCase("Clamp");
  if (!clamp)
    return std::unexpected(std::move(clamp.error()));
  if (*clamp)
    return BoundMode::Clamp;
  auto extrapolate = name.equalsIgnoreCase("Extrapolate");
  if (!extrapolate)
    return std::unexpected(std::move(extrapolate.error()));
  if (*extrapolate)
    return BoundMode::Extrapolate;
  auto wrap = name.equalsIgnoreCase("Wrap");
  if (!wrap)
    return std::unexpected(std::move(wrap.error()));
  if (*wrap)
    return BoundMode::Wrap;
  return std::unexpected(StoredFunctionException(strf("Unrecognized BoundMode '{}'", name)));
}

}

std::expected<ParametricFunction<double, double>, StarException> FunctionDatabase::parametricFunctionFromConfig(Json descriptor) {
  auto parsed = [&]() -> std::expected<ParametricFunction<double, double>, StarException> {
    auto interpolationName = descriptor.getString(0);
    if (!interpolationName)
      return std::unexpected(std::move(interpolationName.error()));
    auto boundName = descriptor.getString(1);
    if (!boundName)
      return std::unexpected(std::move(boundName.error()));
    auto size = descriptor.size();
    if (!size)
      return std::unexpected(std::move(size.error()));
    List<std::pair<double, double>> points;
    for (std::size_t i = 2; i < *size; ++i) {
      auto pair = descriptor.get(i).and_then([](Json const& value) { return value.arrayPtr(); });
      if (!pair)
        return std::unexpected(std::move(pair.error()));
      if ((*pair)->size() != 2)
        return std::unexpected(StoredFunctionException("Each point must be a list of size 2"));
      auto index = (**pair)[0].toDouble();
      if (!index)
        return std::unexpected(std::move(index.error()));
      auto value = (**pair)[1].toDouble();
      if (!value)
        return std::unexpected(std::move(value.error()));
      points.push_back({*index, *value});
    }
    auto interpolation = storedInterpolationMode(*interpolationName);
    if (!interpolation)
      return std::unexpected(std::move(interpolation.error()));
    auto bound = storedBoundMode(*boundName);
    if (!bound)
      return std::unexpected(std::move(bound.error()));
    return ParametricFunction<double, double>::create(std::move(points), *interpolation, *bound);
  }();
  if (!parsed)
    return std::unexpected(StoredFunctionException("Error parsing StoredFunction descriptor", parsed.error()));
  return parsed;
}

std::expected<ParametricTable<int, Json>, StarException> FunctionDatabase::parametricTableFromConfig(Json descriptor) {
  auto parsed = [&]() -> std::expected<ParametricTable<int, Json>, StarException> {
    auto entries = descriptor.iterateArray();
    if (!entries)
      return std::unexpected(std::move(entries.error()));
    List<std::pair<int, Json>> points;
    for (auto const& entry : *entries) {
      auto pair = entry.arrayPtr();
      if (!pair)
        return std::unexpected(std::move(pair.error()));
      if ((*pair)->size() != 2)
        return std::unexpected(StoredFunctionException("Each point must be a list of size 2"));
      auto index = (**pair)[0].toInt();
      if (!index)
        return std::unexpected(std::move(index.error()));
      points.push_back({*index, (**pair)[1]});
    }
    return ParametricTable<int, Json>::create(std::move(points));
  }();
  if (!parsed)
    return std::unexpected(StoredFunctionException("Error parsing StoredConfigFunction descriptor", parsed.error()));
  return parsed;
}

std::expected<MultiTable2D, StarException> FunctionDatabase::multiTable2DFromConfig(Json descriptor) {
  auto parsed = [&]() -> std::expected<MultiTable2D, StarException> {
    auto interpolationName = descriptor.getString(0);
    if (!interpolationName)
      return std::unexpected(std::move(interpolationName.error()));
    auto boundName = descriptor.getString(1);
    if (!boundName)
      return std::unexpected(std::move(boundName.error()));
    auto grid = descriptor.get(2).and_then([](Json const& value) { return value.arrayPtr(); });
    if (!grid)
      return std::unexpected(std::move(grid.error()));
    if ((*grid)->empty())
      return std::unexpected(StoredFunctionException("Function2 grid must contain an axis row"));
    List<double> xaxis;
    List<double> yaxis;
    MultiArray2D points;
    for (std::size_t y = 0; y < (*grid)->size(); ++y) {
      auto row = (**grid)[y].arrayPtr();
      if (!row)
        return std::unexpected(std::move(row.error()));
      if (y == 0) {
        if ((*row)->empty())
          return std::unexpected(StoredFunctionException("Function2 axis row must contain a header"));
        for (std::size_t x = 1; x < (*row)->size(); ++x) {
          auto value = (**row)[x].toFloat();
          if (!value)
            return std::unexpected(std::move(value.error()));
          xaxis.push_back(*value);
        }
        points.resize({(*row)->size() - 1, (*grid)->size() - 1});
      } else {
        if ((*row)->size() < 2)
          return std::unexpected(StoredFunctionException("Function2 row must contain an axis and samples"));
        auto axis = (**row)[0].toFloat();
        if (!axis)
          return std::unexpected(std::move(axis.error()));
        yaxis.push_back(*axis);
        auto cells = (**row)[1].arrayPtr();
        if (!cells)
          return std::unexpected(std::move(cells.error()));
        if ((*cells)->size() != xaxis.size())
          return std::unexpected(StarException("Number of sample points doesn't match axis size."));
        for (std::size_t x = 0; x < (*cells)->size(); ++x) {
          auto value = (**cells)[x].toFloat();
          if (!value)
            return std::unexpected(std::move(value.error()));
          auto set = points.set({x, y - 1}, *value);
          if (!set)
            return std::unexpected(std::move(set.error()));
        }
      }
    }
    auto interpolation = storedInterpolationMode(*interpolationName);
    if (!interpolation)
      return std::unexpected(std::move(interpolation.error()));
    auto bound = storedBoundMode(*boundName);
    if (!bound)
      return std::unexpected(std::move(bound.error()));
    MultiTable2D table;
    table.setRange(0, xaxis);
    table.setRange(1, yaxis);
    table.setInterpolationMode(*interpolation);
    table.setBoundMode(*bound);
    table.array() = std::move(points);
    return table;
  }();
  if (!parsed)
    return std::unexpected(StoredFunctionException("Error parsing function2 descriptor", parsed.error()));
  return parsed;
}

}
