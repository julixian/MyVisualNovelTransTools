#include <Windows.h>
#include <cstdint>
#include <CLI/CLI.hpp>

import std;
import Tool;
import nlohmann.json;

namespace fs = std::filesystem;
using json = nlohmann::ordered_json;

namespace {

constexpr UINT CODE_PAGE_UTF8 = CP_UTF8;
constexpr UINT CODE_PAGE_CP932 = 932;
constexpr size_t TRAILER_SIZE = 8;

enum class ExportKind {
    Line,
    Narration,
    Append,
    Title,
    Choice,
};

struct TextEntry {
    std::string name;
    std::string message;
};

struct ChoiceOption {
    std::vector<uint8_t> prefix;
    std::vector<uint8_t> suffix;
    std::string rawText;
};

struct Instruction {
    size_t start = 0;
    uint8_t opcode = 0;
    std::vector<uint8_t> raw;
    std::optional<ExportKind> exportKind;
    std::string name;
    std::string message;
    std::string suffix;
    std::vector<ChoiceOption> options;

    [[nodiscard]] size_t end() const {
        return start + raw.size();
    }
};

struct CStringResult {
    std::string text;
    size_t end = 0;
};

struct ParseResult {
    std::vector<Instruction> instructions;
    std::vector<TextEntry> exported;
};

[[nodiscard]] const std::unordered_map<uint8_t, size_t>& fixedLengths()
{
    static const std::unordered_map<uint8_t, size_t> value = {
        { 0x01, 11 }, { 0x03, 8 }, { 0x04, 1 }, { 0x05, 3 }, { 0x06, 5 }, { 0x08, 2 }, { 0x0A, 1 }, { 0x0B, 3 },
        { 0x0C, 4 }, { 0x0D, 8 }, { 0x0E, 3 }, { 0x22, 5 }, { 0x24, 2 }, { 0x26, 3 }, { 0x28, 6 }, { 0x29, 5 },
        { 0x30, 5 }, { 0x31, 3 }, { 0x32, 3 }, { 0x33, 8 }, { 0x44, 5 }, { 0x45, 5 }, { 0x47, 3 }, { 0x49, 4 },
        { 0x4A, 7 }, { 0x4B, 21 }, { 0x4C, 9 }, { 0x4D, 14 }, { 0x4E, 5 }, { 0x4F, 5 }, { 0x51, 6 }, { 0x52, 3 },
        { 0x55, 2 }, { 0x56, 2 }, { 0x57, 10 }, { 0x58, 9 }, { 0x60, 2 }, { 0x62, 2 }, { 0x63, 4 }, { 0x64, 9 },
        { 0x65, 6 }, { 0x66, 23 }, { 0x67, 9 }, { 0x68, 10 }, { 0x69, 3 }, { 0x70, 9 }, { 0x72, 2 }, { 0x74, 3 },
        { 0x75, 10 }, { 0x76, 18 }, { 0x77, 10 }, { 0x78, 9 }, { 0x79, 2 }, { 0x81, 3 }, { 0x82, 4 }, { 0x83, 2 },
        { 0x84, 2 }, { 0x85, 3 }, { 0x86, 3 }, { 0x87, 4 }, { 0x88, 4 }, { 0x89, 2 }, { 0x8A, 2 }, { 0x8B, 2 },
        { 0x8C, 4 }, { 0x8D, 2 }, { 0x8E, 2 }, { 0xA0, 7 }, { 0xA1, 8 }, { 0xA2, 7 }, { 0xA3, 7 }, { 0xA4, 7 },
        { 0xA5, 3 }, { 0xA6, 2 }, { 0xA7, 2 }, { 0xA8, 17 }, { 0xA9, 2 }, { 0xAA, 4 }, { 0xAB, 2 }, { 0xAC, 2 },
        { 0xAD, 11 }, { 0xAE, 2 }, { 0xB1, 6 }, { 0xB3, 3 }, { 0xB4, 13 }, { 0xB5, 8 }, { 0xB8, 4 }, { 0xB9, 4 },
        { 0xBB, 2 }, { 0xBC, 5 }, { 0xBD, 3 }, { 0xBE, 4 }, { 0xBF, 9 }, { 0xE2, 2 }, { 0xE3, 2 }, { 0xE4, 3 },
        { 0xE5, 2 }, { 0xE6, 3 }, { 0xE7, 4 }, { 0xE9, 2 }, { 0xEB, 2 }, { 0xFF, 1 },
    };
    return value;
}

[[nodiscard]] const std::unordered_map<uint8_t, size_t>& singleStringOffsets()
{
    static const std::unordered_map<uint8_t, size_t> value = {
        { 0x07, 1 }, { 0x09, 1 }, { 0x21, 11 }, { 0x23, 10 }, { 0x25, 12 }, { 0x27, 10 }, { 0x41, 5 }, { 0x43, 7 },
        { 0x46, 10 }, { 0x48, 12 }, { 0x50, 1 }, { 0x53, 6 }, { 0x54, 1 }, { 0x59, 1 }, { 0x61, 2 }, { 0x71, 1 },
        { 0x73, 10 }, { 0xB2, 3 }, { 0xB6, 3 }, { 0xB7, 6 }, { 0xBA, 12 }, { 0xE0, 1 }, { 0xE8, 1 }, { 0xEA, 2 },
    };
    return value;
}

[[nodiscard]] const std::array<std::string_view, 3>& controlSuffixes()
{
    static constexpr std::array<std::string_view, 3> value = { "%K%P", "%K", "%P" };
    return value;
}

[[nodiscard]] std::string decodeCp932(const uint8_t* ptr, size_t size)
{
    return ascii2Ascii(std::string_view((const char*)ptr, size), CODE_PAGE_CP932, CODE_PAGE_UTF8);
}

[[nodiscard]] std::vector<uint8_t> encodeCp932(std::string_view text)
{
    auto encoded = ascii2Ascii(text, CODE_PAGE_UTF8, CODE_PAGE_CP932);
    return std::vector<uint8_t>(encoded.begin(), encoded.end());
}

[[nodiscard]] uint16_t readU16(const std::vector<uint8_t>& data, size_t offset)
{
    return read<uint16_t>(data.data() + offset);
}

[[nodiscard]] uint32_t readU32(const std::vector<uint8_t>& data, size_t offset)
{
    return read<uint32_t>(data.data() + offset);
}

void writeU32(std::vector<uint8_t>& data, size_t offset, uint32_t value)
{
    write<uint32_t>(data.data() + offset, value);
}

[[nodiscard]] CStringResult readCString(const std::vector<uint8_t>& data, size_t start, size_t limit)
{
    auto end = start;
    while (end < limit && data[end] != 0) {
        ++end;
    }
    if (end >= limit) {
        throw std::runtime_error(std::format("Missing string terminator at 0x{:X}", start));
    }
    return {
        .text = decodeCp932(data.data() + start, end - start),
        .end = end,
    };
}

[[nodiscard]] std::pair<std::string, std::string> stripControlSuffix(std::string text)
{
    std::string suffix;
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto token : controlSuffixes()) {
            if (text.size() >= token.size() && text.ends_with(token)) {
                text.erase(text.size() - token.size());
                suffix.insert(0, token);
                changed = true;
                break;
            }
        }
    }
    return { text, suffix };
}

