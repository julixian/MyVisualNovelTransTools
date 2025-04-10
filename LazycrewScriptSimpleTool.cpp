#include <iostream>
#include <fstream>
#include <windows.h>
#include <vector>
#include <string>
#include <regex>
#include <filesystem>

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
    //for (char ch : str) {
    //    /*if (*(uint8_t*)&ch < 0x20) {
    //        return false;
    //    }*/
    //    textBytes.push_back(*(uint8_t*)&ch);
    //}
    for (size_t i = 0; i < textBytes.size(); i++) {
        if ((textBytes[i] < 0x20 && textBytes[i] != 0x0a && textBytes[i] != 0x0d) || (0x9f < textBytes[i] && textBytes[i] < 0xe0)) {
            //std::cout << str << " :E1 " << i << std::endl;
            return false;
        }
        else if ((0x81 <= textBytes[i] && textBytes[i] <= 0x9f) || (0xe0 <= textBytes[i] && textBytes[i] <= 0xef)) {
            if (textBytes[i + 1] > 0xfc || textBytes[i + 1] < 0x40) {
                //std::cout << str << " :E2 " << i << std::endl;
                return false;
            }
            else {
                i++;
                continue;
            }
        }
    }
    return true;
    /*int required = MultiByteToWideChar(
        932,
        MB_ERR_INVALID_CHARS,
        str.c_str(),
        str.length(),
        nullptr,
        0
    );

    if (required == 0) {
        return false;
    }

    std::wstring wide(required, L'\0');
    int result = MultiByteToWideChar(
        932,
        MB_ERR_INVALID_CHARS,
        str.c_str(),
        str.length(),
        &wide[0],
        required
    );

    return result != 0;*/
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
            std::regex pattern(R"([\n])");
            str = std::regex_replace(str, pattern, "<br>");
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
    std::vector<uint8_t> newBuffer = buffer;
    std::vector<std::string> translations;

    // 读取翻译文本
    std::string line;
    while (std::getline(inputTxt, line)) {
        translations.push_back(line);
    }

    size_t translationIndex = 0;
    for (size_t i = 0; i < newBuffer.size() - 8; i++) {
        if (*(uint16_t*)&newBuffer[i] == 0x04) {
            size_t k = i;
            k += 2;
            uint16_t length = *(uint16_t*)&newBuffer[k];
            if (length < 3)continue;
            k += 2;
            std::vector<uint8_t> encryptedData(&newBuffer[k], &newBuffer[k + length]);
            DecryptData(encryptedData.data(), encryptedData.size());
            if (encryptedData.back() != 0x00)continue;
            std::string str((char*)encryptedData.data());
            if (!isValidCP932(str))continue;
            if (translationIndex >= translations.size()) {
                std::cout << "not have enough translations!" << std::endl;
                return;
            }
            std::string text = translations[translationIndex];
            //std::cout << WideToAscii(AsciiToWide(text, 932), CP_ACP) << std::endl;
            translationIndex++;
            std::regex pattern(R"(<br>)");
            text = std::regex_replace(text, pattern, "\n");
            std::vector<uint8_t> textBytes = stringToCP932(text);
            textBytes.push_back(0x00);
            EncryptData(textBytes.data(), textBytes.size());
            *(uint16_t*)&newBuffer[i + 2] = textBytes.size();
            newBuffer.erase(newBuffer.begin() + i + 4, newBuffer.begin() + i + 4 + length);
            newBuffer.insert(newBuffer.begin() + i + 4, textBytes.begin(), textBytes.end());
            i += 4 + textBytes.size() - 1;
        }
    }

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
    std::cout << "Made by julixian 2025.04.11" << std::endl;
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