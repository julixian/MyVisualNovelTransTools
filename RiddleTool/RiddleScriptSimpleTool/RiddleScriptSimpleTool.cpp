#include <Windows.h>
#include <cstdint>
#include <CLI/CLI.hpp>

import std;
import Tool;

namespace fs = std::filesystem;

std::string extractString(const std::vector<uint8_t>& buffer, size_t& i, uint8_t length)
{
    std::string text;
    size_t j;
    for (j = 0; j < (size_t)length && i + j < buffer.size(); ++j) {
        if (buffer[i + j] == 0x00) {
            break;
        }
        text.push_back((char)buffer[i + j]);
    }
    i += j;
    return text;
}

void dumpText(const fs::path& inputPath, const fs::path& outputPath)
{
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath);

    if (!input.is_open() || !output.is_open()) {
        std::println("error opening files: {} or {}", wide2Ascii(inputPath.native(), CP_UTF8), wide2Ascii(outputPath.native(), CP_UTF8));
        return;
    }

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    if (buffer.size() < 6) {
        return;
    }

    for (size_t i = 0; i + 6 <= buffer.size(); ++i) {
        if (buffer[i] == 0xF0 && buffer[i + 1] == 0x4C) {
            output << "PageChangeSign\n";
        }

        if ((buffer[i] == 0xF0 && buffer[i + 1] == 0x42) || (buffer[i] == 0xF0 && buffer[i + 1] == 0x00)) {
            uint32_t length = 0;
            i += 2;
            std::memcpy(&length, &buffer[i], sizeof(uint32_t));
            if (length == 0) {
                continue;
            }
            i += 4;
            if (i + length >= buffer.size()) {
                continue;
            }
            std::string text = extractString(buffer, i, (uint8_t)length);
            output << text << "\n";
        }
    }

    std::println("extraction complete: {} -> {}", wide2Ascii(inputPath.native(), CP_UTF8), wide2Ascii(outputPath.native(), CP_UTF8));
}

void injectText(const fs::path& inputBinPath, const fs::path& inputTxtPath, const fs::path& outputBinPath)
{
    std::ifstream inputBin(inputBinPath, std::ios::binary);
    std::ifstream inputTxt(inputTxtPath);
    std::ofstream outputBin(outputBinPath, std::ios::binary);

    if (!inputBin.is_open() || !inputTxt.is_open() || !outputBin.is_open()) {
        std::println("error opening files: {}, {} or {}", wide2Ascii(inputBinPath.native(), CP_UTF8), wide2Ascii(inputTxtPath.native(), CP_UTF8), wide2Ascii(outputBinPath.native(), CP_UTF8));
        return;
    }

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(inputBin)), std::istreambuf_iterator<char>());
    std::vector<std::string> translations;

    std::string line;
    std::string pageChangeSign = "PageChangeSign";
    while (std::getline(inputTxt, line)) {
        if (line != pageChangeSign) {
            translations.push_back(line);
        }
    }

    size_t translationIndex = 0;
    std::vector<uint8_t> newBuffer;

    // 我已经看不懂当初写的这什么一坨了，建议直接分析虚拟机去。当然了，我不去（
    if (buffer.size() > 0x40 && 
        (
            read<uint8_t>(&buffer[0x8]) != 0xF0 &&
            read<uint8_t>(&buffer[0x27]) != 0xF0 &&
            read<uint32_t>(&buffer[0x24]) < buffer.size()
            )
        ) 
    {
        std::println("find select script: {}", wide2Ascii(inputBinPath.native(), CP_UTF8));
        for (size_t i = 0x24; i < buffer.size(); i += 0x20) {
            size_t origAddr = 0;
            std::memcpy(&origAddr, &buffer[i], sizeof(uint32_t));
            if (origAddr >= buffer.size() || origAddr == 0x00000000) {
                break;
            }
            size_t origSeLength = 0;
            size_t transSeLength = 0;
            translationIndex = 0;
            for (size_t j = i; j < origAddr; ++j) {
                if ((buffer[j] == 0xF0 && buffer[j + 1] == 0x42) || (buffer[j] == 0xF0 && buffer[j + 1] == 0x00)) {
                    uint32_t origLength = 0;
                    j += 2;
                    std::memcpy(&origLength, &buffer[j], sizeof(uint32_t));
                    if (origLength == 0) {
                        continue;
                    }
                    if (j + 4 + origLength >= buffer.size()) {
                        continue;
                    }
                    if (translationIndex >= translations.size()) {
                        continue;
                    }
                    origSeLength += origLength;
                    std::string translatedText = translations[translationIndex++];
                    std::vector<uint8_t> textBytes = str2Vec(translatedText);
                    transSeLength += textBytes.size() + 1;
                    j += 3 + origLength;
                }
            }
            if (origSeLength >= transSeLength) {
                size_t dvalue = origSeLength - transSeLength;
                size_t transAddr = origAddr - dvalue;
                std::memcpy(&buffer[i], &transAddr, sizeof(uint32_t));
            } else {
                size_t dvalue = transSeLength - origSeLength;
                size_t transAddr = origAddr + dvalue;
                std::memcpy(&buffer[i], &transAddr, sizeof(uint32_t));
            }
        }
    }

    translationIndex = 0;

    for (size_t i = 0; i < buffer.size(); ++i) {
        if ((buffer[i] == 0xF0 && i + 1 < buffer.size() && buffer[i + 1] == 0x42) || (buffer[i] == 0xF0 && i + 1 < buffer.size() && buffer[i + 1] == 0x00)) {
            uint32_t orgilength = 0;
            i += 2;
            newBuffer.push_back(buffer[i - 2]);
            newBuffer.push_back(buffer[i - 1]);
            std::memcpy(&orgilength, &buffer[i], sizeof(uint32_t));
            if (orgilength == 0) {
                newBuffer.push_back(buffer[i]);
                continue;
            }
            if (i + 4 + orgilength >= buffer.size()) {
                newBuffer.push_back(buffer[i]);
                continue;
            }
            if (translationIndex >= translations.size()) {
                std::println("not enough translations at index {} in file {}", translationIndex, wide2Ascii(inputBinPath.native(), CP_UTF8));
                newBuffer.push_back(buffer[i]);
                continue;
            }
            std::string translatedText = translations[translationIndex++];
            std::vector<uint8_t> textBytes = str2Vec(translatedText);
            uint32_t tempLength = (uint32_t)textBytes.size() + 1;
            newBuffer.push_back(0x00);
            newBuffer.push_back(0x00);
            newBuffer.push_back(0x00);
            newBuffer.push_back(0x00);
            std::memcpy(&newBuffer[newBuffer.size() - 4], &tempLength, sizeof(uint32_t));
            newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
            newBuffer.push_back(0x00);
            i += 3 + orgilength;
        } else {
            newBuffer.push_back(buffer[i]);
        }
    }

    outputBin.write((const char*)newBuffer.data(), (std::streamsize)newBuffer.size());

    std::println("write-back complete: {} -> {}", wide2Ascii(inputBinPath.native(), CP_UTF8), wide2Ascii(outputBinPath.native(), CP_UTF8));
}

