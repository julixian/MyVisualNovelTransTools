#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <malloc.h>
#include <cstdint>
#include "tjsTypes.h"
#include "SaveTLG.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// TODO
class tTVPBaseBitmap
{
public:
	tTVPBaseBitmap(std::string file)
	{
		data = stbi_load(file.c_str(), &w, &h, &n, 4);
		if (!data)
			throw std::runtime_error("Unable to load image: " + file);
	}

	~tTVPBaseBitmap()
	{
		stbi_image_free(data);
	}

	bool Is32BPP() const {return true;}
	uint32_t GetWidth() const {return w;}
	uint32_t GetHeight() const {return h;}
	void *GetScanLine(uint32_t y) const {return (char *)data + 4*w*y;}

private:
	int w = 0, h = 0, n = 0;
	void *data = 0;
};

//---------------------------------------------------------------------------
SlideCompressor::SlideCompressor()
{
	S = 0;
	for(int i = 0; i < SLIDE_N + SLIDE_M - 1; i++) Text[i] = 0;
	for(int i = 0; i < 256*256; i++)
		Map[i] = -1;
	for(int i = 0; i < SLIDE_N; i++)
		Chains[i].Prev = Chains[i].Next = -1;
	for(int i = SLIDE_N - 1; i >= 0; i--)
		AddMap(i);
}
//---------------------------------------------------------------------------
SlideCompressor::~SlideCompressor()
{
}
//---------------------------------------------------------------------------
int SlideCompressor::GetMatch(const unsigned char*cur, int curlen, int &pos, int s)
{
	// get match length
	if(curlen < 3) return 0;

	int place = cur[0] + ((int)cur[1] << 8);

	int maxlen = 0;
	if((place = Map[place]) != -1)
	{
		int place_org;
		curlen -= 1;
		do
		{
			place_org = place;
			if(s == place || s == ((place + 1) & (SLIDE_N -1))) continue;
			place += 2;
			int lim = (SLIDE_M < curlen ? SLIDE_M : curlen) + place_org;
			const unsigned char *c = cur + 2;
			if(lim >= SLIDE_N)
			{
				if(place_org <= s && s < SLIDE_N)
					lim = s;
				else if(s < (lim&(SLIDE_N-1)))
					lim = s + SLIDE_N;
			}
			else
			{
				if(place_org <= s && s < lim)
					lim = s;
			}
			while(Text[place] == *(c++) && place < lim) place++;
			int matchlen = place - place_org;
			if(matchlen > maxlen) pos = place_org, maxlen = matchlen;
			if(matchlen == SLIDE_M) return maxlen;

		} while((place = Chains[place_org].Next) != -1);
	}
	return maxlen;
}
//---------------------------------------------------------------------------
void SlideCompressor::AddMap(int p)
{
	int place = Text[p] + ((int)Text[(p + 1) & (SLIDE_N - 1)] << 8);

	if(Map[place] == -1)
	{
		// first insertion
		Map[place] = p;
	}
	else
	{
		// not first insertion
		int old = Map[place];
		Map[place] = p;
		Chains[old].Prev = p;
		Chains[p].Next = old;
		Chains[p].Prev = -1;
	}
}
//---------------------------------------------------------------------------
void SlideCompressor::DeleteMap(int p)
{
	int n;
	if((n = Chains[p].Next) != -1)
		Chains[n].Prev = Chains[p].Prev;

	if((n = Chains[p].Prev) != -1)
	{
		Chains[n].Next = Chains[p].Next;
	}
	else if(Chains[p].Next != -1)
	{
		int place = Text[p] + ((int)Text[(p + 1) & (SLIDE_N - 1)] << 8);
		Map[place] = Chains[p].Next;
	}
	else
	{
		int place = Text[p] + ((int)Text[(p + 1) & (SLIDE_N - 1)] << 8);
		Map[place] = -1;
	}

	Chains[p].Prev = -1;
	Chains[p].Next = -1;
}
//---------------------------------------------------------------------------
void SlideCompressor::Encode(const unsigned char *in, long inlen,
		unsigned char *out, long & outlen)
{
	unsigned char code[40], codeptr, mask;

	if(inlen == 0) return;

	outlen = 0;
	code[0] = 0;
	codeptr = mask = 1;

	int s = S;
	while(inlen > 0)
	{
		int pos = 0;
		int len = GetMatch(in, inlen, pos, s);
		if(len >= 3)
		{
			code[0] |= mask;
			if(len >= 18)
			{
				code[codeptr++] = pos & 0xff;
				code[codeptr++] = ((pos &0xf00)>> 8) | 0xf0;
				code[codeptr++] = len - 18;
			}
			else
			{
				code[codeptr++] = pos & 0xff;
				code[codeptr++] = ((pos&0xf00)>> 8) | ((len-3)<<4);
			}
			while(len--)
			{
				unsigned char c = 0[in++];
				DeleteMap((s - 1) & (SLIDE_N - 1));
				DeleteMap(s);
				if(s < SLIDE_M - 1) Text[s + SLIDE_N] = c;
				Text[s] = c;
				AddMap((s - 1) & (SLIDE_N - 1));
				AddMap(s);
				s++;
				inlen--;
				s &= (SLIDE_N - 1);
			}
		}
		else
		{
			unsigned char c = 0[in++];
			DeleteMap((s - 1) & (SLIDE_N - 1));
			DeleteMap(s);
			if(s < SLIDE_M - 1) Text[s + SLIDE_N] = c;
			Text[s] = c;
	 		AddMap((s - 1) & (SLIDE_N - 1));
			AddMap(s);
			s++;
			inlen--;
			s &= (SLIDE_N - 1);
			code[codeptr++] = c;
		}
		mask <<= 1;

		if(mask == 0)
		{
			for(int i = 0; i < codeptr; i++)
				out[outlen++] = code[i];
			mask = codeptr = 1;
			code[0] = 0;
		}
	}

	if(mask != 1)
	{
		for(int i = 0; i < codeptr; i++)
			out[outlen++] = code[i];
	}

	S = s;
}
//---------------------------------------------------------------------------
void SlideCompressor::Store()
{
	S2 = S;
	int i;
	for(i = 0; i < SLIDE_N + SLIDE_M - 1; i++)
		Text2[i] = Text[i];

	for(i = 0; i < 256*256; i++)
		Map2[i] = Map[i];

	for(i = 0; i < SLIDE_N; i++)
		Chains2[i] = Chains[i];
}
//---------------------------------------------------------------------------
void SlideCompressor::Restore()
{
	S = S2;
	int i;
	for(i = 0; i < SLIDE_N + SLIDE_M - 1; i++)
		Text[i] = Text2[i];

	for(i = 0; i < 256*256; i++)
		Map[i] = Map2[i];

	for(i = 0; i < SLIDE_N; i++)
		Chains[i] = Chains2[i];
}

