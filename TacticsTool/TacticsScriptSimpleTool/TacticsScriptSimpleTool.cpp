#include <windows.h>
#include <CLI/CLI.hpp>

import std.compat;
import Tool;
namespace fs = std::filesystem;

enum class SentenceType
{
	None, Normal, AutoWithVoice, AutoWithoutVoice
};

struct Sentence {
    size_t sentenceOpAddr = 0;
    uint32_t showNameAddr = 0;
    uint32_t trueNameAddr = 0;
    uint32_t voiceAddr = 0;
    uint32_t msgAddr = 0;
    std::string showName;
    std::string trueName;
    std::string voice;
    std::string msg;
    SentenceType sentenceType = SentenceType::Normal;
};

//DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
void dumpText(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream inputBin(inputPath, std::ios::binary);
    std::ofstream outputTxt(outputPath);

    if (!inputBin || !outputTxt) {
        std::println("Error opening files: {} or {}", wide2Ascii(inputPath), wide2Ascii(outputPath));
        return;
    }

    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(inputBin), {});
    std::vector<Sentence> sentences;
    uint32_t scriptOpBegin = *(uint32_t*)&buffer[4];
    while (scriptOpBegin % 4 != 0) {
        scriptOpBegin--;
    }
    uint32_t scriptOpEnd = 0;

    for (size_t i = buffer.size() - 4; i > 0; --i) {
        if (*(uint32_t*)&buffer[i] == 0x05 || *(uint32_t*)&buffer[i] == 0x0C) {
            scriptOpEnd = (uint32_t)i;
            break;
        }
    }

    for (size_t i = scriptOpBegin; i < scriptOpEnd && i + 20 < buffer.size(); i += 4) {
        if ((*(uint32_t*)&buffer[i] == 0x69
            || *(uint32_t*)&buffer[i] == 0x64
            || *(uint32_t*)&buffer[i] == 0x63
            || *(uint32_t*)&buffer[i] == 0x61
            || *(uint32_t*)&buffer[i] == 0x5F
            || *(uint32_t*)&buffer[i] == 0x5D
            || *(uint32_t*)&buffer[i] == 0x5C
            || *(uint32_t*)&buffer[i] == 0x5B)
            && *(uint32_t*)&buffer[i + 4] < buffer.size()
            && *(uint32_t*)&buffer[i + 4] > scriptOpEnd
            && *(uint32_t*)&buffer[i + 8] < buffer.size()
            && *(uint32_t*)&buffer[i + 8] > scriptOpEnd
            && *(uint32_t*)&buffer[i + 12] < buffer.size()
            && *(uint32_t*)&buffer[i + 12] > scriptOpEnd
            && *(uint32_t*)&buffer[i + 16] < buffer.size()
            && *(uint32_t*)&buffer[i + 16] > scriptOpEnd)
        {
            Sentence sentence;
            sentence.sentenceOpAddr = i;
            sentence.showNameAddr = *(uint32_t*)&buffer[i + 4];
            sentence.trueNameAddr = *(uint32_t*)&buffer[i + 8];
            sentence.voiceAddr = *(uint32_t*)&buffer[i + 12];
            sentence.msgAddr = *(uint32_t*)&buffer[i + 16];
            sentence.sentenceType = SentenceType::Normal;
            sentences.push_back(sentence);
            i += 16;
        }
        else if (*(uint32_t*)&buffer[i] == 0x65
            && *(uint32_t*)&buffer[i + 4] == 0x01
            && *(uint32_t*)&buffer[i + 8] == 0x64
            && *(uint32_t*)&buffer[i + 12] < scriptOpEnd
            && *(uint32_t*)&buffer[i + 16] < buffer.size()
            && *(uint32_t*)&buffer[i + 16] > scriptOpEnd
            )
        {
            Sentence sentence;
            sentence.sentenceOpAddr = i;
            sentence.showNameAddr = 0;
            sentence.trueNameAddr = 0;
            sentence.voiceAddr = 0;
            sentence.msgAddr = *(uint32_t*)&buffer[i + 16];
            sentence.sentenceType = SentenceType::AutoWithoutVoice;
            sentences.push_back(sentence);
            i += 16;
        }
        else if (i + 28 < buffer.size()
            && *(uint32_t*)&buffer[i] == 0x64
            && *(uint32_t*)&buffer[i + 4] < scriptOpEnd
            && *(uint32_t*)&buffer[i + 8] < buffer.size()
            && *(uint32_t*)&buffer[i + 8] > scriptOpEnd
            && *(uint32_t*)&buffer[i + 12] < scriptOpEnd
            && *(uint32_t*)&buffer[i + 16] == 0x64
            && *(uint32_t*)&buffer[i + 20] < scriptOpEnd
            && *(uint32_t*)&buffer[i + 24] < buffer.size()
            && *(uint32_t*)&buffer[i + 24] > scriptOpEnd
            )
        {
            Sentence sentence;
            sentence.sentenceOpAddr = i;
            sentence.showNameAddr = *(uint32_t*)&buffer[i + 8];
            sentence.trueNameAddr = 0;
            sentence.voiceAddr = 0;
            sentence.msgAddr = *(uint32_t*)&buffer[i + 24];
            sentence.sentenceType = SentenceType::AutoWithVoice;
            sentences.push_back(sentence);
            i += 24;
        }
    }

    for (const auto& sentence : sentences) {
        if (sentence.sentenceType == SentenceType::AutoWithVoice) {
            outputTxt << "[Auto1]";
        }
        else if (sentence.sentenceType == SentenceType::AutoWithoutVoice) {
            outputTxt << "[Auto2]";
        }
        outputTxt << std::hex << sentence.sentenceOpAddr << ":::::";
        if (sentence.showNameAddr != 0x00 && buffer[sentence.showNameAddr] != 0x00) {
            std::string_view str((char*)&buffer[sentence.showNameAddr]);
            outputTxt << str << ":::::";
        }
        else {
            outputTxt << "empty" << ":::::";
        }
        if (sentence.trueNameAddr != 0x00 && buffer[sentence.trueNameAddr] != 0x00) {
            std::string_view str((char*)&buffer[sentence.trueNameAddr]);
            outputTxt << str << ":::::";
        }
        else {
            outputTxt << "empty" << ":::::";
        }
        if (sentence.voiceAddr != 0x00 && buffer[sentence.voiceAddr] != 0x00) {
            std::string_view str((char*)&buffer[sentence.voiceAddr]);
            outputTxt << str << ":::::";
        }
        else {
            outputTxt << "empty" << ":::::";
        }
        if (buffer[sentence.msgAddr] != 0x00) {
            std::string str((char*)&buffer[sentence.msgAddr]);
            replaceStrInplace(str, "\n", "<br>");
            outputTxt << str << ":::::";
        }
        else {
            outputTxt << "empty" << ":::::";
        }
        outputTxt << "\n";
    }

    inputBin.close();
    outputTxt.close();

    std::println("Extraction complete: {} -> {}", wide2Ascii(inputPath), wide2Ascii(outputPath));
}



