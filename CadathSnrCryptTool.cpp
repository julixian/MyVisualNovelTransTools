#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include <zlib.h>
#include <filesystem>
#include <Windows.h>

namespace fs = std::filesystem;

struct Entry {
    uint32_t offset;
    uint32_t size;
    char name[24] = { 0 };
};

std::string WideToAscii(const std::wstring& wide, UINT CodePage) {
    int len = WideCharToMultiByte(CodePage, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return "";
    std::string ascii(len, '\0');
    WideCharToMultiByte(CodePage, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
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


//===================================================解加密方法=====================================================
uint32_t RotL(uint32_t value, int shift) {
    return (value << shift) | (value >> (32 - shift));
}

void Decrypt_1_1(std::vector<uint8_t>& data) {
    uint8_t key = 0x84;
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] -= key;
        int count = ((i & 0xF) + 2) / 3;
        while (count-- > 0) {
            key += 0x99;
        }
    }
}

void Decrypt_1_2(std::vector<uint8_t>& data) {
    if (data.size() < 4) return;

    const uint32_t seed = 0x3977141B;
    uint32_t key = seed;

    size_t len = data.size() & ~3;
    uint32_t* data32 = reinterpret_cast<uint32_t*>(data.data());

    for (size_t i = 0; i < len; i += 4) {
        key = RotL(key, 3);
        *data32 ^= key;
        data32++;
        key += seed;
    }
}

void Encrypt_1_1(std::vector<uint8_t>& data) {
    Decrypt_1_2(data);
}

void Encrypt_1_2(std::vector<uint8_t>& data) {
    uint8_t key = 0x84;
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] += key;
        int count = ((i & 0xF) + 2) / 3;
        while (count-- > 0) {
            key += 0x99;
        }
    }
}

std::vector<uint8_t> Decrypt_2(std::vector<uint8_t> data) //https://github.com/One-sixth/TsukikagerouTranslateProject/blob/main/tools_src/%E6%9C%88%E9%98%B3%E7%82%8E%E6%B1%89%E5%8C%96%E8%AE%A1%E5%88%92%20C%2B%2B/format_lib/SnrFile.cpp
{
    uint8_t* ecx = data.data();
    unsigned int edi = data.size();
    unsigned int esi = 0;
    unsigned int edx = 0;
    unsigned int eax = 0;
    unsigned int ebx = 0;
    unsigned long long tmp64 = 0;

    *(char*)&ebx = 0x71;

loop1:
    *(char*)&eax = *(char*)ecx;
    *(char*)&eax = *(char*)&eax - *(char*)&ebx;
    *(char*)ecx = *(char*)&eax;
    eax = esi;
    eax = eax & 0xF;
    if (eax > 0)
    {
        edx = eax + 0x4;
        eax = 0xCCCCCCCD;
        tmp64 = (unsigned long long)eax * (unsigned long long)edx;
        eax = tmp64;
        edx = tmp64 >> 32;
        eax = edx;
        *(char*)&edx = 0x47;
        eax = eax >> 0x2;
        *(short*)&eax = (short)*(char*)&eax * (short)*(char*)&edx;
        *(char*)&ebx += *(char*)&eax;
    }
    ecx += 1;
    esi += 1;
    if (esi < edi)
        goto loop1;

    return data;
}

std::vector<uint8_t> Encrypt_2(std::vector<uint8_t> data)
{
    uint8_t* ecx = data.data();
    unsigned int edi = data.size();
    unsigned int esi = 0;
    unsigned int edx = 0;
    unsigned int eax = 0;
    unsigned int ebx = 0;
    unsigned long long tmp64 = 0;

    *(char*)&ebx = 0x71;

loop1:
    *(char*)&eax = *(char*)ecx;
    *(char*)&eax = *(char*)&eax + *(char*)&ebx;
    *(char*)ecx = *(char*)&eax;
    eax = esi;
    eax = eax & 0xF;
    if (eax > 0)
    {
        edx = eax + 0x4;
        eax = 0xCCCCCCCD;
        tmp64 = (unsigned long long)eax * (unsigned long long)edx;
        eax = tmp64;
        edx = tmp64 >> 32;
        eax = edx;
        *(char*)&edx = 0x47;
        eax = eax >> 0x2;
        *(short*)&eax = (short)*(char*)&eax * (short)*(char*)&edx;
        *(char*)&ebx += *(char*)&eax;
    }
    ecx += 1;
    esi += 1;
    if (esi < edi)
        goto loop1;

    return data;
}

