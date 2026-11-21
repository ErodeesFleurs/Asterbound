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

constexpr std::string_view path_prefix = "/opensb_data;";
constexpr std::size_t max_json_depth = 256;
// Compressed representations can be slightly larger than their plaintext.
constexpr std::size_t max_encoded_bytes = max_embedded_data_size + max_embedded_data_size / 128 + 1024;

std::unexpected<StarException> invalid(std::string_view detail) {
  return std::unexpected(StarException(Star::format("Invalid embedded data: {}", detail)));
}

std::optional<EmbeddedCompressionMethod> compression_method(std::string_view name) {
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

// The caller bounds bytes by the embedded payload or compression bound.
std::expected<void, StarException> append_base64(std::string& output, std::span<char const> bytes, std::string_view token = {}) {
  constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  auto const start = output.size();
  auto const extra = token.size() + (bytes.size() + 2) / 3 * 4;
  if (extra > output.max_size() - start)
    return invalid("embedded path exceeds string capacity");
  output.resize_and_overwrite(start + extra, [&](std::same_as<char*> auto storage, std::size_t capacity) noexcept {
    auto tail = std::span(storage, capacity).subspan(start);
    std::ranges::copy(token, tail.begin());
    auto out = token.size();
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
      auto a = static_cast<unsigned char>(bytes[i]);
      auto b = i + 1 < bytes.size() ? static_cast<unsigned char>(bytes[i + 1]) : 0;
      auto c = i + 2 < bytes.size() ? static_cast<unsigned char>(bytes[i + 2]) : 0;
      tail[out++] = alphabet[a >> 2];
      tail[out++] = alphabet[((a & 3) << 4) | (b >> 4)];
      tail[out++] = i + 1 < bytes.size() ? alphabet[((b & 15) << 2) | (c >> 6)] : '=';
      tail[out++] = i + 2 < bytes.size() ? alphabet[c & 63] : '=';
    }
    return start + out;
  });
  return {};
}

std::expected<ByteArray, StarException> decode_base64(std::string_view text, std::size_t limit) {
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
  if (digits / 4 > limit / 3)
    return invalid("encoded payload exceeds size limit");
  auto decoded_size = digits / 4 * 3 + (remainder ? remainder - 1 : 0);
  if (decoded_size > limit)
    return invalid("encoded payload exceeds size limit");
  ByteArray bytes;
  bytes.resize(decoded_size);
  std::uint32_t bits = 0;
  unsigned bit_count = 0;
  std::size_t out = 0;
  for (std::size_t i = 0; i < digits; ++i) {
    auto digit = value(text[i]);
    if (digit < 0)
      return invalid("invalid Base64URL character");
    bits = (bits << 6) | static_cast<unsigned>(digit);
    bit_count += 6;
    if (bit_count >= 8) {
      bit_count -= 8;
      bytes[out++] = static_cast<char>(bits >> bit_count);
    }
  }
  if (bit_count && (bits & ((1u << bit_count) - 1)))
    return invalid("nonzero Base64URL padding bits");
  return bytes;
}

// Grow only in powers of two up to the decoded limit. At the limit a single
// probe byte lets the codec consume trailers without allocating past the cap.
std::span<char> prepare_output_tail(ByteArray& output, std::span<char, 1> probe) {
  auto previous_size = output.size();
  if (previous_size == max_embedded_data_size)
    return probe;
  if (previous_size == output.capacity())
    output.reserve(output.capacity() ? std::min(max_embedded_data_size, output.capacity() * 2) : 4096);
  output.resize(output.capacity());
  return output.span().subspan(previous_size);
}

std::expected<ByteArray, StarException> inflate_bytes(std::span<char const> bytes, bool gzip) {
  zng_stream stream{};
  auto status = zng_inflateInit2(&stream, gzip ? 31 : 15);
  if (status != Z_OK)
    return std::unexpected(StarException(Star::format("Embedded inflate initialization error ({})", status)));
  struct StreamCleanup {
    zng_stream& stream;
    ~StreamCleanup() { (void)zng_inflateEnd(&stream); }
  } cleanup{stream};
  stream.next_in = reinterpret_cast<std::uint8_t const*>(bytes.data());
  stream.avail_in = bytes.size();
  ByteArray output;
  std::array<char, 1> probe;
  for (;;) {
    auto previous_size = output.size();
    auto tail = prepare_output_tail(output, probe);
    stream.next_out = reinterpret_cast<std::uint8_t*>(tail.data());
    stream.avail_out = tail.size();
    auto previous_input = stream.avail_in;
    status = zng_inflate(&stream, Z_NO_FLUSH);
    if (status != Z_OK && status != Z_STREAM_END)
      return std::unexpected(StarException(Star::format("Embedded inflate error ({}): {}", status,
          stream.msg ? stream.msg : "corrupt or truncated compressed payload")));
    auto produced = tail.size() - stream.avail_out;
    if (produced > max_embedded_data_size - previous_size)
      return invalid("decompressed payload exceeds size limit");
    output.resize(previous_size + produced);
    if (status == Z_STREAM_END) {
      if (stream.avail_in)
        return invalid("trailing compressed bytes or concatenated streams");
      return output;
    }
    if (!produced && previous_input == stream.avail_in)
      return invalid("truncated compressed payload");
  }
}

