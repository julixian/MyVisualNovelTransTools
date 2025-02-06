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
size_t setScriptBegin = 0;

BOOL isOption1(const std::vector<uint8_t>& buffer, size_t i) {
    int Optioncount = 0;
    bool istruese = false;
    uint8_t seq = 0x01;
    while (buffer[i] == 0x01 && buffer[i + 1] == 0xff && buffer[i + 2] == seq && buffer[i + 5] == 0x00) {
        seq += 0x01;
        Optioncount++;
        if (buffer[i + 6] == 0x04) istruese = true;
        i += 6;
    }
    return Optioncount >= 2 && istruese;
}

BOOL isOption2(const std::vector<uint8_t>& buffer, size_t i, size_t ScriptBegin) {
    BOOL istruese = true;
    uint8_t seq = 0x00;
    while (buffer[i] == 0x00 && buffer[i + 1] == 0x00 && buffer[i + 2] == 0xff && buffer[i + 3] == seq) {
        seq += 0x01;
        i += 4;
        std::vector<uint8_t> temp;
        temp.push_back(buffer[i + 1]);
        temp.push_back(buffer[i]);
        uint16_t offset = 0;
        memcpy(&offset, &temp[0], sizeof(uint16_t));
        if (ScriptBegin + offset >= buffer.size()) {
            istruese = false;
            break;
        }
        i += 2;
    }
    return istruese;
}

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
    size_t ScriptLength = 0;
    memcpy(&ScriptLength, &buffer[0x8], sizeof(uint32_t));
    ScriptBegin = buffer.size() - ScriptLength;

    for (size_t i = 0; i < ScriptBegin; ++i) {

        if (buffer[i] == 0x01) {
            if (buffer[i + 1] == 0x00 && buffer[i + 2] == 0xff && buffer[i + 3] == 0x00 && buffer[i + 4] == 0x00 && buffer[i + 7] == 0x00 && buffer[i + 8] == 0x04) {
                uint16_t offset = 0;
                memcpy(&offset, &buffer[i + 5], sizeof(uint16_t));
                size_t SeAddr = ScriptBegin + offset;
                if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef || buffer[SeAddr - 1] != 0x00) {
                    
                }
                else {
                    std::string text = extractString(buffer, SeAddr);
                    output << text << "\n";
                    i += 8;
                    continue;
                }
            }
            if (buffer[i + 3] != 0x00)continue;
            if (buffer[i + 2] == 0xff)continue;
            if ((buffer[i + 4] == 0x04) || (buffer[i + 3] == 0x48) || (buffer[i + 4] != 0x00 && buffer[i + 6] == 0xff)) {
                uint16_t offset = 0;
                i++;
                memcpy(&offset, &buffer[i], sizeof(uint16_t));
                size_t SeAddr = ScriptBegin + offset;
                if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                    continue;
                }
                std::string text = extractString(buffer, SeAddr);
                output << text << "\n";
                i += 3;
            }
            else if (buffer[i + 4] == 0x00) {
                if (buffer[i + 6] == 0xff || buffer[i + 7] != 0x00)continue;
                if (buffer[i + 5] == 0x00 && buffer[i + 6] == 0x00)continue;
                uint16_t offset = 0;
                i++;
                memcpy(&offset, &buffer[i], sizeof(uint16_t));
                size_t SeAddr = ScriptBegin + offset;
                if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                    continue;
                }
                std::string text = extractString(buffer, SeAddr);
                output << text << "\n";
                i += 4;
                for (; buffer[i + 2] == 0x00; i += 4) {
                    memcpy(&offset, &buffer[i], sizeof(uint16_t));
                    SeAddr = ScriptBegin + offset;
                    if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                        continue;
                    }
                    std::string text2 = extractString(buffer, SeAddr);
                    output << text2 << "\n";
                    if (buffer[i + 3] != 0x00)break;
                }
                i += 3;
            }
            else if (buffer[i + 4] == 0x01) {
                if (buffer[i + 6] == 0xff || buffer[i + 7] != 0x00)continue;
                if (buffer[i + 5] == 0x00 && buffer[i + 6] == 0x00)continue;
                uint16_t offset = 0;
                i++;
                memcpy(&offset, &buffer[i], sizeof(uint16_t));
                size_t SeAddr = ScriptBegin + offset;
                if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                    continue;
                }
                std::string text = extractString(buffer, SeAddr);
                output << text << "\n";
                i += 3;
                while (buffer[i] == 0x01 && buffer[i + 3] == 0x00) {
                    i++;
                    memcpy(&offset, &buffer[i], sizeof(uint16_t));
                    SeAddr = ScriptBegin + offset;
                    if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                        continue;
                    }
                    std::string text2 = extractString(buffer, SeAddr);
                    output << text2 << "\n";
                    i += 3;
                }
            }

        }
    }

    input.close();
    output.close();

    std::cout << "Extraction complete. Output saved to " << outputPath << std::endl;
}

//IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII
void injectText_covering(const fs::path& inputBinPath, const fs::path& inputTxtPath, const fs::path& outputBinPath, bool iscovering) {
    std::ifstream inputBin(inputBinPath, std::ios::binary);
    std::ifstream inputTxt(inputTxtPath);
    std::ofstream outputBin(outputBinPath, std::ios::binary);

    if (!inputBin || !inputTxt || !outputBin) {
        std::cerr << "Error opening files: " << inputBinPath << " or " << inputTxtPath << " or " << outputBinPath << std::endl;
        return;
    }
    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(inputBin), {});
    std::vector<std::string> translations;
    std::vector<uint16_t> offsets;

    // 读取翻译文本
    std::string line;
    while (std::getline(inputTxt, line)) {
        translations.push_back(line);
    }

    size_t translationIndex = 0;
    std::vector<uint8_t> newBuffer;

    size_t ScriptBegin = 0;
    size_t ScriptLength = 0;
    memcpy(&ScriptLength, &buffer[0x8], sizeof(uint32_t));
    ScriptBegin = buffer.size() - ScriptLength;

    if (setScriptBegin != 0) {
        for (size_t i = 0; i < setScriptBegin; i++) {
            newBuffer.push_back(buffer[i]);
        }
    }
    else {
        if (iscovering) {
            for (size_t i = 0; i < ScriptBegin; i++) {
                newBuffer.push_back(buffer[i]);
            }
        }
        else {
            for (size_t i = 0; i < buffer.size(); i++) {
                newBuffer.push_back(buffer[i]);
            }
        }
    }

    for (size_t i = 0; i < ScriptBegin; ++i) {

        if (buffer[i] == 0x01) {
            if (buffer[i + 1] == 0x00 && buffer[i + 2] == 0xff && buffer[i + 3] == 0x00 && buffer[i + 4] == 0x00 && buffer[i + 7] == 0x00 && buffer[i + 8] == 0x04) {
                uint16_t offset = 0;
                memcpy(&offset, &buffer[i + 5], sizeof(uint16_t));
                size_t SeAddr = ScriptBegin + offset;
                if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef || buffer[SeAddr - 1] != 0x00) {

                }
                else {
                    i += 5;
                    std::string text = translations[translationIndex];
                    translationIndex++;
                    auto it = find(translations.begin(), translations.begin() + translationIndex - 1, text);
                    if (it == translations.begin() + translationIndex - 1) {
                        std::vector<uint8_t> textBytes = stringToCP932(text);
                        size_t transoffset = newBuffer.size() - ScriptBegin;
                        offsets.push_back(transoffset);
                        memcpy(&newBuffer[i], &transoffset, sizeof(uint16_t));
                        newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
                        newBuffer.push_back(0x00);
                        newBuffer.push_back(0x00);
                    }
                    else {
                        size_t transoffset = offsets[it - translations.begin()];
                        offsets.push_back(transoffset);
                        memcpy(&newBuffer[i], &transoffset, sizeof(uint16_t));
                    }
                    i += 3;
                    continue;
                }
            }
            if (buffer[i + 3] != 0x00)continue;
            if (buffer[i + 2] == 0xff)continue;
            if ((buffer[i + 4] == 0x04) || (buffer[i + 3] == 0x48) || (buffer[i + 4] != 0x00 && buffer[i + 6] == 0xff)) {
                i++;
                uint16_t offset = 0;
                memcpy(&offset, &buffer[i], sizeof(uint16_t));
                size_t SeAddr = ScriptBegin + offset;
                if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                    continue;
                }
                if (translationIndex >= translations.size()) {
                    std::cout << "Not Enough Translations！" << std::endl;
                    continue;
                }
                std::string text = translations[translationIndex];
                translationIndex++;
                auto it = find(translations.begin(), translations.begin() + translationIndex - 1, text);
                if (it == translations.begin() + translationIndex - 1) {
                    std::vector<uint8_t> textBytes = stringToCP932(text);
                    size_t transoffset = newBuffer.size() - ScriptBegin;
                    offsets.push_back(transoffset);
                    memcpy(&newBuffer[i], &transoffset, sizeof(uint16_t));
                    newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
                    newBuffer.push_back(0x00);
                    newBuffer.push_back(0x00);
                }
                else {
                    size_t transoffset = offsets[it - translations.begin()];
                    offsets.push_back(transoffset);
                    memcpy(&newBuffer[i], &transoffset, sizeof(uint16_t));
                }
                i += 3;
            }
            else if (buffer[i + 4] == 0x00) {
                if (buffer[i + 6] == 0xff || buffer[i + 7] != 0x00)continue;
                if (buffer[i + 5] == 0x00 && buffer[i + 6] == 0x00)continue;
                i++;
                uint16_t offset = 0;
                memcpy(&offset, &buffer[i], sizeof(uint16_t));
                size_t SeAddr = ScriptBegin + offset;
                if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                    continue;
                }
                if (translationIndex >= translations.size()) {
                    std::cout << "Not Enough Translations！" << std::endl;
                    continue;
                }
                std::string text = translations[translationIndex];
                translationIndex++;
                auto it = find(translations.begin(), translations.begin() + translationIndex - 1, text);
                if (it == translations.begin() + translationIndex - 1) {
                    std::vector<uint8_t> textBytes = stringToCP932(text);
                    size_t transoffset = newBuffer.size() - ScriptBegin;
                    offsets.push_back(transoffset);
                    memcpy(&newBuffer[i], &transoffset, sizeof(uint16_t));
                    newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
                    newBuffer.push_back(0x00);
                    newBuffer.push_back(0x00);
                }
                else {
                    size_t transoffset = offsets[it - translations.begin()];
                    offsets.push_back(transoffset);
                    memcpy(&newBuffer[i], &transoffset, sizeof(uint16_t));
                }
                i += 4;
                for (; buffer[i + 2] == 0x00; i += 4) {
                    memcpy(&offset, &buffer[i], sizeof(uint16_t));
                    SeAddr = ScriptBegin + offset;
                    if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                        continue;
                    }
                    if (translationIndex >= translations.size()) {
                        std::cout << "Not Enough Translations！" << std::endl;
                        continue;
                    }
                    std::string text2 = translations[translationIndex];
                    translationIndex++;
                    auto it = find(translations.begin(), translations.begin() + translationIndex - 1, text2);
                    if (it == translations.begin() + translationIndex - 1) {
                        std::vector<uint8_t> textBytes2 = stringToCP932(text2);
                        size_t transoffset2 = newBuffer.size() - ScriptBegin;
                        offsets.push_back(transoffset2);
                        memcpy(&newBuffer[i], &transoffset2, sizeof(uint16_t));
                        newBuffer.insert(newBuffer.end(), textBytes2.begin(), textBytes2.end());
                        newBuffer.push_back(0x00);
                        newBuffer.push_back(0x00);
                    }
                    else {
                        size_t transoffset2 = offsets[it - translations.begin()];
                        offsets.push_back(transoffset2);
                        memcpy(&newBuffer[i], &transoffset2, sizeof(uint16_t));
                    }
                    if (buffer[i + 3] != 0x00)break;
                }
                i += 3;
            }
            else if (buffer[i + 4] == 0x01) {
                if (buffer[i + 6] == 0xff || buffer[i + 7] != 0x00)continue;
                if (buffer[i + 5] == 0x00 && buffer[i + 6] == 0x00)continue;
                i++;
                uint16_t offset = 0;
                memcpy(&offset, &buffer[i], sizeof(uint16_t));
                size_t SeAddr = ScriptBegin + offset;
                if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                    continue;
                }
                if (translationIndex >= translations.size()) {
                    std::cout << "Not Enough Translations！" << std::endl;
                    continue;
                }
                std::string text = translations[translationIndex];
                translationIndex++;
                auto it = find(translations.begin(), translations.begin() + translationIndex - 1, text);
                if (it == translations.begin() + translationIndex - 1) {
                    std::vector<uint8_t> textBytes = stringToCP932(text);
                    size_t transoffset = newBuffer.size() - ScriptBegin;
                    offsets.push_back(transoffset);
                    memcpy(&newBuffer[i], &transoffset, sizeof(uint16_t));
                    newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
                    newBuffer.push_back(0x00);
                    newBuffer.push_back(0x00);
                }
                else {
                    size_t transoffset = offsets[it - translations.begin()];
                    offsets.push_back(transoffset);
                    memcpy(&newBuffer[i], &transoffset, sizeof(uint16_t));
                }
                i += 3;
                while (buffer[i] == 0x01 && buffer[i + 3] == 0x00) {
                    i++;
                    memcpy(&offset, &buffer[i], sizeof(uint16_t));
                    SeAddr = ScriptBegin + offset;
                    if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                        continue;
                    }
                    if (translationIndex >= translations.size()) {
                        std::cout << "Not Enough Translations！" << std::endl;
                        continue;
                    }
                    std::string text2 = translations[translationIndex];
                    translationIndex++;
                    auto it = find(translations.begin(), translations.begin() + translationIndex - 1, text2);
                    if (it == translations.begin() + translationIndex - 1) {
                        std::vector<uint8_t> textBytes2 = stringToCP932(text2);
                        size_t transoffset2 = newBuffer.size() - ScriptBegin;
                        offsets.push_back(transoffset2);
                        memcpy(&newBuffer[i], &transoffset2, sizeof(uint16_t));
                        newBuffer.insert(newBuffer.end(), textBytes2.begin(), textBytes2.end());
                        newBuffer.push_back(0x00);
                        newBuffer.push_back(0x00);
                    }
                    else {
                        size_t transoffset2 = offsets[it - translations.begin()];
                        offsets.push_back(transoffset2);
                        memcpy(&newBuffer[i], &transoffset2, sizeof(uint16_t));
                    }
                    i += 3;
                }
            }
        }
    }

    if (newBuffer.size() < buffer.size()) {  //what???
        size_t paddingsize = buffer.size() - newBuffer.size();
        newBuffer.insert(newBuffer.end(), buffer.end() - paddingsize, buffer.end());
    }

    // 写入新文件
    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::cout << "Write-back complete. Output saved to " << outputBinPath << std::endl;
}