class TLG6BitStream
{
	int BufferBitPos; // bit position of output buffer
	long BufferBytePos; // byte position of output buffer
	std::ostream * OutStream; // output stream
	unsigned char *Buffer; // output buffer
	long BufferCapacity; // output buffer capacity

public:
	TLG6BitStream(std::ostream * outstream) :
		OutStream(outstream),
		BufferBitPos(0),
		BufferBytePos(0),
		Buffer(NULL),
		BufferCapacity(0)
	{
	}

	~TLG6BitStream()
	{
		Flush();
	}

public:
	int GetBitPos() const { return BufferBitPos; }
	long GetBytePos() const { return BufferBytePos; }

	void Flush()
	{
		if(Buffer && (BufferBitPos || BufferBytePos))
		{
			if(BufferBitPos) BufferBytePos ++;
			OutStream->write((const char *)Buffer, BufferBytePos);
			BufferBytePos = 0;
			BufferBitPos = 0;
		}
		free(Buffer);
		BufferCapacity = 0;
		Buffer = NULL;
	}

	long GetBitLength() const { return BufferBytePos * 8 + BufferBitPos; }

	void Put1Bit(bool b)
	{
		if(BufferBytePos == BufferCapacity)
		{
			// need more bytes
			long org_cap = BufferCapacity;
			BufferCapacity += 0x1000;
			if(Buffer)
				Buffer = (unsigned char *)realloc(Buffer, BufferCapacity);
			else
				Buffer = (unsigned char *)malloc(BufferCapacity);
			if(!Buffer) throw std::runtime_error("TVPTlgInsufficientMemory");
			memset(Buffer + org_cap, 0, BufferCapacity - org_cap);
		}

		if(b) Buffer[BufferBytePos] |= 1 << BufferBitPos;
		BufferBitPos ++;
		if(BufferBitPos == 8)
		{
			BufferBitPos = 0;
			BufferBytePos ++;
		}
	}

	void PutGamma(int v)
	{
		// Put a gamma code.
		// v must be larger than 0.
		int t = v;
		t >>= 1;
		int cnt = 0;
		while(t)
		{
			Put1Bit(0);
			t >>= 1;
			cnt ++;
		}
		Put1Bit(1);
		while(cnt--)
		{
			Put1Bit(v&1);
			v >>= 1;
		}
	}

