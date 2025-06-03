#include <iostream>
#include <fstream>
#include <windows.h>
#include <vector>
#include <string>
#include <regex>
#include <filesystem>

namespace fs = std::filesystem;

std::vector<uint8_t> stringToCP932(const std::string& str) {
    std::vector<uint8_t> result;
    for (char c : str) {
        result.push_back(static_cast<uint8_t>(c));
    }
    return result;
}

bool isValidCP932(const std::string& str) {
    std::vector<uint8_t> textBytes = stringToCP932(str);
    for (size_t i = 0; i < textBytes.size(); i++) {
        if (textBytes[i] < 0x20 || (0x9f < textBytes[i] && textBytes[i] < 0xe0)) {
            //std::cout << str << " :E1 " << i << std::endl;
            return false;
        }
        else if ((i + 1 < textBytes.size())
            &&
            ((0x81 <= textBytes[i] && textBytes[i] <= 0x9f) || (0xe0 <= textBytes[i] && textBytes[i] <= 0xef))) { //most games use sjis, for cp932 exactly should be <= 0xfc here
            if (textBytes[i + 1] > 0xfc || textBytes[i + 1] < 0x40) {
                //std::cout << str << " :E2 " << i << std::endl;
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
    size_t offsetAddr;
    uint16_t seq;
    std::string str;
};

struct pushedSentence {
    uint32_t newOffset;
    uint16_t seq;
    std::string str;
};

//DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
void dumpText(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath);

    if (!input || !output) {
        std::cerr << "Error opening files: " << inputPath << " or " << outputPath << std::endl;
        return;
    }

    std::vector<Sentence> Sentences;
    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});
    size_t p = buffer.size() - 1;
    while (buffer[p] == 0x00)p--;
    while (!(buffer[p] == 0x00 && buffer[p - 4] != 0x02))p--;
    p++;

    bool oldver = false;
    size_t ScriptBegin = *(uint32_t*)&buffer[0x4];
    if (ScriptBegin >= buffer.size()) {
        oldver = true;
        ScriptBegin = *(uint32_t*)&buffer[0x6];
    }

    ScriptBegin += oldver ? 0x12 : 0x10;
    size_t firstOffset = 0;
    if (*(uint32_t*)&buffer[p - 4] != 0x00) {
        firstOffset = p - ScriptBegin;
    }
    else {
        firstOffset = p - 3 - ScriptBegin;
    }
    bool begin = false;
    for (size_t i = oldver ? 0x12 : 0x10; i < ScriptBegin; i += 4) {
        uint32_t offset = 0;
        memcpy(&offset, &buffer[i], sizeof(uint32_t));
        if (!begin) {
            if (offset == firstOffset) {
                begin = true;
            }
            else {
                continue;
            }
        }
        size_t SeAddr = ScriptBegin + offset;
        if (SeAddr >= buffer.size())continue;
        if (buffer[SeAddr] == 0 && buffer[SeAddr + 3] >= 0x20 && buffer[SeAddr + 3] != 0x40) {
            uint16_t seq = 0;
            memcpy(&seq, &buffer[SeAddr + 1], sizeof(uint16_t));
            std::string str((char*)&buffer[SeAddr + 3]);
            if (str.find('\x02') != std::string::npos) {
                while (buffer[SeAddr + 3 + str.length() + 1] != 0x00 && str.back() != '$') {
                    str.push_back('\x00');
                    str += std::string((char*)&buffer[SeAddr + 3 + str.length()]);
                }
            }
            //std::cout << "offset: " << std::hex << i << " SeAddr: " << SeAddr << std::endl;
            size_t length = str.length();// 修复自定义人名
            if (!oldver && str.length() > 4) {
                for (size_t j = SeAddr + 3; j < SeAddr + 3 + length - 4; j++) {
                    if (buffer[j] == 0x20 && buffer[j + 1] == 0xbc && buffer[j + 2] == 0x80 && buffer[j + 3] == 0x01) {
                        //std::cout << "Fix find: " << std::hex << j << std::endl;
                        size_t fixoffset = j - ScriptBegin; //0x20的偏移，也就是偏移列表应该显示的一个偏移
                        size_t fixop = 0; //显示这个偏移的位置
                        for (size_t k = 0x10; k < ScriptBegin; k += 4) {
                            uint32_t foffset = 0;
                            memcpy(&foffset, &buffer[k], sizeof(uint32_t));
                            if (foffset == fixoffset) {
                                fixop = k;
                            }
                        }
                        std::string nameFix = "";
                        nameFix += "[Fix:";
                        nameFix += std::to_string(fixop);
                        nameFix += "]";
                        str.insert(str.length(), nameFix);
                    }
                }
            }
            else if (oldver && str.length() > 5) {
                for (size_t j = SeAddr + 3; j < SeAddr + 3 + length - 5; j++) {
                    if (buffer[j] == 0x02 && buffer[j + 3] == 0x92 && buffer[j + 4] == 0x00) {
                        //std::cout << "Fix find: " << std::hex << j + 1 << std::endl;
                        size_t fixoffset = j + 1 - ScriptBegin; //0x02后一位的偏移，也就是偏移列表应该显示的一个偏移
                        size_t fixop = 0; //显示这个偏移的位置
                        for (size_t k = 0xA; k < ScriptBegin; k += 4) {
                            uint32_t foffset = 0;
                            memcpy(&foffset, &buffer[k], sizeof(uint32_t));
                            if (foffset == fixoffset) {
                                fixop = k;
                            }
                        }
                        std::string nameFix = "";
                        nameFix += "[Fix:";
                        nameFix += std::to_string(fixop);
                        nameFix += "]";
                        str.insert(str.length(), nameFix);
                    }
                }
            }
            //if (!isValidCP932(str))continue;
            Sentence se;
            se.offsetAddr = i;
            se.seq = seq;
            se.str = str;
            Sentences.push_back(se);
            //std::cout << std::hex << i << std::endl;
        }
        else if ((buffer[SeAddr] == 0x00 && buffer[SeAddr + 2] == 0x00 && buffer[SeAddr + 3] != 0x00 && buffer[SeAddr + 4] == 0x00)
            || (oldver && buffer[SeAddr] < 10 && buffer[SeAddr + 1] > 3 && buffer[SeAddr + 2] == 0x00)) {
            if (!oldver) {
                uint16_t selectCount = 0;
                memcpy(&selectCount, &buffer[SeAddr + 3], sizeof(uint16_t));
                if (selectCount == 0) {
                    //std::cout << "select0 at: " << std::hex << i << " : " << SeAddr << std::endl;
                    continue;
                }
                SeAddr += 5;
                std::string str = "Select:";
                for (size_t j = 0; j < selectCount; j++) {
                    uint16_t length = 0;
                    memcpy(&length, &buffer[SeAddr], sizeof(uint16_t));
                    std::string select((char*)&buffer[SeAddr + 2]);
                    str += select;
                    str += "|||||";
                    SeAddr += length;
                }
                Sentence se;
                se.offsetAddr = i;
                se.seq = selectCount;
                se.str = str;
                Sentences.push_back(se);
            }
            else {
                uint8_t selectCount = 0;
                memcpy(&selectCount, &buffer[SeAddr], sizeof(uint8_t));
                if (selectCount == 0) {
                    //std::cout << "select0 at: " << std::hex << i << " : " << SeAddr << std::endl;
                    continue;
                }
                SeAddr += 1;
                std::string str = "Select:";
                for (size_t j = 0; j < selectCount; j++) {
                    uint16_t length = 0;
                    memcpy(&length, &buffer[SeAddr], sizeof(uint16_t));
                    std::string select((char*)&buffer[SeAddr + 2]);
                    str += select;
                    str += "|||||";
                    SeAddr += length;
                }
                Sentence se;
                se.offsetAddr = i;
                se.seq = selectCount;
                se.str = str;
                Sentences.push_back(se);
            }
        }
        else if (buffer[SeAddr] >= 0x20 && buffer[SeAddr] <= 0xef) {
            std::string str((char*)&buffer[SeAddr]);
            //std::cout << str << std::endl;
            //if (str.length() < 2 && str!="0")continue;
            if (!isValidCP932(str))continue;
            Sentence se;
            se.offsetAddr = i;
            se.seq = 0xffff;
            se.str = str;
            Sentences.push_back(se);
        }
    }

    //reverse(Sentences.begin(), Sentences.end());
    std::erase_if(Sentences, [&](Sentence& Se)
        {
            return *(uint32_t*)&buffer[Se.offsetAddr] < *(uint32_t*)&buffer[Sentences.back().offsetAddr];
        });

    for (auto it = Sentences.begin(); it != Sentences.end(); it++) {
        if (it->str.length() > 5) {
            for (size_t k = 0; k < it->str.length() - 5; k++) {
                if (it->str[k] == '\x02' && it->str[k + 3] == '\x92' && it->str[k + 4] == '\x00') {
                    it->str.replace(k + 3, 2, "\\x92\\x00");
                    k += 10;
                }
            }
        }
        output << std::hex << it->offsetAddr << ":::::" << (size_t)it->seq << ":::::" << it->str << std::endl;
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

    std::vector<Sentence> Sentences;
    std::vector<pushedSentence> pushedSentences;
    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(inputBin), {});
    std::vector<std::string> translations;

    std::string line;
    while (std::getline(inputTxt, line)) {

        size_t first_sep = line.find(":::::");

        size_t second_sep = line.find(":::::", first_sep + 5);

        std::string first_str = line.substr(0, first_sep);
        std::string second_str = line.substr(first_sep + 5, second_sep - (first_sep + 5));
        std::string trans = line.substr(second_sep + 5);

        size_t opoffset = std::stoul(first_str, nullptr, 16);
        uint16_t seq = std::stoul(second_str, nullptr, 16);
        std::regex pattern(R"(\\x92\\x00)");
        std::string tmp;
        tmp.push_back('\x92'); tmp.push_back('\x00');
        trans = std::regex_replace(trans, pattern, tmp);
        Sentences.push_back({ opoffset, seq, trans });
    }

    bool oldver = false;
    size_t ScriptBegin = *(uint32_t*)&buffer[0x4];
    if (ScriptBegin >= buffer.size()) {
        oldver = true;
        ScriptBegin = *(uint32_t*)&buffer[0x6];
    }
    ScriptBegin += oldver ? 0x12 : 0x10;
    std::vector<uint8_t> newBuffer;
    reverse(Sentences.begin(), Sentences.end());
    uint32_t ScriptChunckBegin = 0;
    memcpy(&ScriptChunckBegin, &buffer[Sentences[0].offsetAddr], sizeof(uint32_t));
    ScriptChunckBegin += ScriptBegin;
    newBuffer.resize(ScriptChunckBegin);
    memcpy(newBuffer.data(), buffer.data(), ScriptChunckBegin);

    for (auto it = Sentences.begin(); it != Sentences.end(); it++) {
        if (strncmp(it->str.c_str(), "Select:", 7) == 0) {
            if (!oldver) {
                uint32_t newOffset = newBuffer.size() - ScriptBegin;
                memcpy(&newBuffer[it->offsetAddr], &newOffset, sizeof(uint32_t));
                newBuffer.push_back(0x00);
                newBuffer.push_back(0x00);
                newBuffer.push_back(0x00);
                size_t orgiof = 0;
                memcpy(&orgiof, &buffer[it->offsetAddr], sizeof(uint32_t));
                memcpy(&newBuffer[newBuffer.size() - 2], &buffer[orgiof + ScriptBegin + 1], sizeof(uint16_t));
                newBuffer.push_back(0x00);
                newBuffer.push_back(0x00);
                size_t stepf = 7;
                size_t stepl = 0;
                memcpy(&newBuffer[newBuffer.size() - 2], &it->seq, sizeof(uint16_t));
                for (size_t j = 0; j < it->seq; j++) {
                    stepl = it->str.find("|||||", stepf);
                    std::string text = it->str.substr(stepf, stepl - stepf);
                    //std::cout << text << std::endl;
                    std::vector<uint8_t> textBytes = stringToCP932(text);
                    uint16_t newlength = textBytes.size() + 3;
                    newBuffer.push_back(0x00);
                    newBuffer.push_back(0x00);
                    memcpy(&newBuffer[newBuffer.size() - 2], &newlength, sizeof(uint16_t));
                    newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
                    newBuffer.push_back(0x00);
                    stepf = stepl + 5;
                }
                newBuffer.push_back(0x00);
            }
            else {
                uint32_t newOffset = newBuffer.size() - ScriptBegin;
                memcpy(&newBuffer[it->offsetAddr], &newOffset, sizeof(uint32_t));
                newBuffer.push_back(0x00);
                size_t stepf = 7;
                size_t stepl = 0;
                memcpy(&newBuffer[newBuffer.size() - 1], &it->seq, sizeof(uint8_t));
                for (size_t j = 0; j < it->seq; j++) {
                    stepl = it->str.find("|||||", stepf);
                    std::string text = it->str.substr(stepf, stepl - stepf);
                    //std::cout << text << std::endl;
                    std::vector<uint8_t> textBytes = stringToCP932(text);
                    uint16_t newlength = textBytes.size() + 3;
                    newBuffer.push_back(0x00);
                    newBuffer.push_back(0x00);
                    memcpy(&newBuffer[newBuffer.size() - 2], &newlength, sizeof(uint16_t));
                    newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
                    newBuffer.push_back(0x00);
                    stepf = stepl + 5;
                }
                newBuffer.push_back(0x00);
            }
        }
        else {
            if (it->seq == 0xffff) {
                auto pushedit = std::find_if(pushedSentences.begin(), pushedSentences.end(), [&](const pushedSentence& pushedSe)
                    {
                        return pushedSe.seq == 0xFFFF && pushedSe.str == it->str;
                    });
                if (pushedit != pushedSentences.end()) {
                    uint32_t newOffset = pushedit->newOffset;
                    memcpy(&newBuffer[it->offsetAddr], &newOffset, sizeof(uint32_t));
                }
                else {
                    uint32_t newOffset = newBuffer.size() - ScriptBegin;
                    memcpy(&newBuffer[it->offsetAddr], &newOffset, sizeof(uint32_t));
                    std::vector<uint8_t> textBytes = stringToCP932(it->str);
                    newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
                    newBuffer.push_back(0x00);
                    pushedSentences.push_back({ newOffset, 0xFFFF, it->str });
                }
            }
            else {
                auto pushedit = std::find_if(pushedSentences.begin(), pushedSentences.end(), [&](const pushedSentence& pushedSe)
                    {
                        return pushedSe.seq == it->seq;
                    });
                if (pushedit != pushedSentences.end()) {
                    uint32_t newOffset = pushedit->newOffset;
                    memcpy(&newBuffer[it->offsetAddr], &newOffset, sizeof(uint32_t));
                    continue;
                }
                std::vector<size_t> Fixops;
                while (true) {
                    size_t pos = it->str.find("[Fix:");
                    if (pos != std::string::npos) {
                        //std::cout << it->str << std::endl;
                        size_t end = it->str.find("]", pos);
                        std::string Fixopstr = it->str.substr(pos + 5, end - pos - 5);
                        //std::cout << "Fixopstr: " << Fixopstr << std::endl;
                        size_t Fixop = std::stoul(Fixopstr, nullptr, 10);
                        Fixops.push_back(Fixop);
                        it->str.erase(pos, end - pos + 1);
                        //std::cout << it->str << std::endl;
                    }
                    else {
                        break;
                    }
                }
                uint32_t newOffset = newBuffer.size() - ScriptBegin;
                memcpy(&newBuffer[it->offsetAddr], &newOffset, sizeof(uint32_t));
                newBuffer.push_back(0x00);
                newBuffer.push_back(0x00);
                newBuffer.push_back(0x00);
                memcpy(&newBuffer[newBuffer.size() - 2], &it->seq, sizeof(uint16_t));
                std::vector<uint8_t> textBytes = stringToCP932(it->str);
                newBuffer.insert(newBuffer.end(), textBytes.begin(), textBytes.end());
                newBuffer.push_back(0x00);
                if (!Fixops.empty()) {
                    //std::cout << it->str << std::endl;
                    size_t u = 0;
                    if (!oldver) {
                        for (size_t j = newBuffer.size() - textBytes.size() - 1; j < newBuffer.size() - 1 - 4; j++) {
                            if (newBuffer[j] == 0x20 && newBuffer[j + 1] == 0xbc && newBuffer[j + 2] == 0x80 && newBuffer[j + 3] == 0x01) {
                                size_t fixedoffset = j - ScriptBegin;
                                if (u >= Fixops.size()) {
                                    std::cout << "Sentence: " << it->str << "(seq:" << std::hex << it->seq << ")\ndosen't have enough fix ops " << std::endl;
                                    system("pause");
                                    continue;
                                }
                                memcpy(&newBuffer[Fixops[u]], &fixedoffset, sizeof(uint32_t));
                                u++;
                            }
                        }
                    }
                    else {
                        for (size_t j = newBuffer.size() - textBytes.size() - 1; j < newBuffer.size() - 1 - 5; j++) {
                            if (newBuffer[j] == 0x02 && newBuffer[j + 3] == 0x92 && newBuffer[j + 4] == 0x00) {
                                size_t fixedoffset = j + 1 - ScriptBegin;
                                if (u >= Fixops.size()) {
                                    std::cout << "Sentence: " << it->str << "(seq:" << std::hex << it->seq << ")\ndosen't have enough fix ops " << std::endl;
                                    system("pause");
                                    continue;
                                }
                                memcpy(&newBuffer[Fixops[u]], &fixedoffset, sizeof(uint32_t));
                                u++;
                            }
                        }
                    }
                    if (u != Fixops.size()) {
                        std::cout << "Sentence: " << it->str << "(seq:" << std::hex << it->seq << ")\ndoesn't have enough custom name" << std::endl;
                        system("pause");
                    }
                }
                pushedSentences.push_back({ newOffset, it->seq, "" });
            }
        }
    }

    // 写入新文件
    if (newBuffer.size() < buffer.size()) {
        //newBuffer.insert(newBuffer.end(), buffer.begin() + newBuffer.size(), buffer.end());
        newBuffer.resize(buffer.size());
    }
    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::cout << "Write-back complete. Output saved to " << outputBinPath << std::endl;
}

void printUsage() {
    std::cout << "Made by julixian 2025.06.03" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  Dump:   ./program dump <input_folder> <output_folder>" << std::endl;
    std::cout << "  Inject: ./program inject <input_orgi-bin_folder> <input_translated-txt_folder> <output_folder>" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }
    system("chcp 932");
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