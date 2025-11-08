#include <cstdint>
#include <cstring>

import std;
namespace fs = std::filesystem;

std::vector<uint8_t> stringToCP932(const std::string& str) {
    std::vector<uint8_t> result;
    for (char c : str) {
        result.push_back(static_cast<uint8_t>(c));
    }
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
        std::cerr << "Error opening files: " << inputPath << " or " << outputPath << std::endl;
        return;
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
                std::cout << "Not have enough translations!" << std::endl;
                continue;
            }
            std::string str = translations[translationIndex];
            translationIndex++;
            str = std::regex_replace(str, pattern_0, "\x00");
            str = std::regex_replace(str, pattern_1, "\x01");
            str = std::regex_replace(str, pattern_2, "\x02");
            str = std::regex_replace(str, pattern_3, "\x03");
            str = std::regex_replace(str, pattern_4, "\x04");
            str = std::regex_replace(str, pattern_5, "\x05");
            str = std::regex_replace(str, pattern_6, "\x06");
            str = std::regex_replace(str, lb1, "\r");
            str = std::regex_replace(str, lb2, "\n");
            std::vector<uint8_t> textBytes = stringToCP932(str);
            textBytes.push_back(0x00);
            uint32_t length = (uint32_t)textBytes.size();
            newBuffer.insert(newBuffer.end(), (uint8_t*)&length, (uint8_t*)&length + 4);
            newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
        }
    }

    if (translationIndex != translations.size()) {
        std::cout << "Warning: translations too much" << std::endl;
    }

    *(uint32_t*)&newBuffer[4] = (uint32_t)newBuffer.size() - scriptBegin;

    // 写入新文件
    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::cout << "Write-back complete. Output saved to " << outputBinPath << std::endl;
}

void printUsage() {
    std::cout << "Made by julixian 2025.11.08" << std::endl;
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