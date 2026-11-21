module;
import std;
import star.format;
import star.interpolation;
import star.list;
import star.math_common;

namespace Star {

namespace ParametricDetail {
  template <typename T>
  concept ordered_key = requires(T const& a, T const& b) { a < b ? 0 : 0; };

  // Only sort and consume storage whose ownership is known. Copying a span
  // (or a tuple of references) would still mutate the caller's elements.
  template <typename> inline constexpr bool point_owner = false;
  template <typename T, typename A> inline constexpr bool point_owner<std::vector<T, A>> = true;
  template <typename T, typename A> inline constexpr bool point_owner<List<T, A>> = true;
  template <typename T, std::size_t N> inline constexpr bool point_owner<std::array<T, N>> = true;

  struct PointLess {
    template <typename A, typename B>
    bool operator()(A const& a, B const& b) const {
      return std::get<0>(a) < std::get<0>(b) ? true : false;
    }
  };

  template <typename T>
  concept vector_element = std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>;

  template <typename Range, typename Index, typename Value>
  concept point_source = point_owner<std::remove_cvref_t<Range>>
    && std::ranges::random_access_range<std::remove_cvref_t<Range>>
    && ordered_key<Index> && vector_element<Index> && vector_element<Value>
    && requires(std::ranges::range_value_t<Range>&& point) {
      requires std::tuple_size<std::ranges::range_value_t<Range>>::value == 2;
      requires !std::is_reference_v<std::tuple_element_t<0, std::ranges::range_value_t<Range>>>;
      requires !std::is_reference_v<std::tuple_element_t<1, std::ranges::range_value_t<Range>>>;
      std::get<0>(point) < std::get<0>(point) ? 0 : 0;
      Index(std::get<0>(std::move(point)));
      Value(std::get<1>(std::move(point)));
    }
    && std::sortable<std::ranges::iterator_t<std::remove_cvref_t<Range>>, PointLess>
    && ((!std::is_lvalue_reference_v<Range> && !std::is_const_v<std::remove_reference_t<Range>>)
        || std::is_copy_constructible_v<std::ranges::range_value_t<Range>>);

  // Float-to-integer casts outside this half-open interval are undefined.
  // The power-of-two upper bound stays exact even when INT_MAX rounds up.
#pragma float_control(precise, on, push)
  template <std::integral I, std::floating_point F>
  bool in_integer_range(F value) {
    F limit = std::ldexp(F(1), std::numeric_limits<I>::digits);
    return value >= (std::is_signed_v<I> ? -limit : F(0)) && value < limit;
  }

  template <typename To, typename From>
  bool representable(From const& value) {
    if constexpr (std::same_as<To, From> || !std::is_arithmetic_v<To> || !std::is_arithmetic_v<From>) {
      return true;
    } else if constexpr (std::integral<From> && std::integral<To>) {
      return !std::cmp_less(+value, +std::numeric_limits<To>::lowest())
        && !std::cmp_greater(+value, +std::numeric_limits<To>::max());
    } else if constexpr (std::integral<To>) {
      return in_integer_range<To>(value) && static_cast<From>(static_cast<To>(value)) == value;
    } else if constexpr (std::integral<From>) {
      static_assert(std::numeric_limits<To>::max_exponent > std::numeric_limits<From>::digits);
      if constexpr (std::numeric_limits<To>::digits >= std::numeric_limits<From>::digits)
        return true;
      To converted = static_cast<To>(value);
      return in_integer_range<From>(converted) && static_cast<From>(converted) == value;
    } else if constexpr (std::numeric_limits<To>::max_exponent >= std::numeric_limits<From>::max_exponent
                         && std::numeric_limits<To>::digits >= std::numeric_limits<From>::digits) {
      return true;
    } else {
      constexpr auto limit = std::numeric_limits<To>::max();
      return value >= -limit && value <= limit && static_cast<From>(static_cast<To>(value)) == value;
    }
  }

  template <typename T>
  bool valid_key(T const& key) {
    if constexpr (std::floating_point<T>)
      return key == key; // NaNs do not meet sort's strict weak ordering precondition.
    else
      return true;
  }

