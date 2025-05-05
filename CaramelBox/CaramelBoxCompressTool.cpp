#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

// === 解压相关类和函数 ===
class BitStream {
private:
    std::istream& input;
    uint32_t bits;
    int cached_bits;

public:
    BitStream(std::istream& in) : input(in), bits(0), cached_bits(0) {}

    void reset() {
        bits = 0;
        cached_bits = 0;
    }

    int getBits(int count) {
        while (cached_bits < count) {
            char b1, b2;
            input.get(b1);
            if (input.eof()) return -1;

            bits = (bits << 8) | (unsigned char)b1;
            cached_bits += 8;

            input.get(b2);
            if (!input.eof()) {
                bits = (bits << 8) | (unsigned char)b2;
                cached_bits += 8;
            }
        }
        int mask = (1 << count) - 1;
        cached_bits -= count;
        return (bits >> cached_bits) & mask;
    }

    int getNextBit() {
        return getBits(1);
    }
};

// === 压缩相关类和函数 ===
class BitWriter {
private:
    std::vector<uint8_t>& output;

public:
    BitWriter(std::vector<uint8_t>& out) : output(out) {}

    void writeBits(uint32_t value, int bits) {
        for (int i = bits - 1; i >= 0; --i) {
            output.push_back((value >> i) & 1);
        }
    }

    void packToBytes(std::vector<uint8_t>& packed) {
        uint8_t current_byte = 0;
        int bit_count = 0;

        for (uint8_t bit : output) {
            current_byte = (current_byte << 1) | (bit & 1);
            bit_count++;

            if (bit_count == 8) {
                packed.push_back(current_byte);
                current_byte = 0;
                bit_count = 0;
            }
        }

        if (bit_count > 0) {
            current_byte <<= (8 - bit_count);
            packed.push_back(current_byte);
        }
    }
};

// === 共用函数 ===
int getLzeInteger(BitStream& bits) {
    int length = 0;
    for (int i = 0; i < 16; ++i) {
        if (bits.getNextBit() != 0)
            break;
        ++length;
    }
    int v = 1 << length;
    if (length > 0) {
        int additional = bits.getBits(length);
        if (additional == -1) return -1;
        v |= additional;
    }
    return v;
}

void writeLzeInteger(BitWriter& writer, uint32_t value) {
    int length = 0;
    uint32_t temp = value;
    while (temp > 1) {
        temp >>= 1;
        length++;
    }

    for (int i = 0; i < length; i++) {
        writer.writeBits(0, 1);
    }
    writer.writeBits(1, 1);

    if (length > 0) {
        writer.writeBits(value & ((1 << length) - 1), length);
    }
}

// === 解压函数 ===
bool unpackZeChunk(BitStream& bits, std::vector<uint8_t>& output, size_t& dst, int chunk_length) {
    size_t output_end = dst + chunk_length;

    while (dst < output_end) {
        int count = getLzeInteger(bits);
        if (count == -1) return false;

        while (--count > 0) {
            int data = bits.getBits(8);
            if (data == -1) return false;

            if (dst < output_end)
                output[dst++] = (uint8_t)data;
        }
        if (count > 0 || dst >= output_end) break;

        int offset = getLzeInteger(bits);
        if (offset == -1) return false;

        count = getLzeInteger(bits);
        if (count == -1) return false;

        size_t src = dst - offset;
        for (int i = 0; i < count; ++i) {
            if (dst >= output_end) break;
            output[dst++] = output[src++];
        }
    }

    return dst == output_end;
}

bool unpackLze(const std::string& input_path, const std::string& output_path) {
    std::ifstream file(input_path, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open input file: " << input_path << std::endl;
        return false;
    }

    char magic[2];
    file.read(magic, 2);
    if (magic[0] != 'l' || magic[1] != 'z') {
        std::cerr << "Invalid file format" << std::endl;
        return false;
    }

    uint32_t unpacked_size = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t b;
        file.read((char*)&b, 1);
        unpacked_size = (unpacked_size << 8) | b;
    }

    std::vector<uint8_t> output(unpacked_size);
    size_t dst = 0;
    BitStream bits(file);

    while (dst < unpacked_size) {
        char header[4];
        file.read(header, 4);
        if (file.eof()) break;

        if (header[0] != 'z' || header[1] != 'e') {
            std::cerr << "Invalid chunk header" << std::endl;
            return false;
        }

        int chunk_length = ((unsigned char)header[2] << 8) | (unsigned char)header[3];

        bits.reset();
        if (!unpackZeChunk(bits, output, dst, chunk_length)) {
            std::cerr << "Failed to unpack chunk" << std::endl;
            return false;
        }
    }

    std::ofstream outfile(output_path, std::ios::binary);
    if (!outfile) {
        std::cerr << "Cannot create output file: " << output_path << std::endl;
        return false;
    }

    outfile.write((char*)output.data(), output.size());
    return true;
}

