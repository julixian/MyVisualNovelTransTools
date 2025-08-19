#include <windows.h>
#include <stdint.h>

import std;
namespace fs = std::filesystem;

std::vector<uint8_t> stringToCP932(const std::string& str) {
    std::vector<uint8_t> result;
    for (char c : str) {
        result.push_back(static_cast<uint8_t>(c));
    }
    return result;
}

bool isValidCP932(const std::string& str) {
    std::vector<uint8_t> textBytes = stringToCP932(str);
    for (size_t i = 0; i < textBytes.size(); i++) {
        if ((textBytes[i] < 0x20 && textBytes[i] != 0x0a && textBytes[i] != 0x0d) || (0x9f < textBytes[i] && textBytes[i] < 0xe0)) {
            return false;
        }
        else if ((0x81 <= textBytes[i] && textBytes[i] <= 0x9f) || (0xe0 <= textBytes[i] && textBytes[i] <= 0xfc)) {
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

std::string WideToAscii(const std::wstring& wide, UINT CodePage) {
    int len = WideCharToMultiByte(936, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return "";
    std::string ascii(len, '\0');
    WideCharToMultiByte(936, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
    ascii.pop_back();
    return ascii;
}

std::wstring AsciiToWide(const std::string& ascii, UINT CodePage) {
    int len = MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, nullptr, 0);
    if (len == 0) return L"";
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, &wide[0], len);
    wide.pop_back();
    return wide;
}

void DecryptData(uint8_t* data, size_t length, uint32_t key = 0x4B5AB4A5) {
    for (size_t i = 0; i < length; ++i) {
        data[i] ^= static_cast<uint8_t>(key);
        key = data[i] ^ ((key << 9) | ((key >> 23) & 0x1F0));
    }
}

void EncryptData(uint8_t* data, size_t length, uint32_t key = 0x4B5AB4A5) {
    uint8_t buf = 0;
    for (size_t i = 0; i < length; ++i) {
        buf = data[i];
        data[i] ^= static_cast<uint8_t>(key);
        key = buf ^ ((key << 9) | ((key >> 23) & 0x1F0));
    }
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
    std::regex pattern(R"([\n])");

    for (size_t i = 0; i < buffer.size() - 8; i++) {
        if (*(uint16_t*)&buffer[i] == 0x04) {
            size_t k = i;
            k += 2;
            uint16_t length = *(uint16_t*)&buffer[k];
            if (length < 3)continue;
            k += 2;
            std::vector<uint8_t> encryptedData(&buffer[k], &buffer[k + length]);
            DecryptData(encryptedData.data(), encryptedData.size());
            if (encryptedData.back() != 0x00)continue;
            std::string str((char*)encryptedData.data());
            if (!isValidCP932(str))continue;
            str = std::regex_replace(str, pattern, "/");
            output << str << std::endl;
            i += 4 + length - 1;
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
    std::vector<uint8_t> newBuffer;
    std::vector<std::string> translations;

    // 读取翻译文本
    std::string line;
    std::regex pattern(R"(/)");
    while (std::getline(inputTxt, line)) {
        line = std::regex_replace(line, pattern, "\n");
        translations.push_back(line);
    }

    size_t translationIndex = 0;
    for (size_t i = 0; i < buffer.size() - 8; i++) {
        if (*(uint16_t*)&buffer[i] == 0x04) {
            size_t k = i;
            k += 2;
            uint16_t length = *(uint16_t*)&buffer[k];
            if (length < 3) {
                newBuffer.push_back(buffer[i]);
                continue;
            }
            k += 2;
            std::vector<uint8_t> encryptedData(&buffer[k], &buffer[k + length]);
            DecryptData(encryptedData.data(), encryptedData.size());
            if (encryptedData.back() != 0x00) {
                newBuffer.push_back(buffer[i]);
                continue;
            }
            std::string str((char*)encryptedData.data());
            if (!isValidCP932(str)) {
                newBuffer.push_back(buffer[i]);
                continue;
            }
            if (translationIndex >= translations.size()) {
                std::cout << "not have enough translations!" << std::endl;
                return;
            }
            std::string text = translations[translationIndex++];
            //std::cout << WideToAscii(AsciiToWide(text, 932), CP_ACP) << std::endl;
            std::vector<uint8_t> textBytes = stringToCP932(text);
            textBytes.push_back(0x00);
            EncryptData(textBytes.data(), textBytes.size());
            uint16_t newLength = static_cast<uint16_t>(textBytes.size());
            newBuffer.push_back(0x04);
            newBuffer.push_back(0x00);
            newBuffer.push_back(newLength & 0xFF);
            newBuffer.push_back((newLength >> 8) & 0xFF);
            newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
            i += 4 + length - 1;
        }
        else {
            newBuffer.push_back(buffer[i]);
        }
    }

    newBuffer.insert(newBuffer.end(), buffer.end() - 8, buffer.end());

    if (translationIndex != translations.size()) {
        std::cout << "Warning: translations too much" << std::endl;
    }

    // 写入新文件
    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::cout << "Write-back complete. Output saved to " << outputBinPath << std::endl;
}

void printUsage() {
    std::cout << "Made by julixian 2025.08.19" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  Dump:   ./program dump <script.dat> <output_txt>" << std::endl;
    std::cout << "  Inject: ./program inject <org_script.dat> <new_txt> <script_new.dat>" << std::endl;
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
        dumpText(argv[2], argv[3]);
    }
    else if (mode == "inject") {
        if (argc != 5) {
            std::cerr << "Error: Incorrect number of arguments for inject mode." << std::endl;
            printUsage();
            return 1;
        }
        injectText(argv[2], argv[3], argv[4]);
    }
    else {
        std::cerr << "Error: Invalid mode selected." << std::endl;
        printUsage();
        return 1;
    }

    return 0;
}