void Decrypt_3(DWORD* a1, int a2, int a3) //待解密内容指针, 长度, 解密后长度
{
    int v3; // eax
    DWORD* v4; // ecx

    v3 = a2 / 4;
    v4 = a1;
    if (a2 / 4 > 0)
    {
        do
        {
            *v4++ -= ((a3 | (a3 << 12)) << 11) ^ a3;
            --v3;
        } while (v3);
    }
}

void Encrypt_3(DWORD* a1, int a2, int a3)
{
    int v3; // eax
    DWORD* v4; // ecx

    v3 = a2 / 4;
    v4 = a1;
    if (a2 / 4 > 0)
    {
        do
        {
            *v4++ += ((a3 | (a3 << 12)) << 11) ^ a3;
            --v3;
        } while (v3);
    }
}

void Decrypt_4_1(BYTE* a1, int a2)
{
    int v3; // esi
    char i; // bl
    unsigned int v5; // edx

    v3 = 0;
    for (i = -124; v3 < a2; ++v3)
    {
        *a1 -= i;
        if ((v3 & 0xF) != 0)
        {
            v5 = ((v3 & 0xFu) + 2) / 3;
            do
            {
                i -= 103;
                --v5;
            } while (v5);
        }
        ++a1;
    }
}

void Decrypt_4_2(DWORD* a1, int a2)
{
    DWORD* result; // eax
    int v3; // ecx
    bool v4; // cc
    int v5; // edx
    int v6; // et0
    int v7; // [esp+Ch] [ebp+Ch]

    result = a1;
    v3 = a2 >> 2;
    v4 = a2 >> 2 <= 0;
    v7 = 963969828;
    if (!v4)
    {
        v5 = v3;
        do
        {
            v6 = RotL(v7, 3);
            *result++ ^= v6;
            --v5;
            v7 = v6 + 963969828;
        } while (v5);
    }
}

void Encrypt_4_1(DWORD* a1, int a2)
{
    Decrypt_4_2(a1, a2);
}

void Encrypt_4_2(BYTE* a1, int a2)
{
    int v3; // esi
    char i; // bl
    unsigned int v5; // edx

    v3 = 0;
    for (i = -124; v3 < a2; ++v3)
    {
        *a1 += i;
        if ((v3 & 0xF) != 0)
        {
            v5 = ((v3 & 0xFu) + 2) / 3;
            do
            {
                i -= 103;
                --v5;
            } while (v5);
        }
        ++a1;
    }
}

//=========================================================解加密方法结束=================================================

std::vector<uint8_t> CompressData(const std::vector<uint8_t>& input) {
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    if (deflateInit(&strm, Z_BEST_COMPRESSION) != Z_OK) {
        throw std::runtime_error("deflateInit failed");
    }

    std::vector<uint8_t> output(deflateBound(&strm, input.size()));

    strm.avail_in = input.size();
    strm.next_in = const_cast<uint8_t*>(input.data());
    strm.avail_out = output.size();
    strm.next_out = output.data();

    if (deflate(&strm, Z_FINISH) != Z_STREAM_END) {
        deflateEnd(&strm);
        throw std::runtime_error("deflate failed");
    }

    output.resize(strm.total_out);
    deflateEnd(&strm);
    return output;
}

