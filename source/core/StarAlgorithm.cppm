module;
import star.format;
import std;
import star.output_proxy;

namespace Star {

// Recognize only the project's checked result, without changing payload or
// reference semantics. Callable wrappers retain their own invocation policies.
template <typename T>
struct AlgorithmResultInfo {
  using Value = T;
  static constexpr bool Fallible = false;
};

template <typename T>
struct AlgorithmResultInfo<std::expected<T, StarException>> {
  using Value = T;
  static constexpr bool Fallible = true;
};

// Function that does nothing and takes any number of arguments
template <typename... T>
void nothing(T&&...) {}

// Functional constructor call / casting.
template <typename ToType>
struct construct {
  template <typename... FromTypes>
  ToType operator()(FromTypes&&... fromTypes) const {
    return ToType(std::forward<FromTypes>(fromTypes)...);
  }
};

template <typename Func>
struct SwallowReturn {
  template <typename... T>
  [[nodiscard]] auto operator()(T&&... args) {
    using Result = std::remove_cvref_t<std::invoke_result_t<Func&, T...>>;
    constexpr bool nullable = requires { func == nullptr; };
    if constexpr (nullable) {
      if (func == nullptr)
        return std::expected<void, StarException>(std::unexpected(StarException("Cannot invoke an empty callback")));
    }
    if constexpr (AlgorithmResultInfo<Result>::Fallible) {
      auto result = std::invoke(func, std::forward<T>(args)...);
      if (!result)
        return std::expected<void, StarException>(std::unexpected(std::move(result.error())));
      return std::expected<void, StarException>();
    } else {
      std::invoke(func, std::forward<T>(args)...);
      if constexpr (nullable)
        return std::expected<void, StarException>();
    }
  }

  Func func;
};

template <typename Func>
SwallowReturn<Func> swallow(Func f) {
  return SwallowReturn<Func>{std::move(f)};
}

struct Empty {
  bool operator==(Empty const) const {
    return true;
  }

