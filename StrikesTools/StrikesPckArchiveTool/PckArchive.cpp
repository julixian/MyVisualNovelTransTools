module;

#include <Windows.h>
#include <cstdint>

module PckArchive;

import std;
import Tool;

namespace fs = std::filesystem;

namespace {

    constexpr uint32_t headerXor = 0xDEFD32D3u;
    constexpr uint32_t blockXor = 0xC53A9A6Cu;
    constexpr uint32_t smallBlockXor = 0x6C9A3AC5u;

    struct RawGroupInfo {
        uint32_t groupIndex{};
        uint32_t metadataSize{};
        uint32_t entryCount{};
        uint32_t groupKey{};
        std::array<uint8_t, 16> headerBytes{};
    };

    struct RawEntryInfo {
        uint32_t groupIndex{};
        uint32_t entryIndex{};
        std::string nameCp932;
        std::wstring nameWide;
        uint32_t offset{};
        uint32_t packedSize{};
        uint32_t unpackedSize{};
        bool isEncrypted{};
        bool isPacked{};
        uint32_t groupKey{};
    };

    struct ReplacementEntryData {
        std::vector<uint8_t> bytes;
        bool replaced{};
    };

    struct ArchiveState {
        fs::path packagePath;
        std::vector<uint8_t> rawData;
        std::vector<uint8_t> decodedConfigHeader;
        Strikes::PckConfigInfo configInfo;
        std::vector<RawGroupInfo> rawGroups;
        std::vector<RawEntryInfo> rawEntries;
        std::vector<Strikes::PckGroupInfo> groupInfos;
        std::vector<Strikes::PckEntry> entries;
    };

    uint32_t readBe32(const std::vector<uint8_t>& data, size_t offset)
    {
        if (offset + 4 > data.size()) {
            throw std::runtime_error("unexpected EOF while reading big-endian u32");
        }
        return ((uint32_t)data[offset] << 24) |
            ((uint32_t)data[offset + 1] << 16) |
            ((uint32_t)data[offset + 2] << 8) |
            (uint32_t)data[offset + 3];
    }

    uint32_t readLe32(const std::vector<uint8_t>& data, size_t offset)
    {
        if (offset + 4 > data.size()) {
            throw std::runtime_error("unexpected EOF while reading little-endian u32");
        }
        return (uint32_t)data[offset] |
            ((uint32_t)data[offset + 1] << 8) |
            ((uint32_t)data[offset + 2] << 16) |
            ((uint32_t)data[offset + 3] << 24);
    }

    uint32_t readBe31(const std::vector<uint8_t>& data, size_t offset)
    {
        if (offset + 4 > data.size()) {
            throw std::runtime_error("unexpected EOF while reading packed 31-bit integer");
        }
        return (((uint32_t)data[offset] & 0x7Fu) << 24) |
            ((uint32_t)data[offset + 1] << 16) |
            ((uint32_t)data[offset + 2] << 8) |
            (uint32_t)data[offset + 3];
    }

    void writeBe32(std::vector<uint8_t>& data, size_t offset, uint32_t value)
    {
        if (offset + 4 > data.size()) {
            throw std::runtime_error("unexpected EOF while writing big-endian u32");
        }
        data[offset] = (uint8_t)(value >> 24);
        data[offset + 1] = (uint8_t)(value >> 16);
        data[offset + 2] = (uint8_t)(value >> 8);
        data[offset + 3] = (uint8_t)value;
    }

    void writeLe32(std::vector<uint8_t>& data, size_t offset, uint32_t value)
    {
        if (offset + 4 > data.size()) {
            throw std::runtime_error("unexpected EOF while writing little-endian u32");
        }
        data[offset] = (uint8_t)value;
        data[offset + 1] = (uint8_t)(value >> 8);
        data[offset + 2] = (uint8_t)(value >> 16);
        data[offset + 3] = (uint8_t)(value >> 24);
    }

    void appendBe32(std::vector<uint8_t>& data, uint32_t value)
    {
        data.push_back((uint8_t)(value >> 24));
        data.push_back((uint8_t)(value >> 16));
        data.push_back((uint8_t)(value >> 8));
        data.push_back((uint8_t)value);
    }

    uint32_t byteSwap32(uint32_t value)
    {
        return ((value & 0x000000FFu) << 24) |
            ((value & 0x0000FF00u) << 8) |
            ((value & 0x00FF0000u) >> 8) |
            ((value & 0xFF000000u) >> 24);
    }

