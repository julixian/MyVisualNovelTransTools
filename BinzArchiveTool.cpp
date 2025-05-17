// Game: [010316][Mink]椿色のプリジオーネ
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <Windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
	LZSS_OK,
	LZSS_NOMEM,
	LZSS_NODATA,
	LZSS_INVARG
} lzss_error_t;

std::string WideToAscii(const std::wstring& wide, UINT CodePage) {
    int len = WideCharToMultiByte(CodePage, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return "";
    std::string ascii(len, '\0');
    WideCharToMultiByte(CodePage, 0, wide.c_str(), -1, &ascii[0], len, nullptr, nullptr);
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

size_t lzss_decompress(uint8_t* dst, unsigned int dstlen, uint8_t* src, unsigned int srclen);
size_t lzss_compress(uint8_t* dst, unsigned int dstlen, uint8_t* src, unsigned int srclen);

struct FileEntry {
    char filename[32];
    uint32_t offset;
    uint32_t decompressedSize;
    uint32_t compressedSize;
};

bool extractPackage(const std::string& packagePath, const std::string& outputDir) {
    
    std::ifstream packageFile(packagePath, std::ios::binary);
    if (!packageFile) {
        std::cerr << "Error: Could not open package file: " << packagePath << std::endl;
        return false;
    }

    char header[4];
    packageFile.read(header, 4);
    if (std::strncmp(header, "BINZ", 4) != 0) {
        std::cerr << "Error: Invalid package format. Expected 'BINZ' header." << std::endl;
        return false;
    }

    uint32_t fileCount;
    packageFile.read(reinterpret_cast<char*>(&fileCount), 4);

    std::vector<FileEntry> entries(fileCount);
    for (uint32_t i = 0; i < fileCount; ++i) {
        packageFile.read(reinterpret_cast<char*>(&entries[i]), sizeof(FileEntry));
    }

    if (!std::filesystem::exists(outputDir)) {
        std::filesystem::create_directories(outputDir);
    }

    for (const auto& entry : entries) {
        std::cout << "Extracting: " << AsciiToAscii(entry.filename, 932, CP_ACP) << std::endl;
        std::cout << "  Compressed size: " << entry.compressedSize << " bytes" << std::endl;
        std::cout << "  Decompressed size: " << entry.decompressedSize << " bytes" << std::endl;

        std::vector<uint8_t> compressedData(entry.compressedSize);
        packageFile.seekg(entry.offset, std::ios::beg);
        packageFile.read(reinterpret_cast<char*>(compressedData.data()), entry.compressedSize);

        std::vector<uint8_t> decompressedData(entry.decompressedSize);
        size_t decompressedBytes;

        if (entry.compressedSize == entry.decompressedSize) {
            decompressedData = std::move(compressedData);
            decompressedBytes = entry.decompressedSize;
        }
        else {
            decompressedBytes = lzss_decompress(
                decompressedData.data(), entry.decompressedSize,
                compressedData.data(), entry.compressedSize
            );
        }

        if (decompressedBytes != entry.decompressedSize) {
            std::cerr << "Warning: Decompressed size mismatch for " << AsciiToAscii(entry.filename, 932, CP_ACP)
                << ". Expected: " << entry.decompressedSize
                << ", Got: " << decompressedBytes << std::endl;
        }

        std::string fullPath = outputDir + "/" + AsciiToAscii(entry.filename, 932, CP_ACP);
        std::filesystem::path filePath(fullPath);
        std::filesystem::create_directories(filePath.parent_path());

        std::ofstream outFile(fullPath, std::ios::binary);
        if (!outFile) {
            std::cerr << "Error: Could not create output file: " << fullPath << std::endl;
            continue;
        }
        outFile.write(reinterpret_cast<char*>(decompressedData.data()), decompressedBytes);
        outFile.close();

        std::cout << "  Successfully extracted to: " << fullPath << std::endl;
    }

    packageFile.close();
    return true;
}

bool createPackage(const std::string& inputDir, const std::string& packagePath) {
    
    if (!std::filesystem::exists(inputDir) || !std::filesystem::is_directory(inputDir)) {
        std::cerr << "Error: Input directory does not exist: " << inputDir << std::endl;
        return false;
    }

    std::vector<std::filesystem::path> files;
    std::filesystem::path inputPath(inputDir);

    for (const auto& entry : std::filesystem::recursive_directory_iterator(inputDir)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }

    if (files.empty()) {
        std::cerr << "Error: No files found in the input directory." << std::endl;
        return false;
    }

    std::cout << "Found " << files.size() << " files to package." << std::endl;

    std::ofstream packageFile(packagePath, std::ios::binary);
    if (!packageFile) {
        std::cerr << "Error: Could not create package file: " << packagePath << std::endl;
        return false;
    }

    packageFile.write("BINZ", 4);

    uint32_t fileCount = static_cast<uint32_t>(files.size());
    packageFile.write(reinterpret_cast<char*>(&fileCount), 4);

    uint32_t currentOffset = 8 + fileCount * sizeof(FileEntry);

    std::vector<FileEntry> entries(fileCount);
    std::vector<std::vector<uint8_t>> compressedData(fileCount);

    for (size_t i = 0; i < files.size(); ++i) {
        FileEntry& entry = entries[i];
        memset(entry.filename, 0, sizeof(entry.filename));

        std::string relativePath = WideToAscii(std::filesystem::relative(files[i], inputDir).wstring(), 932);
        
        if (relativePath.length() >= sizeof(entry.filename)) {
            std::cerr << "Warning: Filename too long, will be truncated: " << AsciiToAscii(relativePath, 932, CP_ACP) << std::endl;
        }
        
        strcpy_s(entry.filename, relativePath.c_str());

        std::ifstream inputFile(files[i], std::ios::binary | std::ios::ate);
        if (!inputFile) {
            std::cerr << "Error: Could not open input file: " << files[i] << std::endl;
            continue;
        }

        std::streamsize fileSize = inputFile.tellg();
        inputFile.seekg(0, std::ios::beg);

        std::vector<uint8_t> fileData(fileSize);
        inputFile.read(reinterpret_cast<char*>(fileData.data()), fileSize);
        inputFile.close();

        entry.decompressedSize = static_cast<uint32_t>(fileSize);

        std::vector<uint8_t> compressed((fileSize + 7) / 8 * 9);

        size_t compressedSize = lzss_compress(
            compressed.data(), compressed.size(),
            fileData.data(), fileData.size()
        );

        if (compressedSize >= fileSize) {
            compressedSize = fileSize;
            compressed = std::move(fileData);
        }

        compressed.resize(compressedSize);
        compressedData[i] = std::move(compressed);
        entry.compressedSize = static_cast<uint32_t>(compressedSize);

        entry.offset = currentOffset;
        currentOffset += entry.compressedSize;

        std::cout << "Adding file: " << AsciiToAscii(entry.filename, 932, CP_ACP) << std::endl;
        std::cout << "  Original size: " << fileSize << " bytes" << std::endl;
        std::cout << "  Compressed size: " << compressedSize << " bytes" << std::endl;
    }

    for (const auto& entry : entries) {
        packageFile.write(reinterpret_cast<const char*>(&entry), sizeof(FileEntry));
    }

    for (size_t i = 0; i < files.size(); ++i) {
        packageFile.write(reinterpret_cast<const char*>(compressedData[i].data()), compressedData[i].size());
    }

    packageFile.close();
    std::cout << "Package created successfully: " << packagePath << std::endl;
    return true;
}

void printUsage(const char* programName) {
    std::cout << "Made by julixian 2025.05.18" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "For extract: " << programName << " -e <package_file> <output_directory>" << std::endl;
    std::cout << "For pack:  " << programName << " -p <input_directory> <package_file>" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    std::string path1 = argv[2];
    std::string path2 = argv[3];

    if (mode == "-e") {
        std::cout << "Extracting package: " << path1 << std::endl;
        std::cout << "Output directory: " << path2 << std::endl;

        if (extractPackage(path1, path2)) {
            std::cout << "Extraction completed successfully!" << std::endl;
            return 0;
        }
        else {
            std::cerr << "Extraction failed!" << std::endl;
            return 1;
        }
    }
    else if (mode == "-p") {
        std::cout << "Creating package from directory: " << path1 << std::endl;
        std::cout << "Output package: " << path2 << std::endl;

        if (createPackage(path1, path2)) {
            std::cout << "Package created successfully!" << std::endl;
            return 0;
        }
        else {
            std::cerr << "Package creation failed!" << std::endl;
            return 1;
        }
    }
    else {
        std::cerr << "Error: Unknown mode. Use -e for extract or -p for create." << std::endl;
        printUsage(argv[0]);
        return 1;
    }
}


// -------------------------------------------------------------
// https://github.com/satan53x/SExtractor/tree/main/libs/lzss
// -------------------------------------------------------------

lzss_error_t lzss_errno = LZSS_OK;

const char* lzss_strerror(lzss_error_t error) {
	switch (error) {
	case LZSS_NOMEM:
		return "Memory allocation failure";

	case LZSS_NODATA:
		return "Provided data is too short";

	case LZSS_INVARG:
		return "Invalid argument";

	case LZSS_OK:
		return "No error";

	default:
		return NULL;
	}
}

/**************************************************************
	LZSS.C -- A Data Compression Program
	(tab = 4 spaces)
 ***************************************************************
	4/6/1989 Haruhiko Okumura
	Use, distribute, and modify this program freely.
	Please send me your improved versions.
 PC-VAN		SCIENCE
 NIFTY-Serve	PAF01022
 CompuServe	74050,1022
 **************************************************************/
#define N         4096  /* size of ring buffer - must be power of 2 */
#define F         18    /* upper limit for match_length */
#define THRESHOLD 2     /* encode string into position and length if match_length is greater than this */
#define NIL       N     /* index for root of binary search trees */

const char Padding = '\0';

size_t lzss_decompress(uint8_t* dst, unsigned int dstlen, uint8_t* src, unsigned int srclen)
{
	if (dst && src && dstlen && srclen) {
		/* ring buffer of size N, with extra F-1 bytes to aid string comparison */
		uint8_t text_buf[N + F - 1];
		uint8_t* dststart = dst;
		uint8_t* srcend = src + srclen;
		uint8_t* dstend = dst + dstlen;
		int  i, j, k, r, c;
		unsigned int flags;

		srcend = src + srclen;
		dst = dststart;
		for (i = 0; i < N - F; i++)
			text_buf[i] = Padding;
		r = N - F;
		flags = 0;

		while (1) {
			if (((flags >>= 1) & 0x100) == 0) {
				if (src < srcend) c = *src++; else break;
				flags = c | 0xFF00;  /* uses higher byte cleverly */
			}   /* to count eight */
			if (flags & 1) {
				if (src < srcend) c = *src++; else break;
				if (dst < dstend)
					*dst++ = c;
				else {
					lzss_errno = LZSS_NOMEM;
					return -1;
				}
				text_buf[r++] = c;
				r &= (N - 1);
			}
			else {
				if (src < srcend) i = *src++; else break;
				if (src < srcend) j = *src++; else break;
				i |= ((j & 0xF0) << 4);
				j = (j & 0x0F) + THRESHOLD;
				for (k = 0; k <= j; k++) {
					c = text_buf[(i + k) & (N - 1)];
					if (dst <= dstend)
						*dst++ = c;
					else {
						lzss_errno = LZSS_NOMEM;
						return -1;
					}
					text_buf[r++] = c;
					r &= (N - 1);
				}
			}
		}

		lzss_errno = LZSS_OK;
		return (size_t)dst - (size_t)dststart;
	}
	else {
		lzss_errno = LZSS_INVARG;
		return -1;
	}
}

struct encode_state {
	/* left & right children & parent. These constitute binary search trees. */
	int lchild[N + 1], rchild[N + 257], parent[N + 1];

	/* ring buffer of size N, with extra F-1 bytes to aid string comparison */
	uint8_t text_buf[N + F - 1];

	/* match_length of longest match.
	 * These are set by the insert_node() procedure.
	 */
	int match_position, match_length;
};

/* initialize state, mostly the trees
 *
 * For i = 0 to N - 1, rchild[i] and lchild[i] will be the right and left
 * children of node i.  These nodes need not be initialized.  Also, parent[i]
 * is the parent of node i.  These are initialized to NIL (= N), which stands
 * for 'not used.'  For i = 0 to 255, rchild[N + i + 1] is the root of the
 * tree for strings that begin with character i.  These are initialized to NIL.
 * Note there are 256 trees. */
static void init_state(struct encode_state* sp) {
	int  i;

	memset(sp, 0, sizeof(*sp));

	for (i = 0; i < N - F; i++)
		sp->text_buf[i] = Padding;
	for (i = N + 1; i <= N + 256; i++)
		sp->rchild[i] = NIL;
	for (i = 0; i < N; i++)
		sp->parent[i] = NIL;
}

/* Inserts string of length F, text_buf[r..r+F-1], into one of the trees
 * (text_buf[r]'th tree) and returns the longest-match position and length
 * via the global variables match_position and match_length.
 * If match_length = F, then removes the old node in favor of the new one,
 * because the old one will be deleted sooner. Note r plays double role,
 * as tree node and position in buffer.
 */
static void insert_node(struct encode_state* sp, int r) {
	int  i, p, cmp;
	uint8_t* key;

	cmp = 1;
	key = &sp->text_buf[r];
	p = N + 1 + key[0];
	sp->rchild[r] = sp->lchild[r] = NIL;
	sp->match_length = 0;
	for (; ; ) {
		if (cmp >= 0) {
			if (sp->rchild[p] != NIL)
				p = sp->rchild[p];
			else {
				sp->rchild[p] = r;
				sp->parent[r] = p;
				return;
			}
		}
		else {
			if (sp->lchild[p] != NIL)
				p = sp->lchild[p];
			else {
				sp->lchild[p] = r;
				sp->parent[r] = p;
				return;
			}
		}
		for (i = 1; i < F; i++) {
			if ((cmp = key[i] - sp->text_buf[p + i]) != 0)
				break;
		}
		if (i > sp->match_length) {
			sp->match_position = p;
			if ((sp->match_length = i) >= F)
				break;
		}
	}
	sp->parent[r] = sp->parent[p];
	sp->lchild[r] = sp->lchild[p];
	sp->rchild[r] = sp->rchild[p];
	sp->parent[sp->lchild[p]] = r;
	sp->parent[sp->rchild[p]] = r;
	if (sp->rchild[sp->parent[p]] == p)
		sp->rchild[sp->parent[p]] = r;
	else
		sp->lchild[sp->parent[p]] = r;
	sp->parent[p] = NIL;  /* remove p */
}

/* deletes node p from tree */
static void delete_node(struct encode_state* sp, int p) {
	int  q;

	if (sp->parent[p] == NIL)
		return;  /* not in tree */
	if (sp->rchild[p] == NIL)
		q = sp->lchild[p];
	else if (sp->lchild[p] == NIL)
		q = sp->rchild[p];
	else {
		q = sp->lchild[p];
		if (sp->rchild[q] != NIL) {
			do {
				q = sp->rchild[q];
			} while (sp->rchild[q] != NIL);
			sp->rchild[sp->parent[q]] = sp->lchild[q];
			sp->parent[sp->lchild[q]] = sp->parent[q];
			sp->lchild[q] = sp->lchild[p];
			sp->parent[sp->lchild[p]] = q;
		}
		sp->rchild[q] = sp->rchild[p];
		sp->parent[sp->rchild[p]] = q;
	}
	sp->parent[q] = sp->parent[p];
	if (sp->rchild[sp->parent[p]] == p)
		sp->rchild[sp->parent[p]] = q;
	else
		sp->lchild[sp->parent[p]] = q;
	sp->parent[p] = NIL;
}

size_t lzss_compress(uint8_t* dst, unsigned int dstlen, uint8_t* src, unsigned int srclen) {
	if (dst && src && dstlen && srclen) {
		/* Encoding state, mostly tree but some current match stuff */
		struct encode_state* sp;

		int  i, c, len, r, s, last_match_length, code_buf_ptr;
		uint8_t code_buf[17], mask;
		uint8_t* srcend = src + srclen;
		uint8_t* dstend = dst + dstlen;
		uint8_t* dststart = dst;

		/* initialize trees */
		sp = (struct encode_state*)malloc(sizeof(*sp));
		init_state(sp);

		/* code_buf[1..16] saves eight units of code, and code_buf[0] works
		 * as eight flags, "1" representing that the unit is an unencoded
		 * letter (1 byte), "" a position-and-length pair (2 bytes).
		 * Thus, eight units require at most 16 bytes of code.
		 */
		code_buf[0] = 0;
		code_buf_ptr = mask = 1;

		/* Clear the buffer with any character that will appear often. */
		s = 0;  r = N - F;

		/* Read F bytes into the last F bytes of the buffer */
		for (len = 0; len < F && src < srcend; len++)
			sp->text_buf[r + len] = *src++;
		if (!len) {
			free(sp);
			lzss_errno = LZSS_NODATA;
			return -1;
		}
		/*
		 * Insert the F strings, each of which begins with one or more
		 * 'space' characters.  Note the order in which these strings are
		 * inserted.  This way, degenerate trees will be less likely to occur.
		 */
		for (i = 1; i <= F; i++)
			insert_node(sp, r - i);

		/* Finally, insert the whole string just read.
		 * The global variables match_length and match_position are set.
		 */
		insert_node(sp, r);
		do {
			/* match_length may be spuriously long near the end of text. */
			if (sp->match_length > len)
				sp->match_length = len;
			if (sp->match_length <= THRESHOLD) {
				sp->match_length = 1;  /* Not long enough match.  Send one byte. */
				code_buf[0] |= mask;  /* 'send one byte' flag */
				code_buf[code_buf_ptr++] = sp->text_buf[r];  /* Send uncoded. */
			}
			else {
				/* Send position and length pair. Note match_length > THRESHOLD. */
				code_buf[code_buf_ptr++] = (uint8_t)sp->match_position;
				code_buf[code_buf_ptr++] = (uint8_t)
					(((sp->match_position >> 4) & 0xF0)
						| (sp->match_length - (THRESHOLD + 1)));
			}
			if ((mask <<= 1) == 0) {  /* Shift mask left one bit. */
				/* Send at most 8 units of code together */
				for (i = 0; i < code_buf_ptr; i++)
					if (dst < dstend)
						*dst++ = code_buf[i];
					else {
						free(sp);
						lzss_errno = LZSS_NOMEM;
						return -1;
					}
				code_buf[0] = 0;
				code_buf_ptr = mask = 1;
			}
			last_match_length = sp->match_length;
			for (i = 0; i < last_match_length && src < srcend; i++) {
				delete_node(sp, s);    /* Delete old strings and */
				c = *src++;
				sp->text_buf[s] = c;    /* read new bytes */

				/* If the position is near the end of buffer, extend the buffer
				 * to make string comparison easier.
				 */
				if (s < F - 1)
					sp->text_buf[s + N] = c;

				/* Since this is a ring buffer, increment the position modulo N. */
				s = (s + 1) & (N - 1);
				r = (r + 1) & (N - 1);

				/* Register the string in text_buf[r..r+F-1] */
				insert_node(sp, r);
			}
			while (i++ < last_match_length) {
				delete_node(sp, s);

				/* After the end of text, no need to read, */
				s = (s + 1) & (N - 1);
				r = (r + 1) & (N - 1);
				/* but buffer may not be empty. */
				if (--len)
					insert_node(sp, r);
			}
		} while (len > 0);   /* until length of string to be processed is zero */

		if (code_buf_ptr > 1) {    /* Send remaining code. */
			for (i = 0; i < code_buf_ptr; i++)
				if (dst < dstend)
					*dst++ = code_buf[i];
				else {
					free(sp);
					lzss_errno = LZSS_NOMEM;
					return -1;
				}
		}

		free(sp);
		lzss_errno = LZSS_OK;
		return (size_t)dst - (size_t)dststart;
	}
	else {
		lzss_errno = LZSS_INVARG;
		return -1;
	}
}