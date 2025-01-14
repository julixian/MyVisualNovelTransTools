#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstdint>
#include <memory>
#include <zlib.h>
#include <png.h>
#include <iomanip>
#include <sstream>

// 调试工具函数
void PrintHex(const uint8_t* data, size_t size, size_t max_display = 16) {
    for (size_t i = 0; i < std::min(size, max_display); ++i) {
        printf("%02X ", data[i]);
    }
    if (size > max_display) printf("...");
    printf("\n");
}

std::string BytesToString(uint64_t bytes) {
    const char* units[] = { "B", "KB", "MB", "GB" };
    int unit = 0;
    double size = bytes;
    while (size >= 1024 && unit < 3) {
        size /= 1024;
        unit++;
    }
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    return ss.str();
}

struct ImageStats {
    uint64_t total_r = 0, total_g = 0, total_b = 0, total_a = 0;
    uint8_t max_r = 0, max_g = 0, max_b = 0, max_a = 0;
    uint8_t min_r = 255, min_g = 255, min_b = 255, min_a = 255;

    void analyze(const std::vector<uint8_t>& imageData) {
        for (size_t i = 0; i < imageData.size(); i += 4) {
            uint8_t b = imageData[i];
            uint8_t g = imageData[i + 1];
            uint8_t r = imageData[i + 2];
            uint8_t a = imageData[i + 3];

            total_b += b; total_g += g; total_r += r; total_a += a;
            max_b = std::max(max_b, b); max_g = std::max(max_g, g);
            max_r = std::max(max_r, r); max_a = std::max(max_a, a);
            min_b = std::min(min_b, b); min_g = std::min(min_g, g);
            min_r = std::min(min_r, r); min_a = std::min(min_a, a);
        }
    }

    void print(size_t pixelCount) {
        printf("Color Statistics:\n");
        printf("Channel | Average | Min | Max\n");
        printf("--------|---------|-----|-----\n");
        printf("Red     | %.2f    | %3d | %3d\n", float(total_r) / pixelCount, min_r, max_r);
        printf("Green   | %.2f    | %3d | %3d\n", float(total_g) / pixelCount, min_g, max_g);
        printf("Blue    | %.2f    | %3d | %3d\n", float(total_b) / pixelCount, min_b, max_b);
        printf("Alpha   | %.2f    | %3d | %3d\n", float(total_a) / pixelCount, min_a, max_a);
    }
};

#pragma pack(push, 1)
struct BitmapFileHeader {
    uint16_t type = 0x4D42;  // 'BM'
    uint32_t size;
    uint16_t reserved1 = 0;
    uint16_t reserved2 = 0;
    uint32_t offsetBits;
};

struct BitmapInfoHeader {
    uint32_t size = 40;
    int32_t width;
    int32_t height;
    uint16_t planes = 1;
    uint16_t bitCount = 32;
    uint32_t compression = 0;
    uint32_t sizeImage;
    int32_t xPelsPerMeter = 0;
    int32_t yPelsPerMeter = 0;
    uint32_t clrUsed = 0;
    uint32_t clrImportant = 0;
};

struct CpbHeader {
    uint32_t signature = 0x1a425043; // 'CPB\x1a'
    uint8_t type = 0;
    uint8_t bpp = 32;  // Changed to 32 for PNG to CPB
    uint16_t version = 1;
    uint32_t padding = 0x00ffffff;
    uint16_t width;
    uint16_t height;
    uint32_t channelSize[4] = { 0 }; // A, B, G, R for PNG to CPB
};
#pragma pack(pop)

class CpbMetaData {
public:
    int type;
    int version;
    int width;
    int height;
    int bpp;
    uint32_t channel[4];
    uint32_t dataOffset;

