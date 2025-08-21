#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cstring>
#include <boost/endian.hpp>
#include "arc4common.h"

void unobfuscate(uint8_t* buff,
                 uint32_t  len, 
                 uint32_t seed)
{
    uint16_t* p   = (uint16_t*) buff;

  uint32_t crc = seed;
  uint32_t c = 0;
  for (int i = 0; i < len / 2; i++) {
      crc = 1336793 * crc + 4021;
      p[i] -= (WORD)(crc >> 16);
      c = p[i] + 3 * c;
  }

  if ((c % 0xFFF1) != seed) {
      std::cout << "Warning: crc wrong!" << std::endl;
  }
}

uint16_t obfuscate(uint8_t* buff, uint32_t len)
{
    uint16_t* p = (uint16_t*)buff;

    uint32_t c = 0;
    for (int i = 0; i < len / 2; i++) {
        c = p[i] + 3 * c;
    }
    uint32_t seed = c % 0xFFF1;
    uint16_t result = seed;

    for (int i = 0; i < len / 2; i++) {
        seed = 1336793 * seed + 4021;
        p[i] += (WORD)(seed >> 16);
    }
    return result;
}

uint32_t uncompress(uint8_t* buff,
    uint32_t  len,
    uint8_t* out_buff,
    uint32_t out_len)
{
    uint8_t* end     = buff + len;
    uint8_t* out_end = out_buff + out_len;

  while (true) {
      uint32_t  c      = *buff++;
      uint32_t  p      = 0;
      uint32_t  n      = 0;
      uint8_t* source = NULL;

    if (c & 0x80) {
      if (c & 0x40) {
        if (c & 0x20) {
          c = (c << 8) | *buff++;
          c = (c << 8) | *buff++;

          n = (c & 0x3F) + 4;
          p = (c >> 6) & 0x7FFF;
        } else {
          c = (c << 8) | *buff++;

          n = (c & 0x07) + 3;
          p = (c >> 3) & 0x3FF;
        }
      } else {
        n = (c & 0x03) + 2;
        p = (c >> 2) & 0x0F;
      }

      source = out_buff - p - 1;
    } else {
      n      = c;
      p      = 0;
      source = buff;

      buff += n;

      if (!n) {
        break;
      }
    }

    while (n--) {
      *out_buff++ = *source++;
    }
  }

  return out_len - (out_end - out_buff);
}

struct Match {
    uint32_t offset = 0;
    uint32_t len = 0;
};

// Finds the best match for the current position in the lookbehind buffer
void find_best_match(const uint8_t* data, uint32_t current_pos, uint32_t total_len, Match& best_match) {
    best_match.len = 0;
    best_match.offset = 0;

    const uint32_t max_len = std::min<uint32_t>((uint32_t)67, total_len - current_pos);
    if (max_len < 2) return;

    const uint32_t max_offset = 32768;
    const uint32_t window_start = (current_pos > max_offset) ? (current_pos - max_offset) : 0;

    for (uint32_t pos = window_start; pos < current_pos; ++pos) {
        uint32_t current_len = 0;
        while (current_len < max_len && data[pos + current_len] == data[current_pos + current_len]) {
            current_len++;
        }

        if (current_len > best_match.len) {
            best_match.len = current_len;
            best_match.offset = current_pos - pos;
        }
    }
}
uint32_t compress(uint8_t* buff,
    uint32_t  len,
    uint8_t* out_buff,
    uint32_t out_len)
{
    uint8_t* out_ptr = out_buff;
    uint8_t* out_end = out_buff + out_len;
    uint32_t current_pos = 0;

    std::vector<uint8_t> literals;

    auto flush_literals = [&]() {
        if (literals.empty()) return false;

        uint32_t num_literals = literals.size();
        uint32_t pos = 0;
        while (pos < num_literals) {
            uint32_t chunk_size = std::min<uint32_t>((uint32_t)127, num_literals - pos);
            if (out_ptr + 1 + chunk_size > out_end) return true; // Error: out of space

            *out_ptr++ = chunk_size;
            memcpy(out_ptr, literals.data() + pos, chunk_size);
            out_ptr += chunk_size;
            pos += chunk_size;
        }
        literals.clear();
        return false;
        };

    while (current_pos < len) {
        Match best_match;
        find_best_match(buff, current_pos, len, best_match);

        // We need a match of at least 2 to be worth encoding.
        // A match of 2 can be encoded in 1 byte, saving 1 byte.
        // A match of 3 can be encoded in 2 bytes, saving 1 byte.
        // Let's use a simple threshold: if a match exists, use it.
        bool use_match = (best_match.len >= 2);

        // Check which encoding is possible and if it's better than literals
        if (use_match) {
            uint32_t p = best_match.offset - 1;
            uint32_t n = best_match.len;

            // Try to fit into the smallest possible encoding
            if (n >= 2 && n <= 5 && p < 16) {
                // Use 1-byte encoding, which is always a gain
            }
            else if (n >= 3 && n <= 10 && p < 1024) {
                // Use 2-byte encoding. Gain if n > 2.
            }
            else if (n >= 4 && n <= 67 && p < 32768) {
                // Use 3-byte encoding. Gain if n > 3.
            }
            else {
                use_match = false; // Match doesn't fit any format or is not efficient
            }
        }


        if (use_match) {
            if (flush_literals()) return 0; // Error

            uint32_t p = best_match.offset - 1;
            uint32_t n = best_match.len;

            // Encode the match based on its length and offset
            if (n >= 2 && n <= 5 && p < 16) {
                if (out_ptr + 1 > out_end) return 0; // Error
                *out_ptr++ = 0x80 | (p << 2) | (n - 2);
            }
            else if (n >= 3 && n <= 10 && p < 1024) {
                if (out_ptr + 2 > out_end) return 0; // Error
                uint16_t code = 0xC000 | (p << 3) | (n - 3);
                *out_ptr++ = (code >> 8);
                *out_ptr++ = (code & 0xFF);
            }
            else if (n >= 4 && n <= 67 && p < 32768) {
                if (out_ptr + 3 > out_end) return 0; // Error
                uint32_t code = 0xE00000 | (p << 6) | (n - 4);
                *out_ptr++ = (code >> 16);
                *out_ptr++ = (code >> 8) & 0xFF;
                *out_ptr++ = (code & 0xFF);
            }

            current_pos += best_match.len;

        }
        else {
            // No good match found, add to literals
            literals.push_back(buff[current_pos]);
            current_pos++;

            // Flush if literals buffer is full
            if (literals.size() == 127) {
                if (flush_literals()) return 0; // Error
            }
        }
    }

    // Flush any remaining literals
    if (flush_literals()) return 0; // Error

    // Write end of stream marker
    if (out_ptr + 1 > out_end) return 0; // Error
    *out_ptr++ = 0x00;

    return out_ptr - out_buff;
}