//D2D2D2D2DD2D2D2D2DD2D2D2D2D2D2D2DD2D2D2D2D2D2D2DD2D2D2D2D2D2DD2D2D2D2DD2D2D2D2DD2D2D2D2
void dumpTextV2(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath);

    if (!input || !output) {
        std::cerr << "Error opening files: " << inputPath << " or " << outputPath << std::endl;
        return;
    }

    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});

    size_t ScriptBegin = 0;
    size_t ScriptLength = 0;
    memcpy(&ScriptLength, &buffer[0x8], sizeof(uint32_t));
    ScriptBegin = buffer.size() - ScriptLength;

    for (size_t i = 0; i < ScriptBegin; ++i) {

        if (buffer[i] == 0x01 && buffer[i + 1] == 0xff) {
            if (buffer[i + 2] != 0x00 || buffer[i + 3] != 0x00 || buffer[i + 6] != 0x00)continue;
            if (buffer[i + 5] == 0xff)continue;
            if (buffer[i + 7] == 0x04) {
                uint16_t offset = 0;
                i += 4;
                memcpy(&offset, &buffer[i], sizeof(uint16_t));
                size_t SeAddr = ScriptBegin + offset;
                if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                    continue;
                }
                std::string text = extractString(buffer, SeAddr);
                output << text << "\n";
                i += 3;
            }
            else if ((buffer[i + 7] == 0x00 || buffer[i + 7] == 0x01) && buffer[i + 8] == 0xff && buffer[i + 9] == 0x01) {
                if (buffer[i + 10] != 0x00)continue;
                i += 4;
                uint16_t offset = 0;
                memcpy(&offset, &buffer[i], sizeof(uint16_t));
                size_t SeAddr = ScriptBegin + offset;
                if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                    continue;
                }
                std::string text = extractString(buffer, SeAddr);
                output << text << "\n";
                i += 4;
                uint8_t seq = 0x01;
                while (buffer[i] == 0xff && buffer[i + 1] == seq && buffer[i + 2] == 0x00 && buffer[i + 5] == 0x00) {
                    seq += 0x01;
                    i += 3;
                    memcpy(&offset, &buffer[i], sizeof(uint16_t));
                    SeAddr = ScriptBegin + offset;
                    if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                        continue;
                    }
                    std::string text2 = extractString(buffer, SeAddr);
                    output << text2 << "\n";
                    i += 4;
                }
            }

        }
    }

    input.close();
    output.close();

    std::cout << "Extraction complete. Output saved to " << outputPath << std::endl;
}

