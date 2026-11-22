module;

import std;
import star.format;
import star.rect;
import star.list;
import star.hash;
import star.map;
import star.set;
import star.block_allocator;
import star.vector;
import star.math_common;

namespace Star {

template <typename T>
concept spatial_integer = std::integral<T> && !std::same_as<T, bool> && std::same_as<T, std::remove_cv_t<T>>;

template <typename T>
concept spatial_scalar = spatial_integer<T> || finite_classifiable<T>;

template <typename T>
concept spatial_key = hash_invocable<hash<T> const&, T const&>
    && requires(T const& a, T const& b) { { a == b } -> std::convertible_to<bool>; };

template <typename Range, typename Rect>
concept spatial_rect_range = std::ranges::input_range<Range>
    && std::same_as<std::ranges::range_value_t<Range>, Rect>
    && std::convertible_to<std::ranges::range_reference_t<Range>, Rect const&>;

template <typename Bounds, typename Rect>
concept spatial_query_bounds = std::same_as<std::remove_cvref_t<Bounds>, Rect> || spatial_rect_range<Bounds, Rect>;

template <typename Bounds, typename Rect>
concept spatial_entry_bounds = spatial_query_bounds<Bounds, Rect>
    || std::same_as<std::remove_cvref_t<Bounds>, typename Rect::Coord>;

template <typename Function, typename Value>
concept spatial_visitor = std::invocable<Function&, Value const&>
    && (std::same_as<std::invoke_result_t<Function&, Value const&>, void>
        || std::same_as<std::invoke_result_t<Function&, Value const&>, std::expected<void, StarException>>);

// An entry owns its exact closed bounds. The secondary index only borrows
// stable nodes; neither rehashing nor moving the node owner relocates them.
template <spatial_key KeyT, spatial_scalar ScalarT, typename ValueT, spatial_integer IntT = int,
          std::size_t block_size = 4096>
  requires allocator_block_size<block_size> && std::is_object_v<ValueT> && (!std::is_array_v<ValueT>)
class SpatialHash2D {
public:
  using Key = KeyT;
  using Scalar = ScalarT;
  using Rect = Box<Scalar, 2>;
  using Coord = typename Rect::Coord;
  using Value = ValueT;

private:
  using Sector = Vector<IntT, 2>;
  using SectorRange = Box<IntT, 2>;
  // Keep the common single rectangle compact. Only disjoint/overlapping
  // multi-rectangle coverage needs an explicit, sorted union of cells.
  struct Oversized {};
  using Cells = SmallList<Sector, 4>;
  using Coverage = std::variant<std::monostate, SectorRange, Cells, Oversized>;
  using Rects = SmallList<Rect, 2>;

public:
  struct Entry {
    template <typename V> requires std::constructible_from<Value, V>
    Entry(V&& initial, Rects&& bounds, Coverage&& sectors)
      : rects(std::move(bounds)), value(std::forward<V>(initial)), coverage(std::move(sectors)) {}

    Rects rects;
    Value value;

  private:
    friend class SpatialHash2D;
    Coverage coverage;
  };

  using EntryMap = StableHashMap<Key, Entry, hash<Key>, std::equal_to<Key>,
                                 BlockAllocator<std::pair<Key const, Entry>, block_size>>;

  [[nodiscard]] static std::expected<SpatialHash2D, StarException> create(Scalar const& sector_size) {
    if (!valid_size(sector_size))
      return std::unexpected(StarException("Spatial sector size must be finite and positive"));
    return SpatialHash2D(sector_size);
  }

  template <int size>
    requires (size > 0 && (std::floating_point<Scalar> || !std::cmp_greater(size, +std::numeric_limits<Scalar>::max())))
  explicit SpatialHash2D(std::integral_constant<int, size>) : SpatialHash2D(Scalar(size)) {}

  SpatialHash2D(SpatialHash2D const&) = delete;
  SpatialHash2D& operator=(SpatialHash2D const&) = delete;
  SpatialHash2D(SpatialHash2D&&) = default;
  SpatialHash2D& operator=(SpatialHash2D&& other) {
    if (this != std::addressof(other)) {
      entries_ = std::move(other.entries_);
      sectors_ = std::move(other.sectors_);
      oversized_ = std::move(other.oversized_);
      scale_ = other.scale_;
    }
    return *this;
  }