[[nodiscard]] std::pair<std::string, std::string> exportMessage(const std::string& rawText)
{
    return stripControlSuffix(rawText);
}

[[nodiscard]] std::vector<uint8_t> scriptMessageBytes(const std::string& message, const std::string& suffix)
{
    return encodeCp932(message + suffix);
}

[[nodiscard]] Instruction parseChoiceInstruction(const std::vector<uint8_t>& data, size_t pos, size_t limit)
{
    if (pos + 3 > limit) {
        throw std::runtime_error(std::format("Truncated choice block at 0x{:X}", pos));
    }

    auto count = (size_t)data[pos + 1];
    auto cursor = pos + 3;
    std::vector<ChoiceOption> options;
    options.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        if (cursor + 2 >= limit) {
            throw std::runtime_error(std::format("Truncated choice item at 0x{:X}", cursor));
        }

        auto prefix = std::vector<uint8_t>(data.begin() + (intptr_t)cursor, data.begin() + (intptr_t)(cursor + 2));
        auto rawText = readCString(data, cursor + 2, limit);
        auto suffixStart = rawText.end + 1;
        if (suffixStart + 4 > limit) {
            throw std::runtime_error(std::format("Broken choice suffix at 0x{:X}", cursor));
        }

        auto mode = data[suffixStart + 3];
        size_t nextCursor = 0;
        if (mode == 3) {
            nextCursor = suffixStart + 11;
        }
        else if (mode == 6) {
            nextCursor = suffixStart + 9;
        }
        else if (mode == 7) {
            auto extraText = readCString(data, suffixStart + 4, limit);
            nextCursor = extraText.end + 1;
        }
        else {
            nextCursor = suffixStart + 3;
        }

        options.push_back({
            .prefix = std::move(prefix),
            .suffix = std::vector<uint8_t>(data.begin() + (intptr_t)suffixStart, data.begin() + (intptr_t)nextCursor),
            .rawText = std::move(rawText.text),
        });
        cursor = nextCursor;
    }

    return {
        .start = pos,
        .opcode = 0x02,
        .raw = std::vector<uint8_t>(data.begin() + (intptr_t)pos, data.begin() + (intptr_t)cursor),
        .exportKind = ExportKind::Choice,
        .options = std::move(options),
    };
}

