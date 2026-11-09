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

inline constexpr std::size_t UuidSize = 16;

class Uuid;
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Uuid& uuid);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Uuid const& uuid);

class Uuid {
public:
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Uuid& uuid);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Uuid const& uuid);
  friend struct hash<Uuid>;
  friend std::expected<Uuid, StarException> DataStream::read<Uuid>();

  // An explicitly unfilled identity for wire staging, never identity generation.
  static Uuid nil() { return Uuid(NilTag{}); }

  explicit Uuid(RandomService& random);
  [[nodiscard]] static std::expected<Uuid, StarException> create(ByteArray const& bytes);
  [[nodiscard]] static std::expected<Uuid, StarException> create(String const& hex);

  std::span<char const> span() const;
  ByteArray bytes() const;
  String hex() const;

  bool operator==(Uuid const& u) const;
  bool operator!=(Uuid const& u) const;
  bool operator<(Uuid const& u) const;
  bool operator<=(Uuid const& u) const;
  bool operator>(Uuid const& u) const;
  bool operator>=(Uuid const& u) const;

private:
  struct NilTag {};
  explicit Uuid(NilTag) : m_data() {}

  Array<char, UuidSize> m_data;
};

template <>
struct hash<Uuid> {
  std::size_t operator()(Uuid const& u) const;
};


// Decode directly without generating a throwaway random identity.
template <>
[[nodiscard]] std::expected<Uuid, StarException> DataStream::read<Uuid>();

}

export module star.uuid;

export namespace Star {
  template <>
  struct hash<Uuid>;
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
  using ::Star::UuidSize;
  using ::Star::Uuid;
}
