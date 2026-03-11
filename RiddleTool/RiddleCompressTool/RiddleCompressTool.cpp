#include <Windows.h>
#include <CLI/CLI.hpp>

import std;
import Tool;

namespace fs = std::filesystem;

class CmpReader {
private:
    std::ifstream& input;
    std::vector<uint8_t> output;
    int srcTotal;
    int srcCount = 0;
    int bits = 0;
    int cachedBits = 0;

    int getBits(int count)
    {
        while (cachedBits < count) {
            if (srcCount >= srcTotal) {
                return -1;
            }
            char b = 0;
            if (!input.read(&b, 1)) {
                return -1;
            }
            bits = (bits << 8) | (uint8_t)b;
            cachedBits += 8;
            srcCount++;
        }
        int mask = (1 << count) - 1;
        cachedBits -= count;
        return (bits >> cachedBits) & mask;
    }

public:
    CmpReader(std::ifstream& file, int srcSize, int dstSize)
        : input(file), output((size_t)dstSize), srcTotal(srcSize)
    {}

    void unpack()
    {
        int dst = 0;
        std::vector<uint8_t> shift(0x800, 0x20);
        int edi = 0x7ef;

        while (dst < (int)output.size()) {
            int bit = getBits(1);
            if (bit == -1) {
                break;
            }

            if (bit == 1) {
                int data = getBits(8);
                if (data == -1) {
                    break;
                }
                output[(size_t)dst++] = (uint8_t)data;
                shift[(size_t)edi++] = (uint8_t)data;
                edi &= 0x7ff;
            }
            else {
                int offset = getBits(11);
                if (offset == -1) {
                    break;
                }
                int count = getBits(4);
                if (count == -1) {
                    break;
                }
                count += 2;

                for (int i = 0; i < count; ++i) {
                    uint8_t data = shift[(size_t)((offset + i) & 0x7ff)];
                    output[(size_t)dst++] = data;
                    shift[(size_t)edi++] = data;
                    edi &= 0x7ff;
                    if (dst == (int)output.size()) {
                        return;
                    }
                }
            }
        }
    }

    const std::vector<uint8_t>& getData() const
    {
        return output;
    }
};

class BitWriter {
private:
    std::ofstream& output;
    int bitBuffer = 0;
    int bitCount = 0;

public:
    BitWriter(std::ofstream& out) : output(out) {}

    void writeBits(int bits, int count) {
        bitBuffer = (bitBuffer << count) | bits;
        bitCount += count;
        while (bitCount >= 8) {
            bitCount -= 8;
            output.put(static_cast<char>((bitBuffer >> bitCount) & 0xFF));
        }
    }

    void flush() {
        if (bitCount > 0) {
            writeBits(0, 8 - bitCount);
        }
    }
};

bool isCmp1File(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    char signature[4]{};
    file.read(signature, 4);
    if (!file) {
        return false;
    }
    return signature[0] == 'C' && signature[1] == 'M' && signature[2] == 'P' && signature[3] == '1';
}

void decompressFile(const fs::path& inputPath, const fs::path& outputPath)
{
    std::ifstream file(inputPath, std::ios::binary);
    if (!file.is_open()) {
        std::println("failed to open file: {}", wide2Ascii(inputPath.native(), CP_UTF8));
        return;
    }

    char signature[4]{};
    file.read(signature, 4);
    if (!file || signature[0] != 'C' || signature[1] != 'M' || signature[2] != 'P' || signature[3] != '1') {
        std::println("invalid CMP1 signature: {}", wide2Ascii(inputPath.native(), CP_UTF8));
        return;
    }

    uint32_t unpackedSize = 0;
    file.read((char*)&unpackedSize, 4);

    uint32_t reserved = 0;
    file.read((char*)&reserved, 4);

    file.seekg(0, std::ios::end);
    std::streamoff endPos = file.tellg();
    int packedSize = (int)endPos - 12;
    if (packedSize < 0) {
        std::println("packed size negative for file: {}", wide2Ascii(inputPath.native(), CP_UTF8));
        return;
    }
    file.seekg(12, std::ios::beg);

    CmpReader reader(file, packedSize, (int)unpackedSize);
    reader.unpack();

    const std::vector<uint8_t>& data = reader.getData();

    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile.is_open()) {
        std::println("failed to create file: {}", wide2Ascii(outputPath.native(), CP_UTF8));
        return;
    }
    if (!data.empty()) {
        outFile.write((const char*)data.data(), (std::streamsize)data.size());
    }

    std::println("decompressed {} -> {}", wide2Ascii(inputPath.native(), CP_UTF8), wide2Ascii(outputPath.native(), CP_UTF8));
}