    void print() const {
        printf("\nCPB Metadata:\n");
        printf("Type: %d\n", type);
        printf("Version: %d\n", version);
        printf("Dimensions: %dx%d\n", width, height);
        printf("Bits per pixel: %d\n", bpp);
        printf("Channel sizes:\n");
        for (int i = 0; i < 4; ++i) {
            printf("Channel %d: %s\n", i, BytesToString(channel[i]).c_str());
        }
        printf("Data offset: 0x%X\n", dataOffset);
    }
};

// CPB to BMP conversion functions
class CpbReader {
private:
    std::vector<uint8_t> m_output;
    CpbMetaData m_info;
    std::vector<uint8_t> m_streamMap;
    std::vector<uint8_t> m_channelMap;
    std::ifstream& m_input;

public:
    CpbReader(std::ifstream& input, const CpbMetaData& info)
        : m_input(input), m_info(info) {
        m_output.resize(info.width * info.height * 4);

        if (info.version == 1) {
            m_streamMap = { 0, 3, 1, 2 };
            m_channelMap = { 3, 0, 1, 2 };
            printf("Using Version 1 channel mapping\n");
        }
        else {
            m_streamMap = { 0, 1, 2, 3 };
            m_channelMap = { 2, 1, 0, 3 };
            printf("Using Version 0 channel mapping\n");
        }

        printf("Stream Map: ");
        PrintHex(m_streamMap.data(), m_streamMap.size());
        printf("Channel Map: ");
        PrintHex(m_channelMap.data(), m_channelMap.size());
    }

    const std::vector<uint8_t>& GetData() const { return m_output; }

    void Unpack() {
        printf("\nUnpacking CPB data:\n");
        if (m_info.version == 0 && m_info.type == 3) {
            printf("Using V3 unpacking method\n");
            UnpackV3();
        }
        else {
            printf("Using V0 unpacking method\n");
            UnpackV0();
        }
    }

private:
    void UnpackV0() {
        std::vector<uint8_t> channel(m_info.width * m_info.height);
        std::streampos start_pos = m_input.tellg();

        for (int i = 0; i < 4; ++i) {
            if (m_info.channel[m_streamMap[i]] == 0) {
                printf("Channel %d: Skipped (empty)\n", i);
                continue;
            }

            printf("\nProcessing channel %d:\n", i);
            printf("Stream index: %d, Channel index: %d\n", m_streamMap[i], m_channelMap[i]);

            m_input.seekg(start_pos + std::streamoff(4)); // skip CRC32

            // Read compressed data
            std::vector<uint8_t> compressed(m_info.channel[m_streamMap[i]]);
            m_input.read(reinterpret_cast<char*>(compressed.data()), compressed.size());
            printf("Compressed size: %s\n", BytesToString(compressed.size()).c_str());

            // Decompress using zlib
            z_stream strm = {};
            inflateInit(&strm);

            strm.next_in = compressed.data();
            strm.avail_in = compressed.size();
            strm.next_out = channel.data();
            strm.avail_out = channel.size();

            int result = inflate(&strm, Z_FINISH);
            inflateEnd(&strm);

            printf("Decompression result: %d\n", result);
            printf("Decompressed size: %s\n", BytesToString(channel.size()).c_str());

            // Show sample of decompressed data
            printf("Sample data: ");
            PrintHex(channel.data(), std::min(size_t(16), channel.size()));

            // Copy to output buffer
            int dst = m_channelMap[i];
            for (size_t j = 0; j < channel.size(); ++j) {
                m_output[dst] = channel[j];
                dst += 4;
            }

            start_pos += m_info.channel[m_streamMap[i]];
        }
    }

