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

std::string extractString(const std::vector<uint8_t>& buffer, size_t& i, uint8_t length) {
    std::string text;
    size_t j;
    for (j = 0; j < length && i + j < buffer.size(); ++j) {
        if (buffer[i + j] == 0x00) break;
        text.push_back(static_cast<char>(buffer[i + j]));
    }
    i += j;
    return text;
}

void dumpText(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath);

    if (!input || !output) {
        std::cerr << "Error opening files: " << inputPath << " or " << outputPath << std::endl;
        return;
    }

    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});

    for (size_t i = 0; i < buffer.size() - 6; ++i) {

        if (buffer[i] == 0xF0 && buffer[i + 1] == 0x4C) {
            output << "PageChangeSign\n";
        }

        if ((buffer[i] == 0xF0 && buffer[i + 1] == 0x42) || (buffer[i] == 0xF0 && buffer[i + 1] == 0x00)) {

            uint32_t length = 0;
            i += 2;
            memcpy(&length, &buffer[i], sizeof(uint32_t));
            if (length == 0)continue;
            i += 4;
            if (i + length >= buffer.size()) continue;
            std::string text = extractString(buffer, i, length);
            output << text << "\n";
        }
    }

    input.close();
    output.close();

    std::cout << "Extraction complete. Output saved to " << outputPath << std::endl;
}



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
    std::string PCSIGN = "PageChangeSign";
    while (std::getline(inputTxt, line)) {
        if (line != PCSIGN) {
            translations.push_back(line);
        }
    }

    size_t translationIndex = 0;
    std::vector<uint8_t> newBuffer;

    if ((buffer[0x8] == 0x53 && buffer[0x9] == 0x65 && buffer[0xa] == 0x6c && buffer[0xb] == 0x65 && buffer[0xc] == 0x63 && buffer[0xd] == 0x74) || (buffer[0x8] == 0x43 && buffer[0x9] == 0x6E && buffer[0xa] == 0x67 && buffer[0xb] == 0x45 && buffer[0xc] == 0x78 && buffer[0xd] == 0x65) || (buffer[0x8] == 0x46 && buffer[0x9] == 0x34 && buffer[0xa] == 0x37)) {
        std::cout << "Find Select!" << std::endl;
        for (size_t i = 0x24; i < buffer.size(); i += 0x20) {
            size_t orgiAddr = 0;
            memcpy(&orgiAddr, &buffer[i], sizeof(uint32_t));
            if (orgiAddr >= buffer.size() || orgiAddr == 0x00000000) { break; }
            size_t orgiSelength = 0;
            size_t transSelength = 0;
            translationIndex = 0;
            for (size_t j = i; j < orgiAddr; ++j) {
                if ((buffer[j] == 0xF0 && buffer[j + 1] == 0x42) || (buffer[j] == 0xF0 && buffer[j + 1] == 0x00)) {

                    uint32_t orgilength = 0;
                    j += 2;
                    memcpy(&orgilength, &buffer[j], sizeof(uint32_t));
                    if (orgilength == 0) {
                        continue;
                    }
                    if (j + 4 + orgilength >= buffer.size()) {
                        continue;
                    }
                    if (translationIndex >= translations.size()) {
                        continue;
                    }
                    orgiSelength += orgilength;
                    std::string translatedText = translations[translationIndex++];
                    std::vector<uint8_t> textBytes = stringToCP932(translatedText);
                    transSelength += textBytes.size() + 1;
                    j += 3 + orgilength;
                }
                else {

                }
            }
            if (orgiSelength >= transSelength) {
                size_t dvalue = orgiSelength - transSelength;
                size_t transAddr = orgiAddr - dvalue;
                memcpy(&buffer[i], &transAddr, sizeof(uint32_t));
            }
            else {
                size_t dvalue = transSelength - orgiSelength;
                size_t transAddr = orgiAddr + dvalue;
                memcpy(&buffer[i], &transAddr, sizeof(uint32_t));
            }
        }
    }

    translationIndex = 0;

    for (size_t i = 0; i < buffer.size(); ++i) {
        if ((buffer[i] == 0xF0 && buffer[i + 1] == 0x42) || (buffer[i] == 0xF0 && buffer[i + 1] == 0x00)) {

            uint32_t orgilength = 0;
            i += 2;
            newBuffer.push_back(buffer[i - 2]);
            newBuffer.push_back(buffer[i - 1]);
            memcpy(&orgilength, &buffer[i], sizeof(uint32_t));
            if (orgilength == 0) {
                newBuffer.push_back(buffer[i]);
                continue;
            }
            if (i + 4 + orgilength >= buffer.size()) {
                newBuffer.push_back(buffer[i]);
                continue;
            }
            if (translationIndex >= translations.size()) {
                std::cerr << "Error: Not enough translations. Index: " << translationIndex
                    << ", Buffer position: 0x" << std::hex << i << std::dec << std::endl;
                newBuffer.push_back(buffer[i]);
                continue;
            }
            std::string translatedText = translations[translationIndex++];
            std::vector<uint8_t> textBytes = stringToCP932(translatedText);
            uint32_t tempLength = textBytes.size() + 1;
            newBuffer.push_back(0x00);
            newBuffer.push_back(0x00);
            newBuffer.push_back(0x00);
            newBuffer.push_back(0x00);
            memcpy(&newBuffer[newBuffer.size() - 4], &tempLength, sizeof(uint32_t));
            newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
            newBuffer.push_back(0x00);
            i += 3 + orgilength;
        }
        else {
            newBuffer.push_back(buffer[i]);
        }
    }

    //if (newBuffer.size() < buffer.size()) {  //what???
    //    size_t paddingsize = buffer.size() - newBuffer.size();
    //    newBuffer.insert(newBuffer.end(), buffer.end() - paddingsize, buffer.end());
    //}

    // 写入新文件
    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::cout << "Write-back complete. Output saved to " << outputBinPath << std::endl;
}

int main() {
    std::string mode;
    std::cout << "Choose mode (1 for dump, 2 for inject): ";
    std::getline(std::cin, mode);

    if (mode == "1") {
        std::string inputFolder, outputFolder;
        std::cout << "Enter the path of the input folder: ";
        std::getline(std::cin, inputFolder);
        std::cout << "Enter the path for the output folder: ";
        std::getline(std::cin, outputFolder);

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
    else if (mode == "2") {
        std::string inputBinFolder, inputTxtFolder, outputFolder;
        std::cout << "Enter the path of the folder containing original files: ";
        std::getline(std::cin, inputBinFolder);
        std::cout << "Enter the path of the folder containing translated files: ";
        std::getline(std::cin, inputTxtFolder);
        std::cout << "Enter the path for the output folder for new files: ";
        std::getline(std::cin, outputFolder);

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
        std::cerr << "Invalid mode selected." << std::endl;
        return 1;
    }
    system("pause");
    return 0;
}