    std::vector<uint8_t> readBinaryFile(const fs::path& filePath)
    {
        std::ifstream input(filePath, std::ios::binary);
        if (!input.is_open()) {
            throw std::runtime_error(std::format("failed to open input file: {}", wide2Ascii(filePath.native(), CP_UTF8)));
        }

        input.seekg(0, std::ios::end);
        auto size = input.tellg();
        if (size < 0) {
            throw std::runtime_error(std::format("failed to determine input file size: {}", wide2Ascii(filePath.native(), CP_UTF8)));
        }
        input.seekg(0, std::ios::beg);

        std::vector<uint8_t> data((size_t)size);
        if (!data.empty()) {
            input.read((char*)data.data(), (std::streamsize)data.size());
            if (!input) {
                throw std::runtime_error(std::format("failed to read input file: {}", wide2Ascii(filePath.native(), CP_UTF8)));
            }
        }
        return data;
    }

    void writeBinaryFile(const fs::path& filePath, const std::vector<uint8_t>& data)
    {
        if (!filePath.parent_path().empty()) {
            fs::create_directories(filePath.parent_path());
        }

        std::ofstream output(filePath, std::ios::binary | std::ios::trunc);
        if (!output.is_open()) {
            throw std::runtime_error(std::format("failed to create output file: {}", wide2Ascii(filePath.native(), CP_UTF8)));
        }

        if (!data.empty()) {
            output.write((const char*)data.data(), (std::streamsize)data.size());
        }
        if (!output) {
            throw std::runtime_error(std::format("failed to write output file: {}", wide2Ascii(filePath.native(), CP_UTF8)));
        }
    }

    std::string trimCp932CString(const uint8_t* data, size_t maxLength)
    {
        size_t length = 0;
        while (length < maxLength && data[length] != 0) {
            ++length;
        }
        return std::string((const char*)data, (const char*)data + length);
    }

    std::wstring sanitizeFileName(std::wstring_view fileName)
    {
        if (fileName.empty()) {
            return L"_empty_name";
        }

        std::wstring sanitized(fileName);
        for (auto& character : sanitized) {
            if (character < 0x20 ||
                character == L':' ||
                character == L'*' ||
                character == L'?' ||
                character == L'"' ||
                character == L'<' ||
                character == L'>' ||
                character == L'|' ||
                character == L'/' ||
                character == L'\\') {
                character = L'_';
            }
        }

        if (sanitized == L"." || sanitized == L"..") {
            sanitized.insert(sanitized.begin(), L'_');
        }
        return sanitized;
    }

    struct KnuthRng {
        std::array<uint32_t, 57> state{};

        explicit KnuthRng(uint32_t seed)
        {
            state[56] = seed;
            uint32_t previous = 1;
            uint32_t current = seed;
            for (int i = 1; i <= 54; ++i) {
                int index = ((21 * i) % 55) + 1;
                state[(size_t)index] = previous;
                int64_t next = (int64_t)current - (int64_t)previous;
                if (next < 0) {
                    next += 1000000000;
                }
                previous = (uint32_t)next;
                current = state[(size_t)index];
            }
            mix();
            mix();
            mix();
            state[0] = 55;
        }

        void mix()
        {
            for (int position = 2; position <= 25; ++position) {
                int64_t value = (int64_t)state[(size_t)position] - (int64_t)state[(size_t)(position + 31)];
                if (value < 0) {
                    value += 1000000000;
                }
                state[(size_t)position] = (uint32_t)value;
            }
            for (int position = 26; position <= 56; ++position) {
                int64_t value = (int64_t)state[(size_t)position] - (int64_t)state[(size_t)(position - 24)];
                if (value < 0) {
                    value += 1000000000;
                }
                state[(size_t)position] = (uint32_t)value;
            }
        }

        uint32_t next()
        {
            if (++state[0] > 55) {
                mix();
                state[0] = 1;
            }
            return state[(size_t)state[0] + 1];
        }
    };

    std::vector<uint8_t> lzssDecompress(const std::vector<uint8_t>& input, uint32_t expectedSize, bool strictSize = false)
    {
        std::vector<uint8_t> output;
        output.reserve((size_t)std::min(expectedSize, 16u * 1024u * 1024u));
        std::array<uint8_t, 4096> ring{};
        uint16_t ringPosition = 4078;
        uint16_t flags = 0;
        size_t sourceOffset = 0;

        while (sourceOffset < input.size() && output.size() < expectedSize) {
            flags >>= 1;
            if ((flags & 0x100u) == 0) {
                flags = (uint16_t)(input[sourceOffset++] | 0xFF00u);
                if (sourceOffset > input.size()) {
                    break;
                }
            }

            if ((flags & 1u) != 0) {
                if (sourceOffset >= input.size()) {
                    break;
                }
                uint8_t value = input[sourceOffset++];
                output.push_back(value);
                ring[(size_t)ringPosition] = value;
                ringPosition = (uint16_t)((ringPosition + 1) & 0x0FFFu);
            }
            else {
                if (sourceOffset + 1 >= input.size()) {
                    break;
                }
                uint8_t lo = input[sourceOffset++];
                uint8_t hi = input[sourceOffset++];
                uint16_t copyPosition = (uint16_t)(lo | ((hi & 0xF0u) << 4));
                uint16_t copyLength = (uint16_t)((hi & 0x0Fu) + 3);
                for (uint16_t i = 0; i < copyLength && output.size() < expectedSize; ++i) {
                    uint8_t value = ring[(size_t)((copyPosition + i) & 0x0FFFu)];
                    output.push_back(value);
                    ring[(size_t)ringPosition] = value;
                    ringPosition = (uint16_t)((ringPosition + 1) & 0x0FFFu);
                }
            }
        }

        if (strictSize && output.size() != expectedSize) {
            throw std::runtime_error(std::format("strict LZSS decompression produced {} bytes, expected {}", output.size(), expectedSize));
        }
        return output;
    }