void insertStr(std::vector<uint8_t>& buffer, const std::string& str, size_t zeroCount) {
    std::vector<uint8_t> textBytes = str2Vec(str);
    buffer.insert(buffer.end(), textBytes.begin(), textBytes.end());
    for (size_t i = 0; i < zeroCount; i++) {
        buffer.push_back(0x00);
    }
}

//IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII
void injectText(const fs::path& inputBinPath, const fs::path& inputTxtPath, const fs::path& outputBinPath) {
    std::ifstream inputBin(inputBinPath, std::ios::binary);
    std::ifstream inputTxt(inputTxtPath);
    std::ofstream outputBin(outputBinPath, std::ios::binary);

    if (!inputBin || !inputTxt || !outputBin) {
        std::println("Error opening files: {}, {} or {}", wide2Ascii(inputBinPath), wide2Ascii(inputTxtPath), wide2Ascii(outputBinPath));
        return;
    }

    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(inputBin), {});
    std::vector<uint8_t> newBuffer = buffer;
    std::vector<Sentence> sentences;

    // 读取翻译文本
    std::string line;
    while (std::getline(inputTxt, line)) {
        Sentence sentence;
        size_t posb = 0;
        size_t pose = line.find(":::::", posb);
        if (line.starts_with("[Auto1]")) {
            sentence.sentenceType = SentenceType::AutoWithVoice;
            posb = 7;
        }
    	else if (line.starts_with("[Auto2]")) {
            sentence.sentenceType = SentenceType::AutoWithoutVoice;
            posb = 7;
        }
        sentence.sentenceOpAddr = std::stoul(line.substr(posb, pose - posb), nullptr, 16);
        posb = pose + 5;
        pose = line.find(":::::", posb);
        sentence.showName = line.substr(posb, pose - posb);
        posb = pose + 5;
        pose = line.find(":::::", posb);
        sentence.trueName = line.substr(posb, pose - posb);
        posb = pose + 5;
        pose = line.find(":::::", posb);
        sentence.voice = line.substr(posb, pose - posb);
        posb = pose + 5;
        pose = line.find(":::::", posb);
        sentence.msg = line.substr(posb, pose - posb);
        replaceStrInplace(sentence.msg, "<br>", "\n");
        sentences.push_back(sentence);
    }

    for (const auto& sentence : sentences) {
        if (sentence.sentenceType == SentenceType::Normal) {
            if (sentence.showName != "empty") {
                while (newBuffer.size() % 4 != 0) {
                    newBuffer.push_back(0);
                }
                *(uint32_t*)&newBuffer[sentence.sentenceOpAddr + 4] = (uint32_t)newBuffer.size();
                insertStr(newBuffer, sentence.showName, 2);
            }
            if (sentence.trueName != "empty") {
                while (newBuffer.size() % 4 != 0) {
                    newBuffer.push_back(0);
                }
                *(uint32_t*)&newBuffer[sentence.sentenceOpAddr + 8] = (uint32_t)newBuffer.size();
                insertStr(newBuffer, sentence.trueName, 2);
            }
            if (sentence.voice != "empty") {
                while (newBuffer.size() % 4 != 0) {
                    newBuffer.push_back(0);
                }
                *(uint32_t*)&newBuffer[sentence.sentenceOpAddr + 12] = (uint32_t)newBuffer.size();
                insertStr(newBuffer, sentence.voice, 2);
            }
            if (sentence.msg != "empty") {
                while (newBuffer.size() % 4 != 0) {
                    newBuffer.push_back(0);
                }
                *(uint32_t*)&newBuffer[sentence.sentenceOpAddr + 16] = (uint32_t)newBuffer.size();
                insertStr(newBuffer, sentence.msg, 2);
            }
        }
        else if (sentence.sentenceType == SentenceType::AutoWithVoice) {
            if (sentence.showName != "empty") {
                while (newBuffer.size() % 4 != 0) {
                    newBuffer.push_back(0);
                }
                *(uint32_t*)&newBuffer[sentence.sentenceOpAddr + 8] = (uint32_t)newBuffer.size();
                insertStr(newBuffer, sentence.showName, 2);
            }
            if (sentence.msg != "empty") {
                while (newBuffer.size() % 4 != 0) {
                    newBuffer.push_back(0);
                }
                *(uint32_t*)&newBuffer[sentence.sentenceOpAddr + 24] = (uint32_t)newBuffer.size();
                insertStr(newBuffer, sentence.msg, 2);
            }
        }
        else if (sentence.sentenceType == SentenceType::AutoWithoutVoice) {
            if (sentence.msg != "empty") {
                while (newBuffer.size() % 4 != 0) {
                    newBuffer.push_back(0);
                }
                *(uint32_t*)&newBuffer[sentence.sentenceOpAddr + 16] = (uint32_t)newBuffer.size();
                insertStr(newBuffer, sentence.msg, 2);
            }
        }
    }

    // 写入新文件
    outputBin.write((char*)newBuffer.data(), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::println("Write-back complete: {} -> {}", wide2Ascii(inputBinPath), wide2Ascii(outputBinPath));
}


