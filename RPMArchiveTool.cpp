#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <filesystem>
#include <windows.h>

namespace fs = std::filesystem;

std::string decode_string;
int entry_size;

struct ArcEntry {
    char name[32];
    uint32_t uncomprLen;
    uint32_t comprLen;
    uint32_t offset;
};

int get_code(const uint8_t* buf, uint32_t index_entries, bool hasFlag) {
    uint32_t entry_sizes[] = { 44, 36, 28, 0 };

    for (int j = 0; entry_sizes[j]; ++j) {
        uint32_t offset = (hasFlag ? 8 : 4) + index_entries * entry_sizes[j];
        const uint8_t* crypt = &buf[entry_sizes[j] - 4];
        uint8_t code[4], r_code[4];
        uint8_t plain[4];
        memcpy(plain, &offset, 4);

        for (int i = 0; i < 4; ++i) {
            code[i] = plain[i] - crypt[i];
            r_code[i] = 0 - code[i];
        }

        uint32_t name_len = entry_sizes[j] - 12;
        uint32_t pos[2];
        int find = 0;
        for (int i = name_len - 4; i >= 0; --i) {
            if (memcmp(buf + i, r_code, 4) == 0) {
                pos[find++] = i;
                if (find == 2)
                    break;
            }
        }
        if (find != 2)
            continue;

        int code_len = pos[0] - pos[1];
        decode_string.resize(code_len);
        for (int i = 0; i < code_len; ++i)
            decode_string[(pos[1] + i) % code_len] = 0 - buf[pos[1] + i];

        entry_size = entry_sizes[j];
        return 0;
    }
    return -1;
}

void raw_guess_key(const std::string& arc_path, bool hasFlag) {
    std::ifstream arc_file(arc_path, std::ios::binary);
    if (!arc_file) {
        std::cerr << "Failed to open " << arc_path << std::endl;
        return;
    }

    uint32_t num_entries;
    arc_file.read(reinterpret_cast<char*>(&num_entries), 4);

    if (hasFlag) {
        arc_file.seekg(8);
    }

    std::vector<uint8_t> index_buffer(num_entries * sizeof(ArcEntry));
    arc_file.read(reinterpret_cast<char*>(index_buffer.data()), index_buffer.size());

    if (arc_path.find("instdata.arc") != std::string::npos) {
        decode_string = "inst";
        return;
    }
    else if (arc_path.find("system.arc") != std::string::npos) {
        decode_string = "while";
        return;
    }
    else if (get_code(index_buffer.data(), num_entries, hasFlag) != 0) {
        std::cerr << "Failed to guess the code" << std::endl;
        return;
    }

    //if (filename.find(".lib") != std::string::npos) {
    //    const char* key = "偰偡偲";  // てすと
    //    for (size_t i = 0, k = 0; i < file_data.size(); i++) {
    //        file_data[i] -= key[k++];
    //        if (k >= strlen(key))
    //            k = 0;
    //    }
    //}
}

class LzssDecompressor {
public:
    LzssDecompressor(int frameSize = 0x1000, uint8_t frameFill = 0, int frameInitPos = 0xFEE)
        : m_frameSize(frameSize), m_frameFill(frameFill), m_frameInitPos(frameInitPos) {
    }

    std::vector<uint8_t> decompress(const std::vector<uint8_t>& input) {
        std::vector<uint8_t> output;
        std::vector<uint8_t> frame(m_frameSize, m_frameFill);
        int framePos = m_frameInitPos;
        int frameMask = m_frameSize - 1;

        size_t inputPos = 0;
        while (inputPos < input.size()) {
            uint8_t ctrl = input[inputPos++];
            for (int bit = 1; bit != 0x100 && inputPos < input.size(); bit <<= 1) {
                if (ctrl & bit) {
                    uint8_t b = input[inputPos++];
                    frame[framePos++ & frameMask] = b;
                    output.push_back(b);
                }
                else {
                    if (inputPos + 1 >= input.size()) break;
                    uint8_t lo = input[inputPos++];
                    uint8_t hi = input[inputPos++];
                    int offset = ((hi & 0xf0) << 4) | lo;
                    int count = 3 + (hi & 0xF);

                    for (int i = 0; i < count; ++i) {
                        uint8_t v = frame[offset++ & frameMask];
                        frame[framePos++ & frameMask] = v;
                        output.push_back(v);
                    }
                }
            }
        }

        return output;
    }

private:
    int m_frameSize;
    uint8_t m_frameFill;
    int m_frameInitPos;
};