    void xorPrefixDwordwise(std::vector<uint8_t>& data, size_t length, uint32_t key)
    {
        size_t actualLength = std::min(length, data.size());
        for (size_t offset = 0; offset + 4 <= actualLength; offset += 4) {
            uint32_t value = readLe32(data, offset) ^ key;
            writeLe32(data, offset, value);
        }
    }

    void xorPrefixBytewise(std::vector<uint8_t>& data, size_t length, uint32_t key)
    {
        size_t actualLength = std::min(length, data.size());
        std::array<uint8_t, 4> keyBytes = {
            (uint8_t)key,
            (uint8_t)(key >> 8),
            (uint8_t)(key >> 16),
            (uint8_t)(key >> 24),
        };
        for (size_t i = 0; i < actualLength; ++i) {
            data[i] ^= keyBytes[i & 3u];
        }
    }

    int detectResourceSkipWords(const std::vector<uint8_t>& packed, uint32_t packedSize)
    {
        for (int skipWords = 8; skipWords > 0; --skipWords) {
            size_t testOffset = (size_t)skipWords * 4;
            if (testOffset + 4 > packed.size()) {
                continue;
            }
            uint32_t testSize = readBe32(packed, testOffset);
            if (testSize + 4 == packedSize) {
                return skipWords;
            }
        }
        return 0;
    }

    int computeResourceSkipWords(uint8_t groupBase, uint32_t entryIndex)
    {
        return 8 - (((int)groupBase + (int)entryIndex) & 7);
    }

    std::vector<uint8_t> readResourceChunk(const std::vector<uint8_t>& packed, int skipWords, uint32_t* chunkSize)
    {
        if (packed.size() < 4) {
            throw std::runtime_error("resource chunk is too small");
        }

        if (skipWords == 0) {
            if (chunkSize != nullptr) {
                *chunkSize = (uint32_t)packed.size();
            }
            return packed;
        }

        size_t skipBytes = (size_t)skipWords * 4;
        if (skipBytes + 4 > packed.size()) {
            throw std::runtime_error("resource chunk skip exceeds packed size");
        }

        uint32_t header = readLe32(packed, 0);
        uint32_t storedSize = readBe32(packed, skipBytes);
        uint32_t payloadSize = storedSize & 0x7FFFFFFFu;
        if (4ull + payloadSize > packed.size()) {
            throw std::runtime_error("resource chunk payload exceeds packed size");
        }

        std::vector<uint8_t> chunk(packed.begin() + 4, packed.begin() + 4 + (ptrdiff_t)payloadSize);
        std::memmove(chunk.data() + 4, chunk.data(), skipBytes - 4);
        writeLe32(chunk, 0, header);
        if (chunkSize != nullptr) {
            *chunkSize = storedSize;
        }
        return chunk;
    }

    std::vector<uint8_t> decodeIndexedBlock(const std::vector<uint8_t>& rawData, uint32_t tableOffset, uint32_t index, uint32_t expectedUnpackedSize)
    {
        size_t entryOffset = (size_t)tableOffset + (size_t)index * 4;
        if (entryOffset + 4 > rawData.size() || (size_t)tableOffset + 4 > rawData.size()) {
            throw std::runtime_error("indexed block table points outside the package");
        }

        uint32_t entry = readBe32(rawData, entryOffset);
        bool isCompressed = (entry & 0x80000000u) != 0;
        uint32_t packedSize = entry & 0x7FFFFFFFu;
        size_t payloadOffset = (size_t)tableOffset + 4;
        if (payloadOffset + packedSize > rawData.size()) {
            throw std::runtime_error("indexed block payload points outside the package");
        }

        std::vector<uint8_t> packed(rawData.begin() + (ptrdiff_t)payloadOffset, rawData.begin() + (ptrdiff_t)(payloadOffset + packedSize));
        if (index != 0) {
            size_t shiftBytes = (size_t)index * 4;
            if (shiftBytes > packed.size()) {
                throw std::runtime_error("indexed block shift exceeds block size");
            }
            std::array<uint8_t, 4> firstWord = {
                rawData[(size_t)tableOffset],
                rawData[(size_t)tableOffset + 1],
                rawData[(size_t)tableOffset + 2],
                rawData[(size_t)tableOffset + 3],
            };
            std::memmove(packed.data() + 4, packed.data(), shiftBytes - 4);
            std::copy(firstWord.begin(), firstWord.end(), packed.begin());
        }

        if (!isCompressed) {
            if (packed.size() != expectedUnpackedSize) {
                throw std::runtime_error("indexed block size mismatch");
            }
            return packed;
        }
        return lzssDecompress(packed, expectedUnpackedSize, true);
    }

