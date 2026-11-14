module;
import star.format;
import std;

namespace Star {

// Recognize only the project's checked result. Each algorithm defines its own
// ownership and invocation policy rather than flattening arbitrary wrappers.
template <typename T>
struct AlgorithmResultInfo {
  using value_type = T;
  static constexpr bool is_fallible = false;
};

template <typename T>
struct AlgorithmResultInfo<std::expected<T, StarException>> {
  using value_type = T;
  static constexpr bool is_fallible = true;
};

template <typename Function, typename... Args>
concept algorithm_invocable = std::invocable<Function, Args...>;

template <typename Function, typename... Args>
concept algorithm_predicate = algorithm_invocable<Function, Args...>
  && ((!AlgorithmResultInfo<std::remove_cvref_t<std::invoke_result_t<Function, Args...>>>::is_fallible
       && requires(Function&& function, Args&&... args) {
            static_cast<bool>(std::invoke(std::forward<Function>(function), std::forward<Args>(args)...));
          })
      || (AlgorithmResultInfo<std::remove_cvref_t<std::invoke_result_t<Function, Args...>>>::is_fallible
          && requires(Function&& function, Args&&... args) {
               static_cast<bool>(*std::invoke(std::forward<Function>(function), std::forward<Args>(args)...));
             }));

template <typename ToType>
struct Construct {
  template <typename... FromTypes>
    requires requires(FromTypes&&... args) { ToType(std::forward<FromTypes>(args)...); }
  constexpr ToType operator()(FromTypes&&... args) const
    noexcept(noexcept(ToType(std::forward<FromTypes>(args)...))) {
    return ToType(std::forward<FromTypes>(args)...);
  }
};

template <typename Target, typename Source>
concept algorithm_merge_target = std::ranges::input_range<Source const>
  && requires(Target& target, std::ranges::range_reference_t<Source const> entry) {
       target.insert(entry);
     }
  && requires(Target& target, std::ranges::range_reference_t<Source const> entry,
              typename AlgorithmResultInfo<
                std::remove_cvref_t<decltype(target.insert(entry))>>::value_type& inserted) {
       static_cast<bool>(inserted.second);
       inserted.first->second = entry.second;
     };

template <typename Target, typename Source>
  requires algorithm_merge_target<Target, Source>
[[nodiscard]] auto map_merge(Target& target, Source const& source, bool overwrite = false) {
  using insertion = std::remove_cvref_t<decltype(target.insert(*std::ranges::begin(source)))>;
  constexpr bool fallible = AlgorithmResultInfo<insertion>::is_fallible;
  using result_type = std::conditional_t<fallible, std::expected<bool, StarException>, bool>;
  bool disjoint = true;
  for (auto const& entry : source) {
    auto inserted = target.insert(entry);
    if constexpr (fallible) {
      if (!inserted)
        return result_type(std::unexpected(std::move(inserted.error())));
      if (!static_cast<bool>(inserted->second)) {
        disjoint = false;
        if (overwrite)
          inserted->first->second = entry.second;
      }
    } else if (!static_cast<bool>(inserted.second)) {
      disjoint = false;
      if (overwrite)
        inserted.first->second = entry.second;
    }
  }
  return result_type(disjoint);
}

template <typename Left, typename Right>
concept algorithm_comparable_maps = std::ranges::input_range<Left const>
  && requires(Left const& left, Right const& right, std::ranges::range_reference_t<Left const> entry) {
       static_cast<bool>(left.size() != right.size());
       right.find(entry.first);
       static_cast<bool>(right.find(entry.first) == right.end());
       static_cast<bool>(right.find(entry.first)->second == entry.second);
     };

template <typename Left, typename Right>
  requires algorithm_comparable_maps<Left, Right>
[[nodiscard]] bool maps_equal(Left const& left, Right const& right) {
  if constexpr (std::same_as<Left, Right>) {
    if (std::addressof(left) == std::addressof(right))
      return true;
  }
  if (left.size() != right.size())
    return false;
  for (auto const& entry : left) {
    auto found = right.find(entry.first);
    if (found == right.end() || !static_cast<bool>(found->second == entry.second))
      return false;
  }
  return true;
}

template <typename Container, typename Predicate>
concept algorithm_erase_target = std::ranges::input_range<Container>
  && algorithm_predicate<Predicate&, std::ranges::range_reference_t<Container>>
  && requires(Container& container, std::ranges::iterator_t<Container> position) {
       { container.erase(position) } -> std::same_as<std::ranges::iterator_t<Container>>;
     };

namespace AlgorithmDetail {

// Immediate erase is intentional: callbacks can observe each destruction and
// size change, and a checked failure leaves exactly the processed prefix.
template <bool Keep, typename Container, typename Predicate>
  requires algorithm_erase_target<Container, Predicate>
[[nodiscard]] auto erase_matching(Container& container, Predicate&& predicate) {
  using invocation = std::remove_cvref_t<
    std::invoke_result_t<Predicate&, std::ranges::range_reference_t<Container>>>;
  constexpr bool fallible = AlgorithmResultInfo<invocation>::is_fallible;
  using result_type = std::conditional_t<fallible, std::expected<void, StarException>, void>;
  auto position = std::ranges::begin(container);
  while (position != std::ranges::end(container)) {
    if constexpr (fallible && requires { predicate == nullptr; }) {
      if (predicate == nullptr)
        return result_type(std::unexpected(StarException("Cannot invoke an empty erase predicate")));
    }
    auto decision = std::invoke(predicate, *position);
    bool accepted;
    if constexpr (fallible) {
      if (!decision)
        return result_type(std::unexpected(std::move(decision.error())));
      accepted = static_cast<bool>(*decision);
    } else {
      accepted = static_cast<bool>(decision);
    }
    if (accepted != Keep)
      position = container.erase(position);
    else
      ++position;
  }
  if constexpr (fallible)
    return result_type();
}

}// namespace AlgorithmDetail

template <typename Container, typename Predicate>
  requires algorithm_erase_target<Container, Predicate>
[[nodiscard]] auto filter(Container& container, Predicate&& predicate) {
  return AlgorithmDetail::erase_matching<true>(container, std::forward<Predicate>(predicate));
}

template <typename Container, typename Predicate>
  requires algorithm_erase_target<Container, Predicate>
[[nodiscard]] auto erase_if(Container& container, Predicate&& predicate) {
  return AlgorithmDetail::erase_matching<false>(container, std::forward<Predicate>(predicate));
}


template <typename Container, typename Getter>
concept algorithm_key_source = requires(Container& container, std::size_t index) {
  { container.size() } -> std::convertible_to<std::size_t>;
  container[index];
} && algorithm_invocable<Getter&, decltype(std::declval<Container&>()[std::size_t{}])>;

namespace AlgorithmDetail {

// Indexed proxies must become logical values before sorting the key cache.
// Keep range metadata lazy: indexed-only containers need no iterators.
template <typename Container, typename Key>
struct SortKeyValue {
  using value_type = Key;
};

template <typename Container, typename Key>
  requires (!std::is_reference_v<decltype(std::declval<Container&>()[std::size_t{}])>)
  && std::same_as<Key, std::remove_cvref_t<decltype(std::declval<Container&>()[std::size_t{}])>>
  && requires { typename std::ranges::range_value_t<Container>; }
  && std::is_constructible_v<std::ranges::range_value_t<Container>, Key>
struct SortKeyValue<Container, Key> {
  using value_type = std::ranges::range_value_t<Container>;
};

}// namespace AlgorithmDetail

template <typename Container, typename Getter>
  requires algorithm_key_source<Container, Getter>
using algorithm_sort_key = typename AlgorithmDetail::SortKeyValue<Container,
  typename AlgorithmResultInfo<std::remove_cvref_t<std::invoke_result_t<
    Getter&, decltype(std::declval<Container&>()[std::size_t{}])>>>::value_type>::value_type;

template <typename Container, typename Getter>
concept algorithm_key_sortable = algorithm_key_source<Container, Getter>
  && std::is_object_v<algorithm_sort_key<Container, Getter>>
  && std::is_move_constructible_v<algorithm_sort_key<Container, Getter>>
  && std::is_move_assignable_v<algorithm_sort_key<Container, Getter>>
  && requires(Container& container, std::size_t index,
              algorithm_sort_key<Container, Getter> const& left,
              algorithm_sort_key<Container, Getter> const& right) {
       static_cast<bool>(left < right);
       std::ranges::swap(container[index], container[index]);
     }
  && (!AlgorithmResultInfo<std::remove_cvref_t<decltype(
       std::declval<algorithm_sort_key<Container, Getter> const&>()
       < std::declval<algorithm_sort_key<Container, Getter> const&>())>>::is_fallible);

namespace AlgorithmDetail {

// Snapshot every key once, before reordering any values. Keys remain owned even
// when getters return references; later getter calls can mutate their sources.
template <bool Stable, typename Container, typename Getter>
  requires algorithm_key_sortable<Container, Getter>
[[nodiscard]] auto sort_by_key(Container& container, Getter&& getter) {
  using invocation = std::remove_cvref_t<
    std::invoke_result_t<Getter&, decltype(container[std::size_t{}])>>;
  constexpr bool fallible = AlgorithmResultInfo<invocation>::is_fallible;
  using result_type = std::conditional_t<fallible, std::expected<void, StarException>, void>;
  using entry_type = std::pair<algorithm_sort_key<Container, Getter>, std::size_t>;
  std::size_t count = container.size();
  if (count <= 1) {
    if constexpr (fallible)
      return result_type();
    else
      return;
  }
  std::vector<entry_type> work;
  work.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    if constexpr (fallible) {
      if constexpr (requires { getter == nullptr; }) {
        if (getter == nullptr)
          return result_type(std::unexpected(StarException("Cannot invoke an empty sort key getter")));
      }
      auto key = std::invoke(getter, container[index]);
      if (!key)
        return result_type(std::unexpected(std::move(key.error())));
      work.emplace_back(std::move(*key), index);
    } else {
      work.emplace_back(std::invoke(getter, container[index]), index);
    }
  }
  auto compare = [](entry_type const& left, entry_type const& right) {
    return static_cast<bool>(left.first < right.first);
  };
  if constexpr (Stable)
    std::stable_sort(work.begin(), work.end(), compare);
  else
    std::sort(work.begin(), work.end(), compare);

