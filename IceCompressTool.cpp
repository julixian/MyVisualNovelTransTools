#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstdint>

namespace fs = std::filesystem;

class TpwHandler {
private:
    static void UnpackTpw(std::istream& input, std::vector<uint8_t>& output) {
        // Skip TPW header
        input.seekg(8);

        std::vector<int> offsets(4);
        uint16_t base_offset;
        input.read(reinterpret_cast<char*>(&base_offset), sizeof(base_offset));
        offsets[0] = base_offset;
        offsets[1] = base_offset * 2;
        offsets[2] = base_offset * 3;
        offsets[3] = base_offset * 4;

        size_t dst = 0;
        while (dst < output.size()) {
            uint8_t ctl;
            input.read(reinterpret_cast<char*>(&ctl), sizeof(ctl));
            if (ctl == 0) break;

            size_t remaining = output.size() - dst;
            size_t count;

            if (ctl < 0x40) {
                count = std::min<size_t>(ctl, remaining);
                input.read(reinterpret_cast<char*>(output.data() + dst), count);
                dst += count;
            }
            else if (ctl <= 0x6F) {
                if (ctl == 0x6F) {
                    input.read(reinterpret_cast<char*>(&count), sizeof(uint16_t));
                }
                else {
                    count = ctl - 0x3D;
                }
                count = std::min(count, remaining);
                uint8_t v;
                input.read(reinterpret_cast<char*>(&v), sizeof(v));
                std::fill_n(output.begin() + dst, count, v);
                dst += count;
            }
            else if (ctl <= 0x9F) {
                if (ctl == 0x9F) {
                    input.read(reinterpret_cast<char*>(&count), sizeof(uint16_t));
                }
                else {
                    count = ctl - 0x6E;
                }
                input.read(reinterpret_cast<char*>(output.data() + dst), std::min<size_t>(2, remaining));
                dst += 2;
                --count;
                if (count > 0 && remaining > 2) {
                    count = std::min(count * 2, remaining - 2);
                    std::copy_n(output.begin() + dst - 2, count, output.begin() + dst);
                    dst += count;
                }
            }
            else if (ctl <= 0xBF) {
                if (ctl == 0xBF) {
                    input.read(reinterpret_cast<char*>(&count), sizeof(uint16_t));
                }
                else {
                    count = ctl - 0x9E;
                }
                input.read(reinterpret_cast<char*>(output.data() + dst), std::min<size_t>(3, remaining));
                dst += 3;
                --count;
                if (count > 0 && remaining > 3) {
                    count = std::min(count * 3, remaining - 3);
                    std::copy_n(output.begin() + dst - 3, count, output.begin() + dst);
                    dst += count;
                }
            }
            else {
                count = std::min<size_t>((ctl & 0x3F) + 3, remaining);
                uint8_t offset_byte;
                input.read(reinterpret_cast<char*>(&offset_byte), sizeof(offset_byte));
                int offset = (offset_byte & 0x3F) - offsets[offset_byte >> 6];
                std::copy_n(output.begin() + dst + offset, count, output.begin() + dst);
                dst += count;
            }
        }
    }

public:
    static bool UnpackFile(const fs::path& inputPath, const fs::path& outputPath) {
        std::ifstream input(inputPath, std::ios::binary);
        if (!input) {
            std::cerr << "Failed to open input file: " << inputPath << std::endl;
            return false;
        }

        // Read TPW header
        char header[4];
        input.read(header, 4);
        if (std::string(header, 3) != "TPW") {
            std::cerr << "Invalid TPW file: " << inputPath << std::endl;
            return false;
        }

        // Read unpacked size
        uint32_t unpackedSize;
        input.seekg(4);
        input.read(reinterpret_cast<char*>(&unpackedSize), sizeof(unpackedSize));

        std::vector<uint8_t> output(unpackedSize);
        input.seekg(0);
        UnpackTpw(input, output);

        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile) {
            std::cerr << "Failed to create output file: " << outputPath << std::endl;
            return false;
        }

        outFile.write(reinterpret_cast<const char*>(output.data()), output.size());
        std::cout << "Decompressed: " << inputPath.filename() << " -> " << outputPath.filename() << std::endl;

        return true;
    }

    static bool PackFile(const fs::path& inputPath, const fs::path& outputPath) {
        std::ifstream input(inputPath, std::ios::binary);
        if (!input) {
            std::cerr << "Failed to open input file: " << inputPath << std::endl;
            return false;
        }

        std::ofstream output(outputPath, std::ios::binary);
        if (!output) {
            std::cerr << "Failed to create output file: " << outputPath << std::endl;
            return false;
        }

        // Write TPW header
        output.write("TPW", 3);
        uint8_t compressionFlag = 0; // 0 for uncompressed
        output.write(reinterpret_cast<const char*>(&compressionFlag), 1);

        // Write the file content without compression
        std::vector<char> buffer(4096); // Use a buffer for efficient reading/writing
        while (input) {
            input.read(buffer.data(), buffer.size());
            std::streamsize bytesRead = input.gcount();
            output.write(buffer.data(), bytesRead);
        }

        std::cout << "Compressed: " << inputPath.filename() << " -> " << outputPath.filename() << std::endl;
        return true;
    }
};

void ProcessDirectory(const fs::path& inputDir, const fs::path& outputDir, bool unpack) {
    for (const auto& entry : fs::directory_iterator(inputDir)) {
        if (entry.is_regular_file()) {
            fs::path inputPath = entry.path();
            fs::path outputPath = outputDir / inputPath.filename();
            if (unpack) {
                TpwHandler::UnpackFile(inputPath, outputPath);
            }
            else {
                TpwHandler::PackFile(inputPath, outputPath);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <decompress|compress> <input_directory> <output_directory>" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    fs::path inputDir = argv[2];
    fs::path outputDir = argv[3];

    if (mode != "decompress" && mode != "compress") {
        std::cerr << "Invalid mode. Use 'decompress' or 'compress'." << std::endl;
        return 1;
    }

    if (!fs::exists(inputDir) || !fs::is_directory(inputDir)) {
        std::cerr << "Input directory does not exist or is not a directory." << std::endl;
        return 1;
    }

    if (!fs::exists(outputDir)) {
        fs::create_directories(outputDir);
    }

    ProcessDirectory(inputDir, outputDir, mode == "decompress");

    std::cout << (mode == "decompress" ? "Decompressing" : "Compressing") << " completed." << std::endl;
    return 0;
}