  bool operator<(Empty const) const {
    return false;
  }
};

template <typename MapType1, typename MapType2>
[[nodiscard]] auto mapMerge(MapType1& targetMap, MapType2 const& sourceMap, bool overwrite = false) {
  using Insertion = std::remove_cvref_t<decltype(targetMap.insert(*sourceMap.begin()))>;
  constexpr bool fallible = AlgorithmResultInfo<Insertion>::Fallible;
  using Result = std::conditional_t<fallible, std::expected<bool, StarException>, bool>;
  bool noCommonKeys = true;
  for (auto i = sourceMap.begin(); i != sourceMap.end(); ++i) {
    auto res = targetMap.insert(*i);
    if constexpr (fallible) {
      if (!res)
        return Result(std::unexpected(std::move(res.error())));
      if (!res->second) {
        noCommonKeys = false;
        if (overwrite)
          res->first->second = i->second;
      }
    } else {
      if (!res.second) {
        noCommonKeys = false;
        if (overwrite)
          res.first->second = i->second;
      }
    }
  }
  return Result(noCommonKeys);
}

template <typename MapType1, typename MapType2>
bool mapsEqual(MapType1 const& m1, MapType2 const& m2) {
  if (&m1 == &m2)
    return true;

  if (m1.size() != m2.size())
    return false;

  for (auto const& m1pair : m1) {
    auto m2it = m2.find(m1pair.first);
    if (m2it == m2.end() || !(m2it->second == m1pair.second))
      return false;
  }

  return true;
}

template <typename Container, typename Filter>
[[nodiscard]] auto filter(Container& container, Filter&& filter) {
  using Predicate = std::remove_cvref_t<std::invoke_result_t<Filter&, decltype(*std::begin(container))>>;
  using Result = std::conditional_t<AlgorithmResultInfo<Predicate>::Fallible, std::expected<void, StarException>, void>;
  auto p = std::begin(container);
  while (p != std::end(container)) {
    if constexpr (AlgorithmResultInfo<Predicate>::Fallible && requires { filter == nullptr; }) {
      if (filter == nullptr)
        return Result(std::unexpected(StarException("Cannot invoke an empty filter")));
    }
    auto decision = std::invoke(filter, *p);
    bool keep;
    if constexpr (AlgorithmResultInfo<Predicate>::Fallible) {
      if (!decision)
        return Result(std::unexpected(std::move(decision.error())));
      keep = *decision;
    } else {
      keep = decision;
    }
    if (!keep)
      p = container.erase(p);
    else
      ++p;
  }
  if constexpr (AlgorithmResultInfo<Predicate>::Fallible)
    return Result();
}

template <typename OutContainer, typename InContainer, typename Filter>
[[nodiscard]] auto filtered(InContainer const& input, Filter&& filter) {
  using Predicate = std::remove_cvref_t<std::invoke_result_t<Filter&, decltype(*std::begin(input))>>;
  using Insertion = decltype(std::declval<OutContainer&>().insert(std::declval<OutContainer&>().end(), *std::begin(input)));
  constexpr bool fallible = AlgorithmResultInfo<Predicate>::Fallible || AlgorithmResultInfo<std::remove_cvref_t<Insertion>>::Fallible;
  using Result = std::conditional_t<fallible, std::expected<OutContainer, StarException>, OutContainer>;
  OutContainer out;
  auto p = std::begin(input);
  while (p != std::end(input)) {
    if constexpr (fallible && requires { filter == nullptr; }) {
      if (filter == nullptr)
        return Result(std::unexpected(StarException("Cannot invoke an empty filter")));
    }
    auto decision = std::invoke(filter, *p);
    bool keep;
    if constexpr (AlgorithmResultInfo<Predicate>::Fallible) {
      if (!decision)
        return Result(std::unexpected(std::move(decision.error())));
      keep = *decision;
    } else {
      keep = decision;
    }
    if (keep) {
      if constexpr (AlgorithmResultInfo<std::remove_cvref_t<Insertion>>::Fallible) {
        auto inserted = out.insert(out.end(), *p);
        if (!inserted)
          return Result(std::unexpected(std::move(inserted.error())));
      } else {
        out.insert(out.end(), *p);
      }
    }
    ++p;
  }
  return Result(std::move(out));
}

template <typename Container, typename Cond>
[[nodiscard]] auto eraseWhere(Container& container, Cond&& cond) {
  using Predicate = std::remove_cvref_t<std::invoke_result_t<Cond&, decltype(*std::begin(container))>>;
  using Result = std::conditional_t<AlgorithmResultInfo<Predicate>::Fallible, std::expected<void, StarException>, void>;
  auto p = std::begin(container);
  while (p != std::end(container)) {
    if constexpr (AlgorithmResultInfo<Predicate>::Fallible && requires { cond == nullptr; }) {
      if (cond == nullptr)
        return Result(std::unexpected(StarException("Cannot invoke an empty erase predicate")));
    }
    auto decision = std::invoke(cond, *p);
    bool erase;
    if constexpr (AlgorithmResultInfo<Predicate>::Fallible) {
      if (!decision)
        return Result(std::unexpected(std::move(decision.error())));
      erase = *decision;
    } else {
      erase = decision;
    }
    if (erase)
      p = container.erase(p);
    else
      ++p;
  }
  if constexpr (AlgorithmResultInfo<Predicate>::Fallible)
    return Result();
}

template <typename Container, typename Compare>
Container sorted(Container const& c, Compare comp) {
  static_assert(std::is_invocable_r_v<bool, Compare&, typename Container::value_type const&, typename Container::value_type const&>,
      "Sort comparison must be nonfallible; check fallible keys with sortByComputedValue first");
  auto c2 = c;
  std::sort(c2.begin(), c2.end(), comp);
  return c2;
}


template <typename Container>
Container sorted(Container const& c) {
  auto c2 = c;
  std::sort(c2.begin(), c2.end(), std::less<typename Container::value_type>());
  return c2;
}


// Sort a container by the output of a computed value. The computed value is
// only computed *once* per item in the container, which is useful both for
// when the computed value is costly, and to avoid sorting instability with
// floating point values.  Container must have size() and operator[], and also
// must be constructable with Container(size_t).
template <typename Container, typename Getter>
[[nodiscard]] auto sortByComputedValue(Container& container, Getter&& valueGetter, bool stable = false)
    -> std::conditional_t<AlgorithmResultInfo<std::remove_cvref_t<std::invoke_result_t<Getter&, typename Container::value_type&>>>::Fallible,
        std::expected<void, StarException>, void> {
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Getter&, typename Container::value_type&>>;
  using ComputedValue = typename AlgorithmResultInfo<Invocation>::Value;
  using ComputedPair = std::pair<ComputedValue, std::size_t>;
  static_assert(std::is_convertible_v<decltype(std::declval<ComputedValue const&>() < std::declval<ComputedValue const&>()), bool>,
      "Computed sort keys must provide a nonfallible bool ordering");

  std::size_t containerSize = container.size();
  if (containerSize <= 1) {
    if constexpr (AlgorithmResultInfo<Invocation>::Fallible)
      return {};
    else
      return;
  }

  std::vector<ComputedPair> work;
  work.reserve(containerSize);
  for (std::size_t i = 0; i < containerSize; ++i) {
    if constexpr (AlgorithmResultInfo<Invocation>::Fallible) {
      if constexpr (requires { valueGetter == nullptr; }) {
        if (valueGetter == nullptr)
          return std::unexpected(StarException("Cannot invoke an empty sort key getter"));
      }
      auto computed = std::invoke(valueGetter, container[i]);
      if (!computed)
        return std::unexpected(std::move(computed.error()));
      work.emplace_back(std::move(*computed), i);
    } else {
      work.emplace_back(std::invoke(valueGetter, container[i]), i);
    }
  }

  auto compare = [](ComputedPair const& a, ComputedPair const& b) { return a.first < b.first; };

  // Sort the comptued values and the associated indexes
  if (stable)
    std::stable_sort(work.begin(), work.end(), compare);
  else
    std::sort(work.begin(), work.end(), compare);

  Container result(containerSize);
  for (std::size_t i = 0; i < containerSize; ++i) {
    if constexpr (requires { swap(result[i], container[work[i].second]); })
      swap(result[i], container[work[i].second]);
    else
      std::swap(result[i], container[work[i].second]);
  }

  if constexpr (requires { swap(container, result); })
    swap(container, result);
  else
    std::swap(container, result);
  if constexpr (AlgorithmResultInfo<Invocation>::Fallible)
    return {};
}

template <typename Container, typename Getter>
[[nodiscard]] auto stableSortByComputedValue(Container& container, Getter&& valueGetter) {
  return sortByComputedValue(container, std::forward<Getter>(valueGetter), true);
}

template <typename T>
T copy(T c) {
  return c;
}

template <typename OutContainer, typename InContainer, typename Function>
[[nodiscard]] auto transformInto(OutContainer& outContainer, InContainer&& inContainer, Function&& function) {
  using Input = decltype(std::forward_like<InContainer>(*std::begin(inContainer)));
  using Invocation = std::remove_cvref_t<std::invoke_result_t<Function&, Input>>;
  using Value = typename AlgorithmResultInfo<Invocation>::Value;
  using Insertion = decltype(outContainer.insert(outContainer.end(), std::declval<Value>()));
  constexpr bool fallible = AlgorithmResultInfo<Invocation>::Fallible || AlgorithmResultInfo<std::remove_cvref_t<Insertion>>::Fallible;
  using Result = std::conditional_t<fallible, std::expected<void, StarException>, void>;
  for (auto&& elem : inContainer) {
    if constexpr (fallible && requires { function == nullptr; }) {
      if (function == nullptr)
        return Result(std::unexpected(StarException("Cannot invoke an empty transform callback")));
    }
    if constexpr (AlgorithmResultInfo<Invocation>::Fallible) {
      auto value = std::invoke(function, std::forward_like<InContainer>(elem));
      if (!value)
        return Result(std::unexpected(std::move(value.error())));
      if constexpr (AlgorithmResultInfo<std::remove_cvref_t<Insertion>>::Fallible) {
        auto inserted = outContainer.insert(outContainer.end(), std::move(*value));
        if (!inserted)
          return Result(std::unexpected(std::move(inserted.error())));
      } else {
        outContainer.insert(outContainer.end(), std::move(*value));
      }
    } else {
      if constexpr (AlgorithmResultInfo<std::remove_cvref_t<Insertion>>::Fallible) {
        auto inserted = outContainer.insert(outContainer.end(), std::invoke(function, std::forward_like<InContainer>(elem)));
        if (!inserted)
          return Result(std::unexpected(std::move(inserted.error())));
      } else {
        outContainer.insert(outContainer.end(), std::invoke(function, std::forward_like<InContainer>(elem)));
      }
    }
  }
  if constexpr (fallible)
    return Result();
}

template <typename OutContainer, typename InContainer, typename Function>
[[nodiscard]] auto transform(InContainer&& container, Function&& function) {
  OutContainer res;
  if constexpr (std::ranges::sized_range<InContainer>) {
    if constexpr (requires {
                    { res.reserve(std::ranges::size(container)) } -> std::same_as<void>;
                  })
      res.reserve(std::ranges::size(container));
  }
  using Status = decltype(transformInto(res, std::forward<InContainer>(container), std::forward<Function>(function)));
  if constexpr (std::is_void_v<Status>) {
    transformInto(res, std::forward<InContainer>(container), std::forward<Function>(function));
    return res;
  } else {
    auto transformed = transformInto(res, std::forward<InContainer>(container), std::forward<Function>(function));
    if (!transformed)
      return std::expected<OutContainer, StarException>(std::unexpected(std::move(transformed.error())));
    return std::expected<OutContainer, StarException>(std::move(res));
  }
}

// Moves the given value and into an rvalue.  Works whether or not the type has
// a valid move constructor or not.  Always leaves the given value in its
// default constructed state.
// Default construction follows the move; std::exchange constructs its
// replacement first, which differs if default construction throws.
template <typename T>
T take(T& t) {
  T t2 = std::move(t);
  t = T();
  return t2;
}

template <typename Functor>
class FinallyGuard {
public:
  FinallyGuard(Functor functor) : functor(std::move(functor)), dismiss(false) {}

