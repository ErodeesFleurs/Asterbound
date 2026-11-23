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

template <typename T>
concept optional_stream_writable = requires(DataStream& ds, T const& payload) {
  { ds.write(payload) } -> std::same_as<std::expected<void, StarException>>;
};

template <typename T>
concept optional_stream_readable = requires(DataStream& ds, std::optional<T>& value, T& payload) {
  { ds.read<T>() } -> std::same_as<std::expected<T, StarException>>;
  value.emplace(std::move(payload));
};

// Declare the std-owned optional codec before generic containers instantiate
// dependent stream expressions; its payload need not associate namespace Star.
template <typename T>
  requires(optional_stream_writable<T>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator<<(DataStream& ds, std::optional<T> const& value);
template <typename T>
  requires(optional_stream_readable<T>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds,
                                                                                          std::optional<T>& value);

template <typename... Types>
  requires((stream_writable<Types> && ...))
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator<<(DataStream& ds, std::variant<Types...> const& value);
template <typename... Types>
  requires((stream_readable<Types> && ...))
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator>>(DataStream& ds, std::variant<Types...>& value);

struct DataStreamWriteFunctor {
  DataStreamWriteFunctor(DataStream& ds) : ds(ds) {}

  DataStream& ds;
  template <typename T>
    requires(stream_writable<T>)
  [[nodiscard]] std::expected<void, StarException> operator()(T const& t) const {
    return ds.write(t);
  }
};

struct DataStreamReadFunctor {
  DataStreamReadFunctor(DataStream& ds) : ds(ds) {}

  DataStream& ds;
  template <typename T>
    requires(stream_readable<T>)
  [[nodiscard]] std::expected<void, StarException> operator()(T& t) const {
    return ds.read(t);
  }
};

[[nodiscard]] inline std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, std::monostate const&) {
  return std::ref(ds);
}

[[nodiscard]] inline std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, std::monostate&) {
  return std::ref(ds);
}

