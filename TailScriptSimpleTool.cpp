#include <iostream>
#include <fstream>
#include <windows.h>
#include <vector>
#include <string>
#include <iomanip>
#include <cstdint>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

std::vector<uint8_t> stringToCP932(const std::string& str) {
    std::vector<uint8_t> result;
    for (char c : str) {
        result.push_back(static_cast<uint8_t>(c));
    }
    return result;
}

std::string extractString(const std::vector<uint8_t>& buffer, size_t& i) {
    std::string text;
    for (size_t j = 0; buffer[i + j] != 0x00; ++j) {
        text.push_back(static_cast<char>(buffer[i + j]));
    }
    return text;
}

//DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
void dumpText(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath);

    if (!input || !output) {
        std::cerr << "Error opening files: " << inputPath << " or " << outputPath << std::endl;
        return;
    }

    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});

    size_t ScriptBegin = 0;
    memcpy(&ScriptBegin, &buffer[0x20], sizeof(uint32_t));

    for (size_t i = 0x24; i < ScriptBegin; i ++) {
        if (buffer[i] == 0x01 && buffer[i + 1] == 0x05 && buffer[i + 6] == 0x07) {
            i += 2;
            uint32_t offset = 0;
            memcpy(&offset, &buffer[i], sizeof(uint32_t));
            size_t SeAddr = ScriptBegin + offset;
            if (SeAddr >= buffer.size()) {
                //std::cout << "溢出于D1 i = 0x" << std::hex << i << std::dec << std::endl;
                continue;
            }
            if (buffer[SeAddr - 1] != 0x00 || buffer[SeAddr] <= 0x20 || buffer[SeAddr] >= 0xeF) {
                //std::cout << "Offset Wrong！Please Check it: " << std::hex << i << std::dec << std::endl;
                continue;
            }
            std::string text = extractString(buffer, SeAddr);
            output << text << "\n";
            i += 3;
        }
    }

    input.close();
    output.close();

    std::cout << "Extraction complete. Output saved to " << outputPath << std::endl;
}

//IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII
void injectText(const fs::path& inputBinPath, const fs::path& inputTxtPath, const fs::path& outputBinPath) {
    std::ifstream inputBin(inputBinPath, std::ios::binary);
    std::ifstream inputTxt(inputTxtPath);
    std::ofstream outputBin(outputBinPath, std::ios::binary);

    if (!inputBin || !inputTxt || !outputBin) {
        std::cerr << "Error opening files: " << inputBinPath << " or " << inputTxtPath << " or " << outputBinPath << std::endl;
        return;
    }
    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(inputBin), {});
    std::vector<std::string> translations;

    // 读取翻译文本
    std::string line;
    while (std::getline(inputTxt, line)) {
        translations.push_back(line);
    }

    size_t translationIndex = 0;
    std::vector<uint8_t> newBuffer;

    size_t ScriptBegin = 0;
    memcpy(&ScriptBegin, &buffer[0x20], sizeof(uint32_t));

    for (size_t i = 0; i < ScriptBegin; i++) {
        newBuffer.push_back(buffer[i]);
    }

    for (size_t i = 0x24; i < ScriptBegin; i++) {
        if (buffer[i] == 0x01 && buffer[i + 1] == 0x05 && buffer[i + 6] == 0x07) {
            i += 2;
            uint32_t offset = 0;
            memcpy(&offset, &buffer[i], sizeof(uint32_t));
            size_t SeAddr = ScriptBegin + offset;
            if (SeAddr >= buffer.size()) {
                //std::cout << "溢出于D1 i = 0x" << std::hex << i << std::dec << std::endl;
                continue;
            }
            if (buffer[SeAddr - 1] != 0x00 || buffer[SeAddr] <= 0x20 || buffer[SeAddr] >= 0xeF) {
                //std::cout << "Offset Wrong！Please Check it: " << std::hex << i << std::dec << std::endl;
                continue;
            }
            if (translationIndex >= translations.size()) {
                std::cout << "Not Enough Translations！" << std::endl;
                continue;
            }
            std::string text = translations[translationIndex];
            translationIndex++;
            std::vector<uint8_t> textBytes = stringToCP932(text);
            uint32_t transoffset = newBuffer.size() - ScriptBegin;
            memcpy(&newBuffer[i], &transoffset, sizeof(uint32_t));
            newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
            newBuffer.push_back(0x00);
            i += 3;
        }
    }

    // 写入新文件
    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::cout << "Write-back complete. Output saved to " << outputBinPath << std::endl;
}

void printUsage() {
    std::cout << "Usage:" << std::endl;
    std::cout << "  Dump:   ./program dump <input_folder> <output_folder>" << std::endl;
    std::cout << "  Inject: ./program inject <input_orgi-bin_folder> <input_translated-txt_folder> <output_folder>" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "dump") {
        if (argc != 4) {
            std::cerr << "Error: Incorrect number of arguments for dump mode." << std::endl;
            printUsage();
            return 1;
        }
        std::string inputFolder = argv[2];
        std::string outputFolder = argv[3];

        fs::create_directories(outputFolder);

        for (const auto& entry : fs::recursive_directory_iterator(inputFolder)) {
            if (fs::is_regular_file(entry)) {
                fs::path relativePath = fs::relative(entry.path(), inputFolder);
                fs::path outputPath = fs::path(outputFolder) / relativePath;
                fs::create_directories(outputPath.parent_path());
                dumpText(entry.path(), outputPath);
            }
        }
    }
    else if (mode == "inject") {
        if (argc != 5) {
            std::cerr << "Error: Incorrect number of arguments for inject mode." << std::endl;
            printUsage();
            return 1;
        }
        std::string inputBinFolder = argv[2];
        std::string inputTxtFolder = argv[3];
        std::string outputFolder = argv[4];

        fs::create_directories(outputFolder);

        for (const auto& entry : fs::recursive_directory_iterator(inputBinFolder)) {
            if (fs::is_regular_file(entry)) {
                fs::path relativePath = fs::relative(entry.path(), inputBinFolder);
                fs::path txtPath = fs::path(inputTxtFolder) / relativePath;
                fs::path outputPath = fs::path(outputFolder) / relativePath;
                fs::create_directories(outputPath.parent_path());
                if (fs::exists(txtPath)) {
                    injectText(entry.path(), txtPath, outputPath);
                }
                else {
                    std::cerr << "Warning: No corresponding file found for " << relativePath << std::endl;
                }
            }
        }
    }
    else {
        std::cerr << "Error: Invalid mode selected." << std::endl;
        printUsage();
        return 1;
    }

    return 0;
}