  FinallyGuard(FinallyGuard&& o) : functor(std::move(o.functor)), dismiss(o.dismiss) {
    o.cancel();
  }

  FinallyGuard& operator=(FinallyGuard&& o) {
    functor = std::move(o.functor);
    dismiss = o.dismiss;
    o.cancel();
    return *this;
  }

  ~FinallyGuard() {
    static_assert(std::is_void_v<std::invoke_result_t<Functor&>>,
        "Cleanup callbacks must handle their errors at the cleanup boundary");
    if (!dismiss)
      functor();
  }

  void cancel() {
    dismiss = true;
  }

private:
  Functor functor;
  bool dismiss;
};

template <typename Functor>
FinallyGuard<typename std::decay<Functor>::type> finally(Functor&& f) {
  return FinallyGuard<Functor>(std::forward<Functor>(f));
}

// For generic types, directly use the result of the signature of its
// 'operator()'
template <typename T>
struct FunctionTraits : public FunctionTraits<decltype(&T::operator())> {};

template <typename ReturnType, typename... ArgsTypes>
struct FunctionTraits<ReturnType(ArgsTypes...)> {
  // arity is the number of arguments.
  static constexpr std::size_t Arity = sizeof...(ArgsTypes);

  using Return = ReturnType;

  using ArgTuple = std::tuple<ArgsTypes...>;

