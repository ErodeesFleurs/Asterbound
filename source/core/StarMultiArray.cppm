module;
import std;
import star.format;
import star.text_formatter;
import star.list;
import star.array;
import star.optional;

namespace Star {

struct MultiArrayExceptionTag {
  static constexpr std::string_view name() { return "MultiArrayException"; }
};
using MultiArrayException = StarError<MultiArrayExceptionTag, StarException>;

template <std::size_t rank, typename... Dimensions>
concept multi_array_dimensions = sizeof...(Dimensions) == rank
    && ((std::integral<Dimensions> && !std::same_as<std::remove_cv_t<Dimensions>, bool>
         && std::convertible_to<Dimensions, std::size_t>) && ...);

template <typename Reference, typename Source>
concept multi_array_assignable = requires(Reference&& element, Source&& source) {
  std::forward<Reference>(element) = std::forward<Source>(source);
};

template <typename Op, typename Index, typename Reference>
concept multi_array_callback = requires(Op& op, Index& index, Reference&& element) {
  std::invoke(op, index, std::forward<Reference>(element));
};

// Owning, last-axis-contiguous storage, matching std::layout_right. Element
// references follow the storage's reference type, including vector<bool> proxies.
template <typename ElementT, std::size_t array_rank>
  requires (array_rank > 0)
class MultiArray {
public:
  using Storage = List<ElementT>;
  using Element = ElementT;
  static constexpr std::size_t rank = array_rank;
  using IndexArray = Array<std::size_t, rank>;
  using SizeArray = Array<std::size_t, rank>;
  using Extents = std::dextents<std::size_t, rank>;
  using View = std::mdspan<Element, Extents, std::layout_right>;
  using ConstView = std::mdspan<Element const, Extents, std::layout_right>;
  using iterator = typename Storage::iterator;
  using const_iterator = typename Storage::const_iterator;
  using value_type = Element;
  using reference = typename Storage::reference;
  using const_reference = typename Storage::const_reference;

  MultiArray() = default;
  MultiArray(MultiArray const&) requires std::is_copy_constructible_v<Element> = default;
  MultiArray& operator=(MultiArray const&)
      requires (std::is_copy_constructible_v<Element> && std::is_copy_assignable_v<Element>) = default;

  MultiArray(MultiArray&& other) noexcept : m_data(std::move(other.m_data)), m_shape(other.m_shape) {
    other.clear();
  }

  MultiArray& operator=(MultiArray&& other) noexcept {
    if (this != std::addressof(other)) {
      m_data = std::move(other.m_data);
      m_shape = other.m_shape;
      other.clear();
    }
    return *this;
  }

  explicit MultiArray(SizeArray const& shape)
      requires (std::is_default_constructible_v<Element>
          && std::is_move_constructible_v<Element>) {
    require_success(set_size(shape));
  }

  explicit MultiArray(SizeArray const& shape, Element const& value)
      requires (std::is_copy_constructible_v<Element> && std::is_move_constructible_v<Element>) {
    require_success(set_size(shape, value));
  }

  template <typename... Dimensions>
    requires (multi_array_dimensions<rank, Dimensions...>
        && std::is_default_constructible_v<Element>
        && std::is_move_constructible_v<Element>)
  explicit MultiArray(Dimensions... dimensions) {
    auto shape = checked_dimensions(dimensions...);
    if (!shape) {
      Star::fatal_error(shape.error().what(), false);
      return;
    }
    require_success(set_size(*shape));
  }

  SizeArray const& extents() const noexcept { return m_shape; }
  std::size_t extent(std::size_t dimension) const noexcept { return m_shape[dimension]; }
  std::size_t size() const noexcept { return m_data.size(); }
  bool empty() const noexcept { return m_data.empty(); }
  std::size_t capacity() const noexcept { return m_data.capacity(); }
  std::size_t max_size() const noexcept { return m_data.max_size(); }

  void clear() {
    m_data.clear();
    m_shape.fill(0);
  }

  // Preserve the coordinate intersection; initialize only newly added cells.
  [[nodiscard]] std::expected<void, StarException> resize(SizeArray const& shape)
      requires (std::is_default_constructible_v<Element>
          && std::is_move_constructible_v<Element>
          && (std::is_constructible_v<Element, const_reference>
              || multi_array_assignable<reference, const_reference>)) {
    auto count = checked_size(shape);
    if (!count)
      return std::unexpected(std::move(count.error()));
    return resize_impl(shape, *count, [](Storage& data, std::size_t count) {
      data.resize(data.size() + count);
    });
  }

