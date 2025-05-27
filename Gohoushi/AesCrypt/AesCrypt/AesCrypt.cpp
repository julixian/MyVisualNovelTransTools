#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <cryptopp/cryptlib.h>
#include <cryptopp/aes.h>
#include <cryptopp/sha.h>
#include <cryptopp/hex.h>
#include <cryptopp/filters.h>
#include <cryptopp/pwdbased.h>

// 用于将十六进制字符串转换为字节数组
std::vector<CryptoPP::byte> HexToBytes(const std::string& hex) {
    std::vector<CryptoPP::byte> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        if (i + 1 >= hex.length()) break; // 避免奇数长度的十六进制字符串
        std::string byteString = hex.substr(i, 2);
        CryptoPP::byte b = (CryptoPP::byte)strtol(byteString.c_str(), NULL, 16);
        bytes.push_back(b);
    }
    return bytes;
}

// 将字节数组转换为十六进制字符串
std::string BytesToHex(const CryptoPP::byte* data, size_t length) {
    std::string result;
    CryptoPP::HexEncoder encoder(new CryptoPP::StringSink(result), false);
    encoder.Put(data, length);
    encoder.MessageEnd();
    return result;
}

// 模拟游戏中的 PasswordDeriveBytes 函数
std::vector<CryptoPP::byte> DeriveKeyFromPassword(const std::string& password, const std::vector<CryptoPP::byte>& salt, size_t keyLength) {
    std::vector<CryptoPP::byte> derivedKey(keyLength);

    // 使用 PBKDF1 与 SHA1，迭代 100 次 (与游戏中的默认值相同)
    CryptoPP::PKCS5_PBKDF1<CryptoPP::SHA1> pbkdf;
    pbkdf.DeriveKey(
        derivedKey.data(), derivedKey.size(),
        0, // purpose
        (CryptoPP::byte*)password.data(), password.size(),
        salt.data(), salt.size(),
        100, // 迭代次数
        0.0 // 不使用时间约束
    );

    return derivedKey;
}

