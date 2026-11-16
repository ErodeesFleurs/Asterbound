module;
#include <zlib-ng.h>
#include <zstd.h>

module star.path_embed;
import std;
import star.byte_array;
import star.string;
import star.json;
import star.json_builder;
import star.json_parser;
import star.data_stream_devices;
import star.format;

namespace Star {
namespace {

constexpr std::string_view Prefix = "/opensb_data;";
constexpr std::size_t MaxJsonDepth = 256;
// Compressed representations can be slightly larger than their plaintext.
constexpr std::size_t MaxEncodedBytes = MaxEmbeddedDataSize + MaxEmbeddedDataSize / 128 + 1024;

std::unexpected<StarException> invalid(std::string_view detail) {
  return std::unexpected(StarException(Star::format("Invalid embedded data: {}", detail)));
}

std::optional<EmbeddedCompressionMethod> compressionMethod(std::string_view name) {
  auto equal = [name](std::string_view wanted) {
    if (name.size() != wanted.size())
      return false;
    for (std::size_t i = 0; i < name.size(); ++i) {
      auto c = name[i];
      if (c >= 'A' && c <= 'Z')
        c += 'a' - 'A';
      if (c != wanted[i])
        return false;
    }
    return true;
  };
  if (equal("none")) return EmbeddedCompressionMethod::None;
  if (equal("gzip")) return EmbeddedCompressionMethod::Gzip;
  if (equal("zlib")) return EmbeddedCompressionMethod::Zlib;
  if (equal("zstd")) return EmbeddedCompressionMethod::Zstd;
  return {};
}

String encodeBase64(std::span<char const> bytes, std::string_view prefix = {}) {
  constexpr std::string_view Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  std::string output(prefix.size() + (bytes.size() + 2) / 3 * 4, '=');
  std::ranges::copy(prefix, output.begin());
  std::size_t out = prefix.size();
  for (std::size_t i = 0; i < bytes.size(); i += 3) {
    auto a = static_cast<unsigned char>(bytes[i]);
    auto b = i + 1 < bytes.size() ? static_cast<unsigned char>(bytes[i + 1]) : 0;
    auto c = i + 2 < bytes.size() ? static_cast<unsigned char>(bytes[i + 2]) : 0;
    output[out++] = Alphabet[a >> 2];
    output[out++] = Alphabet[((a & 3) << 4) | (b >> 4)];
    if (i + 1 < bytes.size()) output[out + 0] = Alphabet[((b & 15) << 2) | (c >> 6)];
    if (i + 2 < bytes.size()) output[out + 1] = Alphabet[c & 63];
    out += 2;
  }
  return String(std::move(output));
}

std::expected<ByteArray, StarException> decodeBase64(std::string_view text) {
  auto value = [](char c) -> int {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-') return 62;
    if (c == '_') return 63;
    return -1;
  };
  auto size = text.size();
  std::size_t padding = 0;
  while (padding < size && text[size - padding - 1] == '=') ++padding;
  if (padding > 2 || (padding && size % 4))
    return invalid("invalid Base64URL padding");
  auto digits = size - padding;
  auto remainder = digits % 4;
  if (remainder == 1 || (padding && padding != 4 - remainder))
    return invalid("invalid Base64URL length");
  if (digits / 4 > MaxEncodedBytes / 3)
    return invalid("encoded payload exceeds size limit");
  auto decodedSize = digits / 4 * 3 + (remainder ? remainder - 1 : 0);
  if (decodedSize > MaxEncodedBytes)
    return invalid("encoded payload exceeds size limit");
  ByteArray bytes;
  bytes.resize(decodedSize);
  std::uint32_t bits = 0;
  unsigned bitCount = 0;
  std::size_t out = 0;
  for (std::size_t i = 0; i < digits; ++i) {
    auto digit = value(text[i]);
    if (digit < 0)
      return invalid("invalid Base64URL character");
    bits = (bits << 6) | static_cast<unsigned>(digit);
    bitCount += 6;
    if (bitCount >= 8) {
      bitCount -= 8;
      bytes[out++] = static_cast<char>(bits >> bitCount);
    }
  }
  if (bitCount && (bits & ((1u << bitCount) - 1)))
    return invalid("nonzero Base64URL padding bits");
  return bytes;
}

std::expected<ByteArray, StarException> inflateBytes(std::span<char const> bytes, bool gzip) {
  zng_stream stream{};
  auto status = zng_inflateInit2(&stream, gzip ? 31 : 15);
  if (status != Z_OK)
    return std::unexpected(StarException(Star::format("Embedded inflate initialization error ({})", status)));
  auto cleanup = [](zng_stream* stream) noexcept { (void)zng_inflateEnd(stream); };
  std::unique_ptr<zng_stream, decltype(cleanup)> owner(&stream, cleanup);
  stream.next_in = reinterpret_cast<std::uint8_t const*>(bytes.data());
  stream.avail_in = bytes.size();
  ByteArray output;
  std::array<char, 32768> chunk;
  for (;;) {
    stream.next_out = reinterpret_cast<std::uint8_t*>(chunk.data());
    stream.avail_out = chunk.size();
    auto previousInput = stream.avail_in;
    status = zng_inflate(&stream, Z_NO_FLUSH);
    if (status != Z_OK && status != Z_STREAM_END)
      return std::unexpected(StarException(Star::format("Embedded inflate error ({}): {}", status,
          stream.msg ? stream.msg : "corrupt or truncated compressed payload")));
    auto produced = chunk.size() - stream.avail_out;
    if (produced > MaxEmbeddedDataSize - output.size())
      return invalid("decompressed payload exceeds size limit");
    output.append(std::span<char const>(chunk.data(), produced));
    if (status == Z_STREAM_END) {
      if (stream.avail_in)
        return invalid("trailing compressed bytes or concatenated streams");
      return output;
    }
    if (!produced && previousInput == stream.avail_in)
      return invalid("truncated compressed payload");
  }
}

std::expected<ByteArray, StarException> inflateZstd(std::span<char const> bytes) {
  auto declared = ZSTD_getFrameContentSize(bytes.data(), bytes.size());
  if (declared == ZSTD_CONTENTSIZE_ERROR)
    return invalid("invalid Zstandard header");
  if (declared != ZSTD_CONTENTSIZE_UNKNOWN && declared > MaxEmbeddedDataSize)
    return invalid("Zstandard payload exceeds size limit");
  auto cleanup = [](ZSTD_DCtx* context) noexcept { (void)ZSTD_freeDCtx(context); };
  std::unique_ptr<ZSTD_DCtx, decltype(cleanup)> context(ZSTD_createDCtx(), cleanup);
  if (!context)
    return invalid("cannot initialize Zstandard decompression");
  auto status = ZSTD_DCtx_setParameter(context.get(), ZSTD_d_windowLogMax, 26);
  if (ZSTD_isError(status))
    return invalid("cannot bound Zstandard decompression window");
  ZSTD_inBuffer input{bytes.data(), bytes.size(), 0};
  ByteArray output;
  std::array<char, 32768> chunk;
  for (;;) {
    ZSTD_outBuffer out{chunk.data(), chunk.size(), 0};
    auto previousInput = input.pos;
    status = ZSTD_decompressStream(context.get(), &out, &input);
    if (ZSTD_isError(status))
      return std::unexpected(StarException(Star::format("Embedded Zstandard decompression: {}", ZSTD_getErrorName(status))));
    if (out.pos > MaxEmbeddedDataSize - output.size())
      return invalid("decompressed payload exceeds size limit");
    output.append(std::span<char const>(chunk.data(), out.pos));
    if (!status) {
      if (input.pos != input.size)
        return invalid("trailing compressed bytes or concatenated streams");
      return output;
    }
    if (!out.pos && previousInput == input.pos)
      return invalid("truncated Zstandard payload");
  }
}

std::expected<ByteArray, StarException> unescapeText(std::string_view text) {
  if (text.size() > MaxEmbeddedDataSize * 2)
    return invalid("escaped text exceeds size limit");
  // Reserve at most the decoded limit, not an arbitrarily large escaped input.
  ByteArray output = ByteArray::with_reserve(std::min(text.size(), MaxEmbeddedDataSize));
  for (std::size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (c == '%') {
      if (++i == text.size())
        return invalid("dangling text escape");
      switch (text[i]) {
        case '%': c = '%'; break;
        case 'c': c = ':'; break;
        case 'q': c = '?'; break;
        case 's': c = ';'; break;
        case 'b': c = '/'; break;
        default: return invalid("unknown text escape");
      }
    } else if (c == ':' || c == '?' || c == '/') {
      return invalid("unescaped asset-path delimiter in text");
    }
    if (output.size() == MaxEmbeddedDataSize)
      return invalid("text payload exceeds size limit");
    output.push_back(c);
  }
  return output;
}

// Empty fields are significant, including the empty final payload.
struct Fields {
  std::string_view remaining;
  bool available = true;
  std::optional<std::string_view> take() {
    if (!available) return {};
    auto separator = remaining.find(';');
    if (separator == std::string_view::npos) {
      available = false;
      return remaining;
    }
    auto field = remaining.substr(0, separator);
    remaining.remove_prefix(separator + 1);
    return field;
  }
};

std::expected<void, StarException> validateFramesFields(std::string_view body) {
  Fields fields{body};
  auto type = fields.take();
  auto payload = fields.take();
  if (!type || !payload)
    return invalid("missing embedded frames type or payload");
  if (*type == "text") {
    if (fields.available) return invalid("extra embedded frames text fields");
  } else if (*type == "bin" || *type == "json") {
    if (fields.available && compressionMethod(*payload))
      payload = fields.take();
    if (fields.available) return invalid("extra embedded frames payload fields");
  } else {
    return invalid("unsupported frames type (images cannot be nested in frames)");
  }
  return {};
}

std::expected<EmbeddedData, StarException> decodeBody(std::string_view body, bool asBytes) {
  Fields fields{body};
  auto type = fields.take();
  auto payload = fields.take();
  if (!type || !payload)
    return invalid("missing type or payload");
  EmbeddedAssetType assetType;
  if (*type == "text") assetType = EmbeddedAssetType::Text;
  else if (*type == "bin") assetType = EmbeddedAssetType::Binary;
  else if (*type == "json") assetType = EmbeddedAssetType::Json;
  else if (*type == "image") assetType = EmbeddedAssetType::Image;
  else return invalid("unsupported type");

  ByteArray bytes;
  if (assetType == EmbeddedAssetType::Text) {
    if (fields.available)
      return invalid("extra text fields");
    auto decoded = unescapeText(*payload);
    if (!decoded) return std::unexpected(std::move(decoded.error()));
    bytes = std::move(*decoded);
  } else {
    auto method = EmbeddedCompressionMethod::None;
    if (fields.available) {
      if (auto named = compressionMethod(*payload)) {
        method = *named;
        payload = fields.take();
      }
    }
    auto decoded = decodeBase64(*payload);
    if (!decoded) return std::unexpected(std::move(decoded.error()));
    if (method == EmbeddedCompressionMethod::None) {
      if (decoded->size() > MaxEmbeddedDataSize)
        return invalid("payload exceeds size limit");
      bytes = std::move(*decoded);
    } else {
      auto inflated = method == EmbeddedCompressionMethod::Zstd
          ? inflateZstd(decoded->span())
          : inflateBytes(decoded->span(), method == EmbeddedCompressionMethod::Gzip);
      if (!inflated) return std::unexpected(std::move(inflated.error()));
      bytes = std::move(*inflated);
    }
  }

  EmbeddedData result{assetType, std::move(bytes), {}, {}};
  if (assetType != EmbeddedAssetType::Image) {
    if (fields.available)
      return invalid("extra payload fields");
    return result;
  }
  if (!fields.available) {
    if (asBytes) return result;
    return invalid("image requires frames or framespath (use bin for an unframed image)");
  }
  auto specification = fields.take();
  if (*specification == "framespath") {
    auto path = fields.take();
    if (!path || path->empty() || fields.available)
      return invalid("framespath requires exactly one nonempty path");
    result.framesPath = String(std::string(*path));
  } else if (*specification == "frames") {
    if (!fields.available)
      return invalid("missing embedded frames");
    // Validate the complete suffix without decoding it twice. Its ordinary
    // JSON consumer performs payload validation and bounded decompression.
    auto frames = validateFramesFields(fields.remaining);
    if (!frames) return std::unexpected(std::move(frames.error()));
    std::string path;
    path.reserve(Prefix.size() + fields.remaining.size());
    path.append(Prefix);
    path.append(fields.remaining);
    result.framesData = String(std::move(path));
  } else {
    return invalid("unsupported image frame specification");
  }
  return result;
}

// Validate wire before the ordinary decoder can allocate strings or recurse.
// This accepts the historical zero tag for null, as well as the current tag 1.
class WireScanner {
public:
  explicit WireScanner(std::span<char const> input) : m_input(input) {}

