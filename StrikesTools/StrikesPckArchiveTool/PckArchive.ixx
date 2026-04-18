module;

#include <cstdint>

export module PckArchive;

import std;

export namespace Strikes {

    struct PckConfigInfo {
        uint32_t firstBlockSize{};
        uint32_t firstPayloadOffset{};
        uint32_t secondBlockSize{};
        uint32_t secondTableOffset{};
        uint32_t thirdBlockSize{};
        uint32_t thirdTableOffset{};
        uint32_t configOffset{};
        uint32_t metaSizeOffset{};
        uint32_t metaUnpackedSize{};
        uint32_t metaTableOffset{};
        std::array<uint32_t, 9> groupSizeTable{};
    };

    struct PckGroupInfo {
        uint32_t groupIndex{};
        uint32_t metadataSize{};
        uint32_t entryCount{};
        uint32_t groupKey{};
        uint8_t groupBase{};
    };

    struct PckEntry {
        uint32_t groupIndex{};
        uint32_t entryIndex{};
        std::wstring nameWide;
        std::string nameCp932;
        uint32_t offset{};
        uint32_t packedSize{};
        uint32_t unpackedSize{};
        bool isEncrypted{};
        bool isPacked{};
        uint32_t groupKey{};
        uint8_t groupBase{};
    };

    class PckArchive {
    public:
        explicit PckArchive(const std::filesystem::path& packagePath);
        ~PckArchive();

        PckArchive(const PckArchive&) = delete;
        PckArchive& operator=(const PckArchive&) = delete;
        PckArchive(PckArchive&&) noexcept;
        PckArchive& operator=(PckArchive&&) noexcept;

        [[nodiscard]] const std::filesystem::path& getPackagePath() const;
        [[nodiscard]] const PckConfigInfo& getConfigInfo() const;
        [[nodiscard]] const std::vector<PckGroupInfo>& getGroupInfos() const;
        [[nodiscard]] const std::vector<PckEntry>& getEntries() const;

        [[nodiscard]] std::vector<uint8_t> extractEntry(const PckEntry& entry) const;
        void extractToDirectory(const std::filesystem::path& outputDirectory, std::wstring_view filter = L"") const;
        void rebuildToFile(const std::filesystem::path& outputPath, const std::filesystem::path& replacementDirectory) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };

    [[nodiscard]] std::wstring sanitizeEntryName(std::wstring_view name);
    [[nodiscard]] std::filesystem::path buildEntryPath(const std::filesystem::path& rootPath, const PckEntry& entry, bool withIndex = false);
    [[nodiscard]] bool matchesEntryFilter(const PckEntry& entry, std::wstring_view filter);
}
