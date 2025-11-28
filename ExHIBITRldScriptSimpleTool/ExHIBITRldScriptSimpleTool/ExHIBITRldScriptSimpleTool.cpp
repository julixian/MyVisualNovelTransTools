#include <Windows.h>
#include <cstdint>

import std;
namespace fs = std::filesystem;
namespace stdv = std::ranges::views;

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

auto splitStringFunc = [](auto&& str, auto&& delimiter) -> decltype(auto)
    {
        std::vector<std::remove_cvref_t<decltype(str)>> result;
        for (auto&& subStrView : str | std::views::split(delimiter)) {
            result.emplace_back(subStrView.begin(), subStrView.end());
        }
        return result;
    };
std::vector<std::string> splitString(const std::string& str, char delimiter) { return splitStringFunc(str, delimiter); }
std::vector<std::string> splitString(const std::string& str, std::string_view delimiter) { return splitStringFunc(str, delimiter); }

struct OpCode {
    uint32_t op;
    uint32_t initCount;
    uint32_t strCount;
    uint32_t unk;
};

struct Sentence {
    uint32_t offset;
    std::string text;
};

OpCode readOpCode(const std::vector<uint8_t>& buffer, uint32_t op) {
    OpCode result;
    result.op = op & 0xFFFF;
    result.initCount = (op & 0xFF0000) >> 16;
    result.strCount = (op >> 24) & 0xF;
    result.unk = op >> 28;
    return result;
}

std::map<uint32_t, std::string> getNameTable(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath);

    if (!input || !output) {
        throw std::runtime_error("Error opening files: " + wide2Ascii(inputPath) + " or " + wide2Ascii(outputPath));
    }

    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});

    if (read<uint32_t>(&buffer[0])!= 0x524C4400) { // \0DLR
        throw std::runtime_error("Invalid file format: " + wide2Ascii(inputPath));
    }

    std::map<uint32_t, std::string> nameTable;

    uint32_t opOffset = read<uint32_t>(&buffer[0x8]);
    uint32_t opCount = read<uint32_t>(&buffer[0xC]);

    uint32_t currentOffset = opOffset + 4;
    for (uint32_t i = 0; i < opCount; i++) {
        uint32_t currentOpOffset = currentOffset;
        OpCode op = readOpCode(buffer, read<uint32_t>(&buffer[currentOffset]));
        currentOffset += 4;

        std::vector<uint32_t> allInit;
        std::vector<Sentence> allStr;

        for (uint32_t j = 0; j < op.initCount; j++) {
            allInit.push_back(read<uint32_t>(&buffer[currentOffset]));
            currentOffset += 4;
        }
        for (uint32_t j = 0; j < op.strCount; j++) {
            std::string str((char*)&buffer[currentOffset]);
            Sentence sentence;
            sentence.offset = currentOffset;
            sentence.text = std::move(str);
            currentOffset += (uint32_t)sentence.text.length() + 1;
            allStr.push_back(std::move(sentence));
        }

        switch (op.op)
        {
        case 0x30:
        {
            output << std::format("{:08X}:::::{}\n", allStr[0].offset, allStr[0].text);
            std::vector<std::string> splitted = splitString(allStr[0].text, ',');
            if (splitted.size() < 4) {
                throw std::runtime_error(std::format("Invalid name table entry: {}", ascii2Ascii(allStr[0].text, 932)));
            }
            nameTable[std::stoul(splitted[0])] = splitted[3];
        }
        case 0x31:
        break;

        default:
            throw std::runtime_error(std::format("Unsupported op code In getNameTable: {:#x} at {:#x}.", op.op, currentOpOffset));
        }
    }

    input.close();
    output.close();

    return nameTable;
}

//DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
void dumpText(const fs::path& inputPath, const fs::path& outputPath, const std::map<uint32_t, std::string>& nameTable) {
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath);

    if (!input || !output) {
        throw std::runtime_error("Error opening files: " + wide2Ascii(inputPath) + " or " + wide2Ascii(outputPath));
    }

    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});

    if (read<uint32_t>(&buffer[0])!= 0x524C4400) { // \0DLR
        throw std::runtime_error("Invalid file format: " + wide2Ascii(inputPath));
    }

    uint32_t opOffset = read<uint32_t>(&buffer[0x8]);
    uint32_t opCount = read<uint32_t>(&buffer[0xC]);

    uint32_t currentOffset = opOffset + 4;
    for (uint32_t i = 0; i < opCount; i++) {
        uint32_t currentOpOffset = currentOffset;
        OpCode op = readOpCode(buffer, read<uint32_t>(&buffer[currentOffset]));
        currentOffset += 4;

        std::vector<uint32_t> allInit;
        std::vector<Sentence> allStr;

        for (uint32_t j = 0; j < op.initCount; j++) {
            allInit.push_back(read<uint32_t>(&buffer[currentOffset]));
            currentOffset += 4;
        }
        for (uint32_t j = 0; j < op.strCount; j++) {
            std::string str((char*)&buffer[currentOffset]);
            Sentence sentence;
            sentence.offset = currentOffset;
            sentence.text = std::move(replaceStrInplace(str, "\n", "[n]"));
            currentOffset += (uint32_t)sentence.text.length() + 1;
            allStr.push_back(std::move(sentence));
        }

        switch (op.op)
        {

        case 0x15:
        {
            for (const auto& sentence : allStr) {
                output << std::format("{:08X}:::::{}\n", sentence.offset, sentence.text);
            }
        }
        break;

        case 0x1c:
        {
            if (!allInit.empty()) {
                if (auto it = nameTable.find(allInit[0]); it != nameTable.end()) {
                    output << std::format("{}\n", it->second);
                }
            }
            for (const auto& sentence : allStr) {
                output << std::format("{:08X}:::::{}\n", sentence.offset, sentence.text);
            }
        }
        break;

        case 0x30:
        case 0xbf:
        {
            if (!allStr.empty()) {
                output << std::format("{:08X}:::::{}\n", allStr[0].offset, allStr[0].text);
            }
        }
        break;

        default:
            break;
            //throw std::runtime_error(std::format("Unsupported op code: {:#x} at {:#x}.", op.op, currentOpOffset));
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
    std::vector<uint8_t> newBuffer;
    newBuffer.reserve(buffer.size());

    std::vector<Sentence> sentences;

    // 读取翻译文本
    std::string line;
    while (std::getline(inputTxt, line)) {
        if (size_t pos = line.find(":::::"); pos != std::string::npos) {
            Sentence sentence;
            sentence.offset = std::stoul(line.substr(0, pos), nullptr, 16);
            sentence.text = line.substr(pos + 5);
            replaceStrInplace(sentence.text, "[n]", "\n");
            sentences.push_back(std::move(sentence));
        }
    }

    uint32_t currentOffset = 0;
    for (const auto& sentence : sentences) {
        newBuffer.insert(newBuffer.end(), buffer.begin() + currentOffset, buffer.begin() + sentence.offset);
        std::vector<uint8_t> newTextBytes = string2Bytes(sentence.text);
        newBuffer.insert(newBuffer.end(), newTextBytes.begin(), newTextBytes.end());
        newBuffer.push_back(0);
        currentOffset = sentence.offset + (uint32_t)std::string((char*)&buffer[sentence.offset]).length() + 1;
    }
    if (currentOffset < buffer.size()) {
        newBuffer.insert(newBuffer.end(), buffer.begin() + currentOffset, buffer.end());
    }

    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::println("Injection complete. Output saved to {}", wide2Ascii(outputBinPath));
}

void printUsage(const fs::path& programPath) {
    std::print("Made by julixian 2025.11.26\n"
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
            std::map<uint32_t, std::string> nameTable = getNameTable(inputFolder / L"defChara.bin", outputFolder / L"defChara.txt");
            for (const auto& entry : fs::recursive_directory_iterator(inputFolder)) {
                if (entry.is_regular_file() && entry.path().extension() == L".bin" && entry.path().filename() != L"defChara.bin") {
                    const fs::path inputPath = entry.path();
                    const fs::path outputPath = (outputFolder / fs::relative(inputPath, inputFolder)).replace_extension(L".txt");
                    if (!fs::exists(outputPath.parent_path())) {
                        fs::create_directories(outputPath.parent_path());
                    }
                    dumpText(inputPath, outputPath, nameTable);
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
                if (entry.is_regular_file() && entry.path().extension() == L".bin") {
                    const fs::path inputBinPath = entry.path();
                    const fs::path inputTxtPath = (inputTxtFolder / fs::relative(inputBinPath, inputBinFolder)).replace_extension(L".txt");
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
        std::println("Error: {}", e.what());
        return 1;
    }

    return 0;
}
