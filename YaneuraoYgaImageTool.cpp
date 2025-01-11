#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <sstream>

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct BmpHeader {
    uint16_t fileType;
    uint32_t fileSize;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offsetData;
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bitCount;
    uint32_t compression;
    uint32_t sizeImage;
    int32_t xPelsPerMeter;
    int32_t yPelsPerMeter;
    uint32_t clrUsed;
    uint32_t clrImportant;
};
#pragma pack(pop)

struct YgaHeader {
    uint32_t signature;  // 'yga\0' or 'epf\0'
    uint32_t width;
    uint32_t height;
    uint32_t compression;
    uint32_t unpackedSize;
    uint32_t unknown;
};

class LzssDecompressor {
public:
    LzssDecompressor(int frameSize = 0x1000, uint8_t frameFill = 0, int frameInitPos = 0xFEE)
        : m_frameSize(frameSize), m_frameFill(frameFill), m_frameInitPos(frameInitPos) {}

    std::vector<uint8_t> decompress(const std::vector<uint8_t>& input) {
        std::vector<uint8_t> output;
        std::vector<uint8_t> frame(m_frameSize, m_frameFill);
        int framePos = m_frameInitPos;
        int frameMask = m_frameSize - 1;

        size_t inputPos = 0;
        while (inputPos < input.size()) {
            uint8_t ctrl = input[inputPos++];
            for (int bit = 1; bit != 0x100 && inputPos < input.size(); bit <<= 1) {
                if (ctrl & bit) {
                    uint8_t b = input[inputPos++];
                    frame[framePos++ & frameMask] = b;
                    output.push_back(b);
                }
                else {
                    if (inputPos + 1 >= input.size()) break;
                    uint8_t lo = input[inputPos++];
                    uint8_t hi = input[inputPos++];
                    int offset = ((hi & 0xf0) << 4) | lo;
                    int count = 3 + (hi & 0xF);

                    for (int i = 0; i < count; ++i) {
                        uint8_t v = frame[offset++ & frameMask];
                        frame[framePos++ & frameMask] = v;
                        output.push_back(v);
                    }
                }
            }
        }

        return output;
    }

private:
    int m_frameSize;
    uint8_t m_frameFill;
    int m_frameInitPos;
};

bool processYgaFile(std::istream& input, const fs::path& outputPath) {
    YgaHeader header;
    input.read(reinterpret_cast<char*>(&header), sizeof(YgaHeader));

    if (header.signature != 0x616779 && header.signature != 0x667065) {  // 'yga\0' or 'epf\0'
        std::cerr << "Invalid YGA file: " << outputPath << std::endl;
        return false;
    }

    std::vector<uint8_t> inputData(header.unpackedSize);
    input.seekg(0x18);  // Skip header
    input.read(reinterpret_cast<char*>(inputData.data()), header.unpackedSize);

    std::vector<uint8_t> outputData;
    if (header.compression == 1) {
        LzssDecompressor decompressor;
        outputData = decompressor.decompress(inputData);
    }
    else {
        outputData = inputData;
    }

    // 翻转图像数据
    std::vector<uint8_t> flippedData(outputData.size());
    int stride = header.width * 4;
    for (uint32_t y = 0; y < header.height; ++y) {
        memcpy(flippedData.data() + (header.height - 1 - y) * stride,
            outputData.data() + y * stride,
            stride);
    }

    // Write as BMP
    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        std::cerr << "Failed to create output file: " << outputPath << std::endl;
        return false;
    }

    // BMP Header
    uint32_t fileSize = 54 + header.width * header.height * 4;
    uint32_t dataOffset = 54;
    uint32_t headerSize = 40;
    uint16_t planes = 1;
    uint16_t bitsPerPixel = 32;
    uint32_t compression = 0;
    uint32_t imageSize = header.width * header.height * 4;
    uint32_t xPixelsPerMeter = 0;
    uint32_t yPixelsPerMeter = 0;
    uint32_t colorsUsed = 0;
    uint32_t importantColors = 0;

    output.write("BM", 2);
    output.write(reinterpret_cast<char*>(&fileSize), 4);
    output.write("\0\0\0\0", 4);
    output.write(reinterpret_cast<char*>(&dataOffset), 4);
    output.write(reinterpret_cast<char*>(&headerSize), 4);
    output.write(reinterpret_cast<char*>(&header.width), 4);
    output.write(reinterpret_cast<char*>(&header.height), 4);
    output.write(reinterpret_cast<char*>(&planes), 2);
    output.write(reinterpret_cast<char*>(&bitsPerPixel), 2);
    output.write(reinterpret_cast<char*>(&compression), 4);
    output.write(reinterpret_cast<char*>(&imageSize), 4);
    output.write(reinterpret_cast<char*>(&xPixelsPerMeter), 4);
    output.write(reinterpret_cast<char*>(&yPixelsPerMeter), 4);
    output.write(reinterpret_cast<char*>(&colorsUsed), 4);
    output.write(reinterpret_cast<char*>(&importantColors), 4);

    // Write pixel data (BGRA, flipped)
    output.write(reinterpret_cast<char*>(flippedData.data()), flippedData.size());

    output.close();

    std::cout << "File converted successfully: " << outputPath << std::endl;
    return true;
}

bool decompressAndProcessYga(const std::vector<uint8_t>& compressedData, const fs::path& outputPath) {
    LzssDecompressor decompressor;
    std::vector<uint8_t> decompressedData = decompressor.decompress(compressedData);

    // 创建一个内存流来模拟文件输入
    std::istringstream memoryStream(std::string(decompressedData.begin(), decompressedData.end()));

    // 调用原始的 processYgaFile 函数处理解压后的数据
    return processYgaFile(memoryStream, outputPath);
}

