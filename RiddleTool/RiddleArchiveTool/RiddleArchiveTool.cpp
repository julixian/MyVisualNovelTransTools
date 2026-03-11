#include <Windows.h>
#include <cstdint>
#include <CLI/CLI.hpp>

import std;
import Tool;

namespace fs = std::filesystem;

struct PacEntry {
    std::wstring         nameW;
    std::string          nameCp932;
    uint32_t             size{};
    uint32_t             unpackedSize{};
    bool                 isPacked{};
    uint32_t             offset{};
    std::vector<uint8_t> header;
    std::vector<uint8_t> originalData;
};

std::vector<PacEntry> readPac(const fs::path& pacPath)
{
    std::vector<PacEntry> entries;

    std::ifstream file(pacPath, std::ios::binary);
    if (!file.is_open()) {
        std::println("failed to open pac: {}", wide2Ascii(pacPath.native(), CP_UTF8));
        return entries;
    }

    uint32_t signature = 0;
    file.read((char*)&signature, 4);
    if (signature != 0x31434150) {
        std::println("invalid pac signature: {}", wide2Ascii(pacPath.native(), CP_UTF8));
        return entries;
    }

    uint32_t count = 0;
    file.read((char*)&count, 4);
    if ((int)count < 0 || count > 10000) {
        std::println("invalid entry count {} in pac {}", count, wide2Ascii(pacPath.native(), CP_UTF8));
        return entries;
    }

    uint32_t baseOffset = 8 + count * 0x20;

    entries.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        PacEntry entry{};
        entry.header.resize(0x20);
        file.read((char*)entry.header.data(), 0x20);
        if (!file) {
            std::println("failed to read directory entry {} in pac {}", i, wide2Ascii(pacPath.native(), CP_UTF8));
            entries.clear();
            return entries;
        }

        entry.nameCp932.assign((const char*)entry.header.data(), 16);
        auto pos = entry.nameCp932.find('\0');
        if (pos != std::string::npos) {
            entry.nameCp932.resize(pos);
        }
        entry.nameW = ascii2Wide(entry.nameCp932, 932);

        entry.size = read<uint32_t>(entry.header.data() + 0x10);
        entry.unpackedSize = read<uint32_t>(entry.header.data() + 0x18);

        char cmpTag[5]{};
        std::memcpy(cmpTag, entry.header.data() + 0x14, 4);
        cmpTag[4] = '\0';

        std::wstring ext = str2Lower(fs::path(entry.nameW).extension());
        bool isScp = (ext == L".scp");
        entry.isPacked = isScp && entry.size > 12 && std::string(cmpTag) == "CMP1";

        entry.offset = baseOffset;
        baseOffset += entry.size;

        entries.push_back(std::move(entry));
    }

    for (auto& entry : entries) {
        if (entry.size == 0) {
            continue;
        }
        entry.originalData.resize(entry.size);
        file.seekg(entry.offset, std::ios::beg);
        file.read((char*)entry.originalData.data(), (std::streamsize)entry.size);
        if (!file) {
            std::println("failed to read data for entry {} in pac {}", entry.nameCp932, wide2Ascii(pacPath.native(), CP_UTF8));
            entry.originalData.clear();
        }
    }

    return entries;
}

void extractPac(const fs::path& pacPath, const fs::path& outputDir)
{
    auto entries = readPac(pacPath);
    if (entries.empty()) {
        return;
    }

    for (auto& entry : entries) {
        fs::path outPath = outputDir / entry.nameW;
        fs::create_directories(outPath.parent_path());

        std::ofstream out(outPath, std::ios::binary);
        if (!out.is_open()) {
            std::println("failed to create file {}", wide2Ascii(outPath.native(), CP_UTF8));
            continue;
        }

        if (!entry.originalData.empty()) {
            out.write((char*)entry.originalData.data(), (std::streamsize)entry.originalData.size());
        }

        std::println("extract {} -> {}", entry.nameCp932, wide2Ascii(outPath.native(), CP_UTF8));
    }
}

void repackPac(const fs::path& inputDir, const fs::path& originalPacPath, const fs::path& outputPacPath)
{
    auto entries = readPac(originalPacPath);
    if (entries.empty()) {
        return;
    }

    for (auto& entry : entries) {
        fs::path candidate = inputDir / entry.nameW;
        if (fs::exists(candidate) && fs::is_regular_file(candidate)) {
            std::println("replacing file: {}", wide2Ascii(candidate.native(), CP_UTF8));
            std::ifstream in(candidate, std::ios::binary);
            if (!in.is_open()) {
                std::println("failed to open replacement {}, use original", wide2Ascii(candidate.native(), CP_UTF8));
                continue;
            }
            std::vector<uint8_t> newData((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            entry.originalData = std::move(newData);
            entry.size = (uint32_t)entry.originalData.size();
            std::memcpy(entry.header.data() + 0x10, &entry.size, sizeof(uint32_t));
        }
        else {
            std::println("missing replacement, use original: {}", wide2Ascii(candidate.native(), CP_UTF8));
        }
    }

    std::ofstream out(outputPacPath, std::ios::binary);
    if (!out.is_open()) {
        std::println("failed to create pac: {}", wide2Ascii(outputPacPath.native(), CP_UTF8));
        return;
    }

    uint32_t signature = 0x31434150;
    uint32_t count = (uint32_t)entries.size();
    out.write((char*)&signature, 4);
    out.write((char*)&count, 4);

    for (auto& entry : entries) {
        out.write((char*)entry.header.data(), (std::streamsize)entry.header.size());
    }

    for (auto& entry : entries) {
        if (!entry.originalData.empty()) {
            out.write((char*)entry.originalData.data(), (std::streamsize)entry.originalData.size());
        }
    }

    std::println("repack done: {}", wide2Ascii(outputPacPath.native(), CP_UTF8));
}

int main(int argc, char** argv)
{
    SetConsoleOutputCP(CP_UTF8);

    CLI::App app("Made by julixian 2026.03.11", "RiddleArchiveTool");
    argv = app.ensure_utf8(argv);
    app.set_help_all_flag("-a");
    app.require_subcommand(1);

    fs::path pacPath;
    fs::path outDir;

    fs::path orgPac;
    fs::path inDir;
    fs::path outPac;

    auto extractCmd = app.add_subcommand("extract");
    extractCmd->alias("-e");
    extractCmd->add_option("inputPac", pacPath, "input pac file")->required();
    extractCmd->add_option("outputDir", outDir, "output directory")->required();

    auto repackCmd = app.add_subcommand("repack");
    repackCmd->alias("-r");
    repackCmd->add_option("originalPac", orgPac, "original pac file")->required();
    repackCmd->add_option("inputDir", inDir, "input directory")->required();
    repackCmd->add_option("outputPac", outPac, "output pac file")->required();

    CLI11_PARSE(app, argc, argv);

    if (extractCmd->parsed()) {
        extractPac(pacPath, outDir);
    }
    else if (repackCmd->parsed()) {
        repackPac(inDir, orgPac, outPac);
    }

    return 0;
}


