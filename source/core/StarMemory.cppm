module;

import std;

namespace Star {

inline constexpr std::size_t npos = static_cast<std::size_t>(-1);

// Uninitialized engine storage. The owner always frees the requested capacity;
// a failed resize leaves both that capacity and its contents unchanged.
class MemoryAllocation {
public:
  [[nodiscard]] bool resize(std::size_t size) noexcept;
  std::size_t size() const noexcept { return m_bytes.get_deleter().capacity; }
  std::span<char> span() & noexcept { return {m_bytes.get(), size()}; }
  std::span<char const> span() const & noexcept { return {m_bytes.get(), size()}; }
  std::span<char const> span() const && = delete;

private:
  struct SizedFree {
    SizedFree() = default;
    SizedFree(SizedFree&& other) noexcept : capacity(std::exchange(other.capacity, 0)) {}
    SizedFree& operator=(SizedFree&& other) noexcept {
      capacity = std::exchange(other.capacity, 0);
      return *this;
    }
    void operator()(char* allocation) const noexcept;
    std::size_t capacity = 0;
  };
  std::unique_ptr<char[], SizedFree> m_bytes{nullptr, SizedFree{}};
};

// Lua owns this block across calls to its C allocator callback. When ptr is
// null, Lua may supply a type tag rather than a byte count in old_size.
void* reallocate_lua_storage(void* userdata, void* ptr, std::size_t old_size, std::size_t new_size) noexcept;

}

export module star.memory;

export namespace Star {
  using ::Star::npos;
  using ::Star::MemoryAllocation;
  using ::Star::reallocate_lua_storage;
}
