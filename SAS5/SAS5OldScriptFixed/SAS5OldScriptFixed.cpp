#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

// =========================
// Basic Helpers
// =========================

static void ensureRange(size_t total, size_t off, size_t len, const char* what)
{
    if (off > total || len > total - off) {
        std::ostringstream oss;
        oss << what << " out of range at 0x" << std::hex << std::uppercase << off;
        throw std::runtime_error(oss.str());
    }
}

template<typename T>
T readValue(const std::vector<uint8_t>& buf, size_t off)
{
    ensureRange(buf.size(), off, sizeof(T), "readValue");
    T value{};
    std::memcpy(&value, buf.data() + off, sizeof(T));
    return value;
}

template<typename T>
void writeValue(std::vector<uint8_t>& buf, size_t off, T value)
{
    ensureRange(buf.size(), off, sizeof(T), "writeValue");
    std::memcpy(buf.data() + off, &value, sizeof(T));
}

template<typename T>
void appendValue(std::vector<uint8_t>& buf, T value)
{
    size_t pos = buf.size();
    buf.resize(pos + sizeof(T));
    std::memcpy(buf.data() + pos, &value, sizeof(T));
}

static std::string hexValue(uint64_t v)
{
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << v;
    return oss.str();
}

static bool startsWith(const std::string& s, const std::string& prefix)
{
    return s.size() >= prefix.size() &&
        std::memcmp(s.data(), prefix.data(), prefix.size()) == 0;
}

static void replaceAll(std::string& s, const std::string& from, const std::string& to)
{
    if (from.empty()) return;

    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

static bool isNoArgOpcode(uint8_t op)
{
    if (op >= 0x80 && op <= 0xB8) return true;

    switch (op) {
    case 0xC0:
    case 0xC8:
    case 0xC9:
    case 0xD0:
    case 0xD1:
    case 0xD2:
    case 0xD4:
    case 0xD5:
    case 0xD6:
    case 0xD7:
    case 0xD8:
    case 0xD9:
    case 0xDA:
    case 0xDB:
    case 0xDC:
    case 0xDD:
    case 0xDE:
        return true;
    default:
        return false;
    }
}

static std::vector<uint8_t> readAllBytes(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open file: " + path.string());
    }
    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>()
    );
}

static std::string readAllTextBinary(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open file: " + path.string());
    }

    std::ostringstream oss;
    oss << in.rdbuf();
    return oss.str();
}

static std::vector<std::string> splitLinesUtf8(std::string raw)
{
    // Strip UTF-8 BOM
    if (startsWith(raw, "\xEF\xBB\xBF")) {
        raw.erase(0, 3);
    }

    std::vector<std::string> lines;
    size_t start = 0;

    while (start < raw.size()) {
        size_t end = raw.find('\n', start);
        if (end == std::string::npos) {
            std::string line = raw.substr(start);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(line);
            return lines;
        }

        std::string line = raw.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);

        start = end + 1;
        if (start == raw.size()) {
            // trailing '\n' does not create an extra line
            return lines;
        }
    }

    return lines;
}

// =========================
// Encoding
// =========================

static std::string codePageBytesToUtf8(const uint8_t* data, size_t len, UINT codePage)
{
    if (len == 0) return "";

    int wlen = MultiByteToWideChar(
        codePage,
        0,
        reinterpret_cast<LPCCH>(data),
        static_cast<int>(len),
        nullptr,
        0
    );
    if (wlen <= 0) {
        throw std::runtime_error("MultiByteToWideChar failed.");
    }

    std::wstring wide(static_cast<size_t>(wlen), L'\0');
    if (MultiByteToWideChar(
        codePage,
        0,
        reinterpret_cast<LPCCH>(data),
        static_cast<int>(len),
        &wide[0],
        wlen
    ) <= 0) {
        throw std::runtime_error("MultiByteToWideChar failed.");
    }

    int u8len = WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.data(),
        wlen,
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (u8len <= 0) {
        throw std::runtime_error("WideCharToMultiByte(CP_UTF8) failed.");
    }

    std::string utf8(static_cast<size_t>(u8len), '\0');
    if (WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.data(),
        wlen,
        &utf8[0],
        u8len,
        nullptr,
        nullptr
    ) <= 0) {
        throw std::runtime_error("WideCharToMultiByte(CP_UTF8) failed.");
    }

    return utf8;
}

