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
  random.randBytes({m_data.data(), UuidSize});
}

std::expected<Uuid, StarException> Uuid::create(ByteArray const& bytes) {
  if (bytes.size() != UuidSize)
    return std::unexpected(UuidException(Star::format("Size mismatch in reading Uuid from ByteArray: {} vs {}", bytes.size(), UuidSize)));
  auto uuid = nil();
  bytes.copy_to({uuid.m_data.data(), UuidSize});
  return uuid;
}

std::expected<Uuid, StarException> Uuid::create(String const& hex) {
  auto bytes = hexDecode(hex);
  if (!bytes)
    return std::unexpected(std::move(bytes.error()));
  return create(*bytes);
}

std::span<char const> Uuid::span() const {
  return {m_data.data(), UuidSize};
}

ByteArray Uuid::bytes() const {
  return ByteArray(span());
}

String Uuid::hex() const {
  return hexEncode(span());
}

bool Uuid::operator==(Uuid const& u) const {
  return m_data == u.m_data;
}

bool Uuid::operator!=(Uuid const& u) const {
  return m_data != u.m_data;
}

bool Uuid::operator<(Uuid const& u) const {
  return m_data < u.m_data;
}

bool Uuid::operator<=(Uuid const& u) const {
  return m_data <= u.m_data;
}

bool Uuid::operator>(Uuid const& u) const {
  return m_data > u.m_data;
}

bool Uuid::operator>=(Uuid const& u) const {
  return m_data >= u.m_data;
}

std::size_t hash<Uuid>::operator()(Uuid const& u) const {
  std::size_t hashval = 0;
  for (std::size_t i = 0; i < UuidSize; ++i)
    hash_combine(hashval, u.span()[i]);
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
  auto result = read_data({uuid.m_data.data(), UuidSize});
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