    void UnpackV3() {
        std::vector<uint8_t> channel(m_info.width * m_info.height);
        std::streampos start_pos = m_input.tellg();

        for (int i = 0; i < 4; ++i) {
            uint32_t packed_size = m_info.channel[m_streamMap[i]];
            if (packed_size == 0) {
                printf("Channel %d: Skipped (empty)\n", i);
                continue;
            }

            printf("\nProcessing channel %d:\n", i);
            printf("Stream index: %d, Channel index: %d\n", m_streamMap[i], m_channelMap[i]);

            m_input.seekg(start_pos);

            std::vector<uint8_t> input(packed_size);
            m_input.read(reinterpret_cast<char*>(input.data()), packed_size);
            printf("Packed size: %s\n", BytesToString(packed_size).c_str());

            int src1 = 0x14;
            int src2 = src1 + *reinterpret_cast<int32_t*>(&input[4]);
            int src3 = src2 + *reinterpret_cast<int32_t*>(&input[8]);
            int remaining = *reinterpret_cast<int32_t*>(&input[0x10]);

            printf("Decompression info:\n");
            printf("src1: 0x%X, src2: 0x%X, src3: 0x%X\n", src1, src2, src3);
            printf("Remaining: %d bytes\n", remaining);

            int dst = 0;
            int mask = 0x80;

            while (remaining > 0) {
                int count;
                if (mask & input[src1]) {
                    uint16_t offset = *reinterpret_cast<uint16_t*>(&input[src2]);
                    src2 += 2;
                    count = (offset >> 13) + 3;
                    offset = (offset & 0x1FFF) + 1;

                    for (int j = 0; j < count; ++j)
                        channel[dst + j] = channel[dst - offset + j];
                }
                else {
                    count = input[src3++] + 1;
                    std::copy_n(&input[src3], count, &channel[dst]);
                    src3 += count;
                }

                dst += count;
                remaining -= count;
                mask >>= 1;
                if (mask == 0) {
                    ++src1;
                    mask = 0x80;
                }
            }

            printf("Decompressed size: %s\n", BytesToString(channel.size()).c_str());
            printf("Sample data: ");
            PrintHex(channel.data(), std::min(size_t(16), channel.size()));

            int dstOffset = m_channelMap[i];
            for (size_t j = 0; j < channel.size(); ++j) {
                m_output[dstOffset] = channel[j];
                dstOffset += 4;
            }

            start_pos += packed_size;
        }
    }
};

CpbMetaData ReadMetaData(std::ifstream& file) {
    CpbMetaData info = {};

    printf("\nReading CPB Metadata:\n");
    printf("File position: 0x%llX\n", (long long)file.tellg());

    file.seekg(4); // Skip signature

    info.type = file.get();
    info.bpp = file.get();

    printf("Type: %d\n", info.type);
    printf("Bits per pixel: %d\n", info.bpp);

    if (info.bpp != 24 && info.bpp != 32) {
        throw std::runtime_error("Unsupported CPB image format: " + std::to_string(info.bpp) + " BPP");
    }

    uint16_t version;
    file.read(reinterpret_cast<char*>(&version), 2);
    info.version = version;

    printf("Version: %d\n", version);

    if (version != 0 && version != 1) {
        throw std::runtime_error("Unsupported CPB version: " + std::to_string(version));
    }

    if (version == 1) {
        uint32_t skip;
        file.read(reinterpret_cast<char*>(&skip), 4);
        printf("Skipped value: 0x%X\n", skip);

        file.read(reinterpret_cast<char*>(&info.width), 2);
        file.read(reinterpret_cast<char*>(&info.height), 2);
    }
    else {
        file.read(reinterpret_cast<char*>(&info.width), 2);
        file.read(reinterpret_cast<char*>(&info.height), 2);

        uint32_t skip;
        file.read(reinterpret_cast<char*>(&skip), 4);
        printf("Skipped value: 0x%X\n", skip);
    }

    file.read(reinterpret_cast<char*>(info.channel), sizeof(info.channel));
    info.dataOffset = file.tellg();

    info.print();
    return info;
}

