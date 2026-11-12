module;
import std;
import star.format;
import star.string;
import star.vector;
import star.image;
import star.byte_array;
import star.map;

namespace Star {

struct FontExceptionTag {
  static constexpr std::string_view name() { return "FontException"; }
};
using FontException = StarError<FontExceptionTag, StarException>;

struct FontImpl;
struct FTContext;

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
  std::shared_ptr<FTContext> m_context;
};

class Font {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<Font>, StarException> loadFont(FontLibrary const& library, String const& fileName, unsigned pixelSize = 12);
  [[nodiscard]] static std::expected<std::shared_ptr<Font>, StarException> loadFont(FontLibrary const& library, std::shared_ptr<ByteArray const> const& bytes, unsigned pixelSize = 12);

  explicit Font(FontLibrary const& library);
  ~Font();

  Font(Font const&) = delete;
  Font const& operator=(Font const&) = delete;

  // Create a new font from the same data
  [[nodiscard]] std::expected<std::shared_ptr<Font>, StarException> clone() const;
  // Asset caches must retain decoded data while independent faces still use it.
  bool sharesBackingBuffer() const { return m_fontBuffer.use_count() > 1; }

  [[nodiscard]] std::expected<void, StarException> setPixelSize(unsigned pixelSize);
  void setAlphaThreshold(std::uint8_t alphaThreshold = 0);

  unsigned height() const;
  [[nodiscard]] std::expected<unsigned, StarException> width(String::Char c);

  // Empty glyph bitmaps are valid; FreeType failures are diagnostic values.
  [[nodiscard]] std::expected<std::tuple<Image, Vec2I, bool>, StarException> render(String::Char c);
  [[nodiscard]] std::expected<bool, StarException> exists(String::Char c);

private:
  explicit Font(std::shared_ptr<FTContext> context);

  std::shared_ptr<FTContext> m_context;
  std::shared_ptr<ByteArray const> m_fontBuffer;
  std::shared_ptr<FontImpl> m_fontImpl;
  unsigned m_pixelSize;
  std::uint8_t m_alphaThreshold;

  [[nodiscard]] std::expected<void, StarException> loadFontImpl();
  HashMap<std::pair<String::Char, unsigned>, unsigned> m_widthCache;
};

}

export module star.font;

export namespace Star {
  using ::Star::FontExceptionTag;
  using ::Star::FontException;
  using ::Star::FontImpl;
  using ::Star::FTContext;
  using ::Star::FontLibrary;
  using ::Star::Font;
}
