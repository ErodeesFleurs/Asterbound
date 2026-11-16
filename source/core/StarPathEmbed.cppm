module;
import std;
import star.byte_array;
import star.string;
import star.json;
import star.format;

export module star.path_embed;

export namespace Star {

enum class EmbeddedCompressionMethod { None, Gzip, Zlib, Zstd };

// Untrusted embedded payloads are limited independently of ordinary assets.
constexpr std::size_t MaxEmbeddedDataSize = 64 * 1024 * 1024;

[[nodiscard]] std::expected<EmbeddedCompressionMethod, StarException> embeddedCompressionMethod(String const& name);
// Returns the payload (including an optional compression token), not an asset path.
[[nodiscard]] std::expected<String, StarException> embedAndCompressData(std::span<char const> data, EmbeddedCompressionMethod method);
[[nodiscard]] std::expected<String, StarException> embedEscapeText(String const& text);

enum class EmbeddedAssetType { Text, Binary, Json, Image };
struct EmbeddedData {
  EmbeddedAssetType type;
  ByteArray bytes;
  std::optional<String> framesPath;
  // Complete embedded asset path, never an unprefixed token suffix.
  std::optional<String> framesData;
};

// Accepts a complete /opensb_data; base path, without subpath or directives.
// Raw byte consumers may read an image without its frame specification.
[[nodiscard]] std::expected<EmbeddedData, StarException> decodeEmbeddedData(String const& basePath, bool asBytes = false);
// Text / binary payloads contain textual JSON; Json payloads retain DataStream wire.
// Both forms enforce a maximum container nesting depth of 256.
[[nodiscard]] std::expected<Json, StarException> embeddedDataToJson(EmbeddedData const& data);

}
