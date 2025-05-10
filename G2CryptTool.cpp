#include <iostream>
#include <fstream>
#include <windows.h>
#include <vector>
#include <string>
#include <regex>
#include <filesystem>

namespace fs = std::filesystem;

void decryptStr(uint8_t* data, int length) {
    for (int i = 0; i < length; i++) {
        data[i] = data[i] - (uint8_t)i - length;
    }
}

void encryptStr(uint8_t* data, int length) {
    for (int i = 0; i < length; i++) {
        data[i] = data[i] + (uint8_t)i + length;
    }
}

std::vector<uint8_t> stringToCP932(const std::string& str) {
    std::vector<uint8_t> result;
    for (char c : str) {
        result.push_back(static_cast<uint8_t>(c));
    }
    return result;
}

bool isValidCP932(const std::string& str) {
    if (str.empty())return false;
    std::vector<uint8_t> textBytes = stringToCP932(str);
    for (size_t i = 0; i < textBytes.size(); i++) {
        if ((textBytes[i] < 0x20 && textBytes[i] != 0x0d && textBytes[i] != 0x0a) || (0x9f < textBytes[i] && textBytes[i] < 0xe0)) {
            return false;
        }
        else if ((0x81 <= textBytes[i] && textBytes[i] <= 0x9f) || (0xe0 <= textBytes[i] && textBytes[i] <= 0xef)) {
            if (textBytes[i + 1] > 0xfc || textBytes[i + 1] < 0x40) {
                return false;
            }
            else {
                i++;
                continue;
            }
        }
    }
    return true;
}

//DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
void dumpText(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath);

    if (!input || !output) {
        std::cerr << "Error opening files: " << inputPath.string() << " or " << outputPath.string() << std::endl;
        return;
    }

    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});

    for (size_t i = 0; i < buffer.size() - 6; i++) {
        if ((*(uint16_t*)&buffer[i] == 0x0100 || *(uint16_t*)&buffer[i] == 0x0200) && *(uint32_t*)&buffer[i + 2] < 384) {
            uint32_t length = *(uint32_t*)&buffer[i + 2];
            std::string str((char*)&buffer[i + 6], length);
            decryptStr((uint8_t*)str.data(), str.length());
            if (!isValidCP932(str)) {
                continue;
            }
            std::regex pattern(R"([\r])");
            str = std::regex_replace(str, pattern, "\\r");
            pattern = R"([\n])";
            str = std::regex_replace(str, pattern, "\\n");
            //output << std::hex << i << " :" << str << std::endl;
            output << str << std::endl;
            i += 6 + length - 1;
        }
    }

    input.close();
    output.close();

    std::cout << "Extraction complete. Output saved to " << outputPath.string() << std::endl;
}


