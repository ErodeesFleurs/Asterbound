module;
import std;

import star.format;

namespace Star {

template <typename>
inline constexpr bool is_unique_function_expected = false;

template <typename Value, typename Error>
inline constexpr bool is_unique_function_expected<std::expected<Value, Error>> = true;

template <typename Result>
concept unique_function_expected = is_unique_function_expected<std::remove_cvref_t<Result>>;

template <typename From, typename To>
concept safe_reference_result = std::is_reference_v<From> && std::is_reference_v<To>
    && std::is_convertible_v<std::add_pointer_t<std::remove_reference_t<From>>,
                             std::add_pointer_t<std::remove_reference_t<To>>>;

template <typename Callable, typename Result, typename... Args>
concept unique_function_target = std::is_constructible_v<std::decay_t<Callable>, Callable&&>
    && std::is_invocable_r_v<Result, std::decay_t<Callable>&, Args...>
    && (!std::is_void_v<Result>
        || !unique_function_expected<std::invoke_result_t<std::decay_t<Callable>&, Args...>>)
    && (!std::is_reference_v<Result>
        || safe_reference_result<std::invoke_result_t<std::decay_t<Callable>&, Args...>, Result>);

inline constexpr std::size_t unique_function_inline_size = 3 * sizeof(void*);

template <typename Value>
concept unique_function_inline_slot = std::is_object_v<Value> && sizeof(Value) <= unique_function_inline_size
    && alignof(Value) <= alignof(std::max_align_t) && std::is_nothrow_move_constructible_v<Value>;

template <typename Result>
struct UniqueFunctionInvocationResult {
  using type = std::expected<std::conditional_t<std::is_reference_v<Result>,
      std::reference_wrapper<std::remove_reference_t<Result>>, Result>, StarException>;
};

template <unique_function_expected Result>
  requires (!std::is_reference_v<Result>)
struct UniqueFunctionInvocationResult<Result> {
  using type = std::remove_cv_t<Result>;
  static_assert(std::same_as<typename type::error_type, StarException>,
      "UniqueFunction callbacks must use StarException errors");
};

template <typename Signature>
class UniqueFunction;

template <typename Result, typename... Args>
class UniqueFunction<Result(Args...)> {
  struct Storage {
    alignas(std::max_align_t) std::byte bytes[unique_function_inline_size];

    template <unique_function_inline_slot Slot>
    Slot& get() noexcept {
      return *std::launder(reinterpret_cast<Slot*>(bytes));
    }

    template <unique_function_inline_slot Slot, typename Value>
      requires std::is_constructible_v<Slot, Value&&>
    void construct(Value&& value) {
      std::construct_at(reinterpret_cast<Slot*>(bytes), std::forward<Value>(value));
    }
  } m_storage;

  struct Operations {
    Result (&invoke)(Storage&, Args&&...);
    void (&destroy)(Storage&) noexcept;
    void (&move)(Storage&, Storage&) noexcept;
  };
  // The optional lives in static storage, not in every wrapper. Each instance
  // borrows either a target's table or the single disengaged table.
  using OperationTable = std::optional<Operations>;
  static constexpr OperationTable empty_table{};
  std::reference_wrapper<OperationTable const> m_operations{empty_table};

  template <typename Function>
  using slot_type = std::conditional_t<unique_function_inline_slot<Function>, Function, std::unique_ptr<Function>>;

  template <typename Function>
  static Result invoke_target(Storage& storage, Args&&... args) {
    auto& slot = storage.template get<slot_type<Function>>();
    if constexpr (unique_function_inline_slot<Function>)
      return std::invoke_r<Result>(slot, std::forward<Args>(args)...);
    else
      return std::invoke_r<Result>(*slot, std::forward<Args>(args)...);
  }

  template <unique_function_inline_slot Slot>
  static void destroy_slot(Storage& storage) noexcept {
    std::destroy_at(std::addressof(storage.template get<Slot>()));
  }

  template <unique_function_inline_slot Slot>
  static void move_slot(Storage& destination, Storage& source) noexcept {
    auto& slot = source.template get<Slot>();
    destination.template construct<Slot>(std::move(slot));
    std::destroy_at(std::addressof(slot));
  }

  template <typename Function>
  static constexpr OperationTable operations = Operations{
    invoke_target<Function>, destroy_slot<slot_type<Function>>, move_slot<slot_type<Function>>
  };

  OperationTable const& take_operations() noexcept {
    return std::exchange(m_operations, std::cref(empty_table)).get();
  }

  void reset() noexcept {
    if (auto const& table = take_operations())
      table->destroy(m_storage);
  }

  void move_from(UniqueFunction& source) noexcept {
    // User move constructors and destructors can query or clear the source.
    // Publish the destination only after the old slot's lifetime has ended.
    if (auto const& table = source.take_operations()) {
      table->move(m_storage, source.m_storage);
      m_operations = table;
    }
  }

public:
  // Preserve an error-valued task's result instead of nesting expected values.
  // Ordinary reference results borrow the value; invocation failure remains explicit.
  using InvocationResult = typename UniqueFunctionInvocationResult<Result>::type;

  UniqueFunction() noexcept = default;
  UniqueFunction(std::nullptr_t) noexcept {}
  UniqueFunction(UniqueFunction const&) = delete;
  UniqueFunction& operator=(UniqueFunction const&) = delete;

  template <typename Callable>
    requires (!std::same_as<std::remove_cvref_t<Callable>, UniqueFunction>
        && unique_function_target<Callable, Result, Args...>)
  UniqueFunction(Callable&& callable) {
    using Function = std::decay_t<Callable>;
    if constexpr (std::is_pointer_v<std::remove_reference_t<Callable>> || std::is_member_pointer_v<Function>) {
      if (callable == nullptr)
        return;
    }
    if constexpr (unique_function_inline_slot<Function>)
      m_storage.template construct<Function>(std::forward<Callable>(callable));
    else
      m_storage.template construct<slot_type<Function>>(std::make_unique<Function>(std::forward<Callable>(callable)));
    m_operations = operations<Function>;
  }

  UniqueFunction(UniqueFunction&& source) noexcept { move_from(source); }

  UniqueFunction& operator=(UniqueFunction&& source) noexcept {
    if (this != std::addressof(source)) {
      reset();
      move_from(source);
    }
    return *this;
  }

  UniqueFunction& operator=(std::nullptr_t) noexcept { reset(); return *this; }
  ~UniqueFunction() { reset(); }
  explicit operator bool() const noexcept { return m_operations.get().has_value(); }

  [[nodiscard]] InvocationResult operator()(Args... args) {
    auto const& table = m_operations.get();
    if (!table)
      return std::unexpected(StarException("Cannot invoke an empty UniqueFunction"));
    if constexpr (std::is_void_v<Result>) {
      table->invoke(m_storage, std::forward<Args>(args)...);
      return {};
    } else if constexpr (std::is_reference_v<Result>) {
      auto&& result = table->invoke(m_storage, std::forward<Args>(args)...);
      return std::reference_wrapper<std::remove_reference_t<Result>>(result);
    } else {
      return table->invoke(m_storage, std::forward<Args>(args)...);
    }
  }
};

}

export module star.unique_function;
export namespace Star {
  using ::Star::UniqueFunction;
}
