#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>
#include <png.h>

size_t lzss_compress(uint8_t* dst, unsigned int dstlen, uint8_t* src, unsigned int srclen);

#pragma pack(push, 1)
struct RmtHeader {
    uint32_t signature; // 0x20544D52 ('RMT ')
    int32_t  offsetX;
    int32_t  offsetY;
    uint32_t width;
    uint32_t height;
};
#pragma pack(pop)

const uint32_t RMT_SIGNATURE = 0x20544D52;
const size_t RMT_HEADER_FIXED_SIZE = sizeof(RmtHeader);

bool read_png_file(const char* filename, uint32_t& width, uint32_t& height, std::vector<uint8_t>& image_data_rgba) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        std::cerr << "错误：无法打开文件 " << filename << " 进行读取。" << std::endl;
        return false;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        std::cerr << "错误：png_create_read_struct 失败。" << std::endl;
        fclose(fp);
        return false;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        std::cerr << "错误：png_create_info_struct 失败。" << std::endl;
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        fclose(fp);
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        std::cerr << "错误：PNG 读取过程中发生错误 (setjmp)。" << std::endl;
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        fclose(fp);
        return false;
    }

    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);

    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);
    png_byte color_type = png_get_color_type(png_ptr, info_ptr);
    png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    if (bit_depth == 16)
        png_set_strip_16(png_ptr);

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);

    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    if (png_get_rowbytes(png_ptr, info_ptr) != width * 4) {
        std::cerr << "错误：PNG 转换后不是 32bpp，或行字节数不符。" << std::endl;
        std::cerr << "期望行字节数: " << width * 4 << ", 得到: " << png_get_rowbytes(png_ptr, info_ptr) << std::endl;
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        fclose(fp);
        return false;
    }

    image_data_rgba.resize(height * png_get_rowbytes(png_ptr, info_ptr));
    std::vector<png_bytep> row_pointers(height);
    for (uint32_t y = 0; y < height; ++y) {
        row_pointers[y] = image_data_rgba.data() + y * png_get_rowbytes(png_ptr, info_ptr);
    }

    png_read_image(png_ptr, row_pointers.data());

    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    fclose(fp);

    std::cout << "Successfully Read PNG: " << filename << " (Width: " << width << ", Height: " << height << ")" << std::endl;
    return true;
}


