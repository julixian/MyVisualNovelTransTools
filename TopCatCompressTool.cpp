#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

// 字节右旋转
uint8_t rotateRight(uint8_t byte, int positions) {
    return (byte >> positions) | (byte << (8 - positions));
}

// 字节左旋转(加密)
uint8_t rotateLeft(uint8_t byte, int positions) {
    return (byte << positions) | (byte >> (8 - positions));
}

// LZ解压缩
std::vector<uint8_t> unpackLz(const std::vector<uint8_t>& input) {
    uint32_t unpackedSize;
    memcpy(&unpackedSize, input.data(), 4);

    std::vector<uint8_t> output(unpackedSize);
    size_t dst = 0;
    size_t src = 4;
    int bits = 2;

    while (dst < output.size() && src < input.size()) {
        bits >>= 1;
        if (bits == 1) {
            bits = input[src++];
            if (src >= input.size()) break;
            bits |= 0x100;
        }

        if ((bits & 1) == 0) {
            if (src + 1 >= input.size()) break;
            int count = input[src];
            int offset = (input[src + 1] << 4) | (count >> 4);
            count = std::min((count & 0xF) + 3, (int)(output.size() - dst));
            src += 2;

            // 重叠复制
            for (int i = 0; i < count; ++i) {
                if (dst >= offset) {
                    output[dst] = output[dst - offset];
                }
                dst++;
            }
        }
        else {
            if (src >= input.size()) break;
            output[dst++] = input[src++];
        }
    }

    return output;
}

// 压缩实现 - 严格匹配解压格式
std::vector<uint8_t> packLz(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> output;

    // 写入原始大小(4字节)
    uint32_t inputSize = static_cast<uint32_t>(input.size());
    output.resize(4);
    memcpy(output.data(), &inputSize, 4);

    size_t inputPos = 0;
    uint8_t controlByte = 0;
    int controlBits = 8;
    size_t controlBytePos = 0;

    while (inputPos < input.size()) {
        // 需要新的控制字节
        if (controlBits == 8) {
            controlBytePos = output.size();
            output.push_back(0); // 占位符
            controlByte = 0;
            controlBits = 0;
        }

        // 始终使用未压缩字节（控制位为1）
        controlByte |= (1 << controlBits);
        output[controlBytePos] = controlByte;
        output.push_back(input[inputPos++]);
        controlBits++;
    }

    return output;
}

// 解密脚本
void decryptScript(std::vector<uint8_t>& data) {
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = rotateRight(data[i], 1);
    }
}

// 加密脚本
void encryptScript(std::vector<uint8_t>& data) {
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = rotateLeft(data[i], 1);
    }
}

// 处理单个文件 (解压)
void processFileUnpack(const fs::path& inputPath, const fs::path& outputPath) {
    // 读取输入文件
    std::ifstream input(inputPath, std::ios::binary);
    if (!input) {
        std::cerr << "无法打开输入文件: " << inputPath << std::endl;
        return;
    }

    // 读取文件内容
    std::vector<uint8_t> buffer(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );
    input.close();

    // 解压缩
    std::vector<uint8_t> unpacked = unpackLz(buffer);

    // 解密
    decryptScript(unpacked);

    // 创建输出目录（如果不存在）
    fs::create_directories(outputPath.parent_path());

    // 写入输出文件
    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        std::cerr << "无法创建输出文件: " << outputPath << std::endl;
        return;
    }
    output.write(reinterpret_cast<const char*>(unpacked.data()), unpacked.size());
    output.close();

    std::cout << "解压完成: " << inputPath.filename() << std::endl;
}

// 处理单个文件 (压缩)
void processFilePack(const fs::path& inputPath, const fs::path& outputPath) {
    // 读取输入文件
    std::ifstream input(inputPath, std::ios::binary);
    if (!input) {
        std::cerr << "无法打开输入文件: " << inputPath << std::endl;
        return;
    }

    // 读取文件内容
    std::vector<uint8_t> buffer(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );
    input.close();

    // 加密
    encryptScript(buffer);

    // 压缩
    std::vector<uint8_t> packed = packLz(buffer);

    // 创建输出目录（如果不存在）
    fs::create_directories(outputPath.parent_path());

    // 写入输出文件
    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        std::cerr << "无法创建输出文件: " << outputPath << std::endl;
        return;
    }
    output.write(reinterpret_cast<const char*>(packed.data()), packed.size());
    output.close();

    std::cout << "压缩完成: " << inputPath.filename() << std::endl;
}

// 处理目录
void processDirectory(const fs::path& inputDir, const fs::path& outputDir, bool isPacking) {
    try {
        for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
            if (entry.is_regular_file()) {
                // 获取相对路径
                fs::path relativePath = fs::relative(entry.path(), inputDir);
                fs::path outputPath = outputDir / relativePath;

                // 检查文件扩展名
                std::string ext = entry.path().extension().string();
                if (ext == ".TCT" || ext == ".TSF") {
                    if (isPacking) {
                        processFilePack(entry.path(), outputPath);
                    }
                    else {
                        processFileUnpack(entry.path(), outputPath);
                    }
                }
            }
        }
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "文件系统错误: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cout << "Made by julixian 2025.01.01" << std::endl;
        std::cout << "Usage: " << argv[0] << " <compress|decompress> <inputdir> <outputdir>" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    fs::path inputDir(argv[2]);
    fs::path outputDir(argv[3]);

    if (!fs::exists(inputDir)) {
        std::cerr << "输入目录不存在: " << inputDir << std::endl;
        return 1;
    }

    if (mode == "compress") {
        processDirectory(inputDir, outputDir, true);
    }
    else if (mode == "decompress") {
        processDirectory(inputDir, outputDir, false);
    }
    else {
        std::cerr << "Invaild mode. Please use 'pack' or 'unpack'." << std::endl;
        return 1;
    }

    return 0;
}
