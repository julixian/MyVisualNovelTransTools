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

struct LC_FUNC {
    uint32_t func;
    uint32_t param1;
    uint32_t param2;
};

//DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
void dumpText(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath);

    if (!input || !output) {
        throw std::runtime_error("Error opening files: " + wide2Ascii(inputPath) + " or " + wide2Ascii(outputPath));
    }

    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});
    uint32_t funcCount = *(uint32_t*)&buffer[0];
    uint32_t scriptBegin = funcCount * 12 + 8;

    std::regex pattern_0(R"([\x00])");
    std::regex pattern_1(R"([\x01])");
    std::regex pattern_2(R"([\x02])");
    std::regex pattern_3(R"([\x03])");
    std::regex pattern_4(R"([\x04])");
    std::regex pattern_5(R"([\x05])");
    std::regex pattern_6(R"([\x06])");
    std::regex lb1(R"(\r)");
    std::regex lb2(R"(\n)");
    for (size_t i = 0; i < funcCount; i++) {
        LC_FUNC lc_func = *(LC_FUNC*)&buffer[8 + i * 12];
        if (lc_func.func == 0x11 && lc_func.param1 == 0x02) {
            uint32_t lengthOffset = scriptBegin + lc_func.param2;
            uint32_t length = *(uint32_t*)&buffer[lengthOffset];
            uint32_t offset = scriptBegin + lc_func.param2 + 4;
            std::string str((char*)&buffer[offset], length - 1);
            str = std::regex_replace(str, pattern_0, "\\x00");
            str = std::regex_replace(str, pattern_1, "\\x01");
            str = std::regex_replace(str, pattern_2, "\\x02");
            str = std::regex_replace(str, pattern_3, "\\x03");
            str = std::regex_replace(str, pattern_4, "\\x04");
            str = std::regex_replace(str, pattern_5, "\\x05");
            str = std::regex_replace(str, pattern_6, "\\x06");
            str = std::regex_replace(str, lb1, "[r]");
            str = std::regex_replace(str, lb2, "[n]");
            output << str << std::endl;
        }
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
    std::vector<std::string> translations;

    std::string line;
    while (std::getline(inputTxt, line)) {
        translations.push_back(line);
    }

    size_t translationIndex = 0;
    uint32_t funcCount = *(uint32_t*)&buffer[0];
    uint32_t scriptBegin = funcCount * 12 + 8;
    std::vector<uint8_t> newBuffer(scriptBegin);
    memcpy(newBuffer.data(), buffer.data(), scriptBegin);

    std::regex pattern_0(R"(\\x00)");
    std::regex pattern_1(R"(\\x01)");
    std::regex pattern_2(R"(\\x02)");
    std::regex pattern_3(R"(\\x03)");
    std::regex pattern_4(R"(\\x04)");
    std::regex pattern_5(R"(\\x05)");
    std::regex pattern_6(R"(\\x06)");
    std::regex lb1(R"(\[r\])");
    std::regex lb2(R"(\[n\])");
    for (size_t i = 0; i < funcCount; i++) {
        LC_FUNC lc_func = *(LC_FUNC*)&buffer[8 + i * 12];
        if (lc_func.func == 0x11 && lc_func.param1 == 0x02) {
            uint32_t offset = (uint32_t)newBuffer.size() - scriptBegin;
            lc_func.param2 = offset;
            *(LC_FUNC*)&newBuffer[8 + i * 12] = lc_func;
            if (translationIndex >= translations.size()) {
                throw std::runtime_error("Not enough translations provided.");
            }
            std::string str = translations[translationIndex];
            translationIndex++;
            str = std::regex_replace(str, pattern_0, std::string("\x00", 1));
            str = std::regex_replace(str, pattern_1, "\x01");
            str = std::regex_replace(str, pattern_2, "\x02");
            str = std::regex_replace(str, pattern_3, "\x03");
            str = std::regex_replace(str, pattern_4, "\x04");
            str = std::regex_replace(str, pattern_5, "\x05");
            str = std::regex_replace(str, pattern_6, "\x06");
            str = std::regex_replace(str, lb1, "\r");
            str = std::regex_replace(str, lb2, "\n");
            std::vector<uint8_t> textBytes = string2Bytes(str);
            textBytes.push_back(0x00);
            uint32_t length = (uint32_t)textBytes.size();
            newBuffer.insert(newBuffer.end(), (uint8_t*)&length, (uint8_t*)&length + 4);
            newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
        }
    }

    if (translationIndex < translations.size()) {
        std::println("Warning: {0} translations provided, expected {1}.", translations, translationIndex);
    }

    *(uint32_t*)&newBuffer[4] = (uint32_t)newBuffer.size() - scriptBegin;

    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::println("Injection complete. Output saved to {}", wide2Ascii(outputBinPath));
}

void printUsage(const fs::path& programPath) {
    std::print("Made by julixian 2025.11.16\n"
        "Usage: \n"
        "  Dump: {0} dump <input_folder> <output_folder>\n"
        "  Inject: {0} inject <input_orig-bin_folder> <input_translated-txt_folder> <output_folder>",
        wide2Ascii(programPath.filename()));
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
                    if (!fs::exists(outputPath.parent_path())) {
                        fs::create_directories(outputPath.parent_path());
                    }
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