    std::vector<uint8_t> decodeConfigHeader(ArchiveState& state)
    {
        auto& rawData = state.rawData;
        auto& config = state.configInfo;

        config.firstBlockSize = readBe31(rawData, 4);
        config.firstPayloadOffset = config.firstBlockSize + 4;

        config.secondBlockSize = readBe31(rawData, (size_t)config.firstBlockSize + 16);
        config.secondTableOffset = config.secondBlockSize + config.firstPayloadOffset + 4;

        config.thirdBlockSize = readBe31(rawData, (size_t)config.secondTableOffset + 8);
        config.thirdTableOffset = config.thirdBlockSize + config.secondTableOffset + 4;

        config.configOffset = readBe32(rawData, config.thirdTableOffset);
        if ((uint64_t)config.configOffset + 0x68u > rawData.size()) {
            throw std::runtime_error("config header points outside the package");
        }

        std::vector<uint8_t> header(rawData.begin() + (ptrdiff_t)config.configOffset, rawData.begin() + (ptrdiff_t)(config.configOffset + 0x68));
        uint32_t seed = readBe32(header, 0);
        KnuthRng rng(seed);

        for (size_t word = 1; word < 26; ++word) {
            size_t offset = word * 4;
            uint32_t value = readLe32(header, offset) ^ byteSwap32(rng.next());
            writeLe32(header, offset, value);
        }

        for (size_t offset : { 0u, 8u, 12u, 16u, 20u, 24u, 28u, 32u, 36u, 60u }) {
            std::reverse(header.begin() + (ptrdiff_t)offset, header.begin() + (ptrdiff_t)(offset + 4));
        }
        for (size_t offset : { 44u, 46u, 48u, 50u, 56u }) {
            std::reverse(header.begin() + (ptrdiff_t)offset, header.begin() + (ptrdiff_t)(offset + 2));
        }

        uint32_t checkInput =
            (uint32_t)header[5] |
            ((uint32_t)header[6] << 8) |
            ((uint32_t)header[4] << 16) |
            ((uint32_t)header[7] << 24);
        uint32_t storedCheck = readLe32(header, 28);
        if ((checkInput ^ headerXor) != storedCheck) {
            throw std::runtime_error("config header checksum mismatch");
        }

        config.groupSizeTable = {
            readLe32(header, 0x08),
            readLe32(header, 0x0C),
            readLe32(header, 0x10),
            readLe32(header, 0x14),
            readLe32(header, 0x18),
            readLe32(header, 0x28),
            readLe32(header, 0x24),
            readLe32(header, 0x34),
            readLe32(header, 0x44),
        };
        config.metaSizeOffset = readLe32(header, 0x20);
        config.metaUnpackedSize = readBe32(rawData, config.metaSizeOffset);
        config.metaTableOffset = config.metaSizeOffset + 4;

        return header;
    }