// 实现类似于游戏中的 cipher 方法的加密/解密逻辑
// 注意：由于使用 XOR，加密和解密操作是相同的
void ProcessBuffer(CryptoPP::byte* buffer, size_t bufferSize, const CryptoPP::byte* key, bool isEncrypt) {
    const size_t blockSize = CryptoPP::AES::BLOCKSIZE;

    // 创建 AES 加密对象
    CryptoPP::AES::Encryption aesEncryption(key, CryptoPP::AES::DEFAULT_KEYLENGTH);

    // 为每个块创建一个临时缓冲区
    std::vector<CryptoPP::byte> counterBlock(blockSize, 0);
    std::vector<CryptoPP::byte> encryptedCounter(blockSize);

    // 按块处理整个缓冲区
    uint64_t blockIndex = 1; // 从1开始，与游戏逻辑一致

    for (size_t position = 0; position < bufferSize; ) {
        // 将 blockIndex 转换为字节数组（小端序）
        for (int j = 0; j < 8; j++) {
            counterBlock[j] = (blockIndex >> (j * 8)) & 0xFF;
        }

        // 加密计数器块
        aesEncryption.ProcessBlock(counterBlock.data(), encryptedCounter.data());

        // 计算这个块可以处理的字节数
        size_t bytesToProcess = std::min(blockSize, bufferSize - position);

        // 使用 XOR 处理这个块的数据
        for (size_t i = 0; i < bytesToProcess; i++) {
            buffer[position + i] ^= encryptedCounter[i];
        }

        // 移动到下一个块
        position += bytesToProcess;
        blockIndex++;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cout << "Usage:" << std::endl;
        std::cout << "  Decrypt with direct key:  " << argv[0] << " decrypt <input_file> <output_file> -k <key_hex>" << std::endl;
        std::cout << "  Decrypt with password:    " << argv[0] << " decrypt <input_file> <output_file> -p <password> <bundle_key>" << std::endl;
        std::cout << "  Encrypt with direct key:  " << argv[0] << " encrypt <input_file> <output_file> -k <key_hex>" << std::endl;
        std::cout << "  Encrypt with password:    " << argv[0] << " encrypt <input_file> <output_file> -p <password> <bundle_key>" << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  " << argv[0] << " decrypt encrypted.bundle decrypted.bundle -k 1A2B3C4D5E6F7A8B9C0D1E2F3A4B5C6D" << std::endl;
        std::cout << "  " << argv[0] << " decrypt encrypted.bundle decrypted.bundle -p 0123456789012345 bg_control" << std::endl;
        std::cout << "  " << argv[0] << " encrypt decrypted.bundle encrypted.bundle -k 1A2B3C4D5E6F7A8B9C0D1E2F3A4B5C6D" << std::endl;
        std::cout << "  " << argv[0] << " encrypt decrypted.bundle encrypted.bundle -p 0123456789012345 bg_control" << std::endl;
        return 1;
    }

    std::string operation = argv[1];
    std::string inputFile = argv[2];
    std::string outputFile = argv[3];
    std::string mode = argv[4];

    bool isEncrypt = false;
    if (operation == "encrypt") {
        isEncrypt = true;
    }
    else if (operation == "decrypt") {
        isEncrypt = false;
    }
    else {
        std::cerr << "Error: Operation must be 'encrypt' or 'decrypt'" << std::endl;
        return 1;
    }

    std::vector<CryptoPP::byte> key;

    try {
        if (mode == "-k" && argc >= 6) {
            // 直接密钥模式
            std::string keyHex = argv[5];
            key = HexToBytes(keyHex);

            std::cout << "Using provided key: " << keyHex << std::endl;
        }
        else if (mode == "-p" && argc >= 7) {
            // 密码派生模式
            std::string password = argv[5];
            std::string bundleKey = argv[6];

            // 将 bundle key 转换为 UTF-8 字节数组作为盐值
            std::vector<CryptoPP::byte> salt(bundleKey.begin(), bundleKey.end());

            // 派生密钥
            key = DeriveKeyFromPassword(password, salt, CryptoPP::AES::DEFAULT_KEYLENGTH);

            std::cout << "Password: " << password << std::endl;
            std::cout << "Bundle Key (salt): " << bundleKey << std::endl;
            std::cout << "Salt bytes: " << BytesToHex(salt.data(), salt.size()) << std::endl;
            std::cout << "Derived key: " << BytesToHex(key.data(), key.size()) << std::endl;
        }
        else {
            std::cerr << "Error: Invalid mode or missing arguments" << std::endl;
            return 1;
        }

        // 验证密钥长度
        if (key.size() != CryptoPP::AES::DEFAULT_KEYLENGTH) {
            std::cerr << "Error: Key must be exactly 16 bytes (32 hex characters)" << std::endl;
            return 1;
        }

        // 打开输入文件
        std::ifstream inFile(inputFile, std::ios::binary);
        if (!inFile) {
            std::cerr << "Error: Cannot open input file " << inputFile << std::endl;
            return 1;
        }

        // 获取文件大小
        inFile.seekg(0, std::ios::end);
        size_t fileSize = inFile.tellg();
        inFile.seekg(0, std::ios::beg);

        // 读取整个文件到内存
        std::vector<CryptoPP::byte> buffer(fileSize);
        inFile.read(reinterpret_cast<char*>(buffer.data()), fileSize);
        inFile.close();

        std::cout << "File size: " << fileSize << " bytes" << std::endl;
        std::cout << (isEncrypt ? "Encrypting..." : "Decrypting...") << std::endl;

        // 处理缓冲区 (加密或解密)
        ProcessBuffer(buffer.data(), fileSize, key.data(), isEncrypt);

        // 写入输出文件
        std::ofstream outFile(outputFile, std::ios::binary);
        if (!outFile) {
            std::cerr << "Error: Cannot open output file " << outputFile << std::endl;
            return 1;
        }
        outFile.write(reinterpret_cast<char*>(buffer.data()), fileSize);
        outFile.close();

        std::cout << (isEncrypt ? "Encryption" : "Decryption") << " completed successfully. Output written to " << outputFile << std::endl;

    }
    catch (const CryptoPP::Exception& e) {
        std::cerr << "Crypto++ Error: " << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
