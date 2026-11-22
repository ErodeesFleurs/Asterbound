export module star.benchmark_byte_fingerprint;

import std;

export namespace BenchmarkFixtures {

inline std::uint64_t fingerprint(std::span<std::byte const> bytes) {
  std::uint64_t hash = 14695981039346656037ull;
  for (auto byte : bytes)
    hash = (hash ^ std::to_integer<unsigned char>(byte)) * 1099511628211ull;
  return hash;
}

inline std::uint64_t fingerprint(std::span<char const> bytes) {
  return fingerprint(std::as_bytes(bytes));
}

}// namespace BenchmarkFixtures
