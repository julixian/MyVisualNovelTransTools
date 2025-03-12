#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <windows.h>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

#pragma pack(1)

struct FileHeader {
    char magic[4];
    DWORD filesize;
};

struct BlockHeader {
    char magic[4];
    DWORD data_length;
    DWORD head_size;
    DWORD seed;
};

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

        bool shouldStop = false;
        while (inFile.peek() != EOF && !shouldStop) {
            BlockHeader bHeader;
            inFile.read((char*)&bHeader, sizeof(BlockHeader));

            vector<char> extraHeader;
            size_t extraHeaderSize = 0;
            if (strncmp(bHeader.magic, "cCOD", 4) == 0) {
                extraHeaderSize = bHeader.head_size - sizeof(BlockHeader);
                extraHeader.resize(extraHeaderSize);
                inFile.read(extraHeader.data(), extraHeaderSize);
            }

            size_t dataSize = bHeader.data_length - bHeader.head_size;
            vector<BYTE> data(dataSize);
            inFile.read((char*)data.data(), dataSize);

            outFile.write((char*)&bHeader, sizeof(BlockHeader));
            if (extraHeaderSize > 0) {
                outFile.write(extraHeader.data(), extraHeaderSize);
            }

            vector<BYTE> processedData(dataSize);
            if (isEncryption) {
                encryptBlock(data.data(), processedData.data(), dataSize, bHeader.seed);
                cout << "加密块: ";
            }
            else {
                decryptBlock(data.data(), processedData.data(), dataSize, bHeader.seed);
                cout << "解密块: ";
            }
            outFile.write((char*)processedData.data(), dataSize);

            cout << string(bHeader.magic, 4) << " 大小: " << dataSize << " 字节" << endl;

            if (strncmp(bHeader.magic, "cCOD", 4) == 0 || strncmp(bHeader.magic, "cQZT", 4) == 0) {
                shouldStop = true;
                if (inFile.peek() != EOF) {
                    cout << "复制剩余内容..." << endl;
                    char buffer[4096];
                    while (inFile.read(buffer, sizeof(buffer))) {
                        outFile.write(buffer, inFile.gcount());
                    }
                    if (inFile.gcount() > 0) {
                        outFile.write(buffer, inFile.gcount());
                    }
                }
            }
        }

        cout << (isEncryption ? "加密" : "解密") << "完成!" << endl;
        return true;
    }

private:
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

        memcpy(dec + size / 4, enc + size / 4, size % 4);
    }

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

        memcpy(dst + size / 4, src + size / 4, size % 4);
    }

    string inFilePath;
    string outFilePath;
    bool isEncryption;
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        cout << "Made by julixian 2025.03.01" << endl;
        cout << "用法: " << argv[0] << " <操作类型:0解密/1加密> <输入文件夹> <输出文件夹>" << endl;
        return 1;
    }

    bool isEncrypt = (string(argv[1]) == "1");
    string inDir = argv[2];
    string outDir = argv[3];

    // 确保输出目录存在
    fs::create_directories(outDir);

    // 遍历输入目录
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