    void parseMetadata(ArchiveState& state)
    {
        state.decodedConfigHeader = decodeConfigHeader(state);
        std::vector<uint8_t> metadata = decodeIndexedBlock(state.rawData, state.configInfo.metaTableOffset, 8, state.configInfo.metaUnpackedSize);

        uint32_t metadataOffset = 0;
        for (size_t groupIndex = 0; groupIndex < state.configInfo.groupSizeTable.size(); ++groupIndex) {
            uint32_t groupSize = state.configInfo.groupSizeTable[groupIndex];
            if (groupSize == 0) {
                continue;
            }

            if ((uint64_t)metadataOffset + groupSize > metadata.size()) {
                throw std::runtime_error("group metadata exceeds decoded metadata size");
            }

            const uint8_t* group = metadata.data() + metadataOffset;
            if (groupSize < 16) {
                throw std::runtime_error("group metadata is too small");
            }

            RawGroupInfo rawGroup{};
            rawGroup.groupIndex = (uint32_t)groupIndex;
            rawGroup.metadataSize = groupSize;
            rawGroup.entryCount =
                ((uint32_t)group[12] << 24) |
                ((uint32_t)group[13] << 16) |
                ((uint32_t)group[14] << 8) |
                (uint32_t)group[15];
            rawGroup.groupKey =
                ((uint32_t)group[8] << 24) |
                ((uint32_t)group[9] << 16) |
                ((uint32_t)group[10] << 8) |
                (uint32_t)group[11];
            std::copy(group, group + 16, rawGroup.headerBytes.begin());
            state.rawGroups.push_back(rawGroup);
            state.groupInfos.push_back(Strikes::PckGroupInfo{
                .groupIndex = rawGroup.groupIndex,
                .metadataSize = rawGroup.metadataSize,
                .entryCount = rawGroup.entryCount,
                .groupKey = rawGroup.groupKey,
                .groupBase = (uint8_t)(rawGroup.groupKey & 0xFFu),
            });

            uint64_t requiredSize = 16ull + (uint64_t)rawGroup.entryCount * 56ull;
            if (requiredSize > groupSize) {
                throw std::runtime_error("group entry table exceeds group metadata size");
            }

            for (uint32_t entryIndex = 0; entryIndex < rawGroup.entryCount; ++entryIndex) {
                const uint8_t* entry = group + 16 + (size_t)entryIndex * 56;
                RawEntryInfo rawEntry{};
                rawEntry.groupIndex = rawGroup.groupIndex;
                rawEntry.entryIndex = entryIndex;
                rawEntry.nameCp932 = trimCp932CString(entry, 40);
                rawEntry.nameWide = ascii2Wide(rawEntry.nameCp932, 932);
                rawEntry.offset =
                    ((uint32_t)entry[40] << 24) |
                    ((uint32_t)entry[41] << 16) |
                    ((uint32_t)entry[42] << 8) |
                    (uint32_t)entry[43];
                rawEntry.packedSize =
                    ((uint32_t)entry[44] << 24) |
                    ((uint32_t)entry[45] << 16) |
                    ((uint32_t)entry[46] << 8) |
                    (uint32_t)entry[47];
                rawEntry.isEncrypted = entry[48] != 0;
                rawEntry.unpackedSize =
                    (uint32_t)entry[52] |
                    ((uint32_t)entry[53] << 8) |
                    ((uint32_t)entry[54] << 16) |
                    ((uint32_t)entry[55] << 24);
                rawEntry.isPacked = rawEntry.packedSize != rawEntry.unpackedSize;
                rawEntry.groupKey = rawGroup.groupKey;
                state.rawEntries.push_back(rawEntry);
                state.entries.push_back(Strikes::PckEntry{
                    .groupIndex = rawEntry.groupIndex,
                    .entryIndex = rawEntry.entryIndex,
                    .nameWide = rawEntry.nameWide,
                    .nameCp932 = rawEntry.nameCp932,
                    .offset = rawEntry.offset,
                    .packedSize = rawEntry.packedSize,
                    .unpackedSize = rawEntry.unpackedSize,
                    .isEncrypted = rawEntry.isEncrypted,
                    .isPacked = rawEntry.isPacked,
                    .groupKey = rawEntry.groupKey,
                    .groupBase = (uint8_t)(rawEntry.groupKey & 0xFFu),
                });
            }

            metadataOffset += groupSize;
        }
    }

    std::vector<uint8_t> extractEntryBytes(const ArchiveState& state, const RawEntryInfo& entry)
    {
        if ((uint64_t)entry.offset + entry.packedSize > state.rawData.size()) {
            throw std::runtime_error(std::format("entry points outside the package: {}", wide2Ascii(entry.nameWide, CP_UTF8)));
        }

        std::vector<uint8_t> packed(state.rawData.begin() + (ptrdiff_t)entry.offset, state.rawData.begin() + (ptrdiff_t)(entry.offset + entry.packedSize));
        if (!entry.isEncrypted && !entry.isPacked) {
            if (packed.size() != entry.unpackedSize) {
                throw std::runtime_error(std::format("stored entry size mismatch: {}", wide2Ascii(entry.nameWide, CP_UTF8)));
            }
            return packed;
        }

        int skipWords = 0;
        if (entry.isEncrypted) {
            skipWords = computeResourceSkipWords((uint8_t)(entry.groupKey & 0xFFu), entry.entryIndex);
            int detectedSkipWords = detectResourceSkipWords(packed, entry.packedSize);
            if (detectedSkipWords != 0 && detectedSkipWords != skipWords) {
                skipWords = detectedSkipWords;
            }
        }

        uint32_t chunkSize = 0;
        std::vector<uint8_t> chunk = readResourceChunk(packed, skipWords, &chunkSize);

        if (entry.isEncrypted) {
            if ((chunkSize & 0x7FFFFFFFu) >= 0x10u) {
                xorPrefixDwordwise(chunk, 0x10, blockXor);
            }
            else {
                xorPrefixBytewise(chunk, chunk.size(), smallBlockXor);
            }
        }

        if (entry.isPacked) {
            return lzssDecompress(chunk, entry.unpackedSize);
        }

        if (chunk.size() != entry.unpackedSize) {
            throw std::runtime_error(std::format("decoded entry size mismatch: {}", wide2Ascii(entry.nameWide, CP_UTF8)));
        }
        return chunk;
    }

