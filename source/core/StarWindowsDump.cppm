module;
#include <windows.h>

namespace Star {

// A fresh thread writes the still-live faulting thread's native context.
bool try_write_minidump(DWORD thread_id, EXCEPTION_POINTERS& exception) noexcept;

}

export module star.windows_dump;

export namespace Star {
using ::Star::try_write_minidump;
}