//I2I2I2II2I2I2I2I2II2I2I2I2I2I2I2II2I2I2I2I2I2I2II2I2I2I2I2I2II2I2I2I2I2I22I2I2I2I2II2I2I2I2I2I
void injectTextV2_covering(const fs::path& inputBinPath, const fs::path& inputTxtPath, const fs::path& outputBinPath, bool iscovering) {
    std::ifstream inputBin(inputBinPath, std::ios::binary);
    std::ifstream inputTxt(inputTxtPath);
    std::ofstream outputBin(outputBinPath, std::ios::binary);

    if (!inputBin || !inputTxt || !outputBin) {
        std::cerr << "Error opening files: " << inputBinPath << " or " << inputTxtPath << " or " << outputBinPath << std::endl;
        return;
    }
    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(inputBin), {});
    std::vector<std::string> translations;
    std::vector<uint16_t> offsets;

    // 读取翻译文本
    std::string line;
    while (std::getline(inputTxt, line)) {
        translations.push_back(line);
    }

    size_t translationIndex = 0;
    std::vector<uint8_t> newBuffer;

    size_t ScriptBegin = 0;
    size_t ScriptLength = 0;
    memcpy(&ScriptLength, &buffer[0x8], sizeof(uint32_t));
    ScriptBegin = buffer.size() - ScriptLength;

    if (setScriptBegin != 0) {
        for (size_t i = 0; i < setScriptBegin; i++) {
            newBuffer.push_back(buffer[i]);
        }
    }
    else {
        if (iscovering) {
            for (size_t i = 0; i < ScriptBegin; i++) {
                newBuffer.push_back(buffer[i]);
            }
        }
        else {
            for (size_t i = 0; i < buffer.size(); i++) {
                newBuffer.push_back(buffer[i]);
            }
        }
    }

    for (size_t i = 0; i < ScriptBegin; ++i) {

        if (buffer[i] == 0x01 && buffer[i + 1] == 0xff) {
            if (buffer[i + 2] != 0x00 || buffer[i + 3] != 0x00 || buffer[i + 6] != 0x00)continue;
            if (buffer[i + 5] == 0xff)continue;
            if (buffer[i + 7] == 0x04) {
                i += 4;
                uint16_t offset = 0;
                memcpy(&offset, &buffer[i], sizeof(uint16_t));
                size_t SeAddr = ScriptBegin + offset;
                if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                    continue;
                }
                if (translationIndex >= translations.size()) {
                    std::cout << "Not Enough Translations！" << std::endl;
                    continue;
                }
                std::string text = translations[translationIndex];
                translationIndex++;
                auto it = find(translations.begin(), translations.begin() + translationIndex - 1, text);
                if (it == translations.begin() + translationIndex - 1) {
                    std::vector<uint8_t> textBytes = stringToCP932(text);
                    size_t transoffset = newBuffer.size() - ScriptBegin;
                    offsets.push_back(transoffset);
                    memcpy(&newBuffer[i], &transoffset, sizeof(uint16_t));
                    newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
                    newBuffer.push_back(0x00);
                    newBuffer.push_back(0x00);
                }
                else {
                    size_t transoffset = offsets[it - translations.begin()];
                    offsets.push_back(transoffset);
                    memcpy(&newBuffer[i], &transoffset, sizeof(uint16_t));
                }
                i += 3;
            }
            else if ((buffer[i + 7] == 0x00 || buffer[i + 7] == 0x01) && buffer[i + 8] == 0xff && buffer[i + 9] == 0x01) {
                if (buffer[i + 10] != 0x00)continue;
                i += 4;
                uint16_t offset = 0;
                memcpy(&offset, &buffer[i], sizeof(uint16_t));
                size_t SeAddr = ScriptBegin + offset;
                if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                    continue;
                }
                if (translationIndex >= translations.size()) {
                    std::cout << "Not Enough Translations！" << std::endl;
                    continue;
                }
                std::string text = translations[translationIndex];
                translationIndex++;
                auto it = find(translations.begin(), translations.begin() + translationIndex - 1, text);
                if (it == translations.begin() + translationIndex - 1) {
                    std::vector<uint8_t> textBytes = stringToCP932(text);
                    size_t transoffset = newBuffer.size() - ScriptBegin;
                    offsets.push_back(transoffset);
                    memcpy(&newBuffer[i], &transoffset, sizeof(uint16_t));
                    newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
                    newBuffer.push_back(0x00);
                    newBuffer.push_back(0x00);
                }
                else {
                    size_t transoffset = offsets[it - translations.begin()];
                    offsets.push_back(transoffset);
                    memcpy(&newBuffer[i], &transoffset, sizeof(uint16_t));
                }
                i += 4;
                uint8_t seq = 0x01;
                while (buffer[i] == 0xff && buffer[i + 1] == seq && buffer[i + 2] == 0x00 && buffer[i + 5] == 0x00) {
                    seq++;
                    i += 3;
                    memcpy(&offset, &buffer[i], sizeof(uint16_t));
                    SeAddr = ScriptBegin + offset;
                    if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                        continue;
                    }
                    if (translationIndex >= translations.size()) {
                        std::cout << "Not Enough Translations！" << std::endl;
                        continue;
                    }
                    std::string text3 = translations[translationIndex];
                    translationIndex++;
                    auto it = find(translations.begin(), translations.begin() + translationIndex - 1, text3);
                    if (it == translations.begin() + translationIndex - 1) {
                        std::vector<uint8_t> textBytes3 = stringToCP932(text3);
                        size_t transoffset3 = newBuffer.size() - ScriptBegin;
                        offsets.push_back(transoffset3);
                        memcpy(&newBuffer[i], &transoffset3, sizeof(uint16_t));
                        newBuffer.insert(newBuffer.end(), textBytes3.begin(), textBytes3.end());
                        newBuffer.push_back(0x00);
                        newBuffer.push_back(0x00);
                    }
                    else {
                        size_t transoffset3 = offsets[it - translations.begin()];
                        offsets.push_back(transoffset3);
                        memcpy(&newBuffer[i], &transoffset3, sizeof(uint16_t));
                    }
                    i += 4;
                }
            }
        }
    }

    if (newBuffer.size() < buffer.size()) {  //what???
        size_t paddingsize = buffer.size() - newBuffer.size();
        newBuffer.insert(newBuffer.end(), buffer.end() - paddingsize, buffer.end());
    }

    // 写入新文件
    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::cout << "Write-back complete. Output saved to " << outputBinPath << std::endl;
}