  // Follow each source-index cycle, marking it in the existing index storage.
  // Fixed points never move; no second element buffer or default values.
  for (std::size_t start = 0; start < count; ++start) {
    auto current = start;
    auto next = work[current].second;
    while (next != start) {
      std::ranges::swap(container[current], container[next]);
      work[current].second = current;
      current = next;
      next = work[current].second;
    }
    work[current].second = current;
  }
  if constexpr (fallible)
    return result_type();
}

}// namespace AlgorithmDetail

template <typename Container, typename Getter>
  requires algorithm_key_sortable<Container, Getter>
[[nodiscard]] auto sort_by_key(Container& container, Getter&& getter) {
  return AlgorithmDetail::sort_by_key<false>(container, std::forward<Getter>(getter));
}

template <typename Container, typename Getter>
  requires algorithm_key_sortable<Container, Getter>
[[nodiscard]] auto stable_sort_by_key(Container& container, Getter&& getter) {
  return AlgorithmDetail::sort_by_key<true>(container, std::forward<Getter>(getter));
}

template <typename Range>
  requires std::ranges::input_range<Range>
using algorithm_source_reference =
  decltype(std::forward_like<Range>(*std::ranges::begin(std::declval<Range&>())));

template <typename Invocation>
using algorithm_insert_value = std::conditional_t<
  AlgorithmResultInfo<std::remove_cvref_t<Invocation>>::is_fallible,
  std::add_rvalue_reference_t<typename AlgorithmResultInfo<std::remove_cvref_t<Invocation>>::value_type>,
  Invocation>;