  template <ordered_key T>
  bool increasing(T const& left, T const& right) {
    return left < right ? true : false;
  }
#pragma float_control(pop)

  template <typename T>
  concept nothrow_shiftable = std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>;

  template <typename T>
  concept insertable = nothrow_shiftable<T> || std::is_copy_constructible_v<T>;

  // Prepare both columns before committing either insertion. Usual scalar
  // and noexcept-movable columns just reserve geometrically; a throwing
  // element shift instead builds a replacement without changing the original.
  template <insertable T>
  auto prepare_insert(std::vector<T>& column, std::size_t pos, T const& entry) {
    if constexpr (nothrow_shiftable<T>) {
      if (column.size() == column.capacity()) {
        auto capacity = column.capacity();
        column.reserve(capacity + std::max<std::size_t>(1, std::min(capacity, column.max_size() - capacity)));
      }
      return std::monostate{};
    } else {
      std::vector<T> next;
      next.reserve(column.size() + 1);
      for (std::size_t i = 0; i < pos; ++i)
        next.emplace_back(column[i]);
      next.emplace_back(entry);
      for (std::size_t i = pos; i < column.size(); ++i)
        next.emplace_back(column[i]);
      return next;
    }
  }

  template <insertable T, typename Staging>
  void commit_insert(std::vector<T>& column, std::size_t pos, T& entry, Staging& staging) noexcept {
    if constexpr (nothrow_shiftable<T>)
      column.insert(column.begin() + pos, std::move(entry));
    else
      column.swap(staging);
  }

  template <typename T>
  concept borrowable_slot = std::same_as<typename std::vector<T>::const_reference, T const&>;

  template <typename Index, typename Value>
  concept interpolatable = requires(std::vector<Index> const& indexes, std::vector<Value> const& values,
                                    Index const& index, BoundMode mode) {
    std::expected<Value, StarException>(Value{});
    parametric_interpolate2(indexes, values, index, StepWeightOperator<Index>(), mode);
    parametric_interpolate2(indexes, values, index, LinearWeightOperator<Index>(), mode);
    parametric_interpolate4(indexes, values, index, Cubic4WeightOperator<Index>(true), mode);
  };
}

// Ordered index/value bins. Borrowed slots may be invalidated by mutation.
template <typename IndexType, typename ValueType = IndexType>
class ParametricTable {
public:
  using Index = IndexType;
  using Value = ValueType;

  // Sort a private copy of an lvalue input, or consume an owning rvalue.
  // Validate the destination keys, not just the source ordering.
  template <typename Range>
    requires ParametricDetail::point_source<Range, Index, Value>
  [[nodiscard]] static std::expected<ParametricTable, StarException> create(Range&& input) {
    // Bind rvalue storage directly: moving an inline array into another
    // local owner would move all its points before even starting the sort.
    using Storage = std::conditional_t<std::is_lvalue_reference_v<Range> || std::is_const_v<Range>,
                                       std::remove_cvref_t<Range>, Range&>;
    Storage points(input);
    if constexpr (std::floating_point<std::tuple_element_t<0, std::ranges::range_value_t<Range>>>) {
      for (auto const& point : points)
        if (!ParametricDetail::valid_key(std::get<0>(point)))
          return std::unexpected(MathException("Unordered index in ParametricTable"));
    }
    std::ranges::sort(points, ParametricDetail::PointLess{});
    ParametricTable table;
    table.reserve(points.size());
    for (auto& point : points) {
      auto appended = table.append_checked(std::get<0>(std::move(point)), std::get<1>(std::move(point)));
      if (!appended)
        return std::unexpected(std::move(appended.error()));
    }
    return table;
  }