// === 压缩函数 ===
bool packLze(const std::string& input_path, const std::string& output_path) {
    std::ifstream infile(input_path, std::ios::binary);
    if (!infile) {
        std::cerr << "Cannot open input file: " << input_path << std::endl;
        return false;
    }

    infile.seekg(0, std::ios::end);
    uint32_t file_size = infile.tellg();
    infile.seekg(0, std::ios::beg);

    std::vector<uint8_t> input_data(file_size);
    infile.read((char*)input_data.data(), file_size);

    std::ofstream outfile(output_path, std::ios::binary);
    if (!outfile) {
        std::cerr << "Cannot create output file: " << output_path << std::endl;
        return false;
    }

    outfile.put('l');
    outfile.put('z');

    outfile.put((file_size >> 24) & 0xFF);
    outfile.put((file_size >> 16) & 0xFF);
    outfile.put((file_size >> 8) & 0xFF);
    outfile.put(file_size & 0xFF);

    const size_t CHUNK_SIZE = 16 * 1024;
    size_t pos = 0;

    while (pos < file_size) {
        size_t current_chunk_size = std::min(CHUNK_SIZE, file_size - pos);

        outfile.put('z');
        outfile.put('e');
        outfile.put((current_chunk_size >> 8) & 0xFF);
        outfile.put(current_chunk_size & 0xFF);

        std::vector<uint8_t> bits;
        BitWriter writer(bits);

        writeLzeInteger(writer, current_chunk_size + 1);

        for (size_t i = 0; i < current_chunk_size; i++) {
            writer.writeBits(input_data[pos + i], 8);
        }

        std::vector<uint8_t> packed_data;
        writer.packToBytes(packed_data);

        outfile.write((char*)packed_data.data(), packed_data.size());

        pos += current_chunk_size;
    }

    return true;
}

// 创建目录(如果不存在)
void ensureDirectory(const fs::path& path) {
    if (!fs::exists(path)) {
        fs::create_directories(path);
    }
}

// 处理单个文件
bool processFile(const fs::path& input_path, const fs::path& output_path, bool isCompress) {
    // 确保输出目录存在
    ensureDirectory(output_path.parent_path());

    if (isCompress) {
        return packLze(input_path.string(), output_path.string());
    }
    else {
        return unpackLze(input_path.string(), output_path.string());
    }
}

// 处理目录
void processDirectory(const fs::path& input_dir, const fs::path& output_dir, bool isCompress) {
    // 确保输出目录存在
    ensureDirectory(output_dir);

    // 计数器
    int success_count = 0;
    int fail_count = 0;
    int skip_count = 0;

    // 遍历输入目录
    for (const auto& entry : fs::recursive_directory_iterator(input_dir)) {
        if (!entry.is_regular_file()) continue;

        fs::path input_path = entry.path();
        fs::path relative_path = fs::relative(input_path, input_dir);
        fs::path output_path = output_dir / relative_path;

        // 检查文件扩展名
        std::string ext = input_path.extension().string();

        // 只处理.scb文件
        if (ext == ".scb" || ext == ".$$$") {
            if (isCompress) {
                std::cout << "Compressing: " << relative_path.string() << std::endl;
            }
            else {
                std::cout << "Decompressing: " << relative_path.string() << std::endl;
            }

            if (processFile(input_path, output_path, isCompress)) {
                success_count++;
            }
            else {
                fail_count++;
            }
        }
        else {
            skip_count++;
        }
    }

    // 输出统计信息
    std::cout << "\nProcessing complete!\n"
        << "Successfully processed: " << success_count << " files\n"
        << "Failed to process: " << fail_count << " files\n"
        << "Skipped: " << skip_count << " files\n";
}

void printUsage(const char* programName) {
    std::cout << "Made by julixian 2025.05.06" << std::endl;
    std::cout << "Usage: " << programName << " [options] <input_directory> <output_directory>\n"
        << "Options:\n"
        << "  -c    Compress .scb/.$$$ files\n"
        << "  -d    Decompress .scb/$$$ files\n"
        << "Example:\n"
        << "  " << programName << " -c input_dir output_dir    # compress all .scb/.$$$ files\n"
        << "  " << programName << " -d input_dir output_dir    # decompress all .scb/.$$$ files\n";
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string option = argv[1];
    fs::path input_dir = argv[2];
    fs::path output_dir = argv[3];

    // 检查输入目录是否存在
    if (!fs::exists(input_dir)) {
        std::cerr << "Error: Input directory does not exist: " << input_dir << std::endl;
        return 1;
    }

    if (!fs::is_directory(input_dir)) {
        std::cerr << "Error: Input path is not a directory: " << input_dir << std::endl;
        return 1;
    }

    try {
        if (option == "-c") {
            std::cout << "Starting compression...\n";
            processDirectory(input_dir, output_dir, true);
        }
        else if (option == "-d") {
            std::cout << "Starting decompression...\n";
            processDirectory(input_dir, output_dir, false);
        }
        else {
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
