#include <iostream>
#include <fstream>
#include <windows.h>
#include <vector>
#include <string>
#include <iomanip>
#include <cstdint>
#include <filesystem>
#include <regex>
#include <format>
#include <algorithm>

namespace fs = std::filesystem;

std::string WideToAscii(const std::wstring& wide, UINT CodePage) {
    int len = WideCharToMultiByte(CodePage, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return "";
    std::string ascii(len, '\0');
    WideCharToMultiByte(CodePage, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
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

std::vector<uint8_t> stringToCP932(const std::string& str) {
    std::vector<uint8_t> result;
    for (char c : str) {
        result.push_back(static_cast<uint8_t>(c));
    }
    return result;
}

bool isValidCP932(const std::string& str) {
    if (str.empty()) return false;
    std::vector<uint8_t> textBytes = stringToCP932(str);
    for (size_t i = 0; i < textBytes.size(); i++) {
        if (textBytes[i] < 0x20 || (0x9f < textBytes[i] && textBytes[i] < 0xe0)) {
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
        std::cerr << "Error opening files: " << inputPath << " or " << outputPath << std::endl;
        return;
    }

    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});

    int lastCount = -1;
    for (size_t i = 0; i < buffer.size() - 4; i++) {
        if (*(uint16_t*)&buffer[i] == 0x4100) {
            if (*(uint16_t*)&buffer[i + 2] == lastCount + 1 && isValidCP932(std::string((char*)&buffer[i + 6]))) {
                lastCount++;
                i += 6;
                std::string str((char*)&buffer[i]);
                output << str << std::endl;
                i += str.length() - 1;
            }
        }
        else if (*(uint16_t*)&buffer[i] == 0x4200) {
            if (*(uint16_t*)&buffer[i + 2] == lastCount + 1 && isValidCP932(std::string((char*)&buffer[i + 7]))) {
                lastCount++;
                i += 7;
                std::string strName((char*)&buffer[i]);
                output << strName << ":::::";
                i += strName.length() + 1;
                std::string str((char*)&buffer[i]);
                output << str << std::endl;
                i += str.length() - 1;
            }
        }
        else if (*(uint16_t*)&buffer[i] == 0x0200) {
            if (*(uint16_t*)&buffer[i + 2] > 0 && *(uint16_t*)&buffer[i + 2] < 10 && *(uint16_t*)&buffer[i + 4] == lastCount + 1 && isValidCP932(std::string((char*)&buffer[i + 6]))) {
				uint16_t selectCount = *(uint16_t*)&buffer[i + 2];
                i += 4;
                for(size_t j = 0; j < selectCount; j++) {
                    while (true) {
                        if (
                            *(uint16_t*)&buffer[i] == lastCount + 1 
                            && isValidCP932(std::string((char*)&buffer[i + 2]))
                            ) {
                            break;
                        }
                        i++;
                    }
                    lastCount++;
                    i += 2;
                    std::string str((char*)&buffer[i]);
                    output << str << std::endl;
                    i += str.length();
				}
            }
        }
    }

    input.close();
    output.close();

    std::cout << "Extraction complete. Output saved to " << outputPath << std::endl;
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
    std::vector<Sentence> Sentences;
    std::vector<size_t> jmps;

    int lastCount = -1;
    for (size_t i = 0; i < buffer.size() - 4; i++) {
        if (*(uint16_t*)&buffer[i] == 0x4100) {
            if (*(uint16_t*)&buffer[i + 2] == lastCount + 1 && isValidCP932(std::string((char*)&buffer[i + 6]))) {
                lastCount++;
                newBuffer.insert(newBuffer.end(), buffer.begin() + i, buffer.begin() + i + 6);
                i += 6;
                std::string str((char*)&buffer[i]);
                if (translationIndex >= translations.size()) {
                    std::cout << "Warning: Not enough translations!" << std::endl;
                    system("pause");
                    newBuffer.push_back(buffer[i]);
                    continue;
                }
                std::string text = translations[translationIndex++];
                std::vector<uint8_t> textBytes = stringToCP932(text);
                Sentence Se;
                Se.addr = i;
                Se.offset = (int)textBytes.size() - str.length();
                Sentences.push_back(Se);
                newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
                i += str.length() - 1;
                continue;
            }
            else {
                newBuffer.push_back(buffer[i]);
                continue;
            }
        }
        else if (*(uint16_t*)&buffer[i] == 0x4200) {
            if (*(uint16_t*)&buffer[i + 2] == lastCount + 1 && isValidCP932(std::string((char*)&buffer[i + 7]))) {
                lastCount++;
                newBuffer.insert(newBuffer.end(), buffer.begin() + i, buffer.begin() + i + 7);
                i += 7;
                Sentence Se;
                Se.addr = i;
                std::string strName((char*)&buffer[i]);
                i += strName.length() + 1;
                std::string str((char*)&buffer[i]);
                if (translationIndex >= translations.size()) {
                    std::cout << "Warning: Not enough translations!" << std::endl;
                    system("pause");
                    newBuffer.push_back(buffer[i]);
                    continue;
                }
                std::string text = translations[translationIndex++];
                size_t pos = text.find(":::::");
                if (pos == std::string::npos) {
                    std::cout << "Warning: Format wrong!" << "\n"
                        << "Please check sentence: " << WideToAscii(AsciiToWide(text, 932), CP_ACP) << std::endl;
                    system("pause");
                    newBuffer.push_back(buffer[i]);
                    continue;
                }
                std::vector<uint8_t> nameBytes = stringToCP932(text.substr(0, pos));
                std::vector<uint8_t> textBytes = stringToCP932(text.substr(pos + 5));
                Se.offset = (int)textBytes.size() - str.length() + nameBytes.size() - strName.length();
                Sentences.push_back(Se);
                newBuffer.insert(newBuffer.end(), nameBytes.begin(), nameBytes.end());
                newBuffer.push_back(0);
                newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
                i += str.length() - 1;
            }
            else {
                newBuffer.push_back(buffer[i]);
                continue;
            }
        }
        else if (*(uint16_t*)&buffer[i] == 0x0200 
            && *(uint16_t*)&buffer[i + 2] > 0 
            && *(uint16_t*)&buffer[i + 2] < 10 
            && *(uint16_t*)&buffer[i + 4] == lastCount + 1
            && isValidCP932(std::string((char*)&buffer[i + 6]))) {
            uint16_t selectCount = *(uint16_t*)&buffer[i + 2];
            newBuffer.insert(newBuffer.end(), buffer.begin() + i, buffer.begin() + i + 4);
            i += 4;
            for (size_t j = 0; j < selectCount; j++) {
                while (true) {
                    if (
                        *(uint16_t*)&buffer[i] == lastCount + 1
                        && isValidCP932(std::string((char*)&buffer[i + 2]))
                        ) {
                        break;
                    }
                    newBuffer.push_back(buffer[i]);
                    i++;
                }
                lastCount++;
				newBuffer.insert(newBuffer.end(), buffer.begin() + i, buffer.begin() + i + 2);
                i += 2;
                std::string str((char*)&buffer[i]);
                if (translationIndex >= translations.size()) {
                    std::cout << "Warning: Not enough translations!" << std::endl;
                    system("pause");
                    newBuffer.push_back(buffer[i]);
                    break;
                }
                std::string text = translations[translationIndex++];
                std::vector<uint8_t> textBytes = stringToCP932(text);
                Sentence Se;
                Se.addr = i;
                Se.offset = (int)textBytes.size() - str.length();
                Sentences.push_back(Se);
                newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
                if (buffer[i + str.length() + 4] == 0x06) {
                    jmps.push_back(newBuffer.size() + 5);
                }
                i += str.length();
            }
            i--;
        }
        else if (*(uint16_t*)&buffer[i] == 0x0600 && *(uint32_t*)&buffer[i + 2] < buffer.size() && buffer[*(uint32_t*)&buffer[i + 2]] != 0x00) {
            jmps.push_back(newBuffer.size() + 2);
            newBuffer.push_back(buffer[i]);
        }
        else {
            newBuffer.push_back(buffer[i]);
        }
    }
	newBuffer.insert(newBuffer.end(), buffer.end() - 4, buffer.end());

    if (translationIndex != translations.size()) {
        std::cout << "Warning: too many translations!" << std::endl;
        system("pause");
    }

    for (size_t i = 0; i < jmps.size(); i++) {
        uint32_t jmp = *(uint32_t*)&newBuffer[jmps[i]];
        int offset = 0;
        for (size_t j = 0; j < Sentences.size() && Sentences[j].addr < jmp; j++) {
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

    std::cout << "Write-back complete. Output saved to " << outputBinPath << std::endl;
}

void printUsage() {
    std::cout << "Made by julixian 2025.07.03" << std::endl;
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