//D3D3D3D3D3D3D3D3D3D3D3D3D3D3DD3D3D3D3D3DD3D3D3D3D3D3D3333333D3D3D3D3D3D3DD3D3D3D3D3D3D
void dumpTextV3(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath);

    if (!input || !output) {
        std::cerr << "Error opening files: " << inputPath << " or " << outputPath << std::endl;
        return;
    }

    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});

    size_t ScriptBegin = 0;
    size_t ScriptLength = 0;
    memcpy(&ScriptLength, &buffer[0x8], sizeof(uint32_t));
    ScriptBegin = buffer.size() - ScriptLength;

    for (size_t i = 0; i < ScriptBegin; ++i) {

        if (buffer[i] == 0x01 && buffer[i + 1] == 0xff) {
            if (isOption1(buffer, i)) {
                uint8_t seq = 0x01;
                while (buffer[i] == 0x01 && buffer[i + 1] == 0xff && buffer[i + 2] == seq && buffer[i + 5] == 0x00) {
                    seq += 0x01;
                    i += 3;
                    uint16_t offset = 0;
                    std::vector<uint8_t> temp;
                    temp.push_back(buffer[i + 1]);
                    temp.push_back(buffer[i]);
                    memcpy(&offset, &temp[0], sizeof(uint16_t));
                    temp.clear();
                    size_t SeAddr = ScriptBegin + offset;
                    if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                        continue;
                    }
                    std::string text = extractString(buffer, SeAddr);
                    output << text << "\n";
                    i += 3;
                }
                continue;
            }
            if (buffer[i + 2] != 0x00 || buffer[i + 3] == 0xff || buffer[i + 5] != 0x00)continue;
            if (buffer[i + 6] == 0x04) {
                uint16_t offset = 0;
                i += 3;
                std::vector<uint8_t> temp;
                temp.push_back(buffer[i + 1]);
                temp.push_back(buffer[i]);
                memcpy(&offset, &temp[0], sizeof(uint16_t));
                temp.clear();
                size_t SeAddr = ScriptBegin + offset;
                if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                    continue;
                }
                std::string text = extractString(buffer, SeAddr);
                output << text << "\n";
                i += 3;
            }
            else if ((buffer[i + 6] == 0x00 || buffer[i + 6] == 0x01) && buffer[i + 7] == 0xff && buffer[i + 8] == 0x01) {
                i += 3;
                uint16_t offset = 0;
                std::vector<uint8_t> temp;
                temp.push_back(buffer[i + 1]);
                temp.push_back(buffer[i]);
                memcpy(&offset, &temp[0], sizeof(uint16_t));
                temp.clear();
                size_t SeAddr = ScriptBegin + offset;
                if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                    continue;
                }
                std::string text = extractString(buffer, SeAddr);
                output << text << "\n";
                i += 4;
                uint8_t seq = 0x01;
                while (buffer[i] == 0xff && buffer[i + 1] == seq) {
                    seq += 0x1;
                    i += 2;
                    temp.push_back(buffer[i + 1]);
                    temp.push_back(buffer[i]);
                    memcpy(&offset, &temp[0], sizeof(uint16_t));
                    temp.clear();
                    SeAddr = ScriptBegin + offset;
                    if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                        continue;
                    }
                    std::string text2 = extractString(buffer, SeAddr);
                    output << text2 << "\n";
                    i += 4;
                }
            }

        }
        else if (buffer[i] == 0xff && buffer[i - 1] == 0x00 && buffer[i + 1] == 0x00 && buffer[i + 5] == 0x0a && buffer[i + 6] == 0xff && buffer[i + 7] == 0xff) {
            i += 2;
            uint16_t offset = 0;
            std::vector<uint8_t> temp;
            temp.push_back(buffer[i + 1]);
            temp.push_back(buffer[i]);
            memcpy(&offset, &temp[0], sizeof(uint16_t));
            temp.clear();
            size_t SeAddr = ScriptBegin + offset;
            if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                continue;
            }
            std::string text3 = extractString(buffer, SeAddr);
            output << text3 << "\n";
            i += 5;
        }
        else if (buffer[i] == 0x00 && buffer[i + 1] == 0x00 && buffer[i + 2] == 0xff && buffer[i + 3] == 0x00 && isOption2(buffer, i, ScriptBegin)) {
            uint8_t seq = 0x00;
            while (buffer[i] == 0x00 && buffer[i + 1] == 0x00 && buffer[i + 2] == 0xff && buffer[i + 3] == seq) {
                seq += 0x01;
                i += 4;
                uint16_t offset = 0;
                std::vector<uint8_t> temp;
                temp.push_back(buffer[i + 1]);
                temp.push_back(buffer[i]);
                memcpy(&offset, &temp[0], sizeof(uint16_t));
                temp.clear();
                size_t SeAddr = ScriptBegin + offset;
                std::string text3 = extractString(buffer, SeAddr);
                output << text3 << "\n";
                i += 2;
            }
        }

    }

    input.close();
    output.close();

    std::cout << "Extraction complete. Output saved to " << outputPath << std::endl;
}

