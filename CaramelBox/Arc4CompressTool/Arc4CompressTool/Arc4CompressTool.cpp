#define _CRT_SECURE_NO_WARNINGS
#include <boost/endian.hpp>
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <iostream>
#include <regex>
#include "arc4common.h"

namespace fs = std::filesystem;

std::string WideToAscii(const std::wstring& wide, UINT CodePage) {
    int len = WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return "";
    std::string ascii(len, '\0');
    WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
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

std::string AsciiToAscii(const std::string& ascii, UINT src, UINT dst) {
    return WideToAscii(AsciiToWide(ascii, src), dst);
}

#pragma pack(1)
struct ARC4HDR {
    uint8_t signature[4];     // "ARC4"
    uint32_t unknown1;
    uint32_t toc_length;//索引长度
    uint32_t block_size;         // 2048 - block size
    uint32_t entry_count;//文件数目
    uint32_t header_length;    // 48
    uint32_t table_length;     // 文件数据表
    uint32_t filenames_offset; // header_length + table_length
    uint32_t filenames_length;
    uint32_t subtoc_offset;   // 如果有subentry_count不为1的文件，每个entry的各个文件的偏移专门存在这里
    uint32_t subtoc_length; //如果subentry_count都为1，这个长度是0
    uint32_t data_base;
};

struct ARC4ENTRY {
    uint8_t filename_offset[3]; // in 2-byte blocks
    uint8_t filename_length;
    uint8_t subentry_count;
    uint8_t offset[3];          // in 2048-byte blocks (relative to data_base)
};

struct ARC4DATAHDR {
    uint32_t length_blocks; // in 2048-byte blocks
    uint32_t length;
    uint32_t length2;       // ??
    uint32_t unknown;       // padding
};

#pragma pack()

void save_entry(std::ifstream& ifs, uint32_t offset, const fs::path& filename, uint32_t block_size) {
    offset *= block_size;

    ARC4DATAHDR datahdr;
    ifs.seekg(offset);
    ifs.read((char*)&datahdr, sizeof(datahdr));

    boost::endian::big_to_native_inplace(datahdr.length_blocks);
    boost::endian::big_to_native_inplace(datahdr.length);
    boost::endian::big_to_native_inplace(datahdr.length2);

    std::vector<uint8_t> data(datahdr.length);
    ifs.read((char*)data.data(), data.size());

    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs) {
        std::cout << "Can not open file: " << filename << std::endl;
        return;
    }
    ofs.write((char*)data.data(), data.size());
    ofs.close();
}

std::vector<uint8_t> Get_entry(std::ifstream& ifs, uint32_t offset, uint32_t block_size) {
    offset *= block_size;
    ARC4DATAHDR datahdr;
    ifs.seekg(offset);
    ifs.read((char*)&datahdr, sizeof(datahdr));
    boost::endian::big_to_native_inplace(datahdr.length_blocks);
    boost::endian::big_to_native_inplace(datahdr.length);
    boost::endian::big_to_native_inplace(datahdr.length2);
    std::vector<uint8_t> data(datahdr.length);
    ifs.read((char*)data.data(), data.size());
    return data;
}

