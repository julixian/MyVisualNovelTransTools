#include <Windows.h>
#include <cstdint>

import std;
namespace fs = std::filesystem;

template<typename T>
T read(void* ptr)
{
    T value;
    memcpy(&value, ptr, sizeof(T));
    return value;
}

template<typename T>
void write(void* ptr, T value)
{
    memcpy(ptr, &value, sizeof(T));
}

std::vector<uint8_t> string2Bytes(const std::string& str) {
    std::vector<uint8_t> result;
    result.reserve(str.size());
    for (char c : str) {
        result.push_back(static_cast<uint8_t>(c));
    }
    return result;
}

std::string wide2Ascii(const std::wstring& wide, UINT CodePage = CP_UTF8);
std::wstring ascii2Wide(const std::string& ascii, UINT CodePage = CP_ACP);
std::string ascii2Ascii(const std::string& ascii, UINT src = CP_ACP, UINT dst = CP_UTF8);

std::string wide2Ascii(const std::wstring& wide, UINT CodePage) {
    int len = WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return {};
    std::string ascii(len, '\0');
    WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
    ascii.pop_back();
    return ascii;
}

std::wstring ascii2Wide(const std::string& ascii, UINT CodePage) {
    int len = MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, nullptr, 0);
    if (len == 0) return {};
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, &wide[0], len);
    wide.pop_back();
    return wide;
}

std::string ascii2Ascii(const std::string& ascii, UINT src, UINT dst) {
    return wide2Ascii(ascii2Wide(ascii, src), dst);
}

std::string& replaceStrInplace(std::string& str, std::string_view org, std::string_view rep) {
    str = str | std::views::split(org) | std::views::join_with(rep) | std::ranges::to<std::string>();
    return str;
}

std::string replaceStr(std::string_view str, std::string_view org, std::string_view rep) {
    std::string result = str | std::views::split(org) | std::views::join_with(rep) | std::ranges::to<std::string>();
    return result;
}

bool isValidSjis(const std::string& str, bool enableCP932 = true) {
    std::vector<uint8_t> textBytes = string2Bytes(str);
    uint8_t leadByteLimit = enableCP932 ? 0xfc : 0xef;
    for (size_t i = 0; i < textBytes.size(); i++) {
        if (textBytes[i] < 0x20 || textBytes[i] > leadByteLimit || (0x9f < textBytes[i] && textBytes[i] < 0xe0)) {
            return false;
        }
        else if ((0x81 <= textBytes[i] && textBytes[i] <= 0x9f) || (0xe0 <= textBytes[i] && textBytes[i] <= leadByteLimit)) {
            if (i + 1 >= textBytes.size()) {
                return false;
            }
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

void printUsage(const fs::path& programPath) {
    std::print("Made by julixian 2025.11.12\n"
        "Usage: \n"
    "  Dump: {0} dump <input_folder> <output_folder>\n"
    "  Inject: {0} inject <input_orig-bin_folder> <input_translated-txt_folder> <output_folder>",
        wide2Ascii(programPath.filename()));
}

//DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
void dumpText(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath);

    if (!input || !output) {
        throw std::runtime_error("Error opening files: " + wide2Ascii(inputPath) + " or " + wide2Ascii(outputPath));
    }

    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});

    if (std::string(buffer.begin(), buffer.begin() + 4) != "MBT0") {
        throw std::runtime_error("Invalid file format: " + wide2Ascii(inputPath));
    }

    uint32_t sentenceCount = read<uint32_t>(&buffer[0x8]);
    for (uint32_t i = 0; i < sentenceCount - 3; i++) {
        uint32_t indexOfOffset = 0x14 + i * 4;
        uint32_t offset = read<uint32_t>(&buffer[indexOfOffset]);
        std::string sentence((char*)&buffer[offset]);
        replaceStrInplace(sentence, "\r", "[r]");
        replaceStrInplace(sentence, "\n", "[n]");
        output << sentence << "\n";
    }

    input.close();
    output.close();

    std::println("Extraction complete. Output saved to {}", wide2Ascii(outputPath));
}

//IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII
void injectText(const fs::path& inputBinPath, const fs::path& inputTxtPath, const fs::path& outputBinPath) {
    std::ifstream inputBin(inputBinPath, std::ios::binary);
    std::ifstream inputTxt(inputTxtPath);
    std::ofstream outputBin(outputBinPath, std::ios::binary);

    if (!inputBin || !inputTxt || !outputBin) {
        throw std::runtime_error("Error opening files: " + wide2Ascii(inputBinPath) + " or " + wide2Ascii(inputTxtPath) + " or " + wide2Ascii(outputBinPath));
    }
    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(inputBin), {});
    std::vector<uint8_t> newBuffer = buffer;

    std::vector<std::string> sentences;
    uint32_t translationIndex = 0;

    // 读取翻译文本
    std::string line;
    while (std::getline(inputTxt, line)) {
        replaceStrInplace(line, "[r]", "\r");
        replaceStrInplace(line, "[n]", "\n");
        sentences.push_back(std::move(line));
    }

    uint32_t sentenceCount = read<uint32_t>(&buffer[0x8]);

    for (uint32_t i = 0; i < sentenceCount - 3; i++) {
        if (translationIndex >= sentences.size()) {
            throw std::runtime_error(std::format("Not enough translations provided, expected {0}, got {1}.", sentenceCount - 1, translationIndex));
        }
        uint32_t indexOfOffset = 0x14 + i * 4;
        write(&newBuffer[indexOfOffset], static_cast<uint32_t>(newBuffer.size()));
        const std::string& sentence = sentences[translationIndex++];
        std::vector<uint8_t> sentenceBytes = string2Bytes(sentence);
        sentenceBytes.push_back(0);
        newBuffer.insert(newBuffer.end(), sentenceBytes.begin(), sentenceBytes.end());
        while (newBuffer.size() % 4 != 0) {
            newBuffer.push_back(0);
        }
    }

    if (translationIndex < sentenceCount - 3) {
        std::println("Warning: {0} translations provided, expected {1}.", translationIndex, sentenceCount - 1);
    }
    
    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::println("Injection complete. Output saved to {}", wide2Ascii(outputBinPath));
}

int wmain(int argc, wchar_t* argv[])
{
    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    try {
        std::wstring mode = argv[1];
        if (mode == L"dump") {
            if (argc < 4) {
                printUsage(argv[0]);
                return 1;
            }
            const fs::path inputFolder = argv[2];
            const fs::path outputFolder = argv[3];
            fs::create_directories(outputFolder);
            for (const auto& entry : fs::recursive_directory_iterator(inputFolder)) {
                if (entry.is_regular_file()) {
                    const fs::path inputPath = entry.path();
                    const fs::path outputPath = outputFolder / fs::relative(inputPath, inputFolder);
                    dumpText(inputPath, outputPath);
                }
            }
        }
        else if (mode == L"inject") {
            if (argc < 5) {
                printUsage(argv[0]);
                return 1;
            }
            const fs::path inputBinFolder = argv[2];
            const fs::path inputTxtFolder = argv[3];
            const fs::path outputFolder = argv[4];
            fs::create_directories(outputFolder);
            for (const auto& entry : fs::recursive_directory_iterator(inputBinFolder)) {
                if (entry.is_regular_file()) {
                    const fs::path inputBinPath = entry.path();
                    const fs::path inputTxtPath = inputTxtFolder / fs::relative(inputBinPath, inputBinFolder);
                    if (!fs::exists(inputTxtPath)) {
                        std::println("Warning: {0} not found, skipped.", wide2Ascii(inputTxtPath));
                        continue;
                    }
                    const fs::path outputBinPath = outputFolder / fs::relative(inputBinPath, inputBinFolder);
                    if (!fs::exists(outputBinPath.parent_path())) {
                        fs::create_directories(outputBinPath.parent_path());
                    }
                    injectText(inputBinPath, inputTxtPath, outputBinPath);
                }
            }
        }
        else {
            printUsage(argv[0]);
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::print("Error: {0}", e.what());
        return 1;
    }

    return 0;
}