template <typename Container, typename Value>
concept algorithm_insert_target = requires(Container& container) {
  container.insert(container.end(), std::declval<Value>());
};

template <typename OutContainer, typename InContainer, typename Function>
concept algorithm_transform_target = std::ranges::input_range<InContainer>
  && algorithm_invocable<Function&, algorithm_source_reference<InContainer>>
  && algorithm_insert_target<OutContainer, algorithm_insert_value<
       std::invoke_result_t<Function&, algorithm_source_reference<InContainer>>>>;

// Source and destination must not alias. On failure, retain the inserted prefix
// and any source mutations/moves already performed by the callback.
template <typename OutContainer, typename InContainer, typename Function>
  requires algorithm_transform_target<OutContainer, InContainer, Function>
[[nodiscard]] auto transform_into(OutContainer& output, InContainer&& input, Function&& function) {
  using invocation = std::invoke_result_t<Function&, algorithm_source_reference<InContainer>>;
  using insertion = decltype(output.insert(output.end(), std::declval<algorithm_insert_value<invocation>>()));
  constexpr bool callback_fallible = AlgorithmResultInfo<std::remove_cvref_t<invocation>>::is_fallible;
  constexpr bool insert_fallible = AlgorithmResultInfo<std::remove_cvref_t<insertion>>::is_fallible;
  constexpr bool fallible = callback_fallible || insert_fallible;
  using result_type = std::conditional_t<fallible, std::expected<void, StarException>, void>;
  for (auto&& element : input) {
    if constexpr (fallible && requires { function == nullptr; }) {
      if (function == nullptr)
        return result_type(std::unexpected(StarException("Cannot invoke an empty transform callback")));
    }
    if constexpr (callback_fallible) {
      auto value = std::invoke(function, std::forward_like<InContainer>(element));
      if (!value)
        return result_type(std::unexpected(std::move(value.error())));
      if constexpr (insert_fallible) {
        auto inserted = output.insert(output.end(), std::move(*value));
        if (!inserted)
          return result_type(std::unexpected(std::move(inserted.error())));
      } else {
        output.insert(output.end(), std::move(*value));
      }
    } else {
      if constexpr (insert_fallible) {
        auto inserted = output.insert(output.end(), std::invoke(function, std::forward_like<InContainer>(element)));
        if (!inserted)
          return result_type(std::unexpected(std::move(inserted.error())));
      } else {
        output.insert(output.end(), std::invoke(function, std::forward_like<InContainer>(element)));
      }
    }
  }
  if constexpr (fallible)
    return result_type();
}