void RepackARC4(std::string& in_filename, fs::path& input_dir, std::string& out_filename, bool compress_type)
{
    std::ofstream ofs(out_filename, std::ios::binary);
    if (!ofs) {
        std::cout << "Can not open file: " << out_filename << std::endl;
        return;
    }

    std::ifstream ifs(in_filename, std::ios::binary);
    ARC4HDR hdr;
    ifs.read((char*)&hdr, sizeof(hdr));
    std::vector<uint8_t> compressedIndex(hdr.toc_length);
    ifs.read((char*)compressedIndex.data(), compressedIndex.size());
    std::vector<uint8_t> index = uncompress_sequence(compressedIndex);
    uint32_t orgDatabase = hdr.data_base;
    hdr.data_base = (hdr.header_length + index.size() + hdr.block_size - 1) / hdr.block_size + 1;
    ofs.seekp(hdr.data_base * hdr.block_size);
    uint8_t* toc_buff = index.data();

    uint8_t* subtoc_buff = toc_buff + hdr.subtoc_offset - hdr.header_length;

    ARC4ENTRY* entries = (ARC4ENTRY*)toc_buff;
    char* filenames = (char*)(toc_buff + hdr.table_length);

    for (uint32_t i = 0; i < hdr.entry_count; i++) {
        uint32_t filename_offset = get_stupid_long(entries[i].filename_offset) * 2;
        uint32_t offset = get_stupid_long(entries[i].offset);

        std::wstring filename = AsciiToWide(std::string(filenames + filename_offset, entries[i].filename_length), 932);

        if (entries[i].subentry_count == 1) {
            fs::path newFilePath = input_dir / filename;
            std::vector<uint8_t> replaceBuf;
            if (fs::exists(newFilePath)) {
                std::ifstream replace(newFilePath, std::ios::binary);
                if (!replace) {
                    std::cout << "Can not open file: " << newFilePath << std::endl;
                    return;
                }
                else {
                    std::cout << "Processing replacement: " << newFilePath << std::endl;
                    replaceBuf.resize(fs::file_size(newFilePath));
                    replace.read((char*)replaceBuf.data(), replaceBuf.size());
                }
            }
            else {
                std::cout << "Will not replace: " << WideToAscii(filename, 0) << std::endl;
                replaceBuf = Get_entry(ifs, orgDatabase + offset, hdr.block_size);
            }
            ofs.seekp(hdr.block_size - ofs.tellp() % hdr.block_size, std::ios::cur);
            std::cout << "New offset at: 0x" << std::hex << (size_t)ofs.tellp() << std::endl;
            write_stupid_long(entries[i].offset, ofs.tellp() / hdr.block_size - hdr.data_base);
            ARC4DATAHDR dataHdr;
            dataHdr.length_blocks = boost::endian::native_to_big((uint32_t)(replaceBuf.size() + hdr.block_size - 1) / hdr.block_size);
            dataHdr.length = boost::endian::native_to_big((uint32_t)replaceBuf.size());
            dataHdr.length2 = boost::endian::native_to_big((uint32_t)replaceBuf.size());
            dataHdr.unknown = 0;
            ofs.write((char*)&dataHdr, sizeof(dataHdr));
            ofs.write((char*)replaceBuf.data(), replaceBuf.size());

        }
        else {
            offset *= 3;
            for (unsigned long j = 0; j < entries[i].subentry_count; j++) {

                std::wstring subname = std::format(L"{}_{:03d}{}", fs::path(filename).stem().wstring(), j, fs::path(filename).extension().wstring());

                fs::path newFilePath = input_dir / subname;
                std::vector<uint8_t> replaceBuf;
                if (fs::exists(newFilePath)) {
                    std::ifstream replace(newFilePath, std::ios::binary);
                    if (!replace) {
                        std::cout << "Can not open file: " << newFilePath << std::endl;
                        return;
                    }
                    else {
                        std::cout << "Processing replacement: " << newFilePath << std::endl;
                        replaceBuf.resize(fs::file_size(newFilePath));
                        replace.read((char*)replaceBuf.data(), replaceBuf.size());
                    }
                }
                else {
                    std::cout << "Will not replace: " << newFilePath << std::endl;
                    uint32_t real_offset = get_stupid_long(subtoc_buff + offset);
                    replaceBuf = Get_entry(ifs, orgDatabase + real_offset, hdr.block_size);
                }
                ofs.seekp(hdr.block_size - ofs.tellp() % hdr.block_size, std::ios::cur);
                std::cout << "New offset at: 0x" << std::hex << (size_t)ofs.tellp() << std::endl;
                write_stupid_long(subtoc_buff + offset, ofs.tellp() / hdr.block_size - hdr.data_base);
                ARC4DATAHDR dataHdr;
                dataHdr.length_blocks = boost::endian::native_to_big((uint32_t)(replaceBuf.size() + hdr.block_size - 1) / hdr.block_size);
                dataHdr.length = boost::endian::native_to_big((uint32_t)replaceBuf.size());
                dataHdr.length2 = boost::endian::native_to_big((uint32_t)replaceBuf.size());
                dataHdr.unknown = 0;
                ofs.write((char*)&dataHdr, sizeof(dataHdr));
                ofs.write((char*)replaceBuf.data(), replaceBuf.size());
                offset += 3;
            }
        }
    }
    ofs.seekp(hdr.header_length);
    std::vector<uint8_t> newIndex = compress_sequence(index, hdr.block_size, compress_type);
    ofs.write((char*)newIndex.data(), newIndex.size());
    hdr.toc_length = newIndex.size();
    ofs.seekp(0);
    ofs.write((char*)&hdr, sizeof(hdr));
    ofs.close();
}