	void PutInterleavedGamma(int v)
	{
		// Put a gamma code, interleaved.
		// interleaved gamma codes are:
		//     1 :                   1
		//   <=3 :                 1x0
		//   <=7 :               1x0x0
		//  <=15 :             1x0x0x0
		//  <=31 :           1x0x0x0x0
		// and so on.
		// v must be larger than 0.
		
		v --;
		while(v)
		{
			v >>= 1;
			Put1Bit(0);
			Put1Bit(v&1);
		}
		Put1Bit(1);
	}

	static int GetGammaBitLengthGeneric(int v)
	{
		int needbits = 1;
		v >>= 1;
		while(v)
		{
			needbits += 2;
			v >>= 1;
		}
		return needbits;
	}

	static int GetGammaBitLength(int v)
	{
		// Get bit length where v is to be encoded as a gamma code.
		if(v<=1) return 1;    //                   1
		if(v<=3) return 3;    //                 x10
		if(v<=7) return 5;    //               xx100
		if(v<=15) return 7;   //             xxx1000
		if(v<=31) return 9;   //          x xxx10000
		if(v<=63) return 11;  //        xxx xx100000
		if(v<=127) return 13; //      xxxxx x1000000
		if(v<=255) return 15; //    xxxxxxx 10000000
		if(v<=511) return 17; //   xxxxxxx1 00000000
		return GetGammaBitLengthGeneric(v);
	}

	void PutNonzeroSigned(int v, int len)
	{
		// Put signed value into the bit pool, as length of "len".
		// v must not be zero. abs(v) must be less than 257.
		if(v > 0) v--;
		while(len --)
		{
			Put1Bit(v&1);
			v >>= 1;
		}
	}

	static int GetNonzeroSignedBitLength(int v)
	{
		// Get bit (minimum) length where v is to be encoded as a non-zero signed value.
		// v must not be zero. abs(v) must be less than 257.
		if(v == 0) return 0;
		if(v < 0) v = -v;
		if(v <= 1) return 1;
		if(v <= 2) return 2;
		if(v <= 4) return 3;
		if(v <= 8) return 4;
		if(v <= 16) return 5;
		if(v <= 32) return 6;
		if(v <= 64) return 7;
		if(v <= 128) return 8;
		if(v <= 256) return 9;
		return 10;
	}

	void PutValue(long v, int len)
	{
		// put value "v" as length of "len"
		while(len --)
		{
			Put1Bit(v&1);
			v >>= 1;
		}
	}

};

// Table for 'k' (predicted bit length) of golomb encoding
#define TVP_TLG6_GOLOMB_N_COUNT 4

// golomb bit length table is compressed, so we need to
// decompress it.
char TVPTLG6GolombBitLengthTable[TVP_TLG6_GOLOMB_N_COUNT*2*128][TVP_TLG6_GOLOMB_N_COUNT];
static bool TVPTLG6GolombTableInit = false;
static short int TVPTLG6GolombCompressed[TVP_TLG6_GOLOMB_N_COUNT][9] = {
		{3,7,15,27,63,108,223,448,130,},
		{3,5,13,24,51,95,192,384,257,},
		{2,5,12,21,39,86,155,320,384,},
		{2,3,9,18,33,61,129,258,511,},
	// Tuned by W.Dee, 2004/03/25
};

void TVPTLG6InitGolombTable()
{
	if(TVPTLG6GolombTableInit) return;
	for(int n = 0; n < TVP_TLG6_GOLOMB_N_COUNT; n++)
	{
		int a = 0;
		for(int i = 0; i < 9; i++)
		{
			for(int j = 0; j < TVPTLG6GolombCompressed[n][i]; j++)
				TVPTLG6GolombBitLengthTable[a++][n] = (char)i;
		}
		if(a != TVP_TLG6_GOLOMB_N_COUNT*2*128)
			*(char*)0 = 0;   // THIS MUST NOT BE EXECUETED!
				// (this is for compressed table data check)
	}
	TVPTLG6GolombTableInit = true;
}

#define GOLOMB_GIVE_UP_BYTES 4