void SaveBMP(const std::string& filename, const std::vector<uint8_t>& imageData,
    int width, int height) {
    printf("\nSaving BMP file:\n");
    printf("Output file: %s\n", filename.c_str());
    printf("Dimensions: %dx%d\n", width, height);
    printf("Data size: %s\n", BytesToString(imageData.size()).c_str());

    BitmapFileHeader fileHeader;
    BitmapInfoHeader infoHeader;

    infoHeader.width = width;
    infoHeader.height = -height; // Top-down image
    infoHeader.sizeImage = width * height * 4;

    fileHeader.size = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader) + infoHeader.sizeImage;
    fileHeader.offsetBits = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);

    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        throw std::runtime_error("Failed to create output file: " + filename);
    }

    outFile.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
    outFile.write(reinterpret_cast<const char*>(&infoHeader), sizeof(infoHeader));
    outFile.write(reinterpret_cast<const char*>(imageData.data()), imageData.size());

    printf("BMP file saved successfully\n");
}

void ConvertCpbToBmp(const std::string& inputPath, const std::string& outputPath) {
    printf("\n========================================\n");
    printf("Converting CPB to BMP:\n");
    printf("Input: %s\n", inputPath.c_str());
    printf("Output: %s\n", outputPath.c_str());
    printf("========================================\n");

    std::ifstream file(inputPath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << inputPath << std::endl;
        return;
    }

    uint32_t signature;
    file.read(reinterpret_cast<char*>(&signature), 4);
    printf("File signature: %08X\n", signature);

    if (signature != 0x1a425043) {
        std::cerr << "Invalid CPB file signature" << std::endl;
        return;
    }

    try {
        CpbMetaData metadata = ReadMetaData(file);
        CpbReader reader(file, metadata);
        reader.Unpack();

        auto imageData = reader.GetData();

        ImageStats stats;
        stats.analyze(imageData);
        printf("\nImage Analysis:\n");
        stats.print(metadata.width * metadata.height);

        SaveBMP(outputPath, imageData, metadata.width, metadata.height);
        printf("\nConversion completed successfully\n");
    }
    catch (const std::exception& e) {
        std::cerr << "Error processing " << inputPath << ": " << e.what() << std::endl;
    }
}

// PNG to CPB conversion functions
std::vector<uint8_t> readPNG(const char* filename, int& width, int& height) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) throw std::runtime_error("Cannot open file");

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) throw std::runtime_error("png_create_read_struct failed");

    png_infop info = png_create_info_struct(png);
    if (!info) throw std::runtime_error("png_create_info_struct failed");

    if (setjmp(png_jmpbuf(png))) throw std::runtime_error("Error during png reading");

    png_init_io(png, fp);
    png_read_info(png, info);

    width = png_get_image_width(png, info);
    height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    if (bit_depth == 16) png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    std::vector<uint8_t> image(width * height * 4);
    std::vector<png_bytep> row_pointers(height);
    for (int y = 0; y < height; y++) {
        row_pointers[y] = &image[y * width * 4];
    }

    png_read_image(png, row_pointers.data());
    fclose(fp);
    png_destroy_read_struct(&png, &info, nullptr);

    return image;
}

std::vector<uint8_t> compressChannel(const std::vector<uint8_t>& data) {
    uLong sourceLen = data.size();
    uLong destLen = compressBound(sourceLen);
    std::vector<uint8_t> compressed(destLen);

    if (compress2(compressed.data(), &destLen, data.data(), sourceLen, Z_BEST_COMPRESSION) != Z_OK) {
        throw std::runtime_error("Compression failed");
    }

    compressed.resize(destLen);
    return compressed;
}

