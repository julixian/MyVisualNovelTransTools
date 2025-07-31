#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <Windows.h>
#include <map>
#include <regex>

#pragma pack(1)
struct FileHeader {
    char magic[4];
    DWORD filesize;
};

// 标准块头 (cTEX, cFNM, etc.) - 16 bytes
struct BlockHeader {
    char magic[4];
    DWORD data_length;
    DWORD head_size;
    DWORD seed;
};

// cCOD 专用块头 - 28 bytes (0x1C)
struct BlockHeader_cCOD {
    char magic[4];
    DWORD data_length;
    DWORD head_size;
    DWORD code_length;
    DWORD seed;
    DWORD mode;
    DWORD code_count;
};

// cQZT 专用块头 - 20 bytes (0x14)
struct BlockHeader_cQZT {
    char magic[4];
    DWORD data_length;
    DWORD head_size;
    DWORD seed;
    DWORD code_count;
};
#pragma pack()

using namespace std;
namespace fs = std::filesystem;

class TextTool {
private:
    // 存储文件内容
    std::regex reg1 = std::regex(R"(\x00)");
    std::regex reg2 = std::regex(R"(\\x00)");
    FileHeader file_header_;
    map<string, vector<char>> block_headers_;
    map<string, vector<char>> block_data_;

    // 从cTEX数据中获取字符串
    string getStringFromTex(uint32_t offset) {
        const auto& tex_data = block_data_["cTEX"];
        if (offset >= tex_data.size()) return "[INVALID_OFFSET]";

        size_t end = offset;
        // 寻找双空字节结尾
        while (end + 1 < tex_data.size() && !(tex_data[end] == 0 && tex_data[end + 1] == 0)) {
            end++;
        }

        std::string str(tex_data.begin() + offset, tex_data.begin() + end);
        return std::regex_replace(str, reg1, "\\x00");
    }

    vector<char> preprocessText(const string& text) {
        vector<char> result;
        std::string str = std::regex_replace(text, reg2, "\x00");
        for (char c : str) {
            result.push_back(c);
        }
        result.push_back('\0');
        result.push_back('\0');
        return result;
    }

    vector<string> block_order;


public:
    // 加载解密后的脚本文件到内存
    bool loadScript(const string& inPath) {
        ifstream inFile(inPath, ios::binary);
        if (!inFile) {
            cerr << "Error: Cannot open input file " << inPath << endl;
            return false;
        }

        inFile.read((char*)&file_header_, sizeof(FileHeader));
        if (strncmp(file_header_.magic, "fAGS", 4) != 0 && strncmp(file_header_.magic, "fHKQ", 4) != 0) {
            cerr << "Error: Invalid file header." << endl;
            return false;
        }

        while (inFile.peek() != EOF) {
            char magic_peek[4];
            inFile.read(magic_peek, 4);
            if (inFile.gcount() < 4) break;
            inFile.seekg(-4, ios::cur);

            string magic_str(magic_peek, 4);
            uint32_t header_size = 0;
            uint32_t data_len = 0;
            uint32_t head_len = 0;

            if (magic_str == "cCOD") {
                header_size = sizeof(BlockHeader_cCOD);
                BlockHeader_cCOD header;
                inFile.read((char*)&header, header_size);
                data_len = header.data_length;
                head_len = header.head_size;
            }
            else if (magic_str == "cQZT") {
                header_size = sizeof(BlockHeader_cQZT);
                BlockHeader_cQZT header;
                inFile.read((char*)&header, header_size);
                data_len = header.data_length;
                head_len = header.head_size;
            }
            else {
                header_size = sizeof(BlockHeader);
                BlockHeader header;
                inFile.read((char*)&header, header_size);
                data_len = header.data_length;
                head_len = header.head_size;
            }

            inFile.seekg(-(long)header_size, ios::cur);
            block_headers_[magic_str].resize(header_size);
            inFile.read(block_headers_[magic_str].data(), header_size);

            size_t dataSize = data_len - head_len;
            block_data_[magic_str].resize(dataSize);
            inFile.read(block_data_[magic_str].data(), dataSize);
            block_order.push_back(magic_str);
        }
        return true;
    }