std::expected<ByteArray, StarException> inflate_zstd(std::span<char const> bytes) {
  auto declared = ZSTD_getFrameContentSize(bytes.data(), bytes.size());
  if (declared == ZSTD_CONTENTSIZE_ERROR)
    return invalid("invalid Zstandard header");
  if (declared != ZSTD_CONTENTSIZE_UNKNOWN && declared > max_embedded_data_size)
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
  std::array<char, 1> probe;
  for (;;) {
    auto previous_size = output.size();
    auto tail = prepare_output_tail(output, probe);
    ZSTD_outBuffer out{tail.data(), tail.size(), 0};
    auto previous_input = input.pos;
    status = ZSTD_decompressStream(context.get(), &out, &input);
    if (ZSTD_isError(status))
      return std::unexpected(StarException(Star::format("Embedded Zstandard decompression: {}", ZSTD_getErrorName(status))));
    if (out.pos > max_embedded_data_size - previous_size)
      return invalid("decompressed payload exceeds size limit");
    output.resize(previous_size + out.pos);
    if (!status) {
      if (input.pos != input.size)
        return invalid("trailing compressed bytes or concatenated streams");
      return output;
    }
    if (!out.pos && previous_input == input.pos)
      return invalid("truncated Zstandard payload");
  }
}

std::expected<ByteArray, StarException> unescape_text(std::string_view text) {
  if (text.size() > max_embedded_data_size * 2)
    return invalid("escaped text exceeds size limit");
  // Reserve at most the decoded limit, not an arbitrarily large escaped input.
  ByteArray output = ByteArray::with_reserve(std::min(text.size(), max_embedded_data_size));
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
    if (output.size() == max_embedded_data_size)
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

struct Payload {
  EmbeddedAssetType type;
  EmbeddedCompressionMethod method;
  std::string_view text;
};

std::expected<Payload, StarException> parse_payload(Fields& fields) {
  auto type = fields.take();
  auto text = fields.take();
  if (!type || !text)
    return invalid("missing type or payload");
  EmbeddedAssetType asset_type;
  if (*type == "text") asset_type = EmbeddedAssetType::Text;
  else if (*type == "bin") asset_type = EmbeddedAssetType::Binary;
  else if (*type == "json") asset_type = EmbeddedAssetType::Json;
  else if (*type == "image") asset_type = EmbeddedAssetType::Image;
  else return invalid("unsupported type");

  auto method = EmbeddedCompressionMethod::None;
  if (asset_type != EmbeddedAssetType::Text && fields.available) {
    if (auto named = compression_method(*text)) {
      method = *named;
      text = fields.take();
    }
  }
  return Payload{asset_type, method, *text};
}

struct Layout {
  Payload payload;
  enum class Frames { None, External, Embedded } frames = Frames::None;
  std::string_view frames_value{};
};

std::expected<Layout, StarException> parse_layout(std::string_view body, DecodeMode mode) {
  Fields fields{body};
  auto payload = parse_payload(fields);
  if (!payload) return std::unexpected(std::move(payload.error()));
  if (payload->type != EmbeddedAssetType::Image) {
    if (fields.available) return invalid("extra payload fields");
    return Layout{*payload};
  }
  if (!fields.available) {
    if (mode == DecodeMode::RawBytes) return Layout{*payload};
    return invalid("image requires frames or framespath (use bin for an unframed image)");
  }
  auto specification = fields.take();
  if (*specification == "framespath") {
    auto path = fields.take();
    if (!path || path->empty() || fields.available)
      return invalid("framespath requires exactly one nonempty path");
    return Layout{*payload, Layout::Frames::External, *path};
  }
  if (*specification != "frames") return invalid("unsupported image frame specification");
  if (!fields.available) return invalid("missing embedded frames");
  // Check the nested layout; the JSON consumer decodes its payload once.
  auto suffix = fields.remaining;
  auto frames = parse_payload(fields);
  if (!frames) return std::unexpected(std::move(frames.error()));
  if (frames->type == EmbeddedAssetType::Image) return invalid("images cannot be nested in frames");
  if (fields.available) return invalid("extra embedded frames payload fields");
  return Layout{*payload, Layout::Frames::Embedded, suffix};
}

std::expected<ByteArray, StarException> decode_payload(Payload const& payload) {
  if (payload.type == EmbeddedAssetType::Text)
    return unescape_text(payload.text);
  auto bytes = decode_base64(payload.text,
      payload.method == EmbeddedCompressionMethod::None ? max_embedded_data_size : max_encoded_bytes);
  if (!bytes || payload.method == EmbeddedCompressionMethod::None)
    return bytes;
  if (payload.method == EmbeddedCompressionMethod::Zstd)
    return inflate_zstd(bytes->span());
  return inflate_bytes(bytes->span(), payload.method == EmbeddedCompressionMethod::Gzip);
}

// Validate wire before the ordinary decoder can allocate strings or recurse.
// This accepts the historical zero tag for null, as well as the current tag 1.
class WireScanner {
public:
  explicit WireScanner(std::span<char const> input) : m_input(input) {}

  std::expected<void, StarException> scan() {
    struct Frame { std::uint64_t remaining; bool object; };
    std::array<Frame, max_json_depth + 1> stack{};
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
        if (depth == max_json_depth)
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
std::expected<void, StarException> check_text_depth(std::span<char const> text) {
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
      if (++depth > max_json_depth) return invalid("textual JSON exceeds nesting depth 256");
    } else if ((c == ']' || c == '}') && depth) {
      --depth;
    }
  }
  return {};
}

}

