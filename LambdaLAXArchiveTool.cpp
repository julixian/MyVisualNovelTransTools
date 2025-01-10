#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <map>
#include <iomanip>

namespace fs = std::filesystem;

// 文件条目结构
struct FileEntry {
    std::string name;
    uint32_t unpackedSize;
    uint32_t size;
    uint32_t offset;
    uint8_t reserved1[8];    // 0x08-0x0F
    uint8_t reserved2[8];    // 0x1C-0x23
};

class MsbBitStream {
private:
    std::ifstream& m_input;
    uint32_t m_bits;
    int m_bit_count;

public:
    MsbBitStream(std::ifstream& input) : m_input(input), m_bits(0), m_bit_count(0) {}

    int GetBits(int count) {
        while (m_bit_count < count) {
            int byte = m_input.get();
            if (byte == EOF) return -1;
            m_bits = (m_bits << 8) | byte;
            m_bit_count += 8;
        }
        int result = (m_bits >> (m_bit_count - count)) & ((1 << count) - 1);
        m_bit_count -= count;
        return result;
    }
};

// LZSS解压缩类
class LzssDecoder {
private:
    uint8_t frame[0x1000];
    int framePos;

public:
    LzssDecoder() : framePos(0xFEE) {
        memset(frame, 0, sizeof(frame));
    }

    size_t decode(const uint8_t* input, size_t inSize, uint8_t* output, size_t outSize) {
        int bits = 2;
        size_t inPos = 0;
        size_t outPos = 0;

        while (outPos < outSize && inPos < inSize) {
            bits >>= 1;
            if (bits == 1) {
                if (inPos >= inSize) break;
                bits = input[inPos++] | 0x100;
            }

            if (inPos >= inSize) break;
            uint8_t lo = input[inPos++];

            if (bits & 1) {
                // 直接字节
                output[outPos++] = frame[framePos++ & 0xFFF] = lo;
            }
            else {
                // 压缩引用
                if (inPos >= inSize) break;
                uint8_t hi = input[inPos++];
                int offset = ((hi & 0xF0) << 4) | lo;
                int count = std::min(3 + (hi & 0xF), (int)(outSize - outPos));

                while (count-- > 0) {
                    uint8_t v = frame[offset++ & 0xFFF];
                    output[outPos++] = frame[framePos++ & 0xFFF] = v;
                }
            }
        }
        return outPos;
    }
};

class HuffmanDecoder {
private:
    static const int TREE_SIZE = 512;
    uint16_t lhs[TREE_SIZE];
    uint16_t rhs[TREE_SIZE];
    uint16_t m_token;
    MsbBitStream& m_input;

    uint16_t CreateTree() {
        int bit = m_input.GetBits(1);
        if (bit == -1) {
            throw std::runtime_error("Unexpected end of the Huffman-compressed stream.");
        }
        else if (bit != 0) {
            uint16_t v = m_token++;
            if (v >= TREE_SIZE)
                throw std::runtime_error("Invalid Huffman-compressed stream.");
            lhs[v] = CreateTree();
            rhs[v] = CreateTree();
            return v;
        }
        else {
            return m_input.GetBits(8);
        }
    }

public:
    HuffmanDecoder(MsbBitStream& input) : m_input(input), m_token(256) {
        memset(lhs, 0, sizeof(lhs));
        memset(rhs, 0, sizeof(rhs));
    }

    size_t decode(uint8_t* output, size_t outSize) {
        m_token = 256;
        uint16_t root = CreateTree();
        size_t outPos = 0;

        while (outPos < outSize) {
            uint16_t symbol = root;
            while (symbol >= 0x100) {
                int bit = m_input.GetBits(1);
                if (bit == -1) return outPos;
                symbol = (bit != 0) ? rhs[symbol] : lhs[symbol];
            }
            output[outPos++] = (uint8_t)symbol;
        }
        return outPos;
    }
};

// LAX流处理类
class LaxStream {
private:
    std::ifstream& file;
    std::vector<uint8_t> m_buffer;
    size_t m_buffer_size;
    size_t m_buffer_pos;
    bool m_eof;