void SNR_EncryptFile(const std::string& input_file, const std::string& output_file, int version) {
    // 读取输入文件
    std::ifstream input(input_file, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open input file");
    }
    else {
        std::cout << "Encrypting: " << input_file << std::endl;
    }

    // 读取文件内容
    std::vector<uint8_t> input_data(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );
    input.close();

    // 压缩数据
    std::vector<uint8_t> compressed_data = CompressData(input_data);

    uint32_t checksum = 0;

    // 创建最终数据（校验和 + 压缩数据）
    std::vector<uint8_t> final_data(4 + compressed_data.size());
    *(uint32_t*)final_data.data() = checksum;
    std::copy(compressed_data.begin(), compressed_data.end(), final_data.begin() + 4);

    // 加密数据
    if (version == 1) {
        Encrypt_1_1(final_data);
        Encrypt_1_2(final_data);
    }
    else if (version == 2) {
        Encrypt_1_1(final_data);
        final_data = Encrypt_2(final_data);
    }
    else if (version == 3) {
        Encrypt_3((DWORD*)final_data.data(), final_data.size(), input_data.size());
    }
    else if (version == 4) {
        Encrypt_4_1((DWORD*)final_data.data(), final_data.size());
        Encrypt_4_2(final_data.data(), final_data.size());
    }
    else {
        std::cout << "Not a valid version!" << std::endl;
        return;
    }

    std::ofstream output(output_file, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Failed to create output file");
    }

    output.write("SNR\x1A", 4);

    uint32_t compressed_size = final_data.size();
    output.write(reinterpret_cast<char*>(&compressed_size), 4);

    uint32_t original_size = input_data.size();
    output.write(reinterpret_cast<char*>(&original_size), 4);

    output.write(reinterpret_cast<char*>(final_data.data()), final_data.size());

    output.close();
}

void SNR_DecryptFile(const std::string& input_file, const std::string& output_file, int version) {
    std::ifstream input(input_file, std::ios::binary);
    if (!input) {
        std::cerr << "Failed to open input file" << std::endl;
        return;
    }
    else {
        std::cout << "Decrypting: " << input_file << std::endl;
    }

    // 读取文件头
    char header[4];
    input.read(header, 4);
    if (std::string(header, 4) != "SNR\x1A") {
        std::cerr << "Invalid SNR file" << std::endl;
        return;
    }

    uint32_t complen;
    uint32_t decomplen;
    input.read((char*)&complen, 4);
    input.read((char*)&decomplen, 4);

    // 读取文件内容
    input.seekg(0, std::ios::end);
    size_t fileSize = input.tellg();
    input.seekg(12, std::ios::beg);

    std::vector<uint8_t> data(fileSize - 12);
    input.read(reinterpret_cast<char*>(data.data()), fileSize - 12);
    input.close();

    // 执行解密
    if (version == 1) {
        Decrypt_1_1(data);
        Decrypt_1_2(data);
    }
    else if (version == 2) {
        data = Decrypt_2(data);
        Decrypt_1_2(data);
    }
    else if (version == 3) {
        Decrypt_3((DWORD*)data.data(), data.size(), decomplen);
    }
    else if (version == 4) {
        Decrypt_4_1(data.data(), data.size());
        Decrypt_4_2((DWORD*)data.data(), data.size());
    }
    else {
        std::cout << "Not a valid version!" << std::endl;
        return;
    }

    uint32_t stored_alder32 = *(uint32_t*)data.data();

    std::vector<uint8_t> decompressed_data(decomplen);
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = data.size() - 4;
    strm.next_in = data.data() + 4;
    strm.avail_out = decomplen;
    strm.next_out = decompressed_data.data();

    if (inflateInit(&strm) != Z_OK) {
        std::cerr << "zlib初始化失败: " << input_file << std::endl;
        return;
    }

    if (inflate(&strm, Z_FINISH) != Z_STREAM_END) {
        inflateEnd(&strm);
        std::cerr << "解压缩失败: " << input_file << std::endl;
        return;
    }

    inflateEnd(&strm);

    //decompressed_data = data;

    // 写入输出文件
    std::ofstream output(output_file, std::ios::binary);
    if (!output) {
        std::cerr << "Failed to create output file" << std::endl;
        return;
    }

    output.write(reinterpret_cast<char*>(decompressed_data.data()), decompressed_data.size());
    output.close();

}