  List<Key> keys() const requires std::constructible_from<Key, Key const&> { return entries_.keys(); }
  List<Value> values() const requires std::constructible_from<Value, Value const&> {
    List<Value> result;
    result.reserve(size());
    for (auto const& item : entries_)
      result.emplace_back(item.second.value);
    return result;
  }
  EntryMap const& entries() const & { return entries_; }
  EntryMap const& entries() const && = delete;
  std::size_t size() const { return entries_.size(); }
  bool contains(Key const& key) const { return entries_.contains(key); }

  template <typename Self>
    requires std::same_as<Self, SpatialHash2D&> || std::same_as<Self, SpatialHash2D const&>
  [[nodiscard]] auto get(this Self&& self, Key const& key) {
    using Ref = std::reference_wrapper<std::remove_reference_t<decltype((self.entries_.begin()->second.value))>>;
    using Result = std::expected<Ref, StarException>;
    auto entry = self.entries_.get(key);
    if (!entry)
      return Result(std::unexpected(std::move(entry.error())));
    return Result(std::in_place, entry->get().value);
  }

  template <typename Self>
    requires std::same_as<Self, SpatialHash2D&> || std::same_as<Self, SpatialHash2D const&>
  auto find_ref(this Self&& self, Key const& key) {
    using Ref = std::reference_wrapper<std::remove_reference_t<decltype((self.entries_.begin()->second.value))>>;
    auto entry = self.entries_.find_ref(key);
    return entry ? std::optional<Ref>(std::in_place, entry->get().value) : std::optional<Ref>{};
  }

  Value value(Key const& key) const requires std::default_initializable<Value> && std::constructible_from<Value, Value const&> {
    auto found = find_ref(key);
    return found ? Value(found->get()) : Value();
  }

  template <spatial_query_bounds<Rect> Bounds>
    requires std::constructible_from<Value, Value const&>
  [[nodiscard]] std::expected<List<Value>, StarException> query_values(Bounds&& bounds) const {
    List<Value> result;
    auto visited = for_each(std::forward<Bounds>(bounds), [&](Value const& value) { result.emplace_back(value); });
    if (!visited)
      return std::unexpected(std::move(visited.error()));
    return result;
  }

  // Snapshot membership, not values: adding entries, replacing values, moving
  // bounds, reindexing and nested queries are safe. Do not erase entries or
  // replace/destroy the owner during a callback.
  template <spatial_query_bounds<Rect> Bounds, spatial_visitor<Value> Function>
  [[nodiscard]] std::expected<void, StarException> for_each(Bounds&& bounds, Function&& function) const {
    SmallList<EntryRef, 32> hits;
    bool saw_rect = false, may_repeat = false;
    auto gather = [&](Rect const& rect) -> std::expected<void, StarException> {
      auto range = sector_range(rect, scale_);
      if (!range)
        return std::unexpected(std::move(range.error()));
      if (range->is_null())
        return {};
      may_repeat |= std::exchange(saw_rect, true);
      auto consider = [&](Entry const& entry) {
        bool intersects = entry.rects.size() == 1 ? entry.rects.front().intersects(rect)
            : std::ranges::any_of(entry.rects, [&](Rect const& stored) {
                return stored.intersects(rect) && !stored.is_negative();
              });
        if (intersects)
          hits.emplace_back(entry);
      };
      auto visit_bucket = [&](Sector const& cell) {
        auto bucket = sectors_.find(cell);
        if (bucket != sectors_.end())
          for (EntryRef entry : bucket->second)
            consider(entry);
      };
      // A world-sized query costs O(entries), even when its cell count can't
      // be represented by size_t. Oversized entries are disjoint from buckets.
      if (range->min() == range->max()) {
        visit_bucket(range->min());
      } else if (cell_count(*range, std::max<std::size_t>(4, size()))) {
        may_repeat = true;
        each_cell(*range, visit_bucket);
      } else {
        for (auto const& item : entries_)
          if (!std::holds_alternative<std::monostate>(item.second.coverage))
            consider(item.second);
        return {};
      }
      for (EntryRef entry : oversized_)
        consider(entry);
      return {};
    };
    auto gathered = each_rect(std::forward<Bounds>(bounds), gather);
    if (!gathered)
      return gathered;

    // Address order is observable (first matching entity / first error).
    auto end = hits.end();
    if (hits.size() > 1) {
      std::sort(hits.begin(), end, [](EntryRef a, EntryRef b) {
        return std::less<>{}(std::addressof(a.get()), std::addressof(b.get()));
      });
      if (may_repeat)
        end = std::unique(hits.begin(), end, IdentityEqual{});
    }
    for (auto i = hits.begin(); i != end; ++i) {
      if constexpr (std::same_as<std::invoke_result_t<Function&, Value const&>, void>) {
        std::invoke(function, i->get().value);
      } else {
        auto visited = std::invoke(function, i->get().value);
        if (!visited)
          return visited;
      }
    }
    return {};
  }