void CompressValuesGolomb(TLG6BitStream &bs, char *buf, int size)
{
	// golomb encoding, -- http://oku.edu.mie-u.ac.jp/~okumura/compression/golomb/

	// run-length golomb method
	bs.PutValue(buf[0]?1:0, 1); // initial value state

	int count;

	int n = TVP_TLG6_GOLOMB_N_COUNT - 1; // 個数のカウンタ
	int a = 0; // 予測誤差の絶対値の和

	count = 0;
	for(int i = 0; i < size; i++)
	{
		if(buf[i])
		{
			// write zero count
			if(count) bs.PutGamma(count);

			// count non-zero values
			count = 0;
			int ii;
			for(ii = i; ii < size; ii++)
			{
				if(buf[ii]) count++; else break;
			}

			// write non-zero count
			bs.PutGamma(count);

			// write non-zero values
			for(; i < ii; i++)
			{
				int e = buf[i];
#ifdef WRITE_VSTXT
				fprintf(vstxt, "%d ", e);
#endif
				int k = TVPTLG6GolombBitLengthTable[a][n];
				int m = ((e >= 0) ? 2*e : -2*e-1) - 1;
				int store_limit = bs.GetBytePos() + GOLOMB_GIVE_UP_BYTES;
				bool put1 = true;
				for(int c = (m >> k); c > 0; c--)
				{
					if(store_limit == bs.GetBytePos())
					{
						bs.PutValue(m >> k, 8);
#ifdef WRITE_VSTXT
						fprintf(vstxt, "[ %d ] ", m>>k);
#endif
						put1 = false;
						break;
					}
					bs.Put1Bit(0);
				}
				if(store_limit == bs.GetBytePos())
				{
					bs.PutValue(m >> k, 8);
#ifdef WRITE_VSTXT
					fprintf(vstxt, "[ %d ] ", m>>k);
#endif
					put1 = false;
				}
				if(put1) bs.Put1Bit(1);
				bs.PutValue(m, k);
				a += (m>>1);
				if (--n < 0) {
					a >>= 1; n = TVP_TLG6_GOLOMB_N_COUNT - 1;
				}
			}

#ifdef WRITE_VSTXT
			fprintf(vstxt, "\n");
#endif

			i = ii - 1;
			count = 0;
		}
		else
		{
			// zero
			count ++;
		}
	}

	if(count) bs.PutGamma(count);
}

class TryCompressGolomb
{
	int TotalBits; // total bit count
	int Count; // running count
	int N;
	int A;
	bool LastNonZero;

public:
	TryCompressGolomb()
	{
		Reset();
	}

	TryCompressGolomb(const TryCompressGolomb &ref)
	{
		Copy(ref);
	}

	~TryCompressGolomb() { ; }

	void Copy(const TryCompressGolomb &ref)
	{
		TotalBits = ref.TotalBits;
		Count = ref.Count;
		N = ref.N;
		A = ref.A;
		LastNonZero = ref.LastNonZero;
	}

	void Reset()
	{
		TotalBits = 1;
		Count = 0;
		N = TVP_TLG6_GOLOMB_N_COUNT - 1;
		A = 0;
		LastNonZero = false;
	}

	int Try(char *buf, int size)
	{
		for(int i = 0; i < size; i++)
		{
			if(buf[i])
			{
				// write zero count
				if(!LastNonZero)
				{
					if(Count)
						TotalBits +=
							TLG6BitStream::GetGammaBitLength(Count);

					// count non-zero values
					Count = 0;
				}

				// write non-zero values
				for(; i < size; i++)
				{
					int e = buf[i];
					if(!e) break;
					Count ++;
					int k = TVPTLG6GolombBitLengthTable[A][N];
					int m = ((e >= 0) ? 2*e : -2*e-1) - 1;
					int unexp_bits = (m>>k);
					if(unexp_bits >= (GOLOMB_GIVE_UP_BYTES*8-8/2))
						unexp_bits = (GOLOMB_GIVE_UP_BYTES*8-8/2)+8;
					TotalBits += unexp_bits + 1 + k;
					A += (m>>1);
					if (--N < 0) {
						A >>= 1; N = TVP_TLG6_GOLOMB_N_COUNT - 1;
					}
				}

				// write non-zero count

				i--;
				LastNonZero = true;
			}
			else
			{
				// zero
				if(LastNonZero)
				{
					if(Count)
					{
						TotalBits += TLG6BitStream::GetGammaBitLength(Count);
						Count = 0;
					}
				}

				Count ++;
				LastNonZero = false;
			}
		}
		return TotalBits;
	}

	int Flush()
	{
		if(Count)
		{
			TotalBits += TLG6BitStream::GetGammaBitLength(Count);
			Count = 0;
		}
		return TotalBits;
	}
};

#define DO_FILTER \
	len -= 4; \
	for(d = 0; d < len; d+=4) \
	{ FILTER_FUNC(0); FILTER_FUNC(1); FILTER_FUNC(2); FILTER_FUNC(3); } \
	len += 4; \
	for(; d < len; d++) \
	{ FILTER_FUNC(0); }