int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Made by julixian 2025.05.29" << std::endl;
        std::cerr << "Usage: " << argv[0] << " <input.png> <output.rmt>" << std::endl;
        return 1;
    }

    const char* png_filename = argv[1];
    const char* rmt_filename = argv[2];

    uint32_t width, height;
    std::vector<uint8_t> image_data_rgba;

    if (!read_png_file(png_filename, width, height, image_data_rgba)) {
        return 1;
    }

    if (height > 1) {
        int row_bytes = width * 4;
        std::vector<uint8_t> temp_row(row_bytes);
        for (uint32_t y = 0; y < height / 2; ++y) {
            uint8_t* ptr_top_row = image_data_rgba.data() + y * row_bytes;
            uint8_t* ptr_bottom_row = image_data_rgba.data() + (height - 1 - y) * row_bytes;

            memcpy(temp_row.data(), ptr_top_row, row_bytes);
            memcpy(ptr_top_row, ptr_bottom_row, row_bytes);
            memcpy(ptr_bottom_row, temp_row.data(), row_bytes);
        }
    }

    std::vector<uint8_t> image_data_bgra = image_data_rgba;
    for (size_t i = 0; i < image_data_bgra.size(); i += 4) {
        std::swap(image_data_bgra[i], image_data_bgra[i + 2]);
    }

    int stride = static_cast<int>(width * 4);
    std::vector<uint8_t> delta_encoded_pixels = image_data_bgra;

    for (int i = static_cast<int>(delta_encoded_pixels.size()) - 4; i >= stride; i -= 4) {
        delta_encoded_pixels[i] -= delta_encoded_pixels[i - stride];
        delta_encoded_pixels[i + 1] -= delta_encoded_pixels[i - stride + 1];
        delta_encoded_pixels[i + 2] -= delta_encoded_pixels[i - stride + 2];
        delta_encoded_pixels[i + 3] -= delta_encoded_pixels[i - stride + 3];
    }

    if (height > 0) {
        for (int i = stride - 4; i >= 4; i -= 4) {
            delta_encoded_pixels[i] -= delta_encoded_pixels[i - 4];
            delta_encoded_pixels[i + 1] -= delta_encoded_pixels[i - 3];
            delta_encoded_pixels[i + 2] -= delta_encoded_pixels[i - 2];
            delta_encoded_pixels[i + 3] -= delta_encoded_pixels[i - 1];
        }
    }


    unsigned int raw_pixel_data_size = static_cast<unsigned int>(delta_encoded_pixels.size());
    unsigned int compressed_buffer_capacity = raw_pixel_data_size;
    if (raw_pixel_data_size > 0) {
        compressed_buffer_capacity = raw_pixel_data_size + (raw_pixel_data_size / 8) + 256;
    }
    else {
        compressed_buffer_capacity = 256;
    }

    std::vector<uint8_t> compressed_data(compressed_buffer_capacity);
    size_t actual_compressed_size = 0;

    if (raw_pixel_data_size > 0) {
        actual_compressed_size = lzss_compress(
            compressed_data.data(),
            static_cast<unsigned int>(compressed_data.size()),
            delta_encoded_pixels.data(),
            raw_pixel_data_size
        );
    }

    if (raw_pixel_data_size > 0 && actual_compressed_size == 0) {
        std::cerr << "错误：LZSS 压缩失败 (返回 0)。" << std::endl;
        std::cerr << "  源数据大小: " << raw_pixel_data_size << ", 缓冲区容量: " << compressed_data.size() << std::endl;
        return 1;
    }
    if (actual_compressed_size > compressed_data.size()) {
        std::cerr << "错误：LZSS 压缩报告的大小超出了缓冲区容量。这表明 lzss_compress 或缓冲区大小设置存在错误。" << std::endl;
        return 1;
    }

    if (raw_pixel_data_size > 0) {
        compressed_data.resize(actual_compressed_size);
    }
    else {
        compressed_data.clear();
    }

    std::cout << "LZSS Compress Succeed. Original Size: " << raw_pixel_data_size
        << ", Compressed Size: " << actual_compressed_size << " Bytes." << std::endl;

    std::ofstream rmt_file(rmt_filename, std::ios::binary | std::ios::trunc);
    if (!rmt_file) {
        std::cerr << "错误：无法打开 RMT 文件进行写入: " << rmt_filename << std::endl;
        return 1;
    }

    RmtHeader header;
    header.signature = RMT_SIGNATURE;
    header.offsetX = 0;
    header.offsetY = 0;
    header.width = width;
    header.height = height;

    rmt_file.write(reinterpret_cast<const char*>(&header), RMT_HEADER_FIXED_SIZE);
    if (!rmt_file) {
        std::cerr << "错误：写入 RMT 文件头失败。" << std::endl;
        return 1;
    }

    if (!compressed_data.empty()) {
        rmt_file.write(reinterpret_cast<const char*>(compressed_data.data()), compressed_data.size());
        if (!rmt_file) {
            std::cerr << "错误：写入压缩后的 RMT 数据失败。" << std::endl;
            return 1;
        }
    }

    rmt_file.close();
    std::cout << "Write RMT File Successfully: " << rmt_filename << std::endl;

    return 0;
}


/*-
 * Copyright 2015 Pupyshev Nikita
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __ibootim__lzss__
#define __ibootim__lzss__

#include <stdint.h>

typedef enum {
    LZSS_OK,
    LZSS_NOMEM,
    LZSS_NODATA,
    LZSS_INVARG
} lzss_error_t;

#endif /* defined(__ibootim__lzss__) */

// -------------------------------------------------------------
// https://github.com/satan53x/SExtractor/tree/main/libs/lzss
// -------------------------------------------------------------
#include <stdlib.h>
#include <string.h>

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