[[nodiscard]] std::pair<Instruction, std::string> parseInstruction(
    const std::vector<uint8_t>& data,
    size_t pos,
    size_t limit,
    const std::string& currentName)
{
    auto opcode = data[pos];

    if (opcode == 0x00) {
        return {
            Instruction {
                .start = pos,
                .opcode = opcode,
                .raw = { data[pos] },
            },
            currentName,
        };
    }

    if (opcode == 0x02) {
        return { parseChoiceInstruction(data, pos, limit), currentName };
    }

    if (opcode == 0x42) {
        auto nameRaw = readCString(data, pos + 6, limit);
        auto messageRaw = readCString(data, nameRaw.end + 1, limit);
        auto [visibleMessage, suffix] = exportMessage(messageRaw.text);
        return {
            Instruction {
                .start = pos,
                .opcode = opcode,
                .raw = std::vector<uint8_t>(data.begin() + (intptr_t)pos, data.begin() + (intptr_t)(messageRaw.end + 1)),
                .exportKind = ExportKind::Line,
                .name = nameRaw.text,
                .message = std::move(visibleMessage),
                .suffix = std::move(suffix),
            },
            nameRaw.text,
        };
    }

    if (opcode == 0x41 || opcode == 0xB6 || opcode == 0xE0) {
        auto stringOffset = singleStringOffsets().at(opcode);
        auto rawText = readCString(data, pos + stringOffset, limit);
        auto [visibleMessage, suffix] = exportMessage(rawText.text);
        auto exportKind = opcode == 0x41 ? ExportKind::Narration : opcode == 0xB6 ? ExportKind::Append : ExportKind::Title;
        auto name = opcode == 0xB6 ? currentName : std::string {};
        auto nextName = currentName;
        if (opcode == 0x41 || opcode == 0xE0) {
            nextName.clear();
        }
        return {
            Instruction {
                .start = pos,
                .opcode = opcode,
                .raw = std::vector<uint8_t>(data.begin() + (intptr_t)pos, data.begin() + (intptr_t)(rawText.end + 1)),
                .exportKind = exportKind,
                .name = std::move(name),
                .message = std::move(visibleMessage),
                .suffix = std::move(suffix),
            },
            nextName,
        };
    }

    if (auto it = singleStringOffsets().find(opcode); it != singleStringOffsets().end()) {
        auto rawText = readCString(data, pos + it->second, limit);
        return {
            Instruction {
                .start = pos,
                .opcode = opcode,
                .raw = std::vector<uint8_t>(data.begin() + (intptr_t)pos, data.begin() + (intptr_t)(rawText.end + 1)),
            },
            currentName,
        };
    }

    if (auto it = fixedLengths().find(opcode); it != fixedLengths().end()) {
        auto end = pos + it->second;
        if (end > limit) {
            throw std::runtime_error(std::format("Instruction 0x{:02X} truncated at 0x{:X}", opcode, pos));
        }
        return {
            Instruction {
                .start = pos,
                .opcode = opcode,
                .raw = std::vector<uint8_t>(data.begin() + (intptr_t)pos, data.begin() + (intptr_t)end),
            },
            currentName,
        };
    }

    throw std::runtime_error(std::format("Unknown opcode 0x{:02X} at 0x{:X}", opcode, pos));
}