  std::expected<void, StarException> scan() {
    struct Frame { std::uint64_t remaining; bool object; };
    std::array<Frame, MaxJsonDepth + 1> stack{};
    stack[0] = {1, false};
    std::size_t depth = 0;
    for (;;) {
      if (!stack[depth].remaining) {
        if (!depth) break;
        --depth;
        continue;
      }
      --stack[depth].remaining;
      if (stack[depth].object) {
        auto key = string();
        if (!key) return key;
      }
      if (m_input.empty()) return invalid("truncated binary JSON value");
      auto tag = static_cast<unsigned char>(m_input.front());
      m_input = m_input.subspan(1);
      if (tag <= 1) continue;
      if (tag == 2 || tag == 3) {
        auto size = tag == 2 ? 8u : 1u;
        if (m_input.size() < size) return invalid("truncated binary JSON scalar");
        m_input = m_input.subspan(size);
      } else if (tag == 4) {
        auto integer = vlq();
        if (!integer) return std::unexpected(std::move(integer.error()));
      } else if (tag == 5) {
        auto text = string();
        if (!text) return text;
      } else if (tag == 6 || tag == 7) {
        auto count = vlq();
        if (!count) return std::unexpected(std::move(count.error()));
        if (*count > m_input.size() / (tag == 7 ? 2 : 1))
          return invalid("binary JSON count exceeds remaining bytes");
        if (depth == MaxJsonDepth)
          return invalid("binary JSON exceeds nesting depth 256");
        stack[++depth] = {*count, tag == 7};
      } else {
        return invalid("unknown binary JSON type");
      }
    }
    if (!m_input.empty()) return invalid("trailing binary JSON bytes");
    return {};
  }

private:
  std::expected<std::uint64_t, StarException> vlq() {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < 10; ++i) {
      if (m_input.empty()) return invalid("truncated binary JSON VLQ");
      auto byte = static_cast<unsigned char>(m_input.front());
      m_input = m_input.subspan(1);
      if (value > (std::numeric_limits<std::uint64_t>::max() >> 7))
        return invalid("binary JSON VLQ overflow");
      value = (value << 7) | (byte & 127);
      if (!(byte & 128)) return value;
    }
    return invalid("unterminated binary JSON VLQ");
  }

