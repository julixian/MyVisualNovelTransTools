#define _RANGES_
#include <Windows.h>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <tinyxml2.h>

import std;
using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;
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

//DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
void dumpText(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath);

    if (!input || !output) {
        throw std::runtime_error("Error opening files: " + wide2Ascii(inputPath) + " or " + wide2Ascii(outputPath));
    }

    ordered_json j = ordered_json::array();
    std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    replaceStrInplace(content, " @l=", " _l=");

    tinyxml2::XMLDocument doc;
    tinyxml2::XMLError error = doc.Parse(content.c_str());
    if (error != tinyxml2::XML_SUCCESS) {
        throw std::runtime_error("XML Parse Error ID: " + std::to_string(error));
    }
    tinyxml2::XMLElement* root = doc.FirstChildElement("xscript");
    if (root) {
        tinyxml2::XMLElement* code = root->FirstChildElement("code");
        if (code) {
            tinyxml2::XMLElement* msg = code->FirstChildElement("msg");
            while (msg) {
                const char* nameAttr = msg->Attribute("name");
                const char* textAttr = msg->Attribute("text");

                if (textAttr && std::strlen(textAttr) > 0) {
                    ordered_json entry;
                    if (nameAttr && std::strlen(nameAttr) > 0) {
                        entry["name"] = nameAttr;
                    }
                    entry["message"] = textAttr;
                    j.push_back(entry);
                }

                msg = msg->NextSiblingElement("msg");
            }
        }
    }

    output << j.dump(2);

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
    
    std::string content((std::istreambuf_iterator<char>(inputBin)), std::istreambuf_iterator<char>());
    replaceStrInplace(content, " @l=", " _l=");

    tinyxml2::XMLDocument doc;
    tinyxml2::XMLError error = doc.Parse(content.c_str());
    if (error != tinyxml2::XML_SUCCESS) {
        throw std::runtime_error("XML Parse Error ID: " + std::to_string(error));
    }

    json j = json::parse(inputTxt);
    size_t translationIndex = 0;

    tinyxml2::XMLElement* root = doc.FirstChildElement("xscript");
    if (root) {
        tinyxml2::XMLElement* code = root->FirstChildElement("code");
        if (code) {
            tinyxml2::XMLElement* msg = code->FirstChildElement("msg");
            while (msg) {
                const char* nameAttr = msg->Attribute("name");
                const char* textAttr = msg->Attribute("text");

                if (textAttr && std::strlen(textAttr) > 0) {
                    if (translationIndex >= j.size()) {
                        throw std::runtime_error("Not enough translations!");
                    }
                    json& item = j[translationIndex++];
                    if (nameAttr && std::strlen(nameAttr) > 0) {
                        msg->SetAttribute("name", item["name"].get<std::string>().c_str());
                    }
                    msg->SetAttribute("text", item["message"].get<std::string>().c_str());
                }

                msg = msg->NextSiblingElement("msg");
            }
        }
    }

    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    std::string xmlStr = printer.CStr();
    outputBin << replaceStrInplace(xmlStr, " _l=", " @l=");

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::println("Injection complete. Output saved to {}", wide2Ascii(outputBinPath));
}

void printUsage(const fs::path& programPath) {
    std::print("Made by julixian 2025.11.27\n"
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
                    const fs::path outputPath = (outputFolder / fs::relative(inputPath, inputFolder)).replace_extension(L".json");
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
                    const fs::path inputTxtPath = (inputTxtFolder / fs::relative(inputBinPath, inputBinFolder)).replace_extension(L".json");
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