    const RawEntryInfo& getRawEntryByPublicEntry(const ArchiveState& state, const Strikes::PckEntry& entry)
    {
        if (entry.groupIndex >= state.groupInfos.size()) {
            throw std::runtime_error("invalid entry group index");
        }

        for (const auto& rawEntry : state.rawEntries) {
            if (rawEntry.groupIndex == entry.groupIndex && rawEntry.entryIndex == entry.entryIndex) {
                return rawEntry;
            }
        }
        throw std::runtime_error("failed to locate raw entry");
    }

    std::vector<uint8_t> buildDecodedConfigHeaderForRebuild(const ArchiveState& state, uint32_t newMetaSizeOffset)
    {
        std::vector<uint8_t> header = state.decodedConfigHeader;
        uint32_t newMetaTableOffset = newMetaSizeOffset + 4;
        writeLe32(header, 0x1C, newMetaTableOffset);
        writeLe32(header, 0x20, newMetaSizeOffset);

        uint32_t checkInput = newMetaTableOffset ^ headerXor;
        header[4] = (uint8_t)(checkInput >> 16);
        header[5] = (uint8_t)checkInput;
        header[6] = (uint8_t)(checkInput >> 8);
        header[7] = (uint8_t)(checkInput >> 24);
        return header;
    }

    std::vector<uint8_t> encodeConfigHeader(const std::vector<uint8_t>& decodedHeader)
    {
        if (decodedHeader.size() != 0x68) {
            throw std::runtime_error("decoded config header must be 0x68 bytes");
        }

        std::vector<uint8_t> raw = decodedHeader;
        for (size_t offset : { 44u, 46u, 48u, 50u, 56u }) {
            std::reverse(raw.begin() + (ptrdiff_t)offset, raw.begin() + (ptrdiff_t)(offset + 2));
        }
        for (size_t offset : { 0u, 8u, 12u, 16u, 20u, 24u, 28u, 32u, 36u, 60u }) {
            std::reverse(raw.begin() + (ptrdiff_t)offset, raw.begin() + (ptrdiff_t)(offset + 4));
        }

        uint32_t seed = readBe32(raw, 0);
        KnuthRng rng(seed);
        for (size_t word = 1; word < 26; ++word) {
            size_t offset = word * 4;
            uint32_t value = readLe32(raw, offset) ^ byteSwap32(rng.next());
            writeLe32(raw, offset, value);
        }
        return raw;
    }

    ReplacementEntryData loadReplacementOrOriginal(const ArchiveState& state, const RawEntryInfo& entry, const fs::path& replacementDirectory)
    {
        auto plainPath = Strikes::buildEntryPath(replacementDirectory, Strikes::PckEntry{
            .groupIndex = entry.groupIndex,
            .entryIndex = entry.entryIndex,
            .nameWide = entry.nameWide,
            .nameCp932 = entry.nameCp932,
            .offset = entry.offset,
            .packedSize = entry.packedSize,
            .unpackedSize = entry.unpackedSize,
            .isEncrypted = entry.isEncrypted,
            .isPacked = entry.isPacked,
            .groupKey = entry.groupKey,
            .groupBase = (uint8_t)(entry.groupKey & 0xFFu),
        }, false);
        auto indexedPath = Strikes::buildEntryPath(replacementDirectory, Strikes::PckEntry{
            .groupIndex = entry.groupIndex,
            .entryIndex = entry.entryIndex,
            .nameWide = entry.nameWide,
            .nameCp932 = entry.nameCp932,
            .offset = entry.offset,
            .packedSize = entry.packedSize,
            .unpackedSize = entry.unpackedSize,
            .isEncrypted = entry.isEncrypted,
            .isPacked = entry.isPacked,
            .groupKey = entry.groupKey,
            .groupBase = (uint8_t)(entry.groupKey & 0xFFu),
        }, true);

        if (fs::exists(indexedPath) && fs::is_regular_file(indexedPath)) {
            return ReplacementEntryData{ .bytes = readBinaryFile(indexedPath), .replaced = true };
        }
        if (fs::exists(plainPath) && fs::is_regular_file(plainPath)) {
            return ReplacementEntryData{ .bytes = readBinaryFile(plainPath), .replaced = true };
        }
        return ReplacementEntryData{ .bytes = extractEntryBytes(state, entry), .replaced = false };
    }