std::vector<uint8_t> stringToCP932(const std::string& str) {
    std::vector<uint8_t> result;
    for (char c : str) {
        result.push_back(static_cast<uint8_t>(c));
    }
    return result;
}

void injectEVEM(const fs::path& inputBinPath, const fs::path& inputTxtPath, const fs::path& outputBinPath, bool compress_type) {
    std::ifstream inputBin(inputBinPath, std::ios::binary);
    std::ifstream inputTxt(inputTxtPath);
    std::ofstream outputBin(outputBinPath, std::ios::binary);

    if (!inputBin || !inputTxt || !outputBin) {
        std::cerr << "Error opening files: " << inputBinPath << " or " << inputTxtPath << " or " << outputBinPath << std::endl;
        return;
    }

    std::vector<std::string> translations;

    std::string line;
    std::regex lb1(R"(\[r\])");
    std::regex lb2(R"(\[n\])");
    while (std::getline(inputTxt, line)) {
        line = std::regex_replace(line, lb1, "\r");
        line = std::regex_replace(line, lb2, "\n");
        translations.push_back(line);
    }

    size_t translationIndex = 0;

    std::vector<uint8_t> compressedData(fs::file_size(inputBinPath));
    inputBin.read((char*)compressedData.data(), compressedData.size());
    EVEMHDR ehdr = *(EVEMHDR*)compressedData.data();
    ehdr.scriptBegin = boost::endian::big_to_native(ehdr.scriptBegin);
    ehdr.scriptLength = boost::endian::big_to_native(ehdr.scriptLength);
    if (ehdr.scriptBegin + ehdr.scriptLength != *(uint32_t*)&compressedData[0x16] || ehdr.magic != 0x4D455645) {
        throw std::runtime_error("Unknown EVEM header!");
    }
    compressedData.erase(compressedData.begin(), compressedData.begin() + 0x14);
    std::vector<uint8_t> buffer = uncompress_sequence(compressedData);

    for (uint32_t i = 0; i < ehdr.scriptBegin; i++) {
        if (*(uint16_t*)&buffer[i] == 0x1A80) {
            uint32_t offset = boost::endian::big_to_native(*(uint32_t*)&buffer[i + 2]);
            if (offset != 0 && (offset > ehdr.scriptLength || buffer[ehdr.scriptBegin + offset - 1] != 0x00)) {
                continue;
            }
            if (translationIndex >= translations.size()) {
                throw std::runtime_error("Not enough translations!");
            }
            uint32_t newOffset = boost::endian::native_to_big((uint32_t)buffer.size() - ehdr.scriptBegin);
            std::vector<uint8_t> textBytes = stringToCP932(translations[translationIndex++]);
            textBytes.push_back(0x00);
            buffer.insert(buffer.end(), textBytes.begin(), textBytes.end());
            *(uint32_t*)&buffer[i + 2] = newOffset;
            i += 5;
        }
    }

    if (translationIndex < translations.size()) {
        std::cout << "Warning: too many translations!" << std::endl;
        system("pause");
    }

    ehdr.scriptLength = boost::endian::native_to_big((uint32_t)buffer.size() - ehdr.scriptBegin);
    ehdr.scriptBegin = boost::endian::native_to_big(ehdr.scriptBegin);
    std::vector<uint8_t> compressedBuffer = compress_sequence(buffer, 0x8000, compress_type);

    outputBin.write((char*)&ehdr, sizeof(ehdr));
    outputBin.write((char*)compressedBuffer.data(), compressedBuffer.size());
    std::cout << "Write-back complete. Output saved to " << outputBinPath << std::endl;
}

void printUsage(fs::path progName) {
    std::cout << "exarc4 v1.0 by asmodean, patched by julixian 2025.08.22\n"
        << "Usage: \n"
        << "For extract: " << progName.filename().string() << " -e <input.bin> <output_dir> \n"
        << "For repack: " << progName.filename().string() << " -p <inputorg.bin> <input_new_files_dir> <output.bin> [--compress | -c]\n"
        << "For decompress: " << progName.filename().string() << " -d <input_dir> <output_dir>\n"
        << "For dump EVEM: " << progName.filename().string() << " -de <input_dir> <output_dir>\n"
        << "For compress: " << progName.filename().string() << " -c <input_dir> <output_dir> [--compress | -c]\n"
        << "For inject EVEM: " << progName.filename().string() << " -ie <input_orgbin_dir> <input_txt_dir> <output_dir> [--compress | -c]\n"
        << "--compress | -c: use LZ3 compress type. EVE-JONG need this option." << std::endl;
}