static std::vector<uint8_t> utf8ToCodePageBytes(const std::string& utf8, UINT codePage)
{
    if (utf8.empty()) return {};

    int wlen = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        utf8.data(),
        static_cast<int>(utf8.size()),
        nullptr,
        0
    );
    if (wlen <= 0) {
        throw std::runtime_error("Invalid UTF-8 text.");
    }

    std::wstring wide(static_cast<size_t>(wlen), L'\0');
    if (MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        utf8.data(),
        static_cast<int>(utf8.size()),
        &wide[0],
        wlen
    ) <= 0) {
        throw std::runtime_error("Invalid UTF-8 text.");
    }

    BOOL usedDefaultChar = FALSE;
    int outLen = WideCharToMultiByte(
        codePage,
        WC_NO_BEST_FIT_CHARS,
        wide.data(),
        wlen,
        nullptr,
        0,
        nullptr,
        &usedDefaultChar
    );
    if (outLen <= 0) {
        throw std::runtime_error("WideCharToMultiByte(target code page) failed.");
    }

    std::vector<uint8_t> out(static_cast<size_t>(outLen));
    usedDefaultChar = FALSE;
    if (WideCharToMultiByte(
        codePage,
        WC_NO_BEST_FIT_CHARS,
        wide.data(),
        wlen,
        reinterpret_cast<LPSTR>(out.data()),
        outLen,
        nullptr,
        &usedDefaultChar
    ) <= 0) {
        throw std::runtime_error("WideCharToMultiByte(target code page) failed.");
    }

    if (usedDefaultChar) {
        throw std::runtime_error("Text contains characters not representable in target code page.");
    }

    return out;
}

// =========================
// Script Structures
// =========================

enum class TextKind
{
    Cmd,
    Raw
};

struct TextSlot
{
    uint32_t index = 0;
    uint32_t oldAddr = 0;
    uint32_t oldLen = 0;
    TextKind kind = TextKind::Cmd;
    std::vector<uint8_t> data;
    std::string prefix;
};

struct ScanResult
{
    std::vector<TextSlot> slots;
    std::unordered_set<uint32_t> validTargets;
};

struct AbsFixup
{
    size_t patchPosNew = 0;
    uint32_t oldTarget = 0;
    uint8_t opcode = 0;
    uint32_t immPosOld = 0;
};

struct RelFixup
{
    size_t patchPosNew = 0;
    uint32_t oldBase = 0;
    uint32_t oldTarget = 0;
    uint8_t opcode = 0;
    uint32_t immPosOld = 0;
};

struct DeltaRec
{
    uint32_t oldTextAddr = 0;
    int32_t delta = 0;
};

// =========================
// Text Prefix Helpers
// =========================

static std::string getSpecPrefix(uint16_t executorType, uint8_t argId, int argCount)
{
    if ((((executorType == 0x1C) || (executorType == 0x28)) && argId == 0x02 && argCount == 3) ||
        (executorType == 0x200 && argId == 0x00)) {
        return "[Spec1]";
    }
    return "[Spec2]";
}

static std::string escapeTextForDump(std::string s)
{
    replaceAll(s, "\r", "[r]");
    replaceAll(s, "\n", "[n]");
    return s;
}

static std::string stripLeadingMarkers(std::string line)
{
    while (true) {
        bool changed = false;

        if (startsWith(line, "[pre_unfinish]")) {
            line.erase(0, 14);
            changed = true;
        }
        if (startsWith(line, "[Spec1]")) {
            line.erase(0, 7);
            changed = true;
        }
        else if (startsWith(line, "[Spec2]")) {
            line.erase(0, 7);
            changed = true;
        }

        if (!changed) break;
    }

    replaceAll(line, "[r]", "\r");
    replaceAll(line, "[n]", "\n");
    return line;
}

// =========================
// First Pass Scan
// =========================

