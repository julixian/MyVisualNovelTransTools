#include <Windows.h>
#include <cstdint>

import std;
import nlohmann.json;
namespace fs = std::filesystem;
using json = nlohmann::json;

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

void decompress(const fs::path& inputDatPath, const fs::path& outputJsonPath) {
    std::ifstream ifs(inputDatPath, std::ios::binary);
    std::ofstream ofs(outputJsonPath);
    if (!ifs.is_open() || !ofs.is_open()) {
        throw std::runtime_error("Failed to open file");
    }

    json data = json::array();

    uint64_t fileSize = fs::file_size(inputDatPath);
    uint32_t argCount = 0;
    ifs.seekg(8);
    ifs.read((char*)&argCount, 4);
    for (uint32_t i = 0; i < argCount; i++) {
        uint32_t strLength, unk1, unk2;
        std::string key, value;
        ifs.read((char*)&strLength, 4);
        ifs.read((char*)&unk1, 4);
        ifs.read((char*)&unk2, 4);
        std::string str;
        str.resize(strLength);
        ifs.read((char*)&str[0], strLength);
        str.pop_back();
        size_t pos = str.find_first_of('\0');
        if (pos != std::string::npos) {
            key = ascii2Ascii(str.substr(0, pos), 932);
            value = ascii2Ascii(str.substr(pos + 1), 932);
        }
        json obj = { { "key", key }, { "value", value }, { "unk1", unk1 }, {"unk2", unk2} };
        data.push_back(obj);
    }

    ofs << data.dump(2);
    ofs.close();

    std::println("Dumped {} arguments", argCount);
}

void injectText(const fs::path& inputJsonPath, const fs::path& outputDatPath, const fs::path& scbDirPath, UINT codePage) {
    std::ifstream ifs(inputJsonPath);
    std::ofstream ofs(outputDatPath, std::ios::binary);
    if (!ifs.is_open() || !ofs.is_open()) {
        throw std::runtime_error("Failed to open file");
    }

    json data = json::parse(ifs);
    ifs.close();
    uint32_t argCount = (uint32_t)data.size();
    ofs.write("ARGS    ", 8);
    ofs.write((char*)&argCount, 4);

    for (auto& obj : data) {
        uint32_t unk1 = obj["unk1"].get<uint32_t>();
        uint32_t unk2 = obj["unk2"].get<uint32_t>();
        if (unk2 != 1 || (unk1 != 4 && unk1 != 6 && unk1 != 7 && unk1 != 8 && unk1 != 10)) {
            continue;
        }
        std::string label = obj["key"].get<std::string>();
        std::string scbFileName = obj["value"].get<std::string>();
        uint32_t orgEntryOffset = 0;
        if (size_t pos = scbFileName.find(':'); pos != std::string::npos) {
            orgEntryOffset = std::stoi(scbFileName.substr(pos + 1));
            if (orgEntryOffset < 20) {
                continue;
            }
            scbFileName = scbFileName.substr(0, pos);
        }
        else {
            throw std::runtime_error("Invalid SCB file: " + scbFileName);
        }
        const std::string wholeLabelStr = "label : " + label + ";";
        const fs::path fixedScbFilePath = scbDirPath / fs::path(ascii2Wide(scbFileName, 65001)).filename();
        if (!fs::exists(fixedScbFilePath)) {
            throw std::runtime_error("SCB file does not exist: " + wide2Ascii(fixedScbFilePath));
        }
        ifs.open(fixedScbFilePath, std::ios::binary);
        if (!ifs.is_open()) {
            throw std::runtime_error("Failed to open SCB file: " + wide2Ascii(fixedScbFilePath));
        }
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        replaceStrInplace(content, "__SF0__SIG__", "");
        ifs.close();
        if (size_t pos = content.find(wholeLabelStr); pos != std::string::npos) {
            if (pos != content.rfind(wholeLabelStr)) {
                throw std::runtime_error(std::format("Multiple labels {} in SCB file {}", wholeLabelStr, wide2Ascii(fixedScbFilePath)));
            }
            std::string newScbFileName = scbFileName + ":" + std::to_string(pos + wholeLabelStr.length() - 1);
            obj["value"] = newScbFileName;
            std::println("Found label {} at 0x{:X} in SCB file {}", wholeLabelStr, pos, wide2Ascii(fixedScbFilePath));
        }
        else {
            throw std::runtime_error(std::format("Can not find label {} in SCB file {}", wholeLabelStr, wide2Ascii(fixedScbFilePath)));
        }
    }

    for (const auto& obj : data) {
        std::string key = ascii2Ascii(obj["key"].get<std::string>(), 65001, codePage);
        std::string value = ascii2Ascii(obj["value"].get<std::string>(), 65001, codePage);
        uint32_t strLength = (uint32_t)(key.size() + value.size() + 2);
        uint32_t unk1 = obj["unk1"].get<uint32_t>();
        uint32_t unk2 = obj["unk2"].get<uint32_t>();
        ofs.write((char*)&strLength, 4);
        ofs.write((char*)&unk1, 4);
        ofs.write((char*)&unk2, 4);
        ofs.write(key.c_str(), key.size() + 1);
        ofs.write(value.c_str(), value.size() + 1);
    }
    ofs.close();
    std::println("Injected {} arguments", argCount);
}

void printUsage(const fs::path& programPath) {
    std::print("Made by julixian 2025.12.29\n"
        "Usage: \n"
        "  Dump: {0} dump <system.dat> <output_json>\n"
        "  Inject: {0} inject <input_fixed-json> <output_system.dat> <fixed_scb_dir> [codePage]\n",
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
            const fs::path inputDatPath = argv[2];
            const fs::path outputJsonPath = argv[3];
            fs::create_directories(outputJsonPath.parent_path());
            decompress(inputDatPath, outputJsonPath);
        }
        else if (mode == L"inject") {
            if (argc < 5) {
                printUsage(argv[0]);
                return 1;
            }
            const fs::path inputJsonPath = argv[2];
            const fs::path outputDatPath = argv[3];
            const fs::path scbDirPath = argv[4];
            fs::create_directories(outputDatPath.parent_path());
            UINT codePage = 932;
            if (argc >= 6) {
                codePage = std::stoi(argv[5]);
            }
            injectText(inputJsonPath, outputDatPath, scbDirPath, codePage);
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