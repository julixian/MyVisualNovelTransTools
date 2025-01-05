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

std::wstring cp932ToUnicode(const std::string& input) {
    if (input.empty()) return L"";
    int size_needed = MultiByteToWideChar(932, 0, &input[0], (int)input.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(932, 0, &input[0], (int)input.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string unicodeToCp932(const std::wstring& input) {
    if (input.empty()) return "";
    int size_needed = WideCharToMultiByte(932, 0, &input[0], (int)input.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(932, 0, &input[0], (int)input.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

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

bool isOptionStructure(const std::vector<uint8_t>& buffer, size_t pos) {
    int optionCount = 0;
    size_t i = pos;
    while (i < buffer.size() - 4 && (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {
        optionCount++;
        uint8_t length = buffer[i + 2];
        i += 4 + length;
    }
    return ((optionCount >= 2) && (buffer[i] == 0x0f && buffer[i + 1] == 0x08));  // 如果连续的0x801数量大于2，则认为是选项结构
}

bool isJumpStructure(const std::vector<uint8_t>& buffer, size_t i) {
    const uint8_t jumpPattern[] = { 0x10, 0x08, 0x56, 0x39, 0x72, 0x38, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00 };
    if (i + sizeof(jumpPattern) > buffer.size()) return false;
    return std::equal(jumpPattern, jumpPattern + sizeof(jumpPattern), buffer.begin() + i);
}

bool isemphasis(const std::vector<uint8_t>& buffer, size_t i) {
    const uint8_t jumpPattern[] = { 0x10, 0x08, 0x6A, 0xF2, 0x93, 0x2F, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00 };
    if (i + sizeof(jumpPattern) > buffer.size()) return false;
    return std::equal(jumpPattern, jumpPattern + sizeof(jumpPattern), buffer.begin() + i);
}

bool isPageChange(const std::vector<uint8_t>& buffer, size_t i) {
    return ((buffer[i] == 0x77 && buffer[i + 1] == 0x00 && buffer[i + 2] == 0x3a && buffer[i + 3] == 0x08) || (buffer[i] == 0x77 && buffer[i - 1] == 0x08 && buffer[i - 2] == 0x3a) || (buffer[i] == 0xf0 && buffer[i + 1] == 0xfd && buffer[i + 2] == 0x2a && buffer[i + 3] == 0x81) || (buffer[i] == 0x41 && buffer[i + 1] == 0x08 && buffer[i + 2] == 0x42 && buffer[i + 3] == 0x08 && buffer[i + 4] == 0x02 && buffer[i + 5] == 0x00 && buffer[i + 6] == 0x70 && buffer[i + 7] == 0x00));
}

bool isOptionlength(const std::vector<uint8_t>& buffer, size_t i) {
    return (((buffer[i + 2] == 0x31 && buffer[i + 3] == 0x08) || (buffer[i + 2] == 0x2c && buffer[i + 3] == 0x08) || (buffer[i + 2] == 0x2e && buffer[i + 3] == 0x08)) && buffer[i + 8] != 0x00 && buffer[i + 9] == 0x08);
}

bool isexFurigana(const std::vector<uint8_t>& buffer, size_t i) {
    if (buffer[i] != 0x01 || buffer[i + 1] != 0x08 || buffer[i+2]==0x01||buffer[i+2]==0x00||buffer[i+3]!=0x00)return false;
    uint8_t susbaselength = buffer[i + 2];
    i = i + 4 + susbaselength;
    const uint8_t jumpPattern[] = { 0x10, 0x08, 0x01, 0xfd, 0x98, 0x31, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00 };
    if (i + sizeof(jumpPattern) > buffer.size()) return false;
    return std::equal(jumpPattern, jumpPattern + sizeof(jumpPattern), buffer.begin() + i);
}

size_t calcuoriginalSelength(const std::vector<uint8_t>& buffer, size_t i, size_t j) {//计算每个PAGE的总长
    
    size_t Sbsign = 0;
    size_t Sesign = 0;
    int Separt = 0;
    size_t sumSelength = 0;

    for (; i < j; ++i) {

        if (isPageChange(buffer, i)) {
            if (Separt == 0) {
                continue;
            }
            else {
                sumSelength += Sesign - Sbsign + 1;
                Separt = 0;
                Sesign = 0;
                Sbsign = 0;
                continue;
            }
        }

        if ((buffer[i] == 0x40 && buffer[i + 1] == 0x08) || (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {

            uint8_t originalLength = buffer[i + 2];

            if (i + 4 + originalLength >= buffer.size()) {
                continue;
            }
            if (buffer[i + 3] != 0x00 || buffer[i + 2] == 0x01 || buffer[i + 2] == 0x00) continue;

            if (isexFurigana(buffer, i + 4 + originalLength) && (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {
                if (Sbsign == 0) {
                    Sbsign = i;
                }
                i += 4 + originalLength;
                uint8_t basetextlength = buffer[i + 2];
                i += 15 + basetextlength;
                Sesign = i;
                Separt++;
                continue;
            }
            else if (isemphasis(buffer, i + 4 + originalLength) && (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {
                if (Sbsign == 0) {
                    Sbsign = i;
                }
                i += 15 + originalLength;
                Sesign = i;
                Separt++;
                continue;
            }
            else if (isJumpStructure(buffer, i + 4 + originalLength) && (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {
                if (Sbsign == 0) {
                    Sbsign = i;
                }
                i += 15 + originalLength;
                Sesign = i;
                Separt++;
                continue;
            }
            else if (isOptionStructure(buffer, i) && (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {
                while (i < buffer.size() - 4 && (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {
                    uint8_t length = buffer[i + 2];
                    i += 4 + length;
                    sumSelength += length + 4;
                }
                i = i - 1;
                continue;
            }
            else if (buffer[i] == 0x40 && buffer[i + 1] == 0x08) {
                if (buffer[i + 4 + originalLength] == 0x3a && buffer[i + 5 + originalLength] == 0x08) {
                    sumSelength += originalLength + 4;
                }
                else {
                    if (Sbsign == 0) {
                        Sbsign = i;
                    }
                    Sesign = i + 3 + originalLength;
                    Separt++;
                }
                i += 3 + originalLength;
                continue;
            }
        }
        else {
            continue;
        }
    }
    if(Separt != 0) {
        sumSelength += Sesign - Sbsign + 1;
    }
    return sumSelength;
}

int calcuoriginalSenum(const std::vector<uint8_t>& buffer, size_t i, size_t j) {

    int Secount = 0;
    int Separt = 0;

    for (; i < j; ++i) {

        if (isPageChange(buffer, i)) {
            if (Separt == 0) {
                continue;
            }
            else {
                Secount++;
                Separt = 0;
                continue;
            }
        }

        if ((buffer[i] == 0x40 && buffer[i + 1] == 0x08) || (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {

            uint8_t originalLength = buffer[i + 2];

            // 检查是否有足够的字节来构成完整的文本结构
            if (i + 4 + originalLength >= buffer.size()) {
                continue;
            }
            if (buffer[i + 3] != 0x00 || buffer[i + 2] == 0x01 || buffer[i + 2] == 0x00) continue;

            if (isexFurigana(buffer, i + 4 + originalLength) && (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {
                i += 4 + originalLength;
                uint8_t basetextlength = buffer[i + 2];
                i += 3 + basetextlength;
                Separt++;
                continue;
            }
            else if (isemphasis(buffer, i + 4 + originalLength) && (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {
                i += 15 + originalLength;
                Separt++;
                continue;
            }
            else if (isJumpStructure(buffer, i + 4 + originalLength) && (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {
                i += 15 + originalLength;
                Separt++;
                continue;
            }
            else if (isOptionStructure(buffer, i) && (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {
                while (i < buffer.size() - 4 && (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {
                    uint8_t length = buffer[i + 2];
                    i += 4 + length;
                    Secount++;
                }
                i = i - 1;
                continue;
            }
            else if (buffer[i] == 0x40 && buffer[i + 1] == 0x08) {
                i += 3 + originalLength;
                if (buffer[i + 1] == 0x3a && buffer[i + 2] == 0x08) {
                    Secount++;
                }
                else {
                    Separt++;
                }
                continue;
            }
        }
        else {
            continue;
        }
    }
    if(Separt != 0) {
        Secount++;
    }
    return Secount;
}

std::vector<std::string> splitTranslation(const std::string& text) {
    std::wstring wtext = cp932ToUnicode(text);
    std::vector<std::wstring> wparts;
    std::wstring wcurrent;

    for (size_t i = 0; i < wtext.length(); ++i) {
        if (wtext[i] == L'[' || wtext[i] == L'{') {
            if (!wcurrent.empty()) {
                wparts.push_back(wcurrent);
                wcurrent.clear();
            }
            size_t end = wtext.find(wtext[i] == L'[' ? L']' : L'}', i);
            if (end != std::wstring::npos) {
                wparts.push_back(wtext.substr(i, end - i + 1));
                i = end;
            }
        }
        else if (wtext.substr(i, 4) == L"\\r\\n") {
            if (!wcurrent.empty()) {
                wparts.push_back(wcurrent);
                wcurrent.clear();
            }
            wparts.push_back(L"\\r\\n");
            i += 3;
        }
        else {
            wcurrent += wtext[i];
        }
    }
    if (!wcurrent.empty()) {
        wparts.push_back(wcurrent);
    }

    // Convert back to CP932
    std::vector<std::string> parts;
    for (const auto& wpart : wparts) {
        parts.push_back(unicodeToCp932(wpart));
    }

    return parts;
}

void dumpText(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath);

    if (!input || !output) {
        std::cerr << "Error opening files: " << inputPath << " or " << outputPath << std::endl;
        return;
    }

    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});

    for (size_t i = 0; i < buffer.size() - 4; ++i) {
        if (isPageChange(buffer, i)) {//4108420802007000
            output << "\n";
        }
        //41 08 42 08 02 00 6E 00 3A 08
        if (buffer[i] == 0x41 && buffer[i + 1] == 0x08 && buffer[i + 2] == 0x42 && buffer[i + 3] == 0x08 && buffer[i + 4] == 0x02 && buffer[i + 5] == 0x00 && buffer[i + 6] == 0x6e && buffer[i + 7] == 0x00) {
            output << "\\r\\n";
            i += 7;
            continue;
        }
        if ((buffer[i] == 0x40 && buffer[i + 1] == 0x08) || (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {
            
            uint8_t length = buffer[i + 2];
            if (buffer[i + 2] == 0x00 || buffer[i + 2] == 0x01) continue;
            if (buffer[i + 3] != 0x00) continue;
            
            if (buffer[i] == 0x40 && buffer[i + 1] == 0x08) {
                i += 4;
                if (i + length >= buffer.size()) break;
                std::string text = extractString(buffer, i, length);
                if (buffer[i] == 0x00) i++;
                output << text;
                if (buffer[i] == 0x3a && buffer[i + 1] == 0x08)output << "\n";
                i -= 1;
                continue;
            }
            else if (isOptionStructure(buffer, i) && (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {
                while (i < buffer.size() - 4 && (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {
                    uint8_t length = buffer[i + 2];
                    i += 4;
                    std::string optionText = extractString(buffer, i, length);
                    output << optionText << std::endl;
                    if (buffer[i] == 0x00) i++;
                }
                i = i - 1;
                continue;
            }
            else if (isJumpStructure(buffer, i + 4 + length) && (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {
                i += 4;
                if (i + length >= buffer.size()) break;
                std::string text = extractString(buffer, i, length);
                if (buffer[i] == 0x00) i++;
                output << "{" << text << "}";
                i += 11; // Skip jump structure
            }
            else if (isexFurigana(buffer,i + 4 + length) && (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {
                i += 4;
                if (i + length >= buffer.size()) break;
                std::string text = extractString(buffer, i, length);
                if (buffer[i] == 0x00) i++;
                std::string furigana = text;
                i += 2;
                length = buffer[i];
                i++;
                if (buffer[i] != 0x00) continue;
                i++;
                if (i + length >= buffer.size()) break;
                std::string baseText = extractString(buffer, i, length);
                if (!furigana.empty() && !baseText.empty()) {
                    output << "[" << furigana << "/" << baseText << "]";
                }
            }
            /*else if (buffer[i] == 0x01 && buffer[i + 1] == 0x08 && buffer[i + 4 + length] == 0x31 && buffer[i + 5 + length] == 0x08) {
                i += 4;
                if (i + length >= buffer.size()) break;
                std::string text = extractString(buffer, i, length);
                if (buffer[i] == 0x00) i++;
                output << text << std::endl;
                i -= 1;
                continue;
            }*/
            else if (buffer[i] == 0x01 && buffer[i + 1] == 0x08 && isemphasis(buffer, i + 4 + length)) {
                i += 4;
                if (i + length >= buffer.size()) break;
                std::string text = extractString(buffer, i, length);
                if (buffer[i] == 0x00) i++;
                output << "[";
                for (size_t i = 0; i < (length - 1) / 2; i++) {
                    output << "丒";
                }
                output << "/" << text << "]";
                i -= 1;
                continue;
            }
        }
    }

    input.close();
    output.close();

    std::cout << "Extraction complete. Output saved to " << outputPath << std::endl;
}

void injectNormalText(std::vector<uint8_t>& buffer, const std::string& text) {
    std::vector<uint8_t> textBytes = stringToCP932(text);
    buffer.push_back(0x40);
    buffer.push_back(0x08);
    buffer.push_back(static_cast<uint8_t>(textBytes.size() + 1));
    buffer.push_back(0x00);
    buffer.insert(buffer.end(), textBytes.begin(), textBytes.end());
    buffer.push_back(0x00);
}

void injectFurigana(std::vector<uint8_t>& buffer, const std::string& text) {
    size_t slashPos = text.find('/');
    if (slashPos != std::string::npos) {
        std::string furigana = text.substr(1, slashPos - 1);
        std::string baseText = text.substr(slashPos + 1, text.length() - slashPos - 2);

        std::vector<uint8_t> furiganaBytes = stringToCP932(furigana);
        std::vector<uint8_t> baseTextBytes = stringToCP932(baseText);

        // 振り仮名
        buffer.push_back(0x41);
        buffer.push_back(0x08);
        buffer.push_back(0x01);
        buffer.push_back(0x08);
        buffer.push_back(static_cast<uint8_t>(furiganaBytes.size() + 1));
        buffer.push_back(0x00);
        buffer.insert(buffer.end(), furiganaBytes.begin(), furiganaBytes.end());
        buffer.push_back(0x00);

        // 基本文本
        buffer.push_back(0x01);
        buffer.push_back(0x08);
        buffer.push_back(static_cast<uint8_t>(baseTextBytes.size() + 1));
        buffer.push_back(0x00);
        buffer.insert(buffer.end(), baseTextBytes.begin(), baseTextBytes.end());
        buffer.push_back(0x00);
        //{ 0x10, 0x08, 0x01, 0xfd, 0x98, 0x31, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00 }
        buffer.insert(buffer.end(), { 0x10, 0x08, 0x01, 0xfd, 0x98, 0x31, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00 });
        
    }
}

void injectJumpStructure(std::vector<uint8_t>& buffer, const std::string& text) {
    std::vector<uint8_t> textBytes = stringToCP932(text);
    buffer.push_back(0x41);
    buffer.push_back(0x08);
    buffer.push_back(0x01);
    buffer.push_back(0x08);
    buffer.push_back(static_cast<uint8_t>(textBytes.size() + 1));
    buffer.push_back(0x00);
    buffer.insert(buffer.end(), textBytes.begin(), textBytes.end());
    buffer.push_back(0x00);
    buffer.push_back(0x10);
    buffer.insert(buffer.end(), { 0x08, 0x56, 0x39, 0x72, 0x38, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00 });
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
    std::string PCSIGN = "";
    while (std::getline(inputTxt, line)) {
        if (line != PCSIGN) {
            translations.push_back(line);
        }
    }

    size_t translationIndex = 0;
    std::vector<uint8_t> newBuffer;

    size_t Pbsign = 0;
    size_t Sbsign = 0;
    size_t Sesign = 0;
    int Scount = 0;

    for (size_t i = 0; i < buffer.size(); ++i) {

        if (isPageChange(buffer, i) || i == buffer.size() - 1) {
            if (Scount == 0) {
                for (size_t j = Pbsign; j < i; j++) {
                    newBuffer.push_back(buffer[j]);
                }
                if (i == buffer.size() - 1) {
                    newBuffer.push_back(buffer[i]);
                }
                Pbsign = i;
            }
            else {
                for (size_t j = Pbsign; j < Sbsign; j++) {
                    newBuffer.push_back(buffer[j]);
                }
                //大代码#############################
                std::string translatedText = translations[translationIndex++];
                std::vector<std::string> parts = splitTranslation(translatedText);
                for (const auto& part : parts) {
                    if (part == "\\r\\n") {
                        // 处理换行
                        newBuffer.insert(newBuffer.end(), {0x41,0x08, 0x42, 0x08, 0x02, 0x00, 0x6e, 0x00, 0x3a, 0x08, 0xFF, 0x00 });
                        //newBuffer.insert(newBuffer.end(), {0x41,0x08, 0x42, 0x08, 0x02, 0x00, 0x6e, 0x00 });
                    }
                    else if (part.front() == '[' && part.back() == ']') {
                        // 处理标音
                        injectFurigana(newBuffer, part);
                    }
                    else if (part.front() == '{' && part.back() == '}') {
                        // 处理跳转结构
                        injectJumpStructure(newBuffer, part.substr(1, part.length() - 2));
                    }
                    else {
                        // 处理普通文本
                        injectNormalText(newBuffer, part);
                    }
                }
                for (size_t j = Sesign + 1; j < i; j++) {
                    newBuffer.push_back(buffer[j]);
                }
                if (i == buffer.size() - 1) {
                    newBuffer.push_back(buffer[i]);
                }
                Pbsign = i;
            }
            Scount = 0;
            Sbsign = 0;
            Sesign = 0;
            continue;
        }
        else if (isOptionStructure(buffer, i)) {
            if (Scount == 0) {
                for (size_t j = Pbsign; j < i; j++) {
                    newBuffer.push_back(buffer[j]);
                }
                if (translationIndex >= translations.size()) {
                    std::cerr << "Error: Not enough translations. Index: " << translationIndex
                        << ", Buffer position: 0x" << std::hex << i << std::dec << std::endl;
                    continue;
                }
                while (i < buffer.size() - 4 && (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {
                    uint8_t length = buffer[i + 2];
                    std::string translatedText = translations[translationIndex++];
                    /*if (translatedText.front() != '{' && translatedText.front() != '[') {
                        for (; translatedText.size() < length - 1;) {
                            translatedText += " ";
                        }
                    }*/
                    newBuffer.push_back(0x01);
                    newBuffer.push_back(0x08);
                    std::vector<uint8_t> textBytes = stringToCP932(translatedText);
                    newBuffer.push_back(static_cast<uint8_t>(textBytes.size() + 1));
                    newBuffer.push_back(0x00);
                    newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
                    newBuffer.push_back(0x00);
                    i += 4 + length;
                }
                Pbsign = i;
                i = i - 1;
                Scount = 0;
                Sbsign = 0;
                Sesign = 0;
                continue;
            }
            else {
                //感觉基本不可能，可以有需要再抄
            }
        }
        else if (buffer[i] == 0x40 && buffer[i + 1] == 0x08) {//如果是name
            if (buffer[i + 2] == 0x00 || buffer[i + 2] == 0x01 || buffer[i + 3] != 0x00) continue;
            uint8_t susnamelength = buffer[i + 2];
            if (buffer[i + 4 + susnamelength] == 0x3a && buffer[i + 5 + susnamelength] == 0x08) {
                for (size_t j = Pbsign; j < i; j++) {
                    newBuffer.push_back(buffer[j]);
                }
                std::string translatedText = translations[translationIndex++];
                newBuffer.push_back(0x40);
                newBuffer.push_back(0x08);
                std::vector<uint8_t> textBytes = stringToCP932(translatedText);
                newBuffer.push_back(static_cast<uint8_t>(textBytes.size() + 1));
                newBuffer.push_back(0x00);
                newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
                newBuffer.push_back(0x00);
                i += 3 + susnamelength;
                Pbsign = i + 1;
                Scount = 0;
                Sbsign = 0;
                Sesign = 0;
                continue;
            }
        }
        //else if (buffer[i] == 0x01 && buffer[i + 1] == 0x08) {//如果是tips
        //    if (buffer[i + 2] == 0x00 || buffer[i + 2] == 0x01 || buffer[i + 3] != 0x00) continue;
        //    uint8_t sustipslength = buffer[i + 2];
        //    if (buffer[i + 4 + sustipslength] == 0x31 && buffer[i + 5 + sustipslength] == 0x08) {
        //        for (size_t j = Pbsign; j < i; j++) {
        //            newBuffer.push_back(buffer[j]);
        //        }
        //        std::string translatedText = translations[translationIndex++];
        //        newBuffer.push_back(0x01);
        //        newBuffer.push_back(0x08);
        //        std::vector<uint8_t> textBytes = stringToCP932(translatedText);
        //        newBuffer.push_back(static_cast<uint8_t>(textBytes.size() + 1));
        //        newBuffer.push_back(0x00);
        //        newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
        //        newBuffer.push_back(0x00);
        //        i += 3 + sustipslength;
        //        Pbsign = i + 1;
        //        Scount = 0;
        //        Sbsign = 0;
        //        Sesign = 0;
        //        continue;
        //    }
        //}
        else if (isOptionlength(buffer, i) || i == 0x20 || i == 0x24) {      //可以用于更新一切跳转
            unsigned char bytes[4] = { buffer[i + 4], buffer[i + 5], buffer[i + 6], buffer[i + 7] };
            size_t originalOplength = 0;
            size_t transSelength = 0;
            std::memcpy(&originalOplength, bytes, sizeof(uint32_t));
            if (i + originalOplength >= buffer.size()) {
                originalOplength = 0xffffffff - originalOplength;
                if (originalOplength >= i)continue;
                size_t originalSelength = calcuoriginalSelength(buffer, i + 7 - originalOplength , i + 7);
                int Secount = calcuoriginalSenum(buffer, i + 7 - originalOplength, i + 7);
                for (int j = 1; j <= Secount; j++) {
                    std::string translatedText = translations[translationIndex - j];
                    std::vector<std::string> parts = splitTranslation(translatedText);
                    for (const auto& part : parts) {
                        if (part == "\\r\\n") {
                            transSelength += 12;
                        }
                        else if (part.front() == '[' && part.back() == ']') {
                            std::vector<uint8_t> textBytes = stringToCP932(part);
                            transSelength += textBytes.size() - 3 + 2 + 10 + 12;
                        }
                        else if (part.front() == '{' && part.back() == '}') {
                            std::vector<uint8_t> textBytes = stringToCP932(part);
                            transSelength += textBytes.size() - 2 + 1 + 12 + 6;
                        }
                        else {
                            std::vector<uint8_t> textBytes = stringToCP932(part);
                            transSelength += textBytes.size() + 1 + 4;
                        }
                    }
                }
                if (transSelength <= originalSelength) {
                    size_t dvalue = originalSelength - transSelength;
                    size_t transOplength = originalOplength - dvalue;
                    transOplength = 0xffffffff - transOplength;
                    std::memcpy(&buffer[i + 4], &transOplength, sizeof(uint32_t));
                }
                else {
                    size_t dvalue = transSelength - originalSelength;
                    size_t transOplength = originalOplength + dvalue;
                    transOplength = 0xffffffff - transOplength;
                    std::memcpy(&buffer[i + 4], &transOplength, sizeof(uint32_t));
                }
                continue;
            }
            //std::cout << "originalOplength" << originalOplength << std::endl;
            size_t originalSelength = calcuoriginalSelength(buffer, i + 8, i + 8 +originalOplength);
            //std::cout << "originalSelength=" << originalSelength << std::endl;
            int Secount = calcuoriginalSenum(buffer, i + 8, i + 8 + originalOplength);
            //std::cout << "Secount=" << Secount << std::endl;
            for (int j = 0; j < Secount; j++) {
                std::string translatedText = translations[translationIndex+j];
                std::vector<std::string> parts = splitTranslation(translatedText);
                for (const auto& part : parts) {
                    if (part == "\\r\\n") {
                        transSelength += 12;
                    }
                    else if (part.front() == '[' && part.back() == ']') {
                        std::vector<uint8_t> textBytes = stringToCP932(part);
                        transSelength += textBytes.size() - 3 + 2 + 10 + 12;
                    }
                    else if (part.front() == '{' && part.back() == '}') {
                        std::vector<uint8_t> textBytes = stringToCP932(part);
                        transSelength += textBytes.size() - 2 + 1 + 12 + 6;
                    }
                    else {
                        std::vector<uint8_t> textBytes = stringToCP932(part);
                        transSelength += textBytes.size() + 1 + 4;
                    }
                }
            }
            //std::cout << "transSelength=" << transSelength << std::endl;
            if (transSelength <= originalSelength) {
                size_t dvalue = originalSelength - transSelength;
                size_t transOplength = originalOplength - dvalue;
                std::memcpy(&buffer[i + 4], &transOplength, sizeof(uint32_t));
                //std::cout << "shorter" << std::endl;
            }
            else {
                size_t dvalue = transSelength - originalSelength;
                size_t transOplength = originalOplength + dvalue;
                std::memcpy(&buffer[i + 4], &transOplength, sizeof(uint32_t));
                //std::cout << "longer" << std::endl;
            }
        }

        //#############################################################################################
        //不是选项，不是PAGECHANGE，只用来conut和记录Ssign
        if ((buffer[i] == 0x40 && buffer[i + 1] == 0x08) || (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {

            uint8_t opcode = buffer[i];
            uint8_t subopcode = buffer[i + 1];
            uint8_t originalLength = buffer[i + 2];

            // 检查是否有足够的字节来构成完整的文本结构
            if (i + 4 + originalLength >= buffer.size()) {
                continue;
            }
            if (buffer[i + 3] != 0x00 || buffer[i + 2] == 0x01 || buffer[i + 2] == 0x00) continue; 
            
            if (isexFurigana(buffer, i + 4 + originalLength)&& (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {
                    if (Sbsign == 0)Sbsign = i;
                    Scount++;
                    i += 4 + originalLength;
                    uint8_t basetextlength = buffer[i + 2];
                    i += 3 + basetextlength;
                    Sesign = i;
                    continue;
            }
            else if (isJumpStructure(buffer, i + 4 + originalLength)&& (buffer[i] == 0x01 && buffer[i + 1] == 0x08)) {
                    if (Sbsign == 0)Sbsign = i;
                    Scount++;
                    i += 15 + originalLength;
                    Sesign = i;
                    continue;
            }
            else if (isemphasis(buffer, i + 4 + originalLength)) {
                if (Sbsign == 0)Sbsign = i;
                Scount++;
                i += 15 + originalLength;
                Sesign = i;
                continue;
            }
            else if (buffer[i] == 0x40 && buffer[i + 1] == 0x08) {
                    if (Sbsign == 0)Sbsign = i;
                    Scount++;
                    i += 3 + originalLength;
                    Sesign = i;
                    continue;
            }
        }
        else {
            continue;
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

int main() {
    std::string mode;
    std::cout << "Made by julixian 2025.01.01" << std::endl;
    std::cout << "Choose mode (1 for dump, 2 for inject): ";
    std::getline(std::cin, mode);

    if (mode == "1") {
        std::string inputFolder, outputFolder;
        std::cout << "Enter the path of the input folder containing .mjs files: ";
        std::getline(std::cin, inputFolder);
        std::cout << "Enter the path for the output folder for .txt files: ";
        std::getline(std::cin, outputFolder);

        fs::create_directories(outputFolder);

        for (const auto& entry : fs::directory_iterator(inputFolder)) {
            if (entry.path().extension() == ".mjs") {
                fs::path outputPath = fs::path(outputFolder) / entry.path().filename().replace_extension(".txt");
                dumpText(entry.path(), outputPath);
            }
        }
    }
    else if (mode == "2") {
        std::string inputBinFolder, inputTxtFolder, outputFolder;
        std::cout << "Enter the path of the folder containing original .mjs files: ";
        std::getline(std::cin, inputBinFolder);
        std::cout << "Enter the path of the folder containing translated .txt files: ";
        std::getline(std::cin, inputTxtFolder);
        std::cout << "Enter the path for the output folder for new .mjs files: ";
        std::getline(std::cin, outputFolder);

        fs::create_directories(outputFolder);

        for (const auto& entry : fs::directory_iterator(inputBinFolder)) {
            if (entry.path().extension() == ".mjs") {
                fs::path txtPath = fs::path(inputTxtFolder) / entry.path().filename().replace_extension(".txt");
                fs::path outputPath = fs::path(outputFolder) / entry.path().filename();
                if (fs::exists(txtPath)) {
                    injectText(entry.path(), txtPath, outputPath);
                }
                else {
                    std::cerr << "Warning: No corresponding .txt file found for " << entry.path().filename() << std::endl;
                }
            }
        }
    }
    else {
        std::cerr << "Invalid mode selected." << std::endl;
        return 1;
    }

    return 0;
}