template <typename OutContainer, typename InContainer, typename Function>
  requires algorithm_transform_target<OutContainer, InContainer, Function>
  && requires { OutContainer(); }
[[nodiscard]] auto transform(InContainer&& input, Function&& function) {
  OutContainer output;
  if constexpr (std::ranges::sized_range<InContainer>) {
    if constexpr (requires {
                    { output.reserve(std::ranges::size(input)) } -> std::same_as<void>;
                  })
      output.reserve(std::ranges::size(input));
  }
  using status = decltype(Star::transform_into(output, std::forward<InContainer>(input), std::forward<Function>(function)));
  if constexpr (std::is_void_v<status>) {
    Star::transform_into(output, std::forward<InContainer>(input), std::forward<Function>(function));
    return output;
  } else {
    auto transformed = Star::transform_into(output, std::forward<InContainer>(input), std::forward<Function>(function));
    if (!transformed)
      return std::expected<OutContainer, StarException>(std::unexpected(std::move(transformed.error())));
    return std::expected<OutContainer, StarException>(std::move(output));
  }
}

// Moves the value out, then resets it to its default-constructed state.
// Unlike std::exchange, default construction follows the move. Keep this
// sequencing for replacements with observable construction side effects,
// including allocation; no-exceptions builds do not make those effects inert.
template <typename T>
  requires std::is_convertible_v<T&&, T> && requires(T& value) { value = T(); }
T take(T& value) {
  T result = std::move(value);
  value = T();
  return result;
}

template <typename Function>
concept algorithm_cleanup = std::is_object_v<Function> && algorithm_invocable<Function&>
  && std::same_as<std::invoke_result_t<Function&>, void>;

template <algorithm_cleanup Function>
class ScopeExit {
public:
  template <typename Callback>
    requires std::is_constructible_v<Function, Callback&&>
  explicit ScopeExit(Callback&& callback) noexcept(std::is_nothrow_constructible_v<Function, Callback&&>)
    : m_callback(std::forward<Callback>(callback)) {}

  ScopeExit(ScopeExit const&) = delete;
  ScopeExit& operator=(ScopeExit const&) = delete;
  ScopeExit& operator=(ScopeExit&&) = delete;

  ScopeExit(ScopeExit&& other) noexcept(std::is_nothrow_constructible_v<Function, Function&&>)
    requires std::is_constructible_v<Function, Function&&>
    : m_callback(std::move(other.m_callback)), m_active(std::exchange(other.m_active, false)) {}

  ~ScopeExit() noexcept {
    if (m_active)
      std::invoke(m_callback);
  }

  void release() noexcept {
    m_active = false;
  }

private:
  Function m_callback;
  bool m_active = true;
};

template <typename Function>
  requires requires(Function&& function) { ScopeExit<std::decay_t<Function>>(std::forward<Function>(function)); }
[[nodiscard]] auto make_scope_exit(Function&& function) {
  return ScopeExit<std::decay_t<Function>>(std::forward<Function>(function));
}

namespace AlgorithmDetail {

template <typename T>
struct FunctionTraits {};

template <typename T>
  requires requires { &T::operator(); }
struct FunctionTraits<T> : FunctionTraits<decltype(&T::operator())> {};

template <typename Return, typename... Args>
struct FunctionSignature {
  using result_type = Return;
  using argument_types = std::tuple<Args...>;
  static constexpr std::size_t arity = sizeof...(Args);