void ApplyColorFilter(char * bufb, char * bufg, char * bufr, int len, int code)
{
	int d;
	unsigned char t;
	switch(code)
	{
	case 0:
		break;
	case 1:
		#define FILTER_FUNC(n) \
			bufr[d+n] -= bufg[d+n], \
			bufb[d+n] -= bufg[d+n];
		DO_FILTER
		break;
	case 2:
		for( d = 0; d < len; d++)
			bufr[d] -= bufg[d],
			bufg[d] -= bufb[d];
		break;
	case 3:
		for( d = 0; d < len; d++)
			bufb[d] -= bufg[d],
			bufg[d] -= bufr[d];
		break;
	case 4:
		for( d = 0; d < len; d++)
			bufr[d] -= bufg[d],
			bufg[d] -= bufb[d],
			bufb[d] -= bufr[d];
		break;
	case 5:
		for( d = 0; d < len; d++)
			bufg[d] -= bufb[d],
			bufb[d] -= bufr[d];
		break;
	case 6:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			bufb[d+n] -= bufg[d+n];
		DO_FILTER
		break;
	case 7:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			bufg[d+n] -= bufb[d+n];
		DO_FILTER
		break;
	case 8:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			bufr[d+n] -= bufg[d+n];
		DO_FILTER
		break;
	case 9:
		for( d = 0; d < len; d++)
			bufb[d] -= bufg[d],
			bufg[d] -= bufr[d],
			bufr[d] -= bufb[d];
		break;
	case 10:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			bufg[d+n] -= bufr[d+n], \
			bufb[d+n] -= bufr[d+n];
		DO_FILTER
		break;
	case 11:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			bufr[d+n] -= bufb[d+n], \
			bufg[d+n] -= bufb[d+n];
		DO_FILTER
		break;
	case 12:
		for( d = 0; d < len; d++)
			bufg[d] -= bufr[d],
			bufr[d] -= bufb[d];
		break;
	case 13:
		for( d = 0; d < len; d++)
			bufg[d] -= bufr[d],
			bufr[d] -= bufb[d],
			bufb[d] -= bufg[d];
		break;
	case 14:
		for( d = 0; d < len; d++)
			bufr[d] -= bufb[d],
			bufb[d] -= bufg[d],
			bufg[d] -= bufr[d];
		break;
	case 15:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			t = bufb[d+n]<<1; \
			bufr[d+n] -= t, \
			bufg[d+n] -= t;
		DO_FILTER
		break;
	case 16:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			bufg[d+n] -= bufr[d+n];
		DO_FILTER
		break;
	case 17:
		for( d = 0; d < len; d++)
			bufr[d] -= bufb[d],
			bufb[d] -= bufg[d];
		break;
	case 18:
		for( d = 0; d < len; d++)
			bufr[d] -= bufb[d];
		break;
	case 19:
		for( d = 0; d < len; d++)
			bufb[d] -= bufr[d],
			bufr[d] -= bufg[d];
		break;
	case 20:
		for( d = 0; d < len; d++)
			bufb[d] -= bufr[d];
		break;
	case 21:
		for( d = 0; d < len; d++)
			bufb[d] -= bufg[d]>>1;
		break;
	case 22:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			bufg[d+n] -= bufb[d+n]>>1;
		DO_FILTER
		break;
	case 23:
		for( d = 0; d < len; d++)
			bufg[d] -= bufb[d],
			bufb[d] -= bufr[d],
			bufr[d] -= bufg[d];
		break;
	case 24:
		for( d = 0; d < len; d++)
			bufb[d] -= bufr[d],
			bufr[d] -= bufg[d],
			bufg[d] -= bufb[d];
		break;
	case 25:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			bufg[d+n] -= bufr[d+n]>>1;
		DO_FILTER
		break;
	case 26:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			bufr[d+n] -= bufg[d+n]>>1;
		DO_FILTER
		break;
	case 27:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			t = bufr[d+n]>>1; \
			bufg[d+n] -= t, \
			bufb[d+n] -= t;
		DO_FILTER
		break;
	case 28:
		for( d = 0; d < len; d++)
			bufr[d] -= bufb[d]>>1;
		break;
	case 29:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			t = bufg[d+n]>>1; \
			bufr[d+n] -= t, \
			bufb[d+n] -= t;
		DO_FILTER
		break;
	case 30:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			t = bufb[d+n]>>1; \
			bufr[d+n] -= t, \
			bufg[d+n] -= t;
		DO_FILTER
		break;
	case 31:
		for( d = 0; d < len; d++)
			bufb[d] -= bufr[d]>>1;
		break;
	case 32:
		for( d = 0; d < len; d++)
			bufr[d] -= bufb[d]<<1;
		break;
	case 33:
		for( d = 0; d < len; d++)
			bufb[d] -= bufg[d]<<1;
		break;
	case 34:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			t = bufr[d+n]<<1; \
			bufg[d+n] -= t, \
			bufb[d+n] -= t;
		DO_FILTER
		break;
	case 35:
		for( d = 0; d < len; d++)
			bufg[d] -= bufb[d]<<1;
		break;
	case 36:
		for( d = 0; d < len; d++)
			bufr[d] -= bufg[d]<<1;
		break;
	case 37:
		for( d = 0; d < len; d++)
			bufr[d] -= bufg[d]<<1,
			bufb[d] -= bufg[d]<<1;
		break;
	case 38:
		for( d = 0; d < len; d++)
			bufg[d] -= bufr[d]<<1;
		break;
	case 39:
		for( d = 0; d < len; d++)
			bufb[d] -= bufr[d]<<1;
		break;

	}

}
#define MAX_COLOR_COMPONENTS 4