void injectTextV3_covering(const fs::path& inputBinPath, const fs::path& inputTxtPath, const fs::path& outputBinPath, bool iscovering) {
    std::ifstream inputBin(inputBinPath, std::ios::binary);
    std::ifstream inputTxt(inputTxtPath);
    std::ofstream outputBin(outputBinPath, std::ios::binary);

    if (!inputBin || !inputTxt || !outputBin) {
        std::cerr << "Error opening files: " << inputBinPath << " or " << inputTxtPath << " or " << outputBinPath << std::endl;
        return;
    }
    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(inputBin), {});
    std::vector<std::string> translations;
    std::vector<uint16_t> offsets;

    // 读取翻译文本
    std::string line;
    while (std::getline(inputTxt, line)) {
        translations.push_back(line);
    }

    size_t translationIndex = 0;
    std::vector<uint8_t> newBuffer;

    size_t ScriptBegin = 0;
    size_t ScriptLength = 0;
    memcpy(&ScriptLength, &buffer[0x8], sizeof(uint32_t));
    ScriptBegin = buffer.size() - ScriptLength;

    if (setScriptBegin != 0) {
        for (size_t i = 0; i < setScriptBegin; i++) {
            newBuffer.push_back(buffer[i]);
        }
    }
    else {
        if (iscovering) {
            for (size_t i = 0; i < ScriptBegin; i++) {
                newBuffer.push_back(buffer[i]);
            }
        }
        else {
            for (size_t i = 0; i < buffer.size(); i++) {
                newBuffer.push_back(buffer[i]);
            }
        }
    }
    
    for (size_t i = 0; i < ScriptBegin; ++i) {

        if (buffer[i] == 0x01 && buffer[i + 1] == 0xff) {
            if (isOption1(buffer, i)) {
                uint8_t seq = 0x01;
                while (buffer[i] == 0x01 && buffer[i + 1] == 0xff && buffer[i + 2] == seq && buffer[i + 5] == 0x00) {
                    seq += 0x01;
                    i += 3;
                    uint16_t offset = 0;
                    std::vector<uint8_t> temp;
                    temp.push_back(buffer[i + 1]);
                    temp.push_back(buffer[i]);
                    memcpy(&offset, &temp[0], sizeof(uint16_t));
                    temp.clear();
                    size_t SeAddr = ScriptBegin + offset;
                    if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                        continue;
                    }
                    std::string text = translations[translationIndex];
                    translationIndex++;
                    auto it = find(translations.begin(), translations.begin() + translationIndex - 1, text);
                    if (it == translations.begin() + translationIndex - 1) {
                        std::vector<uint8_t> textBytes = stringToCP932(text);
                        size_t transoffset = newBuffer.size() - ScriptBegin;
                        offsets.push_back(transoffset);
                        memcpy(&newBuffer[i], &transoffset, sizeof(uint16_t));
                        std::swap(newBuffer[i], newBuffer[i + 1]);
                        newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
                        newBuffer.push_back(0x00);
                        newBuffer.push_back(0x00);
                    }
                    else {
                        size_t transoffset = offsets[it - translations.begin()];
                        offsets.push_back(transoffset);
                        memcpy(&newBuffer[i], &transoffset, sizeof(uint16_t));
                        std::swap(newBuffer[i], newBuffer[i + 1]);
                    }
                    i += 3;
                }
                continue;
            }
            if (buffer[i + 2] != 0x00 || buffer[i + 3] == 0xff || buffer[i + 5] != 0x00)continue;
            if (buffer[i + 6] == 0x04) {
                i += 3;
                uint16_t offset = 0;
                std::vector<uint8_t> temp;
                temp.push_back(buffer[i + 1]);
                temp.push_back(buffer[i]);
                memcpy(&offset, &temp[0], sizeof(uint16_t));
                temp.clear();
                size_t SeAddr = ScriptBegin + offset;
                if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                    continue;
                }
                if (translationIndex >= translations.size()) {
                    std::cout << "Not Enough Translations！" << std::endl;
                    continue;
                }
                std::string text = translations[translationIndex];
                translationIndex++;
                auto it = find(translations.begin(), translations.begin() + translationIndex - 1, text);
                if (it == translations.begin() + translationIndex - 1) {
                    std::vector<uint8_t> textBytes = stringToCP932(text);
                    size_t transoffset = newBuffer.size() - ScriptBegin;
                    offsets.push_back(transoffset);
                    memcpy(&newBuffer[i], &transoffset, sizeof(uint16_t));
                    std::swap(newBuffer[i], newBuffer[i + 1]);
                    newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
                    newBuffer.push_back(0x00);
                    newBuffer.push_back(0x00);
                }
                else {
                    size_t transoffset = offsets[it - translations.begin()];
                    offsets.push_back(transoffset);
                    memcpy(&newBuffer[i], &transoffset, sizeof(uint16_t));
                    std::swap(newBuffer[i], newBuffer[i + 1]);
                }
                i += 3;
            }
            else if ((buffer[i + 6] == 0x00 || buffer[i + 6] == 0x01) && buffer[i + 7] == 0xff && buffer[i + 8] == 0x01) {
                i += 3;
                uint16_t offset = 0;
                std::vector<uint8_t> temp;
                temp.push_back(buffer[i + 1]);
                temp.push_back(buffer[i]);
                memcpy(&offset, &temp[0], sizeof(uint16_t));
                temp.clear();
                size_t SeAddr = ScriptBegin + offset;
                if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                    continue;
                }
                if (translationIndex >= translations.size()) {
                    std::cout << "Not Enough Translations！" << std::endl;
                    continue;
                }
                std::string text = translations[translationIndex];
                translationIndex++;
                auto it = find(translations.begin(), translations.begin() + translationIndex - 1, text);
                if (it == translations.begin() + translationIndex - 1) {
                    std::vector<uint8_t> textBytes = stringToCP932(text);
                    size_t transoffset = newBuffer.size() - ScriptBegin;
                    offsets.push_back(transoffset);
                    memcpy(&newBuffer[i], &transoffset, sizeof(uint16_t));
                    std::swap(newBuffer[i], newBuffer[i + 1]);
                    newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
                    newBuffer.push_back(0x00);
                    newBuffer.push_back(0x00);
                }
                else {
                    size_t transoffset = offsets[it - translations.begin()];
                    offsets.push_back(transoffset);
                    memcpy(&newBuffer[i], &transoffset, sizeof(uint16_t));
                    std::swap(newBuffer[i], newBuffer[i + 1]);
                }
                i += 4;
                uint8_t seq = 0x01;
                while (buffer[i] == 0xff && buffer[i + 1] == seq) {
                    seq++;
                    i += 2;
                    temp.push_back(buffer[i + 1]);
                    temp.push_back(buffer[i]);
                    memcpy(&offset, &temp[0], sizeof(uint16_t));
                    temp.clear();
                    size_t SeAddr = ScriptBegin + offset;
                    if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                        continue;
                    }
                    if (translationIndex >= translations.size()) {
                        std::cout << "Not Enough Translations！" << std::endl;
                        continue;
                    }
                    std::string text3 = translations[translationIndex];
                    translationIndex++;
                    auto it = find(translations.begin(), translations.begin() + translationIndex - 1, text3);
                    if (it == translations.begin() + translationIndex - 1) {
                        std::vector<uint8_t> textBytes3 = stringToCP932(text3);
                        size_t transoffset3 = newBuffer.size() - ScriptBegin;
                        offsets.push_back(transoffset3);
                        memcpy(&newBuffer[i], &transoffset3, sizeof(uint16_t));
                        std::swap(newBuffer[i], newBuffer[i + 1]);
                        newBuffer.insert(newBuffer.end(), textBytes3.begin(), textBytes3.end());
                        newBuffer.push_back(0x00);
                        newBuffer.push_back(0x00);
                    }
                    else {
                        size_t transoffset3 = offsets[it - translations.begin()];
                        offsets.push_back(transoffset3);
                        memcpy(&newBuffer[i], &transoffset3, sizeof(uint16_t));
                        std::swap(newBuffer[i], newBuffer[i + 1]);
                    }
                    i += 4;
                }
            }
        }
        else if (buffer[i] == 0xff && buffer[i - 1] == 0x00 && buffer[i + 1] == 0x00 && buffer[i + 5] == 0x0a && buffer[i + 6] == 0xff && buffer[i + 7] == 0xff) {
            i += 2;
            uint16_t offset = 0;
            std::vector<uint8_t> temp;
            temp.push_back(buffer[i + 1]);
            temp.push_back(buffer[i]);
            memcpy(&offset, &temp[0], sizeof(uint16_t));
            temp.clear();
            size_t SeAddr = ScriptBegin + offset;
            if (SeAddr >= buffer.size() || (buffer[SeAddr] < 0x81 && buffer[SeAddr] != 0x5b) || buffer[SeAddr] > 0xef) {
                continue;
            }
            if (translationIndex >= translations.size()) {
                std::cout << "Not Enough Translations！" << std::endl;
                continue;
            }
            uint8_t swap = 0;
            std::string text3 = translations[translationIndex];
            translationIndex++;
            auto it = find(translations.begin(), translations.begin() + translationIndex - 1, text3);
            if (it == translations.begin() + translationIndex - 1) {
                std::vector<uint8_t> textBytes3 = stringToCP932(text3);
                size_t transoffset3 = newBuffer.size() - ScriptBegin;
                offsets.push_back(transoffset3);
                memcpy(&newBuffer[i], &transoffset3, sizeof(uint16_t));
                std::swap(newBuffer[i], newBuffer[i + 1]);
                newBuffer.insert(newBuffer.end(), textBytes3.begin(), textBytes3.end());
                newBuffer.push_back(0x00);
                newBuffer.push_back(0x00);
            }
            else {
                size_t transoffset3 = offsets[it - translations.begin()];
                offsets.push_back(transoffset3);
                memcpy(&newBuffer[i], &transoffset3, sizeof(uint16_t));
                std::swap(newBuffer[i], newBuffer[i + 1]);
            }
            i += 5;
        }
        else if (buffer[i] == 0x00 && buffer[i + 1] == 0x00 && buffer[i + 2] == 0xff && buffer[i + 3] == 0x00 && isOption2(buffer, i, ScriptBegin)) {
            uint8_t seq = 0x00;
            while (buffer[i] == 0x00 && buffer[i + 1] == 0x00 && buffer[i + 2] == 0xff && buffer[i + 3] == seq) {
                seq += 0x01;
                i += 4;
                if (translationIndex >= translations.size()) {
                    std::cout << "Not Enough Translations！" << std::endl;
                    continue;
                }
                uint8_t swap = 0;
                std::string text3 = translations[translationIndex];
                translationIndex++;
                auto it = find(translations.begin(), translations.begin() + translationIndex - 1, text3);
                if (it == translations.begin() + translationIndex - 1) {
                    std::vector<uint8_t> textBytes3 = stringToCP932(text3);
                    size_t transoffset3 = newBuffer.size() - ScriptBegin;
                    offsets.push_back(transoffset3);
                    memcpy(&newBuffer[i], &transoffset3, sizeof(uint16_t));
                    std::swap(newBuffer[i], newBuffer[i + 1]);
                    newBuffer.insert(newBuffer.end(), textBytes3.begin(), textBytes3.end());
                    newBuffer.push_back(0x00);
                    newBuffer.push_back(0x00);
                }
                else {
                    size_t transoffset3 = offsets[it - translations.begin()];
                    offsets.push_back(transoffset3);
                    memcpy(&newBuffer[i], &transoffset3, sizeof(uint16_t));
                    std::swap(newBuffer[i], newBuffer[i + 1]);
                }
                i += 2;
            }
        }
    }

    if (newBuffer.size() < buffer.size()) {  //what???
        size_t paddingsize = buffer.size() - newBuffer.size();
        newBuffer.insert(newBuffer.end(), buffer.end() - paddingsize, buffer.end());
    }

    // 写入新文件
    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::cout << "Write-back complete. Output saved to " << outputBinPath << std::endl;
}