  template <spatial_entry_bounds<Rect> Bounds>
  [[nodiscard]] std::expected<void, StarException> set(Key const& key, Bounds&& bounds) {
    auto existing = entries_.get(key);
    if (!existing)
      return std::unexpected(std::move(existing.error()));
    // Entity movement normally supplies a one-element world-split range.
    // Validate directly into a closed range, without staging another list
    // or reconstructing coverage for a move that stayed in the same cells.
    if constexpr (std::same_as<std::remove_cvref_t<Bounds>, Rect>)
      return set_single_bounds(existing->get(), bounds);
    else if constexpr (std::same_as<std::remove_cvref_t<Bounds>, Coord>)
      return set_single_bounds(existing->get(), Rect(bounds, bounds));
    else if constexpr (std::ranges::sized_range<Bounds>)
      if (std::ranges::size(bounds) == 1)
        return set_single_bounds(existing->get(), *std::ranges::begin(bounds));
    auto next = prepare_bounds(std::forward<Bounds>(bounds));
    if (!next)
      return std::unexpected(std::move(next.error()));
    update(existing->get(), std::move(*next));
    return {};
  }

  template <spatial_entry_bounds<Rect> Bounds, typename V>
    requires std::constructible_from<Key, Key const&> && std::constructible_from<Value, V>
        && std::is_assignable_v<Value&, V>
  [[nodiscard]] std::expected<void, StarException> set(Key const& key, Bounds&& bounds, V&& value) {
    auto next = prepare_bounds(std::forward<Bounds>(bounds));
    if (!next)
      return std::unexpected(std::move(next.error()));
    // try_emplace consumes none of its arguments on a hit.
    auto [position, inserted] = entries_.try_emplace(key, std::forward<V>(value), std::move(next->rects), std::move(next->coverage));
    auto& entry = position->second;
    if (inserted) {
      change_coverage(entry, Coverage{}, entry.coverage);
    } else {
      if constexpr (std::same_as<std::remove_cvref_t<V>, Value>) {
        if (std::addressof(entry.value) != std::addressof(value))
          entry.value = std::forward<V>(value);
      } else {
        entry.value = std::forward<V>(value);
      }
      update(entry, std::move(*next));
    }
    return {};
  }

  std::optional<Value> remove(Key const& key) requires std::constructible_from<Value, Value&&> {
    auto i = entries_.find(key);
    if (i == entries_.end())
      return {};
    change_coverage(i->second, i->second.coverage, Coverage{});
    std::optional<Value> result(std::in_place, std::move(i->second.value));
    entries_.erase(i);
    return result;
  }

