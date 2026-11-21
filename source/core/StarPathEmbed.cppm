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
constexpr std::size_t max_embedded_data_size = 64 * 1024 * 1024;

[[nodiscard]] std::expected<EmbeddedCompressionMethod, StarException> embedded_compression_method(String const& name);
// Append only the payload (including an optional compression token), never the asset type.
// Both append operations require input storage disjoint from output.
[[nodiscard]] std::expected<void, StarException> append_embedded_data(std::string& output,
    std::span<char const> data, EmbeddedCompressionMethod method);
[[nodiscard]] std::expected<void, StarException> append_escaped_text(std::string& output, std::string_view input);

enum class EmbeddedAssetType { Text, Binary, Json, Image };
enum class DecodeMode { Asset, RawBytes };
struct ExternalFramesPath { String path; };
// Complete embedded asset path, never an unprefixed token suffix.
struct EmbeddedFramesPath { String path; };

struct EmbeddedData {
  EmbeddedAssetType type;
  ByteArray bytes;
  std::variant<std::monostate, ExternalFramesPath, EmbeddedFramesPath> frame_source;
};

// Accepts a complete /opensb_data; base path, without subpath or directives.
// Raw byte consumers may read an image without its frame specification.
[[nodiscard]] std::expected<EmbeddedData, StarException> decode_embedded_data(String const& base_path,
    DecodeMode mode = DecodeMode::Asset);
// Text / binary payloads contain textual JSON; Json payloads retain DataStream wire.
// Both forms enforce a maximum container nesting depth of 256.
[[nodiscard]] std::expected<Json, StarException> embedded_data_to_json(EmbeddedData const& data);

}
