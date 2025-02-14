#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstdint>

namespace fs = std::filesystem;

// EAGLS系统密钥
const std::string EAGLS_KEY = "EAGLS_SYSTEM";

// 随机数生成器类
class CRuntimeRandomGenerator {
private:
    uint32_t m_seed;

public:
    void SRand(int seed) {
        m_seed = static_cast<uint32_t>(seed);
    }

    int Rand() {
        m_seed = m_seed * 214013u + 2531011u;
        return (static_cast<int>(m_seed >> 16) & 0x7FFF);
    }
};

// 解密单个文件
bool DecryptFile(const fs::path& inputPath, const fs::path& outputPath) {
    // 读取输入文件
    std::ifstream inFile(inputPath, std::ios::binary);
    if (!inFile) {
        std::cerr << "无法打开输入文件: " << inputPath << std::endl;
        return false;
    }

    // 获取文件大小
    inFile.seekg(0, std::ios::end);
    size_t fileSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);

    // 读取整个文件内容
    std::vector<uint8_t> data(fileSize);
    inFile.read(reinterpret_cast<char*>(data.data()), fileSize);
    inFile.close();

    // 检查文件大小是否足够
    if (fileSize < 3603) { // 3600 + 2 + 1 (最小需要的大小)
        std::cerr << "文件太小: " << inputPath << std::endl;
        return false;
    }

    // 解密过程
    const int text_offset = 3600;
    const int text_length = fileSize - text_offset - 2;

    CRuntimeRandomGenerator rng;
    rng.SRand(static_cast<int8_t>(data[fileSize - 1]));

    for (int i = 0; i < text_length; i += 2) {
        data[text_offset + i] ^= EAGLS_KEY[rng.Rand() % EAGLS_KEY.length()];
    }

    // 创建输出目录（如果不存在）
    fs::create_directories(outputPath.parent_path());

    // 写入输出文件
    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile) {
        std::cerr << "无法创建输出文件: " << outputPath << std::endl;
        return false;
    }

    outFile.write(reinterpret_cast<char*>(data.data()), fileSize);
    outFile.close();

    std::cout << "成功解密: " << inputPath.filename() << std::endl;
    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Made by julixian 2025.02.14" << std::endl;
        std::cout << "用法: " << argv[0] << " <输入目录> <输出目录>" << std::endl;
        return 1;
    }

    fs::path inputDir = argv[1];
    fs::path outputDir = argv[2];

    if (!fs::exists(inputDir)) {
        std::cerr << "输入目录不存在!" << std::endl;
        return 1;
    }

    // 创建输出目录（如果不存在）
    fs::create_directories(outputDir);

    // 计数器
    int successCount = 0;
    int failCount = 0;

    // 遍历输入目录
    for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
        if (!entry.is_regular_file()) continue;

        // 只处理.dat文件
        if (entry.path().extension() != ".dat") continue;

        // 构建输出文件路径
        fs::path relativePath = fs::relative(entry.path(), inputDir);
        fs::path outputPath = outputDir / relativePath;

        // 解密文件
        if (DecryptFile(entry.path(), outputPath)) {
            successCount++;
        }
        else {
            failCount++;
        }
    }

    // 输出统计信息
    std::cout << "\n解密完成!" << std::endl;
    std::cout << "成功: " << successCount << " 个文件" << std::endl;
    std::cout << "失败: " << failCount << " 个文件" << std::endl;

    return 0;
}
