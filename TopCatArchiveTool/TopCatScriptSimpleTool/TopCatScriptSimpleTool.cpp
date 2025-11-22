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

//DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
void dumpText(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath);

    if (!input || !output) {
        throw std::runtime_error("Error opening files: " + wide2Ascii(inputPath) + " or " + wide2Ascii(outputPath));
    }

    auto fileSize = fs::file_size(inputPath);

    while (input.tellg() < fileSize) {
        uint32_t commandHeader;
        input.read(reinterpret_cast<char*>(&commandHeader), 4);
        uint16_t commandLength;
        input.read(reinterpret_cast<char*>(&commandLength), 2);
        input.seekg(-6, std::ios::cur);
        std::vector<uint8_t> commandBytes(commandLength);
        input.read(reinterpret_cast<char*>(commandBytes.data()), commandLength);

        switch (commandHeader)
        {
        case 0x00010004:
        case 0x0000003A:
        {
            std::string text((char*)&commandBytes[9]);
            output << text << "\n";
        }
        break;

        case 0x00000000:
        {
            uint32_t currentCommandOffset = 0x8;
            while (currentCommandOffset < commandLength) {
                uint8_t op = read<uint8_t>(&commandBytes[currentCommandOffset]);
                currentCommandOffset += 1;
                switch (op)
                {
                case 0x64:
                case 0x66:
                case 0x68:
                {
                    uint16_t textLength = read<uint16_t>(&commandBytes[currentCommandOffset]);
                    currentCommandOffset += 2;
                    if (commandBytes[currentCommandOffset] != 0x00) {
                        std::string text((char*)&commandBytes[currentCommandOffset], textLength);
                        output << text << "\n";
                    }
                    currentCommandOffset += textLength;
                }
                break;

                case 0x0F:
                {
                    uint16_t subCommandLength = read<uint16_t>(&commandBytes[currentCommandOffset]);
                    uint32_t nextCommandOffset = currentCommandOffset + subCommandLength - 1;
                    currentCommandOffset += 2;
                    currentCommandOffset += 1;
                    std::string baseText((char*)&commandBytes[currentCommandOffset]);
                    currentCommandOffset += baseText.length() + 1;
                    currentCommandOffset += 1;
                    std::string furigana((char*)&commandBytes[currentCommandOffset]);
                    currentCommandOffset += furigana.length() + 1;
                    if (currentCommandOffset != nextCommandOffset) {
                        throw std::runtime_error("Error: unexpected command format.");
                    }
                    output << "[" << furigana << "/" << baseText << "]\n";
                }
                break;

                case 0x10:
                case 0x11:
                case 0x12:
                {
                    uint16_t subCommandLength = read<uint16_t>(&commandBytes[currentCommandOffset]);
                    uint32_t nextCommandOffset = currentCommandOffset + subCommandLength - 1;
                    currentCommandOffset += 2;
                    currentCommandOffset += 1;
                    std::string text((char*)&commandBytes[currentCommandOffset]);
                    currentCommandOffset += text.length() + 1;
                    if (currentCommandOffset != nextCommandOffset) {
                        throw std::runtime_error("Error: unexpected command format.");
                    }
                    output << text << "\n";
                }
                break;

                case 0x01:
                case 0x02:
                {
                    currentCommandOffset += 2;
                }
                break;

                default:
                    throw std::runtime_error(std::format("Error: unknown command op code 0x{:02X} at offset 0x{:X}, command offset 0x{:X}.", op, currentCommandOffset - 1, (size_t)input.tellg() - commandLength));
                }
            }
        }
        break;

        case 0x0003003C:
        {
            uint32_t currentCommandOffset = 0x8;
            currentCommandOffset += 1;
            std::string text1((char*)&commandBytes[currentCommandOffset]);
            currentCommandOffset += text1.length() + 1;
            currentCommandOffset += 1;
            std::string text2((char*)&commandBytes[currentCommandOffset]);
            currentCommandOffset += text2.length() + 1;
            output << text1 << "\n" << text2 << "\n";
        }
        break;

        default:
            break;
        }
    }

    input.close();
    output.close();

    std::println("Extraction complete. Output saved to {}", wide2Ascii(outputPath));
}

//IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII
void injectText(const fs::path& inputBinPath, const fs::path& inputTxtPath, const fs::path& outputBinPath, std::vector<uint32_t>& tctAbsOffset) {
    std::ifstream inputBin(inputBinPath, std::ios::binary);
    std::ifstream inputTxt(inputTxtPath);
    std::ofstream outputBin(outputBinPath, std::ios::binary);

    if (!inputBin || !inputTxt || !outputBin) {
        throw std::runtime_error("Error opening files: " + wide2Ascii(inputBinPath) + " or " + wide2Ascii(inputTxtPath) + " or " + wide2Ascii(outputBinPath));
    }
    auto fileSize = fs::file_size(inputBinPath);
    std::vector<uint8_t> newBuffer;
    newBuffer.reserve(fileSize);

    std::vector<std::string> sentences;
    uint32_t translationIndex = 0;

    // 读取翻译文本
    std::string line;
    while (std::getline(inputTxt, line)) {
        sentences.push_back(std::move(line));
    }

    std::map<uint32_t, uint32_t> commandHeaderOffsetMap;

    while (inputBin.tellg() < fileSize) {
        commandHeaderOffsetMap[inputBin.tellg()] = newBuffer.size();
        uint32_t commandHeader;
        inputBin.read(reinterpret_cast<char*>(&commandHeader), 4);
        uint16_t commandLength;
        inputBin.read(reinterpret_cast<char*>(&commandLength), 2);
        inputBin.seekg(-6, std::ios::cur);
        std::vector<uint8_t> commandBytes(commandLength);
        inputBin.read(reinterpret_cast<char*>(commandBytes.data()), commandLength);
        std::vector<uint8_t> newCommandBytes = commandBytes;

        switch (commandHeader)
        {
        case 0x00010004:
        case 0x0000003A:
        {
            std::string text((char*)&commandBytes[9]);
            if (translationIndex >= sentences.size()) {
                throw std::runtime_error("Error: not enough translations provided.");
            }
            std::string newText = sentences[translationIndex++];
            std::vector<uint8_t> newTextBytes = string2Bytes(newText);
            auto it = newCommandBytes.begin() + 9;
            it = newCommandBytes.erase(it, it + text.length());
            newCommandBytes.insert(it, newTextBytes.begin(), newTextBytes.end());
            write<uint16_t>(&newCommandBytes[4], newCommandBytes.size());
        }
        break;

        case 0x00000000:
        {
            std::vector<std::function<void()>> commandHandlers;
            uint32_t currentCommandOffset = 0x8;
            while (currentCommandOffset < commandLength) {
                uint8_t op = read<uint8_t>(&commandBytes[currentCommandOffset]);
                currentCommandOffset += 1;
                switch (op)
                {
                case 0x64:
                case 0x66:
                case 0x68:
                {
                    uint16_t textLength = read<uint16_t>(&commandBytes[currentCommandOffset]);
                    currentCommandOffset += 2;
                    if (commandBytes[currentCommandOffset] != 0x00) {
                        if (translationIndex >= sentences.size()) {
                            throw std::runtime_error("Error: not enough translations provided.");
                        }
                        std::string newText = sentences[translationIndex++];
                        std::vector<uint8_t> newTextBytes = string2Bytes(newText);
                        if (newTextBytes.empty()) {
                            std::function<void()> handler = [&, currentCommandOffset, textLength]()
                                {
                                    auto it = newCommandBytes.begin() + currentCommandOffset - 3;
                                    newCommandBytes.erase(it, it + textLength + 3);
                                };
                            commandHandlers.push_back(handler);
                        }
                        else {
                            std::function<void()> handler = [&, currentCommandOffset, textLength, newTextBytes]()
                                {
                                    auto it = newCommandBytes.begin() + currentCommandOffset;
                                    it = newCommandBytes.erase(it, it + textLength);
                                    newCommandBytes.insert(it, newTextBytes.begin(), newTextBytes.end());
                                    write<uint16_t>(&newCommandBytes[currentCommandOffset - 2], newTextBytes.size());
                                };
                            commandHandlers.push_back(handler);
                        }
                    }
                    currentCommandOffset += textLength;
                }
                break;

                case 0x0F:
                {
                    if (translationIndex >= sentences.size()) {
                        throw std::runtime_error("Error: not enough translations provided.");
                    }
                    std::string newText = sentences[translationIndex++];
                    std::vector<uint8_t> newBaseTextBytes;
                    std::vector<uint8_t> newFuriganaBytes;
                    if (newText.starts_with('[') && newText.ends_with(']') && newText.contains('/')) {
                        size_t pos = newText.find('/');
                        newBaseTextBytes = string2Bytes(newText.substr(pos + 1));
                        newBaseTextBytes.pop_back();
                        newFuriganaBytes = string2Bytes(newText.substr(1, pos - 1));
                    }
                    else {
                        newBaseTextBytes = string2Bytes(newText);
                        newFuriganaBytes.push_back(0x20); newFuriganaBytes.push_back(0x20);
                    }

                    uint32_t subCommandOffset = currentCommandOffset - 1;
                    uint16_t subCommandLength = read<uint16_t>(&commandBytes[currentCommandOffset]);

                    std::vector<uint8_t> newSubCommandBytes{ commandBytes[subCommandOffset], 0x00, 0x00 };
                    uint32_t nextCommandOffset = subCommandOffset + subCommandLength;

                    currentCommandOffset += 2;
                    newSubCommandBytes.push_back(commandBytes[currentCommandOffset]);
                    currentCommandOffset += 1;
                    std::string baseText((char*)&commandBytes[currentCommandOffset]);
                    currentCommandOffset += baseText.length() + 1;
                    newSubCommandBytes.insert(newSubCommandBytes.end(), newBaseTextBytes.begin(), newBaseTextBytes.end());
                    newSubCommandBytes.push_back(0x00);
                    newSubCommandBytes.push_back(commandBytes[currentCommandOffset]);
                    currentCommandOffset += 1;
                    std::string furigana((char*)&commandBytes[currentCommandOffset]);
                    currentCommandOffset += furigana.length() + 1;
                    newSubCommandBytes.insert(newSubCommandBytes.end(), newFuriganaBytes.begin(), newFuriganaBytes.end());
                    newSubCommandBytes.push_back(0x00);
                    if (currentCommandOffset != nextCommandOffset) {
                        throw std::runtime_error("Error: unexpected command format.");
                    }

                    write<uint16_t>(&newSubCommandBytes[1], newSubCommandBytes.size());

                    if (newBaseTextBytes.empty()) {
                        std::function<void()> handler = [&, subCommandOffset, subCommandLength]()
                            {
                                auto it = newCommandBytes.begin() + subCommandOffset;
                                newCommandBytes.erase(it, it + subCommandLength);
                            };
                        commandHandlers.push_back(handler);
                    }
                    else {
                        std::function<void()> handler = [&, subCommandOffset, subCommandLength, newSubCommandBytes]()
                            {
                                auto it = newCommandBytes.begin() + subCommandOffset;
                                it = newCommandBytes.erase(it, it + subCommandLength);
                                newCommandBytes.insert(it, newSubCommandBytes.begin(), newSubCommandBytes.end());
                            };
                        commandHandlers.push_back(handler);
                    }
                }
                break;

                case 0x10:
                case 0x11:
                case 0x12:
                {
                    if (translationIndex >= sentences.size()) {
                        throw std::runtime_error("Error: not enough translations provided.");
                    }
                    std::string newText = sentences[translationIndex++];
                    std::vector<uint8_t> newTextBytes = string2Bytes(newText);

                    uint32_t subCommandOffset = currentCommandOffset - 1;
                    uint16_t subCommandLength = read<uint16_t>(&commandBytes[currentCommandOffset]);
                    uint32_t nextCommandOffset = subCommandOffset + subCommandLength;
                    std::vector<uint8_t> newSubCommandBytes{ commandBytes[subCommandOffset], 0x00, 0x00 };

                    currentCommandOffset += 2;
                    newSubCommandBytes.push_back(commandBytes[currentCommandOffset]);
                    currentCommandOffset += 1;
                    std::string text((char*)&commandBytes[currentCommandOffset]);
                    currentCommandOffset += text.length() + 1;
                    newSubCommandBytes.insert(newSubCommandBytes.end(), text.begin(), text.end());
                    newSubCommandBytes.push_back(0x00);
                    if (currentCommandOffset != nextCommandOffset) {
                        throw std::runtime_error("Error: unexpected command format.");
                    }
                    
                    write<uint16_t>(&newSubCommandBytes[1], newSubCommandBytes.size());

                    if(newTextBytes.empty()){
                        std::function<void()> handler = [&, subCommandOffset, subCommandLength]()
                            {
                                auto it = newCommandBytes.begin() + subCommandOffset;
                                newCommandBytes.erase(it, it + subCommandLength);
                            };
                        commandHandlers.push_back(handler);
                    }
                    else {
                        std::function<void()> handler = [&, subCommandOffset, subCommandLength, newSubCommandBytes]()
                            {
                                auto it = newCommandBytes.begin() + subCommandOffset;
                                it = newCommandBytes.erase(it, it + subCommandLength);
                                newCommandBytes.insert(it, newSubCommandBytes.begin(), newSubCommandBytes.end());
                            };
                        commandHandlers.push_back(handler);
                    }
                }
                break;

                case 0x01:
                case 0x02:
                {
                    currentCommandOffset += 2;
                }
                break;

                default:
                    throw std::runtime_error(std::format("Error: unknown command op code 0x{:02X}.", op));
                }
            }

            for (const auto& handler : commandHandlers | stdv::reverse) {
                handler();
            }

            write<uint16_t>(&newCommandBytes[4], newCommandBytes.size());
        }
        break;

        case 0x0003003C:
        {
            if (translationIndex + 1 >= sentences.size()) {
                throw std::runtime_error("Error: not enough translations provided.");
            }
            std::string newText1 = sentences[translationIndex++];
            std::string newText2 = sentences[translationIndex++];
            std::vector<uint8_t> newText1Bytes = string2Bytes(newText1);
            std::vector<uint8_t> newText2Bytes = string2Bytes(newText2);

            uint32_t currentCommandOffset = 0x8;
            currentCommandOffset += 1;
            uint32_t text1Offset = currentCommandOffset;
            std::string text1((char*)&commandBytes[currentCommandOffset]);
            currentCommandOffset += text1.length() + 1;
            currentCommandOffset += 1;
            uint32_t text2Offset = currentCommandOffset;
            std::string text2((char*)&commandBytes[currentCommandOffset]);
            currentCommandOffset += text2.length() + 1;

            auto it = newCommandBytes.begin() + text2Offset;
            it = newCommandBytes.erase(it, it + text2.length());
            newCommandBytes.insert(it, newText2Bytes.begin(), newText2Bytes.end());
            it = newCommandBytes.begin() + text1Offset;
            it = newCommandBytes.erase(it, it + text1.length());
            newCommandBytes.insert(it, newText1Bytes.begin(), newText1Bytes.end());

            write<uint16_t>(&newCommandBytes[4], newCommandBytes.size());
        }
        break;

        default:
            break;
        }

        newBuffer.insert(newBuffer.end(), newCommandBytes.begin(), newCommandBytes.end());
    }

    inputBin.seekg(0);
    while (inputBin.tellg() < fileSize) {
        uint32_t commandHeader;
        inputBin.read(reinterpret_cast<char*>(&commandHeader), 4);
        uint16_t commandLength;
        inputBin.read(reinterpret_cast<char*>(&commandLength), 2);
        inputBin.seekg(-6, std::ios::cur);
        std::vector<uint8_t> commandBytes(commandLength);
        inputBin.read(reinterpret_cast<char*>(commandBytes.data()), commandLength);

        if (commandHeader == 0x0000000A) {
            uint32_t absOffsetOrder = read<uint32_t>(&commandBytes[0x9]);
            if (absOffsetOrder >= tctAbsOffset.size()) {
                throw std::runtime_error("Error: not enough TCT absolute offsets provided.");
            }
            uint32_t orgAbsOffset = tctAbsOffset[absOffsetOrder];
            auto it = commandHeaderOffsetMap.find(orgAbsOffset);
            if (it == commandHeaderOffsetMap.end()) {
                throw std::runtime_error(std::format("Error: TCT absolute offset 0x{:08X} not found in command header offset map.", orgAbsOffset));
            }
            uint32_t newAbsOffset = it->second;
            tctAbsOffset[absOffsetOrder] = newAbsOffset;
        }
    }

    if (translationIndex < sentences.size()) {
        std::println("Warning: {0} translations provided, expected {1}.", sentences.size(), translationIndex);
    }

    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::println("Injection complete. Output saved to {}", wide2Ascii(outputBinPath));
}