void findLongestMatch(const std::vector<uint8_t>& buffer, int current_pos,
    const std::vector<uint8_t>& window, int window_pos,
    int& out_offset, int& out_len)
{
    int best_len = 0;
    int best_offset = 0;
    int max_len = std::min<int>(17, static_cast<int>(buffer.size()) - current_pos);

    if (max_len < 2) {
        out_len = 0;
        return;
    }

    for (int offset = 0; offset < 2048; ++offset) {
        int len = 0;
        int read_pos = offset;
        uint8_t match_buf[17];

        while (len < max_len) {
            uint8_t c;
            bool found_overlap = false;

            for (int k = len - 1; k >= 0; --k) {
                if (read_pos == ((window_pos + k) & 0x7ff)) {
                    c = match_buf[k];
                    found_overlap = true;
                    break;
                }
            }

            if (!found_overlap) {
                c = window[read_pos];
            }

            if (c != buffer[current_pos + len]) {
                break;
            }

            match_buf[len] = c;
            read_pos = (read_pos + 1) & 0x7ff;
            len++;
        }

        if (len > best_len) {
            best_len = len;
            best_offset = offset;
            if (best_len == 17) break;
        }
    }

    if (best_len >= 2) {
        out_offset = best_offset;
        out_len = best_len;
    }
    else {
        out_len = 0;
    }
}

void compressFile(const fs::path& inputPath, const fs::path& outputPath)
{
    std::ifstream inFile(inputPath, std::ios::binary);
    if (!inFile.is_open()) {
        std::println("failed to open input file: {}", wide2Ascii(inputPath.native(), CP_UTF8));
        return;
    }

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());

    if (buffer.empty()) {
        std::println("skip empty file: {}", wide2Ascii(inputPath.native(), CP_UTF8));
        return;
    }

    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile.is_open()) {
        std::println("failed to create output file: {}", wide2Ascii(outputPath.native(), CP_UTF8));
        return;
    }

    outFile.write("CMP1", 4);

    uint32_t fileSize = (uint32_t)buffer.size();
    outFile.write((const char*)&fileSize, 4);

    uint32_t padding = 0;
    outFile.write((const char*)&padding, 4);

    BitWriter writer(outFile);

    std::vector<uint8_t> window(0x800, 0x20);
    int windowPos = 0x7ef;

    int i = 0;
    while (i < (int)buffer.size()) {
        int offset = 0;
        int len = 0;

        findLongestMatch(buffer, i, window, windowPos, offset, len);

        if (len >= 2) {
            writer.writeBits(0, 1);               // Match flag = 0
            writer.writeBits(offset, 11);         // 11-bit offset
            writer.writeBits(len - 2, 4);         // 4-bit count (length - 2)

            for (int k = 0; k < len; ++k) {
                window[windowPos] = buffer[i + k];
                windowPos = (windowPos + 1) & 0x7ff;
            }
            i += len;
        }
        else {
            writer.writeBits(1, 1);               // Literal flag = 1
            writer.writeBits(buffer[i], 8);       // 8-bit data

            window[windowPos] = buffer[i];
            windowPos = (windowPos + 1) & 0x7ff;
            i++;
        }
    }

    writer.flush();
    outFile.close();

    std::println("compressed {} -> {}", wide2Ascii(inputPath.native(), CP_UTF8), wide2Ascii(outputPath.native(), CP_UTF8));
}

void compressDir(const fs::path& inputDir, const fs::path& outputDir)
{
    for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        std::ifstream file(entry.path(), std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::println("failed to open file: {}", wide2Ascii(entry.path().native(), CP_UTF8));
            continue;
        }
        std::streamsize size = file.tellg();
        file.close();

        if (size <= 12) {
            std::println("skip compress (size <= 12): {}", wide2Ascii(entry.path().native(), CP_UTF8));
            continue;
        }

        fs::path relativePath = fs::relative(entry.path(), inputDir);
        fs::path outputPath = outputDir / relativePath;
        fs::create_directories(outputPath.parent_path());

        compressFile(entry.path(), outputPath);
    }
}

void decompressDir(const fs::path& inputDir, const fs::path& outputDir)
{
    for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        if (!isCmp1File(entry.path())) {
            std::println("skip decompress (not CMP1): {}", wide2Ascii(entry.path().native(), CP_UTF8));
            continue;
        }

        fs::path relativePath = fs::relative(entry.path(), inputDir);
        fs::path outputPath = outputDir / relativePath;
        fs::create_directories(outputPath.parent_path());

        decompressFile(entry.path(), outputPath);
    }
}

int main(int argc, char** argv)
{
    SetConsoleOutputCP(CP_UTF8);

    CLI::App app("Made by julixian 2026.03.11", "RiddleCompressTool");
    argv = app.ensure_utf8(argv);
    app.set_help_all_flag("-a");
    app.require_subcommand(1);

    fs::path inputDir;
    fs::path outputDir;

    auto decompressCmd = app.add_subcommand("decompress");
    decompressCmd->alias("-d");
    decompressCmd->add_option("inputDir", inputDir, "input directory")->required();
    decompressCmd->add_option("outputDir", outputDir, "output directory")->required();

    auto compressCmd = app.add_subcommand("compress");
    compressCmd->alias("-c");
    compressCmd->add_option("inputDir", inputDir, "input directory")->required();
    compressCmd->add_option("outputDir", outputDir, "output directory")->required();

    CLI11_PARSE(app, argc, argv);


    if (!fs::exists(inputDir) || !fs::is_directory(inputDir)) {
        std::println("input directory not exists or not directory: {}", wide2Ascii(inputDir.native(), CP_UTF8));
        return 1;
    }

    if (decompressCmd->parsed()) {
        decompressDir(inputDir, outputDir);
    }
    else if (compressCmd->parsed()) {
        compressDir(inputDir, outputDir);
    }

    return 0;
}
