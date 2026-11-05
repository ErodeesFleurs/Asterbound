module;
import std;
import star.format;
import star.list;
import star.map;
import star.set;
import star.data_stream;
import star.multi_array;
import star.color;
import star.poly;
import star.either;
import star.ordered_map;
import star.ordered_set;

import star.array;
import star.vector;
import star.matrix3;
import star.line;
import star.rect;
import star.algorithm;
namespace Star {

// Declare the std-owned optional codec before generic containers instantiate
// dependent stream expressions; its payload need not associate namespace Star.
template <typename T>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, std::optional<T> const& value);
template <typename T>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, std::optional<T>& value);

template <typename... Types>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, std::variant<Types...> const& value);
template <typename... Types>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, std::variant<Types...>& value);

struct DataStreamWriteFunctor {
  DataStreamWriteFunctor(DataStream& ds) : ds(ds) {}

  DataStream& ds;
  template <typename T>
  [[nodiscard]] std::expected<void, StarException> operator()(T const& t) const {
    return ds.write(t);
  }
};

struct DataStreamReadFunctor {
  DataStreamReadFunctor(DataStream& ds) : ds(ds) {}

  DataStream& ds;
  template <typename T> [[nodiscard]] std::expected<void, StarException> operator()(T& t) const { return ds.read(t); }
};

[[nodiscard]] inline std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, std::monostate const&) {
  return std::ref(ds);
}

[[nodiscard]] inline std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, std::monostate&) {
  return std::ref(ds);
}

template <typename ElementT, std::size_t SizeN>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Array<ElementT, SizeN> const& array) {
  for (std::size_t i = 0; i < SizeN; ++i) {
    auto result = ds << array[i];
    if (!result)
      return result;
  }
  return std::ref(ds);
}

template <typename ElementT, std::size_t SizeN>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Array<ElementT, SizeN>& array) {
  for (std::size_t i = 0; i < SizeN; ++i) {
    auto result = ds >> array[i];
    if (!result)
      return result;
  }
  return std::ref(ds);
}

template <typename ElementT, std::size_t RankN>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, MultiArray<ElementT, RankN> const& array) {
  auto size = array.size();
  for (std::size_t i = 0; i < RankN; ++i) {
    auto dimensionWritten = ds.writeVlqU(size[i]);
    if (!dimensionWritten)
      return std::unexpected(std::move(dimensionWritten.error()));
  }

  std::size_t count = array.count();
  for (std::size_t i = 0; i < count; ++i) {
    auto result = ds << array.atIndex(i);
    if (!result)
      return result;
  }

  return std::ref(ds);
}

template <typename ElementT, std::size_t RankN>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, MultiArray<ElementT, RankN>& array) {
  typename MultiArray<ElementT, RankN>::SizeArray size;
  for (std::size_t i = 0; i < RankN; ++i) {
    auto dimension = ds.readVlqU();
    if (!dimension)
      return std::unexpected(std::move(dimension.error()));
    size[i] = *dimension;
  }

  array.setSize(size);
  std::size_t count = array.count();
  for (std::size_t i = 0; i < count; ++i) {
    auto result = ds >> array.atIndex(i);
    if (!result)
      return result;
  }

  return std::ref(ds);
}

[[nodiscard]] inline std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Color const& color) {
  return ds << color.toRgbaF();
}

[[nodiscard]] inline std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Color& color) {
  auto rgba = ds.read<Vec4F>();
  if (!rgba) return std::unexpected(std::move(rgba.error()));
  color = Color::rgbaf(*rgba);
  return std::ref(ds);
}

template <typename First, typename Second>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, std::pair<First, Second> const& pair) {
  return ds << pair.first << pair.second;
}

template <typename First, typename Second>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, std::pair<First, Second>& pair) {
  return ds >> pair.first >> pair.second;
}

template <typename Element>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, std::shared_ptr<Element> const& ptr) {
  return ds.pwrite(ptr).transform([&ds] { return std::ref(ds); });
}

template <typename Element>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, std::shared_ptr<Element>& ptr) {
  return ds.pread(ptr).transform([&ds] { return std::ref(ds); });
}

template <SequenceContainer Container>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Container const& list) {
  return ds.writeContainer(list).transform([&ds] { return std::ref(ds); });
}

template <SequenceContainer Container>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Container& list) {
  return ds.readContainer(list).transform([&ds] { return std::ref(ds); });
}

template <SetContainer Container>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Container const& set) {
  return ds.writeContainer(set).transform([&ds] { return std::ref(ds); });
}

template <SetContainer Container>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Container& set) {
  return ds.readContainer(set).transform([&ds] { return std::ref(ds); });
}

template <MapContainer Container>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Container const& map) {
  return ds.writeMapContainer(map).transform([&ds] { return std::ref(ds); });
}