int main(int argc, char** argv) {

    try {
        if (argc < 2) {
            printUsage(argv[0]);
            return 1;
        }

        std::string mode(argv[1]);
        if (mode == "-p") {
            if (argc < 5) {
                printUsage(argv[0]);
                return 1;
            }
            std::string in_filename(argv[2]);
            fs::path input_dir(argv[3]);
            std::string out_filename(argv[4]);
            bool compress_type = false;
            if (argc >= 6 && (std::string(argv[5]) == "--compress" || std::string(argv[5]) == "-c")) {
                compress_type = true;
            }
            RepackARC4(in_filename, input_dir, out_filename, compress_type);
        }
        else if (mode == "-d") {
            if (argc < 4) {
                printUsage(argv[0]);
                return 1;
            }
            fs::path indir(argv[2]);
            fs::path odir(argv[3]);
            fs::create_directories(odir);
            for (const auto& entry : fs::directory_iterator(indir)) {
                if (!entry.is_regular_file())continue;
                std::cout << "Processing: " << entry.path() << std::endl;
                fs::path oFilePath = odir / fs::relative(entry.path(), indir);
                std::ifstream ifs(entry.path(), std::ios::binary);
                std::vector<uint8_t> compressedData(fs::file_size(entry.path()));
                ifs.read((char*)compressedData.data(), compressedData.size());
                std::vector<uint8_t> decompressedData = uncompress_sequence(compressedData);
                std::ofstream ofs(oFilePath, std::ios::binary);
                if (!ofs) {
                    std::cout << "Can not open file: " << oFilePath << std::endl;
                    continue;
                }
                ofs.write((char*)decompressedData.data(), decompressedData.size());
                ofs.close();
            }
        }
        else if (mode == "-de") {
            if (argc < 4) {
                printUsage(argv[0]);
                return 1;
            }
            fs::path indir(argv[2]);
            fs::path odir(argv[3]);
            fs::create_directories(odir);
            std::regex lb1(R"(\n)");
            std::regex lb2(R"(\r)");
            for (const auto& entry : fs::directory_iterator(indir)) {
                if (!entry.is_regular_file())continue;
                std::cout << "Processing: " << entry.path() << std::endl;
                fs::path oFilePath = odir / fs::relative(entry.path(), indir);
                std::ifstream ifs(entry.path(), std::ios::binary);
                std::vector<uint8_t> compressedData(fs::file_size(entry.path()));
                ifs.read((char*)compressedData.data(), compressedData.size());
                EVEMHDR ehdr = *(EVEMHDR*)compressedData.data();
                ehdr.scriptBegin = boost::endian::big_to_native(ehdr.scriptBegin);
                ehdr.scriptLength = boost::endian::big_to_native(ehdr.scriptLength);
                if (ehdr.scriptBegin + ehdr.scriptLength != *(uint32_t*)&compressedData[0x16] || ehdr.magic != 0x4D455645) {
                    throw std::runtime_error("Unknown EVEM header!");
                }
                compressedData.erase(compressedData.begin(), compressedData.begin() + 0x14);
                std::vector<uint8_t> decompressedData = uncompress_sequence(compressedData);
                std::ofstream ofs(oFilePath);
                if (!ofs) {
                    std::cout << "Can not open file: " << oFilePath << std::endl;
                    continue;
                }
                for (uint32_t i = 0; i < ehdr.scriptBegin; i++) {
                    if (*(uint16_t*)&decompressedData[i] == 0x1A80) {
                        uint32_t offset = boost::endian::big_to_native(*(uint32_t*)&decompressedData[i + 2]);
                        if (offset != 0 && (offset > ehdr.scriptLength || decompressedData[ehdr.scriptBegin + offset - 1] != 0x00)) {
                            continue;
                        }
                        std::string line((char*)&decompressedData[ehdr.scriptBegin + offset]);
                        line = std::regex_replace(line, lb1, "[n]");
                        line = std::regex_replace(line, lb2, "[r]");
                        ofs << line << std::endl;
                        i += 5;
                    }
                }
            }
        }
        else if (mode == "-c") {
            if (argc < 4) {
                printUsage(argv[0]);
                return 1;
            }
            bool compress_type = false;
            if (argc >= 5 && (std::string(argv[4]) == "--compress" || std::string(argv[4]) == "-c")) {
                compress_type = true;
            }
            fs::path indir(argv[2]);
            fs::path odir(argv[3]);
            fs::create_directories(odir);
            for (const auto& entry : fs::directory_iterator(indir)) {
                if (!entry.is_regular_file())continue;
                std::cout << "Processing: " << entry.path() << std::endl;
                fs::path oFilePath = odir / fs::relative(entry.path(), indir);
                std::ifstream ifs(entry.path(), std::ios::binary);
                std::vector<uint8_t> orgData(fs::file_size(entry.path()));
                ifs.read((char*)orgData.data(), orgData.size());
                std::vector<uint8_t> compressedData = compress_sequence(orgData, 0x8000, compress_type);// TODO: set maxChunckSize manually
                std::ofstream ofs(oFilePath, std::ios::binary);
                if (!ofs) {
                    std::cout << "Can not open file: " << oFilePath << std::endl;
                    continue;
                }
                ofs.write((char*)compressedData.data(), compressedData.size());
                ofs.close();
            }
        }
        else if (mode == "-ie") {
            if (argc < 5) {
                printUsage(argv[0]);
                return 1;
            }
            bool compress_type = false;
            if (argc >= 6 && (std::string(argv[5]) == "--compress" || std::string(argv[5]) == "-c")) {
                compress_type = true;
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
                        injectEVEM(entry.path(), txtPath, outputPath, compress_type);
                    }
                    else {
                        std::cerr << "Warning: No corresponding file found for " << relativePath << std::endl;
                    }
                }
            }
        }
        else if (mode == "-e") {
            if (argc < 4) {
                printUsage(argv[0]);
                return 1;
            }
            std::string in_filename(argv[2]);
            fs::path odir(argv[3]);
            fs::create_directories(odir);

            std::ifstream ifs(in_filename, std::ios::binary);

            ARC4HDR hdr;
            ifs.read((char*)&hdr, sizeof(hdr));
            std::vector<uint8_t> index;
            {
                std::vector<uint8_t> compressedIndex(hdr.toc_length);
                ifs.read((char*)compressedIndex.data(), compressedIndex.size());
                index = uncompress_sequence(compressedIndex);
            }

            uint8_t* toc_buff = index.data();
            uint8_t* subtoc_buff = toc_buff + hdr.subtoc_offset - hdr.header_length;

            ARC4ENTRY* entries = (ARC4ENTRY*)toc_buff;
            char* filenames = (char*)(toc_buff + hdr.table_length);

            for (uint32_t i = 0; i < hdr.entry_count; i++) {
                uint32_t filename_offset = get_stupid_long(entries[i].filename_offset) * 2;
                uint32_t offset = get_stupid_long(entries[i].offset);

                std::wstring filename = AsciiToWide(std::string(filenames + filename_offset, entries[i].filename_length), 932);

                if (entries[i].subentry_count == 1) {
                    std::cout << "Extracting: " << (odir / filename) << " at offset: 0x" << std::hex
                        << (size_t)(hdr.data_base + offset) * hdr.block_size << std::endl;
                    save_entry(ifs, hdr.data_base + offset, odir / filename, hdr.block_size);
                }
                else {
                    offset *= 3;

                    for (uint32_t j = 0; j < entries[i].subentry_count; j++) {

                        std::wstring subname = std::format(L"{}_{:03d}{}", fs::path(filename).stem().wstring(), j, fs::path(filename).extension().wstring());

                        uint32_t real_offset = get_stupid_long(subtoc_buff + offset);
                        offset += 3;
                        std::cout << "Extracting: " << (odir / subname) << " at offset: 0x" << std::hex
                            << (size_t)(hdr.data_base + real_offset) * hdr.block_size << std::endl;
                        save_entry(ifs, hdr.data_base + real_offset, odir / subname, hdr.block_size);
                    }
                }
            }

        }
        else {
            std::cout << "Invalid mode!" << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Process complete." << std::endl;
    return 0;
}