  [[nodiscard]] std::expected<void, StarException> set_sector_size(Scalar const& sector_size) {
    if (!valid_size(sector_size))
      return std::unexpected(StarException("Spatial sector size must be finite and positive"));
    if (sector_size == scale_.size)
      return {};
    // New coordinates may no longer fit IntT. Prepare every replacement
    // before touching either the old index or the cached coverage of a node.
    SectorScale next_scale(sector_size);
    std::vector<std::pair<std::reference_wrapper<Entry>, Coverage>> plan;
    plan.reserve(size());
    for (auto& item : entries_) {
      CoverageBuilder builder;
      for (auto const& rect : item.second.rects) {
        auto range = sector_range(rect, next_scale);
        if (!range)
          return std::unexpected(std::move(range.error()));
        builder.add(*range);
      }
      plan.emplace_back(item.second, builder.finish());
    }
    for (auto& [node, coverage] : plan) {
      auto& entry = node.get();
      change_coverage(entry, entry.coverage, coverage);
      entry.coverage = std::move(coverage);
    }
    scale_ = next_scale;
    return {};
  }

private:
  using EntryRef = std::reference_wrapper<Entry const>;
  struct IdentityEqual {
    bool operator()(EntryRef a, EntryRef b) const noexcept {
      return std::addressof(a.get()) == std::addressof(b.get());
    }
  };
  struct IdentityHash {
    std::size_t operator()(EntryRef entry) const noexcept {
      return std::hash<decltype(std::addressof(entry.get()))>{}(std::addressof(entry.get()));
    }
  };
  using Bucket = HashSet<EntryRef, IdentityHash, IdentityEqual>;

  // Scaling by the exact reciprocal of a power of two is the same binary
  // operation as division. Never substitute a rounded reciprocal for a
  // general divisor, or use a subnormal/overflowed reciprocal.
  struct SectorScale {
    explicit SectorScale(Scalar size) : size(size) {
      if constexpr (finite_classifiable<Scalar>) {
        int exponent;
        if (std::frexp(double(size), &exponent) == .5 && exponent >= -1022 && exponent <= 1023)
          multiplier = std::ldexp(1., 1 - exponent);
      }
    }
    Scalar size;
    [[no_unique_address]] std::conditional_t<finite_classifiable<Scalar>, double, std::monostate> multiplier{};
  };

  explicit SpatialHash2D(Scalar sector_size) : scale_(sector_size) {}

  static bool valid_size(Scalar const& size) {
    if constexpr (finite_classifiable<Scalar>)
      if (!is_finite(size))
        return false;
    return size > 0;
  }

  // Exclusive positive bound: converting INT_MAX to a float can round up.
  // Keep division precise (no reciprocal approximation at cell edges), and
  // guard floor-to-integer conversion even when the division overflows.
  static bool sector_coordinate(Scalar coordinate, SectorScale const& scale, IntT& out) {
#pragma float_control(precise, on)
    if constexpr (spatial_integer<Scalar>) {
      auto quotient = coordinate / scale.size;
      if constexpr (std::signed_integral<Scalar>)
        if (coordinate < 0 && coordinate % scale.size != 0)
          --quotient;
      if (std::cmp_less(quotient, +std::numeric_limits<IntT>::lowest())
          || std::cmp_greater(quotient, +std::numeric_limits<IntT>::max()))
        return false;
      out = static_cast<IntT>(quotient);
    } else {
      double quotient = scale.multiplier ? double(coordinate) * scale.multiplier : double(coordinate) / double(scale.size);
      double cell = std::floor(quotient);
      if constexpr (std::same_as<Scalar, double>)
        if (quotient == 0 && coordinate < 0)
          cell = -1; // A finite binary32 ratio cannot underflow binary64.
      constexpr double limit = double(std::uintmax_t{1} << (std::numeric_limits<IntT>::digits - 1)) * 2;
      constexpr double lower = std::is_signed_v<IntT> ? -limit : 0;
      if (!(cell >= lower && cell < limit))
        return false;
      out = static_cast<IntT>(cell);
    }
    return true;
  }