    std::vector<uint8_t> buildPlainMetadata(const ArchiveState& state, const std::vector<uint32_t>& newOffsets, const std::vector<uint32_t>& newSizes)
    {
        std::vector<uint8_t> metadata;
        size_t entryCursor = 0;

        for (const auto& rawGroup : state.rawGroups) {
            size_t groupStart = metadata.size();
            metadata.insert(metadata.end(), rawGroup.headerBytes.begin(), rawGroup.headerBytes.end());

            for (uint32_t entryIndex = 0; entryIndex < rawGroup.entryCount; ++entryIndex, ++entryCursor) {
                if (entryCursor >= state.rawEntries.size()) {
                    throw std::runtime_error("entry cursor exceeded raw entry count");
                }

                const auto& rawEntry = state.rawEntries[entryCursor];
                if (rawEntry.groupIndex != rawGroup.groupIndex || rawEntry.entryIndex != entryIndex) {
                    throw std::runtime_error("raw entry order no longer matches group metadata");
                }

                size_t entryOffset = metadata.size();
                metadata.resize(metadata.size() + 56, 0);

                if (rawEntry.nameCp932.size() >= 40) {
                    throw std::runtime_error(std::format("entry name is too long for metadata: {}", wide2Ascii(rawEntry.nameWide, CP_UTF8)));
                }
                std::copy(rawEntry.nameCp932.begin(), rawEntry.nameCp932.end(), metadata.begin() + (ptrdiff_t)entryOffset);
                writeBe32(metadata, entryOffset + 40, newOffsets[entryCursor]);
                writeBe32(metadata, entryOffset + 44, newSizes[entryCursor]);
                metadata[entryOffset + 48] = 0;
                writeLe32(metadata, entryOffset + 52, newSizes[entryCursor]);
            }

            uint32_t actualGroupSize = (uint32_t)(metadata.size() - groupStart);
            if (actualGroupSize != rawGroup.metadataSize) {
                throw std::runtime_error("rebuilt group metadata size changed unexpectedly");
            }
        }

        if (entryCursor != state.rawEntries.size()) {
            throw std::runtime_error("not all raw entries were consumed while rebuilding metadata");
        }
        return metadata;
    }
}

namespace Strikes {

    struct PckArchive::Impl {
        ArchiveState state;
    };

    PckArchive::PckArchive(const fs::path& packagePath)
        : impl(std::make_unique<Impl>())
    {
        impl->state.packagePath = packagePath;
        impl->state.rawData = readBinaryFile(packagePath);
        parseMetadata(impl->state);
    }

    PckArchive::~PckArchive() = default;

    PckArchive::PckArchive(PckArchive&&) noexcept = default;
    PckArchive& PckArchive::operator=(PckArchive&&) noexcept = default;

    const fs::path& PckArchive::getPackagePath() const
    {
        return impl->state.packagePath;
    }

    const PckConfigInfo& PckArchive::getConfigInfo() const
    {
        return impl->state.configInfo;
    }

    const std::vector<PckGroupInfo>& PckArchive::getGroupInfos() const
    {
        return impl->state.groupInfos;
    }

    const std::vector<PckEntry>& PckArchive::getEntries() const
    {
        return impl->state.entries;
    }

    std::vector<uint8_t> PckArchive::extractEntry(const PckEntry& entry) const
    {
        const auto& rawEntry = getRawEntryByPublicEntry(impl->state, entry);
        return extractEntryBytes(impl->state, rawEntry);
    }

    void PckArchive::extractToDirectory(const fs::path& outputDirectory, std::wstring_view filter) const
    {
        size_t extractedCount = 0;
        for (const auto& entry : impl->state.entries) {
            if (!matchesEntryFilter(entry, filter)) {
                continue;
            }

            auto bytes = extractEntry(entry);
            auto outputPath = buildEntryPath(outputDirectory, entry, false);
            writeBinaryFile(outputPath, bytes);
            std::println("extract {} -> {}", wide2Ascii(entry.nameWide, CP_UTF8), wide2Ascii(outputPath.native(), CP_UTF8));
            ++extractedCount;
        }
        std::println("total extracted: {}", extractedCount);
    }