    bool readSegment() {
        if (m_eof) return false;

        // 记录当前位置用于Huffman解压
        std::streampos chunk_start = file.tellg();

        // 读取段头10字节
        uint8_t header[10];
        if (file.read((char*)header, 10).gcount() != 10) {
            m_eof = true;
            return false;
        }

        // 检查"_AF"标识
        if (memcmp(header, "_AF", 2) != 0) {
            m_eof = true;
            return false;
        }

        uint8_t method = header[3];
        uint32_t chunk_size = *(uint16_t*)(header + 4);
        uint32_t final_size = *(uint16_t*)(header + 6);
        uint32_t unpacked_size = *(uint16_t*)(header + 8);

        // 确保缓冲区足够大
        if (m_buffer.size() < unpacked_size)
            m_buffer.resize(unpacked_size);

        // 根据方法解压
        switch (method) {
        case '1': { // LZSS
            std::vector<uint8_t> compressed(chunk_size - 10);
            if (!file.read((char*)compressed.data(), compressed.size())) {
                m_eof = true;
                return false;
            }
            LzssDecoder decoder;
            m_buffer_size = decoder.decode(compressed.data(), compressed.size(),
                m_buffer.data(), unpacked_size);
            break;
        }
        case '2': { // Huffman
            // 重新定位到数据开始处
            file.seekg(chunk_start + std::streampos(10));
            MsbBitStream bits(file);
            HuffmanDecoder decoder(bits);
            m_buffer_size = decoder.decode(m_buffer.data(), unpacked_size);
            // 确保定位到块末尾
            file.seekg(chunk_start + std::streampos(chunk_size));
            break;
        }
        default: // 无压缩
            m_buffer_size = file.read((char*)m_buffer.data(), unpacked_size).gcount();
            break;
        }

        m_buffer_pos = 0;
        return true;
    }

public:
    LaxStream(std::ifstream& f) : file(f), m_buffer(0x10000),
        m_buffer_size(0), m_buffer_pos(0), m_eof(false) {}

    size_t read(uint8_t* buffer, size_t count) {
        size_t total_read = 0;
        while (count > 0) {
            if (m_buffer_pos >= m_buffer_size) {
                if (!readSegment())
                    break;
            }
            size_t available = std::min(count, m_buffer_size - m_buffer_pos);
            memcpy(buffer + total_read, m_buffer.data() + m_buffer_pos, available);
            m_buffer_pos += available;
            total_read += available;
            count -= available;
        }
        return total_read;
    }
};

class LaxExtractor {
private:
    std::ifstream file;
    std::vector<FileEntry> entries;

