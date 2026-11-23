module;
import std;
import star.string;
import star.format;
import star.hash;
import star.byte_array;

import star.array;
import star.data_stream;
import star.random;

namespace Star {

struct UuidExceptionTag {
  static constexpr std::string_view name() { return "UuidException"; }
};
using UuidException = StarError<UuidExceptionTag, StarException>;

inline constexpr std::size_t uuid_size = 16;

class Uuid;
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Uuid& uuid);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Uuid const& uuid);


template <>
struct HasStreamReadFactory<Uuid> : std::true_type {};
class Uuid {
public:
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream&, Uuid&);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream&, Uuid const&);
  friend std::expected<Uuid, StarException> DataStream::read<Uuid>();

  // An explicitly unfilled identity for wire staging, never identity generation.
  static Uuid nil() { return Uuid(NilTag{}); }

  explicit Uuid(RandomService& random);
  [[nodiscard]] static std::expected<Uuid, StarException> from_bytes(std::span<char const> bytes);
  [[nodiscard]] static std::expected<Uuid, StarException> from_hex(std::string_view hex);

  std::span<char const, uuid_size> span() const & { return std::span<char const, uuid_size>(storage); }
  std::span<char const, uuid_size> span() const && = delete;
  String hex() const;

  bool operator==(Uuid const&) const = default;
  std::weak_ordering operator<=>(Uuid const&) const = default;

private:
  struct NilTag {};
  explicit Uuid(NilTag) : storage() {}

  Array<char, uuid_size> storage;
};

template <>
struct ValueHash<Uuid> {
  std::size_t operator()(Uuid const& u) const;
};


// Decode directly without generating a throwaway random identity.
template <>
[[nodiscard]] std::expected<Uuid, StarException> DataStream::read<Uuid>();

}

export module star.uuid;

export namespace Star {
  template <>
  struct HasStreamReadFactory<Uuid>;
  template <>
  struct ValueHash<Uuid>;
}

export extern "C++" {
namespace Star {
  template <>
  [[nodiscard]] std::expected<Uuid, StarException> DataStream::read<Uuid>();
}
}


export namespace Star {
  using ::Star::UuidExceptionTag;
  using ::Star::UuidException;
  using ::Star::uuid_size;
  using ::Star::Uuid;
}
