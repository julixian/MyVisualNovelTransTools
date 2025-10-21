#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <windows.h>

// Main header for the ERISA library.
#include "xerisa.h"

std::string wide2Ascii(const std::wstring& wide, UINT CodePage) {
    int len = WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return "";
    std::string ascii(len, '\0');
    WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
    ascii.pop_back();
    return ascii;
}

std::wstring ascii2Wide(const std::string& ascii, UINT CodePage) {
    int len = MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, nullptr, 0);
    if (len == 0) return L"";
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CodePage, 0, ascii.c_str(), -1, &wide[0], len);
    wide.pop_back();
    return wide;
}

std::string ascii2Ascii(const std::string& ascii, UINT src, UINT dst) {
    return wide2Ascii(ascii2Wide(ascii, src), dst);
}

// Provide stub implementations for missing virtual functions for the linker.
const char* EPtrArray::GetClassNameW(void) const { return "EPtrArray"; }
const char* ESLObject::GetClassNameW(void) const { return "ESLObject"; }

// Struct to hold an extracted file's path and data in memory.
struct ExtractedFile {
    std::filesystem::path diskPath; // Stored as a Unicode-aware path object.
    std::vector<BYTE> data;
};

// Forward declaration of the recursive function.
void readArchiveToMemory(
    ERISAArchive* archive,
    const std::filesystem::path& current_path_in_archive,
    const char* password,
    std::vector<ExtractedFile>& extracted_files
);

int main(int argc, char* argv[])
{
    SetConsoleOutputCP(65001); // Set console output code page to UTF-8.
    if (argc < 3 || argc > 4)
    {
        std::cout << "Made by julixian 2025.07.07" << std::endl;
        std::cerr << "Usage: " << argv[0] << " <input.noa> <output_folder> [password]" << std::endl;
        std::cerr << "Example: " << argv[0] << " data.noa output" << std::endl;
        std::cerr << "Example: " << argv[0] << " script.noa output MARSELUD" << std::endl;
        std::cerr << "Example: " << argv[0] << " script.noa output convini_cat" << std::endl;
        return 1;
    }
    
    std::filesystem::path inputNoaPath(argv[1]);
    std::filesystem::path outputFolderPath(argv[2]);

    // Check if a password was provided.
    const char* password = (argc == 4) ? argv[3] : nullptr;

    std::cout << "ERISA NOA Archive Extractor" << std::endl;
    std::cout << "--------------------------------------------" << std::endl;
    std::cout << "Input File: " << wide2Ascii(inputNoaPath.wstring(), 65001) << std::endl;
    std::cout << "Output Directory: " << wide2Ascii(outputFolderPath.wstring(), 65001) << std::endl;
    if (password) {
        std::cout << "Using Password: " << password << std::endl;
    }
    else {
        std::cout << "No Password Provided" << std::endl;
    }
    std::cout << std::endl;

    // --- Open the archive ---
    ERawFile* rawFile = new ERawFile();
    if (rawFile->Open(inputNoaPath.string().c_str(), ESLFileObject::modeRead) != eslErrSuccess)
    {
        std::cerr << "Error: Failed to open input .noa file." << std::endl;
        delete rawFile;
        return 1;
    }

    ERISAArchive* noaArchive = new ERISAArchive();
    if (noaArchive->Open(rawFile) != eslErrSuccess)
    {
        std::cerr << "Error: Failed to open file as a NOA archive." << std::endl;
        delete noaArchive;
        delete rawFile;
        return 1;
    }

    // ====================================================================
    // Phase 1: Read all files from the archive into memory.
    // This decouples library interaction from disk I/O to prevent bugs.(I know it seems stupid, but...you know what I want to say.)
    // ====================================================================
    std::vector<ExtractedFile> filesInMemory;
    std::cout << "Phase 1: Scanning archive and reading files into memory..." << std::endl;
    try
    {
        readArchiveToMemory(noaArchive, L"", password, filesInMemory);
    }
    catch (const std::exception& e)
    {
        std::cerr << "\nCritical error while reading the archive: " << e.what() << std::endl;
    }

    // Reading is complete. Close the archive to end all interaction with the library.
    noaArchive->Close();
    delete noaArchive;
    delete rawFile;
    std::cout << "Scan complete. " << filesInMemory.size() << " files read into memory." << std::endl << std::endl;

    // ====================================================================
    // Phase 2: Write the files from memory to the disk.
    // ====================================================================
    std::cout << "Phase 2: Writing files from memory to disk..." << std::endl;
    for (const auto& file : filesInMemory)
    {
        // Construct the final disk path.
        std::filesystem::path finalDiskPath = outputFolderPath / file.diskPath;

        // Convert the Unicode path to the console's encoding for correct display.
        std::string consolePath = wide2Ascii(finalDiskPath.wstring(), 65001);
        std::cout << "Writing: " << consolePath << " (" << file.data.size() << " bytes)" << std::endl;

        try
        {
            if (finalDiskPath.has_parent_path())
            {
                std::filesystem::create_directories(finalDiskPath.parent_path());
            }

            std::ofstream outfile(finalDiskPath, std::ios::binary);
            if (outfile)
            {
                outfile.write(reinterpret_cast<const char*>(file.data.data()), file.data.size());
            }
            else
            {
                std::cerr << "  -> Error: Could not create output file." << std::endl;
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::cerr << "  -> Filesystem error: " << e.what() << std::endl;
        }
    }

    std::cout << "\nExtraction complete." << std::endl;
    return 0;
}

/**
 * @brief Recursively reads all files from a directory within the archive into a memory vector.
 * @param archive The active ERISAArchive object.
 * @param currentPathInArchive The relative path of the current directory being processed.
 * @param password The password to use for encrypted files.
 * @param extractedFiles The vector to store the results in.
 */
void readArchiveToMemory(
    ERISAArchive* archive,
    const std::filesystem::path& currentPathInArchive,
    const char* password,
    std::vector<ExtractedFile>& extractedFiles)
{
    ERISAArchive::EDirectory dir;
    archive->GetFileEntries(dir);

    for (unsigned int i = 0; i < dir.GetSize(); ++i)
    {
        ERISAArchive::FILE_INFO& fileInfo = dir[i];

        // --- Convert filename from UTF-8 to a Unicode path object ---
        std::string u8FileName(fileInfo.ptrFileName);
        std::wstring wideFileName = ascii2Wide(u8FileName, 65001);
        std::filesystem::path newPath = currentPathInArchive / wideFileName;
        // ---

        if (fileInfo.dwAttribute & ERISAArchive::attrDirectory)
        {
            archive->DescendDirectory(fileInfo.ptrFileName);
            readArchiveToMemory(archive, newPath, password, extractedFiles);
            archive->AscendDirectory();
        }
        else
        {
            const char* passToUse = (fileInfo.dwEncodeType & ERISAArchive::etBSHFCrypt) ? password : nullptr;

            if (archive->OpenFile(fileInfo.ptrFileName, passToUse) != eslErrSuccess)
            {
                std::cerr << "  -> Warning: Failed to open file in archive: " << u8FileName << std::endl;
                continue;
            }

            UINT64 size = archive->GetLargeLength();
            ExtractedFile memFile;
            memFile.diskPath = newPath;

            if (size > 0)
            {
                memFile.data.resize(static_cast<size_t>(size));
                archive->Read(memFile.data.data(), (unsigned long)size);
            }

            extractedFiles.push_back(std::move(memFile));
            archive->AscendFile();
        }
    }
}