void printUsage(const fs::path& programPath) {
    std::print("Made by julixian 2025.11.22\n"
        "Usage: \n"
        "  Dump: {0} dump <input_folder> <output_folder>\n"
        "  Inject: {0} inject <input_orig-bin_folder> <input_translated-txt_folder> <output_folder> [tct_abs_offset_file]",
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
            std::vector<uint32_t> tctAbsOffset;
            fs::path newTctAbsOffsetFile;
            if (argc >= 6) {
                const fs::path tctAbsOffsetFile = argv[5];
                std::ifstream ifs(tctAbsOffsetFile);
                if (!ifs) {
                    throw std::runtime_error("Error opening TCT absolute offset file: " + wide2Ascii(tctAbsOffsetFile));
                }
                std::string line;
                while (std::getline(ifs, line)) {
                    tctAbsOffset.push_back(std::stoul(line, nullptr, 16));
                }
                ifs.close();
                newTctAbsOffsetFile = tctAbsOffsetFile;
                newTctAbsOffsetFile.replace_filename(newTctAbsOffsetFile.stem().wstring() + L"_new" + newTctAbsOffsetFile.extension().wstring());
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
                    injectText(inputBinPath, inputTxtPath, outputBinPath, tctAbsOffset);
                }
            }
            if (!newTctAbsOffsetFile.empty()) {
                std::ofstream ofs(newTctAbsOffsetFile);
                if (!ofs) {
                    throw std::runtime_error("Error opening new TCT absolute offset file: " + wide2Ascii(newTctAbsOffsetFile));
                }
                for (uint32_t offset : tctAbsOffset) {
                    ofs << std::format("{:08X}\n", offset);
                }
                ofs.close();
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
