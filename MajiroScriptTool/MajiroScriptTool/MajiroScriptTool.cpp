#define NOMINMAX
#include <Windows.h>
#include <cstdint>

import std;
import nlohmann.json;
using json = nlohmann::json;
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

// ============================================================================
// CRYPTO MODULE
// ============================================================================
class MajiroCrypto {
public:
    static void Process(std::vector<uint8_t>& data, uint32_t offset, uint32_t size) {
        static auto xorTable = GenerateXorTable();
        for (uint32_t i = 0; i < size; ++i) {
            data[offset + i] ^= xorTable[i % 1024];
        }
    }

private:
    static std::vector<uint8_t> GenerateXorTable() {
        std::vector<uint32_t> crcTable(256);
        uint32_t poly = 0xEDB88320;
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t v = i;
            for (int j = 0; j < 8; ++j) {
                if (v & 1) v = (v >> 1) ^ poly;
                else       v >>= 1;
            }
            crcTable[i] = v;
        }

        std::vector<uint8_t> xorTable(1024);
        for (size_t i = 0; i < 256; ++i) {
            uint32_t val = crcTable[i];
            // Little Endian packing
            xorTable[i * 4 + 0] = (val >> 0) & 0xFF;
            xorTable[i * 4 + 1] = (val >> 8) & 0xFF;
            xorTable[i * 4 + 2] = (val >> 16) & 0xFF;
            xorTable[i * 4 + 3] = (val >> 24) & 0xFF;
        }
        return xorTable;
    }
};

// ============================================================================
// MEMORY READER (Replaces std::ifstream)
// ============================================================================
class MemReader {
    const uint8_t* data;
    size_t size;
    size_t pos;
    size_t last_gcount;

public:
    MemReader(const std::vector<uint8_t>& buffer) : data(buffer.data()), size(buffer.size()), pos(0), last_gcount(0) {}

    void read(char* dest, size_t count) {
        if (pos + count > size) {
            last_gcount = size > pos ? size - pos : 0;
        }
        else {
            last_gcount = count;
        }
        if (last_gcount > 0) {
            memcpy(dest, data + pos, last_gcount);
            pos += last_gcount;
        }
    }

    void seekg(size_t newPos) {
        if (newPos > size) pos = size; // EOF
        else pos = newPos;
    }

    void seekg(int64_t offset, std::ios::seekdir dir) {
        if (dir == std::ios::beg) pos = offset;
        else if (dir == std::ios::cur) pos += offset;
        else if (dir == std::ios::end) pos = size + offset;

        if (pos > size) pos = size; // Clamp to EOF
    }

    size_t tellg() const { return pos; }
    size_t gcount() const { return last_gcount; }
    bool eof() const { return pos >= size; }
};

struct Sentence {
    uint32_t firstOpCodeAddr = 0;
    uint32_t totalCommandLength = 0;
    std::string name;
    std::string text;
    std::string type;
    //std::string codePage;
};

void outputSentence(json& jarray, Sentence& sentence) {
    json jsentence =
    {
            { "firstOpCodeAddr", sentence.firstOpCodeAddr },
            { "totalCommandLength", sentence.totalCommandLength },
            { "text", ascii2Ascii(sentence.text, 932) },
            { "type", sentence.type },
            { "codePage", 932 },
    };
    if (!sentence.name.empty()) {
        jsentence["name"] = ascii2Ascii(sentence.name, 932);
    }
    jarray.push_back(std::move(jsentence));
    sentence.name.clear();
    sentence.text.clear();
    sentence.type.clear();
    sentence.firstOpCodeAddr = 0;
    sentence.totalCommandLength = 0;
}

std::optional<std::string> isFurigana(MemReader& ifs) {
    auto currentPos = ifs.tellg();
    uint16_t nextOpCode;
    ifs.read(reinterpret_cast<char*>(&nextOpCode), 2);
    if (ifs.gcount() < 2 || nextOpCode != 0x801) {
        ifs.seekg(currentPos);
        return std::nullopt;
    }
    uint16_t textLength;
    ifs.read(reinterpret_cast<char*>(&textLength), 2);
    std::vector<uint8_t> textBytes(textLength);
    ifs.read(reinterpret_cast<char*>(textBytes.data()), textLength);

    if (textBytes.size() < 2 || textBytes.back() != 0) {
        throw std::runtime_error(std::format("Invalid text bytes when detecting furigana at {:#x}.", (size_t)currentPos));
    }

    uint16_t furiganaProcOpCode;
    ifs.read(reinterpret_cast<char*>(&furiganaProcOpCode), 2);
    if (ifs.gcount() < 2 || furiganaProcOpCode != 0x810) {
        ifs.seekg(currentPos);
        return std::nullopt;
    }

    std::array<uint8_t, 10> furiganaProcFunc;
    ifs.read(reinterpret_cast<char*>(furiganaProcFunc.data()), 10);
    if (read<uint32_t>(furiganaProcFunc.data()) != 0x3198FD01) {
        ifs.seekg(currentPos);
        return std::nullopt;
    }

    return std::string(textBytes.begin(), textBytes.end() - 1);
}

