module;
import std;
import star.format;
import star.hash;
import star.casting;
import star.type_identity;

namespace Star {

class RefCounter;
void ref_ptr_inc_ref(RefCounter const& value) noexcept;
void ref_ptr_dec_ref(RefCounter const& value) noexcept;

template <typename T>
concept intrusive_value = std::is_class_v<T> && !std::is_volatile_v<T>;

template <typename T>
concept intrusive_counted = intrusive_value<T> && requires(T& value) {
  { ref_ptr_inc_ref(value) } noexcept -> std::same_as<void>;
  { ref_ptr_dec_ref(value) } noexcept -> std::same_as<void>;
};

template <typename From, typename To>
concept intrusive_convertible = intrusive_value<From>
  && std::convertible_to<std::add_pointer_t<From>, std::add_pointer_t<To>> && intrusive_counted<To>;

template <typename T, typename... Args>
concept intrusive_constructible =
  intrusive_counted<T> && requires(Args&&... args) { new T(std::forward<Args>(args)...); };

template <intrusive_value T> struct IntrusiveDeleter {
  void operator()(T* value) const noexcept { ref_ptr_dec_ref(*value); }
};

template <intrusive_value T> class DetachedRef;

template <typename Target, typename Source>
concept intrusive_castable = intrusive_value<Target> && intrusive_value<Source>
  && capability_castable<cast_result_t<Target, Source>, Source> && intrusive_counted<cast_result_t<Target, Source>>;

// Non-atomic intrusive counts, represented by a standard unique owner/deleter.
// Constructing from a reference retains; detach/adopt transfer an existing count.
template <intrusive_value T> class RefPtr {
public:
  using element_type = T;

  RefPtr() noexcept = default;
  template <typename U>
    requires (std::is_lvalue_reference_v<U> && intrusive_convertible<std::remove_reference_t<U>, T>)
  explicit RefPtr(U&& value) noexcept : m_owner(std::addressof(value)) { ref_ptr_inc_ref(*m_owner); }

  RefPtr(RefPtr const& other) noexcept { assign_retained(other); }
  RefPtr(RefPtr&&) noexcept = default;
  template <intrusive_value U>
    requires (!std::same_as<U, T> && intrusive_convertible<U, T>)
  RefPtr(RefPtr<U> const& other) noexcept { assign_retained(other); }
  template <intrusive_value U>
    requires (!std::same_as<U, T> && intrusive_convertible<U, T>)
  RefPtr(RefPtr<U>&& other) noexcept : m_owner(other.m_owner.release()) {}

  RefPtr& operator=(RefPtr const& other) noexcept {
    assign_retained(other);
    return *this;
  }
  RefPtr& operator=(RefPtr&& other) noexcept {
    // Consume the source before releasing the old object: it can contain other.
    m_owner.reset(other.m_owner.release());
    return *this;
  }
  template <intrusive_value U>
    requires (!std::same_as<U, T> && intrusive_convertible<U, T>)
  RefPtr& operator=(RefPtr<U> const& other) noexcept {
    assign_retained(other);
    return *this;
  }
  template <intrusive_value U>
    requires (!std::same_as<U, T> && intrusive_convertible<U, T>)
  RefPtr& operator=(RefPtr<U>&& other) noexcept {
    m_owner.reset(other.m_owner.release());
    return *this;
  }

  void reset() noexcept { m_owner.reset(); }
  void reset(T& value) noexcept requires intrusive_counted<T> {
    if (m_owner.get() != std::addressof(value)) {
      ref_ptr_inc_ref(value);
      m_owner.reset(std::addressof(value));
    }
  }
  void reset(T&&) = delete;

  [[nodiscard]] DetachedRef<T> detach() noexcept { return DetachedRef<T>(std::move(*this)); }
  template <intrusive_value U>
    requires intrusive_convertible<U, T>
  void adopt(DetachedRef<U>&& detached) noexcept { *this = std::move(detached.m_owner); }

  void swap(RefPtr& other) noexcept { m_owner.swap(other.m_owner); }
  T& operator*() const noexcept { return *m_owner; }
  auto const& operator->() const noexcept { return m_owner; }
  explicit operator bool() const noexcept { return static_cast<bool>(m_owner); }

private:
  template <intrusive_value> friend class RefPtr;
  template <intrusive_value Target, intrusive_value Source>
    requires intrusive_castable<Target, Source>
  friend RefPtr<cast_result_t<Target, Source>> as(RefPtr<Source>&& owner) noexcept;

  template <intrusive_value U>
  void assign_retained(RefPtr<U> const& other) noexcept {
    if (other)
      reset(*other);
    else
      reset();
  }

  std::unique_ptr<T, IntrusiveDeleter<T>> m_owner;
};

template <intrusive_value T> class DetachedRef {
public:
  DetachedRef(DetachedRef&&) noexcept = default;
  DetachedRef& operator=(DetachedRef&&) noexcept = default;
  T& operator*() const noexcept { return *m_owner; }
  explicit operator bool() const noexcept { return static_cast<bool>(m_owner); }

private:
  template <intrusive_value> friend class RefPtr;
  explicit DetachedRef(RefPtr<T>&& owner) noexcept : m_owner(std::move(owner)) {}
  RefPtr<T> m_owner;
};

template <typename T, typename U>
concept intrusive_comparable = intrusive_value<T> && intrusive_value<U> && requires(T& left, U& right) {
  { std::addressof(left) == std::addressof(right) } -> std::same_as<bool>;
};

template <intrusive_value T, intrusive_value U>
  requires intrusive_comparable<T, U>
bool operator==(RefPtr<T> const& left, RefPtr<U> const& right) noexcept {
  return std::to_address(left) == std::to_address(right);
}
template <intrusive_value T, intrusive_value U>
  requires requires(T& left, U& right) { std::less<>{}(std::addressof(left), std::addressof(right)); }
bool operator<(RefPtr<T> const& left, RefPtr<U> const& right) noexcept {
  return std::less<>{}(std::to_address(left), std::to_address(right));
}
template <intrusive_value T> void swap(RefPtr<T>& left, RefPtr<T>& right) noexcept { left.swap(right); }

template <cast_target Target, intrusive_value Source>
  requires capability_castable<cast_result_t<Target, Source>, Source>
bool is(RefPtr<Source> const& owner) noexcept {
  if constexpr (std::is_void_v<Target>)
    return static_cast<bool>(owner);
  else
    return owner && as_ref<Target>(*owner).has_value();
}

template <intrusive_value Target, intrusive_value Source>
  requires intrusive_castable<Target, Source>
RefPtr<cast_result_t<Target, Source>> as(RefPtr<Source> const& owner) noexcept {
  if (owner)
    if (auto adjusted = as_ref<Target>(*owner))
      return RefPtr<cast_result_t<Target, Source>>(adjusted->get());
  return {};
}

template <intrusive_value Target, intrusive_value Source>
  requires intrusive_castable<Target, Source>
RefPtr<cast_result_t<Target, Source>> as(RefPtr<Source>&& owner) noexcept {
  if (!owner)
    return {};
  if (auto adjusted = as_ref<Target>(*owner)) {
    RefPtr<cast_result_t<Target, Source>> result;
    (void)owner.m_owner.release();
    result.m_owner.reset(std::addressof(adjusted->get()));
    return result;
  }
  return {};
}

template <intrusive_value T, typename... Args>
  requires intrusive_constructible<T, Args...>
RefPtr<T> make_ref(Args&&... args) {
  return RefPtr<T>(*new T(std::forward<Args>(args)...));
}

template <intrusive_value T> struct ValueHash<RefPtr<T>> {
  std::size_t operator()(RefPtr<T> const& value) const noexcept {
    return Star::ValueHash<decltype(std::to_address(value))>{}(std::to_address(value));
  }
};

class RefCounter {
public:
  RefCounter(RefCounter const&) = delete;
  RefCounter(RefCounter&&) = delete;
  RefCounter& operator=(RefCounter const&) = delete;
  RefCounter& operator=(RefCounter&&) = delete;

