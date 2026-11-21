module;

import std;

namespace Star {

// Perform some action every X ticks.  Setting the tick count to 0 means never
// perform the action, 1 performs the action every call, 2 performs the action
// every other call, and so forth.
class Periodic {
public:
  constexpr Periodic(unsigned step_count = 1) noexcept : m_step_count(step_count) {}

  [[nodiscard]] constexpr unsigned step_count() const noexcept {
    return m_step_count;
  }

  constexpr void set_step_count(unsigned step_count) noexcept {
    m_step_count = step_count;
    m_remaining = step_count ? std::min(m_remaining, step_count - 1) : 0;
  }

  // Will the next tick() return true?
  [[nodiscard]] constexpr bool ready() const noexcept {
    return m_step_count != 0 && m_remaining == 0;
  }

  constexpr bool tick() noexcept {
    if (m_remaining != 0) {
      --m_remaining;
      return false;
    }
    if (m_step_count == 0)
      return false;
    m_remaining = m_step_count - 1;
    return true;
  }

  template <typename Function>
    requires std::invocable<Function&>
  constexpr void tick(Function&& function) noexcept(std::is_nothrow_invocable_v<Function&>) {
    if (tick())
      std::invoke(function);
  }

private:
  unsigned m_remaining = 0;
  unsigned m_step_count;
};

}

export module star.periodic;

export namespace Star {
  using ::Star::Periodic;
}