void dumpDir(const fs::path& inputDir, const fs::path& outputDir)
{
    for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        fs::path relativePath = fs::relative(entry.path(), inputDir);
        fs::path outputPath = outputDir / relativePath;
        fs::create_directories(outputPath.parent_path());
        dumpText(entry.path(), outputPath);
    }
}

void injectDir(const fs::path& inputBinDir, const fs::path& inputTxtDir, const fs::path& outputDir)
{
    for (const auto& entry : fs::recursive_directory_iterator(inputBinDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        fs::path relativePath = fs::relative(entry.path(), inputBinDir);
        fs::path txtPath = inputTxtDir / relativePath;
        fs::path outputPath = outputDir / relativePath;
        fs::create_directories(outputPath.parent_path());
        if (fs::exists(txtPath)) {
            injectText(entry.path(), txtPath, outputPath);
        } else {
            std::println("warning: no corresponding txt for {}", wide2Ascii(relativePath.native(), CP_UTF8));
        }
    }
}

int main(int argc, char** argv)
{
    SetConsoleOutputCP(CP_UTF8);

    CLI::App app("Made by julixian 2026.03.12", "RiddleScriptSimpleTool");
    argv = app.ensure_utf8(argv);
    app.set_help_all_flag("-a");
    app.require_subcommand(1);

    fs::path inputBinDir;
    fs::path inputTxtDir;
    fs::path outputDir;

    auto dumpCmd = app.add_subcommand("dump");
    dumpCmd->alias("-d");
    dumpCmd->add_option("inputDir", inputBinDir, "input directory")->required()->check(CLI::ExistingDirectory);
    dumpCmd->add_option("outputDir", outputDir, "output directory")->required();

    auto injectCmd = app.add_subcommand("inject");
    injectCmd->alias("-i");
    injectCmd->add_option("inputBinDir", inputBinDir, "input bin directory")->required()->check(CLI::ExistingDirectory);
    injectCmd->add_option("inputTxtDir", inputTxtDir, "input txt directory")->required()->check(CLI::ExistingDirectory);
    injectCmd->add_option("outputDir", outputDir, "output directory")->required();

    CLI11_PARSE(app, argc, argv);

    if (dumpCmd->parsed()) {
        dumpDir(inputBinDir, outputDir);
    } else if (injectCmd->parsed()) {
        injectDir(inputBinDir, inputTxtDir, outputDir);
    }

    return 0;
}