  template <std::size_t i>
  struct Arg {
    // the i-th argument is equivalent to the i-th tuple element of a tuple
    // composed of those arguments.
    using type = std::tuple_element_t<i, ArgTuple>;
  };
};

template <typename ReturnType, typename... Args>
struct FunctionTraits<ReturnType (*)(Args...)> : public FunctionTraits<ReturnType(Args...)> {};

template <typename FunctionType>
struct FunctionTraits<std::function<FunctionType>> : public FunctionTraits<FunctionType> {};

template <typename ClassType, typename ReturnType, typename... Args>
struct FunctionTraits<ReturnType (ClassType::*)(Args...)> : public FunctionTraits<ReturnType(Args...)> {
  using OwnerType = ClassType&;
};

template <typename ClassType, typename ReturnType, typename... Args>
struct FunctionTraits<ReturnType (ClassType::*)(Args...) const> : public FunctionTraits<ReturnType(Args...)> {
  using OwnerType = ClassType const&;
};

template <typename T>
struct FunctionTraits<T&> : public FunctionTraits<T> {};

template <typename T>
struct FunctionTraits<T const&> : public FunctionTraits<T> {};

template <typename T>
struct FunctionTraits<T&&> : public FunctionTraits<T> {};

template <typename T>
struct FunctionTraits<T const&&> : public FunctionTraits<T> {};

}

export module star.algorithm;

export namespace Star {
  using ::Star::AlgorithmResultInfo;
  using ::Star::nothing;
  using ::Star::construct;
  using ::Star::SwallowReturn;
  using ::Star::swallow;
  using ::Star::Empty;
  using ::Star::mapMerge;
  using ::Star::mapsEqual;
  using ::Star::filter;
  using ::Star::filtered;
  using ::Star::eraseWhere;
  using ::Star::sorted;
  using ::Star::sortByComputedValue;
  using ::Star::stableSortByComputedValue;
  using ::Star::copy;
  using ::Star::transformInto;
  using ::Star::transform;
  using ::Star::take;
  using ::Star::FinallyGuard;
  using ::Star::finally;
  using ::Star::FunctionTraits;
}
