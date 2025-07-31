#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <windows.h>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

#pragma pack(1)

// 文件总头
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

class FileProcessor {
public:
    FileProcessor(const string& inPath, const string& outPath, bool isEncrypt)
        : inFilePath(inPath), outFilePath(outPath), isEncryption(isEncrypt) {
    }

    bool process() {
        ifstream inFile(inFilePath, ios::binary);
        if (!inFile) {
            cout << "无法打开输入文件!" << endl;
            return false;
        }

        ofstream outFile(outFilePath, ios::binary);
        if (!outFile) {
            cout << "无法创建输出文件!" << endl;
            return false;
        }

        // 读取并写入文件头
        FileHeader fHeader;
        inFile.read((char*)&fHeader, sizeof(FileHeader));
        if (strncmp(fHeader.magic, "fAGS", 4) != 0 && strncmp(fHeader.magic, "fHKQ", 4) != 0) {
            cout << "无效的文件格式!" << endl;
            return false;
        }
        outFile.write((char*)&fHeader, sizeof(FileHeader));

        while (inFile.peek() != EOF) {
            // 先读取4字节magic来判断块类型
            char magic_peek[4];
            inFile.read(magic_peek, 4);
            if (inFile.gcount() < 4) break; // 到达文件末尾

            // 将文件指针移回4字节，以便完整读取头结构
            inFile.seekg(-4, ios::cur);

            DWORD data_length = 0, head_size = 0, seed = 0;
            size_t dataSize = 0;

            // 根据magic判断并处理不同的头
            if (strncmp(magic_peek, "cCOD", 4) == 0) {
                BlockHeader_cCOD bHeader;
                inFile.read((char*)&bHeader, sizeof(BlockHeader_cCOD));
                outFile.write((char*)&bHeader, sizeof(BlockHeader_cCOD));

                data_length = bHeader.data_length;
                head_size = bHeader.head_size;
                seed = bHeader.seed;
                cout << "处理块: cCOD" << endl;
            }
            else if (strncmp(magic_peek, "cQZT", 4) == 0) {
                BlockHeader_cQZT bHeader;
                inFile.read((char*)&bHeader, sizeof(BlockHeader_cQZT));
                outFile.write((char*)&bHeader, sizeof(BlockHeader_cQZT));

                data_length = bHeader.data_length;
                head_size = bHeader.head_size;
                seed = bHeader.seed;
                cout << "处理块: cQZT" << endl;
            }
            else { // 处理标准块，如 cTEX, cFNM
                BlockHeader bHeader;
                inFile.read((char*)&bHeader, sizeof(BlockHeader));
                outFile.write((char*)&bHeader, sizeof(BlockHeader));

                data_length = bHeader.data_length;
                head_size = bHeader.head_size;
                seed = bHeader.seed;
                cout << "处理块: " << string(bHeader.magic, 4) << endl;
            }

            // head_size 可能大于头结构本身的大小，但数据区总是 data_length - head_size
            dataSize = data_length - head_size;

            vector<BYTE> data(dataSize);
            inFile.read((char*)data.data(), dataSize);

            vector<BYTE> processedData(dataSize);
            if (isEncryption) {
                encryptBlock(data.data(), processedData.data(), dataSize, seed);
                cout << "  加密了 " << dataSize << " 字节" << endl;
            }
            else {
                decryptBlock(data.data(), processedData.data(), dataSize, seed);
                cout << "  解密了 " << dataSize << " 字节" << endl;
            }
            outFile.write((char*)processedData.data(), dataSize);
        }

        cout << (isEncryption ? "加密" : "解密") << "完成!" << endl;
        return true;
    }

private:
    // 解密函数 (无需修改)
    void decryptBlock(BYTE* srcdata, BYTE* dstdata, size_t size, DWORD seed) {
        DWORD key[2][32];

        for (DWORD i = 0; i < 32; ++i) {
            DWORD _key = 0;
            DWORD _seed = seed;
            for (DWORD j = 0; j < 16; ++j) {
                _key = (_key >> 1) | (USHORT)((_seed ^ (_seed >> 1)) << 15);
                _seed >>= 2;
            }
            key[0][i] = seed;
            key[1][i] = _key;
            seed = (seed << 1) | (seed >> 31);
        }

        DWORD* enc = (DWORD*)srcdata;
        DWORD* dec = (DWORD*)dstdata;
        for (DWORD i = 0; i < size / 4; ++i) {
            DWORD _key = key[1][i & 0x1F];
            DWORD flag3 = 3;
            DWORD flag2 = 2;
            DWORD flag1 = 1;
            DWORD result = 0;

            for (DWORD j = 0; j < 16; ++j) {
                DWORD tmp;
                if (_key & 1)
                    tmp = 2 * (enc[i] & flag1) | (enc[i] >> 1) & (flag2 >> 1);
                else
                    tmp = enc[i] & flag3;
                _key >>= 1;
                result |= tmp;
                flag3 <<= 2;
                flag2 <<= 2;
                flag1 <<= 2;
            }
            dec[i] = result ^ key[0][i & 0x1F];
        }

        if (size % 4 > 0) {
            memcpy(dstdata + (size / 4) * 4, srcdata + (size / 4) * 4, size % 4);
        }
    }