  [[nodiscard]] std::expected<void, StarException> resize(SizeArray const& shape, Element const& value)
      requires (std::is_copy_constructible_v<Element> && std::is_move_constructible_v<Element>) {
    auto count = checked_size(shape);
    if (!count)
      return std::unexpected(std::move(count.error()));
    return resize_impl(shape, *count, [&](Storage& data, std::size_t count) {
      data.resize(data.size() + count, value);
    });
  }

  template <typename... Dimensions>
    requires (multi_array_dimensions<rank, Dimensions...>
        && std::is_default_constructible_v<Element>
        && std::is_move_constructible_v<Element>
        && (std::is_constructible_v<Element, const_reference>
            || multi_array_assignable<reference, const_reference>))
  [[nodiscard]] std::expected<void, StarException> resize(Dimensions... dimensions) {
    auto shape = checked_dimensions(dimensions...);
    if (!shape)
      return std::unexpected(std::move(shape.error()));
    return resize(*shape);
  }

  void fill(Element const& element) requires multi_array_assignable<reference, Element const&> {
    std::fill(m_data.begin(), m_data.end(), element);
  }

  // Linear vector resize: retain the old prefix, initialize appended cells only.
  // Unlike resize(), this does not preserve multidimensional coordinates.
  [[nodiscard]] std::expected<void, StarException> set_size(SizeArray const& shape)
      requires (std::is_default_constructible_v<Element>
          && std::is_move_constructible_v<Element>) {
    auto count = checked_size(shape);
    if (!count)
      return std::unexpected(std::move(count.error()));
    m_data.resize(*count);
    m_shape = shape;
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> set_size(SizeArray const& shape, Element const& value)
      requires (std::is_copy_constructible_v<Element> && std::is_move_constructible_v<Element>) {
    auto count = checked_size(shape);
    if (!count)
      return std::unexpected(std::move(count.error()));
    m_data.resize(*count, value);
    m_shape = shape;
    return {};
  }

  template <typename... Dimensions>
    requires (multi_array_dimensions<rank, Dimensions...>
        && std::is_default_constructible_v<Element>
        && std::is_move_constructible_v<Element>)
  [[nodiscard]] std::expected<void, StarException> set_size(Dimensions... dimensions) {
    auto shape = checked_dimensions(dimensions...);
    if (!shape)
      return std::unexpected(std::move(shape.error()));
    return set_size(*shape);
  }

  reference operator[](std::size_t index) { return m_data[index]; }
  const_reference operator[](std::size_t index) const { return m_data[index]; }
  reference operator()(IndexArray const& index) { return m_data[linear_index(index)]; }
  const_reference operator()(IndexArray const& index) const { return m_data[linear_index(index)]; }

  template <typename... Dimensions> requires multi_array_dimensions<rank, Dimensions...>
  reference operator()(Dimensions... dimensions) {
    return (*this)(unchecked_dimensions(dimensions...));
  }

  template <typename... Dimensions> requires multi_array_dimensions<rank, Dimensions...>
  const_reference operator()(Dimensions... dimensions) const {
    return (*this)(unchecked_dimensions(dimensions...));
  }

  [[nodiscard]] std::expected<std::reference_wrapper<Element>, StarException> at(IndexArray const& index) &
      requires std::same_as<reference, Element&> {
    if (!contains(index))
      return std::unexpected(MultiArrayException(Star::format("Out of bounds on MultiArray::at({})", index)));
    return std::reference_wrapper<Element>(m_data[linear_index(index)]);
  }

  [[nodiscard]] std::expected<std::reference_wrapper<Element const>, StarException> at(IndexArray const& index) const &
      requires std::same_as<const_reference, Element const&> {
    if (!contains(index))
      return std::unexpected(MultiArrayException(Star::format("Out of bounds on MultiArray::at({})", index)));
    return std::reference_wrapper<Element const>(m_data[linear_index(index)]);
  }

  void at(IndexArray const&) && = delete;
  void at(IndexArray const&) const && = delete;

  template <typename... Dimensions>
    requires (multi_array_dimensions<rank, Dimensions...> && std::same_as<reference, Element&>)
  [[nodiscard]] std::expected<std::reference_wrapper<Element>, StarException> at(Dimensions... dimensions) & {
    auto index = checked_dimensions(dimensions...);
    if (!index)
      return std::unexpected(std::move(index.error()));
    return at(*index);
  }

  template <typename... Dimensions>
    requires (multi_array_dimensions<rank, Dimensions...> && std::same_as<const_reference, Element const&>)
  [[nodiscard]] std::expected<std::reference_wrapper<Element const>, StarException> at(Dimensions... dimensions) const & {
    auto index = checked_dimensions(dimensions...);
    if (!index)
      return std::unexpected(std::move(index.error()));
    return at(*index);
  }

  template <typename... Dimensions> requires multi_array_dimensions<rank, Dimensions...>
  void at(Dimensions...) && = delete;
  template <typename... Dimensions> requires multi_array_dimensions<rank, Dimensions...>
  void at(Dimensions...) const && = delete;

  [[nodiscard]] std::expected<void, StarException> set(IndexArray const& index, Element element)
      requires multi_array_assignable<reference, Element&&> {
    if (!contains(index))
      return std::unexpected(MultiArrayException(Star::format("Out of bounds on MultiArray::set({})", index)));
    m_data[linear_index(index)] = std::move(element);
    return {};
  }

  Element get(IndexArray const& index, Element def = Element()) const
      requires std::is_constructible_v<Element, const_reference> {
    if (!contains(index)) {
      if constexpr (std::is_move_constructible_v<Element>)
        return Element(std::move(def));
      else
        return Element(std::as_const(def));
    }
    return Element(m_data[linear_index(index)]);
  }

  [[nodiscard]] std::expected<void, StarException> set_resize(IndexArray const& index, Element element)
      requires (std::is_default_constructible_v<Element>
          && std::is_move_constructible_v<Element>
          && (std::is_constructible_v<Element, const_reference>
              || multi_array_assignable<reference, const_reference>)
          && multi_array_assignable<reference, Element&&>) {
    SizeArray shape;
    for (std::size_t dim = 0; dim < rank; ++dim) {
      if (index[dim] >= max_size())
        return std::unexpected(MultiArrayException("MultiArray::set_resize index exceeds max_size"));
      shape[dim] = std::max(m_shape[dim], index[dim] + 1);
    }
    auto resized = resize(shape);
    if (!resized)
      return std::unexpected(std::move(resized.error()));
    m_data[linear_index(index)] = std::move(element);
    return {};
  }

  // Copy assignment, clipped to the coordinate intersection of both arrays.
  [[nodiscard]] std::expected<void, StarException> copy_from(MultiArray const& source)
      requires multi_array_assignable<reference, const_reference> {
    SizeArray widths;
    for (std::size_t dim = 0; dim < rank; ++dim)
      widths[dim] = std::min(m_shape[dim], source.m_shape[dim]);
    return copy_region(source, IndexArray::filled(0), widths, IndexArray::filled(0));
  }

  // source_max is exclusive. Empty regions still require valid bounds. Identical
  // self-regions are no-ops; other overlapping self-regions are rejected.
  [[nodiscard]] std::expected<void, StarException> copy_region(MultiArray const& source,
      IndexArray const& source_min, IndexArray const& source_max, IndexArray const& target_min)
      requires multi_array_assignable<reference, const_reference> {
    SizeArray widths;
    bool is_empty = false;
    bool identical = true;
    bool overlaps = true;
    for (std::size_t dim = 0; dim < rank; ++dim) {
      if (source_min[dim] > source_max[dim] || source_max[dim] > source.m_shape[dim]
          || target_min[dim] > m_shape[dim])
        return std::unexpected(MultiArrayException("MultiArray::copy_region bounds are invalid"));
      widths[dim] = source_max[dim] - source_min[dim];
      if (widths[dim] > m_shape[dim] - target_min[dim])
        return std::unexpected(MultiArrayException("MultiArray::copy_region exceeds target extents"));
      is_empty = is_empty || widths[dim] == 0;
      identical = identical && source_min[dim] == target_min[dim];
      overlaps = overlaps && source_min[dim] < target_min[dim] + widths[dim]
          && target_min[dim] < source_max[dim];
    }
    if (is_empty)
      return {};
    if (this == std::addressof(source)) {
      if (identical)
        return {};
      if (overlaps)
        return std::unexpected(MultiArrayException("MultiArray::copy_region overlapping self-copy"));
    }
    auto source_strides = strides(source.m_shape);
    auto target_strides = strides(m_shape);
    copy_region_impl(source, widths, source_strides, target_strides,
        source.linear_index(source_min), linear_index(target_min), 0);
    return {};
  }

  // Unchecked region traversal: min and widths must describe an in-bounds region.
  // Callbacks are borrowed and invoked as lvalues, never copied or moved.
  template <typename Self, typename Op>
    requires (std::same_as<std::remove_cvref_t<Self>, MultiArray>
        && multi_array_callback<Op, IndexArray, decltype(std::declval<Self&>().m_data[std::size_t{}])>)
  void for_each(this Self&& self, IndexArray const& min, SizeArray const& widths, Op&& op) {
    for (std::size_t dim = 0; dim < rank; ++dim) {
      if constexpr (Star::debug_enabled)
        Star::star_assert_failed(min[dim] <= self.m_shape[dim] && widths[dim] <= self.m_shape[dim] - min[dim]);
      if (widths[dim] == 0)
        return;
    }
    auto array_strides = strides(self.m_shape);
    IndexArray index = min;
    for_each_impl<0>(self, min, widths, array_strides, op, index, self.linear_index(min));
  }

  template <typename Self, typename Op>
    requires (std::same_as<std::remove_cvref_t<Self>, MultiArray>
        && multi_array_callback<Op, IndexArray, decltype(std::declval<Self&>().m_data[std::size_t{}])>)
  void for_each(this Self&& self, Op&& op) {
    self.for_each(IndexArray::filled(0), self.m_shape, std::forward<Op>(op));
  }

  void print(TextBuffer& output) const requires std::same_as<optional_diagnostic_result<Element>, void> {
    sub_print(output, IndexArray(), 0);
  }

  // Bounded non-owning borrows of real-reference storage (not bool proxies).
  // Keep the owner alive; reacquire after resize, clear, move or replacement.
  std::span<Element> elements() & requires std::same_as<reference, Element&> {
    return m_data;
  }
  std::span<Element const> elements() const & requires std::same_as<const_reference, Element const&> {
    return m_data;
  }
  std::span<Element> elements() && = delete;
  std::span<Element const> elements() const && = delete;

  View view() & requires std::same_as<reference, Element&> {
    return View(elements().data(), Extents(std::span<std::size_t const, rank>(m_shape)));
  }
  ConstView view() const & requires std::same_as<const_reference, Element const&> {
    return ConstView(elements().data(), Extents(std::span<std::size_t const, rank>(m_shape)));
  }
  View view() && = delete;
  ConstView view() const && = delete;

private:
  static void require_success(std::expected<void, StarException> result) {
    if (!result)
      Star::fatal_error(result.error().what(), false);
  }

  template <std::integral Dimension>
  static bool dimension_representable(Dimension value) {
    if constexpr (std::is_signed_v<Dimension>)
      if (value < 0)
        return false;
    if constexpr (std::numeric_limits<Dimension>::digits > std::numeric_limits<std::size_t>::digits)
      if (value > static_cast<Dimension>(std::numeric_limits<std::size_t>::max()))
        return false;
    return true;
  }

  template <typename... Dimensions> requires multi_array_dimensions<rank, Dimensions...>
  static std::expected<SizeArray, StarException> checked_dimensions(Dimensions... dimensions) {
    if (!(dimension_representable(dimensions) && ...))
      return std::unexpected(MultiArrayException("MultiArray dimensions must be representable nonnegative integers"));
    return SizeArray{static_cast<std::size_t>(dimensions)...};
  }

  template <typename... Dimensions> requires multi_array_dimensions<rank, Dimensions...>
  static IndexArray unchecked_dimensions(Dimensions... dimensions) {
    if constexpr (Star::debug_enabled)
      Star::star_assert_failed((dimension_representable(dimensions) && ...));
    return IndexArray{static_cast<std::size_t>(dimensions)...};
  }

  std::expected<std::size_t, StarException> checked_size(SizeArray const& shape) const {
    bool is_empty = false;
    auto maximum = max_size();
    for (auto extent : shape) {
      if (extent > maximum)
        return std::unexpected(MultiArrayException("MultiArray extent exceeds max_size"));
      is_empty = is_empty || extent == 0;
    }
    if (is_empty)
      return 0;
    std::size_t count = 1;
    for (auto extent : shape) {
      if (count > maximum / extent)
        return std::unexpected(MultiArrayException("MultiArray extent product exceeds max_size"));
      count *= extent;
    }
    return count;
  }

  bool contains(IndexArray const& index) const {
    for (std::size_t dim = 0; dim < rank; ++dim)
      if (index[dim] >= m_shape[dim])
        return false;
    return true;
  }

  std::size_t linear_index(IndexArray const& index) const {
    std::size_t offset = 0;
    for (std::size_t dim = 0; dim < rank; ++dim) {
      if constexpr (Star::debug_enabled)
        Star::star_assert_failed(index[dim] < m_shape[dim]);
      offset = offset * m_shape[dim] + index[dim];
    }
    return offset;
  }

  // Only needed for nonempty shapes, whose checked cardinality bounds all strides.
  static SizeArray strides(SizeArray const& shape) {
    SizeArray result;
    result[rank - 1] = 1;
    for (std::size_t dim = rank - 1; dim != 0; --dim)
      result[dim - 1] = result[dim] * shape[dim];
    return result;
  }

  template <typename Append> requires std::invocable<Append&, Storage&, std::size_t>
  std::expected<void, StarException> resize_impl(SizeArray const& shape, std::size_t count, Append&& append) {
    if (shape == m_shape)
      return {};
    if (count == 0) {
      m_data.clear();
      m_shape = shape;
      return {};
    }
    bool linear = m_data.empty();
    if (!linear) {
      linear = true;
      for (std::size_t dim = 1; dim < rank; ++dim)
        linear = linear && shape[dim] == m_shape[dim];
    }
    if (linear) {
      if (count < m_data.size()) {
        if constexpr (std::is_default_constructible_v<Element>) {
          m_data.resize(count);
        } else {
          while (m_data.size() > count)
            m_data.pop_back();
        }
      } else {
        append(m_data, count - m_data.size());
      }
      m_shape = shape;
      return {};
    }

    Storage data;
    data.reserve(count);
    auto source_strides = strides(m_shape);
    resize_region_impl<0>(data, shape, source_strides, append, 0, true);
    m_data = std::move(data);
    m_shape = shape;
    return {};
  }

  template <std::size_t dimension, typename Append>
    requires (dimension < rank && std::invocable<Append&, Storage&, std::size_t>)
  void resize_region_impl(Storage& data, SizeArray const& shape, SizeArray const& source_strides,
      Append& append, std::size_t source_offset, bool source_valid) const {
    if constexpr (dimension == rank - 1) {
      auto copied = source_valid ? std::min(shape[dimension], m_shape[dimension]) : 0;
      if (copied != 0) {
        auto first = m_data.cbegin() + source_offset;
        if constexpr (std::is_constructible_v<Element, const_reference>
            && std::is_copy_assignable_v<Element> && std::is_move_assignable_v<Element>) {
          // Construct the preserved row in bulk without initializing it first.
          data.insert(data.end(), first, first + copied);
        } else if constexpr (std::is_constructible_v<Element, const_reference>) {
          for (std::size_t i = 0; i < copied; ++i, ++first)
            data.emplace_back(*first);
        } else {
          // Copy-assignment-only elements need live destination objects.
          auto target_offset = data.size();
          append(data, copied);
          std::copy_n(first, copied, data.begin() + target_offset);
        }
      }
      append(data, shape[dimension] - copied);
    } else {
      for (std::size_t i = 0; i < shape[dimension]; ++i) {
        bool valid = source_valid && i < m_shape[dimension];
        resize_region_impl<dimension + 1>(data, shape, source_strides, append,
            valid ? source_offset + i * source_strides[dimension] : 0, valid);
      }
    }
  }

  void copy_region_impl(MultiArray const& source, SizeArray const& widths,
      SizeArray const& source_strides, SizeArray const& target_strides,
      std::size_t source_offset, std::size_t target_offset, std::size_t dim)
      requires multi_array_assignable<reference, const_reference> {
    if (dim == rank - 1) {
      std::copy_n(source.m_data.cbegin() + source_offset, widths[dim], m_data.begin() + target_offset);
      return;
    }
    for (std::size_t i = 0; i < widths[dim]; ++i) {
      copy_region_impl(source, widths, source_strides, target_strides, source_offset, target_offset, dim + 1);
      if (i + 1 < widths[dim]) {
        source_offset += source_strides[dim];
        target_offset += target_strides[dim];
      }
    }
  }

  template <std::size_t dimension, typename Self, typename Op>
    requires (dimension < rank && (std::same_as<Self, MultiArray> || std::same_as<Self, MultiArray const>)
        && multi_array_callback<Op, IndexArray, decltype(std::declval<Self&>().m_data[std::size_t{}])>)
  static void for_each_impl(Self& self, IndexArray const& min, SizeArray const& widths,
      SizeArray const& array_strides, Op& op, IndexArray& index, std::size_t offset) {
    if constexpr (dimension == rank - 1) {
      auto element = self.m_data.begin() + offset;
      for (std::size_t i = 0; i < widths[dimension]; ++i, ++element) {
        index[dimension] = min[dimension] + i;
        std::invoke(op, index, *element);
      }
    } else {
      for (std::size_t i = 0; i < widths[dimension]; ++i) {
        index[dimension] = min[dimension] + i;
        for_each_impl<dimension + 1>(self, min, widths, array_strides, op, index,
            offset + i * array_strides[dimension]);
      }
    }
  }

  void sub_print(TextBuffer& output, IndexArray index, std::size_t dim) const
      requires std::same_as<optional_diagnostic_result<Element>, void> {
    if (dim == rank - 1) {
      for (std::size_t i = 0; i < m_shape[dim]; ++i) {
        index[dim] = i;
        auto&& value = m_data[linear_index(index)];
        append_text(output, optional_diagnostic_value(value));
        output.push_back(' ');
      }
      output.push_back('\n');
    } else {
      for (std::size_t i = 0; i < m_shape[dim]; ++i) {
        index[dim] = i;
        sub_print(output, index, dim + 1);
      }
      output.push_back('\n');
    }
  }

  Storage m_data;
  SizeArray m_shape;
};

using MultiArray2I = MultiArray<int, 2>;
using MultiArray2S = MultiArray<std::size_t, 2>;
using MultiArray2U = MultiArray<unsigned, 2>;
using MultiArray2F = MultiArray<float, 2>;
using MultiArray2D = MultiArray<double, 2>;
using MultiArray3I = MultiArray<int, 3>;
using MultiArray3S = MultiArray<std::size_t, 3>;
using MultiArray3U = MultiArray<unsigned, 3>;
using MultiArray3F = MultiArray<float, 3>;
using MultiArray3D = MultiArray<double, 3>;
using MultiArray4I = MultiArray<int, 4>;
using MultiArray4S = MultiArray<std::size_t, 4>;
using MultiArray4U = MultiArray<unsigned, 4>;
using MultiArray4F = MultiArray<float, 4>;
using MultiArray4D = MultiArray<double, 4>;

template <typename Element, std::size_t rank>
  requires (rank > 0 && std::same_as<optional_diagnostic_result<Element>, void>)
void write_text(TextBuffer& output, MultiArray<Element, rank> const& array) {
  array.print(output);
}

}

export module star.multi_array;

export template <typename Element, std::size_t rank>
  requires (rank > 0 && std::same_as<Star::optional_diagnostic_result<Element>, void>)
struct std::formatter<Star::MultiArray<Element, rank>> : Star::TextFormatter {};

export namespace Star {
  using ::Star::MultiArrayExceptionTag;
  using ::Star::MultiArrayException;
  using ::Star::multi_array_dimensions;
  using ::Star::multi_array_assignable;
  using ::Star::multi_array_callback;
  using ::Star::MultiArray;
  using ::Star::MultiArray2I;
  using ::Star::MultiArray2S;
  using ::Star::MultiArray2U;
  using ::Star::MultiArray2F;
  using ::Star::MultiArray2D;
  using ::Star::MultiArray3I;
  using ::Star::MultiArray3S;
  using ::Star::MultiArray3U;
  using ::Star::MultiArray3F;
  using ::Star::MultiArray3D;
  using ::Star::MultiArray4I;
  using ::Star::MultiArray4S;
  using ::Star::MultiArray4U;
  using ::Star::MultiArray4F;
  using ::Star::MultiArray4D;
  using ::Star::write_text;
}