    // 提取文本
    bool extract(const string& outPath) {
        if (block_data_.find("cCOD") == block_data_.end() || block_data_.find("cTEX") == block_data_.end()) {
            cerr << "Error: cCOD or cTEX block not found." << endl;
            return false;
        }

        ofstream outFile(outPath);
        if (!outFile) {
            cerr << "Error: Cannot create output file " << outPath << endl;
            return false;
        }

        auto& ccod = block_data_["cCOD"];
        for (uint32_t i = 0; i < ccod.size();) {
            uint16_t opcode = *(uint16_t*)&ccod[i];
            uint16_t length = *(uint16_t*)&ccod[i + 2];
            int num_args = (length / 4) - 1;

            if (length == 0) {
                /*cout << "Warning: Encountered an instruction with length 0 at offset 0x"
                    << hex << i << ". Stopping scan to prevent infinite loop." << endl;*/
                break; // 发现零长度指令，立即跳出循环
            }
            if (i + length > ccod.size()) {
                cout << "Warning: Instruction at offset 0x" << hex << i
                    << " with length " << length << " would exceed block size. Stopping." << endl;
                break;
            }

            for (int j = 0; j < num_args; ++j) {
                bool is_text_ptr = false;

                if (opcode == 0x01 && j == 1) is_text_ptr = true;
                if (opcode == 0x02 && (j == 1 || j == 2)) is_text_ptr = true;
                if (opcode == 0x09 && (j == 2 || j == 4 || j == 6)) is_text_ptr = true;
                if (opcode == 0x2f && j == 0) is_text_ptr = true;

                if (is_text_ptr) {
                    uint32_t pointer_address = i + 4 + (j * 4);
                    uint32_t text_offset = *(uint32_t*)&ccod[pointer_address];
                    if (text_offset != 0xFFFFFFFF && text_offset < block_data_["cTEX"].size()) {
                        string text = getStringFromTex(text_offset);
                        outFile << hex << uppercase << setfill('0') << setw(8) << pointer_address
                            << ":::::";
                        if (opcode == 0x02 && j == 1) {
                            outFile << "<name>";
                        }
                        else if (opcode == 0x2f && j == 0) {
                            outFile << "<sp>";
                        }
                        outFile << text << endl;
                    }
                }
            }
            i += length;
        }
        cout << "Text extracted to " << outPath << endl;
        return true;
    }

    // 注入文本
    bool inject(const string& txtPath, const string& outPath) {
        ifstream txtFile(txtPath);
        if (!txtFile) {
            cerr << "Error: Cannot open TXT file " << txtPath << endl;
            return false;
        }

        string line;
        while (getline(txtFile, line)) {
            size_t separator_pos = line.find(":::::");
            if (separator_pos == string::npos) continue;

            string addr_str = line.substr(0, separator_pos);
            string new_text = line.substr(separator_pos + 5);
            if (new_text.find("<name>") != string::npos) {
                new_text = new_text.substr(6);
            }
            else if (new_text.find("<sp>") != string::npos) {
                new_text = new_text.substr(4);
            }

            uint32_t pointer_address;
            stringstream ss;
            ss << hex << addr_str;
            ss >> pointer_address;

            vector<char> processed_text_vec = preprocessText(new_text);

            // 获取新偏移量（即当前cTEX数据块的大小）
            uint32_t new_text_offset = block_data_["cTEX"].size();

            // 将新文本追加到cTEX块
            block_data_["cTEX"].insert(block_data_["cTEX"].end(), processed_text_vec.begin(), processed_text_vec.end());

            // 更新cCOD中的指针
            *(uint32_t*)&block_data_["cCOD"][pointer_address] = new_text_offset;
        }

        // --- 重建并保存文件 ---
        ofstream outFile(outPath, ios::binary);
        if (!outFile) {
            cerr << "Error: Cannot create injected file " << outPath << endl;
            return false;
        }

        // 1. 对齐新的cTEX块
        while (block_data_["cTEX"].size() % 4 != 0) {
            block_data_["cTEX"].push_back('\0');
        }

        // 2. 更新头部信息
        // 更新cTEX头
        auto& ctex_header_vec = block_headers_["cTEX"];
        BlockHeader* ctex_header = (BlockHeader*)ctex_header_vec.data();
        uint32_t old_ctex_data_size = ctex_header->data_length - ctex_header->head_size;
        ctex_header->data_length = ctex_header->head_size + block_data_["cTEX"].size();

        // 更新文件总头
        file_header_.filesize = file_header_.filesize - old_ctex_data_size + block_data_["cTEX"].size();

        // 3. 写入文件
        outFile.write((char*)&file_header_, sizeof(FileHeader));

        for (const auto& magic : block_order) {
            if (block_headers_.count(magic)) {
                outFile.write(block_headers_[magic].data(), block_headers_[magic].size());
                outFile.write(block_data_[magic].data(), block_data_[magic].size());
            }
        }

        cout << "Injection complete. Saved to " << outPath << endl;
        return true;
    }
};


int main(int argc, char* argv[]) {
    if (argc < 4) {
        cout << "Made by julixian 2025.07.31" << std::endl;
        cout << "Usage:\n";
        cout << "  To Dump: " << argv[0] << " dump <decrypted_script_dir> <output_dir>\n";
        cout << "  To Inject:  " << argv[0] << " inject <decrypted_script_dir> <input_translated-txt_folder> <output_folder>\n";
        return 1;
    }

    string mode = argv[1];

    if (mode == "dump") {
        if (argc != 4) {
            std::cerr << "Error: Incorrect number of arguments for dump mode." << std::endl;
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
                TextTool tool;
                if (!tool.loadScript(entry.path().string())) {
                    continue;
                }
                tool.extract(outputPath.string());
            }
        }
    }
    else if (mode == "inject") {
        if (argc != 5) {
            std::cerr << "Error: Incorrect number of arguments for inject mode." << std::endl;
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
                    TextTool tool;
                    if (!tool.loadScript(entry.path().string())) {
                        continue;
                    }
                    tool.inject(txtPath.string(), outputPath.string());
                }
                else {
                    std::cerr << "Warning: No corresponding file found for " << relativePath << std::endl;
                }
            }
        }
    }
    else {
        cerr << "Error: Invalid mode '" << mode << "'. Use 'dump' or 'inject'." << endl;
        return 1;
    }

    return 0;
}