static ScanResult scanScript(const std::vector<uint8_t>& buf)
{
    ScanResult result;
    size_t off = 0;
    uint32_t textIndex = 0;

    while (off < buf.size()) {
        uint32_t itemStart = static_cast<uint32_t>(off);
        uint8_t type = buf[off++];
        result.validTargets.insert(itemStart);

        if (type == 0x1A) {
            continue;
        }
        else if (type == 0x1B) {
            uint16_t executorType = readValue<uint16_t>(buf, off);
            off += 2;

            bool preUnfinishPending = (executorType == 0x1F8);
            int argCount = 0;

            while (true) {
                ensureRange(buf.size(), off, 1, "read argId(scan)");
                uint8_t argId = buf[off++];
                ++argCount;

                if (argId == 0xFF) {
                    break;
                }

                while (true) {
                    ensureRange(buf.size(), off, 1, "read opCode(scan)");
                    uint8_t opCode = buf[off++];

                    if (opCode == 0xFF) {
                        break;
                    }

                    switch (opCode) {
                    case 0x01:
                        ensureRange(buf.size(), off, 4, "op 0x01");
                        off += 4;
                        break;

                    case 0x02:
                        ensureRange(buf.size(), off, 2, "op 0x02");
                        off += 2;
                        break;

                    case 0x03:
                        ensureRange(buf.size(), off, 1, "op 0x03");
                        off += 1;
                        break;

                    case 0x04: {
                        uint32_t len = readValue<uint32_t>(buf, off);
                        off += 4;
                        ensureRange(buf.size(), off, len, "op 0x04 string");

                        uint32_t textPos = static_cast<uint32_t>(off);
                        result.validTargets.insert(textPos);

                        TextSlot slot;
                        slot.index = textIndex++;
                        slot.oldAddr = textPos;
                        slot.oldLen = len;
                        slot.kind = TextKind::Cmd;
                        slot.data.assign(buf.begin() + off, buf.begin() + off + len);

                        if (preUnfinishPending) {
                            slot.prefix += "[pre_unfinish]";
                            preUnfinishPending = false;
                        }
                        slot.prefix += getSpecPrefix(executorType, argId, argCount);

                        result.slots.push_back(std::move(slot));
                        off += len;
                        break;
                    }

                    case 0x05:
                        ensureRange(buf.size(), off, 8, "op 0x05");
                        off += 8;
                        break;

                    case 0x10:
                    case 0x11:
                    case 0x12:
                    case 0x13:
                        ensureRange(buf.size(), off, 4, "op 0x10~0x13");
                        off += 4;
                        break;

                    default:
                        if (isNoArgOpcode(opCode)) {
                            break;
                        }
                        throw std::runtime_error("Unknown opCode: " + hexValue(opCode) + " at " + hexValue(off - 1));
                    }
                }
            }
        }
        else if (type >= 0x20) {
            while (off < buf.size() && buf[off] >= 0x20) {
                ++off;
            }

            TextSlot slot;
            slot.index = textIndex++;
            slot.oldAddr = itemStart;
            slot.oldLen = static_cast<uint32_t>(off - itemStart);
            slot.kind = TextKind::Raw;
            slot.data.assign(buf.begin() + itemStart, buf.begin() + off);

            result.slots.push_back(std::move(slot));
        }
        else {
            throw std::runtime_error("Unknown top-level type: " + hexValue(type) + " at " + hexValue(itemStart));
        }
    }

    return result;
}

// =========================
// Jump Fix Heuristic
// =========================

enum class FixMode
{
    None,
    Abs,
    Rel
};

static FixMode chooseFixMode(uint8_t opcode, bool absOk, bool relOk)
{
    if (absOk && !relOk) return FixMode::Abs;
    if (relOk && !absOk) return FixMode::Rel;

    if (absOk && relOk) {
        // 0x01 prefers ABS
        // 0x10~0x13 prefer REL
        return (opcode == 0x01) ? FixMode::Abs : FixMode::Rel;
    }

    return FixMode::None;
}

// =========================
// Address Remapper
// =========================

class AddressRemapper
{
public:
    explicit AddressRemapper(const std::vector<DeltaRec>& deltas)
    {
        std::vector<DeltaRec> sorted = deltas;
        std::sort(sorted.begin(), sorted.end(),
            [](const DeltaRec& a, const DeltaRec& b) {
                return a.oldTextAddr < b.oldTextAddr;
            });

        int64_t total = 0;
        for (const auto& d : sorted) {
            if (d.delta == 0) continue;
            starts_.push_back(d.oldTextAddr);
            total += d.delta;
            cums_.push_back(total);
        }
    }

    uint32_t remap(uint32_t oldAddr) const
    {
        auto it = std::lower_bound(starts_.begin(), starts_.end(), oldAddr);
        if (it == starts_.begin()) {
            return oldAddr;
        }

        size_t idx = static_cast<size_t>((it - starts_.begin()) - 1);
        int64_t mapped = static_cast<int64_t>(oldAddr) + cums_[idx];

        if (mapped < 0 || mapped > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Address remap overflow: " + hexValue(oldAddr));
        }
        return static_cast<uint32_t>(mapped);
    }

private:
    std::vector<uint32_t> starts_;
    std::vector<int64_t> cums_;
};

