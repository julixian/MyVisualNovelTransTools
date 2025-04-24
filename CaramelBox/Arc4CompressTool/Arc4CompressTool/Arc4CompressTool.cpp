#define _CRT_SECURE_NO_WARNINGS
#include <boost/endian.hpp>
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <iostream>
#include "arc4common.h"

namespace fs = std::filesystem;

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

void RepackARC4(std::string& in_filename, fs::path& input_dir, std::string& out_filename)
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
    std::vector<uint8_t> index = uncompress_sequence(compressedIndex, in_filename);
    uint32_t orgDatabase = hdr.data_base;
    hdr.data_base = (hdr.header_length + index.size() + hdr.block_size - 1) / hdr.block_size;
    ofs.seekp(hdr.data_base * hdr.block_size + hdr.block_size);
    uint8_t* toc_buff = index.data();

    uint8_t* subtoc_buff = toc_buff + hdr.subtoc_offset - hdr.header_length;

    ARC4ENTRY* entries = (ARC4ENTRY*)toc_buff;
    char* filenames = (char*)(toc_buff + hdr.table_length);

    for (uint32_t i = 0; i < hdr.entry_count; i++) {
        uint32_t filename_offset = get_stupid_long(entries[i].filename_offset) * 2;
        uint32_t offset = get_stupid_long(entries[i].offset);

        std::string filename(filenames + filename_offset, entries[i].filename_length);

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
                std::cout << "Will not replace: " << filename << std::endl;
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
                std::string prefix = get_file_prefix(filename);
                std::string ext = get_file_extension(filename);

                std::string numidx = std::to_string(j);
                while (numidx.length() < 3)numidx = "0" + numidx;
                std::string subname = prefix + "_" + numidx + "." + ext;

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
                    std::cout << "Will not replace: " << filename << std::endl;
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
    std::vector<uint8_t> newIndex = compress_sequence(index);
    ofs.write((char*)newIndex.data(), newIndex.size());
    hdr.toc_length = newIndex.size();
    ofs.seekp(0);
    ofs.write((char*)&hdr, sizeof(hdr));
    ofs.close();
}

int main(int argc, char** argv) {

    if (argc < 4) {
        std::cout << "exarc4 v1.0 by asmodean, patched by julixian 2025.04.24\n"
            << "Usage: \n"
            << "For extract: " << argv[0] << " -e <input.bin> <output_dir>\n"
            << "For repack: " << argv[0] << " -p <inputorg.bin> <input_new_files_dir> <output.bin>\n"
            << "For decompress: " << argv[0] << " -d <input_dir> <output_dir>\n"
            << "For compress: " << argv[0] << " -c <input_dir> <output_dir>" << std::endl;
        return 1;
    }

    std::string mode(argv[1]);
    if (mode == "-p") {
        std::string in_filename(argv[2]);
        fs::path input_dir(argv[3]);
        std::string out_filename(argv[4]);
        RepackARC4(in_filename, input_dir, out_filename);
    }
    else if (mode == "-d") {
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
            std::vector<uint8_t> decompressedData = uncompress_sequence(compressedData, entry.path().string());
            if (decompressedData.size() == 0)continue;
            std::ofstream ofs(oFilePath, std::ios::binary);
            if (!ofs) {
                std::cout << "Can not open file: " << oFilePath << std::endl;
                continue;
            }
            ofs.write((char*)decompressedData.data(), decompressedData.size());
            ofs.close();
        }
    }
    else if (mode == "-c") {
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
            std::vector<uint8_t> compressedData = compress_sequence(orgData);
            std::ofstream ofs(oFilePath, std::ios::binary);
            if (!ofs) {
                std::cout << "Can not open file: " << oFilePath << std::endl;
                continue;
            }
            ofs.write((char*)compressedData.data(), compressedData.size());
            ofs.close();
        }
    }
    else if (mode == "-e") {
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
            index = uncompress_sequence(compressedIndex, in_filename);
        }
        //std::ofstream ofst("index.bin", std::ios::binary);
        //ofst.write((char*)index.data(), index.size());
        //ofst.close();

        uint8_t* toc_buff = index.data();
        uint8_t* subtoc_buff = toc_buff + hdr.subtoc_offset - hdr.header_length;

        ARC4ENTRY* entries = (ARC4ENTRY*)toc_buff;
        char* filenames = (char*)(toc_buff + hdr.table_length);

        for (uint32_t i = 0; i < hdr.entry_count; i++) {
            uint32_t filename_offset = get_stupid_long(entries[i].filename_offset) * 2;
            uint32_t offset = get_stupid_long(entries[i].offset);

            std::string filename(filenames + filename_offset, entries[i].filename_length);

            if (entries[i].subentry_count == 1) {
                std::cout << "Extracting: " << (odir / filename) << " at offset: 0x" << std::hex
                    << (size_t)(hdr.data_base + offset) * hdr.block_size << std::endl;
                save_entry(ifs, hdr.data_base + offset, odir / filename, hdr.block_size);
            }
            else {
                offset *= 3;

                for (uint32_t j = 0; j < entries[i].subentry_count; j++) {
                    std::string prefix = get_file_prefix(filename);
                    std::string ext = get_file_extension(filename);

                    std::string numidx = std::to_string(j);
                    while (numidx.length() < 3)numidx = "0" + numidx;
                    std::string subname = prefix + "_" + numidx + "." + ext;

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
        return 1;
    }
    return 0;
}