    bool readIndex() {
        // 获取文件大小
        file.seekg(0, std::ios::end);
        auto fileSize = file.tellg();
        std::cout << "File size: 0x" << std::hex << fileSize << std::dec << std::endl;

        // 定位到文件末尾前0x28字节
        file.seekg(-0x28, std::ios::end);
        auto indexPos = file.tellg();
        std::cout << "Index position: 0x" << std::hex << indexPos << std::dec << std::endl;

        // 读取索引头
        char signature[9] = { 0 };  // 多一个字节用于字符串结束符
        file.read(signature, 8);
        std::cout << "Signature: " << signature << std::endl;
        std::cout << "Signature bytes: ";
        for (int i = 0; i < 8; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                << (int)(unsigned char)signature[i] << " ";
        }
        std::cout << std::dec << std::endl;

        if (memcmp(signature, "$LapI_", 6) != 0) {
            std::cerr << "Invalid signature!" << std::endl;
            return false;
        }

        uint32_t count, indexOffset, unpackedSize, packedSize;
        file.read((char*)&count, 4);
        file.read((char*)&indexOffset, 4);
        file.read((char*)&unpackedSize, 4);
        file.read((char*)&packedSize, 4);

        std::cout << "Index info:" << std::endl
            << "  File count: " << count << std::endl
            << "  Index offset: 0x" << std::hex << indexOffset << std::endl
            << "  Unpacked size: 0x" << unpackedSize << std::endl
            << "  Packed size: 0x" << packedSize << std::dec << std::endl;

        // 读取剩余的填充字节并显示
        char padding[16];
        file.read(padding, 16);
        std::cout << "Padding bytes: ";
        for (int i = 0; i < 16; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                << (int)(unsigned char)padding[i] << " ";
        }
        std::cout << std::dec << std::endl;

        // 读取压缩的索引数据
        std::cout << "Seeking to index offset: 0x" << std::hex << indexOffset << std::dec << std::endl;
        file.seekg(indexOffset, std::ios::beg);
        if (file.fail()) {
            std::cerr << "Failed to seek to index offset!" << std::endl;
            return false;
        }

        // 检查索引数据开头
        char indexHeader[4];
        file.read(indexHeader, 4);
        std::cout << "Index data header: ";
        for (int i = 0; i < 4; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                << (int)(unsigned char)indexHeader[i] << " ";
        }
        std::cout << std::dec << std::endl;
        file.seekg(indexOffset, std::ios::beg);  // 重新定位回索引开始处

        // 解压索引数据
        std::vector<uint8_t> index(unpackedSize);
        LaxStream laxStream(file);
        size_t readSize = laxStream.read(index.data(), unpackedSize);
        if (readSize != unpackedSize) {
            std::cerr << "Failed to read index data! Expected " << unpackedSize
                << " bytes, got " << readSize << std::endl;
            return false;
        }

        // 保存解压后的索引数据到文件
        std::ofstream indexFile("index_dump.bin", std::ios::binary);
        if (!indexFile) {
            std::cerr << "Failed to create index dump file!" << std::endl;
            return false;
        }
        indexFile.write((char*)index.data(), index.size());
        indexFile.close();
        std::cout << "Index data dumped to 'index_dump.bin'" << std::endl;

        // 检查索引条目
        size_t pos = 0;
        for (uint32_t i = 0; i < count; ++i) {
            std::cout << "\nChecking entry " << i << " at offset 0x"
                << std::hex << pos << std::dec << std::endl;

            if (memcmp(&index[pos], "$LapF_", 6) != 0) {
                std::cerr << "Invalid entry signature at position " << i << std::endl;
                return false;
            }

            FileEntry entry;
            entry.name = std::string((char*)&index[pos + 0x24]);
            entry.unpackedSize = *(uint32_t*)&index[pos + 0x10];
            entry.size = *(uint32_t*)&index[pos + 0x14];
            entry.offset = *(uint32_t*)&index[pos + 0x18] + 8;

            std::cout << "  Name: " << entry.name << std::endl
                << "  Offset: 0x" << std::hex << entry.offset << std::endl
                << "  Size: " << std::dec << entry.size << " -> "
                << entry.unpackedSize << " bytes" << std::endl;

            entries.push_back(entry);
            pos += 0x128;
        }

        return true;
    }

    bool extractFile(const FileEntry& entry, const fs::path& outPath) {
        std::ofstream outFile(outPath, std::ios::binary);
        if (!outFile) return false;

        // 创建足够大的缓冲区
        std::vector<uint8_t> buffer(0x10000);  // 64KB缓冲区
        size_t remaining = entry.unpackedSize;

        file.seekg(entry.offset, std::ios::beg);
        LaxStream laxStream(file);

        while (remaining > 0) {
            size_t to_read = std::min(remaining, buffer.size());
            size_t read = laxStream.read(buffer.data(), to_read);
            if (read == 0) break;

            outFile.write((char*)buffer.data(), read);
            remaining -= read;
        }

        return remaining == 0;
    }

public:
    bool open(const std::string& filename) {
        file.open(filename, std::ios::binary);
        return file.is_open() && readIndex();
    }