[[nodiscard]] ParseResult parseScript(const std::vector<uint8_t>& data)
{
    if (data.size() < TRAILER_SIZE) {
        throw std::runtime_error("WSC data is smaller than trailer size");
    }

    auto limit = data.size() - TRAILER_SIZE;
    size_t pos = 0;
    std::string currentName;
    ParseResult result;

    while (pos < limit) {
        auto [instruction, nextName] = parseInstruction(data, pos, limit, currentName);
        currentName = std::move(nextName);
        if (instruction.exportKind == ExportKind::Choice) {
            for (const auto& option : instruction.options) {
                auto [message, ignoredSuffix] = exportMessage(option.rawText);
                (void)ignoredSuffix;
                result.exported.push_back({
                    .name = {},
                    .message = std::move(message),
                });
            }
        }
        else if (instruction.exportKind.has_value()) {
            result.exported.push_back({
                .name = instruction.name,
                .message = instruction.message,
            });
        }
        pos = instruction.end();
        result.instructions.push_back(std::move(instruction));
    }

    if (pos != limit) {
        throw std::runtime_error(std::format("Parser stopped at 0x{:X}, expected 0x{:X}", pos, limit));
    }

    return result;
}

[[nodiscard]] std::vector<uint8_t> rebuildInstruction(
    const Instruction& instruction,
    const std::vector<TextEntry>& replacements,
    size_t& replacementIndex)
{
    if (!instruction.exportKind.has_value()) {
        return instruction.raw;
    }

    if (instruction.exportKind == ExportKind::Choice) {
        std::vector<uint8_t> result = {
            instruction.raw.begin(),
            instruction.raw.begin() + (intptr_t)std::min<size_t>(3, instruction.raw.size()),
        };
        for (const auto& option : instruction.options) {
            if (replacementIndex >= replacements.size()) {
                throw std::runtime_error("Replacement entries exhausted while rebuilding choice");
            }
            const auto& entry = replacements[replacementIndex++];
            result.append_range(option.prefix);
            auto messageBytes = scriptMessageBytes(entry.message, {});
            result.append_range(messageBytes);
            result.push_back(0);
            result.append_range(option.suffix);
        }
        return result;
    }

    if (replacementIndex >= replacements.size()) {
        throw std::runtime_error("Replacement entries exhausted while rebuilding instruction");
    }

    const auto& entry = replacements[replacementIndex++];
    if (instruction.opcode == 0x42) {
        std::vector<uint8_t> result(instruction.raw.begin(), instruction.raw.begin() + 6);
        auto nameBytes = encodeCp932(entry.name);
        auto messageBytes = scriptMessageBytes(entry.message, instruction.suffix);
        result.append_range(nameBytes);
        result.push_back(0);
        result.append_range(messageBytes);
        result.push_back(0);
        return result;
    }

    auto stringOffset = singleStringOffsets().at(instruction.opcode);
    std::vector<uint8_t> result(instruction.raw.begin(), instruction.raw.begin() + (intptr_t)stringOffset);
    auto messageBytes = scriptMessageBytes(entry.message, instruction.suffix);
    result.append_range(messageBytes);
    result.push_back(0);
    return result;
}

[[nodiscard]] size_t remapOffset(size_t offset, const std::vector<size_t>& oldStarts, const std::vector<int64_t>& startDeltas)
{
    auto it = std::ranges::upper_bound(oldStarts, offset);
    if (it == oldStarts.begin()) {
        return offset;
    }
    auto index = (size_t)std::distance(oldStarts.begin(), it) - 1;
    auto remapped = (int64_t)offset + startDeltas[index];
    if (remapped < 0) {
        throw std::runtime_error("Remapped offset became negative");
    }
    return (size_t)remapped;
}

void patchControlFlow(
    const std::vector<Instruction>& instructions,
    std::vector<std::vector<uint8_t>>& rebuilt,
    const std::vector<size_t>& newStarts,
    const std::vector<size_t>& oldStarts,
    const std::vector<int64_t>& startDeltas)
{
    for (size_t index = 0; index < instructions.size(); ++index) {
        const auto& instruction = instructions[index];
        auto& blob = rebuilt[index];
        if (instruction.opcode == 0x01) {
            auto oldTarget = instruction.start + 11 + readU32(instruction.raw, 6);
            auto newTarget = remapOffset(oldTarget, oldStarts, startDeltas);
            auto newRel = (int64_t)newTarget - (int64_t)(newStarts[index] + 11);
            if (newRel < 0 || newRel > (int64_t)UINT32_MAX) {
                throw std::runtime_error("Patched relative jump is out of uint32 range");
            }
            writeU32(blob, 6, (uint32_t)newRel);
        }
        else if (instruction.opcode == 0x06) {
            auto oldTarget = readU32(instruction.raw, 1);
            auto newTarget = remapOffset(oldTarget, oldStarts, startDeltas);
            if (newTarget > UINT32_MAX) {
                throw std::runtime_error("Patched absolute jump is out of uint32 range");
            }
            writeU32(blob, 1, (uint32_t)newTarget);
        }
    }
}

