#include "media/Png.h"

#include <array>
#include <cstring>
#include <fstream>
#include <vector>

namespace keyflow {

namespace {

// --- CRC-32, as PNG specifies ---------------------------------------------

unsigned crcTableEntry(unsigned index)
{
    unsigned value = index;
    for (int bit = 0; bit < 8; ++bit) {
        value = (value & 1) ? (0xEDB88320u ^ (value >> 1)) : (value >> 1);
    }
    return value;
}

unsigned crc32(const unsigned char* data, std::size_t length, unsigned seed = 0)
{
    static const std::array<unsigned, 256> table = [] {
        std::array<unsigned, 256> built{};
        for (unsigned i = 0; i < 256; ++i) built[i] = crcTableEntry(i);
        return built;
    }();

    unsigned crc = seed ^ 0xFFFFFFFFu;
    for (std::size_t i = 0; i < length; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

void appendBigEndian(std::string& out, unsigned value)
{
    out.push_back(static_cast<char>((value >> 24) & 0xFF));
    out.push_back(static_cast<char>((value >> 16) & 0xFF));
    out.push_back(static_cast<char>((value >> 8) & 0xFF));
    out.push_back(static_cast<char>(value & 0xFF));
}

/// Write one PNG chunk: length, type, data, CRC over type and data.
void writeChunk(std::string& out, const char* type, const std::string& data)
{
    appendBigEndian(out, static_cast<unsigned>(data.size()));

    const std::size_t crcStart = out.size();
    out.append(type, 4);
    out.append(data);

    const unsigned crc = crc32(
        reinterpret_cast<const unsigned char*>(out.data() + crcStart),
        4 + data.size());
    appendBigEndian(out, crc);
}

// --- Adler-32, as zlib specifies ------------------------------------------

unsigned adler32(const unsigned char* data, std::size_t length)
{
    unsigned a = 1;
    unsigned b = 0;
    // 5552 is the most bytes that can be summed before the 32-bit accumulator
    // can overflow; batching in that size is the standard trick.
    constexpr std::size_t kBatch = 5552;

    std::size_t index = 0;
    while (index < length) {
        const std::size_t run = std::min(kBatch, length - index);
        for (std::size_t i = 0; i < run; ++i) {
            a += data[index + i];
            b += a;
        }
        a %= 65521;
        b %= 65521;
        index += run;
    }
    return (b << 16) | a;
}

/// Deflate with stored (uncompressed) blocks.
///
/// A valid zlib stream, and deliberately the simplest one that is correct: a
/// PNG's pixel data is already close to incompressible for a rendered frame
/// with film grain, and a real deflate is another few hundred lines to get
/// right. The frames are written to disk and immediately re-read by ffmpeg, so
/// the file size costs a little I/O and nothing else.
std::string deflateStored(const std::string& raw)
{
    std::string out;

    // zlib header: deflate, 32K window, no preset dictionary, fastest level.
    out.push_back(static_cast<char>(0x78));
    out.push_back(static_cast<char>(0x01));

    constexpr std::size_t kMaxBlock = 65535;
    std::size_t offset = 0;
    do {
        const std::size_t remaining = raw.size() - offset;
        const std::size_t block = std::min(kMaxBlock, remaining);
        const bool last = (offset + block) >= raw.size();

        out.push_back(static_cast<char>(last ? 1 : 0));

        // LEN and NLEN, little-endian. NLEN is the one's complement of LEN,
        // which is what makes a truncated stream detectable.
        out.push_back(static_cast<char>(block & 0xFF));
        out.push_back(static_cast<char>((block >> 8) & 0xFF));
        const unsigned complement = static_cast<unsigned>(~block) & 0xFFFF;
        out.push_back(static_cast<char>(complement & 0xFF));
        out.push_back(static_cast<char>((complement >> 8) & 0xFF));

        out.append(raw, offset, block);
        offset += block;
    } while (offset < raw.size());

    const unsigned checksum = adler32(
        reinterpret_cast<const unsigned char*>(raw.data()), raw.size());
    appendBigEndian(out, checksum);
    return out;
}

/// Build the filtered scanlines.
///
/// Filter type 1 (Sub) subtracts the pixel to the left. It costs one subtract
/// per channel and helps a great deal on flat areas, which a rendered frame
/// has many of — even with stored deflate, ffmpeg reads the file back faster
/// when the bytes are small.
std::string filterScanlines(const Frame& frame)
{
    const int bpp = 4;
    const std::size_t stride = static_cast<std::size_t>(frame.width) * bpp;

    std::string out;
    out.reserve((stride + 1) * static_cast<std::size_t>(frame.height));

    std::vector<unsigned char> previous(stride, 0);

    for (int y = 0; y < frame.height; ++y) {
        const unsigned char* row =
            frame.pixels.data() + static_cast<std::ptrdiff_t>(y) * stride;

        // Decide between None and Sub by which produces more zero bytes, which
        // is a cheap stand-in for which compresses better.
        std::size_t zeroIfSub = 0;
        for (std::size_t x = 0; x < stride; ++x) {
            const unsigned char left = x >= static_cast<std::size_t>(bpp)
                ? row[x - bpp] : 0;
            if (static_cast<unsigned char>(row[x] - left) == 0) ++zeroIfSub;
        }

        const bool useSub = zeroIfSub * 2 > stride;
        out.push_back(useSub ? 1 : 0);

        if (useSub) {
            for (std::size_t x = 0; x < stride; ++x) {
                const unsigned char left = x >= static_cast<std::size_t>(bpp)
                    ? row[x - bpp] : 0;
                out.push_back(static_cast<char>(row[x] - left));
            }
        } else {
            out.append(reinterpret_cast<const char*>(row), stride);
        }

        std::memcpy(previous.data(), row, stride);
    }

    return out;
}

} // namespace

bool encodePng(const Frame& frame, std::string& out, std::string& error)
{
    if (!frame.valid()) {
        error = "the frame is not a valid RGBA image";
        return false;
    }

    out.clear();

    // Signature.
    const unsigned char signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    out.append(reinterpret_cast<const char*>(signature), 8);

    // IHDR: width, height, 8 bits per channel, colour type 6 (RGBA),
    // deflate, adaptive filtering, no interlace.
    std::string header;
    appendBigEndian(header, static_cast<unsigned>(frame.width));
    appendBigEndian(header, static_cast<unsigned>(frame.height));
    header.push_back(8);  // bit depth
    header.push_back(6);  // colour type: truecolour with alpha
    header.push_back(0);  // compression method
    header.push_back(0);  // filter method
    header.push_back(0);  // interlace method
    writeChunk(out, "IHDR", header);

    // IDAT.
    const std::string filtered = filterScanlines(frame);
    writeChunk(out, "IDAT", deflateStored(filtered));

    // IEND.
    writeChunk(out, "IEND", {});

    return true;
}

bool writePng(const Frame& frame, const std::string& path, std::string& error)
{
    std::string bytes;
    if (!encodePng(frame, bytes, error)) return false;

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "cannot open " + path + " for writing";
        return false;
    }
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!file) {
        error = "failed while writing " + path;
        return false;
    }
    return true;
}

std::string base64Encode(const std::string& bytes)
{
    static const char* alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    out.reserve((bytes.size() + 2) / 3 * 4);

    std::size_t index = 0;
    while (index + 2 < bytes.size()) {
        const unsigned value =
            (static_cast<unsigned char>(bytes[index]) << 16)
            | (static_cast<unsigned char>(bytes[index + 1]) << 8)
            | static_cast<unsigned char>(bytes[index + 2]);
        out.push_back(alphabet[(value >> 18) & 0x3F]);
        out.push_back(alphabet[(value >> 12) & 0x3F]);
        out.push_back(alphabet[(value >> 6) & 0x3F]);
        out.push_back(alphabet[value & 0x3F]);
        index += 3;
    }

    const std::size_t remaining = bytes.size() - index;
    if (remaining == 1) {
        const unsigned value = static_cast<unsigned char>(bytes[index]) << 16;
        out.push_back(alphabet[(value >> 18) & 0x3F]);
        out.push_back(alphabet[(value >> 12) & 0x3F]);
        out.push_back('=');
        out.push_back('=');
    } else if (remaining == 2) {
        const unsigned value =
            (static_cast<unsigned char>(bytes[index]) << 16)
            | (static_cast<unsigned char>(bytes[index + 1]) << 8);
        out.push_back(alphabet[(value >> 18) & 0x3F]);
        out.push_back(alphabet[(value >> 12) & 0x3F]);
        out.push_back(alphabet[(value >> 6) & 0x3F]);
        out.push_back('=');
    }
    return out;
}

std::string encodePngDataUrl(const Frame& frame)
{
    std::string bytes;
    std::string error;
    if (!encodePng(frame, bytes, error)) return {};
    return "data:image/png;base64," + base64Encode(bytes);
}

} // namespace keyflow
