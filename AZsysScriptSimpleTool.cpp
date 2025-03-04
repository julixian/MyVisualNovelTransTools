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
    size_t j = 0;
    for (; buffer[i + j] != 0x00; ++j) {
        text.push_back(static_cast<char>(buffer[i + j]));
    }
    i += j;
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

    for (size_t i = ScriptBegin; i < buffer.size() - 24; i++) {
        if ((buffer[i] == 0x2a || buffer[i] == 0x29) && buffer[i + 1] == 0x00 && buffer[i + 2] == 0x00 && buffer[i + 3] == 0x00) {
            if (buffer[i + 24] <= 0x20 || buffer[i + 24] >= 0xef || buffer[i + 24] == 0x2a)continue;
            i += 4;
            uint32_t length = 0;
            memcpy(&length, &buffer[i], sizeof(uint32_t));
            if (length == 0x0)continue;
            i += 20;
            std::string text = extractString(buffer, i);
            output << text << "\n";
        }
        else if ((buffer[i] == 0x0d) && buffer[i + 1] == 0x00 && buffer[i + 2] == 0x00 && buffer[i + 3] == 0x00) {
            if (buffer[i + 24] <= 0x20 || buffer[i + 24] >= 0xef)continue;
            i += 4;
            uint32_t length = 0;
            memcpy(&length, &buffer[i], sizeof(uint32_t));
            if (length == 0x0)continue;
            i += 20;
            while (!(buffer[i] <= 0x20 || buffer[i] >= 0xef)) {
                std::string text = extractString(buffer, i);
                output << text << "\n";
                i++;
            }
            i--;
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

    int dvi = 0;

    for (size_t i = ScriptBegin; i < buffer.size(); i++) {
        if ((buffer[i] == 0x2a || buffer[i] == 0x29) && buffer[i + 1] == 0x00 && buffer[i + 2] == 0x00 && buffer[i + 3] == 0x00) {
            if (buffer[i + 24] <= 0x20 || buffer[i + 24] >= 0xef || buffer[i + 24] == 0x2a) {
                newBuffer.push_back(buffer[i]);
                continue;
            }
            bool is2a = buffer[i] == 0x2a;
            i += 4;
            uint32_t orgiTotalLength = 0;
            memcpy(&orgiTotalLength, &buffer[i], sizeof(uint32_t));
            if (orgiTotalLength == 0x0) {
                newBuffer.push_back(buffer[i - 4]);
                newBuffer.push_back(buffer[i - 3]);
                newBuffer.push_back(buffer[i - 2]);
                newBuffer.push_back(buffer[i - 1]);
                newBuffer.push_back(buffer[i]);
                continue;
            }
            if (translationIndex >= translations.size()) {
                std::cout << "Not Enough Translations！" << std::endl;
                newBuffer.push_back(buffer[i]);
                continue;
            }
            size_t k = i + 20;
            size_t orgiSelength = 0;
            while (buffer[k] != 0x00) {
                orgiSelength++;
                k++;
            }
            std::string text = translations[translationIndex];
            translationIndex++;
            size_t newSelength = text.length();
            uint32_t newTotalLength = orgiTotalLength;
            if (newSelength < orgiSelength) {
                size_t dv = orgiSelength - newSelength;
                newTotalLength -= dv;
                dvi -= dv;
            }
            else {
                size_t dv = newSelength - orgiSelength;
                newTotalLength += dv;
                dvi += dv;
            }
            if (is2a) {
                newBuffer.push_back(0x2a);
            }
            else {
                newBuffer.push_back(0x29);
            }
            newBuffer.push_back(0x00); newBuffer.push_back(0x00); newBuffer.push_back(0x00);
            newBuffer.push_back(0x00); newBuffer.push_back(0x00); newBuffer.push_back(0x00); newBuffer.push_back(0x00);
            memcpy(&newBuffer[newBuffer.size() - 4], &newTotalLength, sizeof(uint32_t));
            i += 4;
            for (size_t j = 0; j < 16; j++) {
                newBuffer.push_back(buffer[i]);
                i++;
            }
            std::vector<uint8_t> textBytes = stringToCP932(text);
            newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
            i += orgiSelength;
            newBuffer.push_back(buffer[i]);
        }
        else if ((buffer[i] == 0x0d) && buffer[i + 1] == 0x00 && buffer[i + 2] == 0x00 && buffer[i + 3] == 0x00) {
            if (buffer[i + 24] <= 0x20 || buffer[i + 24] >= 0xef) {
                newBuffer.push_back(buffer[i]);
                continue;
            }
            i += 4;
            uint32_t orgiTotalLength = 0;
            memcpy(&orgiTotalLength, &buffer[i], sizeof(uint32_t));
            if (orgiTotalLength == 0x0) {
                newBuffer.push_back(buffer[i - 4]);
                newBuffer.push_back(buffer[i - 3]);
                newBuffer.push_back(buffer[i - 2]);
                newBuffer.push_back(buffer[i - 1]);
                newBuffer.push_back(buffer[i]);
                continue;
            }
            size_t k = i + 20;
            size_t orgiSelength = 0;
            size_t OptionCount = 0;
            while (buffer[k] >= 0x20) {
                if (buffer[k + 1] != 0x00) {
                    orgiSelength++;
                    k++;
                }
                else {
                    OptionCount++;
                    orgiSelength++;
                    k += 2;
                }
            }
            size_t newSelength = 0;
            for (size_t f = 0; f < OptionCount; f++) {
                if (translationIndex + f >= translations.size()) {
                    std::cout << "Not Enough Translations！" << std::endl;
                }
                std::string text = translations[translationIndex + f];
                newSelength += text.length();
            }
            uint32_t newTotalLength = orgiTotalLength;
            if (newSelength < orgiSelength) {
                size_t dv = orgiSelength - newSelength;
                newTotalLength -= dv;
            }
            else {
                size_t dv = newSelength - orgiSelength;
                newTotalLength += dv;
            }
            newBuffer.push_back(0x0d); newBuffer.push_back(0x00); newBuffer.push_back(0x00); newBuffer.push_back(0x00);
            newBuffer.push_back(0x00); newBuffer.push_back(0x00); newBuffer.push_back(0x00); newBuffer.push_back(0x00);
            memcpy(&newBuffer[newBuffer.size() - 4], &newTotalLength, sizeof(uint32_t));
            i += 4;
            for (size_t j = 0; j < 16; j++) {
                newBuffer.push_back(buffer[i]);
                i++;
            }
            for (size_t f = 0; f < OptionCount; f++) {
                std::string text = translations[translationIndex];
                translationIndex++;
                std::vector<uint8_t> textBytes = stringToCP932(text);
                newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
                newBuffer.push_back(0x00);
            }
            i += orgiSelength + OptionCount;
            newBuffer.push_back(buffer[i]);
        }
        else if ((buffer[i] == 0x0b || buffer[i] == 0x0c) && buffer[i+1]==0x00 && buffer[i + 2] == 0x00 && buffer[i + 3] == 0x00 && buffer[i + 4] == 0x18 && buffer[i + 5] == 0x00 && buffer[i + 6] == 0x00 && buffer[i + 7] == 0x00) {
            newBuffer.push_back(buffer[i]);
            newBuffer.push_back(0x00); newBuffer.push_back(0x00); newBuffer.push_back(0x00);
            newBuffer.push_back(0x18); newBuffer.push_back(0x00); newBuffer.push_back(0x00); newBuffer.push_back(0x00);
            uint32_t orgiJump = 0;
            memcpy(&orgiJump, &buffer[i + 8], sizeof(uint32_t));
            uint32_t newJump = orgiJump + dvi;
            newBuffer.insert(newBuffer.end(), { 0, 0, 0, 0 });
            memcpy(&newBuffer[newBuffer.size() - 4], &newJump, sizeof(uint32_t));
            i += 12;
            newBuffer.push_back(buffer[i]);
        }
        else {
            newBuffer.push_back(buffer[i]);
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
    std::cout << "Made by julixian 2025.03.04" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  Dump:   ./program dump <input_folder> <output_folder>" << std::endl;
    std::cout << "  Inject: ./program inject <input_orgi-asb_folder> <input_translated-txt_folder> <output_folder>" << std::endl;
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