#define FILTER_TRY_COUNT 16

#define W_BLOCK_SIZE 8
#define H_BLOCK_SIZE 8

int DetectColorFilter(char *b, char *g, char *r, int size, int &outsize)
{
#ifndef FILTER_TEST
	int minbits = -1;
	int mincode = -1;

	char bbuf[H_BLOCK_SIZE*W_BLOCK_SIZE];
	char gbuf[H_BLOCK_SIZE*W_BLOCK_SIZE];
	char rbuf[H_BLOCK_SIZE*W_BLOCK_SIZE];
	TryCompressGolomb bc, gc, rc;

	for(int code = 0; code < FILTER_TRY_COUNT; code++)   // 17..27 are currently not used
	{
		// copy bbuf, gbuf, rbuf into b, g, r.
		memcpy(bbuf, b, sizeof(char)*size);
		memcpy(gbuf, g, sizeof(char)*size);
		memcpy(rbuf, r, sizeof(char)*size);

		// copy compressor
		bc.Reset();
		gc.Reset();
		rc.Reset();

		// Apply color filter
		ApplyColorFilter(bbuf, gbuf, rbuf, size, code);

		// try to compress
		int bits;
		bits  = (bc.Try(bbuf, size), bc.Flush());
		if(minbits != -1 && minbits < bits) continue;
		bits += (gc.Try(gbuf, size), gc.Flush());
		if(minbits != -1 && minbits < bits) continue;
		bits += (rc.Try(rbuf, size), rc.Flush());

		if(minbits == -1 || minbits > bits)
		{
			minbits = bits, mincode = code;
		}
	}

	outsize = minbits;

	return mincode;
#else
	static int filter = 0;

	int f = filter;

	filter++;
	if(filter >= FILTER_TRY_COUNT) filter = 0;

	outsize = 0;

	return f;
#endif
}

static void WriteInt32(long num, std::ostream *out)
{
	char buf[4];
	buf[0] = num & 0xff;
	buf[1] = (num >> 8) & 0xff;
	buf[2] = (num >> 16) & 0xff;
	buf[3] = (num >> 24) & 0xff;
	out->write(buf, 4);
}

static void TLG6InitializeColorFilterCompressor(SlideCompressor &c)
{
	unsigned char code[4096];
	unsigned char dum[4096];
	unsigned char *p = code;
	for(int i = 0; i < 32; i++)
	{
		for(int j = 0; j < 16; j++)
		{
			p[0] = p[1] = p[2] = p[3] = i;
			p += 4;
			p[0] = p[1] = p[2] = p[3] = j;
			p += 4;
		}
	}

	long dumlen;
	c.Encode(code, 4096, dum, dumlen);
}