  std::expected<void, StarException> string() {
    auto size = vlq();
    if (!size) return std::unexpected(std::move(size.error()));
    if (*size > m_input.size()) return invalid("binary JSON string exceeds remaining bytes");
    m_input = m_input.subspan(static_cast<std::size_t>(*size));
    return {};
  }

  std::span<char const> m_input;
};

// Only bound recursion here; the existing parser still owns JSON syntax,
// Unicode, numbers, comments, and complete-document validation.
std::expected<void, StarException> checkTextDepth(std::span<char const> text) {
  enum class State { Normal, String, Escape, LineComment, BlockComment };
  auto state = State::Normal;
  std::size_t depth = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (!c) return invalid("literal NUL in textual JSON");
    if (state == State::Escape) { state = State::String; continue; }
    if (state == State::String) {
      if (c == '\\') state = State::Escape;
      else if (c == '"') state = State::Normal;
      continue;
    }
    if (state == State::LineComment) {
      if (c == '\n') state = State::Normal;
      continue;
    }
    if (state == State::BlockComment) {
      if (c == '*' && i + 1 < text.size() && text[i + 1] == '/') {
        ++i;
        state = State::Normal;
      }
      continue;
    }
    if (c == '"') state = State::String;
    else if (c == '/' && i + 1 < text.size() && text[i + 1] == '/') {
      ++i;
      state = State::LineComment;
    } else if (c == '/' && i + 1 < text.size() && text[i + 1] == '*') {
      ++i;
      state = State::BlockComment;
    } else if (c == '[' || c == '{') {
      if (++depth > MaxJsonDepth) return invalid("textual JSON exceeds nesting depth 256");
    } else if ((c == ']' || c == '}') && depth) {
      --depth;
    }
  }
  return {};
}

}

