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

struct Sentence {
    uint32_t offsetAddr;
    std::string str;
};

//DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
void dumpText(const fs::path& inputPath, const fs::path& outputPath, bool swapName) {
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath);

    if (!input || !output) {
        throw std::runtime_error("Error opening files: " + wide2Ascii(inputPath) + " or " + wide2Ascii(outputPath));
    }

    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});
    std::string signature((char*)buffer.data(), 0x1b);
    uint32_t headerSize = signature == "BurikoCompiledScriptVer1.00" ? (0x1c + read<uint32_t>(&buffer[0x1c])) : 0;
    std::vector<Sentence> sentences;

    for (uint32_t i = headerSize; i < buffer.size();) {
        uint32_t op = read<uint32_t>(&buffer[i]);
        i += 4;
        if (op == 0xf4) {
            break;
        }
        switch (op)
        {
        case 0x00:
        case 0x01:
        case 0x02:
            i += 4;
            break;
        case 0x03:
        {
            Sentence se;
            se.offsetAddr = i;
            uint32_t offset = read<uint32_t>(&buffer[i]);
            i += 4;
            se.str = (char*)&buffer[headerSize + offset];
            replaceStrInplace(se.str, "\r", "[r]");
            replaceStrInplace(se.str, "\n", "[n]");
            sentences.push_back(std::move(se));
        }
        break;
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0a:
        case 0x0b:
        case 0x0c:
        case 0x0d:
        case 0x0e:
        case 0x0f:
            i += 4;
            break;
        case 0x10:
        case 0x11:
            break;
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            i += 4;
            break;
        case 0x18:
            break;
        case 0x19:
            i += 4;
            break;
        case 0x1a:
        case 0x1b:
        case 0x1c:
        case 0x1d:
        case 0x1e:
        case 0x1f:
        case 0x20:
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2a:
        case 0x2b:
        case 0x2c:
        case 0x2d:
        case 0x2e:
        case 0x2f:
        case 0x30:
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x36:
        case 0x37:
        case 0x38:
        case 0x39:
        case 0x3a:
            break;
        case 0x3f:
            i += 4;
            break;
        case 0x7b:
            i += 12;
            break;
        case 0x7e:
            i += 4;
            break;
        case 0x7f:
            i += 8;
            break;
        default:
            break;
        }
    }

    if (swapName && sentences.size() > 1) {
        std::string open932 = wide2Ascii(L"「", 932);
        std::string close932 = wide2Ascii(L"」", 932);
        for (size_t i = 0; i + 1 < sentences.size(); i++) {
            if (
                (sentences[i].str.starts_with(open932) && sentences[i].str.ends_with(close932)) ||
                (sentences[i].str.starts_with("「") && sentences[i].str.ends_with("」"))
                ) {
                std::swap(sentences[i], sentences[i + 1]);
                i++;
            }
        }
    }

    for (auto& se : sentences) {
        output << std::format("{:08X}:::::{}\n", se.offsetAddr, se.str);
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
    std::string signature((char*)buffer.data(), 0x1b);
    uint32_t headerSize = signature == "BurikoCompiledScriptVer1.00" ? (0x1c + read<uint32_t>(&buffer[0x1c])) : 0;
    
    std::vector<Sentence> sentences;

    // 读取翻译文本
    std::string line;
    while (std::getline(inputTxt, line)) {
        size_t pos = line.find(":::::");
        if (pos == std::string::npos) {
            throw std::runtime_error(std::format("Invalid translation format at line {}", sentences.size() + 1));
        }
        Sentence se;
        std::string offsetAddrStr = line.substr(0, pos);
        std::string str = line.substr(pos + 5);
        se.offsetAddr = (uint32_t)std::stoul(offsetAddrStr, nullptr, 16);
        replaceStrInplace(str, "[r]", "\r");
        replaceStrInplace(str, "[n]", "\n");
        se.str = std::move(str);
        sentences.push_back(std::move(se));
    }

    for (const auto& se : sentences) {
        uint32_t newOffset = (uint32_t)newBuffer.size() - headerSize;
        write<uint32_t>(&newBuffer[se.offsetAddr], newOffset);
        std::vector<uint8_t> newStrBytes = string2Bytes(se.str);
        newStrBytes.push_back(0);
        newBuffer.insert(newBuffer.end(), newStrBytes.begin(), newStrBytes.end());
    }
    
    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::println("Injection complete. Output saved to {}", wide2Ascii(outputBinPath));
}

void printUsage(const fs::path& programPath) {
    std::print("Made by julixian 2025.11.16\n"
        "Usage: \n"
        "  Dump: {0} dump <input_folder> <output_folder> [--swap-name]\n"
        "  Inject: {0} inject <input_orig-bin_folder> <input_translated-txt_folder> <output_folder>\n",
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
            bool swapName = argc >= 5 && std::wstring(argv[4]) == L"--swap-name";
            for (const auto& entry : fs::recursive_directory_iterator(inputFolder)) {
                if (entry.is_regular_file()) {
                    const fs::path inputPath = entry.path();
                    const fs::path outputPath = outputFolder / fs::relative(inputPath, inputFolder);
                    if (!fs::exists(outputPath.parent_path())) {
                        fs::create_directories(outputPath.parent_path());
                    }
                    dumpText(inputPath, outputPath, swapName);
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
                        std::println("Warning: {} not found, skipped.", wide2Ascii(inputTxtPath));
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
        std::print("Error: {}", e.what());
        return 1;
    }

    return 0;
}