    bool extract(const std::string& outDir) {
        if (!file.is_open()) return false;

        fs::create_directories(outDir);

        std::cout << "Found " << entries.size() << " files\n" << std::endl;

        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& entry = entries[i];
            fs::path outPath = fs::path(outDir) / entry.name;
            fs::create_directories(outPath.parent_path());

            // 更详细的信息输出
            std::cout << "[" << (i + 1) << "/" << entries.size() << "] "
                << entry.name << "\n"
                << "    Offset: 0x" << std::hex << std::setw(8) << std::setfill('0')
                << entry.offset << "\n"
                << "    Packed Size: " << std::dec << entry.size << " bytes\n"
                << "    Unpacked Size: " << entry.unpackedSize << " bytes\n"
                << "    Compression Ratio: "
                << std::fixed << std::setprecision(2)
                << (entry.size * 100.0 / entry.unpackedSize) << "%"
                << std::endl;

            if (!extractFile(entry, outPath)) {
                std::cerr << "Failed to extract: " << entry.name << std::endl;
                continue;
            }
        }

        std::cout << "\nExtraction completed successfully" << std::endl;
        return true;
    }

    ~LaxExtractor() {
        if (file.is_open()) file.close();
    }
};

class LaxPacker {
private:
    static const uint32_t MAX_CHUNK_SIZE = 32 * 1024;  // 32KB
    std::vector<FileEntry> entries;
    std::map<std::string, FileEntry> originalEntries;
    std::ofstream outFile;
    uint8_t originalPadding[16];  // 新增：存储原包的尾部16字节

    // 写入单个数据块
    void writeChunk(const uint8_t* data, size_t size) {
        // 写入段头
        outFile.write("_AF", 3);         // _AF标识
        outFile.write("1", 1);           // 方法1（LZSS）

        std::vector<uint8_t> output;
        output.reserve(size + (size + 7) / 8);  // 为数据和控制位预留空间

        size_t pos = 0;
        while (pos < size) {
            // 每8字节一组，先写入控制字节
            uint8_t control = 0xFF;  // 全部标记为直接字节
            output.push_back(control);

            // 写入实际数据
            for (int i = 0; i < 8 && pos < size; ++i, ++pos) {
                output.push_back(data[pos]);
            }
        }

        // 写入块信息
        uint16_t chunk_size = output.size() + 10;  // 数据大小 + 头部大小(10)
        uint16_t final_size = 0;
        uint16_t unpacked_size = size;

        outFile.write((char*)&chunk_size, 2);
        outFile.write((char*)&final_size, 2);
        outFile.write((char*)&unpacked_size, 2);

        // 写入数据
        outFile.write((char*)output.data(), output.size());
    }

    bool loadOriginalIndex(const std::string& originalLax) {
        std::ifstream file(originalLax, std::ios::binary);
        if (!file) return false;

        // 定位到文件末尾前0x28字节
        file.seekg(-0x28, std::ios::end);

        // 读取索引头
        char signature[8];
        file.read(signature, 8);
        if (memcmp(signature, "$LapI__", 6) != 0) return false;

        uint32_t count, indexOffset, unpackedSize, packedSize;
        file.read((char*)&count, 4);
        file.read((char*)&indexOffset, 4);
        file.read((char*)&unpackedSize, 4);
        file.read((char*)&packedSize, 4);

        // 读取并保存填充字节
        file.read((char*)originalPadding, 16);  // 保存原包的填充字节

        // 读取并解压索引数据
        file.seekg(indexOffset);
        std::vector<uint8_t> index(unpackedSize);
        LaxStream laxStream(file);
        size_t readSize = laxStream.read(index.data(), unpackedSize);
        if (readSize != unpackedSize) {
            std::cerr << "Failed to read index data! Expected " << unpackedSize
                << " bytes, got " << readSize << std::endl;
            return false;
        }

        // 解析索引条目
        size_t pos = 0;
        while (pos + 0x128 <= index.size()) {
            if (memcmp(&index[pos], "$LapF__", 6) != 0) break;

            FileEntry entry;
            // 复制保留字节
            memcpy(entry.reserved1, &index[pos + 0x08], 8);
            memcpy(entry.reserved2, &index[pos + 0x1C], 8);

            entry.unpackedSize = *(uint32_t*)&index[pos + 0x10];
            entry.size = *(uint32_t*)&index[pos + 0x14];
            entry.offset = *(uint32_t*)&index[pos + 0x18];

            // 读取文件名
            entry.name = std::string((char*)&index[pos + 0x24]);

            // 存储条目
            originalEntries[entry.name] = entry;

            pos += 0x128;
        }

        return true;
    }

