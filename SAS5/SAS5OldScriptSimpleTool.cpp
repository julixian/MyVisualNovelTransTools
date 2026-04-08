#include <Windows.h>
#include <cstdint>

import std;
namespace fs = std::filesystem;

template<typename T>
T read(void* ptr)
{
    T value;
    memset(&value, 0, sizeof(T));
    std::memcpy(&value, ptr, sizeof(T));
    return value;
}

template<typename T>
void write(void* ptr, T value)
{
    std::memcpy(ptr, &value, sizeof(T));
}

std::vector<uint8_t> stringToCP932(const std::string& str) {
    std::vector<uint8_t> result;
    for (char c : str) {
        result.push_back(static_cast<uint8_t>(c));
    }
    return result;
}

std::string WideToAscii(const std::wstring& wide, UINT CodePage) {
    int len = WideCharToMultiByte(936, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return "";
    std::string ascii(len, '\0');
    WideCharToMultiByte(936, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
    ascii.pop_back();
    return ascii;
}

std::wstring AsciiToWide(const std::string& ascii, UINT CodePage) {
    int len = MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, nullptr, 0);
    if (len == 0) return L"";
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, &wide[0], len);
    wide.pop_back();
    return wide;
}

bool isValidCP932(const std::string& str) {
    if (str.empty())return false;
    std::vector<uint8_t> textBytes = stringToCP932(str);
    for (size_t i = 0; i < textBytes.size(); i++) {
        if ((textBytes[i] < 0x20 && textBytes[i] != 0x0d && textBytes[i] != 0x0a) || (0x9f < textBytes[i] && textBytes[i] < 0xe0)) {
            return false;
        }
        else if ((0x81 <= textBytes[i] && textBytes[i] <= 0x9f) || (0xe0 <= textBytes[i] && textBytes[i] <= 0xFC)) {
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

struct Command
{
    uint32_t addr = 0;
    uint16_t executorType = 0;
    std::string str;
};

struct RelJmp
{
    uint32_t addrInBuffer = 0;
    uint32_t addrInNewBuffer = 0;
};

struct Sentence {
    uint32_t addr;
    int offset;
};

//DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
void dumpText(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream input(inputPath, std::ios::binary);
    std::ofstream output(outputPath);
    std::ofstream debug(outputPath.wstring() + L".debug");

    if (!input || !output || !debug) {
        std::cerr << "Error opening files: " << inputPath << " or " << outputPath
            << " or " << outputPath.string() + ".extra"
            << " or " << outputPath.string() + ".debug" << std::endl;
        return;
    }
    std::vector<Command> commands;
    std::regex lb1(R"(\r)");
    std::regex lb2(R"(\n)");

    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});
    uint32_t offset = 0;
    while (offset < buffer.size()) {
        uint8_t type = buffer[offset];
        offset += 1;
        if (type == 0x1A) {
            Command command;
            command.addr = offset - 1;
            command.str = "0x1A";
            commands.push_back(command);
            continue;
        }
        else if (type == 0x1B) {
            Command command;
            uint16_t executorType = read<uint16_t>(&buffer[offset]);
            command.addr = offset - 1;
            command.executorType = executorType;
            offset += 2;
            uint32_t argCount = 0;
            if (executorType == 0x1F8) {
                output << "[pre_unfinish]";
            }
            while (true) {
                uint8_t argId = buffer[offset];
                offset += 1;
                argCount++;
                if (argId == 0xFF) {
                    break;
                }
                command.str += std::format("ArgId.{:#x}< ", argId);
                while (true) {
                    uint8_t opCode = buffer[offset];
                    offset += 1;
                    if (opCode == 0xFF) {
                        break;
                    }
                    switch (opCode) {
                    case 0x01: {
                        int i32 = read<int>(&buffer[offset]);
                        offset += 4;
                        command.str += std::format("ReadInt32[{:#x}]", i32);
                        if (
                            i32 != 0 && i32 < buffer.size() && 
                            (buffer[i32] == 0x1B || buffer[i32] == 0x1A || (buffer[i32] > 0x80 && isValidCP932(std::string((char*)&buffer[i32]))))
                                ) {
                            debug << std::format("MaybeAbsoluteJump at {:#x}, target: {:#x}\n", offset - 4, i32);
                            command.str += std::format("[MaybeAbsoluteJump to {:#x}]", i32);
                            /*if (!((executorType == 0x02 || executorType == 0x03) && argId == 0x01)) {
                                std::print("AbsoluteJump at {:#x} to {:#x}, executorType: {:#x}, argId: {:#x}\n", offset - 4, i32, executorType, argId);
                                system("pause");
                            }*/
                        }
                        if (i32 + offset < buffer.size() && (buffer[i32 + offset] == 0x1B || buffer[i32 + offset] == 0x1A || (buffer[i32 + offset] > 0x80 && isValidCP932(std::string((char*)&buffer[i32 + offset]))))) {
                            debug << std::format("MaybeRelativeJump at {:#x}, target: {:#x}\n", offset - 4, i32 + offset);
                            command.str += std::format("[MaybeRelativeJump to {:#x}]", i32 + offset);
                        }
                        command.str += ", ";
                        break;
                    }
                    case 0x02: {
                        short i16 = read<short>(&buffer[offset]);
                        offset += 2;
                        command.str += std::format("ReadInt16[{:#x}], ", i16);
                        break;
                    }
                    case 0x03: {
                        char i8 = read<char>(&buffer[offset]);
                        offset += 1;
                        command.str += std::format("ReadInt8[{:#x}], ", i8);
                        break;
                    }
                    case 0x04: {
                        uint32_t length = read<uint32_t>(&buffer[offset]);
                        offset += 4;
                        std::string str((char*)&buffer[offset], length);
                        str = std::regex_replace(str, lb1, "[r]");
                        str = std::regex_replace(str, lb2, "[n]");
                        offset += length;
                        command.str += std::format("ReadString[{}]", str);
                        if (
                            ((executorType == 0x1c || executorType == 0x28) && argId == 0x02 && argCount == 3) ||
                            (executorType == 0x200 && argId == 0x00)
                            ) {
                            output << "[Spec1]";
                        }
                        else {
                            output << "[Spec2]";
                        }
                        output << str << std::endl;
                        break;
                    }
                    case 0x05: {
                        double f64 = read<double>(&buffer[offset]);
                        offset += 8;
                        command.str += std::format("ReadDouble[{}]", f64);
                        break;
                    }

                    case 0x10: {
                        int i32 = read<int>(&buffer[offset]);
                        offset += 4;
                        command.str += std::format("ReadInt32_AsType1[{:#x}]", i32);
                        if (i32 != 0 && i32 < buffer.size() && (buffer[i32] == 0x1B || buffer[i32] == 0x1A || (buffer[i32] > 0x80 && isValidCP932(std::string((char*)&buffer[i32]))))) {
                            debug << std::format("MaybeAbsoluteJump at {:#x}, target: {:#x}\n", offset - 4, i32);
                            command.str += std::format("[MaybeAbsoluteJump to {:#x}]", i32);
                        }
                        if (i32 + offset < buffer.size() && (buffer[i32 + offset] == 0x1B || buffer[i32 + offset] == 0x1A || (buffer[i32 + offset] > 0x80 && isValidCP932(std::string((char*)&buffer[i32 + offset]))))) {
                            debug << std::format("MaybeRelativeJump1 at {:#x}, target: {:#x}, rel: {:#x}\n", offset - 4, i32 + offset, i32);
                            command.str += std::format("[MaybeRelativeJump to {:#x}]", i32 + offset);
                        }
                        command.str += ", ";
                        break;
                    }
                    case 0x11: {
                        int i32 = read<int>(&buffer[offset]);
                        offset += 4;
                        command.str += std::format("ReadInt32_AsType2[{:#x}]", i32);
                        if (i32 != 0 && i32 < buffer.size() && (buffer[i32] == 0x1B || buffer[i32] == 0x1A || (buffer[i32] > 0x80 && isValidCP932(std::string((char*)&buffer[i32]))))) {
                            debug << std::format("MaybeAbsoluteJump at {:#x}, target: {:#x}\n", offset - 4, i32);
                            command.str += std::format("[MaybeAbsoluteJump to {:#x}]", i32);
                        }
                        if (i32 + offset < buffer.size() && (buffer[i32 + offset] == 0x1B || buffer[i32 + offset] == 0x1A || (buffer[i32 + offset] > 0x80 && isValidCP932(std::string((char*)&buffer[i32 + offset]))))) {
                            debug << std::format("MaybeRelativeJump2 at {:#x}, target: {:#x}, rel: {:#x}\n", offset - 4, i32 + offset, i32);
                            command.str += std::format("[MaybeRelativeJump to {:#x}]", i32 + offset);
                        }
                        command.str += ", ";
                        break;
                    }
                    case 0x12: {
                        int i32 = read<int>(&buffer[offset]);
                        offset += 4;
                        command.str += std::format("ReadInt32_AsType3[{:#x}]", i32);
                        if (i32 != 0 && i32 < buffer.size() && (buffer[i32] == 0x1B || buffer[i32] == 0x1A || (buffer[i32] > 0x80 && isValidCP932(std::string((char*)&buffer[i32]))))) {
                            debug << std::format("MaybeAbsoluteJump at {:#x}, target: {:#x}\n", offset - 4, i32);
                            command.str += std::format("[MaybeAbsoluteJump to {:#x}]", i32);
                        }
                        if (i32 + offset < buffer.size() && (buffer[i32 + offset] == 0x1B || buffer[i32 + offset] == 0x1A || (buffer[i32 + offset] > 0x80 && isValidCP932(std::string((char*)&buffer[i32 + offset]))))) {
                            debug << std::format("MaybeRelativeJump3 at {:#x}, target: {:#x}, rel: {:#x}\n", offset - 4, i32 + offset, i32);
                            command.str += std::format("[MaybeRelativeJump to {:#x}]", i32 + offset);
                        }
                        command.str += ", ";
                        break;
                    }
                    case 0x13: {
                        int i32 = read<int>(&buffer[offset]);
                        offset += 4;
                        command.str += std::format("ReadInt32_AsType4[{:#x}]", i32);
                        if (i32 != 0 && i32 < buffer.size() && (buffer[i32] == 0x1B || buffer[i32] == 0x1A || (buffer[i32] > 0x80 && isValidCP932(std::string((char*)&buffer[i32]))))) {
                            debug << std::format("MaybeAbsoluteJump at {:#x}, target: {:#x}\n", offset - 4, i32);
                            command.str += std::format("[MaybeAbsoluteJump to {:#x}]", i32);
                        }
                        if (i32 + offset < buffer.size() && (buffer[i32 + offset] == 0x1B || buffer[i32 + offset] == 0x1A || (buffer[i32 + offset] > 0x80 && isValidCP932(std::string((char*)&buffer[i32 + offset]))))) {
                            debug << std::format("MaybeRelativeJump4 at {:#x}, target: {:#x}, rel: {:#x}\n", offset - 4, i32 + offset, i32);
                            command.str += std::format("[MaybeRelativeJump to {:#x}]", i32 + offset);
                        }
                        command.str += ", ";
                        break;
                    }

                    case 0x80:
                    case 0x81:
                    case 0x82:
                    case 0x83:
                    case 0x84:
                    case 0x85:
                    case 0x86:
                    case 0x87:
                    case 0x88:
                    case 0x89:
                    case 0x8A:
                    case 0x8B:
                    case 0x8C:
                    case 0x8D:
                    case 0x8E:
                    case 0x8F:
                    case 0x90:
                    case 0x91:
                    case 0x92:
                    case 0x93:
                    case 0x94:
                    case 0x95:
                    case 0x96:
                    case 0x97:
                    case 0x98:
                    case 0x99:
                    case 0x9A:
                    case 0x9B:
                    case 0x9C:
                    case 0x9D:
                    case 0x9E:
                    case 0x9F:
                    case 0xA0:
                    case 0xA1:
                    case 0xA2:
                    case 0xA3:
                    case 0xA4:
                    case 0xA5:
                    case 0xA6:
                    case 0xA7:
                    case 0xA8:
                    case 0xA9:
                    case 0xAA:
                    case 0xAB:
                    case 0xAC:
                    case 0xAD:
                    case 0xAE:
                    case 0xAF:
                    case 0xB0:
                    case 0xB1:
                    case 0xB2:
                    case 0xB3:
                    case 0xB4:
                    case 0xB5:
                    case 0xB6:
                    case 0xB7:
                    case 0xB8:

                    case 0xC0:
                    case 0xC8:
                    case 0xC9:

                    case 0xD0:
                    case 0xD1:
                    case 0xD2:

                    case 0xD4:
                    case 0xD5:
                    case 0xD6:
                    case 0xD7:
                    case 0xD8:
                    case 0xD9:
                    case 0xDA:
                    case 0xDB:
                    case 0xDC:
                    case 0xDD:
                    case 0xDE: {
                        break;
                    }

                    default:
                        throw std::runtime_error(std::format("Unknown opCode: {:#x} at {:#x}", opCode, offset - 1));
                    }
                }
                command.str += "> | ";
            }
            commands.push_back(command);
        }
        else if(type >= 0x20) {
            Command command;
            command.addr = offset - 1;
            std::string text;
            text += type;
            while (offset < buffer.size() && buffer[offset] >= 0x20) {
                text += buffer[offset];
                offset += 1;
            }
            output << text << std::endl;
            command.str = "RawText: " + std::move(text);
            commands.push_back(command);
        }
        else {
            throw std::runtime_error(std::format("Unknown type: {:#x} at {:#x}", type, offset - 1));
        }
    }

    debug << "Command Address, Executor Type, Command Text\n";
    for (auto& command : commands) {
        if (command.str.empty()) {
            command.str = "Empty";
        }
        debug << std::format("{:#08x}, {:#04x}, {}\n", command.addr, command.executorType, command.str);
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
    std::vector<uint8_t> newBuffer;
    std::vector<std::string> translations;
    size_t translationIndex = 0;

    std::regex lb1(R"(\[r\])");
    std::regex lb2(R"(\[n\])");
    std::string line;
    while (std::getline(inputTxt, line)) {
        if (line.find("[Spec") == 0) {
            line = line.substr(7);
            line = std::regex_replace(line, lb1, "\r");
            line = std::regex_replace(line, lb2, "\n");
        }
        else if (line.find("[pre_unfinish]") == 0) {
            line = line.substr(14);
        }
        translations.push_back(line);
    }

    std::vector<uint32_t> jmps;
    std::vector<RelJmp> relJmps;
    std::vector<Sentence> sentences;

    uint32_t offset = 0;
    while (offset < buffer.size()) {
        uint8_t type = buffer[offset];
        offset += 1;
        newBuffer.push_back(type);
        if (type == 0x1A) {
            continue;
        }
        else if (type == 0x1B) {
            uint16_t executorType = read<uint16_t>(&buffer[offset]);
            offset += 2;
            newBuffer.insert(newBuffer.end(), (uint8_t*)&executorType, (uint8_t*)&executorType + 2);
            while (true) {
                uint8_t argId = buffer[offset];
                offset += 1;
                newBuffer.push_back(argId);
                if (argId == 0xFF) {
                    break;
                }
                while (true) {
                    uint8_t opCode = buffer[offset];
                    offset += 1;
                    newBuffer.push_back(opCode);
                    if (opCode == 0xFF) {
                        break;
                    }
                    switch (opCode) {
                    case 0x01: {
                        int i32 = read<int>(&buffer[offset]);
                        offset += 4;
                        if (
                            i32 != 0 && i32 < buffer.size() &&
                            (buffer[i32] == 0x1B || buffer[i32] == 0x1A || (buffer[i32] > 0x80 && isValidCP932(std::string((char*)&buffer[i32]))))
                            ) {
                            jmps.push_back(newBuffer.size());
                        }
                        newBuffer.insert(newBuffer.end(), (uint8_t*)&i32, (uint8_t*)&i32 + 4);
                        break;
                    }
                    case 0x02: {
                        short i16 = read<short>(&buffer[offset]);
                        offset += 2;
                        newBuffer.insert(newBuffer.end(), (uint8_t*)&i16, (uint8_t*)&i16 + 2);
                        break;
                    }
                    case 0x03: {
                        char i8 = read<char>(&buffer[offset]);
                        offset += 1;
                        newBuffer.push_back(i8);
                        break;
                    }
                    case 0x04: {
                        uint32_t length = read<uint32_t>(&buffer[offset]);
                        offset += 4;
                        if (translationIndex >= translations.size()) {
                            throw std::runtime_error("Not enough translations.");
                        }
                        Sentence se;
                        se.addr = offset;
                        std::string newText = translations[translationIndex++];
                        uint32_t newLength = newText.size();
                        se.offset = newLength - length;
                        newBuffer.insert(newBuffer.end(), (uint8_t*)&newLength, (uint8_t*)&newLength + 4);
                        std::string str((char*)&buffer[offset], length);
                        offset += length;
                        newBuffer.insert(newBuffer.end(), newText.begin(), newText.end());
                        sentences.push_back(se);
                        break;
                    }
                    case 0x05: {
                        double f64 = read<double>(&buffer[offset]);
                        offset += 8;
                        newBuffer.insert(newBuffer.end(), (uint8_t*)&f64, (uint8_t*)&f64 + 8);
                        break;
                    }

                    case 0x10:
                    case 0x11:
                    case 0x12:
                    case 0x13: {
                        int i32 = read<int>(&buffer[offset]);
                        offset += 4;
                        if (i32 != 0 && i32 < buffer.size() && (buffer[i32] == 0x1B || buffer[i32] == 0x1A)) {

                        }
                        if (i32 + offset < buffer.size() && (buffer[i32 + offset] == 0x1B || buffer[i32 + offset] == 0x1A)) {
                            /*RelJmp relJmp;
                            relJmp.addrInBuffer = offset - 4;
                            relJmp.addrInNewBuffer = newBuffer.size();
                            relJmps.push_back(relJmp);*/
                        }
                        newBuffer.insert(newBuffer.end(), (uint8_t*)&i32, (uint8_t*)&i32 + 4);
                        break;
                    }

                    case 0x80:
                    case 0x81:
                    case 0x82:
                    case 0x83:
                    case 0x84:
                    case 0x85:
                    case 0x86:
                    case 0x87:
                    case 0x88:
                    case 0x89:
                    case 0x8A:
                    case 0x8B:
                    case 0x8C:
                    case 0x8D:
                    case 0x8E:
                    case 0x8F:
                    case 0x90:
                    case 0x91:
                    case 0x92:
                    case 0x93:
                    case 0x94:
                    case 0x95:
                    case 0x96:
                    case 0x97:
                    case 0x98:
                    case 0x99:
                    case 0x9A:
                    case 0x9B:
                    case 0x9C:
                    case 0x9D:
                    case 0x9E:
                    case 0x9F:
                    case 0xA0:
                    case 0xA1:
                    case 0xA2:
                    case 0xA3:
                    case 0xA4:
                    case 0xA5:
                    case 0xA6:
                    case 0xA7:
                    case 0xA8:
                    case 0xA9:
                    case 0xAA:
                    case 0xAB:
                    case 0xAC:
                    case 0xAD:
                    case 0xAE:
                    case 0xAF:
                    case 0xB0:
                    case 0xB1:
                    case 0xB2:
                    case 0xB3:
                    case 0xB4:
                    case 0xB5:
                    case 0xB6:
                    case 0xB7:
                    case 0xB8:

                    case 0xC0:
                    case 0xC8:
                    case 0xC9:

                    case 0xD0:
                    case 0xD1:
                    case 0xD2:

                    case 0xD4:
                    case 0xD5:
                    case 0xD6:
                    case 0xD7:
                    case 0xD8:
                    case 0xD9:
                    case 0xDA:
                    case 0xDB:
                    case 0xDC:
                    case 0xDD:
                    case 0xDE: {
                        break;
                    }
                    default:
                        throw std::runtime_error(std::format("Unknown opCode: {:#x} at {:#x}", opCode, offset - 1));
                    }
                }
            }
        }
        else if (type >= 0x20) {
            std::string text;
            newBuffer.pop_back();
            Sentence se;
            se.addr = offset - 1;
            text += type;
            while (offset < buffer.size() && buffer[offset] >= 0x20) {
                text += buffer[offset];
                offset += 1;
            }
            if (translationIndex >= translations.size()) {
                throw std::runtime_error("Not enough translations.");
            }
            std::string newText = translations[translationIndex++];
            uint32_t newLength = newText.size();
            se.offset = newLength - text.size();
            newBuffer.insert(newBuffer.end(), newText.begin(), newText.end());
            sentences.push_back(se);
        }
        else {
            throw std::runtime_error(std::format("Unknown type: {:#x} at {:#x}", type, offset - 1));
        }
    }

    if (translationIndex != translations.size()) {
        std::cout << "Warning: translations too much, diff: " << translations.size() - translationIndex << std::endl;
    }

    for (uint32_t i = 0; i < jmps.size(); i++) {
        uint32_t jmp = read<uint32_t>(&newBuffer[jmps[i]]);
        int offset = 0;
        for (uint32_t j = 0; j < sentences.size() && sentences[j].addr < jmp; j++) {
            offset += sentences[j].offset;
        }
        jmp += offset;
        write<uint32_t>(&newBuffer[jmps[i]], jmp);
    }

    for (uint32_t i = 0; i < relJmps.size(); i++) {
        uint32_t relJmp = read<uint32_t>(&newBuffer[relJmps[i].addrInNewBuffer]);
        int offset = 0;
        for (uint32_t j = 0; j < sentences.size() && sentences[j].addr < relJmps[i].addrInBuffer + 4 + relJmp; j++) {
            if (sentences[j].addr >= relJmps[i].addrInBuffer + 4) {
                offset += sentences[j].offset;
            }
        }
        relJmp += offset;
        write<uint32_t>(&newBuffer[relJmps[i].addrInNewBuffer], relJmp);
    }

    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();

    std::cout << "Write-back complete. Output saved to " << outputBinPath << std::endl;
}

void printUsage(fs::path programPath) {
    std::string programName = programPath.filename().string();
    std::cout << "Made by julixian 2025.08.24 For [Shiroi Hebi no Yoru(Version100000), Katakoi no Tsuki(Version102000), Katakoi no Tsuki Extra(Version104000)]" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  Dump:   " << programName << " dump <scriptCode> <output_txt>" << std::endl;
    std::cout << "  Inject: " << programName << " inject <org_scriptCode> <new_txt> <script_newCode>" << std::endl;
}

int main(int argc, char* argv[]) {

    try {
        if (argc < 2) {
            printUsage(argv[0]);
            return 1;
        }

        std::string mode = argv[1];

        if (mode == "dump") {
            if (argc < 4) {
                std::cerr << "Error: Incorrect number of arguments for dump mode." << std::endl;
                printUsage(argv[0]);
                return 1;
            }
            dumpText(argv[2], argv[3]);
        }
        else if (mode == "inject") {
            if (argc < 5) {
                std::cerr << "Error: Incorrect number of arguments for inject mode." << std::endl;
                printUsage(argv[0]);
                return 1;
            }
            injectText(argv[2], argv[3], argv[4]);
        }
        else {
            std::cerr << "Error: Invalid mode selected." << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
