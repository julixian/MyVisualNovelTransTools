#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <windows.h>

// Main header for the ERISA library.
#include "xerisa.h"

std::string WideToAscii(const std::wstring& wide, UINT CodePage) {
    int len = WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return "";
    std::string ascii(len, '\0');
    WideCharToMultiByte
    (CodePage, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
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

std::string AsciiToAscii(const std::string& ascii, UINT src, UINT dst) {
    return WideToAscii(AsciiToWide(ascii, src), dst);
}

// Provide stub implementations for missing virtual functions for the linker.
const char* EPtrArray::GetClassNameW(void) const { return "EPtrArray"; }
const char* ESLObject::GetClassNameW(void) const { return "ESLObject"; }

// Struct to hold an extracted file's path and data in memory.
struct ExtractedFile {
    std::filesystem::path disk_path; // Stored as a Unicode-aware path object.
    std::vector<BYTE> data;
};

// Forward declaration of the recursive function.
void ReadArchiveToMemory(
    ERISAArchive* archive,
    const std::filesystem::path& current_path_in_archive,
    const char* password,
    std::vector<ExtractedFile>& extracted_files
);

int main(int argc, char* argv[])
{
    system("chcp 65001");
    if (argc < 3 || argc > 4)
    {
        std::cout << "Made by julixian 2025.07.07" << std::endl;
        std::cerr << "Usage: " << argv[0] << " <input.noa> <output_folder> [password]" << std::endl;
        std::cerr << "Example: " << argv[0] << " data.noa output" << std::endl;
        std::cerr << "Example: " << argv[0] << " script.noa output MARSELUD" << std::endl;
        std::cerr << "Example: " << argv[0] << " script.noa output convini_cat" << std::endl;
        return 1;
    }
    
    std::filesystem::path input_noa_path(argv[1]);
    std::filesystem::path output_folder_path(argv[2]);

    // Check if a password was provided.
    const char* password = (argc == 4) ? argv[3] : nullptr;

    std::cout << "ERISA NOA Archive Extractor" << std::endl;
    std::cout << "--------------------------------------------" << std::endl;
    std::cout << "Input File: " << WideToAscii(input_noa_path.wstring(), 65001) << std::endl;
    std::cout << "Output Directory: " << WideToAscii(output_folder_path.wstring(), 65001) << std::endl;
    if (password) {
        std::cout << "Using Password: " << password << std::endl;
    }
    else {
        std::cout << "No Password Provided" << std::endl;
    }
    std::cout << std::endl;

    // --- Open the archive ---
    ERawFile* rawFile = new ERawFile();
    if (rawFile->Open(input_noa_path.string().c_str(), ESLFileObject::modeRead) != eslErrSuccess)
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
    std::vector<ExtractedFile> files_in_memory;
    std::cout << "Phase 1: Scanning archive and reading files into memory..." << std::endl;
    try
    {
        ReadArchiveToMemory(noaArchive, "", password, files_in_memory);
    }
    catch (const std::exception& e)
    {
        std::cerr << "\nCritical error while reading the archive: " << e.what() << std::endl;
    }

    // Reading is complete. Close the archive to end all interaction with the library.
    noaArchive->Close();
    delete noaArchive;
    delete rawFile;
    std::cout << "Scan complete. " << files_in_memory.size() << " files read into memory." << std::endl << std::endl;

    // ====================================================================
    // Phase 2: Write the files from memory to the disk.
    // ====================================================================
    std::cout << "Phase 2: Writing files from memory to disk..." << std::endl;
    for (const auto& file : files_in_memory)
    {
        // Construct the final disk path.
        std::filesystem::path final_disk_path = output_folder_path / file.disk_path;

        // Convert the Unicode path to the console's encoding for correct display.
        std::string console_path = WideToAscii(final_disk_path.wstring(), 65001);
        std::cout << "Writing: " << console_path << " (" << file.data.size() << " bytes)" << std::endl;

        try
        {
            if (final_disk_path.has_parent_path())
            {
                std::filesystem::create_directories(final_disk_path.parent_path());
            }

            std::ofstream outfile(final_disk_path, std::ios::binary);
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
 * @param current_path_in_archive The relative path of the current directory being processed.
 * @param password The password to use for encrypted files.
 * @param extracted_files The vector to store the results in.
 */
void ReadArchiveToMemory(
    ERISAArchive* archive,
    const std::filesystem::path& current_path_in_archive,
    const char* password,
    std::vector<ExtractedFile>& extracted_files)
{
    ERISAArchive::EDirectory dir;
    archive->GetFileEntries(dir);

    for (unsigned int i = 0; i < dir.GetSize(); ++i)
    {
        ERISAArchive::FILE_INFO& fileInfo = dir[i];

        // --- Convert filename from CP932 to a Unicode path object ---
        std::string cp932FileName(fileInfo.ptrFileName);
        std::wstring wideFileName = AsciiToWide(cp932FileName, 932);
        std::filesystem::path new_path = current_path_in_archive / wideFileName;
        // ---

        if (fileInfo.dwAttribute & ERISAArchive::attrDirectory)
        {
            archive->DescendDirectory(fileInfo.ptrFileName);
            ReadArchiveToMemory(archive, new_path, password, extracted_files);
            archive->AscendDirectory();
        }
        else
        {
            const char* pass_to_use = (fileInfo.dwEncodeType & ERISAArchive::etBSHFCrypt) ? password : nullptr;

            if (archive->OpenFile(fileInfo.ptrFileName, pass_to_use) != eslErrSuccess)
            {
                std::string console_path = WideToAscii(new_path.wstring(), 65001);
                std::cerr << "  -> Warning: Failed to open file in archive: " << console_path << std::endl;
                continue;
            }

            UINT64 size = archive->GetLargeLength();
            ExtractedFile mem_file;
            mem_file.disk_path = new_path;

            if (size > 0)
            {
                mem_file.data.resize(static_cast<size_t>(size));
                archive->Read(mem_file.data.data(), (unsigned long)size);
            }

            extracted_files.push_back(std::move(mem_file));
            archive->AscendFile();
        }
    }
}