    // 加密函数 (无需修改)
    void encryptBlock(BYTE* srcdata, BYTE* dstdata, size_t size, DWORD seed) {
        DWORD key[2][32];

        for (DWORD i = 0; i < 32; ++i) {
            DWORD _key = 0;
            DWORD _seed = seed;
            for (DWORD j = 0; j < 16; ++j) {
                _key = (_key >> 1) | (USHORT)((_seed ^ (_seed >> 1)) << 15);
                _seed >>= 2;
            }
            key[0][i] = seed;
            key[1][i] = _key;
            seed = (seed << 1) | (seed >> 31);
        }

        DWORD* src = (DWORD*)srcdata;
        DWORD* dst = (DWORD*)dstdata;

        for (DWORD i = 0; i < size / 4; ++i) {
            DWORD value = src[i];
            DWORD _key = key[1][i & 0x1F];
            DWORD result = 0;

            value ^= key[0][i & 0x1F];

            for (int j = 0; j < 32; j += 2) {
                DWORD bits = (value >> j) & 3;
                if (_key & (1 << (j / 2))) {
                    bits = ((bits & 2) >> 1) | ((bits & 1) << 1);
                }
                result |= (bits << j);
            }
            dst[i] = result;
        }

        if (size % 4 > 0) {
            memcpy(dstdata + (size / 4) * 4, srcdata + (size / 4) * 4, size % 4);
        }
    }

    string inFilePath;
    string outFilePath;
    bool isEncryption;
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        cout << "Made by julixian 2025.07.31" << endl;
        cout << "Usage: " << argv[0] << " <decrypt/encrypt> <input_dir> <output_dir>" << endl;
        return 1;
    }

    bool isEncrypt = (string(argv[1]) == "encrypt");
    if (!isEncrypt && (string(argv[1]) != "decrypt")) {
        cout << "Unkown operation: " << argv[1] << endl;
        return 1;
    }
    string inDir = argv[2];
    string outDir = argv[3];

    fs::create_directories(outDir);

    for (const auto& entry : fs::directory_iterator(inDir)) {
        if (entry.is_regular_file()) {
            string inFile = entry.path().string();
            string fileName = entry.path().filename().string();
            string outFile = (fs::path(outDir) / fileName).string();

            cout << "\n处理文件: " << fileName << endl;
            FileProcessor processor(inFile, outFile, isEncrypt);
            if (!processor.process()) {
                cout << "处理失败: " << fileName << endl;
                continue;
            }
        }
    }

    cout << "\n所有文件处理完成!" << endl;
    return 0;
}
