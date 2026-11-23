#include <mimalloc.h>

import std;
import star.memory;

namespace Star {

static void* reallocate(void* ptr, std::size_t size) noexcept {
  // A migrated allocation may be this thread's first allocator operation.
  auto heap = mi_theap_get_default();
  return heap ? mi_theap_realloc(heap, ptr, size) : nullptr;
}

void MemoryAllocation::SizedFree::operator()(char* allocation) const noexcept {
  mi_free_size(allocation, capacity);
}

bool MemoryAllocation::resize(std::size_t size) noexcept {
  if (size == this->size())
    return true;
  if (size == 0) {
    m_bytes.reset();
    m_bytes.get_deleter().capacity = 0;
    return true;
  }
  auto previous = m_bytes.release();
  auto allocated = previous ? reallocate(previous, size) : mi_malloc(size);
  if (!allocated) {
    m_bytes.reset(previous);
    return false;
  }
  m_bytes.get_deleter().capacity = size;
  m_bytes.reset(static_cast<char*>(allocated));
  return true;
}

void* reallocate_lua_storage(void*, void* ptr, std::size_t old_size, std::size_t new_size) noexcept {
  if (new_size != 0)
    return reallocate(ptr, new_size);
  mi_free_size(ptr, old_size);
  return nullptr;
}

}// namespace Star

namespace {

// Keep the engine's fail-fast policy without relying on mi_new's exception or
// new-handler configuration. The nothrow overloads deliberately return null.
void* require_allocation(void* ptr) noexcept {
  if (!ptr)
    std::terminate();
  return ptr;
}

}// namespace

// Replacement operators must be non-inline and defined in global scope.
void* operator new(std::size_t size) { return require_allocation(mi_malloc(size)); }

void* operator new[](std::size_t size) { return require_allocation(mi_malloc(size)); }

void* operator new(std::size_t size, std::nothrow_t const&) noexcept { return mi_malloc(size); }

void* operator new[](std::size_t size, std::nothrow_t const&) noexcept { return mi_malloc(size); }

void* operator new(std::size_t size, std::align_val_t alignment) {
  return require_allocation(mi_malloc_aligned(size, static_cast<std::size_t>(alignment)));
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
  return require_allocation(mi_malloc_aligned(size, static_cast<std::size_t>(alignment)));
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