void SaveTLG6( std::ostream* stream, const tTVPBaseBitmap* bmp, bool is24 )
{
	std::ostream *out = stream;

	// DWORD medstart, medend;
#ifdef WRITE_ENTROPY_VALUES
	FILE *vs = fopen("vs.bin", "wb");
#endif

	int colors;

	TVPTLG6InitGolombTable();

	// check pixel format
	if( bmp->Is32BPP() ) {
		if( is24 ) colors = 3;
		else colors = 4;
	} else {
		colors = 1;
	}
	int stride = colors;
	if( stride == 3 ) stride = 4;

	// output stream header
	{
		out->write("TLG6.0\x00raw\x1a\x00", 11);
		out->write((const char *)&colors, 1);
		int n = 0;
		out->write((const char *)&n, 1); // data flag (0)
		out->write((const char *)&n, 1); // color type (0)
		out->write((const char *)&n, 1); // external golomb table (0)
		int width = bmp->GetWidth();
		int height = bmp->GetHeight();
		WriteInt32(width, out);
		WriteInt32(height, out);
	}

	// compress
	long max_bit_length = 0;

	unsigned char *buf[MAX_COLOR_COMPONENTS];
	for(int i = 0; i < MAX_COLOR_COMPONENTS; i++) buf[i] = NULL;
	char *block_buf[MAX_COLOR_COMPONENTS];
	for(int i = 0; i < MAX_COLOR_COMPONENTS; i++) block_buf[i] = NULL;
	unsigned char *filtertypes = NULL;
	std::stringstream *memstream = NULL;

	try
	{
		memstream = new std::stringstream();

		TLG6BitStream bs(memstream);

//		int buf_size = bmp->Width * bmp->Height;

		// allocate buffer
		for(int c = 0; c < colors; c++)
		{
			buf[c] = new unsigned char [W_BLOCK_SIZE * H_BLOCK_SIZE * 3];
			block_buf[c] = new char [H_BLOCK_SIZE * bmp->GetWidth()];
		}
		int w_block_count = (int)((bmp->GetWidth() - 1) / W_BLOCK_SIZE) + 1;
		int h_block_count = (int)((bmp->GetHeight() - 1) / H_BLOCK_SIZE) + 1;
		filtertypes = new unsigned char [w_block_count * h_block_count];

		int fc = 0;
		for(int y = 0; y < (int)bmp->GetHeight(); y += H_BLOCK_SIZE)
		{
			int ylim = y + H_BLOCK_SIZE;
			if(ylim > (int)bmp->GetHeight()) ylim = bmp->GetHeight();
			int gwp = 0;
			int xp = 0;
			for(int x = 0; x < (int)bmp->GetWidth(); x += W_BLOCK_SIZE, xp++)
			{
				int xlim = x + W_BLOCK_SIZE;
				if(xlim > (int)bmp->GetWidth()) xlim = bmp->GetWidth();
				int bw = xlim - x;

				int p0size; // size of MED method (p=0)
				int minp = 0; // most efficient method (0:MED, 1:AVG)
				int ft; // filter type
				int wp; // write point
				for(int p = 0; p < 2; p++)
				{
					int dbofs = (p+1) * (H_BLOCK_SIZE * W_BLOCK_SIZE);

					// do med(when p=0) or take average of upper and left pixel(p=1)
					for(int c = 0; c < colors; c++)
					{
						int wp = 0;
						for(int yy = y; yy < ylim; yy++)
						{
							const unsigned char * sl = x*stride +
								c + (const unsigned char *)bmp->GetScanLine(yy);
							const unsigned char * usl;
							if(yy >= 1)
								usl = x*stride + c + (const unsigned char *)bmp->GetScanLine(yy-1);
							else
								usl = NULL;
							for(int xx = x; xx < xlim; xx++)
							{
								unsigned char pa = xx > 0 ? sl[-stride] : 0;
								unsigned char pb = usl ? *usl : 0;
								unsigned char px = *sl;

								unsigned char py;

//								py = 0;
								if(p == 0)
								{
									unsigned char pc = (xx > 0 && usl) ? usl[-stride] : 0;
									unsigned char min_a_b = pa>pb?pb:pa;
									unsigned char max_a_b = pa<pb?pb:pa;

									if(pc >= max_a_b)
										py = min_a_b;
									else if(pc < min_a_b)
										py = max_a_b;
									else
										py = pa + pb - pc;
								}
								else
								{
									py = (pa+pb+1)>>1;
								}
								
								// stbi_image format is RGBA, so swap to BGRA
								int wc = c == 0 ? 2 : c == 2 ? 0 : c;
								buf[wc][wp] = (unsigned char)(px - py);

								wp++;
								sl += stride;
								if(usl) usl += stride;
							}
						}
					}

					// reordering
					// Transfer the data into block_buf (block buffer).
					// Even lines are stored forward (left to right),
					// Odd lines are stored backward (right to left).

					wp = 0;
					for(int yy = y; yy < ylim; yy++)
					{
						int ofs;
						if(!(xp&1))
							ofs = (yy - y)*bw;
						else
							ofs = (ylim - yy - 1) * bw;
						bool dir; // false for forward, true for backward
						if(!((ylim-y)&1))
						{
							// vertical line count per block is even
							dir = ((yy&1) ^ (xp&1)) ? true : false;
						}
						else
						{
							// otherwise;
							if(xp & 1)
							{
								dir = (yy&1);
							}
							else
							{
								dir = ((yy&1) ^ (xp&1)) ? true : false;
							}
						}

						if(!dir)
						{
							// forward
							for(int xx = 0; xx < bw; xx++)
							{
								for(int c = 0; c < colors; c++)
									buf[c][wp + dbofs] =
									buf[c][ofs + xx];
								wp++;
							}
						}
						else
						{
							// backward
							for(int xx = bw - 1; xx >= 0; xx--)
							{
								for(int c = 0; c < colors; c++)
									buf[c][wp + dbofs] =
									buf[c][ofs + xx];
								wp++;
							}
						}
					}
				}


				for(int p = 0; p < 2; p++)
				{
					int dbofs = (p+1) * (H_BLOCK_SIZE * W_BLOCK_SIZE);
					// detect color filter
					int size = 0;
					int ft_;
					if(colors >= 3)
						ft_ = DetectColorFilter(
							reinterpret_cast<char*>(buf[0] + dbofs),
							reinterpret_cast<char*>(buf[1] + dbofs),
							reinterpret_cast<char*>(buf[2] + dbofs), wp, size);
					else
						ft_ = 0;

					// select efficient mode of p (MED or average)
					if(p == 0)
					{
						p0size = size;
						ft = ft_;
					}
					else
					{
						if(p0size >= size)
							minp = 1, ft = ft_;
					}
				}

				// Apply most efficient color filter / prediction method
				wp = 0;
				int dbofs = (minp + 1)  * (H_BLOCK_SIZE * W_BLOCK_SIZE);
				for(int yy = y; yy < ylim; yy++)
				{
					for(int xx = 0; xx < bw; xx++)
					{
						for(int c = 0; c < colors; c++)
							block_buf[c][gwp + wp] = buf[c][wp + dbofs];
						wp++;
					}
				}

				ApplyColorFilter(block_buf[0] + gwp,
					block_buf[1] + gwp, block_buf[2] + gwp, wp, ft);

				filtertypes[fc++] = (ft<<1) + minp;
//				ftfreq[ft]++;
				gwp += wp;
			}

			// compress values (entropy coding)
			for(int c = 0; c < colors; c++)
			{
				int method;
				CompressValuesGolomb(bs, block_buf[c], gwp);
				method = 0;
#ifdef WRITE_ENTROPY_VALUES
				fwrite(block_buf[c], 1, gwp, vs);
#endif
				long bitlength = bs.GetBitLength();
				if(bitlength & 0xc0000000)
					throw std::runtime_error("TVPTlgTooLargeBitLength");
				// two most significant bits of bitlength are
				// entropy coding method;
				// 00 means Golomb method,
				// 01 means Gamma method (implemented but not used),
				// 10 means modified LZSS method (not yet implemented),
				// 11 means raw (uncompressed) data (not yet implemented).
				if(max_bit_length < bitlength) max_bit_length = bitlength;
				bitlength |= (method << 30);
				WriteInt32(bitlength, memstream);
				bs.Flush();
			}

		}


		// write max bit length
		WriteInt32(max_bit_length, out);

		// output filter types
		{
			SlideCompressor comp;
			TLG6InitializeColorFilterCompressor(comp);
			unsigned char *outbuf = new unsigned char[fc * 2];
			try
			{
				long outlen;
				comp.Encode(filtertypes, fc, outbuf, outlen);
				WriteInt32(outlen, out);
				out->write((const char *)outbuf, outlen);
			}
			catch(...)
			{
				delete [] outbuf;
				throw std::runtime_error("Encoding error");
			}
			delete [] outbuf;

		}

		// copy memory stream to output stream
		std::string str(memstream->str());
		out->write((const char *)str.data(), (tjs_uint)str.size() );
	}
	catch(...)
	{
		for(int i = 0; i < MAX_COLOR_COMPONENTS; i++)
		{
			if(buf[i]) delete [] (buf[i]);
			if(block_buf[i]) delete [] (block_buf[i]);
		}
		if(filtertypes) delete [] filtertypes;
		if(memstream) delete memstream;
		throw;
	}

	for(int i = 0; i < MAX_COLOR_COMPONENTS; i++)
	{
		if(buf[i]) delete [] (buf[i]);
		if(block_buf) delete [] (block_buf[i]);
	}
	if(filtertypes) delete [] filtertypes;
	if(memstream) delete memstream;

#ifdef WRITE_ENTROPY_VALUES
	fclose(vs);
#endif
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		std::cerr << "usage: " << argv[0] << " input output" << std::endl;
		return 1;
	}
	tTVPBaseBitmap bmp(argv[1]);
	std::fstream fs(argv[2], std::fstream::out | std::fstream::binary);
	SaveTLG6( &fs, &bmp, false );
	return 0;
}