bool isHeartSymbol(MemReader& ifs) {
    auto currentPos = ifs.tellg();
    bool isHeart = true;

    for (int i = 0; i < 5; i++) {
        uint16_t nextOpCode;
        ifs.read(reinterpret_cast<char*>(&nextOpCode), 2);
        if (ifs.gcount() < 2 || nextOpCode != 0x800) {
            isHeart = false;
            break;
        }
        uint32_t arg;
        ifs.read(reinterpret_cast<char*>(&arg), 4);
        if (ifs.gcount() < 4 || arg != 0xFFFFFF9D) {
            isHeart = false;
            break;
        }
    }
    if (!isHeart) {
        ifs.seekg(currentPos);
        return false;
    }
    uint16_t nextOpCode;
    ifs.read(reinterpret_cast<char*>(&nextOpCode), 2);
    if (ifs.gcount() < 2 || nextOpCode != 0x842) {
        ifs.seekg(currentPos);
        return false;
    }
    uint16_t textLength;
    ifs.read(reinterpret_cast<char*>(&textLength), 2);
    std::vector<uint8_t> textBytes(textLength);
    ifs.read(reinterpret_cast<char*>(textBytes.data()), textLength);

    if (textLength != 2 || textBytes[0] != 0x67) {
        ifs.seekg(currentPos);
        return false;
    }
    return true;
}

bool isEmphasis(MemReader& ifs) {
    auto currentPos = ifs.tellg();
    uint16_t nextOpCode;
    ifs.read(reinterpret_cast<char*>(&nextOpCode), 2);
    if (ifs.gcount() < 2 || nextOpCode != 0x810) {
        ifs.seekg(currentPos);
        return false;
    }
    std::array<uint8_t, 10> emphasisProcFunc;
    ifs.read(reinterpret_cast<char*>(emphasisProcFunc.data()), 10);
    if (read<uint32_t>(emphasisProcFunc.data()) != 0x2F93F26A) {
        ifs.seekg(currentPos);
        return false;
    }
    return true;
}

bool isTip(MemReader& ifs) {
    auto currentPos = ifs.tellg();
    uint16_t nextOpCode;
    ifs.read(reinterpret_cast<char*>(&nextOpCode), 2);
    if (ifs.gcount() < 2 || nextOpCode != 0x810) {
        ifs.seekg(currentPos);
        return false;
    }
    std::array<uint8_t, 10> tipProcFunc;
    ifs.read(reinterpret_cast<char*>(tipProcFunc.data()), 10);
    if (read<uint32_t>(tipProcFunc.data()) != 0x38723956) {
        ifs.seekg(currentPos);
        return false;
    }
    return true;
}

void verifySentence(json& jarray, Sentence& sentence, uint32_t opCodeAddr) {
    if (sentence.firstOpCodeAddr != 0) {
        sentence.totalCommandLength = opCodeAddr - sentence.firstOpCodeAddr;
        outputSentence(jarray, sentence);
    }
}

//DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
void dumpText(const fs::path& inputPath, const fs::path& outputPath) {
    std::ifstream inFile(inputPath, std::ios::binary);
    std::ofstream ofs(outputPath);

    if (!inFile || !ofs) {
        throw std::runtime_error("Error opening files: " + wide2Ascii(inputPath) + " or " + wide2Ascii(outputPath));
    }

    size_t fileSize = (size_t)fs::file_size(inputPath);
    std::vector<uint8_t> buffer(fileSize);
    inFile.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    char signature[16] = { 0 };
    memcpy(signature, buffer.data(), 16);
    std::string_view sigView(signature, signature[15] == '\0' ? 15 : 16);

    bool isEncrypted = (sigView == "MajiroObjX1.000");
    if (sigView != "MjPlainBytecode" && sigView != "MajiroObjV1.000" && !isEncrypted) {
        throw std::runtime_error("Invalid file signature: " + std::string(signature));
    }

    uint32_t functionCount = read<uint32_t>(buffer.data() + 0x18);
    uint32_t codeBaseOffset = 0x1C + functionCount * 8; // Header(28) + FuncTable(8*N)
    uint32_t dataSize = read<uint32_t>(buffer.data() + codeBaseOffset);
    uint32_t codeStart = codeBaseOffset + 4;

    if (codeStart + dataSize != fileSize) {
        throw std::runtime_error("Invalid data size in header");
    }

    // Decrypt in place
    if (isEncrypted) {
        //std::println("Detected encrypted file, decrypting in memory...");
        MajiroCrypto::Process(buffer, codeStart, dataSize);
    }
    MemReader ifs(buffer);

    json jarray = json::array();
    Sentence currentSentence;

    ifs.seekg(codeStart);
    while (ifs.tellg() < fileSize) {
        uint32_t opCodeAddr = (uint32_t)ifs.tellg();
        uint16_t opCode;
        ifs.read(reinterpret_cast<char*>(&opCode), 2);
        //std::println("OpCode: {:#x} at {:#x}", opCode, opCodeAddr);

        if (opCode >= 0x100 && opCode <= 0x1A9) {
            continue;
        }
        if (opCode >= 0x1AA && opCode <= 0x320) {
            ifs.seekg(8, std::ios::cur);
            continue;
        }

        switch (opCode)
        {
        case 0x80F:
        case 0x810:
        {
            verifySentence(jarray, currentSentence, opCodeAddr);
            ifs.seekg(10, std::ios::cur);
        }
        break;

        case 0x802:
        case 0x837:
        {
            verifySentence(jarray, currentSentence, opCodeAddr);
            ifs.seekg(8, std::ios::cur);
        }
        break;

        case 0x834:
        case 0x835:
        {
            verifySentence(jarray, currentSentence, opCodeAddr);
            ifs.seekg(6, std::ios::cur);
        }
        break;

        case 0x800:
        {
            ifs.seekg(4, std::ios::cur);
            if (isHeartSymbol(ifs)) {
                if (currentSentence.firstOpCodeAddr == 0) {
                    currentSentence.firstOpCodeAddr = opCodeAddr;
                    currentSentence.type = "text";
                }
                currentSentence.text += "[heart]";
            }
            else {
                verifySentence(jarray, currentSentence, opCodeAddr);
            }
        }
        break;

        case 0x803:

        case 0x82C:
        case 0x82D:
        case 0x82E:
        case 0x830:
        case 0x831:
        case 0x832:
        case 0x833:
        case 0x838:
        case 0x839:
        case 0x83B:
        case 0x83C:
        case 0x83D:

        case 0x843:
        case 0x845:
        case 0x847:
        {
            verifySentence(jarray, currentSentence, opCodeAddr);
            ifs.seekg(4, std::ios::cur);
        }
        break;

        case 0x83A:
        {
            ifs.seekg(2, std::ios::cur);
        }
        break;

        case 0x82B:
        case 0x82F:
        case 0x83E:
        case 0x83F:
        case 0x844:
        case 0x846:
            verifySentence(jarray, currentSentence, opCodeAddr);
        case 0x841:
            break;

        case 0x829:
        case 0x836:
        {
            verifySentence(jarray, currentSentence, opCodeAddr);
            uint16_t length;
            ifs.read(reinterpret_cast<char*>(&length), 2);
            ifs.seekg(length, std::ios::cur);
        }
        break;

        case 0x801:
        {
            uint16_t length;
            ifs.read(reinterpret_cast<char*>(&length), 2);
            std::vector<uint8_t> textBytes(length);
            ifs.read(reinterpret_cast<char*>(textBytes.data()), length);
            if (textBytes.size() < 2 || textBytes.back() != 0) {
                break;
            }
            std::string text(textBytes.begin(), textBytes.end() - 1);
            if (std::optional<std::string> furiganaOpt = isFurigana(ifs); furiganaOpt.has_value()) {
                text = "[" + text + "/" + furiganaOpt.value() + "]";
                if (currentSentence.firstOpCodeAddr == 0) {
                    currentSentence.firstOpCodeAddr = opCodeAddr;
                    currentSentence.type = "text";
                }
                currentSentence.text += text;
            }
            else if (isEmphasis(ifs)) {
                text = "[" + wide2Ascii(L"・", 932) + "/" + text + "]";
                if (currentSentence.firstOpCodeAddr == 0) {
                    currentSentence.firstOpCodeAddr = opCodeAddr;
                    currentSentence.type = "text";
                }
                currentSentence.text += text;
            }
            else if (isTip(ifs)) {
                text = "{" + text + "}";
                if (currentSentence.firstOpCodeAddr == 0) {
                    currentSentence.firstOpCodeAddr = opCodeAddr;
                    currentSentence.type = "text";
                }
                currentSentence.text += text;
            }
            else {
                verifySentence(jarray, currentSentence, opCodeAddr);
                currentSentence.firstOpCodeAddr = opCodeAddr;
                currentSentence.type = "ldstr";
                currentSentence.text = std::move(text);
                currentSentence.totalCommandLength = (uint32_t)ifs.tellg() - currentSentence.firstOpCodeAddr;
                outputSentence(jarray, currentSentence);
            }
        }
        break;

        case 0x840:
        {
            uint16_t length;
            ifs.read(reinterpret_cast<char*>(&length), 2);
            std::vector<uint8_t> textBytes(length);
            ifs.read(reinterpret_cast<char*>(textBytes.data()), length);
            if (textBytes.size() < 2 || textBytes.back() != 0) {
                throw std::runtime_error(std::format("Invalid text bytes in op 0x840 at {:#x}.", opCodeAddr));
            }
            std::string text(textBytes.begin(), textBytes.end() - 1);
            if (currentSentence.firstOpCodeAddr == 0) {
                currentSentence.firstOpCodeAddr = opCodeAddr;
                currentSentence.type = "text";
            }
            else if (text.starts_with("\x81\x75") && currentSentence.name.empty()) {
                if (currentSentence.text.empty()) {
                    throw std::runtime_error(std::format("Name not found at {:#x}.", opCodeAddr));
                }
                currentSentence.name = std::move(currentSentence.text);
                currentSentence.text.clear();
            }
            currentSentence.text += text;
        }
        break;

        case 0x842:
        {
            uint16_t commandLength;
            ifs.read(reinterpret_cast<char*>(&commandLength), 2);
            std::vector<uint8_t> commandBytes(commandLength);
            ifs.read(reinterpret_cast<char*>(commandBytes.data()), commandLength);
            if (commandBytes.size() < 2 || commandBytes.back() != 0) {
                throw std::runtime_error(std::format("Invalid text bytes in op 0x842 at {:#x}.", opCodeAddr));
            }
            std::string_view command(reinterpret_cast<const char*>(commandBytes.data()), commandBytes.size() - 1);
            if (command == "p" || command == "r" || command == "\\") {
                if (currentSentence.firstOpCodeAddr != 0) {
                    currentSentence.totalCommandLength = opCodeAddr - currentSentence.firstOpCodeAddr;
                    outputSentence(jarray, currentSentence);
                }
            }
            else if (command == "n") {
                if (currentSentence.firstOpCodeAddr == 0) {
                    currentSentence.firstOpCodeAddr = opCodeAddr;
                    currentSentence.type = "text";
                }
                currentSentence.text += "[n]";
            }
            else {
                if (currentSentence.firstOpCodeAddr != 0) {
                    throw std::runtime_error(std::format("Previous sentence not ended at {:#x}.", opCodeAddr));
                }
                //verifySentence(jarray, currentSentence, opCodeAddr);
            }
        }
        break;

        case 0x850:
        {
            uint16_t jumpCount;
            ifs.read(reinterpret_cast<char*>(&jumpCount), 2);
            ifs.seekg(jumpCount * 4, std::ios::cur);
        }
        break;

        default:
            throw std::runtime_error(std::format("Unknown op code: {:#x} at {:#x}.", opCode, opCodeAddr));

        }
    }

    inFile.close();
    ofs << jarray.dump(2);
    ofs.close();
}