template <typename ElementT, std::size_t extent>
  requires(stream_writable<ElementT>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator<<(DataStream& ds, Array<ElementT, extent> const& array) {
  for (std::size_t i = 0; i < extent; ++i) {
    auto result = ds << array[i];
    if (!result)
      return result;
  }
  return std::ref(ds);
}

template <typename ElementT, std::size_t extent>
  requires(stream_readable<ElementT>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator>>(DataStream& ds, Array<ElementT, extent>& array) {
  for (std::size_t i = 0; i < extent; ++i) {
    auto result = ds >> array[i];
    if (!result)
      return result;
  }
  return std::ref(ds);
}

template <typename ElementT, std::size_t rank>
  requires(rank > 0 && stream_writable<ElementT>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator<<(DataStream& ds, MultiArray<ElementT, rank> const& array) {
  auto const& size = array.extents();
  for (std::size_t i = 0; i < rank; ++i) {
    auto dimension_written = ds.write_vlq_u(size[i]);
    if (!dimension_written)
      return std::unexpected(std::move(dimension_written.error()));
  }

  std::size_t count = array.size();
  for (std::size_t i = 0; i < count; ++i) {
    auto result = ds << array[i];
    if (!result)
      return result;
  }

  return std::ref(ds);
}

template <typename ElementT, std::size_t rank>
  requires(rank > 0 && stream_readable<ElementT>
      && requires(DataStream& stream, MultiArray<ElementT, rank>& array,
                  typename MultiArray<ElementT, rank>::SizeArray const& shape) {
        { array.set_size(shape) } -> std::same_as<std::expected<void, StarException>>;
        { stream >> array[0] } -> stream_result;
      })
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator>>(DataStream& ds, MultiArray<ElementT, rank>& array) {
  typename MultiArray<ElementT, rank>::SizeArray size;
  for (std::size_t i = 0; i < rank; ++i) {
    auto dimension = ds.read_vlq_u();
    if (!dimension)
      return std::unexpected(std::move(dimension.error()));
    if (*dimension > std::numeric_limits<std::size_t>::max())
      return std::unexpected(MultiArrayException("MultiArray extent exceeds size_t"));
    size[i] = static_cast<std::size_t>(*dimension);
  }

  auto resized = array.set_size(size);
  if (!resized)
    return std::unexpected(std::move(resized.error()));
  std::size_t count = array.size();
  for (std::size_t i = 0; i < count; ++i) {
    auto result = ds >> array[i];
    if (!result)
      return result;
  }

  return std::ref(ds);
}

[[nodiscard]] inline std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Color const& color) {
  return ds << color.to_rgba_f();
}

[[nodiscard]] inline std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Color& color) {
  auto rgba = ds.read<Vec4F>();
  if (!rgba) return std::unexpected(std::move(rgba.error()));
  color = Color::rgba_f(*rgba);
  return std::ref(ds);
}

template <typename First, typename Second>
  requires(stream_writable<First> && stream_writable<Second>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator<<(DataStream& ds, std::pair<First, Second> const& pair) {
  return ds << pair.first << pair.second;
}

template <typename First, typename Second>
  requires(stream_readable<First> && stream_readable<Second>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator>>(DataStream& ds, std::pair<First, Second>& pair) {
  return ds >> pair.first >> pair.second;
}

template <typename Element>
  requires(stream_writable<std::remove_cv_t<Element>>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator<<(DataStream& ds, std::shared_ptr<Element> const& ptr) {
  return ds.pwrite(ptr).transform([&ds] { return std::ref(ds); });
}

template <typename Element>
  requires(stream_readable<std::remove_cv_t<Element>>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator>>(DataStream& ds, std::shared_ptr<Element>& ptr) {
  return ds.pread(ptr).transform([&ds] { return std::ref(ds); });
}

template <sequence_container Container>
  requires(stream_writable<typename Container::value_type>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds,
                                                                                          Container const& list) {
  return ds.write_container(list).transform([&ds] { return std::ref(ds); });
}

template <sequence_container Container>
  requires(stream_readable<typename Container::value_type>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds,
                                                                                          Container& list) {
  return ds.read_container(list).transform([&ds] { return std::ref(ds); });
}

template <set_container Container>
  requires(stream_writable<typename Container::value_type>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds,
                                                                                          Container const& set) {
  return ds.write_container(set).transform([&ds] { return std::ref(ds); });
}

template <set_container Container>
  requires(stream_readable<typename Container::value_type>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds,
                                                                                          Container& set) {
  return ds.read_container(set).transform([&ds] { return std::ref(ds); });
}

template <map_container Container>
  requires(stream_writable<typename Container::key_type> && stream_writable<typename Container::mapped_type>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds,
                                                                                          Container const& map) {
  return ds.write_map_container(map).transform([&ds] { return std::ref(ds); });
}

template <map_container Container>
  requires(stream_readable<typename Container::key_type> && stream_readable<typename Container::mapped_type>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds,
                                                                                          Container& map) {
  return ds.read_map_container(map).transform([&ds] { return std::ref(ds); });
}

template <typename Key, typename Value, typename Compare, typename Allocator>
  requires(stream_readable<Key> && stream_readable<Value>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator>>(DataStream& ds, OrderedMap<Key, Value, Compare, Allocator>& map) {
  return ds.read_map_container(map).transform([&ds] { return std::ref(ds); });
}

template <typename Key, typename Value, typename Compare, typename Allocator>
  requires(stream_writable<Key> && stream_writable<Value>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator<<(DataStream& ds, OrderedMap<Key, Value, Compare, Allocator> const& map) {
  return ds.write_map_container(map).transform([&ds] { return std::ref(ds); });
}

template <typename Key, typename Value, typename Hash, typename Equals, typename Allocator>
  requires(stream_readable<Key> && stream_readable<Value>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator>>(DataStream& ds, OrderedHashMap<Key, Value, Hash, Equals, Allocator>& map) {
  return ds.read_map_container(map).transform([&ds] { return std::ref(ds); });
}

template <typename Key, typename Value, typename Hash, typename Equals, typename Allocator>
  requires(stream_writable<Key> && stream_writable<Value>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator<<(DataStream& ds, OrderedHashMap<Key, Value, Hash, Equals, Allocator> const& map) {
  return ds.write_map_container(map).transform([&ds] { return std::ref(ds); });
}

template <typename Value, typename Compare, typename Allocator>
  requires(stream_readable<Value>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator>>(DataStream& ds, OrderedSet<Value, Compare, Allocator>& set) {
  return ds.read_container(set).transform([&ds] { return std::ref(ds); });
}

template <typename Value, typename Compare, typename Allocator>
  requires(stream_writable<Value>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator<<(DataStream& ds, OrderedSet<Value, Compare, Allocator> const& set) {
  return ds.write_container(set).transform([&ds] { return std::ref(ds); });
}

template <typename Value, typename Hash, typename Equals, typename Allocator>
  requires(stream_readable<Value>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator>>(DataStream& ds, OrderedHashSet<Value, Hash, Equals, Allocator>& set) {
  return ds.read_container(set).transform([&ds] { return std::ref(ds); });
}

template <typename Value, typename Hash, typename Equals, typename Allocator>
  requires(stream_writable<Value>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator<<(DataStream& ds, OrderedHashSet<Value, Hash, Equals, Allocator> const& set) {
  return ds.write_container(set).transform([&ds] { return std::ref(ds); });
}

template <typename DataT>
  requires(stream_writable<DataT>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds,
                                                                                          Polygon<DataT> const& poly) {
  return ds.write_container(poly.vertices()).transform([&ds] { return std::ref(ds); });
}

template <typename DataT>
  requires(stream_readable<DataT>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds,
                                                                                          Polygon<DataT>& poly) {
  return ds.read_container(poly.vertices()).transform([&ds] { return std::ref(ds); });
}

template <typename DataT, std::size_t dimension_count>
  requires(stream_writable<DataT>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator<<(DataStream& ds, Box<DataT, dimension_count> const& box) {
  return ds << box.min() << box.max();
}

template <typename DataT, std::size_t dimension_count>
  requires(stream_readable<DataT>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds,
                                                                                          Box<DataT, dimension_count>& box) {
  return ds >> box.min() >> box.max();
}

// Preserve nine scalar transfers in row-major order and the stream's byte order.
template <typename DataT>
  requires(stream_writable<DataT>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds,
                                                                                          Matrix3<DataT> const& mat3) {
  return ds << mat3[0] << mat3[1] << mat3[2];
}

template <typename DataT>
  requires(stream_readable<DataT>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds,
                                                                                          Matrix3<DataT>& mat3) {
  return ds >> mat3[0] >> mat3[1] >> mat3[2];
}

// Variant payloads are decoded before construction.  Non-default-constructible
// alternatives provide DataStream::read<T>() specializations.

namespace detail {
template <std::size_t index = 0, typename... Types>
  requires(index < sizeof...(Types) && (stream_readable<Types> && ...))
[[nodiscard]] std::expected<void, StarException> read_variant_payload(DataStream& ds, std::variant<Types...>& value,
                                                                      std::uint8_t type_index) {
  if (type_index == index) {
    using Type = std::variant_alternative_t<index, std::variant<Types...>>;
    if constexpr (std::is_same_v<Type, std::monostate>) {
      value.template emplace<index>();
    } else {
      auto payload = ds.read<Type>();
      if (!payload) return std::unexpected(std::move(payload.error()));
      value.template emplace<index>(std::move(*payload));
    }
    return {};
  } else if constexpr (index + 1 < sizeof...(Types)) {
    return read_variant_payload<index + 1>(ds, value, type_index);
  } else {
    return std::unexpected(DataStreamException("Invalid variant type index"));
  }
}
}

template <typename... Types>
  requires((stream_writable<Types> && ...))
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator<<(DataStream& ds, std::variant<Types...> const& value) {
  static_assert(sizeof...(Types) <= 255);
  if (value.valueless_by_exception())
    return std::unexpected(DataStreamException("Cannot serialize a valueless variant"));
  auto tag = ds.write<std::uint8_t>(static_cast<std::uint8_t>(value.index()));
  if (!tag) return std::unexpected(std::move(tag.error()));
  return std::visit(
    [&ds]<stream_writable T> [[nodiscard]] (
      T const& payload) -> std::expected<std::reference_wrapper<DataStream>, StarException> {
      if constexpr (std::is_same_v<std::remove_cvref_t<decltype(payload)>, std::monostate>)
        return std::ref(ds);
      else
        return ds << payload;
    },
    value);
}

template <typename... Types>
  requires((stream_readable<Types> && ...))
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator>>(DataStream& ds, std::variant<Types...>& value) {
  static_assert(sizeof...(Types) <= 255);
  auto tag = ds.read<std::uint8_t>();
  if (!tag) return std::unexpected(std::move(tag.error()));
  return detail::read_variant_payload(ds, value, *tag).transform([&ds] { return std::ref(ds); });
}

// Optional payloads are committed only after decoding succeeds.
template <typename T, typename WriteFunction>
  requires(stream_callback<WriteFunction, T const&>)
[[nodiscard]] std::expected<void, StarException> write_optional(DataStream& ds, std::optional<T> const& maybe,
                                                                WriteFunction&& write_function) {
  auto tag = ds.write<bool>(maybe.has_value());
  if (!tag) return std::unexpected(std::move(tag.error()));
  if (maybe) {
    if constexpr (!std::is_function_v<std::remove_reference_t<WriteFunction>>
                  && requires { { write_function == nullptr } -> std::convertible_to<bool>; }) {
      if (write_function == nullptr)
        return std::unexpected(DataStreamException("Cannot invoke an empty optional writer"));
    }
    auto result = write_function(ds, *maybe);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

template <typename T, typename ReadFunction>
  requires(requires { ::new T; } && stream_callback<ReadFunction, T&>
           && requires(std::optional<T>& value, T& payload) { value.emplace(std::move(payload)); })
[[nodiscard]] std::expected<void, StarException> read_optional(DataStream& ds, std::optional<T>& maybe,
                                                               ReadFunction&& read_function) {
  auto present = ds.read<bool>();
  if (!present)
    return std::unexpected(std::move(present.error()));
  if (*present) {
    if constexpr (!std::is_function_v<std::remove_reference_t<ReadFunction>>
                  && requires { { read_function == nullptr } -> std::convertible_to<bool>; }) {
      if (read_function == nullptr)
        return std::unexpected(DataStreamException("Cannot invoke an empty optional reader"));
    }
    T payload;
    auto decoded = read_function(ds, payload);
    if (!decoded)
      return std::unexpected(std::move(decoded.error()));
    maybe.emplace(std::move(payload));
  } else {
    maybe.reset();
  }
  return {};
}

template <typename T>
  requires(optional_stream_writable<T>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator<<(DataStream& ds, std::optional<T> const& maybe) {
  return write_optional(ds, maybe, [] [[nodiscard]] (DataStream & ds, T const& payload) { return ds.write(payload); })
    .transform([&ds] { return std::ref(ds); });
}

template <typename T>
  requires(optional_stream_readable<T>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds,
                                                                                          std::optional<T>& maybe) {
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

// Historical Either wire tags are 1/2, unlike std::variant's 0/1.
// Zero and unknown tags consume one byte and leave the destination unchanged.

template <typename Left, typename Right>
  requires(stream_writable<Left> && stream_writable<Right>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator<<(DataStream& ds, Either<Left, Right> const& either) {
  if (either.is_left())
    return ds << std::uint8_t{1} << either.left_ref();
  if (either.is_right())
    return ds << std::uint8_t{2} << either.right_ref();
  return ds << std::uint8_t{0};
}

template <typename Left, typename Right>
  requires(stream_read_factory<Left> && stream_read_factory<Right> && requires(Either<Left, Right>& destination, Left& left, Right& right) {
    destination.set_left(std::move(left));
    destination.set_right(std::move(right));
  })
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds,
                                                                                          Either<Left, Right>& either) {
  auto tag = ds.read<std::uint8_t>();
  if (!tag) return std::unexpected(std::move(tag.error()));
  if (*tag == 1) {
    auto payload = ds.read<Left>();
    if (!payload) return std::unexpected(std::move(payload.error()));
    either.set_left(std::move(*payload));
  } else if (*tag == 2) {
    auto payload = ds.read<Right>();
    if (!payload) return std::unexpected(std::move(payload.error()));
    either.set_right(std::move(*payload));
  }
  return std::ref(ds);
}

template <typename DataT, std::size_t dimension_count>
  requires(stream_writable<DataT>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator<<(DataStream& ds, Line<DataT, dimension_count> const& line) {
  return ds << line.min() << line.max();
}

template <typename DataT, std::size_t dimension_count>
  requires(stream_readable<DataT>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator>>(DataStream& ds, Line<DataT, dimension_count>& line) {
  return ds >> line.min() >> line.max();
}

template <typename... T>
  requires((stream_readable<T> && ...))
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds,
                                                                                          std::tuple<T...>& t) {
  return std::apply(
    [&ds] [[nodiscard]] (auto&... values) -> std::expected<std::reference_wrapper<DataStream>, StarException> {
      std::expected<std::reference_wrapper<DataStream>, StarException> status = std::ref(ds);
      auto read_one = [&](auto& value) {
        if (status)
          status = ds >> value;
      };
      (read_one(values), ...);
      return status;
    },
    t);
}

template <typename... T>
  requires((stream_writable<T> && ...))
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds,
                                                                                          std::tuple<T...> const& t) {
  return std::apply(
    [&ds] [[nodiscard]] (auto const&... values) -> std::expected<std::reference_wrapper<DataStream>, StarException> {
      std::expected<std::reference_wrapper<DataStream>, StarException> status = std::ref(ds);
      auto write_one = [&](auto const& value) {
        if (status)
          status = ds << value;
      };
      (write_one(values), ...);
      return status;
    },
    t);
}
}

export module star.data_stream_extra;

export namespace Star {
  using ::Star::DataStreamWriteFunctor;
  using ::Star::DataStreamReadFunctor;
  using ::Star::optional_stream_readable;
  using ::Star::optional_stream_writable;
  using ::Star::operator<<;
  using ::Star::operator>>;
  using ::Star::read_optional;
  using ::Star::write_optional;
}
