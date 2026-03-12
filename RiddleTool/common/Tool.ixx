module;

#include <Windows.h>
#include <cstdint>

export module Tool;

import std;

namespace fs = std::filesystem;

export  {

    template<typename T>
    T read(const void* ptr)
    {
        T result;
        memcpy(&result, ptr, sizeof(T));
        return result;
    }

    template<typename T>
    void write(void* ptr, T value)
    {
        memcpy(ptr, &value, sizeof(T));
    }

    template<typename T>
    T calculateAbs(T a, T b) {
        return a > b ? a - b : b - a;
    }

    std::string wide2Ascii(std::wstring_view wide, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr);
    template<typename ...Args>
    std::string wide2Ascii(const std::string&, Args&&... args) = delete;
    inline std::string wide2Ascii(const fs::path& path, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr)
    {
        return wide2Ascii(std::wstring_view(path.native()), codePage, usedDefaultChar);
    }
    inline std::string wide2Ascii(const std::wstring& wide, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr)
    {
        return wide2Ascii(std::wstring_view(wide), codePage, usedDefaultChar);
    }
    inline std::string wide2Ascii(const wchar_t* wide, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr)
    {
        return wide2Ascii(std::wstring_view(wide), codePage, usedDefaultChar);
    }
    std::wstring ascii2Wide(std::string_view ascii, UINT codePage = CP_ACP);
    std::string ascii2Ascii(std::string_view ascii, UINT src = CP_ACP, UINT dst = CP_UTF8, LPBOOL usedDefaultChar = nullptr);

    
    std::wstring str2Lower(std::wstring_view str) {
        return str | std::views::transform([](const auto c) { return std::tolower(c); }) | std::ranges::to<std::wstring>();
    }
    std::wstring str2Lower(const wchar_t* str) {
        return str2Lower(std::wstring_view(str));
    }
    std::string str2Lower(std::string_view str) {
        return str | std::views::transform([](const auto c) { return std::tolower(c); }) | std::ranges::to<std::string>();
    }
    std::string str2Lower(const char* str) {
        return str2Lower(std::string_view(str));
    }
    template <typename CharT, typename Traits, typename Alloc>
    auto str2Lower(const std::basic_string<CharT, Traits, Alloc>& str) {
        return str2Lower(std::basic_string_view<CharT>(str));
    }
    std::wstring str2Lower(const fs::path& path) {
        return str2Lower(path.native());
    }
    template <typename CharT, typename Traits, typename Alloc>
    auto& str2LowerInplace(std::basic_string<CharT, Traits, Alloc>& str) {
        std::transform(str.begin(), str.end(), str.begin(), [](const auto c) { return std::tolower(c); });
        return str;
    }

    std::string replaceStr(std::string_view str, std::string_view org, std::string_view rep);
    std::string& replaceStrInplace(std::string& str, std::string_view org, std::string_view rep);
    std::wstring replaceStr(std::wstring_view str, std::wstring_view org, std::wstring_view rep);
    std::wstring& replaceStrInplace(std::wstring& str, std::wstring_view org, std::wstring_view rep);
    int countSubstring(const std::string& text, std::string_view sub);
    int countSubstring(const std::wstring& text, std::wstring_view sub);

    std::vector<uint8_t> str2Vec(const std::string& str) {
        return str | std::views::transform([](const auto& c) { return (uint8_t)c; }) | std::ranges::to<std::vector>();
    }

}

module :private;

std::string wide2Ascii(std::wstring_view wide, UINT codePage, LPBOOL usedDefaultChar) {
    int len = WideCharToMultiByte(codePage, 0, wide.data(), (int)wide.length(), 
        nullptr, 0, nullptr, usedDefaultChar);
    if (len == 0) return {};
    std::string ascii(len, '\0');
    WideCharToMultiByte(codePage, 0, wide.data(), (int)wide.length(), 
        ascii.data(), len, nullptr, nullptr);
    return ascii;
}

std::wstring ascii2Wide(std::string_view ascii, UINT codePage) {
    int len = MultiByteToWideChar(codePage, 0, ascii.data(), (int)ascii.length(), nullptr, 0);
    if (len == 0) return {};
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(codePage, 0, ascii.data(), (int)ascii.length(), wide.data(), len);
    return wide;
}

std::string ascii2Ascii(std::string_view ascii, UINT src, UINT dst, LPBOOL usedDefaultChar) {
    return wide2Ascii(ascii2Wide(ascii, src), dst, usedDefaultChar);
}

std::string replaceStr(std::string_view str, std::string_view org, std::string_view rep) {
    return str | std::views::split(org) | std::views::join_with(rep) | std::ranges::to<std::string>();
}

std::string& replaceStrInplace(std::string& str, std::string_view org, std::string_view rep) {
    str = str | std::views::split(org) | std::views::join_with(rep) | std::ranges::to<std::string>();
    return str;
}

std::wstring replaceStr(std::wstring_view str, std::wstring_view org, std::wstring_view rep) {
    return str | std::views::split(org) | std::views::join_with(rep) | std::ranges::to<std::wstring>();
}

std::wstring& replaceStrInplace(std::wstring& str, std::wstring_view org, std::wstring_view rep) {
    str = str | std::views::split(org) | std::views::join_with(rep) | std::ranges::to<std::wstring>();
    return str;
}

int countSubstring(const std::string& text, std::string_view sub) {
    return (int)std::ranges::distance(text | std::views::split(sub)) - 1;
}

int countSubstring(const std::wstring& text, std::wstring_view sub) {
    return (int)std::ranges::distance(text | std::views::split(sub)) - 1;
}