    void processFile(const fs::path& filepath, const std::string& relativePath) {
        std::ifstream inFile(filepath, std::ios::binary);
        if (!inFile) {
            std::cerr << "Failed to open: " << filepath << std::endl;
            return;
        }

        // 记录文件起始位置，需要加上文件头的8字节
        uint32_t fileOffset = static_cast<uint32_t>(outFile.tellp()) - 8;  // 减去文件头大小

        // 获取文件大小
        inFile.seekg(0, std::ios::end);
        uint32_t totalSize = static_cast<uint32_t>(inFile.tellg());
        inFile.seekg(0);

        std::cout << "Processing: " << relativePath
            << " (Size: " << totalSize << " bytes)" << std::endl;

        // 分块处理
        std::vector<uint8_t> buffer(MAX_CHUNK_SIZE);
        uint32_t processedSize = 0;
        while (processedSize < totalSize) {
            uint32_t toRead = std::min(MAX_CHUNK_SIZE, totalSize - processedSize);
            inFile.read((char*)buffer.data(), toRead);
            writeChunk(buffer.data(), toRead);
            processedSize += toRead;
        }

        // 记录文件信息
        FileEntry entry;
        entry.name = relativePath;
        std::replace(entry.name.begin(), entry.name.end(), '/', '\\');
        entry.offset = fileOffset;  // 这里使用正确的偏移
        entry.unpackedSize = totalSize;
        entry.size = static_cast<uint32_t>(outFile.tellp()) - fileOffset - 8;  // 计算压缩大小时也要考虑文件头

        // 复制原包中的保留字节
        auto it = originalEntries.find(entry.name);
        if (it != originalEntries.end()) {
            memcpy(entry.reserved1, it->second.reserved1, 8);
            memcpy(entry.reserved2, it->second.reserved2, 8);
        }
        else {
            memset(entry.reserved1, 0, 8);
            memset(entry.reserved2, 0, 8);
        }

        entries.push_back(entry);
    }

    void processDirectory(const std::string& inputDir) {
        std::vector<fs::path> files;
        for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
            if (entry.is_regular_file()) {
                std::string relativePath = entry.path().lexically_relative(inputDir).string();
                std::replace(relativePath.begin(), relativePath.end(), '/', '\\');

                // 检查文件是否在原包中存在
                auto it = originalEntries.find(relativePath);
                if (it != originalEntries.end()) {
                    files.push_back(entry.path());
                }
                else {
                    std::cout << "Skipping new file: " << relativePath << std::endl;
                }
            }
        }

        // 按原包中的顺序排序文件
        std::sort(files.begin(), files.end(),
            [this, &inputDir](const fs::path& a, const fs::path& b) {
                std::string relA = a.lexically_relative(inputDir).string();
                std::string relB = b.lexically_relative(inputDir).string();
                std::replace(relA.begin(), relA.end(), '/', '\\');
                std::replace(relB.begin(), relB.end(), '/', '\\');
                return originalEntries[relA].offset < originalEntries[relB].offset;
            });