[[nodiscard]] std::vector<uint8_t> readBinaryFile(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error(std::format("Failed to open file: {}", wide2Ascii(path)));
    }
    file.seekg(0, std::ios::end);
    auto size = (size_t)file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(size);
    if (size != 0) {
        file.read((char*)data.data(), (std::streamsize)size);
        if (!file) {
            throw std::runtime_error(std::format("Failed to read file: {}", wide2Ascii(path)));
        }
    }
    return data;
}

[[nodiscard]] std::string readTextFileUtf8(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error(std::format("Failed to open text file: {}", wide2Ascii(path)));
    }
    file.seekg(0, std::ios::end);
    auto size = (size_t)file.tellg();
    file.seekg(0, std::ios::beg);

    std::string text(size, '\0');
    if (size != 0) {
        file.read(text.data(), (std::streamsize)size);
        if (!file) {
            throw std::runtime_error(std::format("Failed to read text file: {}", wide2Ascii(path)));
        }
    }
    return text;
}

void writeBinaryFile(const fs::path& path, const std::vector<uint8_t>& data)
{
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path());
    }
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error(std::format("Failed to create file: {}", wide2Ascii(path)));
    }
    if (!data.empty()) {
        file.write((const char*)data.data(), (std::streamsize)data.size());
    }
    if (!file) {
        throw std::runtime_error(std::format("Failed to write file: {}", wide2Ascii(path)));
    }
}

void writeTextFileUtf8(const fs::path& path, const std::string& text)
{
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path());
    }
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error(std::format("Failed to create text file: {}", wide2Ascii(path)));
    }
    file.write(text.data(), (std::streamsize)text.size());
    if (!file) {
        throw std::runtime_error(std::format("Failed to write text file: {}", wide2Ascii(path)));
    }
}

[[nodiscard]] std::vector<fs::path> collectWscFiles(const fs::path& directory)
{
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (str2Lower(entry.path().extension()) == L".wsc") {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);
    return files;
}

[[nodiscard]] json textEntryToJson(const TextEntry& entry)
{
    json value = json::object();
    if (!entry.name.empty()) {
        value["name"] = entry.name;
    }
    value["message"] = entry.message;
    return value;
}

[[nodiscard]] TextEntry textEntryFromJson(const json& value)
{
    if (!value.is_object()) {
        throw std::runtime_error("JSON entry must be an object");
    }
    if (!value.contains("message") || !value["message"].is_string()) {
        throw std::runtime_error("JSON entry is missing string field 'message'");
    }
    TextEntry entry;
    entry.message = value["message"].get<std::string>();
    if (value.contains("name")) {
        if (!value["name"].is_string()) {
            throw std::runtime_error("JSON field 'name' must be a string");
        }
        entry.name = value["name"].get<std::string>();
    }
    return entry;
}

void dumpScripts(const fs::path& inputDir, const fs::path& outputDir)
{
    fs::create_directories(outputDir);
    size_t exportedFiles = 0;
    size_t exportedEntries = 0;

    for (const auto& wscPath : collectWscFiles(inputDir)) {
        auto data = readBinaryFile(wscPath);
        auto parsed = parseScript(data);
        json root = json::array();
        for (const auto& entry : parsed.exported) {
            root.push_back(textEntryToJson(entry));
        }

        auto outputPath = outputDir / wscPath.filename().replace_extension(L".json");
        writeTextFileUtf8(outputPath, root.dump(2));
        ++exportedFiles;
        exportedEntries += parsed.exported.size();
    }

    std::println("exported_files={}", exportedFiles);
    std::println("exported_entries={}", exportedEntries);
    std::println("output_dir={}", wide2Ascii(outputDir));
}

