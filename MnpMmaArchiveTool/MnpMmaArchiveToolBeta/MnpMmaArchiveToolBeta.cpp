#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <functional>
#include <filesystem>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <Windows.h>

namespace fs = std::filesystem;

typedef int(__stdcall* InitializeMME_t)(int);
typedef int(__stdcall* MME_SetArchiveName_t)(const char*, int);
typedef int(__stdcall* MME_GetDataSize_t)(int);
typedef int(__stdcall* MME_GetMemory_t)(int, void*, unsigned int);
typedef int(__stdcall* MME_GetGraphicSize_t)(int, void*, void*);

int main(int argc, char* argv[]) {

    HMODULE hArc = LoadLibraryW(L"ARC.dll");

    InitializeMME_t InitializeMME = (InitializeMME_t)GetProcAddress(hArc, "InitializeMME");
    MME_SetArchiveName_t MME_SetArchiveName = (MME_SetArchiveName_t)GetProcAddress(hArc, "MME_SetArchiveName");
    MME_GetDataSize_t MME_GetDataSize = (MME_GetDataSize_t)GetProcAddress(hArc, "MME_GetDataSize");
    MME_GetMemory_t MME_GetMemory = (MME_GetMemory_t)GetProcAddress(hArc, "MME_GetMemory");
    MME_GetGraphicSize_t MME_GetGraphicSize = (MME_GetGraphicSize_t)GetProcAddress(hArc, "MME_GetGraphicSize");

    InitializeMME(8);
    int ret = MME_SetArchiveName("data0.mma", 0);
    ret = MME_SetArchiveName("data1.mma", 1);

    fs::create_directories(L"output");

    std::cout << "MME_SetArchiveName returned " << ret << std::endl;

    for (int i = 0; ; i++) {
        int size = MME_GetDataSize(i);
        if (size == -1) {
            break;
        }
        std::cout << "MME_GetDataSize returned " << size << std::endl;
        int width = 0, height = 0;
        MME_GetGraphicSize(i, &width, &height);
        std::cout << "MME_GetGraphicSize returned " << width << "x" << height << std::endl;

        std::vector<char> data(size);
        MME_GetMemory(i, data.data(), size);
        std::ofstream ofs(fs::path(L"output") / (L"data" + std::to_wstring(i) + L".bin"), std::ios::binary);
        ofs.write(data.data(), size);
        ofs.close();
    }

    return 0;
}