struct Sentence {
    size_t addr;
    int offset;
};
//IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII
void injectText(const fs::path& inputBinPath, const fs::path& inputTxtPath, const fs::path& outputBinPath) {
    std::ifstream inputBin(inputBinPath, std::ios::binary);
    std::ifstream inputTxt(inputTxtPath);
    std::ofstream outputBin(outputBinPath, std::ios::binary);

    if (!inputBin || !inputTxt || !outputBin) {
        std::cerr << "Error opening files: " << inputBinPath.string() << " or " 
            << inputTxtPath.string() << " or " << outputBinPath.string() << std::endl;
        return;
    }
    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(inputBin), {});
    std::vector<std::string> translations;

    int offset = 0; //0x9308
    uint32_t jmp1 = *(uint32_t*)&buffer[0x26];
    uint32_t jmp2 = *(uint32_t*)&buffer[0x2a];
    uint32_t jmp3 = *(uint32_t*)&buffer[0x2e];
    uint32_t jmp4 = *(uint32_t*)&buffer[0x32];
    std::vector<Sentence> Sentences;

    // 读取翻译文本
    std::string line;
    while (std::getline(inputTxt, line)) {
        std::regex pattern(R"(\\r)");
        line = std::regex_replace(line, pattern, "\r");
        pattern = R"(\\n)";
        line = std::regex_replace(line, pattern, "\n");
        translations.push_back(line);
    }

    size_t translationIndex = 0;
    std::vector<uint8_t> newBuffer;
    std::vector<size_t>jmps;

    for (size_t i = 0; i < buffer.size() - 6; i++) {
        if ((*(uint16_t*)&buffer[i] == 0x0100 || *(uint16_t*)&buffer[i] == 0x0200) && *(uint32_t*)&buffer[i + 2] < 384) {
            uint32_t length = *(uint32_t*)&buffer[i + 2];
            std::string str((char*)&buffer[i + 6], length);
            decryptStr((uint8_t*)str.data(), str.length());
            if (!isValidCP932(str)) {
                newBuffer.push_back(buffer[i]);
                continue;
            }
            if (translationIndex >= translations.size()) {
                std::cout << "Warning: Not enough translations!" << std::endl;
                newBuffer.push_back(buffer[i]);
                continue;
            }
            std::string text = translations[translationIndex++];
            std::vector<uint8_t> textBytes = stringToCP932(text);
            encryptStr(textBytes.data(), textBytes.size());
            newBuffer.push_back(buffer[i]); newBuffer.push_back(buffer[i + 1]);
            uint32_t newLen = textBytes.size();
            Sentence Se;
            Se.addr = i;
            Se.offset = newLen - length;
            Sentences.push_back(Se);
            newBuffer.insert(newBuffer.end(), (uint8_t*)&newLen, (uint8_t*)&newLen + 4);
            newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
            i += 6 + length - 1;
        }
        else if (*(uint16_t*)&buffer[i] == 0x0700 //07 08 09
            || *(uint16_t*)&buffer[i] == 0x0704
            || *(uint16_t*)&buffer[i] == 0x0710
            || *(uint16_t*)&buffer[i] == 0x0893
            || *(uint16_t*)&buffer[i] == 0x0892
            || *(uint16_t*)&buffer[i] == 0x0891
            || *(uint16_t*)&buffer[i] == 0x090d) {
            if (*(uint32_t*)&buffer[i + 2] < buffer.size()) {
                jmps.push_back(newBuffer.size() + 2);
            }
            newBuffer.push_back(buffer[i]);
        }
        else {
            newBuffer.push_back(buffer[i]);
        }
    }

    if (translationIndex != translations.size()) {
        std::cout << "Warning: too many translations!" << std::endl;
        system("pause");
    }
    newBuffer.insert(newBuffer.end(), buffer.end() - 6, buffer.end());

    if (jmp1 < buffer.size() && jmp1 != 0x1201B) {
        jmps.push_back(0x26);
        if (jmp2 < buffer.size() && jmp2 != 0x1201B) {
            jmps.push_back(0x2a);
            if (jmp3 < buffer.size() && jmp3 != 0x1201B) {
                jmps.push_back(0x2e);
                if (jmp4 < buffer.size() && jmp4 != 0x1201B) {
                    jmps.push_back(0x32);
                }
            }
        }
    }
    
    for (size_t i = 0; i < jmps.size(); i++) {
        uint32_t jmp = *(uint32_t*)&newBuffer[jmps[i]];
        offset = 0;
        for (size_t j = 0; j < Sentences.size() && Sentences[j].addr < jmp + 0x51; j++) { //may be different from game to game even from script to script
            //for example, i use "j < Sentences.size() && Sentences[j].addr < jmp + 0x04" when translating 君と恋して結ばれて
            offset += Sentences[j].offset;
        }
        jmp += offset;
        *(uint32_t*)&newBuffer[jmps[i]] = jmp;
    }
    if (newBuffer.size() < buffer.size()) {
        //newBuffer.insert(newBuffer.end(), buffer.begin() + newBuffer.size(), buffer.end());
    }

    // 写入新文件
    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::cout << "Write-back complete. Output saved to " << outputBinPath.string() << std::endl;
}

void printUsage() {
    std::cout << "Made by julixian 2025.05.10" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  Dump:   ./program dump <input_folder> <output_folder>" << std::endl;
    std::cout << "  Inject: ./program inject <input_orgi-bin_folder> <input_translated-txt_folder> <output_folder> " << std::endl;
}

int main(int argc, char* argv[]) {
    //system("chcp 932");
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
                    std::cerr << "Warning: No corresponding file found for " << relativePath.string() << std::endl;
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