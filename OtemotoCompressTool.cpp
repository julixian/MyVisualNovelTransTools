#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

bool all;

class LzssCompressor {
public:
    std::vector<uint8_t> compress(const std::vector<uint8_t>& input) {
        std::vector<uint8_t> output;
        for (size_t i = 0; i < input.size(); i += 8) {
            output.push_back(0xFF);
            for (size_t j = 0; j < 8 && i + j < input.size(); ++j) {
                output.push_back(input[i + j]);
            }
        }
        return output;
    }
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

bool isTargetFile(const fs::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    return extension == ".snr" || extension == ".scr";
}

bool processFile(const fs::path& inputPath, const fs::path& outputPath, bool compress) {

    if (!all) {
        if (!isTargetFile(inputPath)) {
            std::cout << "Skipping non-target file: " << inputPath << std::endl;
            return true;  // Not a failure, just skipped
        }
    }

    std::ifstream input(inputPath, std::ios::binary);
    if (!input) {
        std::cerr << "Failed to open input file: " << inputPath << std::endl;
        return false;
    }

    std::vector<uint8_t> inputData((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();

    std::vector<uint8_t> outputData;
    if (compress) {
        LzssCompressor compressor;
        outputData = compressor.compress(inputData);
    }
    else {
        LzssDecompressor decompressor;
        outputData = decompressor.decompress(inputData);
    }

    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        std::cerr << "Failed to create output file: " << outputPath << std::endl;
        return false;
    }

    output.write(reinterpret_cast<const char*>(outputData.data()), outputData.size());
    output.close();

    std::cout << "File " << (compress ? "compressed" : "decompressed") << " successfully: " << outputPath << std::endl;
    return true;
}

bool processFolder(const fs::path& inputFolder, const fs::path& outputFolder, bool compress) {
    if (!fs::exists(inputFolder) || !fs::is_directory(inputFolder)) {
        std::cerr << "Input folder does not exist or is not a directory: " << inputFolder << std::endl;
        return false;
    }

    fs::create_directories(outputFolder);

    bool success = true;
    for (const auto& entry : fs::directory_iterator(inputFolder)) {
        if (fs::is_regular_file(entry)) {
            fs::path inputPath = entry.path();
            fs::path outputPath = outputFolder / inputPath.filename();
            if (!processFile(inputPath, outputPath, compress)) {
                success = false;
            }
        }
    }

    return success;
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Made by julixian 2025.01.06" << std::endl;
        std::cerr << "Usage: " << argv[0] << " <c|d> <input_folder> <output_folder> <all|sc>" << std::endl;
        std::cerr << "  c: compress" << std::endl;
        std::cerr << "  d: decompress" << std::endl;
        std::cerr << "  all: decompress/compress all files" << std::endl;
        std::cerr << "  sc: just decompress/compress script files" << std::endl;
        return 1;
    }

    bool compress = (argv[1][0] == 'c');
    all = (argv[4][0] == 'a');
    fs::path inputFolder = argv[2];
    fs::path outputFolder = argv[3];

    if (!processFolder(inputFolder, outputFolder, compress)) {
        std::cerr << "Processing failed" << std::endl;
        return 1;
    }

    std::cout << "All files processed successfully" << std::endl;
    return 0;
}