  friend void ref_ptr_inc_ref(RefCounter const& value) noexcept;
  friend void ref_ptr_dec_ref(RefCounter const& value) noexcept;

  virtual void query_type(TypeQuery& query) const noexcept { query_type_of<RefCounter>(*this, query); }
  virtual std::string_view dynamic_type_name() const noexcept { return type_name<RefCounter>(); }

protected:
  RefCounter() = default;
  virtual ~RefCounter() = default;

private:
  mutable std::size_t m_ref_count = 0;
};

inline void ref_ptr_inc_ref(RefCounter const& value) noexcept { ++value.m_ref_count; }
inline void ref_ptr_dec_ref(RefCounter const& value) noexcept {
  if (--value.m_ref_count == 0)
    delete std::addressof(value);
}
}

export module star.ref_ptr;

export namespace Star {
template <intrusive_value T> struct ValueHash<RefPtr<T>>;
}

export namespace Star {
using ::Star::DetachedRef;
using ::Star::intrusive_castable;
using ::Star::intrusive_constructible;
using ::Star::intrusive_convertible;
using ::Star::intrusive_counted;
using ::Star::intrusive_value;
using ::Star::RefCounter;
using ::Star::RefPtr;
using ::Star::operator==;
using ::Star::operator<;
using ::Star::as;
using ::Star::ValueHash;
using ::Star::is;
using ::Star::make_ref;
using ::Star::ref_ptr_dec_ref;
using ::Star::ref_ptr_inc_ref;
using ::Star::swap;
}