void writeCPB(const char* filename, const std::vector<uint8_t>& imageData, int width, int height) {
    CpbHeader header;
    header.width = width;
    header.height = height;

    std::vector<std::vector<uint8_t>> channels(4);
    for (int i = 0; i < 4; ++i) {
        channels[i].resize(width * height);
    }

    for (int i = 0; i < width * height; ++i) {
        channels[0][i] = imageData[i * 4 + 3]; // A
        channels[1][i] = imageData[i * 4 + 2]; // R
        channels[2][i] = imageData[i * 4 + 1]; // G
        channels[3][i] = imageData[i * 4 + 0]; // B
    }

    std::vector<uint32_t> channelSizes(4, 0);
    uint32_t totalSize = sizeof(CpbHeader);

    // First pass: compress channels and calculate total size
    std::vector<std::vector<uint8_t>> compressedChannels(4);
    for (int i = 0; i < 4; ++i) {
        compressedChannels[i] = compressChannel(channels[i]);
        uint32_t crc = crc32(0L, Z_NULL, 0);
        crc = crc32(crc, compressedChannels[i].data(), compressedChannels[i].size());
        channelSizes[i] = compressedChannels[i].size() + 4; // +4 for CRC
        totalSize += channelSizes[i];
    }

    // Set padding to half of the total file size
    header.padding = totalSize / 2;

    // Adjust the order of channel sizes in the header
    header.channelSize[0] = channelSizes[0]; // A
    header.channelSize[1] = channelSizes[2]; // B
    header.channelSize[2] = channelSizes[3]; // G
    header.channelSize[3] = channelSizes[1]; // R
    //I don't know what I'm writing, but it works.

    // Write header and data
    std::ofstream outFile(filename, std::ios::binary);
    outFile.write(reinterpret_cast<const char*>(&header), sizeof(header));

    for (int i = 0; i < 4; ++i) {
        uint32_t crc = crc32(0L, Z_NULL, 0);
        crc = crc32(crc, compressedChannels[i].data(), compressedChannels[i].size());
        outFile.write(reinterpret_cast<const char*>(&crc), 4);
        outFile.write(reinterpret_cast<const char*>(compressedChannels[i].data()), compressedChannels[i].size());
    }
}

void ConvertPngToCpb(const std::string& inputPath, const std::string& outputPath) {
    printf("\n========================================\n");
    printf("Converting PNG to CPB:\n");
    printf("Input: %s\n", inputPath.c_str());
    printf("Output: %s\n", outputPath.c_str());
    printf("========================================\n");

    try {
        int width, height;
        auto imageData = readPNG(inputPath.c_str(), width, height);
        writeCPB(outputPath.c_str(), imageData, width, height);
        std::cout << "Conversion from PNG to CPB completed successfully." << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error converting " << inputPath << ": " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    try {
        if (argc != 4) {
            std::cout << "Made by julixian 2025.01.14" << std::endl;
            std::cout << "Usage: " << argv[0] << " <mode> <input_folder> <output_folder>\n";
            std::cout << "Mode: png2cpb or cpb2bmp\n";
            return 1;
        }

        std::string mode = argv[1];
        std::string inputFolder = argv[2];
        std::string outputFolder = argv[3];

        printf("Mode: %s\n", mode.c_str());
        printf("Input folder: %s\n", inputFolder.c_str());
        printf("Output folder: %s\n", outputFolder.c_str());

        std::filesystem::create_directories(outputFolder);

        size_t fileCount = 0;
        if (mode == "png2cpb") {
            for (const auto& entry : std::filesystem::directory_iterator(inputFolder)) {
                if (entry.path().extension() == ".png") {
                    std::string inputPath = entry.path().string();
                    std::string outputPath = (std::filesystem::path(outputFolder) /
                        entry.path().stem()).string() + ".cpb";
                    ConvertPngToCpb(inputPath, outputPath);
                    fileCount++;
                }
            }
        }
        else if (mode == "cpb2bmp") {
            for (const auto& entry : std::filesystem::directory_iterator(inputFolder)) {
                if (entry.path().extension() == ".cpb") {
                    std::string inputPath = entry.path().string();
                    std::string outputPath = (std::filesystem::path(outputFolder) /
                        entry.path().stem()).string() + ".bmp";
                    ConvertCpbToBmp(inputPath, outputPath);
                    fileCount++;
                }
            }
        }
        else {
            std::cerr << "Invalid mode. Use 'png2cpb' or 'cpb2bmp'\n";
            return 1;
        }

        printf("\nProcessing complete\n");
        printf("Total files processed: %zu\n", fileCount);
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