// =========================
// Dump
// =========================

static void dumpText(const fs::path& inputPath, const fs::path& outputPath, UINT codePage)
{
    std::vector<uint8_t> buf = readAllBytes(inputPath);
    ScanResult scan = scanScript(buf);

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Cannot open output: " + outputPath.string());
    }

    // UTF-8 BOM
    const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
    out.write(reinterpret_cast<const char*>(bom), 3);

    for (const auto& slot : scan.slots) {
        std::string utf8 = codePageBytesToUtf8(slot.data.data(), slot.data.size(), codePage);
        utf8 = escapeTextForDump(utf8);

        std::string line = slot.prefix + utf8;
        out.write(line.data(), static_cast<std::streamsize>(line.size()));
        out.write("\n", 1);
    }

    std::cout << "[OK] Dump complete: " << outputPath.string() << "\n";
    std::cout << "     Text count: " << scan.slots.size() << "\n";
    std::cout << "     Script code page: " << codePage << "\n";
    std::cout << "     TXT encoding: UTF-8\n";
}

// =========================
// Inject
// =========================

static void injectText(const fs::path& inputBinPath, const fs::path& inputTxtPath, const fs::path& outputBinPath, UINT codePage)
{
    std::vector<uint8_t> buf = readAllBytes(inputBinPath);
    ScanResult scan = scanScript(buf);

    std::vector<std::string> lines = splitLinesUtf8(readAllTextBinary(inputTxtPath));
    if (lines.size() != scan.slots.size()) {
        std::ostringstream oss;
        oss << "Text count mismatch. Script needs " << scan.slots.size()
            << " lines, txt has " << lines.size() << " lines.";
        throw std::runtime_error(oss.str());
    }

    std::vector<std::vector<uint8_t>> translated;
    translated.reserve(lines.size());

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string plain = stripLeadingMarkers(lines[i]);
        try {
            translated.push_back(utf8ToCodePageBytes(plain, codePage));
        }
        catch (const std::exception& e) {
            std::ostringstream oss;
            oss << "Line " << (i + 1) << ": " << e.what();
            throw std::runtime_error(oss.str());
        }
    }

    std::vector<uint8_t> newBuf;
    newBuf.reserve(buf.size() + 0x1000);

    std::vector<DeltaRec> deltaRecords;
    std::vector<AbsFixup> absFixups;
    std::vector<RelFixup> relFixups;
    std::vector<std::string> warnings;

    size_t off = 0;
    size_t textIndex = 0;

    while (off < buf.size()) {
        size_t itemStart = off;
        uint8_t type = buf[off++];
        
        if (type == 0x1A) {
            newBuf.push_back(0x1A);
        }
        else if (type == 0x1B) {
            newBuf.push_back(0x1B);

            ensureRange(buf.size(), off, 2, "executorType(inject)");
            newBuf.insert(newBuf.end(), buf.begin() + off, buf.begin() + off + 2);
            off += 2;

            while (true) {
                ensureRange(buf.size(), off, 1, "argId(inject)");
                uint8_t argId = buf[off++];
                newBuf.push_back(argId);

                if (argId == 0xFF) {
                    break;
                }

                while (true) {
                    ensureRange(buf.size(), off, 1, "opCode(inject)");
                    uint8_t opCode = buf[off++];
                    newBuf.push_back(opCode);

                    if (opCode == 0xFF) {
                        break;
                    }

                    switch (opCode) {
                    case 0x01:
                    case 0x10:
                    case 0x11:
                    case 0x12:
                    case 0x13: {
                        ensureRange(buf.size(), off, 4, "jump/immediate(inject)");

                        uint32_t absVal = readValue<uint32_t>(buf, off);
                        int32_t relVal = readValue<int32_t>(buf, off);
                        uint32_t oldBase = static_cast<uint32_t>(off + 4);

                        int64_t relTarget64 = static_cast<int64_t>(oldBase) + static_cast<int64_t>(relVal);
                        bool relRangeOk =
                            (relTarget64 >= 0) &&
                            (relTarget64 <= static_cast<int64_t>(std::numeric_limits<uint32_t>::max()));

                        uint32_t relTarget = relRangeOk ? static_cast<uint32_t>(relTarget64) : 0;

                        bool absOk =
                            (absVal != 0) &&
                            (scan.validTargets.find(absVal) != scan.validTargets.end());

                        bool relOk =
                            relRangeOk &&
                            (relTarget64 < static_cast<int64_t>(buf.size())) &&
                            (scan.validTargets.find(relTarget) != scan.validTargets.end());

                        size_t patchPosNew = newBuf.size();
                        newBuf.insert(newBuf.end(), buf.begin() + off, buf.begin() + off + 4);

                        uint32_t immPosOld = static_cast<uint32_t>(off);
                        off += 4;

                        FixMode mode = chooseFixMode(opCode, absOk, relOk);

                        if (mode == FixMode::Abs) {
                            AbsFixup fx;
                            fx.patchPosNew = patchPosNew;
                            fx.oldTarget = absVal;
                            fx.opcode = opCode;
                            fx.immPosOld = immPosOld;
                            absFixups.push_back(fx);

                            if (relOk) {
                                warnings.push_back(
                                    hexValue(immPosOld) + ": opcode " + hexValue(opCode) +
                                    " ABS/REL both matched, use ABS"
                                );
                            }
                            else if (opCode != 0x01) {
                                warnings.push_back(
                                    hexValue(immPosOld) + ": opcode " + hexValue(opCode) +
                                    " only ABS matched, use ABS"
                                );
                            }
                        }
                        else if (mode == FixMode::Rel) {
                            RelFixup fx;
                            fx.patchPosNew = patchPosNew;
                            fx.oldBase = oldBase;
                            fx.oldTarget = relTarget;
                            fx.opcode = opCode;
                            fx.immPosOld = immPosOld;
                            relFixups.push_back(fx);

                            if (absOk) {
                                warnings.push_back(
                                    hexValue(immPosOld) + ": opcode " + hexValue(opCode) +
                                    " ABS/REL both matched, use REL"
                                );
                            }
                            else if (opCode == 0x01) {
                                warnings.push_back(
                                    hexValue(immPosOld) + ": opcode 0x1 only REL matched, use REL"
                                );
                            }
                        }
                        break;
                    }

                    case 0x02:
                        ensureRange(buf.size(), off, 2, "op 0x02(inject)");
                        newBuf.insert(newBuf.end(), buf.begin() + off, buf.begin() + off + 2);
                        off += 2;
                        break;

                    case 0x03:
                        ensureRange(buf.size(), off, 1, "op 0x03(inject)");
                        newBuf.push_back(buf[off]);
                        off += 1;
                        break;

                    case 0x04: {
                        uint32_t oldLen = readValue<uint32_t>(buf, off);
                        off += 4;
                        ensureRange(buf.size(), off, oldLen, "op 0x04 string(inject)");

                        uint32_t oldTextPos = static_cast<uint32_t>(off);

                        const std::vector<uint8_t>& newText = translated[textIndex++];
                        if (newText.size() > std::numeric_limits<uint32_t>::max()) {
                            throw std::runtime_error("Text too long.");
                        }

                        uint32_t newLen = static_cast<uint32_t>(newText.size());
                        appendValue<uint32_t>(newBuf, newLen);
                        newBuf.insert(newBuf.end(), newText.begin(), newText.end());

                        DeltaRec d;
                        d.oldTextAddr = oldTextPos;
                        d.delta = static_cast<int32_t>(newLen) - static_cast<int32_t>(oldLen);
                        deltaRecords.push_back(d);

                        off += oldLen;
                        break;
                    }

                    case 0x05:
                        ensureRange(buf.size(), off, 8, "op 0x05(inject)");
                        newBuf.insert(newBuf.end(), buf.begin() + off, buf.begin() + off + 8);
                        off += 8;
                        break;

                    default:
                        if (isNoArgOpcode(opCode)) {
                            break;
                        }
                        throw std::runtime_error("Unknown opCode: " + hexValue(opCode) + " at " + hexValue(off - 1));
                    }
                }
            }
        }
        else if (type >= 0x20) {
            while (off < buf.size() && buf[off] >= 0x20) {
                ++off;
            }

            uint32_t oldLen = static_cast<uint32_t>(off - itemStart);
            const std::vector<uint8_t>& newText = translated[textIndex++];

            newBuf.insert(newBuf.end(), newText.begin(), newText.end());

            DeltaRec d;
            d.oldTextAddr = static_cast<uint32_t>(itemStart);
            d.delta = static_cast<int32_t>(newText.size()) - static_cast<int32_t>(oldLen);
            deltaRecords.push_back(d);
        }
        else {
            throw std::runtime_error("Unknown top-level type: " + hexValue(type) + " at " + hexValue(itemStart));
        }
    }

    if (textIndex != translated.size()) {
        throw std::runtime_error("Internal error: translated text count mismatch.");
    }

    AddressRemapper remapper(deltaRecords);

    // Fix absolute addresses
    for (const auto& fx : absFixups) {
        uint32_t newTarget = remapper.remap(fx.oldTarget);
        writeValue<uint32_t>(newBuf, fx.patchPosNew, newTarget);
    }

    // Fix relative addresses
    for (const auto& fx : relFixups) {
        uint32_t newBase = remapper.remap(fx.oldBase);
        uint32_t newTarget = remapper.remap(fx.oldTarget);

        int64_t rel64 = static_cast<int64_t>(newTarget) - static_cast<int64_t>(newBase);
        if (rel64 < std::numeric_limits<int32_t>::min() ||
            rel64 > std::numeric_limits<int32_t>::max()) {
            throw std::runtime_error("Relative jump overflow at " + hexValue(fx.immPosOld));
        }

        writeValue<int32_t>(newBuf, fx.patchPosNew, static_cast<int32_t>(rel64));
    }

    std::ofstream out(outputBinPath, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Cannot open output: " + outputBinPath.string());
    }
    out.write(reinterpret_cast<const char*>(newBuf.data()), static_cast<std::streamsize>(newBuf.size()));

    std::cout << "[OK] Inject complete: " << outputBinPath.string() << "\n";
    std::cout << "     Text count: " << translated.size() << "\n";
    std::cout << "     Fixed ABS jumps: " << absFixups.size() << "\n";
    std::cout << "     Fixed REL jumps: " << relFixups.size() << "\n";
    std::cout << "     Script code page: " << codePage << "\n";

    if (!warnings.empty()) {
        std::cout << "     Warnings: " << warnings.size() << "\n";
        size_t show = std::min<size_t>(warnings.size(), 20);
        for (size_t i = 0; i < show; ++i) {
            std::cout << "       - " << warnings[i] << "\n";
        }
        if (warnings.size() > show) {
            std::cout << "       ... " << (warnings.size() - show) << " more\n";
        }
    }
}

