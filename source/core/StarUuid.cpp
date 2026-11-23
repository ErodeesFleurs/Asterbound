import std;
import star.hash;
import star.data_stream;
import star.uuid;
import star.random;
import star.format;
import star.string;
import star.byte_array;
import star.encode;

namespace Star {

Uuid::Uuid(RandomService& random) {
  random.fill_bytes(storage);
}

std::expected<Uuid, StarException> Uuid::from_bytes(std::span<char const> bytes) {
  if (bytes.size() != uuid_size)
    return std::unexpected(UuidException(Star::format("Size mismatch in reading Uuid from bytes: {} vs {}", bytes.size(), uuid_size)));
  auto uuid = nil();
  std::ranges::copy(bytes, uuid.storage.begin());
  return uuid;
}

std::expected<Uuid, StarException> Uuid::from_hex(std::string_view hex) {
  auto size = hex.size() / 2;
  if (size != uuid_size)
    return std::unexpected(UuidException(Star::format("Size mismatch in reading Uuid from hex: {} vs {}", size, uuid_size)));
  auto uuid = nil();
  (void)hex_decode(std::span<char const>(hex), std::span(uuid.storage));
  return uuid;
}


String Uuid::hex() const {
  return hex_encode(span());
}


std::size_t ValueHash<Uuid>::operator()(Uuid const& u) const {
  std::size_t hashval = 0;
  for (char byte : u.span())
    hash_combine(hashval, byte);
  return hashval;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Uuid& uuid) {
  auto result = ds.read<Uuid>();
  if (!result)
    return std::unexpected(std::move(result.error()));
  uuid = std::move(*result);
  return std::ref(ds);
}

template <>
std::expected<Uuid, StarException> DataStream::read<Uuid>() {
  auto uuid = Uuid::nil();
  auto result = read_data(uuid.storage);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return uuid;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Uuid const& uuid) {
  auto result = ds.write_data(uuid.span());
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

}
