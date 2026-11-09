module;

import star.memory;
import std;
import star.hash;
import star.optional;
import star.text_formatter;
import star.format;

namespace Star {

// These concepts describe the expressions Array actually evaluates, rather than
// imposing regularity or implicit-bool conversion on element types.
template <typename Element, typename Source>
concept array_assignable = requires(Element& element, Source const& source) { element = source; };

template <typename Element>
concept array_unequal = requires(Element const& left, Element const& right) { (left != right) ? true : false; };

template <typename Element>
concept array_less = requires(Element const& left, Element const& right) { (left < right) ? true : false; };

template <typename Element, typename Iterator>
concept array_copy_iterator = requires(Iterator iterator, std::size_t count, Element* output) {
  std::ranges::copy_n(std::move(iterator), count, output);
};

template <typename Element>
concept array_hashable = requires(Star::hash<Element> const& hasher, Element const& element, std::size_t& seed) {
  hash_combine(seed, hasher(element));
};

template <typename Element>
concept array_text_writable = requires(TextBuffer& output, Element const& element) {
  append_text(output, optional_diagnostic_value(element));
} && (std::same_as<optional_diagnostic_result<Element>, void>
      || std::same_as<optional_diagnostic_result<Element>, std::expected<void, StarException>>);

// Value-initialized by default; element constructors deliberately default-
// construct storage, then assign in order with the historical narrowing rules.
template <typename ElementT, std::size_t SizeN> class Array {
  using Storage = std::array<ElementT, SizeN>;

public:
  using Element = ElementT;
  using value_type = Element;
  using size_type = typename Storage::size_type;
  using difference_type = typename Storage::difference_type;
  using reference = typename Storage::reference;
  using const_reference = typename Storage::const_reference;
  using pointer = typename Storage::pointer;
  using const_pointer = typename Storage::const_pointer;
  using iterator = typename Storage::iterator;
  using const_iterator = typename Storage::const_iterator;
  using reverse_iterator = typename Storage::reverse_iterator;
  using const_reverse_iterator = typename Storage::const_reverse_iterator;

  static constexpr Array filled(Element const& element) noexcept(std::is_nothrow_default_constructible_v<Storage>
                                                                 && std::is_nothrow_move_constructible_v<Storage>
                                                                 && noexcept(std::declval<Array&>().fill(element)))
    requires(std::is_default_constructible_v<Storage> && std::is_move_constructible_v<Storage>
             && (SizeN == 0 || array_assignable<Element, Element>))
  {
    Array result;
    result.fill(element);
    return result;
  }

  template <typename Iterator>
  static constexpr Array copy_from(Iterator iterator, std::size_t count = NPos)
      noexcept(std::is_nothrow_default_constructible_v<Storage>
          && std::is_nothrow_move_constructible_v<Storage>
          && std::is_nothrow_destructible_v<Iterator>
          && (SizeN == 0 || requires(Iterator& input, Element* output) {
            { std::ranges::copy_n(std::move(input), count, output) } noexcept;
          }))
      requires (std::is_default_constructible_v<Storage>
          && std::is_move_constructible_v<Storage>
          && (SizeN == 0 || array_copy_iterator<Element, Iterator>)) {
    Array result;
    if constexpr (SizeN != 0)
      std::ranges::copy_n(std::move(iterator), std::min(count, SizeN), result.begin());
    return result;
  }

  constexpr Array() noexcept(std::is_nothrow_default_constructible_v<Storage>)
    requires std::is_default_constructible_v<Storage>
      : m_storage() {}

  explicit constexpr Array(Element const& first)
      noexcept(std::is_nothrow_default_constructible_v<Storage>
          && requires(Element& element, Element const& source) {
            { element = source } noexcept;
          })
      requires (SizeN == 1 && std::is_default_constructible_v<Storage>
          && array_assignable<Element, Element>) {
    set(first);
  }

  template <typename... T>
  constexpr Array(Element const& first, T const&... rest)
      noexcept(std::is_nothrow_default_constructible_v<Storage>
          && requires(Element& element, Element const& source) {
            { element = source } noexcept;
          }
          && (requires(Element& element, T const& source) {
            { element = source } noexcept;
          } && ...))
      requires (sizeof...(T) + 1 == SizeN && std::is_default_constructible_v<Storage>
          && array_assignable<Element, Element> && (array_assignable<Element, T> && ...)) {
    set(first, rest...);
  }

  template <typename Source>
  explicit constexpr Array(Array<Source, SizeN> const& source)
      noexcept(std::is_nothrow_default_constructible_v<Storage>
          && (SizeN == 0 || requires(Element& element, Source const& value) {
            { element = value } noexcept;
          }))
      requires (std::is_default_constructible_v<Storage>
          && (SizeN == 0 || array_assignable<Element, Source>)) {
    if constexpr (SizeN != 0)
      std::copy(source.begin(), source.end(), begin());
  }

  constexpr iterator begin() noexcept { return m_storage.begin(); }
  constexpr const_iterator begin() const noexcept { return m_storage.begin(); }
  constexpr const_iterator cbegin() const noexcept { return m_storage.cbegin(); }
  constexpr iterator end() noexcept { return m_storage.end(); }
  constexpr const_iterator end() const noexcept { return m_storage.end(); }
  constexpr const_iterator cend() const noexcept { return m_storage.cend(); }
  constexpr reverse_iterator rbegin() noexcept { return m_storage.rbegin(); }
  constexpr const_reverse_iterator rbegin() const noexcept { return m_storage.rbegin(); }
  constexpr const_reverse_iterator crbegin() const noexcept { return m_storage.crbegin(); }
  constexpr reverse_iterator rend() noexcept { return m_storage.rend(); }
  constexpr const_reverse_iterator rend() const noexcept { return m_storage.rend(); }
  constexpr const_reverse_iterator crend() const noexcept { return m_storage.crend(); }

  constexpr pointer data() noexcept { return m_storage.data(); }
  constexpr const_pointer data() const noexcept { return m_storage.data(); }
  constexpr size_type size() const noexcept { return m_storage.size(); }
  constexpr size_type max_size() const noexcept { return m_storage.max_size(); }
  constexpr bool empty() const noexcept { return m_storage.empty(); }
  constexpr reference front() noexcept { return m_storage.front(); }
  constexpr const_reference front() const noexcept { return m_storage.front(); }
  constexpr reference back() noexcept { return m_storage.back(); }
  constexpr const_reference back() const noexcept { return m_storage.back(); }
  constexpr reference operator[](size_type i) noexcept { return m_storage[i]; }
  constexpr const_reference operator[](size_type i) const noexcept { return m_storage[i]; }

  template <std::size_t I>
  constexpr reference get() noexcept
    requires(I < SizeN)
  {
    return m_storage[I];
  }

  template <std::size_t I>
  constexpr const_reference get() const noexcept
    requires(I < SizeN)
  {
    return m_storage[I];
  }

  [[nodiscard]] std::expected<std::reference_wrapper<Element>, StarException> at(std::size_t i);
  [[nodiscard]] std::expected<std::reference_wrapper<Element const>, StarException> at(std::size_t i) const;

  constexpr void fill(Element const& element) noexcept(SizeN == 0 || noexcept(std::declval<Storage&>().fill(element)))
    requires(SizeN == 0 || array_assignable<Element, Element>)
  {
    if constexpr (SizeN != 0)
      m_storage.fill(element);
  }

  constexpr void swap(Array& other) noexcept(SizeN == 0 || std::is_nothrow_swappable_v<Element>)
    requires(SizeN == 0 || std::is_swappable_v<Element>)
  {
    if constexpr (SizeN != 0)
      m_storage.swap(other.m_storage);
  }

  template <typename Source>
  constexpr Array& operator=(Array<Source, SizeN> const& source)
      noexcept(SizeN == 0 || requires(Element& element, Source const& value) {
        { element = value } noexcept;
      })
      requires (SizeN == 0 || array_assignable<Element, Source>) {
    if constexpr (SizeN != 0)
      std::copy(source.begin(), source.end(), begin());
    return *this;
  }

  constexpr bool operator==(Array const& other) const
      noexcept(SizeN == 0 || requires(ElementT const& left, ElementT const& right) {
        { (left != right) ? true : false } noexcept;
      })
      requires (SizeN == 0 || array_unequal<ElementT>);
  // Deliberately weak: neither less nor greater does not imply element equality
  // (notably for floating-point NaN).
  constexpr std::weak_ordering operator<=>(Array const& other) const
      noexcept(SizeN == 0 || requires(ElementT const& left, ElementT const& right) {
        { (left < right) ? true : false } noexcept;
      })
      requires (SizeN == 0 || array_less<ElementT>);

  template <std::size_t Size2>
  constexpr Array<Element, Size2> to_size() const
      noexcept(std::is_nothrow_default_constructible_v<std::array<Element, Size2>>
          && std::is_nothrow_move_constructible_v<std::array<Element, Size2>>
          && (SizeN == 0 || Size2 == 0 || requires(Element const* input, Element* output, std::size_t count) {
            { std::ranges::copy_n(input, count, output) } noexcept;
          }))
      requires (std::is_default_constructible_v<std::array<Element, Size2>>
          && std::is_move_constructible_v<std::array<Element, Size2>>
          && (SizeN == 0 || Size2 == 0 || array_copy_iterator<Element, Element const*>)) {
    Array<Element, Size2> result;
    if constexpr (SizeN != 0 && Size2 != 0)
      std::ranges::copy_n(begin(), std::min(Size2, SizeN), result.begin());
    return result;
  }

private:
  template <typename... T>
  constexpr void set(T const&... values) noexcept((requires(Element& element, T const& source) {
    { element = source } noexcept;
  } && ...))
    requires(array_assignable<Element, T> && ...)
  {
    std::size_t i = 0;
    (static_cast<void>(m_storage[i++] = values), ...);
  }

  Storage m_storage;
};

template <typename DataT, std::size_t SizeT>
struct hash<Array<DataT, SizeT>> {
  constexpr std::size_t operator()(Array<DataT, SizeT> const& array) const
      noexcept(SizeT == 0 || requires(Star::hash<DataT> const& hasher, DataT const& value, std::size_t& seed) {
        { hash_combine(seed, hasher(value)) } noexcept;
      })
      requires (SizeT == 0 || array_hashable<DataT>);
  // Empty arrays do not need a usable element hasher.
  std::conditional_t<SizeT == 0, std::tuple<>, Star::hash<DataT>> dataHasher;
};

using Array2I = Array<int, 2>;
using Array2S = Array<std::size_t, 2>;
using Array2U = Array<unsigned, 2>;
using Array2F = Array<float, 2>;
using Array2D = Array<double, 2>;

using Array3I = Array<int, 3>;
using Array3S = Array<std::size_t, 3>;
using Array3U = Array<unsigned, 3>;
using Array3F = Array<float, 3>;
using Array3D = Array<double, 3>;

using Array4I = Array<int, 4>;
using Array4S = Array<std::size_t, 4>;
using Array4U = Array<unsigned, 4>;
using Array4F = Array<float, 4>;
using Array4D = Array<double, 4>;


template <typename Element, std::size_t Size>
std::expected<std::reference_wrapper<Element>, StarException> Array<Element, Size>::at(std::size_t i) {
  if (i >= Size)
    return std::unexpected(OutOfRangeException::format("out of range in Array::at({})", i));
  return std::reference_wrapper<Element>(m_storage[i]);
}

template <typename Element, std::size_t Size>
std::expected<std::reference_wrapper<Element const>, StarException> Array<Element, Size>::at(std::size_t i) const {
  if (i >= Size)
    return std::unexpected(OutOfRangeException::format("out of range in Array::at({})", i));
  return std::reference_wrapper<Element const>(m_storage[i]);
}

template <typename Element, std::size_t Size>
constexpr bool Array<Element, Size>::operator==(Array const& a) const
    noexcept(Size == 0 || requires(Element const& left, Element const& right) {
      { (left != right) ? true : false } noexcept;
    })
    requires (Size == 0 || array_unequal<Element>) {
  if constexpr (Size == 0) {
    return true;
  } else if constexpr (std::is_floating_point_v<Element>) {
    // Preserve the original comparison form under production fast-math.
    for (std::size_t i = 0; i < Size; ++i)
      if ((*this)[i] != a[i])
        return false;
    return true;
  } else {
    return std::equal(begin(), end(), a.begin(), a.end(), [](Element const& left, Element const& right) {
      if (left != right)
        return false;
      return true;
    });
  }
}

template <typename Element, std::size_t Size>
constexpr std::weak_ordering Array<Element, Size>::operator<=>(Array const& a) const
    noexcept(Size == 0 || requires(Element const& left, Element const& right) {
      { (left < right) ? true : false } noexcept;
    })
    requires (Size == 0 || array_less<Element>) {
  if constexpr (Size == 0) {
    return std::weak_ordering::equivalent;
  } else if constexpr (std::is_floating_point_v<Element>) {
    // Standard lexicographic dispatch changes infinity/NaN results with fast-math.
    for (std::size_t i = 0; i < Size; ++i) {
      if ((*this)[i] < a[i])
        return std::weak_ordering::less;
      if (a[i] < (*this)[i])
        return std::weak_ordering::greater;
    }
    return std::weak_ordering::equivalent;
  } else {
    return std::lexicographical_compare_three_way(begin(), end(), a.begin(), a.end(),
                                                  [](Element const& left, Element const& right) {
                                                    if (left < right)
                                                      return std::weak_ordering::less;
                                                    if (right < left)
                                                      return std::weak_ordering::greater;
                                                    return std::weak_ordering::equivalent;
                                                  });
  }
}

template <typename Element, std::size_t Size>
auto write_text(TextBuffer& output, Array<Element, Size> const& array)
  requires(Size == 0 || array_text_writable<Element>)
{
  output.push_back('[');
  if constexpr (Size == 0) {
    output.push_back(']');
  } else {
    using Result = optional_diagnostic_result<Element>;
    for (std::size_t i = 0; i < Size; ++i) {
      if constexpr (std::same_as<Result, void>) {
        append_text(output, optional_diagnostic_value(array[i]));
      } else {
        auto written = append_text(output, optional_diagnostic_value(array[i]));
        if (!written)
          return Result(std::unexpected(std::move(written.error())));
      }
      if (i != Size - 1)
        output.append(", ");
    }
    output.push_back(']');
    if constexpr (!std::same_as<Result, void>)
      return Result{};
  }
}

template <typename DataT, std::size_t SizeT>
constexpr std::size_t hash<Array<DataT, SizeT>>::operator()(Array<DataT, SizeT> const& a) const
    noexcept(SizeT == 0 || requires(Star::hash<DataT> const& hasher, DataT const& value, std::size_t& seed) {
      { hash_combine(seed, hasher(value)) } noexcept;
    })
    requires (SizeT == 0 || array_hashable<DataT>) {
  std::size_t hashval = 0;
  if constexpr (SizeT != 0) {
    for (std::size_t i = 0; i < SizeT; ++i)
      hash_combine(hashval, dataHasher(a[i]));
  }
  return hashval;
}
}

export module star.array;

export namespace Star {
  template <typename DataT, std::size_t SizeT>
  struct hash<Array<DataT, SizeT>>;
}


export namespace Star {
using ::Star::Array;
using ::Star::Array2D;
using ::Star::Array2F;
using ::Star::Array2I;
using ::Star::Array2S;
using ::Star::Array2U;
using ::Star::Array3D;
using ::Star::Array3F;
using ::Star::Array3I;
using ::Star::Array3S;
using ::Star::Array3U;
using ::Star::Array4D;
using ::Star::Array4F;
using ::Star::Array4I;
using ::Star::Array4S;
using ::Star::Array4U;
using ::Star::array_assignable;
using ::Star::array_copy_iterator;
using ::Star::array_hashable;
using ::Star::array_less;
using ::Star::array_text_writable;
using ::Star::array_unequal;
using ::Star::hash;
using ::Star::write_text;
}