template <MapContainer Container>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Container& map) {
  return ds.readMapContainer(map).transform([&ds] { return std::ref(ds); });
}

template <typename Key, typename Value, typename Compare, typename Allocator>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, OrderedMap<Key, Value, Compare, Allocator>& map) {
  return ds.readMapContainer(map).transform([&ds] { return std::ref(ds); });
}

template <typename Key, typename Value, typename Compare, typename Allocator>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, OrderedMap<Key, Value, Compare, Allocator> const& map) {
  return ds.writeMapContainer(map).transform([&ds] { return std::ref(ds); });
}

template <typename Key, typename Value, typename Hash, typename Equals, typename Allocator>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, OrderedHashMap<Key, Value, Hash, Equals, Allocator>& map) {
  return ds.readMapContainer(map).transform([&ds] { return std::ref(ds); });
}

template <typename Key, typename Value, typename Hash, typename Equals, typename Allocator>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, OrderedHashMap<Key, Value, Hash, Equals, Allocator> const& map) {
  return ds.writeMapContainer(map).transform([&ds] { return std::ref(ds); });
}

template <typename Value, typename Compare, typename Allocator>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, OrderedSet<Value, Compare, Allocator>& set) {
  return ds.readContainer(set).transform([&ds] { return std::ref(ds); });
}

template <typename Value, typename Compare, typename Allocator>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, OrderedSet<Value, Compare, Allocator> const& set) {
  return ds.writeContainer(set).transform([&ds] { return std::ref(ds); });
}

template <typename Value, typename Hash, typename Equals, typename Allocator>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, OrderedHashSet<Value, Hash, Equals, Allocator>& set) {
  return ds.readContainer(set).transform([&ds] { return std::ref(ds); });
}

template <typename Value, typename Hash, typename Equals, typename Allocator>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, OrderedHashSet<Value, Hash, Equals, Allocator> const& set) {
  return ds.writeContainer(set).transform([&ds] { return std::ref(ds); });
}

template <typename DataT>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Polygon<DataT> const& poly) {
  return ds.writeContainer(poly.vertexes()).transform([&ds] { return std::ref(ds); });
}

template <typename DataT>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Polygon<DataT>& poly) {
  return ds.readContainer(poly.vertexes()).transform([&ds] { return std::ref(ds); });
}

template <typename DataT, std::size_t Dimensions>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Box<DataT, Dimensions> const& box) {
  return ds << box.min() << box.max();
}

template <typename DataT, std::size_t Dimensions>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Box<DataT, Dimensions>& box) {
  return ds >> box.min() >> box.max();
}

template <typename DataT>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Matrix3<DataT> const& mat3) {
  return ds << mat3[0] << mat3[1] << mat3[2];
}

template <typename DataT>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Matrix3<DataT>& mat3) {
  return ds >> mat3[0] >> mat3[1] >> mat3[2];
}

// Variant payloads are decoded before construction.  Non-default-constructible
// alternatives provide DataStream::read<T>() specializations.

namespace detail {
template <std::size_t Index = 0, typename... Types>
[[nodiscard]] std::expected<void, StarException> readVariantPayload(DataStream& ds, std::variant<Types...>& value, std::uint8_t typeIndex) {
  if (typeIndex == Index) {
    using Type = std::variant_alternative_t<Index, std::variant<Types...>>;
    if constexpr (std::is_same_v<Type, std::monostate>) {
      value.template emplace<Index>();
    } else {
      auto payload = ds.read<Type>();
      if (!payload) return std::unexpected(std::move(payload.error()));
      value.template emplace<Index>(std::move(*payload));
    }
    return {};
  } else if constexpr (Index + 1 < sizeof...(Types)) {
    return readVariantPayload<Index + 1>(ds, value, typeIndex);
  } else {
    return std::unexpected(DataStreamException("Invalid variant type index"));
  }
}
}

template <typename... Types>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, std::variant<Types...> const& value) {
  static_assert(sizeof...(Types) <= 255);
  if (value.valueless_by_exception())
    return std::unexpected(DataStreamException("Cannot serialize a valueless variant"));
  auto tag = ds.write<std::uint8_t>(static_cast<std::uint8_t>(value.index()));
  if (!tag) return std::unexpected(std::move(tag.error()));
  return std::visit(
    [&ds] [[nodiscard]] (auto const& payload) -> std::expected<std::reference_wrapper<DataStream>, StarException> {
      if constexpr (std::is_same_v<std::remove_cvref_t<decltype(payload)>, std::monostate>)
        return std::ref(ds);
      else
        return ds << payload;
    },
    value);
}

