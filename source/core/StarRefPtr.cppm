module;
import std;
import star.format;
import star.hash;
import star.casting;
import star.type_identity;

namespace Star {

class RefCounter;
void ref_ptr_inc_ref(RefCounter const* value) noexcept;
void ref_ptr_dec_ref(RefCounter const* value) noexcept;

template <typename T>
concept intrusive_value = std::is_class_v<T> && !std::is_volatile_v<T>;

template <typename T>
concept intrusive_counted = intrusive_value<T> && requires(T* value) {
  { ref_ptr_inc_ref(value) } noexcept;
  { ref_ptr_dec_ref(value) } noexcept;
};

template <typename From, typename To>
concept intrusive_convertible = intrusive_value<From> && intrusive_value<To> && std::convertible_to<From*, To*>;

template <typename T, typename... Args>
concept intrusive_constructible =
  intrusive_counted<T> && requires(Args&&... args) { new T(std::forward<Args>(args)...); };

template <intrusive_value T> struct intrusive_deleter {
  void operator()(T* value) const noexcept { ref_ptr_dec_ref(value); }
};

// Non-atomic intrusive counts, represented by a standard unique owner/deleter.
// Object addresses enter/leave only through standard pointer interfaces.
template <intrusive_value T> class RefPtr {
public:
  using element_type = T;

  RefPtr() noexcept = default;
  explicit RefPtr(T* value, bool add_ref = true) noexcept
    requires intrusive_counted<T>
  {
    reset(value, add_ref);
  }

  RefPtr(RefPtr const& other) noexcept : RefPtr(other.get()) {}
  RefPtr(RefPtr&&) noexcept = default;
  template <intrusive_value U>
    requires intrusive_convertible<U, T>
  RefPtr(RefPtr<U> const& other) noexcept : RefPtr(other.get()) {}
  template <intrusive_value U>
    requires intrusive_convertible<U, T>
  RefPtr(RefPtr<U>&& other) noexcept : m_owner(other.release()) {}

  RefPtr& operator=(RefPtr const& other) noexcept {
    reset(other.get());
    return *this;
  }
  RefPtr& operator=(RefPtr&& other) noexcept {
    if (this != &other)
      m_owner = std::move(other.m_owner);
    return *this;
  }
  template <intrusive_value U>
    requires intrusive_convertible<U, T>
  RefPtr& operator=(RefPtr<U> const& other) noexcept {
    reset(other.get());
    return *this;
  }
  template <intrusive_value U>
    requires intrusive_convertible<U, T>
  RefPtr& operator=(RefPtr<U>&& other) noexcept {
    m_owner.reset(other.release());
    return *this;
  }

  void reset() noexcept { m_owner.reset(); }
  void reset(T* value, bool add_ref = true) noexcept
    requires intrusive_counted<T>
  {
    if (m_owner.get() == value)
      return;
    if (value && add_ref)
      ref_ptr_inc_ref(value);
    m_owner.reset(value);
  }
  [[nodiscard]] T* release() noexcept { return m_owner.release(); }
  void swap(RefPtr& other) noexcept { m_owner.swap(other.m_owner); }
  T& operator*() const noexcept { return *m_owner; }
  T* operator->() const noexcept { return m_owner.get(); }
  T* get() const noexcept { return m_owner.get(); }
  explicit operator bool() const noexcept { return static_cast<bool>(m_owner); }

private:
  std::unique_ptr<T, intrusive_deleter<T>> m_owner;
};

template <typename T, typename U>
concept intrusive_comparable = intrusive_value<T> && intrusive_value<U> && requires(T* left, U* right) {
  { left == right } -> std::same_as<bool>;
};

template <intrusive_value T, intrusive_value U>
  requires intrusive_comparable<T, U>
bool operator==(RefPtr<T> const& left, RefPtr<U> const& right) noexcept {
  return left.get() == right.get();
}
template <intrusive_value T> bool operator==(RefPtr<T> const& left, T* right) noexcept { return left.get() == right; }
template <intrusive_value T> bool operator==(T* left, RefPtr<T> const& right) noexcept { return left == right.get(); }
template <intrusive_value T, intrusive_value U>
  requires requires(T* left, U* right) { std::less<>{}(left, right); }
bool operator<(RefPtr<T> const& left, RefPtr<U> const& right) noexcept {
  return std::less<>{}(left.get(), right.get());
}
template <intrusive_value T> void swap(RefPtr<T>& left, RefPtr<T>& right) noexcept { left.swap(right); }

template <cast_target Target, intrusive_value Source>
  requires capability_castable<cast_result_t<Target, Source>, Source>
bool is(RefPtr<Source> const& owner) noexcept {
  return is<Target>(owner.get());
}

template <intrusive_value Target, intrusive_value Source>
  requires capability_castable<cast_result_t<Target, Source>, Source>
RefPtr<cast_result_t<Target, Source>> as(RefPtr<Source> const& owner) noexcept {
  return RefPtr<cast_result_t<Target, Source>>(as<Target>(owner.get()));
}

template <intrusive_value Target, intrusive_value Source>
  requires capability_castable<cast_result_t<Target, Source>, Source>
RefPtr<cast_result_t<Target, Source>> as(RefPtr<Source>&& owner) noexcept {
  if (auto adjusted = as<Target>(owner.get())) {
    RefPtr<cast_result_t<Target, Source>> result(adjusted, false);
    (void)owner.release();
    return result;
  }
  return {};
}

template <intrusive_value T, typename... Args>
  requires intrusive_constructible<T, Args...>
RefPtr<T> make_ref(Args&&... args) {
  return RefPtr<T>(new T(std::forward<Args>(args)...));
}

template <intrusive_value T> struct hash<RefPtr<T>> {
  std::size_t operator()(RefPtr<T> const& value) const noexcept { return hash<T*>{}(value.get()); }
};

class RefCounter {
public:
  friend void ref_ptr_inc_ref(RefCounter const* value) noexcept;
  friend void ref_ptr_dec_ref(RefCounter const* value) noexcept;

  virtual void const* query_type(TypeKey key) const noexcept { return query_type_of<RefCounter>(this, key); }
  virtual std::string_view dynamic_type_name() const noexcept { return type_name<RefCounter>(); }

protected:
  RefCounter() = default;
  virtual ~RefCounter() = default;

private:
  mutable std::size_t m_ref_count = 0;
};

inline void ref_ptr_inc_ref(RefCounter const* value) noexcept { ++value->m_ref_count; }
inline void ref_ptr_dec_ref(RefCounter const* value) noexcept {
  if (--value->m_ref_count == 0)
    delete value;
}
}

export module star.ref_ptr;

export namespace Star {
template <intrusive_value T> struct hash<RefPtr<T>>;
}

export namespace Star {
using ::Star::intrusive_constructible;
using ::Star::intrusive_convertible;
using ::Star::intrusive_counted;
using ::Star::intrusive_value;
using ::Star::RefCounter;
using ::Star::RefPtr;
using ::Star::operator==;
using ::Star::operator<;
using ::Star::as;
using ::Star::hash;
using ::Star::is;
using ::Star::make_ref;
using ::Star::ref_ptr_dec_ref;
using ::Star::ref_ptr_inc_ref;
using ::Star::swap;
}
