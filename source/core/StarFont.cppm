module;
import std;
import star.format;
import star.string;
import star.vector;
import star.image;
import star.byte_array;
import star.map;

struct FT_FaceRec_;

export module star.font;

export namespace Star {

class Font;

struct FontExceptionTag {
  static constexpr std::string_view name() { return "FontException"; }
};
using FontException = StarError<FontExceptionTag, StarException>;

// Explicit FreeType library owner. Loaded fonts retain the library, including
// when the originating asset catalog has been replaced.
class FontLibrary {
public:
  [[nodiscard]] static std::expected<FontLibrary, StarException> create();
  FontLibrary(FontLibrary&&) noexcept = default;
  FontLibrary& operator=(FontLibrary&&) noexcept = default;
  ~FontLibrary();

  FontLibrary(FontLibrary const&) = delete;
  FontLibrary& operator=(FontLibrary const&) = delete;

private:
  FontLibrary() = default;
  friend class Font;
  struct Context;
  std::shared_ptr<Context> m_context;
};

struct RenderedGlyph {
  Image image;
  Vec2I offset{};
  bool colored = false;
};

class Font {
  struct ConstructionKey { explicit ConstructionKey() = default; };
  struct FaceDeleter {
    // FreeType borrows the bytes; the deleter retains both dependencies until
    // after FT_Done_Face, then drops the bytes before the library context.
    std::shared_ptr<FontLibrary::Context> context;
    std::shared_ptr<ByteArray const> bytes;
    void operator()(FT_FaceRec_* face) const noexcept;
  };
  using Face = std::unique_ptr<FT_FaceRec_, FaceDeleter>;

public:
  [[nodiscard]] static std::expected<std::shared_ptr<Font>, StarException> load(FontLibrary const& library, String const& filename, unsigned pixel_size = 12);
  [[nodiscard]] static std::expected<std::shared_ptr<Font>, StarException> load(FontLibrary const& library, std::shared_ptr<ByteArray const> bytes, unsigned pixel_size = 12);

  Font(ConstructionKey, Face face, unsigned pixel_size, std::uint8_t alpha_threshold);
  ~Font();

  Font(Font const&) = delete;
  Font const& operator=(Font const&) = delete;

  // Share decoded data, but retain independent face, size and threshold state.
  [[nodiscard]] std::expected<std::shared_ptr<Font>, StarException> clone() const;
  // Asset caches must retain decoded data while independent faces still use it.
  bool shares_backing_buffer() const noexcept { return m_face.get_deleter().bytes.use_count() > 1; }

  [[nodiscard]] std::expected<void, StarException> set_pixel_size(unsigned pixel_size);
  void set_alpha_threshold(std::uint8_t alpha_threshold = 0) noexcept { m_alpha_threshold = alpha_threshold; }

  unsigned height() const noexcept { return m_pixel_size; }
  // Cached advances do not change the active face size.
  [[nodiscard]] std::expected<unsigned, StarException> width(String::Char c, unsigned pixel_size);

  // Empty glyph bitmaps are valid; FreeType failures are diagnostic values.
  [[nodiscard]] std::expected<RenderedGlyph, StarException> render(String::Char c);
  bool exists(String::Char c) const noexcept;

private:
  static std::expected<Face, StarException> open_face(std::shared_ptr<FontLibrary::Context> context, std::shared_ptr<ByteArray const> bytes);
  static std::expected<std::shared_ptr<Font>, StarException> create(std::shared_ptr<FontLibrary::Context> context, std::shared_ptr<ByteArray const> bytes, unsigned pixel_size, std::uint8_t alpha_threshold);

  Face m_face;
  unsigned m_pixel_size;
  std::uint8_t m_alpha_threshold;
  HashMap<std::pair<String::Char, unsigned>, unsigned> m_width_cache;
};

}