// =========================
// CLI
// =========================

static UINT parseCodePageOrDefault(int argc, char* argv[], int argIndex, UINT defaultCodePage)
{
    if (argc <= argIndex) {
        return defaultCodePage;
    }

    try {
        unsigned long v = std::stoul(argv[argIndex]);
        if (v > std::numeric_limits<UINT>::max()) {
            throw std::runtime_error("CodePage value too large.");
        }
        return static_cast<UINT>(v);
    }
    catch (...) {
        throw std::runtime_error("Invalid codePage: " + std::string(argv[argIndex]));
    }
}

static void printUsage(const fs::path& programPath)
{
    std::cout << "SAS5 Old Script Fixed Tool\n";
    std::cout << "TXT file encoding: UTF-8\n";
    std::cout << "Default script code page: 932 (CP932 / Shift-JIS)\n";
    std::cout << "\n";
    std::cout << "Usage:\n";
    std::cout << "  Dump:   " << programPath.filename().string() << " dump <script.bin> <out.txt> [codePage]\n";
    std::cout << "  Inject: " << programPath.filename().string() << " inject <script.bin> <in.txt> <new.bin> [codePage]\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programPath.filename().string() << " dump script.bin out.txt\n";
    std::cout << "  " << programPath.filename().string() << " inject script.bin out.txt script_new.bin\n";
    std::cout << "  " << programPath.filename().string() << " inject script.bin out.txt script_new.bin 936\n";
}

int main(int argc, char* argv[])
{
    try {
        if (argc < 2) {
            printUsage(argv[0]);
            return 1;
        }

        std::string mode = argv[1];

        if (mode == "dump") {
            if (argc < 4 || argc > 5) {
                printUsage(argv[0]);
                return 1;
            }

            UINT codePage = parseCodePageOrDefault(argc, argv, 4, 932);
            dumpText(argv[2], argv[3], codePage);
        }
        else if (mode == "inject") {
            if (argc < 5 || argc > 6) {
                printUsage(argv[0]);
                return 1;
            }

            UINT codePage = parseCodePageOrDefault(argc, argv, 5, 932);
            injectText(argv[2], argv[3], argv[4], codePage);
        }
        else {
            printUsage(argv[0]);
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return 1;
    }

    return 0;
}