  template <typename OtherIndex, typename OtherValue>
    requires ParametricDetail::ordered_key<Index> && std::constructible_from<Index, OtherIndex const&>
      && std::constructible_from<Value, OtherValue const&>
      && ParametricDetail::vector_element<Index> && ParametricDetail::vector_element<Value>
  [[nodiscard]] static std::expected<ParametricTable, StarException> convert(
      ParametricTable<OtherIndex, OtherValue> const& source) {
    ParametricTable table;
    table.reserve(source.size());
    for (std::size_t i = 0; i < source.size(); ++i) {
      OtherIndex const& key = source.m_indexes[i];
      OtherValue const& value = source.m_values[i];
      auto appended = table.append_checked(key, value);
      if (!appended)
        return std::unexpected(std::move(appended.error()));
    }
    return table;
  }

  // Unlike create(), insert accepts duplicates, after existing equal keys.
  // The supplied key must belong to the ordered domain (not a floating NaN).
  std::size_t insert(Index key, Value value)
    requires ParametricDetail::ordered_key<Index> && ParametricDetail::insertable<Index>
      && ParametricDetail::insertable<Value>
  {
    auto pos = static_cast<std::size_t>(std::upper_bound(m_indexes.begin(), m_indexes.end(), key) - m_indexes.begin());
    auto indexes = ParametricDetail::prepare_insert(m_indexes, pos, key);
    auto values = ParametricDetail::prepare_insert(m_values, pos, value);
    ParametricDetail::commit_insert(m_indexes, pos, key, indexes);
    ParametricDetail::commit_insert(m_values, pos, value, values);
    return pos;
  }

  void clear() {
    m_indexes.clear();
    m_values.clear();
  }
  std::size_t size() const { return m_indexes.size(); }
  bool empty() const { return m_indexes.empty(); }

  [[nodiscard]] std::expected<std::reference_wrapper<Index const>, StarException> index(std::size_t i) const &
    requires ParametricDetail::borrowable_slot<Index>
  {
    if (i >= size())
      return std::unexpected(MathException("Index out of bounds in ParametricTable::index"));
    return std::reference_wrapper<Index const>(m_indexes[i]);
  }
  void index(std::size_t) const && = delete;

  [[nodiscard]] std::expected<std::reference_wrapper<Value const>, StarException> value(std::size_t i) const &
    requires ParametricDetail::borrowable_slot<Value>
  {
    if (i >= size())
      return std::unexpected(MathException("Index out of bounds in ParametricTable::value"));
    return std::reference_wrapper<Value const>(m_values[i]);
  }
  void value(std::size_t) const && = delete;

  bool is_invertible() const requires ParametricDetail::ordered_key<Value> {
    if constexpr (std::floating_point<Value>)
      if (size() == 1)
        return ParametricDetail::valid_key(m_values.front());
    for (std::size_t i = 1; i < size(); ++i)
      if (!ParametricDetail::increasing<Value>(m_values[i - 1], m_values[i]))
        return false;
    return true;
  }

  [[nodiscard]] std::expected<void, StarException> invert()
    requires std::same_as<Index, Value> && ParametricDetail::ordered_key<Value>
  {
    if (!is_invertible())
      return std::unexpected(MathException("invert() called on non-invertible ParametricTable"));
    m_indexes.swap(m_values);
    return {};
  }

  // Strict predecessor, clamped to the first value at/below the first key.
  [[nodiscard]] std::expected<std::reference_wrapper<Value const>, StarException> get(Index const& key) const &
    requires ParametricDetail::ordered_key<Index> && ParametricDetail::borrowable_slot<Value>
  {
    if (empty())
      return std::unexpected(MathException("get called on empty ParametricTable"));
    auto i = std::lower_bound(m_indexes.begin(), m_indexes.end(), key);
    if (i != m_indexes.begin())
      --i;
    return std::reference_wrapper<Value const>(m_values[i - m_indexes.begin()]);
  }
  void get(Index const&) const && = delete;

protected:
  auto const& indexes() const & { return m_indexes; }
  auto const& values() const & { return m_values; }
  void indexes() const && = delete;
  void values() const && = delete;

private:
  template <typename, typename> friend class ParametricTable;

  void reserve(std::size_t count) {
    m_indexes.reserve(count);
    m_values.reserve(count);
  }