std::expected<EmbeddedCompressionMethod, StarException> embeddedCompressionMethod(String const& name) {
  if (auto method = compressionMethod(name.utf8())) return *method;
  return invalid("unsupported compression method");
}

std::expected<String, StarException> embedAndCompressData(std::span<char const> data, EmbeddedCompressionMethod method) {
  if (data.size() > MaxEmbeddedDataSize)
    return invalid("input exceeds size limit");
  if (method == EmbeddedCompressionMethod::None)
    return encodeBase64(data);
  if (method == EmbeddedCompressionMethod::Gzip)
    return std::unexpected(StarException("Compressing data in gzip is unsupported."));
  ByteArray bytes;
  std::string_view token;
  if (method == EmbeddedCompressionMethod::Zlib) {
    auto capacity = zng_compressBound(data.size());
    bytes.resize(capacity);
    std::size_t size = capacity;
    auto status = zng_compress2(reinterpret_cast<std::uint8_t*>(bytes.data()), &size,
        reinterpret_cast<std::uint8_t const*>(data.data()), data.size(), 5);
    if (status != Z_OK)
      return std::unexpected(StarException(Star::format("Embedded zlib compression error ({})", status)));
    bytes.resize(size);
    token = "zlib;";
  } else if (method == EmbeddedCompressionMethod::Zstd) {
    auto capacity = ZSTD_compressBound(data.size());
    if (ZSTD_isError(capacity)) return invalid("Zstandard compression bound failed");
    bytes.resize(capacity);
    auto size = ZSTD_compress(bytes.data(), capacity, data.data(), data.size(), 0);
    if (ZSTD_isError(size))
      return std::unexpected(StarException(Star::format("Embedded Zstandard compression: {}", ZSTD_getErrorName(size))));
    bytes.resize(size);
    token = "zstd;";
  } else {
    return invalid("unsupported compression method");
  }
  return encodeBase64(bytes.span(), token);
}