size_t parseInputValue(const std::string& input) {
    size_t value;
    if (input.substr(0, 2) == "0x") {
        // 十六进制输入
        std::istringstream iss(input.substr(2));
        iss >> std::hex >> value;
    }
    else {
        // 十进制输入
        value = std::stoull(input);
    }
    return value;
}

int main(int argc, char* argv[]) {

    if (argc < 4) {
        std::cout << "Made by julixian 2025.01.25" << std::endl;
        std::cout << "Usage:" << std::endl;
        std::cout << "For dump: program.exe <version> dump <input_folder> <output_folder>" << std::endl;
        std::cout << "For inject: program.exe <version> inject <input_bin_folder> <input_txt_folder> <output_folder> <covering/not_covering> [ScriptBegin]" << std::endl;
        std::cout << "version: v1, v2, or v3" << std::endl;
        std::cout << "covering: covering the original scripts, for uint16 often overflows, but cause some scripts disordered in v1 AND v2 because I have't analyze them" << std::endl;
        std::cout << "ScriptBegin: manually set where to begin writing the translated texts, able to use 0x for hex" << std::endl;
        std::cout << "v3: ????-2001\nv2: 2002 - 2005\nv1: 2006-????" << std::endl;
        return 1;
    }

    std::string version = argv[1];
    std::string mode = argv[2];

    if (version == "v1" || version == "v2" || version == "v3") {
        if (mode == "dump") {
            if (argc != 5) {
                std::cerr << "Invalid number of arguments for dump mode." << std::endl;
                return 1;
            }

            std::string inputFolder = argv[3];
            std::string outputFolder = argv[4];

            fs::create_directories(outputFolder);

            for (const auto& entry : fs::recursive_directory_iterator(inputFolder)) {
                if (fs::is_regular_file(entry)) {
                    fs::path relativePath = fs::relative(entry.path(), inputFolder);
                    fs::path outputPath = fs::path(outputFolder) / relativePath;
                    fs::create_directories(outputPath.parent_path());

                    if (version == "v1") {
                        dumpText(entry.path(), outputPath);
                    }
                    else if (version == "v2") {
                        dumpTextV2(entry.path(), outputPath);
                    }
                    else {
                        dumpTextV3(entry.path(), outputPath);
                    }
                }
            }
        }
        else if (mode == "inject") {
            if (argc != 7 && argc != 8) {
                std::cerr << "Invalid number of arguments for inject mode." << std::endl;
                return 1;
            }

            std::string inputBinFolder = argv[3];
            std::string inputTxtFolder = argv[4];
            std::string outputFolder = argv[5];
            bool covering = (argc >= 7 && std::string(argv[6]) == "covering");
            if (argc >= 8) {
                setScriptBegin = parseInputValue(std::string(argv[7]));
            }

            fs::create_directories(outputFolder);

            for (const auto& entry : fs::recursive_directory_iterator(inputBinFolder)) {
                if (fs::is_regular_file(entry)) {
                    fs::path relativePath = fs::relative(entry.path(), inputBinFolder);
                    fs::path txtPath = fs::path(inputTxtFolder) / relativePath;
                    fs::path outputPath = fs::path(outputFolder) / relativePath;
                    fs::create_directories(outputPath.parent_path());

                    if (fs::exists(txtPath)) {
                        if (version == "v1") {
                            injectText_covering(entry.path(), txtPath, outputPath, covering);
                        }
                        else if (version == "v2") {
                            injectTextV2_covering(entry.path(), txtPath, outputPath, covering);
                        }
                        else {
                            injectTextV3_covering(entry.path(), txtPath, outputPath, covering);
                        }
                    }
                    else {
                        std::cerr << "Warning: No corresponding file found for " << relativePath << std::endl;
                    }
                }
            }
        }
        else {
            std::cerr << "Invalid mode selected. Use 'dump' or 'inject'." << std::endl;
            return 1;
        }
    }
    else {
        std::cerr << "Invalid version selected. Use 'v1', 'v2', or 'v3'." << std::endl;
        return 1;
    }

    return 0;
}
