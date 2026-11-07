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

// Generate cryptographically secure random numbers for usage in password salts
// and such using OS facilities
[[nodiscard]] std::expected<ByteArray, StarException> secureRandomBytes(std::size_t size);

}

namespace Star {

std::expected<ByteArray, StarException> secureRandomBytes(std::size_t size) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  HCRYPTPROV context = 0;

  if (!CryptAcquireContext(&context, 0, MS_DEF_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    return std::unexpected(StarException("Could not acquire secure random source."));
  auto res = ByteArray(size, '\0');
  auto success = CryptGenRandom(context, size, (PBYTE)res.ptr());
  CryptReleaseContext(context, 0);

  if (!success)
    return std::unexpected(StarException("Could not read random bytes from source."));

  return res;
#else
  auto file = File::open("/dev/urandom", IOMode::Read);
  if (!file)
    return std::unexpected(std::move(file.error()));
  return (*file)->read_bytes(size);
#endif
}

}
