#include <Windows.h>
#include <cstdint>
#include <CLI/CLI.hpp>

import std;
import Tool;
import PckArchive;

namespace fs = std::filesystem;

int main(int argc, char** argv)
{
    SetConsoleOutputCP(CP_UTF8);

    CLI::App app("Made by julixian 2026.04.18", "StrikesPckArchiveTool");
    argv = app.ensure_utf8(argv);
    app.set_help_all_flag("-a");
    app.require_subcommand(1);

    fs::path packagePath;
    fs::path outputDirectory;
    fs::path replacementDirectory;
    fs::path outputPackagePath;
    std::wstring filter;

    auto infoCommand = app.add_subcommand("info", "show archive metadata");
    infoCommand->add_option("inputPck", packagePath, "input AVGDatas.pck")->required()->check(CLI::ExistingFile);

    auto listCommand = app.add_subcommand("list", "list archive entries");
    listCommand->add_option("inputPck", packagePath, "input AVGDatas.pck")->required()->check(CLI::ExistingFile);
    listCommand->add_option("filter", filter, "entry name filter");

    auto extractCommand = app.add_subcommand("extract", "extract entries");
    extractCommand->alias("-e");
    extractCommand->add_option("inputPck", packagePath, "input AVGDatas.pck")->required()->check(CLI::ExistingFile);
    extractCommand->add_option("outputDir", outputDirectory, "output directory")->required();
    extractCommand->add_option("filter", filter, "entry name filter");

    auto rebuildCommand = app.add_subcommand("rebuild", "rebuild archive with replacements");
    rebuildCommand->alias("-r");
    rebuildCommand->add_option("inputPck", packagePath, "original AVGDatas.pck")->required()->check(CLI::ExistingFile);
    rebuildCommand->add_option("inputDir", replacementDirectory, "replacement directory")->required()->check(CLI::ExistingDirectory);
    rebuildCommand->add_option("outputPck", outputPackagePath, "output AVGDatas.pck")->required();

    CLI11_PARSE(app, argc, argv);

    try {
        Strikes::PckArchive archive(packagePath);

        if (infoCommand->parsed()) {
            auto& configInfo = archive.getConfigInfo();
            std::println("package={}", wide2Ascii(packagePath.native(), CP_UTF8));
            std::println("configOffset=0x{:08X}", configInfo.configOffset);
            std::println("metaSizeOffset=0x{:08X}", configInfo.metaSizeOffset);
            std::println("metaUnpackedSize={}", configInfo.metaUnpackedSize);
            std::println("groups={}", archive.getGroupInfos().size());
            std::println("entries={}", archive.getEntries().size());

            for (const auto& groupInfo : archive.getGroupInfos()) {
                std::println(
                    "group{}: metadataSize={} entryCount={} groupKey=0x{:08X} groupBase=0x{:02X}",
                    groupInfo.groupIndex,
                    groupInfo.metadataSize,
                    groupInfo.entryCount,
                    groupInfo.groupKey,
                    groupInfo.groupBase);
            }
        }
        else if (listCommand->parsed()) {
            std::println("group\tindex\tflags\toffset\tpacked\tunpacked\tname");
            for (const auto& entry : archive.getEntries()) {
                if (!Strikes::matchesEntryFilter(entry, filter)) {
                    continue;
                }

                auto flags = std::string{};
                if (entry.isEncrypted) {
                    flags.push_back('E');
                }
                if (entry.isPacked) {
                    flags.push_back('P');
                }
                if (flags.empty()) {
                    flags = "-";
                }

                std::println(
                    "{}\t{}\t{}\t0x{:08X}\t{}\t{}\t{}",
                    entry.groupIndex,
                    entry.entryIndex,
                    flags,
                    entry.offset,
                    entry.packedSize,
                    entry.unpackedSize,
                    wide2Ascii(entry.nameWide, CP_UTF8)
                );
            }
        }
        else if (extractCommand->parsed()) {
            archive.extractToDirectory(outputDirectory, filter);
        }
        else if (rebuildCommand->parsed()) {
            archive.rebuildToFile(outputPackagePath, replacementDirectory);
        }
    }
    catch (const std::exception& exception) {
        std::println(stderr, "error: {}", exception.what());
        return 2;
    }

    return 0;
}