  template <std::size_t Index>
    requires(Index < arity)
  using argument_type = std::tuple_element_t<Index, argument_types>;
};

template <typename Return, typename... Args, bool Noexcept>
struct FunctionTraits<Return(Args...) noexcept(Noexcept)> : FunctionSignature<Return, Args...> {};

template <typename Return, typename... Args, bool Noexcept>
struct FunctionTraits<Return (*)(Args...) noexcept(Noexcept)> : FunctionSignature<Return, Args...> {};

template <typename Return, typename... Args>
struct FunctionTraits<std::function<Return(Args...)>> : FunctionSignature<Return, Args...> {};

template <typename Owner, typename Return, typename... Args, bool Noexcept>
struct FunctionTraits<Return (Owner::*)(Args...) noexcept(Noexcept)> : FunctionSignature<Return, Args...> {
  using owner_type = Owner&;
};

template <typename Owner, typename Return, typename... Args, bool Noexcept>
struct FunctionTraits<Return (Owner::*)(Args...) const noexcept(Noexcept)> : FunctionSignature<Return, Args...> {
  using owner_type = Owner const&;
};

template <typename Owner, typename Return, typename... Args, bool Noexcept>
struct FunctionTraits<Return (Owner::*)(Args...) volatile noexcept(Noexcept)> : FunctionSignature<Return, Args...> {
  using owner_type = Owner volatile&;
};

template <typename Owner, typename Return, typename... Args, bool Noexcept>
struct FunctionTraits<Return (Owner::*)(Args...) const volatile noexcept(Noexcept)> : FunctionSignature<Return, Args...> {
  using owner_type = Owner const volatile&;
};

template <typename Owner, typename Return, typename... Args, bool Noexcept>
struct FunctionTraits<Return (Owner::*)(Args...) & noexcept(Noexcept)> : FunctionSignature<Return, Args...> {
  using owner_type = Owner&;
};

template <typename Owner, typename Return, typename... Args, bool Noexcept>
struct FunctionTraits<Return (Owner::*)(Args...) const & noexcept(Noexcept)> : FunctionSignature<Return, Args...> {
  using owner_type = Owner const&;
};

template <typename Owner, typename Return, typename... Args, bool Noexcept>
struct FunctionTraits<Return (Owner::*)(Args...) volatile & noexcept(Noexcept)> : FunctionSignature<Return, Args...> {
  using owner_type = Owner volatile&;
};

template <typename Owner, typename Return, typename... Args, bool Noexcept>
struct FunctionTraits<Return (Owner::*)(Args...) const volatile & noexcept(Noexcept)> : FunctionSignature<Return, Args...> {
  using owner_type = Owner const volatile&;
};

template <typename Owner, typename Return, typename... Args, bool Noexcept>
struct FunctionTraits<Return (Owner::*)(Args...) && noexcept(Noexcept)> : FunctionSignature<Return, Args...> {
  using owner_type = Owner&&;
};

template <typename Owner, typename Return, typename... Args, bool Noexcept>
struct FunctionTraits<Return (Owner::*)(Args...) const && noexcept(Noexcept)> : FunctionSignature<Return, Args...> {
  using owner_type = Owner const&&;
};

template <typename Owner, typename Return, typename... Args, bool Noexcept>
struct FunctionTraits<Return (Owner::*)(Args...) volatile && noexcept(Noexcept)> : FunctionSignature<Return, Args...> {
  using owner_type = Owner volatile&&;
};

template <typename Owner, typename Return, typename... Args, bool Noexcept>
struct FunctionTraits<Return (Owner::*)(Args...) const volatile && noexcept(Noexcept)> : FunctionSignature<Return, Args...> {
  using owner_type = Owner const volatile&&;
};

}// namespace AlgorithmDetail

template <typename Function>
struct FunctionTraits : AlgorithmDetail::FunctionTraits<std::remove_cvref_t<Function>> {};

}

export module star.algorithm;

export namespace Star {
  using ::Star::algorithm_cleanup;
  using ::Star::algorithm_comparable_maps;
  using ::Star::algorithm_erase_target;
  using ::Star::algorithm_insert_target;
  using ::Star::algorithm_invocable;
  using ::Star::algorithm_key_sortable;
  using ::Star::algorithm_merge_target;
  using ::Star::algorithm_predicate;
  using ::Star::AlgorithmResultInfo;
  using ::Star::algorithm_transform_target;
  using ::Star::Construct;
  using ::Star::erase_if;
  using ::Star::filter;
  using ::Star::FunctionTraits;
  using ::Star::make_scope_exit;
  using ::Star::map_merge;
  using ::Star::maps_equal;
  using ::Star::ScopeExit;
  using ::Star::sort_by_key;
  using ::Star::stable_sort_by_key;
  using ::Star::take;
  using ::Star::transform;
  using ::Star::transform_into;
}