    void PckArchive::rebuildToFile(const fs::path& outputPath, const fs::path& replacementDirectory) const
    {
        if (impl->state.rawEntries.empty()) {
            throw std::runtime_error("archive has no parsed entries");
        }

        uint32_t resourceStart = impl->state.rawEntries.front().offset;
        if (resourceStart > impl->state.rawData.size()) {
            throw std::runtime_error("invalid first resource offset");
        }

        if (!outputPath.parent_path().empty()) {
            fs::create_directories(outputPath.parent_path());
        }

        std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
        if (!output.is_open()) {
            throw std::runtime_error(std::format("failed to open rebuilt package for writing: {}", wide2Ascii(outputPath.native(), CP_UTF8)));
        }

        if (resourceStart != 0) {
            output.write((const char*)impl->state.rawData.data(), (std::streamsize)resourceStart);
            if (!output) {
                throw std::runtime_error("failed to write preserved package prefix");
            }
        }

        uint64_t outputPosition = resourceStart;
        std::vector<uint32_t> newOffsets;
        std::vector<uint32_t> newSizes;
        newOffsets.reserve(impl->state.rawEntries.size());
        newSizes.reserve(impl->state.rawEntries.size());

        size_t replacedCount = 0;
        for (const auto& entry : impl->state.rawEntries) {
            auto replacement = loadReplacementOrOriginal(impl->state, entry, replacementDirectory);
            if (replacement.replaced) {
                ++replacedCount;
            }

            if (outputPosition > UINT32_MAX || replacement.bytes.size() > UINT32_MAX) {
                throw std::runtime_error("rebuilt archive exceeds 32-bit package limits");
            }

            newOffsets.push_back((uint32_t)outputPosition);
            newSizes.push_back((uint32_t)replacement.bytes.size());
            if (!replacement.bytes.empty()) {
                output.write((const char*)replacement.bytes.data(), (std::streamsize)replacement.bytes.size());
                if (!output) {
                    throw std::runtime_error(std::format("failed to write rebuilt resource: {}", wide2Ascii(entry.nameWide, CP_UTF8)));
                }
            }
            outputPosition += replacement.bytes.size();
        }

        auto metadata = buildPlainMetadata(impl->state, newOffsets, newSizes);
        if (outputPosition > UINT32_MAX) {
            throw std::runtime_error("rebuilt archive exceeds 32-bit package limits");
        }

        if (metadata.size() < 32) {
            throw std::runtime_error("rebuilt metadata is too small for indexed-block layout");
        }

        uint32_t newMetaSizeOffset = (uint32_t)outputPosition;
        std::vector<uint8_t> metaSizeWord;
        appendBe32(metaSizeWord, (uint32_t)metadata.size());
        output.write((const char*)metaSizeWord.data(), (std::streamsize)metaSizeWord.size());
        output.write((const char*)metadata.data(), 32);
        output.write((const char*)metaSizeWord.data(), (std::streamsize)metaSizeWord.size());
        output.write((const char*)metadata.data() + 32, (std::streamsize)(metadata.size() - 32));
        if (!output) {
            throw std::runtime_error("failed to write rebuilt metadata");
        }
        outputPosition += metadata.size() + 8;

        auto decodedHeader = buildDecodedConfigHeaderForRebuild(impl->state, newMetaSizeOffset);
        auto rawConfigHeader = encodeConfigHeader(decodedHeader);
        if (outputPosition > UINT32_MAX) {
            throw std::runtime_error("rebuilt archive exceeds 32-bit package limits");
        }

        uint32_t newConfigOffset = (uint32_t)outputPosition;
        output.write((const char*)rawConfigHeader.data(), (std::streamsize)rawConfigHeader.size());
        if (!output) {
            throw std::runtime_error("failed to write rebuilt config header");
        }

        output.flush();
        output.close();

        std::fstream patch(outputPath, std::ios::binary | std::ios::in | std::ios::out);
        if (!patch.is_open()) {
            throw std::runtime_error("failed to reopen rebuilt package for config pointer patching");
        }

        std::array<uint8_t, 4> pointerBytes = {
            (uint8_t)(newConfigOffset >> 24),
            (uint8_t)(newConfigOffset >> 16),
            (uint8_t)(newConfigOffset >> 8),
            (uint8_t)newConfigOffset,
        };
        patch.seekp((std::streamoff)impl->state.configInfo.thirdTableOffset, std::ios::beg);
        patch.write((const char*)pointerBytes.data(), (std::streamsize)pointerBytes.size());
        if (!patch) {
            throw std::runtime_error("failed to patch rebuilt config pointer");
        }

        std::println("rebuilt {}", wide2Ascii(outputPath.native(), CP_UTF8));
        std::println("entries={} replacements={} size={}", impl->state.rawEntries.size(), replacedCount, outputPosition + rawConfigHeader.size());
        std::println("note: rebuilt resources are stored uncompressed.");
    }

    std::wstring sanitizeEntryName(std::wstring_view name)
    {
        return sanitizeFileName(name);
    }

    fs::path buildEntryPath(const fs::path& rootPath, const PckEntry& entry, bool withIndex)
    {
        auto groupDirectory = rootPath / fs::path(std::wstring(L"group") + std::to_wstring(entry.groupIndex));
        auto fileName = sanitizeFileName(entry.nameWide);
        if (!withIndex) {
            return groupDirectory / fileName;
        }

        auto prefixedName = std::format(L"{:05}_{}", entry.entryIndex, fileName);
        return groupDirectory / prefixedName;
    }

    bool matchesEntryFilter(const PckEntry& entry, std::wstring_view filter)
    {
        if (filter.empty()) {
            return true;
        }

        auto groupPrefix = std::format(L"group{}/", entry.groupIndex);
        auto lowerFilter = str2Lower(filter);
        auto lowerName = str2Lower(entry.nameWide);
        auto lowerCombined = str2Lower(groupPrefix + entry.nameWide);
        return lowerName.find(lowerFilter) != std::wstring::npos ||
            lowerCombined.find(lowerFilter) != std::wstring::npos;
    }
}
