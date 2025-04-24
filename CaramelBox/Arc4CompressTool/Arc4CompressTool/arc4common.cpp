#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <iostream>
#include <vector>
#include <boost/endian.hpp>
#include "arc4common.h"


std::string get_file_prefix(const std::string& filename) {
    std::string temp(filename);

    std::string::size_type pos = temp.find_last_of(".");

  if (pos != std::string::npos) {
    temp = temp.substr(0, pos);
  }

  return temp;
}

std::string get_file_extension(const std::string& filename) {
    std::string temp;

    std::string::size_type pos = filename.find_last_of(".");

  if (pos != std::string::npos) {
    temp = filename.substr(pos + 1);
  }

  return temp;
}

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

std::vector<uint8_t> compress_sequence(std::vector<uint8_t>& data)
{
    std::vector<uint8_t> result;
    ARC4COMPHDR hdr;
    hdr.type = 0x5a74;
    hdr.length = data.size();
    result.insert(result.end(), (uint8_t*) & hdr, ((uint8_t*) & hdr) + sizeof(hdr));
    size_t pos = 0;
    while (pos < data.size()) {
        size_t chunckSize = std::min<size_t>(0x8000, data.size() - pos);
        std::vector<uint8_t> chunckData(data.begin() + pos, data.begin() + pos + chunckSize);
        ARC4COMPCHUNKHDR chunckHdr;
        chunckHdr.type = 0x7453;
        chunckHdr.length = (chunckSize % 2) ? chunckSize + 1 : chunckSize;
        chunckHdr.original_length = chunckSize;
        if (chunckSize % 2)chunckData.push_back(0);
        chunckHdr.seed = obfuscate(chunckData.data(), chunckData.size());
        result.insert(result.end(), (uint8_t*)&chunckHdr, ((uint8_t*)&chunckHdr) + sizeof(chunckHdr));
        result.insert(result.end(), chunckData.begin(), chunckData.end());
        pos += chunckSize;
    }
    return result;
}
std::vector<uint8_t> uncompress_sequence(std::vector<uint8_t>& data, const std::string& filename)
{
    std::vector<uint8_t> result;
    if (*(uint16_t*)&data[6] != 0x745A && *(uint16_t*)&data[6] != 0x7453)return result;
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
            fprintf(stderr, "%s: unknown chunk type: 0x%04X\n",
                filename.c_str(), chunk->type);
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