bool processYgaFile(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    if (!input) {
        std::cerr << "Failed to open input file: " << inputPath << std::endl;
        return false;
    }

    // 读取文件头的前4个字节
    char signature[4];
    input.read(signature, 4);
    input.seekg(0); // 重置文件指针到开头

    if (signature[0] == '\xff' && signature[1] == 'y' && signature[2] == 'g' && signature[3] == 'a') {
        // 压缩文件，需要先解压
        std::vector<uint8_t> compressedData((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        return decompressAndProcessYga(compressedData, outputPath);
    }
    else {
        // 未压缩文件，直接处理
        return processYgaFile(input, outputPath);
    }
}

bool processBmpFile(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    if (!input) {
        std::cerr << "Failed to open input file: " << inputPath << std::endl;
        return false;
    }

    BmpHeader bmpHeader;
    input.read(reinterpret_cast<char*>(&bmpHeader), sizeof(BmpHeader));

    if (bmpHeader.fileType != 0x4D42) {  // 'BM'
        std::cerr << "Invalid BMP file: " << inputPath << std::endl;
        return false;
    }

    std::cout << "BMP Info for file: " << inputPath << std::endl;
    std::cout << "File Size: " << bmpHeader.fileSize << " bytes" << std::endl;
    std::cout << "Data Offset: " << bmpHeader.offsetData << std::endl;
    std::cout << "Image Width: " << bmpHeader.width << std::endl;
    std::cout << "Image Height: " << bmpHeader.height << std::endl;
    std::cout << "Bit Count: " << bmpHeader.bitCount << std::endl;
    std::cout << "Compression: " << bmpHeader.compression << std::endl;
    std::cout << "Image Size: " << bmpHeader.sizeImage << std::endl;

    // 计算图像大小
    uint32_t calculatedImageSize = bmpHeader.width * std::abs(bmpHeader.height) * 4;
    uint32_t imageSize = (bmpHeader.sizeImage != 0) ? bmpHeader.sizeImage : calculatedImageSize;

    // 读取像素数据
    input.seekg(bmpHeader.offsetData);
    std::vector<uint8_t> pixelData(imageSize);
    input.read(reinterpret_cast<char*>(pixelData.data()), imageSize);
    input.close();

    // 翻转图像数据
    std::vector<uint8_t> flippedData(imageSize);
    int stride = bmpHeader.width * 4;
    for (int y = 0; y < std::abs(bmpHeader.height); ++y) {
        memcpy(flippedData.data() + y * stride,
            pixelData.data() + (std::abs(bmpHeader.height) - 1 - y) * stride,
            stride);
    }

    // 创建 YGA 头
    YgaHeader ygaHeader;
    ygaHeader.signature = 0x616779;  // 'yga\0'
    ygaHeader.width = bmpHeader.width;
    ygaHeader.height = std::abs(bmpHeader.height);
    ygaHeader.compression = 0;  // 未压缩
    ygaHeader.unpackedSize = imageSize;
    ygaHeader.unknown = 0;

    // 写入 YGA 文件
    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        std::cerr << "Failed to create output file: " << outputPath << std::endl;
        return false;
    }

    output.write(reinterpret_cast<char*>(&ygaHeader), sizeof(YgaHeader));
    output.write(reinterpret_cast<char*>(flippedData.data()), flippedData.size());
    output.close();

    std::cout << "File converted successfully: " << outputPath << std::endl;
    return true;
}

bool processFolder(const fs::path& inputFolder, const fs::path& outputFolder, bool toYga) {
    if (!fs::exists(inputFolder) || !fs::is_directory(inputFolder)) {
        std::cerr << "Input folder does not exist or is not a directory: " << inputFolder << std::endl;
        return false;
    }

    fs::create_directories(outputFolder);

    bool success = true;
    for (const auto& entry : fs::directory_iterator(inputFolder)) {
        if (fs::is_regular_file(entry)) {
            fs::path inputPath = entry.path();
            fs::path outputPath;
            if (toYga) {
                if (inputPath.extension() == ".bmp") {
                    outputPath = outputFolder / (inputPath.stem().string() + ".yga");
                    if (!processBmpFile(inputPath, outputPath)) {
                        success = false;
                    }
                }
            }
            else {
                if (inputPath.extension() == ".yga" || inputPath.extension() == ".epf") {
                    outputPath = outputFolder / (inputPath.stem().string() + ".bmp");
                    if (!processYgaFile(inputPath, outputPath)) {
                        success = false;
                    }
                }
            }
        }
    }

    return success;
}

void printUsage(const char* programName) {
    std::cout << "Made by julixian 2025.01.11" << std::endl;
    std::cout << "Usage: " << programName << " <mode> <input_folder> <output_folder>" << std::endl;
    std::cout << "Modes:" << std::endl;
    std::cout << "  yga2bmp : Convert YGA/EPF files to BMP" << std::endl;
    std::cout << "  bmp2yga : Convert BMP files to YGA" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    fs::path inputFolder = argv[2];
    fs::path outputFolder = argv[3];

    bool toYga;
    if (mode == "yga2bmp") {
        toYga = false;
    }
    else if (mode == "bmp2yga") {
        toYga = true;
    }
    else {
        std::cerr << "Invalid mode. Use 'yga2bmp' or 'bmp2yga'." << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    if (!processFolder(inputFolder, outputFolder, toYga)) {
        std::cerr << "Processing failed" << std::endl;
        return 1;
    }

    std::cout << "All files processed successfully" << std::endl;
    return 0;
}