struct Jump {
    uint32_t jumpOffsetAddr = 0;
    uint32_t startAddr = 0;
    int32_t offset = 0;
};

std::vector<std::string> splitText(const std::string& text, UINT codePage) {
    std::vector<std::string> parts;

    std::string currentPart;
    for (size_t i = 0; i < text.size(); i++) {
        if (std::string textPart = text.substr(i); textPart.starts_with("[n]")) {
            if (!currentPart.empty()) {
                parts.push_back(currentPart);
                currentPart.clear();
            }
            parts.push_back("[n]");
            i += 2;
        }
        else if (textPart.starts_with("[heart]")) {
            if (!currentPart.empty()) {
                parts.push_back(currentPart);
                currentPart.clear();
            }
            parts.push_back("[heart]");
            i += 6;
        }
        else if (text[i] == '[' || text[i] == '{') {
            if (!currentPart.empty()) {
                parts.push_back(currentPart);
                currentPart.clear();
            }
            size_t endPos = text.find(text[i] == '[' ? "]" : "}", i + 1);
            if (endPos == std::string::npos) {
                throw std::runtime_error(std::format("Invalid text : [{}], there is no matching result for [ or {{ at position {}.", text, i));
            }
            parts.push_back(text.substr(i, endPos - i + 1));
            i = endPos;
        }
        else {
            currentPart.push_back(text[i]);
        }
    }
    if (!currentPart.empty()) {
        parts.push_back(currentPart);
    }
    for (auto& part : parts) {
        part = ascii2Ascii(part, 65001, codePage);
    }
    return parts;
}

