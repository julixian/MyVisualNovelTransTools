#include <windows.h>

import std;
namespace fs = std::filesystem;

std::string wide2Ascii(const std::wstring& wide, UINT CodePage = CP_UTF8);
std::wstring ascii2Wide(const std::string& ascii, UINT CodePage = CP_ACP);
std::string ascii2Ascii(const std::string& ascii, UINT src = CP_ACP, UINT dst = CP_UTF8);

std::string wide2Ascii(const std::wstring& wide, UINT CodePage) {
    int len = WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return "";
    std::string ascii(len, '\0');
    WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
    ascii.pop_back();
    return ascii;
}

std::wstring ascii2Wide(const std::string& ascii, UINT CodePage) {
    int len = MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, nullptr, 0);
    if (len == 0) return L"";
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, &wide[0], len);
    wide.pop_back();
    return wide;
}

std::string ascii2Ascii(const std::string& ascii, UINT src, UINT dst) {
    return wide2Ascii(ascii2Wide(ascii, src), dst);
}

//DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
void dumpText(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath);
    if (!input) {
        std::cerr << "Error opening input file: " << inputPath << std::endl;
        return;
    }
    if (!output) {
        std::cerr << "Error opening output file: " << outputPath << std::endl;
        return;
    }

    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});
    input.close();

    for (size_t i = 0; i < buffer.size() - 1; i++) {
        if ((buffer[i] == 0x02 && buffer[i + 1] == 0x88) || (buffer[i] == 0x03 && buffer[i + 1] == 0x88)) {
            i += 2;
            uint8_t wlength = buffer[i];
            std::wstring text((wchar_t*)&buffer[i + 1], wlength);
            output << wide2Ascii(text) << std::endl;
            i += wlength * 2;
        }
        else if ((buffer[i] == 0x30 && buffer[i + 1] == 0x88)) {
            i += 2;
            uint8_t wlength = buffer[i];
            i += wlength * 2;
        }
    }

    std::cout << "Extraction complete. Output saved to " << outputPath << std::endl;
}

//IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII
void injectText(const fs::path& inputBinPath, const fs::path& inputTxtPath, const fs::path& outputBinPath) {
    // 读取原始二进制文件
    std::ifstream inputBin(inputBinPath, std::ios::binary);
    if (!inputBin) {
        std::cerr << "Error opening input binary file" << std::endl;
        return;
    }
    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(inputBin), {});
    inputBin.close();

    // 读取文本文件
    std::vector<std::wstring> translations;
    std::ifstream inputTxt(inputTxtPath);
    if (!inputTxt) {
        std::cerr << "Error opening input text file" << std::endl;
        return;
    }
    std::string line;
    while (std::getline(inputTxt, line)) {
        translations.push_back(ascii2Wide(line, 65001));
    }

    size_t translationIndex = 0;
    std::vector<uint8_t> newBuffer;

    for (size_t i = 0; i < buffer.size() - 1; i++) {
        if ((buffer[i] == 0x02 && buffer[i + 1] == 0x88) || (buffer[i] == 0x03 && buffer[i + 1] == 0x88)) {
            i += 2;
            uint8_t orgiwlen = buffer[i];
            if (translationIndex >= translations.size()) {
                std::cout << "Not Enough Translations！" << std::endl;
                continue;
            }
            std::wstring newText = translations[translationIndex++];
            uint8_t newLength = static_cast<uint8_t>(newText.length());

            newBuffer.push_back(buffer[i - 2]);
            newBuffer.push_back(buffer[i - 1]);
            newBuffer.push_back(newLength);

            for (size_t j = 0; j < newText.length(); j++) {
                newBuffer.push_back(newText[j] & 0xFF);
                newBuffer.push_back((newText[j] >> 8) & 0xFF);
            }

            i += orgiwlen * 2;
        }
        else if ((buffer[i] == 0x30 && buffer[i + 1] == 0x88)) {
            i += 2;
            uint8_t orgiwlen = buffer[i];
            newBuffer.push_back(buffer[i - 2]);
            newBuffer.push_back(buffer[i - 1]);
            newBuffer.push_back(buffer[i]);
            for (size_t j = 0; j < orgiwlen * 2; j++) {
                newBuffer.push_back(buffer[i + j + 1]);
            }
            i += orgiwlen * 2;
        }
        else {
            newBuffer.push_back(buffer[i]);
        }
    }

    newBuffer.push_back(buffer.back());
    if (translationIndex != translations.size()) {
        std::cout << "Warning: Too much translations! " << std::endl;
        system("pause");
    }

    std::ofstream outputBin(outputBinPath, std::ios::binary);
    if (!outputBin) {
        std::cerr << "Error opening output binary file" << std::endl;
        return;
    }
    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());
    outputBin.close();

    std::cout << "Injection complete. Output saved to " << outputBinPath << std::endl;
}

void printUsage(fs::path programPath) {
    std::cout << "Made by julixian 2025.01.01" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  Dump:   " << programPath.filename().string() << " dump <input_folder> <output_folder>" << std::endl;
    std::cout << "  Inject: " << programPath.filename().string() << " inject <input_orgi-bin_folder> <input_translated-txt_folder> <output_folder>" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "dump") {
        if (argc != 4) {
            std::cerr << "Error: Incorrect number of arguments for dump mode." << std::endl;
            printUsage(argv[0]);
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
            printUsage(argv[0]);
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
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}