        // 处理文件
        for (const auto& file : files) {
            std::string relativePath = file.lexically_relative(inputDir).string();
            std::replace(relativePath.begin(), relativePath.end(), '/', '\\');
            processFile(file, relativePath);
        }
    }

    void writeIndex() {
        // 准备索引数据
        std::vector<uint8_t> indexData;
        for (const auto& entry : entries) {
            // 写入标识
            indexData.insert(indexData.end(), { '$','L','a','p','F','_','_',0 });

            // 写入第一组保留字节
            indexData.insert(indexData.end(),
                entry.reserved1,
                entry.reserved1 + 8);

            // 写入文件信息
            uint32_t values[] = {
                entry.unpackedSize,
                entry.size,
                entry.offset
            };
            indexData.insert(indexData.end(),
                (uint8_t*)values, (uint8_t*)(values + 3));

            // 写入第二组保留字节
            indexData.insert(indexData.end(),
                entry.reserved2,
                entry.reserved2 + 8);

            // 写入文件名
            size_t namePos = indexData.size();
            indexData.resize(namePos + 0x104, 0);
            std::copy(entry.name.begin(),
                entry.name.begin() + std::min(entry.name.length(), size_t(0x103)),
                indexData.begin() + namePos);
        }

        // 记录索引偏移
        uint32_t indexOffset = static_cast<uint32_t>(outFile.tellp());

        // 压缩并写入索引数据
        size_t indexPos = 0;
        while (indexPos < indexData.size()) {
            uint32_t chunkSize = static_cast<uint32_t>(
                std::min(static_cast<size_t>(MAX_CHUNK_SIZE),
                    indexData.size() - indexPos));
            writeChunk(&indexData[indexPos], chunkSize);
            indexPos += chunkSize;
        }

        // 计算索引大小
        uint32_t indexSize = static_cast<uint32_t>(outFile.tellp()) - indexOffset;

        // 写入索引信息
        outFile.write("$LapI__", 7);
        outFile.write("\0", 1);
        uint32_t count = entries.size();
        outFile.write((char*)&count, 4);
        outFile.write((char*)&indexOffset, 4);
        uint32_t unpackedSize = indexData.size();
        outFile.write((char*)&unpackedSize, 4);
        outFile.write((char*)&indexSize, 4);

        // 写入16字节的填充数据（使用原包的填充字节）
        outFile.write((char*)originalPadding, 16);
    }

public:
    bool pack(const std::string& inputDir, const std::string& outputFile,
        const std::string& originalLax) {
        if (!loadOriginalIndex(originalLax)) {
            std::cerr << "Failed to load original LAX index" << std::endl;
            return false;
        }

        outFile.open(outputFile, std::ios::binary);
        if (!outFile) {
            std::cerr << "Failed to create output file" << std::endl;
            return false;
        }

        // 写入文件头
        outFile.write("$LapH__", 7);
        outFile.write("\0", 1);

        // 处理文件
        processDirectory(inputDir);

        // 写入索引
        writeIndex();

        outFile.close();
        return true;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Made by julixian 2025.01.10" << std::endl;
        std::cout << "Usage:\n"
            << "  Extract: " << argv[0] << " extract <input.lax> <output_dir>\n"
            << "  Repack: " << argv[0] << " pack <input_dir> <output.lax> <original.lax>\n";
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "extract") {
        // 解包模式
        if (argc != 4) {
            std::cout << "Extract usage: " << argv[0] << " extract <input.lax> <output_dir>" << std::endl;
            return 1;
        }

        LaxExtractor extractor;
        if (!extractor.open(argv[2])) {
            std::cerr << "Failed to open LAX file" << std::endl;
            return 1;
        }

        if (!extractor.extract(argv[3])) {
            std::cerr << "Failed to extract files" << std::endl;
            return 1;
        }

        //std::cout << "Extraction completed successfully" << std::endl;
    }
    else if (mode == "pack") {
        // 打包模式
        if (argc != 5) {
            std::cout << "Repack usage: " << argv[0] << " pack <input_dir> <output.lax> <original.lax>" << std::endl;
            return 1;
        }

        LaxPacker packer;
        if (!packer.pack(argv[2], argv[3], argv[4])) {
            std::cerr << "Failed to create LAX file" << std::endl;
            return 1;
        }

        std::cout << "LAX file created successfully" << std::endl;
    }
    else {
        std::cout << "Invalid mode. Use extract for extract or pack for create." << std::endl;
        return 1;
    }

    return 0;
}