int main(int argc, char** argv)
{
    SetConsoleOutputCP(CP_UTF8);

    CLI::App app("Made by julixian 2026.06.17", "TacticsScriptSimpleTool");
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

    fs::create_directories(outputDir);
    if (dumpCmd->parsed()) {
        for (const auto& entry : fs::recursive_directory_iterator(inputBinDir)) {
            if (entry.is_regular_file()) {
                const fs::path relativePath = fs::relative(entry.path(), inputBinDir);
                const fs::path outputPath = outputDir / relativePath;
                fs::create_directories(outputPath.parent_path());
                dumpText(entry.path(), outputPath);
            }
        }
    }
    else if (injectCmd->parsed()) {
        for (const auto& entry : fs::recursive_directory_iterator(inputBinDir)) {
            if (entry.is_regular_file()) {
                const fs::path relativePath = fs::relative(entry.path(), inputBinDir);
                const fs::path txtPath = inputTxtDir / relativePath;
                if (fs::exists(txtPath)) {
                    const fs::path outputPath = outputDir / relativePath;
                    fs::create_directories(outputPath.parent_path());
                    injectText(entry.path(), txtPath, outputPath);
                }
                else {
                    std::println("Warning: no corresponding txt for {}", wide2Ascii(relativePath));
                }
            }
        }
    }

    return 0;
}