std::vector<uint8_t> compress(const uint8_t* in_buff, uint32_t in_len)
{
    std::vector<uint8_t> result(in_len * 2);
    uint32_t out_len = compress((uint8_t*)in_buff, in_len, result.data(), result.size());
    if (out_len == 0) {
        throw std::runtime_error("Compression failed");
    }
    result.resize(out_len);
    return result;
}


std::vector<uint8_t> compress_sequence(std::vector<uint8_t>& data, uint32_t maxChunckSize, bool compress_type)
{
    std::vector<uint8_t> result;
    ARC4COMPHDR hdr;
    hdr.type = 0x5A74; // 'tZ'
    hdr.length = data.size();

    result.insert(result.end(), (uint8_t*)&hdr, ((uint8_t*)&hdr) + sizeof(hdr));

    size_t pos = 0;
    while (pos < data.size()) {
        size_t chunkSize = std::min<size_t>(maxChunckSize, data.size() - pos);

        ARC4COMPCHUNKHDR chunkHdr;
        chunkHdr.original_length = chunkSize;

        std::vector<uint8_t> chunkDataToWrite;

        if (!compress_type) {
            chunkHdr.type = 0x7453; // 'St'
            chunkDataToWrite.assign(data.begin() + pos, data.begin() + pos + chunkSize);
        }
        else {
            chunkHdr.type = 0x745A; // 'Zt'
            chunkDataToWrite = compress(data.data() + pos, chunkSize);
        }

        if (chunkDataToWrite.size() % 2 != 0) {
            chunkDataToWrite.push_back(0);
        }

        chunkHdr.length = chunkDataToWrite.size();

        chunkHdr.seed = obfuscate(chunkDataToWrite.data(), chunkDataToWrite.size());

        result.insert(result.end(), (uint8_t*)&chunkHdr, ((uint8_t*)&chunkHdr) + sizeof(chunkHdr));
        result.insert(result.end(), chunkDataToWrite.begin(), chunkDataToWrite.end());

        pos += chunkSize;
    }
    return result;
}
std::vector<uint8_t> uncompress_sequence(std::vector<uint8_t>& data)
{
    std::vector<uint8_t> result;
    if (*(uint16_t*)&data[6] != 0x745A && *(uint16_t*)&data[6] != 0x7453) {
        throw std::runtime_error("Invalid chunk typeF");
    }
    ARC4COMPHDR* hdr = (ARC4COMPHDR*)data.data();
    uint8_t* buff = data.data();
    buff += sizeof(*hdr);
    uint32_t len = data.size();
    result.resize(hdr->length);
    uint8_t* out_buff = result.data();
    uint32_t out_len = result.size();

    uint8_t* end = buff + len;
    uint8_t* out = out_buff;
    uint8_t* out_end = out_buff + out_len;

    static int count = 0;
    while (buff < end && out < out_end) {
        ARC4COMPCHUNKHDR* chunk = (ARC4COMPCHUNKHDR*)buff;
        buff += sizeof(*chunk);

        unobfuscate(buff, chunk->length, chunk->seed);

        if (chunk->type == 0x745A) {
            uncompress(buff, chunk->length, out, chunk->original_length);
        }
        else if (chunk->type == 0x7453) {
            memcpy(out, buff, chunk->original_length);
        }
        else {
            throw std::runtime_error("Invalid chunk typeS");
        }

        buff += chunk->length;
        out += chunk->original_length;
    }
    return result;
}

uint32_t get_stupid_long(uint8_t* buff) {
  return (buff[0] << 16) | (buff[1] << 8) | buff[2];
}

void write_stupid_long(uint8_t* buff, size_t val) {
    boost::endian::native_to_big_inplace(val);
    memcpy(buff, ((uint8_t*)&val) + sizeof(val) - 3, 3);
}