std::expected<EmbeddedCompressionMethod, StarException> embedded_compression_method(String const& name) {
  if (auto method = compression_method(name.utf8())) return *method;
  return invalid("unsupported compression method");
}

std::expected<void, StarException> append_embedded_data(std::string& output, std::span<char const> data, EmbeddedCompressionMethod method) {
  if (data.size() > max_embedded_data_size)
    return invalid("input exceeds size limit");
  if (method == EmbeddedCompressionMethod::None)
    return append_base64(output, data);
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
  return append_base64(output, bytes.span(), token);
}

std::expected<void, StarException> append_escaped_text(std::string& output, std::string_view input) {
  if (input.size() > max_embedded_data_size)
    return invalid("text input exceeds size limit");
  std::size_t escaped = 0;
  for (char c : input)
    escaped += c == '%' || c == ':' || c == '?' || c == ';' || c == '/';
  auto const start = output.size();
  auto const extra = input.size() + escaped;
  if (extra > output.max_size() - start)
    return invalid("embedded path exceeds string capacity");
  output.resize_and_overwrite(start + extra, [&](std::same_as<char*> auto storage, std::size_t capacity) noexcept {
    auto tail = std::span(storage, capacity).subspan(start);
    std::size_t out = 0;
    for (char c : input) {
      switch (c) {
        case '%': break;
        case ':': c = 'c'; break;
        case '?': c = 'q'; break;
        case ';': c = 's'; break;
        case '/': c = 'b'; break;
        default: tail[out++] = c; continue;
      }
      tail[out++] = '%';
      tail[out++] = c;
    }
    return start + out;
  });
  return {};
}

std::expected<EmbeddedData, StarException> decode_embedded_data(String const& base_path, DecodeMode mode) {
  std::string_view path = base_path.utf8();
  if (!path.starts_with(path_prefix)) return invalid("missing /opensb_data; prefix");
  auto layout = parse_layout(path.substr(path_prefix.size()), mode);
  if (!layout) return std::unexpected(std::move(layout.error()));
  auto bytes = decode_payload(layout->payload);
  if (!bytes) return std::unexpected(std::move(bytes.error()));

  EmbeddedData result{layout->payload.type, std::move(*bytes), {}};
  switch (layout->frames) {
    case Layout::Frames::External:
      result.frame_source.emplace<ExternalFramesPath>(String(layout->frames_value));
      break;
    case Layout::Frames::Embedded: {
      std::string complete;
      complete.reserve(path_prefix.size() + layout->frames_value.size());
      complete.append(path_prefix).append(layout->frames_value);
      result.frame_source.emplace<EmbeddedFramesPath>(String(std::move(complete)));
      break;
    }
    case Layout::Frames::None:
      break;
  }
  return result;
}

std::expected<Json, StarException> embedded_data_to_json(EmbeddedData const& data) {
  if (data.bytes.size() > max_embedded_data_size)
    return invalid("JSON payload exceeds size limit");
  if (data.type == EmbeddedAssetType::Json) {
    auto checked = WireScanner(data.bytes.span()).scan();
    if (!checked) return std::unexpected(std::move(checked.error()));
    DataStreamReader stream(data.bytes.span());
    return stream.read<Json>();
  }
  if (data.type == EmbeddedAssetType::Image)
    return invalid("image payload is not JSON");
  auto checked = check_text_depth(data.bytes.span());
  if (!checked) return std::unexpected(std::move(checked.error()));
  return input_utf8_json(data.bytes.begin(), data.bytes.end(), JsonParseType::Value);
}

}
