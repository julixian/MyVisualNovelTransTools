#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

// 文件条目结构
struct Entry {
    char name[12] = { 0 };        // 文件名
    uint32_t size;        // 文件大小
    uint32_t offset;      // 文件偏移量
};

class LzssDecompressor {
public:
    LzssDecompressor(int frameSize = 0x1000, uint8_t frameFill = 0, int frameInitPos = 0xFEE)
        : m_frameSize(frameSize), m_frameFill(frameFill), m_frameInitPos(frameInitPos) {
    }

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

bool packDat(const fs::path& input_dir, const fs::path& datPath, bool snr, int version) {

    uint32_t file_count = 0;
    std::vector<Entry> entries;
    uint32_t CurOffset = 0x4;

    for (auto entry : fs::directory_iterator(input_dir)) {
        if (entry.is_regular_file()) {
            file_count++;
            Entry fentry;
            std::string filename = entry.path().filename().string();
            memcpy(fentry.name, &filename[0], filename.length());
            fentry.size = entry.file_size();
            entries.push_back(fentry);
            CurOffset += sizeof(Entry);
        }
    }

    std::ofstream outDat(datPath, std::ios::binary);
    if (!outDat) {
        std::cout << "Fail to create output_Dat file" << std::endl;
        return false;
    }
    outDat.write((char*)&file_count, 4);

    outDat.seekp(4 + file_count * sizeof(Entry));

    for (auto& entry : entries) {
        std::string filename(entry.name);
        std::cout << "Processing: " << filename << std::endl;
        fs::path input_file = input_dir / filename;
        std::ifstream input(input_file, std::ios::binary);
        auto file_size = fs::file_size(input_file);
        std::vector<uint8_t> file_raw_data(file_size);
        input.read((char*)file_raw_data.data(), file_size);
        input.close();
        std::vector<uint8_t> finalData;
        if (snr) {
            if (version == 1) {
                finalData.resize(16);
                uint32_t raw_data_size = file_raw_data.size();
                memcpy(&finalData[0xc], &raw_data_size, sizeof(uint32_t));
                std::string sig = "snr";
                memcpy(&finalData[0], &sig[0], 3);
                std::vector<uint8_t> compressed = compress(file_raw_data);
                finalData.insert(finalData.end(), compressed.begin(), compressed.end());
            }
            else if (version == 2) {
                finalData.resize(8);
                uint32_t raw_data_size = file_raw_data.size();
                memcpy(&finalData[4], &raw_data_size, sizeof(uint32_t));
                std::string sig = "snr";
                memcpy(&finalData[0], &sig[0], 3);
                std::vector<uint8_t> compressed = compress(file_raw_data);
                finalData.insert(finalData.end(), compressed.begin(), compressed.end());
            }
            else {
                std::cout << "Not a valid version!" << std::endl;
                return false;
            }
        }
        else {
            finalData = file_raw_data;
        }
        entry.size = finalData.size();
        entry.offset = CurOffset;
        CurOffset += finalData.size();
        outDat.write((char*)finalData.data(), finalData.size());
    }

    outDat.seekp(4);
    outDat.write((char*)entries.data(), file_count * sizeof(Entry));
    outDat.close();
    return true;
}

bool extractDat(const std::string& datPath, const std::string& outputDir, bool snr, int version) {
    std::ifstream file(datPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Can not open file: " << datPath << std::endl;
        return false;
    }

    uint32_t count;
    file.read(reinterpret_cast<char*>(&count), 4);

    if (count <= 0 || count > 10000) {
        std::cerr << "Not a vaild file count: " << count << std::endl;
        return false;
    }

    if (!fs::exists(outputDir)) {
        fs::create_directories(outputDir);
    }

    file.seekg(4);

    std::vector<Entry> entries;
    for (uint32_t i = 0; i < count; ++i) {
        Entry entry;
        file.read(reinterpret_cast<char*>(&entry), sizeof(Entry));
        entries.push_back(entry);
    }

    for (const auto& entry : entries) {

        std::string filename(entry.name);
        filename = filename.substr(0, filename.find('\0'));

        if (filename.empty()) {
            continue;
        }

        fs::path outputPath = fs::path(outputDir) / filename;

        std::vector<uint8_t> raw_data(entry.size);
        file.seekg(entry.offset);
        file.read((char*)raw_data.data(), entry.size);

        std::vector<uint8_t> finalData;

        if (snr) {
            uint32_t decompLen;
            if (version == 1) {
                memcpy(&decompLen, &raw_data[0xc], sizeof(uint32_t));
                raw_data.erase(raw_data.begin(), raw_data.begin() + 16);
            }
            else if (version == 2) {
                decompLen = *(uint32_t*)&raw_data[4];
                raw_data.erase(raw_data.begin(), raw_data.begin() + 8);
            }
            else {
                std::cout << "Not a valid version!" << std::endl;
                return false;
            }
            LzssDecompressor decompressor;
            finalData = decompressor.decompress(raw_data);
            if (finalData.size() != decompLen) {
                std::cout << "Warning: expect: " << std::hex << decompLen << " actually get: " << std::hex << finalData.size() << std::endl;
            }
        }
        else {
            finalData = raw_data;
        }

        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile.is_open()) {
            std::cerr << "Can not create file: " << outputPath << std::endl;
            continue;
        }

        outFile.write((char*)finalData.data(), finalData.size());
        outFile.close();

        std::cout << "Extract: " << filename << " (" << finalData.size() << " bytes)" << std::endl;
    }

    file.close();
    return true;
}

int main(int argc, char* argv[]) {

    if (argc < 4) {
        std::cout << "Made by julixian 2025.03.16" << std::endl;
        std::cout << "Usage: " << "\n"
            << "For extract: " << argv[0] << " -e <version> [--snr] <input_file> <output_dir>" << "\n"
            << "For pack: " << argv[0] << " -p <version> [--snr] <input_dir> <output_file>" << "\n"
            << "--snr: " << "use lzss decompress/fake compress when extracting/packing" << "\n"
            << "version: " << "1 or 2" << std::endl;
        return 1;
    }

    std::string mode(argv[1]);
    int version = std::stoi(std::string(argv[2]));
    std::string inputPath = argv[argc - 2];
    std::string outputPath = argv[argc - 1];

    // 打开DAT文件
    if (mode == "-e") {
        bool snr = std::string(argv[3]) == "--snr";
        if (extractDat(inputPath, outputPath, snr, version)) {
            std::cout << "Extract successfully" << std::endl;
            return 0;
        }
        else {
            std::cout << "Fail to extract" << std::endl;
            return 1;
        }
    }
    else if (mode == "-p") {
        bool snr = std::string(argv[3]) == "--snr";
        if (packDat(inputPath, outputPath, snr, version)) {
            std::cout << "Pack successfully" << std::endl;
            return 0;
        }
        else {
            std::cout << "Fail to pack" << std::endl;
            return 1;
        }
    }
    else {
        std::cout << "Not a vaild mode" << std::endl;
        return 1;
    }

    return 0;
}
