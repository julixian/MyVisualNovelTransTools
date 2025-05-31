//Game: [Shelf]かぎろひ～勺景～
#include <iostream>
#include <fstream>
#include <windows.h>
#include <vector>
#include <string>
#include <iomanip>
#include <cstdint>
#include <filesystem>
#include <algorithm>
#include <regex>

namespace fs = std::filesystem;

std::vector<uint8_t> stringToCP932(const std::string& str) {
    std::vector<uint8_t> result;
    for (char c : str) {
        result.push_back(static_cast<uint8_t>(c));
    }
    return result;
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

    uint32_t ScriptChunkSize = *(uint32_t*)&buffer[0xC];
    uint32_t ScriptBegin = buffer.size() - ScriptChunkSize;

    for (size_t i = 0; i < ScriptBegin; i++) {
        if (*(uint16_t*)&buffer[i] == 0x041E
            || *(uint16_t*)&buffer[i] == 0x0403
            || *(uint16_t*)&buffer[i] == 0x403F) {
            uint32_t offset = *(uint32_t*)&buffer[i + 2];
            if (offset + ScriptBegin < buffer.size() && buffer[ScriptBegin + offset] != 0) {
                if (offset != 0 && buffer[ScriptBegin + offset - 1] != 0)continue;
                std::string str((char*)&buffer[ScriptBegin + offset]);
                std::regex pattern(R"(^[a-zA-Z\.\\]+$)");
                if (std::regex_match(str, pattern)) {
                    continue;
                }
                output << std::hex << i + 2 << ":::::" << str << std::endl;
            }
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
    std::vector<uint8_t> newBuffer = buffer;
    std::vector<std::string> translations;

    // 读取翻译文本
    std::string line;
    while (std::getline(inputTxt, line)) {
        translations.push_back(line);
    }

    uint32_t ScriptChunkSize = *(uint32_t*)&buffer[0xC];
    uint32_t addedBytes = 0;
    uint32_t ScriptBegin = buffer.size() - ScriptChunkSize;

    for (const auto& trans : translations) {
        uint32_t newOffset = newBuffer.size() - ScriptBegin;
        size_t pos = trans.find(":::::");
        size_t offsetAddr = std::stoul(trans.substr(0, pos), nullptr, 16);
        *(uint32_t*)&newBuffer[offsetAddr] = newOffset;
        std::vector<uint8_t> textBytes = stringToCP932(trans.substr(pos + 5));
        textBytes.push_back(0);
        addedBytes += textBytes.size();
        newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
    }

    ScriptChunkSize += addedBytes;
    *(uint32_t*)&newBuffer[0xC] = ScriptChunkSize;
    // 写入新文件
    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::cout << "Write-back complete. Output saved to " << outputBinPath << std::endl;
}

void printUsage() {
    std::cout << "Made by julixian 2025.05.13" << std::endl;
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