std::expected<String, StarException> embedEscapeText(String const& text) {
  auto const& input = text.utf8();
  if (input.size() > MaxEmbeddedDataSize)
    return invalid("text input exceeds size limit");
  std::size_t escaped = 0;
  for (char c : input)
    escaped += c == '%' || c == ':' || c == '?' || c == ';' || c == '/';
  std::string output;
  output.reserve(input.size() + escaped);
  for (char c : input) {
    switch (c) {
      case '%': output += "%%"; break;
      case ':': output += "%c"; break;
      case '?': output += "%q"; break;
      case ';': output += "%s"; break;
      case '/': output += "%b"; break;
      default: output += c; break;
    }
  }
  return String(std::move(output));
}

std::expected<EmbeddedData, StarException> decodeEmbeddedData(String const& basePath, bool asBytes) {
  std::string_view path = basePath.utf8();
  if (!path.starts_with(Prefix)) return invalid("missing /opensb_data; prefix");
  return decodeBody(path.substr(Prefix.size()), asBytes);
}

std::expected<Json, StarException> embeddedDataToJson(EmbeddedData const& data) {
  if (data.bytes.size() > MaxEmbeddedDataSize)
    return invalid("JSON payload exceeds size limit");
  if (data.type == EmbeddedAssetType::Json) {
    auto checked = WireScanner(data.bytes.span()).scan();
    if (!checked) return std::unexpected(std::move(checked.error()));
    DataStreamReader stream(data.bytes.span());
    return stream.read<Json>();
  }
  if (data.type == EmbeddedAssetType::Image)
    return invalid("image payload is not JSON");
  auto checked = checkTextDepth(data.bytes.span());
  if (!checked) return std::unexpected(std::move(checked.error()));
  return inputUtf8Json(data.bytes.begin(), data.bytes.end(), JsonParseType::Value);
}

}