  static std::expected<SectorRange, StarException> sector_range(Rect const& rect, SectorScale const& scale) {
    if constexpr (finite_classifiable<Scalar>) {
      // Read the stored objects, not temporary float return values: fast-math
      // can mark a by-value accessor result nofpclass(nan inf) at its ABI.
      auto finite = classify_finite(rect.min()[0], rect.min()[1], rect.max()[0], rect.max()[1]);
      if (!std::ranges::all_of(finite, std::identity{}))
        return std::unexpected(StarException("Spatial bounds must be finite"));
    }
    if (rect.is_negative()) [[unlikely]] {
      if (rect.is_null())
        return SectorRange::null();
      return std::unexpected(StarException("Inverted spatial bounds"));
    }
    Sector lo, hi;
    for (std::size_t axis = 0; axis != 2; ++axis) {
      if (!sector_coordinate(rect.min()[axis], scale, lo[axis])) [[unlikely]]
        return std::unexpected(StarException("Spatial bounds exceed the sector coordinate range"));
      if (rect.min()[axis] == rect.max()[axis])
        hi[axis] = lo[axis];
      else if (!sector_coordinate(rect.max()[axis], scale, hi[axis])) [[unlikely]]
        return std::unexpected(StarException("Spatial bounds exceed the sector coordinate range"));
    }
    return SectorRange(lo, hi);
  }

  // Compare inclusive extents without overflowing even for [INT_MIN, INT_MAX].
  static std::optional<std::size_t> cell_count(SectorRange const& range, std::size_t limit) {
    using U = std::make_unsigned_t<IntT>;
    auto width_delta = U(U(range.x_max()) - U(range.x_min()));
    auto height_delta = U(U(range.y_max()) - U(range.y_min()));
    if (width_delta >= limit)
      return {};
    auto width = std::size_t(width_delta) + 1;
    if (height_delta >= limit / width)
      return {};
    return width * (std::size_t(height_delta) + 1);
  }

  template <typename Function> requires std::invocable<Function&, Sector const&>
  static void each_cell(SectorRange const& range, Function&& function) {
    for (IntT x = range.x_min();; ++x) {
      for (IntT y = range.y_min();; ++y) {
        std::invoke(function, Sector{x, y});
        if (y == range.y_max())
          break;
      }
      if (x == range.x_max())
        break;
    }
  }

  template <spatial_entry_bounds<Rect> Bounds, typename Function>
    requires std::invocable<Function&, Rect const&>
        && std::same_as<std::invoke_result_t<Function&, Rect const&>, std::expected<void, StarException>>
  static std::expected<void, StarException> each_rect(Bounds&& bounds, Function&& function) {
    if constexpr (std::same_as<std::remove_cvref_t<Bounds>, Coord>) {
      return std::invoke(function, Rect(bounds, bounds));
    } else if constexpr (std::same_as<std::remove_cvref_t<Bounds>, Rect>) {
      return std::invoke(function, std::as_const(bounds));
    } else {
      for (Rect const& rect : bounds) {
        auto result = std::invoke(function, rect);
        if (!result)
          return result;
      }
      return {};
    }
  }

  struct CoverageBuilder {
    Coverage coverage;
    // Internal work limit, not an input limit. Oversized entries participate
    // in every candidate scan without materializing billions of empty buckets.
    std::size_t budget = 65536;

    void add(SectorRange const& range) {
      if (range.is_null() || std::holds_alternative<Oversized>(coverage))
        return;
      if (std::holds_alternative<SectorRange>(coverage) && std::get<SectorRange>(coverage) == range)
        return;
      auto count = cell_count(range, budget);
      if (!count) {
        coverage.template emplace<Oversized>();
        return;
      }
      budget -= *count;
      if (std::holds_alternative<std::monostate>(coverage)) {
        coverage = range;
        return;
      }
      if (std::holds_alternative<SectorRange>(coverage)) {
        auto previous = std::get<SectorRange>(coverage);
        auto& cells = coverage.template emplace<Cells>();
        each_cell(previous, [&](Sector const& cell) { cells.emplace_back(cell); });
      }
      auto& cells = std::get<Cells>(coverage);
      each_cell(range, [&](Sector const& cell) { cells.emplace_back(cell); });
    }

    Coverage finish() {
      if (std::holds_alternative<Cells>(coverage)) {
        auto& cells = std::get<Cells>(coverage);
        std::sort(cells.begin(), cells.end());
        cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
      }
      return std::move(coverage);
    }
  };

  struct PreparedBounds {
    Rects rects;
    Coverage coverage;
  };