bool pack_daf(fs::path input_dir, fs::path output_kar) {

    uint32_t signature = 0x1A464144;
    uint32_t file_count = 0;
    std::vector<Entry> entries;
    uint32_t CurOffset = 0x8;

    for (auto entry : fs::directory_iterator(input_dir)) {
        if (entry.is_regular_file()) {
            file_count++;
            CurOffset += sizeof(Entry);
        }
    }

    for (auto entry : fs::directory_iterator(input_dir)) {
        if (entry.is_regular_file()) {
            Entry fentry;
            std::string filename = WideToAscii(fs::relative(entry.path(), input_dir).wstring(), 932);
            memcpy(fentry.name, &filename[0], filename.length());
            fentry.size = entry.file_size();
            fentry.offset = CurOffset;
            entries.push_back(fentry);
            CurOffset += entry.file_size();
        }
    }

    std::ofstream outDaf(output_kar, std::ios::binary);
    if (!outDaf) {
        std::cout << "Fail to create output_Daf file" << std::endl;
        return false;
    }
    outDaf.write((char*)&signature, 4);
    outDaf.write((char*)&file_count, 4);

    for (auto& entry : entries) {
        outDaf.write((char*)&entry.offset, 4);
        outDaf.write((char*)&entry.size, 4);
        outDaf.write(entry.name, 24);
    }

    for (auto& entry : entries) {
        std::string filename(entry.name);
        std::cout << "Processing: " << WideToAscii(AsciiToWide(filename, 932), CP_ACP) << std::endl;
        fs::path input_file = input_dir / AsciiToWide(filename, 932);
        std::ifstream input(input_file, std::ios::binary);
        auto file_size = fs::file_size(input_file);
        std::vector<uint8_t> file_data(file_size);
        input.read((char*)file_data.data(), file_size);
        input.close();
        outDaf.write((char*)file_data.data(), file_size);
    }
    outDaf.close();
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cout << "Made by julixian 2025.03.14" << std::endl;
        std::cout << "Usage: " << "\n"
            << "For decrypt: " << argv[0] << " -d <version> <input_dir> <output_dir>" << "\n"
            << "For encrypt: " << argv[0] << " -e <version> <input_dir> <output_dir>" << "\n"
            << "For pack daf archive: " << argv[0] << "-p <input_dir> <output_file>" << "\n"
            << "version: 1(ＤＡパンツ！！/月陽炎 ～つきかげろう～), 2(月陽炎～千秋恋歌～), 3(てのひらを、たいように), 4(SinsAbell)" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    std::string input_path = argv[argc - 2];
    std::string output_path = argv[argc - 1];
    int version = 1;

    if (mode != "-p") {
        fs::create_directory(output_path);
        version = std::stoi(std::string(argv[2]));
    }

    if (mode == "-e") {
        for (const auto& entry : fs::recursive_directory_iterator(input_path)) {
            if (entry.is_regular_file()) {
                std::string output_file = output_path + "\\" + fs::relative(entry.path(), input_path).string();
                SNR_EncryptFile(entry.path().string(), output_file, version);
            }
        }
        std::cout << "Encryption completed successfully" << std::endl;
    }
    else if (mode == "-d") {
        for (const auto& entry : fs::recursive_directory_iterator(input_path)) {
            if (entry.is_regular_file()) {
                std::string output_file = output_path + "\\" + fs::relative(entry.path(), input_path).string();
                SNR_DecryptFile(entry.path().string(), output_file, version);
            }
        }
        std::cout << "Decryption completed successfully" << std::endl;
    }
    else if (mode == "-p") {
        pack_daf(input_path, output_path);
    }
    else {
        std::cout << "Invalid mode. Use -e for encrypt or -d for decrypt" << std::endl;
        return 1;
    }

    return 0;
}
