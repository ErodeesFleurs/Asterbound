module;
import std;
import star.memory;

export module star.vlq;

export namespace Star {

inline std::size_t vlqUSize(std::uint64_t x) { return std::max(1, (std::bit_width(x) + 6) / 7); }

// Write an unsigned integer as a VLQ (Variable Length Quantity).  Writes the
// integer in 7 byte chunks, with the 8th bit of each octet indicates whether
// another chunk follows.  Endianness independent, as the chunks are always
// written most significant first. Returns number of octet written (writes a
// maximum of a 64 bit integer, so a maximum of 10)
template <typename OutputIterator>
std::size_t writeVlqU(std::uint64_t x, OutputIterator out) {
  std::size_t i = vlqUSize(x) - 1;

  for (std::size_t j = 0; j < i; ++j)
    *out++ = static_cast<std::uint8_t>(((x >> ((i - j) * 7)) & 127)) | 128;

  *out++ = static_cast<std::uint8_t>((x & 127));
  return i + 1;
}

// Read a VLQ (Variable Length Quantity) encoded unsigned integer.  Returns
// number of bytes read.  Reads a *maximum of 10 bytes*, cannot read a larger
// than 64 bit integer!  If no end marker is found within 'maxBytes' or 10
// bytes, whichever is smaller, then will return NPos to signal error.
template <typename InputIterator>
std::size_t readVlqU(std::uint64_t& x, InputIterator in, std::size_t maxBytes = 10) {
  x = 0;
  for (std::size_t i = 0; i < std::min<std::size_t>(10, maxBytes); ++i) {
    std::uint8_t oct = *in++;
    x = (x << 7) | static_cast<std::uint64_t>((oct & 127));
    if (!(oct & 128))
      return i + 1;
  }

  return NPos;
}

// Write a VLQ (Variable Length Quantity) encoded signed integer.  Encoded by
// making the sign bit the least significant bit in the integer.  Returns
// number of bytes written.
template <typename OutputIterator>
std::size_t writeVlqI(std::int64_t v, OutputIterator out) {
  std::uint64_t target;

  // If negative, then add 1 to properly encode -2^63
  if (v < 0)
    target = ((-(v + 1)) << 1) | 1;
  else
    target = v << 1;

  return writeVlqU(target, out);
}

inline std::size_t vlqISize(std::int64_t v) {
  std::uint64_t target;

  // If negative, then add 1 to properly encode -2^63
  if (v < 0)
    target = ((-(v + 1)) << 1) | 1;
  else
    target = v << 1;

  return vlqUSize(target);
}

// Read a VLQ (Variable Length Quantity) encoded signed integer.  Returns
// number of bytes read.  Reads a *maximum of 10 bytes*, cannot read a larger
// than 64 bit integer!  If no end marker is found within 'maxBytes' or 10
// bytes, whichever is smaller, then will return NPos to signal error.
template <typename InputIterator>
std::size_t readVlqI(std::int64_t& v, InputIterator in, std::size_t maxBytes = 10) {
  std::uint64_t source;
  std::size_t bytes = readVlqU(source, in, maxBytes);
  if (bytes == NPos)
    return NPos;

  bool negative = (source & 1);

  // If negative, then need to undo the +1 transformation to encode -2^63
  if (negative)
    v = -static_cast<std::int64_t>((source >> 1)) - 1;
  else
    v = static_cast<std::int64_t>((source >> 1));

  return bytes;
}

}
