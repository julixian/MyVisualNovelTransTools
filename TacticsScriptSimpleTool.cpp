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

struct Sentence {
    size_t SeOpAddr;
    uint32_t showNameAddr;
    uint32_t trueNameAddr;
    uint32_t voiceAddr;
    uint32_t msgAddr;
    std::string showName;
    std::string trueName;
    std::string voice;
    std::string msg;
    bool fix = false;
};

//DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
void dumpText(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath);

    if (!input || !output) {
        std::cerr << "Error opening files: " << inputPath << " or " << outputPath << std::endl;
        return;
    }

    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});
    std::vector<Sentence> Sentences;
    uint32_t ScriptOpBegin = *(uint32_t*)&buffer[4];
    while (ScriptOpBegin % 4 != 0) {
        ScriptOpBegin--;
    }
    uint32_t ScriptOpEnd = 0;

    for (size_t i = buffer.size() - 4; i > 0; --i) {
        if (*(uint32_t*)&buffer[i] == 0x05) {
            ScriptOpEnd = i;
            break;
        }
    }

    for (size_t i = ScriptOpBegin; i < ScriptOpEnd; i += 4) {
        if ((*(uint32_t*)&buffer[i] == 0x69
            || *(uint32_t*)&buffer[i] == 0x64
            || *(uint32_t*)&buffer[i] == 0x63
            || *(uint32_t*)&buffer[i] == 0x61
            || *(uint32_t*)&buffer[i] == 0x5F
            || *(uint32_t*)&buffer[i] == 0x5C)
            && *(uint32_t*)&buffer[i + 4] < buffer.size()
            && *(uint32_t*)&buffer[i + 4] > ScriptOpEnd
            && *(uint32_t*)&buffer[i + 8] < buffer.size()
            && *(uint32_t*)&buffer[i + 8] > ScriptOpEnd
            && *(uint32_t*)&buffer[i + 12] < buffer.size()
            && *(uint32_t*)&buffer[i + 12] > ScriptOpEnd
            && *(uint32_t*)&buffer[i + 16] < buffer.size()
            && *(uint32_t*)&buffer[i + 16] > ScriptOpEnd) {
            Sentence Se;
            Se.SeOpAddr = i;
            Se.showNameAddr = *(uint32_t*)&buffer[i + 4];
            Se.trueNameAddr = *(uint32_t*)&buffer[i + 8];
            Se.voiceAddr = *(uint32_t*)&buffer[i + 12];
            Se.msgAddr = *(uint32_t*)&buffer[i + 16];
            Sentences.push_back(Se);
            i += 16;
        }
    }

    for (auto& Se : Sentences) {
        output << std::hex << Se.SeOpAddr << ":::::";
        if (buffer[Se.showNameAddr] != 0x00) {
            std::string str((char*)&buffer[Se.showNameAddr]);
            output << str << ":::::";
        }
        else {
            output << "empty" << ":::::";
        }
        if (buffer[Se.trueNameAddr] != 0x00) {
            std::string str((char*)&buffer[Se.trueNameAddr]);
            output << str << ":::::";
        }
        else {
            output << "empty" << ":::::";
        }
        if (buffer[Se.voiceAddr] != 0x00) {
            std::string str((char*)&buffer[Se.voiceAddr]);
            output << str << ":::::";
        }
        else {
            output << "empty" << ":::::";
        }
        if (buffer[Se.msgAddr] != 0x00) {
            std::string str((char*)&buffer[Se.msgAddr]);
            str = std::regex_replace(str, std::regex("\n"), "<br>");
            output << str << ":::::";
        }
        else {
            output << "empty" << ":::::";
        }
        output << std::endl;
    }

    input.close();
    output.close();

    std::cout << "Extraction complete. Output saved to " << outputPath << std::endl;
}

void insertStr(std::vector<uint8_t>& buffer, std::string& str, size_t zeroCount) {
    std::vector<uint8_t> textBytes = stringToCP932(str);
    buffer.insert(buffer.end(), textBytes.begin(), textBytes.end());
    for (size_t i = 0; i < zeroCount; i++) {
        buffer.push_back(0x00);
    }
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
    std::vector<Sentence> Sentences;

    // 读取翻译文本
    std::string line;
    while (std::getline(inputTxt, line)) {
        size_t posb = 0;
        Sentence Se;
        size_t pose = line.find(":::::", posb);
        Se.SeOpAddr = std::stoul(line.substr(posb, pose - posb), nullptr, 16);
        posb = pose + 5;
        pose = line.find(":::::", posb);
        Se.showName = line.substr(posb, pose - posb);
        posb = pose + 5;
        pose = line.find(":::::", posb);
        Se.trueName = line.substr(posb, pose - posb);
        posb = pose + 5;
        pose = line.find(":::::", posb);
        Se.voice = line.substr(posb, pose - posb);
        posb = pose + 5;
        pose = line.find(":::::", posb);
        Se.msg = line.substr(posb, pose - posb);
        Se.msg = std::regex_replace(Se.msg, std::regex("<br>"), "\n");
        Sentences.push_back(Se);
    }

    for (auto& Se : Sentences) {
        if (Se.showName != "empty") {
            while (newBuffer.size() % 4 != 0) {
                newBuffer.push_back(0);
            }
            *(uint32_t*)&newBuffer[Se.SeOpAddr + 4] = (uint32_t)newBuffer.size();
            insertStr(newBuffer, Se.showName, 2);
        }
        /*else {
            *(uint32_t*)&newBuffer[Se.SeOpAddr + 4] = (uint32_t)newBuffer.size();
            newBuffer.insert(newBuffer.end(), { 0,0 });
        }*/
        if (Se.trueName != "empty") {
            while (newBuffer.size() % 4 != 0) {
                newBuffer.push_back(0);
            }
            *(uint32_t*)&newBuffer[Se.SeOpAddr + 8] = (uint32_t)newBuffer.size();
            insertStr(newBuffer, Se.trueName, 2);
        }
        /*else {
            *(uint32_t*)&newBuffer[Se.SeOpAddr + 8] = (uint32_t)newBuffer.size();
            newBuffer.insert(newBuffer.end(), { 0,0 });
        }*/
        if (Se.voice != "empty") {
            while (newBuffer.size() % 4 != 0) {
                newBuffer.push_back(0);
            }
            *(uint32_t*)&newBuffer[Se.SeOpAddr + 12] = (uint32_t)newBuffer.size();
            insertStr(newBuffer, Se.voice, 2);
        }
        /*else {
            *(uint32_t*)&newBuffer[Se.SeOpAddr + 12] = (uint32_t)newBuffer.size();
            newBuffer.insert(newBuffer.end(), { 0,0 });
        }*/
        if (Se.msg != "empty") {
            while (newBuffer.size() % 4 != 0) {
                newBuffer.push_back(0);
            }
            *(uint32_t*)&newBuffer[Se.SeOpAddr + 16] = (uint32_t)newBuffer.size();
            insertStr(newBuffer, Se.msg, 2);
        }
        /*else {
            *(uint32_t*)&newBuffer[Se.SeOpAddr + 16] = (uint32_t)newBuffer.size();
            newBuffer.insert(newBuffer.end(), { 0,0 });
        }*/
    }

    // 写入新文件
    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::cout << "Write-back complete. Output saved to " << outputBinPath << std::endl;
}

void printUsage() {
    std::cout << "Made by julixian 2025.05.24" << std::endl;
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