template <typename... Types>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, std::variant<Types...>& value) {
  static_assert(sizeof...(Types) <= 255);
  auto tag = ds.read<std::uint8_t>();
  if (!tag) return std::unexpected(std::move(tag.error()));
  return detail::readVariantPayload(ds, value, *tag).transform([&ds] { return std::ref(ds); });
}

// Optional payloads are committed only after decoding succeeds.
template <typename T, typename WriteFunction>
[[nodiscard]] std::expected<void, StarException> writeOptional(DataStream& ds, std::optional<T> const& maybe, WriteFunction&& writeFunction) {
  auto tag = ds.write<bool>(maybe.has_value());
  if (!tag) return std::unexpected(std::move(tag.error()));
  if (maybe) {
    auto result = writeFunction(ds, *maybe);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

template <typename T, typename ReadFunction>
[[nodiscard]] std::expected<void, StarException> readOptional(DataStream& ds, std::optional<T>& maybe, ReadFunction&& readFunction) {
  auto present = ds.read<bool>();
  if (!present)
    return std::unexpected(std::move(present.error()));
  if (*present) {
    T payload;
    auto decoded = readFunction(ds, payload);
    if (!decoded)
      return std::unexpected(std::move(decoded.error()));
    maybe.emplace(std::move(payload));
  } else {
    maybe.reset();
  }
  return {};
}

template <typename T>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, std::optional<T> const& maybe) {
  return writeOptional(ds, maybe, [] [[nodiscard]] (DataStream & ds, T const& payload) { return ds.write(payload); })
    .transform([&ds] { return std::ref(ds); });
}

template <typename T>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, std::optional<T>& maybe) {
  auto present = ds.read<bool>();
  if (!present)
    return std::unexpected(std::move(present.error()));
  if (*present) {
    auto payload = ds.read<T>();
    if (!payload) return std::unexpected(std::move(payload.error()));
    maybe.emplace(std::move(*payload));
  } else {
    maybe.reset();
  }
  return std::ref(ds);
}

// Writes / reads an Either type, an Either can either have a left or right
// value, or in edge cases, nothing.

template <typename Left, typename Right>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Either<Left, Right> const& either) {
  if (either.isLeft()) {
    auto tagWritten = ds.write<std::uint8_t>(1);
    if (!tagWritten)
      return std::unexpected(std::move(tagWritten.error()));
    return ds << either.leftRef();
  } else if (either.isRight()) {
    auto tagWritten = ds.write<std::uint8_t>(2);
    if (!tagWritten)
      return std::unexpected(std::move(tagWritten.error()));
    return ds << either.rightRef();
  } else {
    return ds.write<std::uint8_t>(0).transform([&ds] { return std::ref(ds); });
  }
}

template <typename Left, typename Right>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Either<Left, Right>& either) {
  auto tag = ds.read<std::uint8_t>();
  if (!tag) return std::unexpected(std::move(tag.error()));
  if (*tag == 1) {
    auto payload = ds.read<Left>();
    if (!payload) return std::unexpected(std::move(payload.error()));
    either = makeLeft(std::move(*payload));
  } else if (*tag == 2) {
    auto payload = ds.read<Right>();
    if (!payload) return std::unexpected(std::move(payload.error()));
    either = makeRight(std::move(*payload));
  }
  return std::ref(ds);
}

template <typename DataT, std::size_t Dimensions>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Line<DataT, Dimensions> const& line) {
  return ds << line.min() << line.max();
}

template <typename DataT, std::size_t Dimensions>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Line<DataT, Dimensions>& line) {
  return ds >> line.min() >> line.max();
}

template <typename... T>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, std::tuple<T...>& t) {
  return std::apply(
    [&ds] [[nodiscard]] (auto&... values) -> std::expected<std::reference_wrapper<DataStream>, StarException> {
      std::expected<std::reference_wrapper<DataStream>, StarException> status = std::ref(ds);
      auto readOne = [&](auto& value) {
        if (status)
          status = ds >> value;
      };
      (readOne(values), ...);
      return status;
    },
    t);
}

template <typename... T>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, std::tuple<T...> const& t) {
  return std::apply(
    [&ds] [[nodiscard]] (auto const&... values) -> std::expected<std::reference_wrapper<DataStream>, StarException> {
      std::expected<std::reference_wrapper<DataStream>, StarException> status = std::ref(ds);
      auto writeOne = [&](auto const& value) {
        if (status)
          status = ds << value;
      };
      (writeOne(values), ...);
      return status;
    },
    t);
}

}

export module star.data_stream_extra;

export namespace Star {
  using ::Star::DataStreamWriteFunctor;
  using ::Star::DataStreamReadFunctor;
  using ::Star::operator<<;
  using ::Star::operator>>;
  using ::Star::writeOptional;
  using ::Star::readOptional;
}