  template <spatial_entry_bounds<Rect> Bounds>
  std::expected<PreparedBounds, StarException> prepare_bounds(Bounds&& bounds) const {
    PreparedBounds result;
    CoverageBuilder builder;
    auto checked = each_rect(std::forward<Bounds>(bounds), [&](Rect const& rect) -> std::expected<void, StarException> {
      auto range = sector_range(rect, scale_);
      if (!range)
        return std::unexpected(std::move(range.error()));
      builder.add(*range);
      result.rects.emplace_back(rect);
      return {};
    });
    if (!checked)
      return std::unexpected(std::move(checked.error()));
    result.coverage = builder.finish();
    return result;
  }

  static bool covers(Coverage const& coverage, Sector const& cell) {
    if (std::holds_alternative<SectorRange>(coverage))
      return std::get<SectorRange>(coverage).contains(cell);
    if (std::holds_alternative<Cells>(coverage)) {
      auto const& cells = std::get<Cells>(coverage);
      return std::binary_search(cells.begin(), cells.end(), cell);
    }
    return false;
  }

  template <typename Function> requires std::invocable<Function&, Sector const&>
  static void each_indexed_cell(Coverage const& coverage, Function&& function) {
    if (std::holds_alternative<SectorRange>(coverage))
      each_cell(std::get<SectorRange>(coverage), function);
    else if (std::holds_alternative<Cells>(coverage))
      for (auto const& cell : std::get<Cells>(coverage))
        std::invoke(function, cell);
  }

  void change_coverage(Entry const& entry, Coverage const& old, Coverage const& next) {
    if (std::holds_alternative<SectorRange>(old) && std::holds_alternative<SectorRange>(next)
        && std::get<SectorRange>(old) == std::get<SectorRange>(next))
      return;
    bool was_oversized = std::holds_alternative<Oversized>(old);
    bool is_oversized = std::holds_alternative<Oversized>(next);
    if (was_oversized != is_oversized) {
      if (is_oversized)
        oversized_.add(std::cref(entry));
      else
        oversized_.remove(std::cref(entry));
    }
    each_indexed_cell(old, [&](Sector const& cell) {
      if (!covers(next, cell)) {
        auto bucket = sectors_.find(cell);
        if (bucket->second.size() == 1)
          sectors_.erase(bucket);
        else
          bucket->second.remove(std::cref(entry));
      }
    });
    each_indexed_cell(next, [&](Sector const& cell) {
      if (!covers(old, cell))
        sectors_[cell].add(std::cref(entry));
    });
  }

  std::expected<void, StarException> set_single_bounds(Entry& entry, Rect const& rect) {
    auto range = sector_range(rect, scale_);
    if (!range)
      return std::unexpected(std::move(range.error()));
    if (!(std::holds_alternative<SectorRange>(entry.coverage) && std::get<SectorRange>(entry.coverage) == *range)) {
      CoverageBuilder builder;
      builder.add(*range);
      change_coverage(entry, entry.coverage, builder.coverage);
      entry.coverage = std::move(builder.coverage);
    }
    if (entry.rects.size() == 1) {
      entry.rects.front() = rect;
    } else {
      Rect saved(rect); // The input may be one of the old bounds.
      entry.rects.clear();
      entry.rects.emplace_back(std::move(saved));
    }
    return {};
  }

  void update(Entry& entry, PreparedBounds&& next) {
    change_coverage(entry, entry.coverage, next.coverage);
    entry.coverage = std::move(next.coverage);
    entry.rects = std::move(next.rects);
  }

  SectorScale scale_;
  EntryMap entries_;
  HashMap<Sector, Bucket> sectors_;
  Bucket oversized_;
};

}

export module star.spatial_hash_2d;

export namespace Star {
  using ::Star::SpatialHash2D;
  using ::Star::spatial_integer;
  using ::Star::spatial_scalar;
  using ::Star::spatial_key;
  using ::Star::spatial_rect_range;
  using ::Star::spatial_query_bounds;
  using ::Star::spatial_entry_bounds;
  using ::Star::spatial_visitor;
}