std::vector<uint8_t> processNormalText(const std::string& name, const std::string& text, UINT codePage) {
    std::vector<uint8_t> result;

    if (!name.empty()) {
        std::string nameAscii = ascii2Ascii(name, 65001, codePage);
        uint16_t opCode = 0x840;
        uint16_t nameLength = (uint16_t)nameAscii.size() + 1;
        result.insert(result.end(), reinterpret_cast<const uint8_t*>(&opCode), reinterpret_cast<const uint8_t*>(&opCode + 1));
        result.insert(result.end(), reinterpret_cast<const uint8_t*>(&nameLength), reinterpret_cast<const uint8_t*>(&nameLength + 1));
        result.insert(result.end(), reinterpret_cast<const uint8_t*>(nameAscii.c_str()), reinterpret_cast<const uint8_t*>(nameAscii.c_str() + nameAscii.size() + 1));
    }

    std::vector<std::string> parts = splitText(text, codePage);

    for (const std::string& part : parts) {
        if (parts.empty()) {
            throw std::runtime_error("Internal error: Invalid text.");
        }
        if (part == "[n]") {
            uint8_t commandBytes[] = { 0x42, 0x08, 0x02, 0x00, 0x6E, 0x00 };
            result.insert(result.end(), commandBytes, commandBytes + sizeof(commandBytes));
        }
        else if (part == "[heart]") {
            uint8_t commandBytes[] = { 
                0x00,0x08,0x00,0x00,0x00,0x00,
                0x00,0x08,0x9D,0xFF,0xFF,0xFF,
                0x00,0x08,0x9D,0xFF,0xFF,0xFF,
                0x00,0x08,0x9D,0xFF,0xFF,0xFF,
                0x00,0x08,0x9D,0xFF,0xFF,0xFF,
                0x00,0x08,0x9D,0xFF,0xFF,0xFF,
                0x42,0x08,0x02,0x00,0x67,0x00 };
            result.insert(result.end(), commandBytes, commandBytes + sizeof(commandBytes));
        }
        else if (part.front() == '[' && part.back() == ']') {
            size_t slashPos = part.find('/');
            if (slashPos == std::string::npos) {
                throw std::runtime_error(std::format("Invalid furigana: [{}], there is no / in it.", part));
            }
            std::string furigana = part.substr(1, slashPos - 1);
            std::string text = part.substr(slashPos + 1, part.size() - slashPos - 2);
            if (furigana.empty() || text.empty()) {
                throw std::runtime_error(std::format("Invalid furigana: [{}], furigana or text is empty.", part));
            }
            uint16_t opCode = 0x801;
            uint16_t furiganaLength = (uint16_t)furigana.size() + 1;
            uint16_t textLength = (uint16_t)text.size() + 1;
            result.insert(result.end(), reinterpret_cast<const uint8_t*>(&opCode), reinterpret_cast<const uint8_t*>(&opCode + 1));
            result.insert(result.end(), reinterpret_cast<const uint8_t*>(&furiganaLength), reinterpret_cast<const uint8_t*>(&furiganaLength + 1));
            result.insert(result.end(), reinterpret_cast<const uint8_t*>(furigana.c_str()), reinterpret_cast<const uint8_t*>(furigana.c_str() + furigana.size() + 1));
            result.insert(result.end(), reinterpret_cast<const uint8_t*>(&opCode), reinterpret_cast<const uint8_t*>(&opCode + 1));
            result.insert(result.end(), reinterpret_cast<const uint8_t*>(&textLength), reinterpret_cast<const uint8_t*>(&textLength + 1));
            result.insert(result.end(), reinterpret_cast<const uint8_t*>(text.c_str()), reinterpret_cast<const uint8_t*>(text.c_str() + text.size() + 1));
            uint8_t commandBytes[] = { 0x10,0x08,0x01,0xFD,0x98,0x31,0x00,0x00,0x00,0x00,0x02,0x00 };
            result.insert(result.end(), commandBytes, commandBytes + sizeof(commandBytes));
        }
        else if (part.front() == '{' && part.back() == '}') {
            std::string text = part.substr(1, part.size() - 2);
            if (text.empty()) {
                throw std::runtime_error(std::format("Invalid tip: [{}], text is empty.", part));
            }
            uint16_t opCode = 0x801;
            uint16_t textLength = (uint16_t)text.size() + 1;
            result.insert(result.end(), reinterpret_cast<const uint8_t*>(&opCode), reinterpret_cast<const uint8_t*>(&opCode + 1));
            result.insert(result.end(), reinterpret_cast<const uint8_t*>(&textLength), reinterpret_cast<const uint8_t*>(&textLength + 1));
            result.insert(result.end(), reinterpret_cast<const uint8_t*>(text.c_str()), reinterpret_cast<const uint8_t*>(text.c_str() + text.size() + 1));
            uint8_t commandBytes[] = { 0x10,0x08,0x56,0x39,0x72,0x38,0x00,0x00,0x00,0x00,0x01,0x00 };
            result.insert(result.end(), commandBytes, commandBytes + sizeof(commandBytes));
        }
        else {
            uint16_t opCode = 0x840;
            uint16_t length = (uint16_t)part.size() + 1;
            result.insert(result.end(), reinterpret_cast<const uint8_t*>(&opCode), reinterpret_cast<const uint8_t*>(&opCode + 1));
            result.insert(result.end(), reinterpret_cast<const uint8_t*>(&length), reinterpret_cast<const uint8_t*>(&length + 1));
            result.insert(result.end(), reinterpret_cast<const uint8_t*>(part.c_str()), reinterpret_cast<const uint8_t*>(part.c_str() + part.size() + 1));
            uint16_t procOpCode = 0x841;
            result.insert(result.end(), reinterpret_cast<const uint8_t*>(&procOpCode), reinterpret_cast<const uint8_t*>(&procOpCode + 1));
        }
    }

    return result;
}

//IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII
void injectText(const fs::path& inputBinPath, const fs::path& inputTxtPath, const fs::path& outputBinPath) {
    std::ifstream inputBin(inputBinPath, std::ios::binary);
    std::ifstream inputTxt(inputTxtPath);
    std::ofstream outputBin(outputBinPath, std::ios::binary);

    if (!inputBin || !inputTxt || !outputBin) {
        throw std::runtime_error("Error opening files: " + wide2Ascii(inputBinPath) + " or " + wide2Ascii(inputTxtPath) + " or " + wide2Ascii(outputBinPath));
    }

    size_t fileSize = (size_t)fs::file_size(inputBinPath);
    std::vector<uint8_t> buffer(fileSize);
    inputBin.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    char signature[16] = { 0 };
    memcpy(signature, buffer.data(), 16);
    std::string_view sigView(signature, signature[15] == '\0' ? 15 : 16);

    bool isEncrypted = (sigView == "MajiroObjX1.000");
    if (sigView != "MjPlainBytecode" && sigView != "MajiroObjV1.000" && !isEncrypted) {
        throw std::runtime_error("Invalid file signature: " + std::string(signature));
    }

    // Parse Header
    int32_t mainFuncEntryOffset = read<int32_t>(buffer.data() + 0x10);
    uint32_t functionCount = read<uint32_t>(buffer.data() + 0x18);
    uint32_t codeBaseOffset = 0x1C + functionCount * 8;
    uint32_t dataSize = read<uint32_t>(buffer.data() + codeBaseOffset);
    uint32_t codeStart = codeBaseOffset + 4;

    if (codeStart + dataSize != fileSize) {
        throw std::runtime_error("Invalid data size in header");
    }

    // Decrypt in place
    if (isEncrypted) {
        //std::println("Detected encrypted file, decrypting in memory for analysis...");
        MajiroCrypto::Process(buffer, codeStart, dataSize);
    }

    std::vector<Jump> jumps;
    jumps.push_back({ 0x10, codeStart, mainFuncEntryOffset }); // Main entry
    for (uint32_t i = 0; i < functionCount; i++) {
        int32_t functionOffset = read<int32_t>(buffer.data() + 0x1C + 8 * i + 4);
        jumps.push_back({ 0x1C + 8 * i + 4, codeStart, functionOffset });
    }

    MemReader ifs(buffer);
    ifs.seekg(codeStart);
    while (ifs.tellg() < fileSize) {
        uint32_t opCodeAddr = (uint32_t)ifs.tellg();
        uint16_t opCode;
        ifs.read(reinterpret_cast<char*>(&opCode), 2);
        //std::println("OpCode: {:#x} at {:#x}", opCode, opCodeAddr);

        if (opCode >= 0x100 && opCode <= 0x1A9) {
            continue;
        }
        if (opCode >= 0x1AA && opCode <= 0x320) {
            ifs.seekg(8, std::ios::cur);
            continue;
        }

        switch (opCode)
        {
        case 0x80F:
        case 0x810:
        {
            ifs.seekg(10, std::ios::cur);
        }
        break;

        case 0x802:
        case 0x837:
        {
            ifs.seekg(8, std::ios::cur);
        }
        break;

        case 0x834:
        case 0x835:
        {
            ifs.seekg(6, std::ios::cur);
        }
        break;

        case 0x800:
        case 0x803:
        {
            ifs.seekg(4, std::ios::cur);
        }
        break;

        case 0x83A:
        {
            ifs.seekg(2, std::ios::cur);
        }
        break;

        case 0x82B:
        case 0x82F:
        case 0x83E:
        case 0x83F:
        case 0x844:
        case 0x846:
        case 0x841:
            break;

        case 0x801:
        case 0x829:
        case 0x836:
        case 0x840:
        case 0x842:
        {
            uint16_t length;
            ifs.read(reinterpret_cast<char*>(&length), 2);
            ifs.seekg(length, std::ios::cur);
        }
        break;

        case 0x82C:
        case 0x82D:
        case 0x82E:
        case 0x830:
        case 0x831:
        case 0x832:
        case 0x833:
        case 0x838:
        case 0x839:

        case 0x83B:
        case 0x83C:
        case 0x83D:

        case 0x843:
        case 0x845:
        case 0x847:
        {
            Jump jump;
            jump.jumpOffsetAddr = opCodeAddr + 2;
            jump.startAddr = jump.jumpOffsetAddr + 4;
            ifs.read(reinterpret_cast<char*>(&jump.offset), 4);
            jumps.push_back(jump);
        }
        break;

        case 0x850:
        {
            uint16_t jumpCount;
            ifs.read(reinterpret_cast<char*>(&jumpCount), 2);
            for (uint16_t j = 0; j < jumpCount; j++) {
                Jump jump;
                jump.jumpOffsetAddr = opCodeAddr + 2 + 2 + 4 * j;
                jump.startAddr = jump.jumpOffsetAddr + 4;
                ifs.read(reinterpret_cast<char*>(&jump.offset), 4);
                jumps.push_back(jump);
            }
        }
        break;

        default:
            throw std::runtime_error(std::format("Unknown op code: {:#x} at {:#x}.", opCode, opCodeAddr));
        }
    }

    json jarray = json::parse(inputTxt);
    std::vector<uint8_t> newBuffer;

    uint32_t currentPos = 0;
    for (json& jsentence : jarray) {
        uint32_t firstOpCodeAddr = jsentence.value("firstOpCodeAddr", 0);
        uint32_t totalCommandLength = jsentence.value("totalCommandLength", 0);
        std::string type = jsentence.value("type", "");
        std::string name = jsentence.value("name", "");
        std::string text = jsentence.value("text", "");
        int codePage = jsentence.value("codePage", 0);

        newBuffer.insert(newBuffer.end(), buffer.begin() + currentPos, buffer.begin() + firstOpCodeAddr);
        currentPos = firstOpCodeAddr + totalCommandLength;
        std::vector<uint8_t> newCommandBytes;

        if (type == "ldstr") {
            text = ascii2Ascii(text, 65001, codePage);
            uint16_t opCode = 0x801;
            uint16_t length = (uint16_t)text.size() + 1;
            newCommandBytes.insert(newCommandBytes.end(), reinterpret_cast<const uint8_t*>(&opCode), reinterpret_cast<const uint8_t*>(&opCode + 1));
            newCommandBytes.insert(newCommandBytes.end(), reinterpret_cast<const uint8_t*>(&length), reinterpret_cast<const uint8_t*>(&length + 1));
            newCommandBytes.insert(newCommandBytes.end(), reinterpret_cast<const uint8_t*>(text.c_str()), reinterpret_cast<const uint8_t*>(text.c_str() + text.size() + 1));
        }
        else if (type == "text") {
            newCommandBytes = processNormalText(name, text, (UINT)codePage);
        }
        else {
            throw std::runtime_error(std::format("Unknown sentence type: {}", type));
        }

        newBuffer.insert(newBuffer.end(), newCommandBytes.begin(), newCommandBytes.end());
        jsentence["offset"] = (int)newCommandBytes.size() - (int)totalCommandLength;
    }

    if (currentPos < buffer.size()) {
        newBuffer.insert(newBuffer.end(), buffer.begin() + currentPos, buffer.end());
    }

    for (Jump& jump : jumps) {
        uint32_t jumpOffsetAddr = jump.jumpOffsetAddr;
        for (auto& jsentence : jarray) {
            uint32_t firstOpCodeAddr = jsentence.value("firstOpCodeAddr", 0);
            if (firstOpCodeAddr > jump.jumpOffsetAddr) {
                break;
            }
            int offset = jsentence.value("offset", 0);
            jumpOffsetAddr += offset;
        }
        jump.jumpOffsetAddr = jumpOffsetAddr;

        uint32_t startAddr = jump.startAddr;
        uint32_t endAddr = jump.startAddr + jump.offset;
        uint32_t trueStartAddr = std::min(startAddr, endAddr);
        uint32_t trueEndAddr = std::max(startAddr, endAddr);
        int32_t positiveOffset = (int32_t)(trueEndAddr - trueStartAddr);
        for (auto& jsentence : jarray) {
            uint32_t firstOpCodeAddr = jsentence.value("firstOpCodeAddr", 0);
            if (firstOpCodeAddr < trueStartAddr) {
                continue;
            }
            if (firstOpCodeAddr > trueEndAddr) {
                break;
            }
            int offset = jsentence.value("offset", 0);
            positiveOffset += offset;
        }
        jump.offset = jump.offset >= 0 ? positiveOffset : -positiveOffset;
        write<int32_t>(newBuffer.data() + jump.jumpOffsetAddr, jump.offset);
    }

    uint32_t newCodeDataSize = (uint32_t)newBuffer.size() - codeStart;
    write<uint32_t>(newBuffer.data() + codeBaseOffset, newCodeDataSize);

    if (isEncrypted) {
        //std::println("Re-encrypting data...");
        MajiroCrypto::Process(newBuffer, codeStart, newCodeDataSize);
        // Restore Signature
        const char encSig[] = "MajiroObjX1.000";
        memcpy(newBuffer.data(), encSig, 16);
    }

    outputBin.write(reinterpret_cast<const char*>(newBuffer.data()), newBuffer.size());

    inputBin.close();
    inputTxt.close();
    outputBin.close();
}


