//Game: [OVERDRIVE]MUSICUS!
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

    uint32_t offsetListSize = *(uint32_t*)&buffer[0x10];
    uint32_t ScriptChunkSize = *(uint32_t*)&buffer[0xC];
    uint32_t ScriptBegin = buffer.size() - offsetListSize - ScriptChunkSize;
    uint32_t offsetListBegin = buffer.size() - offsetListSize;

    /*if (offsetListSize == 0 || ScriptChunkSize == 0) {
        input.close();
        output.close();
        fs::remove(outputPath);
        return;
    }*/

    for (size_t i = offsetListBegin; i < buffer.size(); i += 4) {
        uint32_t offset = *(uint32_t*)&buffer[i];
        if (buffer[ScriptBegin + offset] == 0x00) {
            output << "empty" << std::endl;
        }
        else {
            std::string str((char*)&buffer[ScriptBegin + offset]);
            output << str << std::endl;
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
    std::vector<uint32_t> offsets;

    // 读取翻译文本
    std::string line;
    while (std::getline(inputTxt, line)) {
        translations.push_back(line);
    }

    uint32_t OrgiOffsetListSize = *(uint32_t*)&buffer[0x10];

    if (translations.size() * 4 != OrgiOffsetListSize) {
        std::cout << "Num of Translations is wrong!" << std::endl;
        return;
    }

    uint32_t OrgiScriptChunkSize = *(uint32_t*)&buffer[0xC];
    uint32_t NewScriptChunkSize = 0;
    uint32_t ScriptBegin = buffer.size() - OrgiOffsetListSize - OrgiScriptChunkSize;

    std::vector<uint8_t> newBuffer;
    newBuffer.insert(newBuffer.end(), buffer.data(), buffer.data() + ScriptBegin);

    for (const auto& trans : translations) {
        offsets.push_back((uint32_t)(newBuffer.size() - ScriptBegin));
        if (trans != "empty") {
            std::vector<uint8_t> textBytes = stringToCP932(trans);
            newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
            NewScriptChunkSize += textBytes.size();
        }
        NewScriptChunkSize++;
        newBuffer.push_back(0);
    }

    for (const auto& offset : offsets) {
        newBuffer.insert(newBuffer.end(), (uint8_t*)&offset, (uint8_t*)&offset + 4);
    }

    // 写入新文件
    *(uint32_t*)&newBuffer[0xC] = NewScriptChunkSize;
    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::cout << "Write-back complete. Output saved to " << outputBinPath << std::endl;
}

void printUsage() {
    std::cout << "Made by julixian 2025.03.17" << std::endl;
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