std::string WideToAscii(const std::wstring& wide, UINT CodePage) {
    int len = WideCharToMultiByte(CodePage, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return "";
    std::string ascii(len, '\0');
    WideCharToMultiByte(CodePage, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
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

std::vector<uint8_t> compress(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> output;
    for (size_t i = 0; i < input.size(); i += 8) {
        output.push_back(0xFF);
        for (size_t j = 0; j < 8 && i + j < input.size(); ++j) {
            output.push_back(input[i + j]);
        }
    }
    return output;
}

struct GameType {
    std::string keyValue;
    int fileNameLen;
};

struct FileEntry {
    std::string fileName;
    uint32_t decryptedSize;
    uint32_t cryptedSize;
    uint32_t fileOffset;
};

std::vector<FileEntry> parseFileTable(const std::vector<char>& data, int fileCount, int fileNameLen) {
    std::vector<FileEntry> entries;
    size_t offset = 0;
    for (int i = 0; i < fileCount; ++i) {
        FileEntry entry;
        entry.fileName = std::string(data.data() + offset, fileNameLen);
        entry.fileName = entry.fileName.c_str(); // Trim null characters
        offset += fileNameLen;

        memcpy(&entry.decryptedSize, data.data() + offset, 4);
        offset += 4;
        memcpy(&entry.cryptedSize, data.data() + offset, 4);
        offset += 4;
        memcpy(&entry.fileOffset, data.data() + offset, 4);
        offset += 4;

        entries.push_back(entry);
    }
    return entries;
}

bool decryptTable(std::vector<char>& data, const GameType& key) {
    std::vector<char> decrypted(key.fileNameLen);
    int j = 0;
    bool iszero = false;
    int zpos = 0;

    for (int i = 0; i < key.fileNameLen; ++i) {
        if (j >= key.keyValue.length()) j = 0;
        decrypted[i] = data[i] + key.keyValue[j];
        if (decrypted[i] == 0 && !iszero) {
            iszero = true;
            zpos = i;
        }
        else if (iszero && decrypted[i] != 0) {
            return false;
        }
        ++j;
    }

    if (iszero && zpos > 3) {
        if (decrypted[zpos - 4] == '.' || decrypted[zpos - 3] == '.') {
            std::copy(decrypted.begin(), decrypted.end(), data.begin());
            return true;
        }
    }
    return false;
}

GameType analyzeOriginalPackage(const std::string& arcFile, bool hasFlag) {

    raw_guess_key(arcFile, hasFlag);
    if (!decode_string.empty()) {
        int fileNameLen = entry_size - 12;
        return { decode_string, fileNameLen};
    }

    throw std::runtime_error("Failed to determine correct key for the original package.");
}

void extractFiles(const std::string& arcFile, const std::string& outputDir, GameType correctKey, bool hasFlag, bool decompress) {
    std::ifstream file(arcFile, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << arcFile << std::endl;
        return;
    }

    uint32_t fileCount;
    file.read(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));

    if (hasFlag) {
        uint32_t flag;
        file.read(reinterpret_cast<char*>(&flag), sizeof(flag));
    }

    std::vector<char> tableData;

    std::cout << "Using key: " << correctKey.keyValue << std::endl;
    std::cout << "filename length: " << correctKey.fileNameLen << std::endl;

    tableData.resize(fileCount * (correctKey.fileNameLen + 12));
    file.read(tableData.data(), tableData.size());

    int j = 0;
    for (size_t i = 0; i < tableData.size(); ++i) {
        if (j >= correctKey.keyValue.length()) j = 0;
        tableData[i] += correctKey.keyValue[j];
        ++j;
    }

    auto entries = parseFileTable(tableData, fileCount, correctKey.fileNameLen);

    fs::create_directories(outputDir);

    for (const auto& entry : entries) {
        std::string outputPath = outputDir + "/" + WideToAscii(AsciiToWide(entry.fileName, 932), CP_ACP);
        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile) {
            std::cerr << "Failed to create output file: " << outputPath << std::endl;
            continue;
        }

        file.seekg(entry.fileOffset);
        std::vector<uint8_t> fileData(entry.cryptedSize);
        file.read((char*)fileData.data(), entry.cryptedSize);
        std::vector<uint8_t> outputData(entry.decryptedSize);
        //std::cout << entry.fileName << "  " << entry.cryptedSize << " " << entry.decryptedSize << std::endl;
        if (decompress) {
            LzssDecompressor decompressor;
            outputData = decompressor.decompress(fileData);
        }
        else {
            outputData = fileData;
        }

        outFile.write((char*)outputData.data(), outputData.size());
        std::cout << "Extracted: " << WideToAscii(AsciiToWide(entry.fileName, 932), CP_ACP) << std::endl;
    }

    std::cout << "Extraction complete." << std::endl;
}

void createRPMPackage(const std::string& inputDir, const std::string& outputArc, const GameType& key, bool usePseudoCompression, bool addFlag) {
    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }

    std::ofstream outFile(outputArc, std::ios::binary);
    if (!outFile) {
        throw std::runtime_error("Failed to create output archive file.");
    }

    uint32_t fileCount = static_cast<uint32_t>(files.size());
    outFile.write(reinterpret_cast<const char*>(&fileCount), sizeof(fileCount));

    if (addFlag) {
        uint32_t flag;
        if (usePseudoCompression) {
            flag = 1;
        }
        else {
            flag = 0;
        }
        outFile.write(reinterpret_cast<const char*>(&flag), sizeof(flag));
    }

    std::streampos tableStart = outFile.tellp();
    std::vector<char> tableData(fileCount * (key.fileNameLen + 12), 0);
    outFile.write(tableData.data(), tableData.size());

    uint32_t currentOffset = static_cast<uint32_t>(outFile.tellp());
    for (size_t i = 0; i < files.size(); ++i) {
        const auto& file = files[i];
        std::cout << "Processing: " << file.filename().string() << std::endl;
        std::ifstream inFile(file, std::ios::binary);
        if (!inFile) {
            throw std::runtime_error("Failed to open input file: " + file.string());
        }

        inFile.seekg(0, std::ios::end);
        uint32_t fileSize = static_cast<uint32_t>(inFile.tellg());
        inFile.seekg(0, std::ios::beg);

        std::vector<uint8_t> fileData(fileSize);
        inFile.read(reinterpret_cast<char*>(fileData.data()), fileSize);

        std::vector<uint8_t> outputData;
        uint32_t outputSize;
        if (usePseudoCompression) {
            outputData = compress(fileData);
            outputSize = static_cast<uint32_t>(outputData.size());
        }
        else {
            outputData = fileData;
            outputSize = fileSize;
        }

        outFile.write(reinterpret_cast<const char*>(outputData.data()), outputSize);

        size_t tableOffset = i * (key.fileNameLen + 12);
        std::string fileName = WideToAscii(file.filename().wstring(), 932);
        fileName.resize(key.fileNameLen, '\0');
        std::copy(fileName.begin(), fileName.end(), tableData.begin() + tableOffset);

        memcpy(tableData.data() + tableOffset + key.fileNameLen, &fileSize, 4);
        memcpy(tableData.data() + tableOffset + key.fileNameLen + 4, &outputSize, 4);
        memcpy(tableData.data() + tableOffset + key.fileNameLen + 8, &currentOffset, 4);

        currentOffset += outputSize;
    }

    int j = 0;
    for (size_t i = 0; i < tableData.size(); ++i) {
        if (j >= key.keyValue.length()) j = 0;
        tableData[i] -= key.keyValue[j];
        ++j;
    }

    outFile.seekp(tableStart);
    outFile.write(tableData.data(), tableData.size());

    std::cout << "RPM package created successfully: " << outputArc << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Made by julixian 2025.03.08" << std::endl;
        std::cout << "Usage: " << std::endl;
        std::cout << "For extract: " << argv[0] << " -e [--decompress] [--flag] [--info keyString filenameLength(decimal)] <input_arc_file> <output_directory>" << std::endl;
        std::cout << "For pack: " << argv[0] << " -p [--compress] [--flag] <--info keyString filenameLength(decimal) / original_arc_file> <input_directory> <output_arc_file>" << std::endl;
        std::cout << "--decompress: " << "Decompress files when extracting" << std::endl;
        std::cout << "--compress: " << "Use fake lzss compress when packing" << std::endl;
        std::cout << "--flag: " << "If the archive has the compress flag at 0x4 - 0x7" << std::endl;
        std::cout << "--info" << "Set archive key information by yourself instead of automatically guessing key" << std::endl;
        std::cout << "Example: " << std::endl;
        std::cout << "./programme.exe -e --decompress --flag msg.arc output" << std::endl;
        std::cout << "./programme.exe -p --info rpm 24 input_dir msg_new.arc" << std::endl;
        std::cout << "./programme.exe -p --compress --flag msg.arc input_dir msg_new.arc" << std::endl;
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "-e") {
        try {
            bool decompress = false;
            bool hasFlag = false;
            bool info = false;
            int argOffset = 2;

            if (std::string(argv[2]) == "--decompress") {
                decompress = true;
                argOffset++;
            }
            if (std::string(argv[argOffset]) == "--flag") {
                hasFlag = true;
                argOffset++;
            }
            if (std::string(argv[argOffset]) == "--info") {
                info = true;
                argOffset++;
            }
            GameType key;
            if (info) {
                key.keyValue = std::string(argv[argOffset++]);
                key.fileNameLen = std::stol(std::string(argv[argOffset++]));
            }
            else {
                key = analyzeOriginalPackage(argv[argc - 2], hasFlag);
            }
            extractFiles(argv[argc - 2], argv[argc - 1], key, hasFlag, decompress);
        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
        return 0;
    }
    else if (mode == "-p") {
        try {
            bool usePseudoCompression = false;
            bool addFlag = false;
            bool info = false;
            int argOffset = 2;

            if (std::string(argv[2]) == "--compress") {
                usePseudoCompression = true;
                argOffset++;
            }
            if (std::string(argv[argOffset]) == "--flag") {
                addFlag = true;
                argOffset++;
            }
            if (std::string(argv[argOffset]) == "--info") {
                info = true;
                argOffset++;
            }
            
            GameType key;
            if (info) {
                key.keyValue = std::string(argv[argOffset++]);
                key.fileNameLen = std::stol(std::string(argv[argOffset++]));
            }
            else {
                key = analyzeOriginalPackage(argv[argc - 3], addFlag);
            }
            createRPMPackage(argv[argc - 2], argv[argc - 1], key, usePseudoCompression, addFlag);
        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
    }
    else {
        std::cout << "Not a valid mode!" << std::endl;
        return 1;
    }

    return 0;
}