  // Only called while building a private, unpublished result.
  template <typename Key, typename Payload>
  std::expected<void, StarException> append_checked(Key&& key, Payload&& payload) {
    if (!ParametricDetail::representable<Index>(key) || !ParametricDetail::representable<Value>(payload))
      return std::unexpected(MathException("Inexact conversion in ParametricTable"));
    auto i = m_indexes.size();
    m_indexes.emplace_back(std::forward<Key>(key));
    if (i ? !ParametricDetail::increasing<Index>(m_indexes[i - 1], m_indexes[i])
          : !ParametricDetail::valid_key(m_indexes.back()))
      return std::unexpected(MathException("ParametricTable indexes must be strictly increasing"));
    m_values.emplace_back(std::forward<Payload>(payload));
    return {};
  }

  std::vector<Index> m_indexes;
  std::vector<Value> m_values;
};

template <typename Index, typename Value = Index>
class ParametricFunction : public ParametricTable<Index, Value> {
public:
  using Base = ParametricTable<Index, Value>;

  ParametricFunction() = default;
  explicit ParametricFunction(InterpolationMode interpolation_mode, BoundMode bound_mode = BoundMode::Clamp)
    : m_interpolation_mode(interpolation_mode), m_bound_mode(bound_mode) {}

  template <typename Range>
    requires ParametricDetail::point_source<Range, Index, Value>
  [[nodiscard]] static std::expected<ParametricFunction, StarException> create(
      Range&& points, InterpolationMode interpolation_mode = InterpolationMode::Linear,
      BoundMode bound_mode = BoundMode::Clamp) {
    auto base = Base::create(std::forward<Range>(points));
    if (!base)
      return std::unexpected(std::move(base.error()));
    return ParametricFunction(std::move(*base), interpolation_mode, bound_mode);
  }

  template <typename OtherIndex, typename OtherValue>
    requires requires(ParametricTable<OtherIndex, OtherValue> const& table) { Base::convert(table); }
  [[nodiscard]] static std::expected<ParametricFunction, StarException> convert(
      ParametricFunction<OtherIndex, OtherValue> const& source) {
    auto base = Base::convert(source);
    if (!base)
      return std::unexpected(std::move(base.error()));
    return ParametricFunction(std::move(*base), source.interpolation_mode(), source.bound_mode());
  }

  InterpolationMode interpolation_mode() const { return m_interpolation_mode; }
  void set_interpolation_mode(InterpolationMode mode) { m_interpolation_mode = mode; }
  BoundMode bound_mode() const { return m_bound_mode; }
  void set_bound_mode(BoundMode mode) { m_bound_mode = mode; }

  [[nodiscard]] std::expected<Value, StarException> interpolate(Index const& index) const
    requires ParametricDetail::interpolatable<Index, Value>
  {
    if (Base::empty())
      return Value();
    switch (m_interpolation_mode) {
    case InterpolationMode::HalfStep:
      return parametric_interpolate2(Base::indexes(), Base::values(), index, StepWeightOperator<Index>(), m_bound_mode);
    case InterpolationMode::Linear:
      return parametric_interpolate2(Base::indexes(), Base::values(), index, LinearWeightOperator<Index>(), m_bound_mode);
    case InterpolationMode::Cubic:
      return parametric_interpolate4(Base::indexes(), Base::values(), index, Cubic4WeightOperator<Index>(true),
                                    m_bound_mode);
    }
    return std::unexpected(MathException("Unsupported interpolation type in ParametricFunction::interpolate"));
  }

  [[nodiscard]] std::expected<Value, StarException> operator()(Index const& index) const
    requires ParametricDetail::interpolatable<Index, Value>
  {
    return interpolate(index);
  }

private:
  ParametricFunction(Base&& table, InterpolationMode interpolation_mode, BoundMode bound_mode)
    : Base(std::move(table)), m_interpolation_mode(interpolation_mode), m_bound_mode(bound_mode) {}

  InterpolationMode m_interpolation_mode = InterpolationMode::Linear;
  BoundMode m_bound_mode = BoundMode::Clamp;
};

}

export module star.parametric_function;

export namespace Star {
  using ::Star::ParametricTable;
  using ::Star::ParametricFunction;
}
