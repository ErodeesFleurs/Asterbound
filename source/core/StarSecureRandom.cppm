module;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
#include <windows.h>
#include <wincrypt.h>
#endif
import std;
import star.format;

import star.byte_array;

#ifndef STAR_SYSTEM_FAMILY_WINDOWS
import star.file;
import star.io_device;
#endif

export module star.secure_random;

export namespace Star {

// OS CSPRNG only; failures are returned without falling back to a PRNG.
// On failure the destination may contain a successfully filled prefix.
[[nodiscard]] std::expected<void, StarException> secure_random_fill(std::span<char> bytes);
[[nodiscard]] std::expected<ByteArray, StarException> secure_random_bytes(std::size_t size);

}

namespace Star {

std::expected<void, StarException> secure_random_fill(std::span<char> bytes) {
  if (bytes.empty())
    return {};
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  // CryptoAPI requires native handle and byte-pointer adapters. Keep these
  // C-ABI exceptions local; the public interface borrows only a bounded span.
  struct Context {
    HCRYPTPROV handle = 0;
    Context() = default;
    Context(Context const&) = delete;
    Context& operator=(Context const&) = delete;
    ~Context() {
      if (handle)
        CryptReleaseContext(handle, 0);
    }
  } context;
  if (!CryptAcquireContext(&context.handle, nullptr, MS_DEF_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
    auto error = GetLastError();
    return std::unexpected(StarException::format("Could not acquire secure random source (Windows error {}).", error));
  }
  while (!bytes.empty()) {
    auto count = static_cast<DWORD>(std::min(bytes.size(), static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
    if (!CryptGenRandom(context.handle, count, reinterpret_cast<BYTE*>(bytes.data()))) {
      auto error = GetLastError();
      return std::unexpected(StarException::format("Could not read secure random bytes (Windows error {}).", error));
    }
    bytes = bytes.subspan(count);
  }
#else
  auto file = File::open("/dev/urandom", IOMode::Read);
  if (!file)
    return std::unexpected(std::move(file.error()));
  // File::read already retries EINTR and bounds native read lengths.
  while (!bytes.empty()) {
    auto count = (*file)->read(bytes);
    if (!count)
      return std::unexpected(std::move(count.error()));
    if (*count == 0)
      return std::unexpected(StarException("Unexpected end of secure random source."));
    bytes = bytes.subspan(*count);
  }
#endif
  return {};
}

std::expected<ByteArray, StarException> secure_random_bytes(std::size_t size) {
  ByteArray bytes;
  bytes.resize(size);
  auto filled = secure_random_fill(bytes.span());
  if (!filled)
    return std::unexpected(std::move(filled.error()));
  return bytes;
}

}