void injectScripts(const fs::path& inputBinDir, const fs::path& inputJsonDir, const fs::path& outputDir)
{
    fs::create_directories(outputDir);
    size_t patchedFiles = 0;
    size_t patchedEntries = 0;

    for (const auto& wscPath : collectWscFiles(inputBinDir)) {
        auto jsonPath = inputJsonDir / wscPath.filename().replace_extension(L".json");
        if (!fs::exists(jsonPath)) {
            continue;
        }

        auto data = readBinaryFile(wscPath);
        auto parsed = parseScript(data);
        auto root = json::parse(readTextFileUtf8(jsonPath));
        if (!root.is_array()) {
            throw std::runtime_error(std::format("JSON root must be an array: {}", wide2Ascii(jsonPath)));
        }

        std::vector<TextEntry> replacements;
        replacements.reserve(root.size());
        for (const auto& item : root) {
            replacements.push_back(textEntryFromJson(item));
        }

        if (replacements.size() != parsed.exported.size()) {
            throw std::runtime_error(std::format(
                "{}: entry count mismatch, expected {}, got {}",
                wide2Ascii(jsonPath.filename()),
                parsed.exported.size(),
                replacements.size()));
        }

        std::vector<std::vector<uint8_t>> rebuilt;
        rebuilt.reserve(parsed.instructions.size());
        std::vector<size_t> oldStarts;
        std::vector<size_t> newStarts;
        std::vector<int64_t> startDeltas;
        oldStarts.reserve(parsed.instructions.size());
        newStarts.reserve(parsed.instructions.size());
        startDeltas.reserve(parsed.instructions.size());

        size_t replacementIndex = 0;
        size_t newCursor = 0;
        for (const auto& instruction : parsed.instructions) {
            oldStarts.push_back(instruction.start);
            newStarts.push_back(newCursor);
            startDeltas.push_back((int64_t)newCursor - (int64_t)instruction.start);
            auto blob = rebuildInstruction(instruction, replacements, replacementIndex);
            newCursor += blob.size();
            rebuilt.push_back(std::move(blob));
        }

        if (replacementIndex != replacements.size()) {
            throw std::runtime_error(std::format("{}: unused replacement entries remain", wide2Ascii(jsonPath.filename())));
        }

        patchControlFlow(parsed.instructions, rebuilt, newStarts, oldStarts, startDeltas);

        std::vector<uint8_t> outData;
        outData.reserve(newCursor + TRAILER_SIZE);
        for (const auto& blob : rebuilt) {
            outData.append_range(blob);
        }
        outData.insert(outData.end(), data.end() - (intptr_t)TRAILER_SIZE, data.end());

        auto outputPath = outputDir / wscPath.filename();
        writeBinaryFile(outputPath, outData);
        ++patchedFiles;
        patchedEntries += replacements.size();
    }

    std::println("patched_files={}", patchedFiles);
    std::println("patched_entries={}", patchedEntries);
    std::println("output_dir={}", wide2Ascii(outputDir));
}


} // namespace

int main(int argc, char** argv)
{
    SetConsoleOutputCP(CP_UTF8);

    CLI::App app("Made by julixian 2026.03.22", "AdvHD WscScriptTool");
    argv = app.ensure_utf8(argv);
    app.set_help_all_flag("-a");
    app.require_subcommand(1);

    fs::path inputBinDir;
    fs::path inputJsonDir;
    fs::path outputDir;

    auto dumpCmd = app.add_subcommand("dump");
    dumpCmd->alias("export");
    dumpCmd->alias("-d");
    dumpCmd->add_option("inputDir", inputBinDir, "input directory")->required()->check(CLI::ExistingDirectory);
    dumpCmd->add_option("outputDir", outputDir, "output directory")->required();

    auto injectCmd = app.add_subcommand("inject");
    injectCmd->alias("import");
    injectCmd->alias("-i");
    injectCmd->add_option("inputBinDir", inputBinDir, "input bin directory")->required()->check(CLI::ExistingDirectory);
    injectCmd->add_option("inputJsonDir", inputJsonDir, "input json directory")->required()->check(CLI::ExistingDirectory);
    injectCmd->add_option("outputDir", outputDir, "output directory")->required();

    CLI11_PARSE(app, argc, argv);

    try {
        if (*dumpCmd) {
            dumpScripts(inputBinDir, outputDir);
        }
        else if (*injectCmd) {
            injectScripts(inputBinDir, inputJsonDir, outputDir);
        }
    }
    catch (const std::exception& e) {
        std::println(stderr, "Error: {}", e.what());
        return 1;
    }

    return 0;
}
