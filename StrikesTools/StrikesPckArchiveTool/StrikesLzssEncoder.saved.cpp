// Saved Strikes AVGDatas.pck LZSS encoder draft.
//
// This file is intentionally not listed in the vcxproj, so it is not built or
// enabled by the current rebuilder. It is kept here only as a reference for the
// later compressed-rebuild investigation.
//
// Decoder facts:
// - Ring size: 0x1000.
// - Initial write position: 0xFEE.
// - Control bit 1 = literal.
// - Control bit 0 = backreference.
// - Backreference encoding:
//   offset = lo | ((hi & 0xF0) << 4)
//   count = (hi & 0x0F) + 3
//
// Important compressor pitfall:
// When measuring a candidate match, simulate decode-time overlapping copy.
// A valid match can read bytes that were produced earlier in the same match,
// because the decoder writes each byte back into the ring immediately.

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace StrikesSavedLzss {

    struct Match {
        int offset{};
        int length{};
    };

    [[nodiscard]] inline int measureMatch(const std::vector<uint8_t>& input, size_t src, int candidateOffset, const std::array<uint8_t, 0x1000>& frame)
    {
        int maxLength = (int)std::min<size_t>(18, input.size() - src);
        std::array<uint8_t, 0x1000> simulatedFrame = frame;

        int length = 0;
        while (length < maxLength) {
            uint8_t value = simulatedFrame[(candidateOffset + length) & 0xFFF];
            if (value != input[src + (size_t)length]) {
                break;
            }

            // This mirrors the decoder's immediate writeback and is the part
            // the first broken encoder was missing.
            simulatedFrame[(0xFEE + (int)src + length) & 0xFFF] = value;
            ++length;
        }

        return length;
    }

    [[nodiscard]] inline Match findMatch(const std::vector<uint8_t>& input, size_t src, const std::array<uint8_t, 0x1000>& frame)
    {
        Match best{};
        for (int offset = 0; offset < 0x1000; ++offset) {
            int length = measureMatch(input, src, offset, frame);
            if (length >= best.length) {
                best.offset = offset;
                best.length = length;
            }
        }

        if (best.length < 3) {
            best.length = 0;
        }
        return best;
    }

    [[nodiscard]] inline std::vector<uint8_t> pack(const std::vector<uint8_t>& input)
    {
        std::vector<uint8_t> output;
        output.reserve(input.size());

        std::array<uint8_t, 0x1000> frame{};
        int framePos = 0xFEE;

        size_t src = 0;
        while (src < input.size()) {
            size_t controlOffset = output.size();
            output.push_back(0);
            uint8_t control = 0;

            for (int bit = 0; bit < 8 && src < input.size(); ++bit) {
                Match match = findMatch(input, src, frame);
                if (match.length >= 3) {
                    output.push_back((uint8_t)match.offset);
                    output.push_back((uint8_t)(((match.offset >> 4) & 0xF0) | (match.length - 3)));

                    for (int i = 0; i < match.length; ++i) {
                        frame[(framePos++) & 0xFFF] = input[src++];
                    }
                }
                else {
                    control |= (uint8_t)(1u << bit);
                    uint8_t value = input[src++];
                    output.push_back(value);
                    frame[(framePos++) & 0xFFF] = value;
                }
            }

            output[controlOffset] = control;
        }

        return output;
    }
}
