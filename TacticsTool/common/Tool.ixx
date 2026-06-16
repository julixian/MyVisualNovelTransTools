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
    inline std::string wide2Ascii(const fs::path& path, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr)
    {
        return wide2Ascii(std::wstring_view(path.native()), codePage, usedDefaultChar);
    }
    template<typename T>
        requires(!std::is_same_v<std::remove_cvref_t<T>, fs::path> &&
    !std::is_same_v<std::remove_cvref_t<T>, std::wstring_view>)
        inline std::string wide2Ascii(T&& wide, UINT codePage = CP_UTF8, LPBOOL usedDefaultChar = nullptr) {
        return wide2Ascii(std::wstring_view(wide), codePage, usedDefaultChar);
    }

    std::wstring ascii2Wide(std::string_view ascii, UINT codePage = CP_ACP);
    std::string ascii2Ascii(std::string_view ascii, UINT src = CP_ACP, UINT dst = CP_UTF8, LPBOOL usedDefaultChar = nullptr);

    template<typename RetT>
    RetT str2LowerImpl(auto&& str) {
        return str | std::views::transform([](const auto c) { return std::tolower(c); }) | std::ranges::to<RetT>();
    }
    template<typename T>
        requires(!std::is_same_v<std::remove_cvref_t<T>, fs::path>)
    inline auto str2Lower(T&& str) {
        if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::wstring_view>) {
            return str2LowerImpl<std::wstring>(str);
        }
        if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::string_view>) {
            return str2LowerImpl<std::string>(str);
        }
        if constexpr (std::constructible_from<std::wstring_view, T>) {
            return str2LowerImpl<std::wstring>(std::wstring_view(str));
        }
        if constexpr (std::constructible_from<std::string_view, T>) {
            return str2LowerImpl<std::string>(std::string_view(str));
        }
    }
    std::wstring str2Lower(const fs::path& path) {
        return str2Lower(std::wstring_view(path.native()));
    }
    template <typename CharT, typename Traits, typename Alloc>
    auto& str2LowerInplace(std::basic_string<CharT, Traits, Alloc>& str) {
        std::transform(str.begin(), str.end(), str.begin(), [](const auto c) { return std::tolower(c); });
        return str;
    }

    std::string replaceStr(std::string_view strv, std::string_view org, std::string_view rep);
    std::string& replaceStrInplace(std::string& str, std::string_view org, std::string_view rep);
    std::wstring replaceStr(std::wstring_view strv, std::wstring_view org, std::wstring_view rep);
    std::wstring& replaceStrInplace(std::wstring& str, std::wstring_view org, std::wstring_view rep);
    int countSubstring(const std::string& text, std::string_view sub);
    int countSubstring(const std::wstring& text, std::wstring_view sub);

    std::vector<uint8_t> str2Vec(const std::string& str) {
        return str | std::views::transform([](const auto& c) { return (uint8_t)c; }) | std::ranges::to<std::vector>();
    }

}

module :private;

std::string wide2Ascii(std::wstring_view wide, UINT codePage, LPBOOL usedDefaultChar) {
#ifndef IMPL_WIDE_TO_ASCII
#define IMPL_WIDE_TO_ASCII WideCharToMultiByte
#endif
    int len = IMPL_WIDE_TO_ASCII
    (codePage, 0, wide.data(), (int)wide.length(), nullptr, 0, nullptr, usedDefaultChar);
    if (len == 0) return {};
    std::string ascii(len, '\0');
    IMPL_WIDE_TO_ASCII
    (codePage, 0, wide.data(), (int)wide.length(), ascii.data(), len, nullptr, nullptr);
    return ascii;
}

std::wstring ascii2Wide(std::string_view ascii, UINT codePage) {
#ifndef IMPL_ASCII_TO_WIDE
#define IMPL_ASCII_TO_WIDE MultiByteToWideChar
#endif
    int len = IMPL_ASCII_TO_WIDE(codePage, 0, ascii.data(), (int)ascii.length(), nullptr, 0);
    if (len == 0) return {};
    std::wstring wide(len, L'\0');
    IMPL_ASCII_TO_WIDE(codePage, 0, ascii.data(), (int)ascii.length(), wide.data(), len);
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