void printUsage(const fs::path& programPath) {
    std::print("Made by julixian 2025.12.03\n"
        "Usage: \n"
        "  Dump: {0} dump <input_folder> <output_folder> \n"
        "  Inject: {0} inject <input_orig-bin_folder> <input_translated-json_folder> <output_folder>",
        wide2Ascii(programPath.filename()));
}

int wmain(int argc, wchar_t* argv[])
{
    SetConsoleOutputCP(CP_UTF8);

    try {
        if (argc < 2) {
            printUsage(argv[0]);
            return 1;
        }

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
                    const fs::path outputPath = outputFolder / fs::relative(inputPath, inputFolder).replace_extension(".json");
                    if (outputPath.has_parent_path() && !fs::exists(outputPath.parent_path())) {
                        fs::create_directories(outputPath.parent_path());
                    }
                    std::println("Processing: {}", wide2Ascii(inputPath));
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
                    const fs::path inputTxtPath = inputTxtFolder / fs::relative(inputBinPath, inputBinFolder).replace_extension(".json");
                    if (!fs::exists(inputTxtPath)) {
                        std::println("Warning: {} not found, skipped.", wide2Ascii(inputTxtPath));
                        continue;
                    }
                    const fs::path outputBinPath = outputFolder / fs::relative(inputBinPath, inputBinFolder);
                    if (outputBinPath.has_parent_path() && !fs::exists(outputBinPath.parent_path())) {
                        fs::create_directories(outputBinPath.parent_path());
                    }
                    std::println("Processing: {}", wide2Ascii(inputBinPath));
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
