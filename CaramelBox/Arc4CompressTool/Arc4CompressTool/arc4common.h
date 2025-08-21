#ifndef __ARC4COMMON_H__
#define __ARC4COMMON_H__

#pragma pack(1)
struct ARC4COMPHDR {
  uint16_t type; // 0x5A74
  uint32_t  length;
};

struct ARC4COMPCHUNKHDR {
	uint16_t type; // 0x745A or 0x7453
	uint16_t length;
	uint16_t original_length;
	uint16_t seed;
};

struct EVEMHDR {
	uint32_t magic; // 0x4D455645
	uint32_t unknown; // 0x100
	uint32_t reserved; // 0x0
	uint32_t scriptBegin;
	uint32_t scriptLength;
};
#pragma pack()

std::vector<uint8_t> uncompress_sequence(std::vector<uint8_t>& data);
uint32_t get_stupid_long(uint8_t* buff);
std::vector<uint8_t> compress_sequence(std::vector<uint8_t>& data, uint32_t maxChunckSize, bool compress_type);
void write_stupid_long(uint8_t* buff, size_t val);

#endif
