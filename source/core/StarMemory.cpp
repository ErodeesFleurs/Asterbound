#include <mimalloc.h>

import std;
import star.memory;

namespace Star {

void* malloc(std::size_t size) { return mi_malloc(size); }

void* realloc(void* ptr, std::size_t size) {
  // A migrated allocation may be this thread's first allocator operation.
  auto heap = mi_theap_get_default();
  return heap ? mi_theap_realloc(heap, ptr, size) : nullptr;
}

void free(void* ptr) { mi_free(ptr); }

void free(void* ptr, std::size_t size) { mi_free_size(ptr, size); }

}// namespace Star

namespace {

// Keep the engine's fail-fast policy without relying on mi_new's exception or
// new-handler configuration. The nothrow overloads deliberately return null.
void* requireAllocation(void* ptr) {
  if (!ptr)
    std::terminate();
  return ptr;
}

}// namespace

// Replacement operators must be non-inline and defined in global scope.
void* operator new(std::size_t size) { return requireAllocation(mi_malloc(size)); }

void* operator new[](std::size_t size) { return requireAllocation(mi_malloc(size)); }

void* operator new(std::size_t size, std::nothrow_t const&) noexcept { return mi_malloc(size); }

void* operator new[](std::size_t size, std::nothrow_t const&) noexcept { return mi_malloc(size); }

void* operator new(std::size_t size, std::align_val_t alignment) {
  return requireAllocation(mi_malloc_aligned(size, static_cast<std::size_t>(alignment)));
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
  return requireAllocation(mi_malloc_aligned(size, static_cast<std::size_t>(alignment)));
}

void* operator new(std::size_t size, std::align_val_t alignment, std::nothrow_t const&) noexcept {
  return mi_malloc_aligned(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment, std::nothrow_t const&) noexcept {
  return mi_malloc_aligned(size, static_cast<std::size_t>(alignment));
}

void operator delete(void* ptr) noexcept { mi_free(ptr); }

void operator delete[](void* ptr) noexcept { mi_free(ptr); }

void operator delete(void* ptr, std::nothrow_t const&) noexcept { mi_free(ptr); }

void operator delete[](void* ptr, std::nothrow_t const&) noexcept { mi_free(ptr); }

void operator delete(void* ptr, std::size_t size) noexcept { mi_free_size(ptr, size); }

void operator delete[](void* ptr, std::size_t size) noexcept { mi_free_size(ptr, size); }

void operator delete(void* ptr, std::align_val_t alignment) noexcept {
  mi_free_aligned(ptr, static_cast<std::size_t>(alignment));
}

void operator delete[](void* ptr, std::align_val_t alignment) noexcept {
  mi_free_aligned(ptr, static_cast<std::size_t>(alignment));
}

void operator delete(void* ptr, std::size_t size, std::align_val_t alignment) noexcept {
  mi_free_size_aligned(ptr, size, static_cast<std::size_t>(alignment));
}

void operator delete[](void* ptr, std::size_t size, std::align_val_t alignment) noexcept {
  mi_free_size_aligned(ptr, size, static_cast<std::size_t>(alignment));
}

void operator delete(void* ptr, std::align_val_t alignment, std::nothrow_t const&) noexcept {
  mi_free_aligned(ptr, static_cast<std::size_t>(alignment));
}

void operator delete[](void* ptr, std::align_val_t alignment, std::nothrow_t const&) noexcept {
  mi_free_aligned(ptr, static_cast<std::size_t>(alignment));
}
