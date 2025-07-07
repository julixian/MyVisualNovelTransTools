
; ****************************************************************************
;                        E R I S A - L i b r a r y
; ----------------------------------------------------------------------------
;         Copyright (C) 2000-2004 Leshade Entis. All rights reserved.
; ****************************************************************************


;
;	画像情報構造体
; ----------------------------------------------------------------------------

PVOID	TYPEDEF	PTR
PBYTE	TYPEDEF	PTR BYTE
PSBYTE	TYPEDEF	PTR SBYTE
PWORD	TYPEDEF	PTR WORD
PINT	TYPEDEF	PTR SDWORD
PREAL32	TYPEDEF	PTR REAL4

ERI_INFO_HEADER		STRUCT
dwVersion		DWORD	?
fdwTransformation	DWORD	?
dwArchitecture		DWORD	?
fdwFormatType		DWORD	?
nImageWidth		SDWORD	?
nImageHeight		SDWORD	?
dwBitsPerPixel		DWORD	?
dwClippedPixel		DWORD	?
dwSamplingFlags		DWORD	?
dwQuantumizedBits	SDWORD	?, ?
dwAllottedBits		DWORD	?, ?
dwBlockingDegree	DWORD	?
dwLappedBlock		DWORD	?
dwFrameTransform	DWORD	?
dwFrameDegree		DWORD	?
ERI_INFO_HEADER		ENDS

RASTER_IMAGE_INFO	STRUCT
fdwFormatType		DWORD	?
ptrImageArray		PBYTE	?
nImageWidth		SDWORD	?
nImageHeight		SDWORD	?
dwBitsPerPixel		DWORD	?
BytesPerLine		SDWORD	?
RASTER_IMAGE_INFO	ENDS
PRASTER_IMAGE_INFO	TYPEDEF	PTR RASTER_IMAGE_INFO

EFH_CONTAIN_IMAGE	EQU	00000001H
EFH_CONTAIN_ALPHA	EQU	00000002H
EFH_CONTAIN_PALETTE	EQU	00000010H
EFH_CONTAIN_WAVE	EQU	00000100H
EFH_CONTAIN_SEQUENCE	EQU	00000200H

ERI_RGB_IMAGE		EQU	00000001H
ERI_RGBA_IMAGE		EQU	04000001H
ERI_GRAY_IMAGE		EQU	00000002H
ERI_TYPE_MASK		EQU	00FFFFFFH
ERI_WITH_PALETTE	EQU	01000000H
ERI_USE_CLIPPING	EQU	02000000H
ERI_WITH_ALPHA		EQU	04000000H

CVTYPE_LOSSLESS_ERI	EQU	03020000H
CVTYPE_DCT_ERI		EQU	00000001H
CVTYPE_LOT_ERI		EQU	00000005H
CVTYPE_LOT_ERI_MSS	EQU	00000105H

ERI_ARITHMETIC_CODE	EQU	32
ERI_RUNLENGTH_GAMMA	EQU	0FFFFFFFFH
ERI_RUNLENGTH_HUFFMAN	EQU	0FFFFFFFCH

ERISF_YUV_4_4_4		EQU	00040404H
ERISF_YUV_4_2_2		EQU	00040202H
ERISF_YUV_4_1_1		EQU	00040101H


;
;	例外処理構造体
; ----------------------------------------------------------------------------

TIB_pSEHList		EQU	00H
ExceptionContinueExecution	EQU	0
ExceptionContinueSearch		EQU	1
ExceptionNestedException	EQU	2
ExceptionCollidedUnwind		EQU	3


; ---- The size of 80387 registers in byte ----

SIZE_OF_80387_REGISTERS	EQU	80


; ---- The structure for floating registers ----

FLOATING_SAVE_AREA	STRUCT	1
ControlWord		DWORD	?
StatusWord		DWORD	?
TagWord			DWORD	?
ErrorOffset		DWORD	?
ErrorSelector		DWORD	?
DataOffset		DWORD	?
DataSelector		DWORD	?
RegisterArea		BYTE	SIZE_OF_80387_REGISTERS DUP( ? )
Cr0NpxState		DWORD	?
FLOATING_SAVE_AREA	ENDS


; ---- The structure of the context that is IA32 ----

CONTEXT	STRUCT	1

ContextFlags	DWORD	?

__Dr0		DWORD	?
__Dr1		DWORD	?
__Dr2		DWORD	?
__Dr3		DWORD	?
__Dr6		DWORD	?
__Dr7		DWORD	?

FloatSave	FLOATING_SAVE_AREA	{}

__SegGs		DWORD	?
__SegFs		DWORD	?
__SegEs		DWORD	?
__SegDs		DWORD	?

__Edi		DWORD	?
__Esi		DWORD	?
__Ebx		DWORD	?
__Edx		DWORD	?
__Ecx		DWORD	?
__Eax		DWORD	?

__Ebp		DWORD	?
__Eip		DWORD	?
__SegCs		DWORD	?
__EFlags	DWORD	?
__Esp		DWORD	?
__SegSs		DWORD	?

CONTEXT	ENDS
PCONTEXT		TYPEDEF	PTR CONTEXT


; ---- The maximum number of exception parameters ----

EXCEPTION_MAXIMUM_PARAMETERS	EQU	15

; ---- The exception record ----

EXCEPTION_RECORD	STRUCT	1
ExceptionCode		DWORD	?
ExceptionFlags		DWORD	?
ExceptionRecord		PVOID	?
ExceptionAddress	PVOID	?
NumberParameters	DWORD	?
ExceptionInformation	DWORD	EXCEPTION_MAXIMUM_PARAMETERS DUP( ? )
EXCEPTION_RECORD	ENDS
PEXCEPTION_RECORD	TYPEDEF	PTR EXCEPTION_RECORD


;
;	ハフマン・ツリー・構造体
; ----------------------------------------------------------------------------

ERINA_CODE_FLAG		EQU	80000000H
ERINA_HUFFMAN_ESCAPE	EQU	7FFFFFFFH
ERINA_HUFFMAN_NULL	EQU	8000H
ERINA_HUFFMAN_MAX	EQU	4000H
ERINA_HUFFMAN_ROOT	EQU	200H

ERINA_HUFFMAN_NODE	STRUCT
m_weight		WORD	?
m_parent		WORD	?
m_child_code		DWORD	?
ERINA_HUFFMAN_NODE	ENDS

IF	(SIZEOF ERINA_HUFFMAN_NODE) Ne 8
	.ERR
ENDIF

PERINA_HUFFMAN_NODE	TYPEDEF	PTR ERINA_HUFFMAN_NODE

ERINA_HUFFMAN_TREE	STRUCT
m_hnTree		ERINA_HUFFMAN_NODE	201H DUP( { } )
m_iSymLookup		SDWORD	100H DUP( ? )
m_iEscape		SDWORD	?
m_iTreePointer		SDWORD	?
ERINA_HUFFMAN_TREE	ENDS

PERINA_HUFFMAN_TREE	TYPEDEF	PTR ERINA_HUFFMAN_TREE


;
;	BSHF 符号バッファ
; ----------------------------------------------------------------------------

EString		STRUCT
pvfnTable	PVOID	?		; 仮想関数テーブル
m_pszString	PSBYTE	?		; 文字列ポインタ
m_nLength	DWORD	?		; 文字列長
m_nBufLimit	DWORD	?		; バッファ長
EString		ENDS

ERIBshfBuffer	STRUCT
pvfnTable	PVOID	?		; 仮想関数テーブル
m_strPassword	EString	{}		; パスワード
m_dwPassOffset	DWORD	?		; パスワードオフセット
m_bufBSHF	BYTE	32 DUP(?)	; BSHF 符号用バッファ
m_srcBSHF	BYTE	32 DUP(?)
m_maskBSHF	BYTE	32 DUP(?)
ERIBshfBuffer	ENDS


;
;	展開コンテキスト
; ----------------------------------------------------------------------------

ERISA_TOTAL_LIMIT	EQU	2000H		; 母数の限界値
ERISA_SYMBOL_SORTS	EQU	101H		; シンボルの種類
ERISA_SUB_SORT_MAX	EQU	80H
ERISA_PROB_SLOT_MAX	EQU	800H		; 統計モデルの最大スロット数
ERISA_ESC_CODE		EQU	(-1)		; エスケープ記号

ERISA_CODE_SYMBOL	STRUCT
wOccured		WORD	?	; シンボルの出現回数
wSymbol			SWORD	?	; シンボル
ERISA_CODE_SYMBOL	ENDS

ERISA_PROB_MODEL	STRUCT
dwTotalCount		DWORD	?	; 母数 < 2000H
dwSymbolSorts		DWORD	?	; シンボルの種類数
dwReserved		DWORD	2 DUP(?)
acsSymTable		ERISA_CODE_SYMBOL	ERISA_SYMBOL_SORTS DUP({})
dwReserved2		DWORD	3 DUP(?)
acsSubModel		ERISA_CODE_SYMBOL	ERISA_SUB_SORT_MAX DUP({})
ERISA_PROB_MODEL	ENDS

PERISA_PROB_MODEL	TYPEDEF	PTR ERISA_PROB_MODEL

ERISADecodeContext	STRUCT

pvfnTable		PVOID	?	; 仮想関数テーブル

; ビットストリームバッファ
m_nIntBufCount		DWORD	?	; 中間入力バッファに蓄積されているビット数
m_dwIntBuffer		DWORD	?	; 中間入力バッファ
m_nBufferingSize	DWORD	?	; バッファリングするバイト数
m_nBufCount		DWORD	?	; バッファの残りバイト数
m_ptrBuffer		PBYTE	?	; 入力バッファの先頭へのポインタ
m_ptrNextBuf		PBYTE	?	; 次に読み込むべき入力バッファへのポインタ

m_pFile			PVOID	?	; 入力オブジェクト
m_pContext		PVOID	?

; 現在使用されている符号の展開関数
m_pfnDecodeSymbolBytes	PVOID	?

; ランレングスガンマ符号コンテキスト
m_flgZero		SDWORD	?	; ゼロフラグ
m_nLength		DWORD	?	; ランレングス

; ERINA（ハフマン）符号コンテキスト
m_dwERINAFlags		DWORD	0
m_pLastHuffmanTree	PERINA_HUFFMAN_TREE	0
m_ppHuffmanTree		PVOID	?

; ERISA（算術）符号コンテキスト
m_dwCodeRegister	DWORD	?	; コードレジスタ（16 bit）
m_dwAugendRegister	DWORD	?	; オージェンドレジスタ（16 bit）
m_nPostBitCount		SDWORD	?	; 終端ビットバッファカウンタ
m_bytLastSymbol		BYTE	4 DUP(?)	; 最近の生起シンボル
m_iLastSymbol		SDWORD	?
m_pProbERISA		PVOID	?
; ERISA-N 符号コンテキスト
m_nNemesisLeft		SDWORD	?	; スライド辞書復号カウンタ
m_nNemesisNext		SDWORD	?
m_pNemesisBuf		PBYTE	?	; スライド辞書用バッファ
m_nNemesisIndex		SDWORD	?
m_pNemesisLookup	PVOID	?
m_pPhraseLenProb	PVOID	?
m_pPhraseIndexProb	PVOID	?
m_pRunLenProb		PVOID	?
m_pLastERISAProb	PVOID	?
m_ppTableERISA		PVOID	?
m_nFlagEOF		SDWORD	?

; BSHF 符号コンテキスト
m_pBshfBuf		PVOID	?
m_dwBufPos		DWORD	?

ERISADecodeContext	ENDS

PERISADecodeContext	TYPEDEF	PTR ERISADecodeContext


;
;	仮想関数インデックス
; ----------------------------------------------------------------------------

VFI_Destructer		EQU	0
VFI_ReadNextData	EQU	(4*4)


;
;	展開オブジェクト
; ----------------------------------------------------------------------------

ERISADecoder	STRUCT

pvfnTable		PVOID	?		; 仮想関数テーブル

m_eihInfo		ERI_INFO_HEADER	{ }	; 画像情報ヘッダ

; 展開用パラメータ
m_nBlockSize		DWORD	?		; ブロッキングサイズ
m_nBlockArea		DWORD	?		; ブロック面積
m_nBlockSamples		DWORD	?		; ブロックのサンプル数
m_nChannelCount		DWORD	?		; チャネル数
m_nWidthBlocks		DWORD	?		; 画像の幅（ブロック数）
m_nHeightBlocks		DWORD	?		; 画像の高さ（ブロック数）

; 展開先ブロックパラメータ
m_ptrDstBlock		PBYTE	?		; 出力先アドレス
m_nDstLineBytes		SDWORD	?		; 出力先ライン長
m_nDstPixelBytes	DWORD	?		; 1 ピクセルのバイト数
m_nDstWidth		DWORD	?		; 出力先ブロック幅
m_nDstHeight		DWORD	?		; 出力先ブロック高
m_fdwDecFlags		DWORD	?		; 復号フラグ

; 可逆展開用バッファ
m_fEnhancedMode		SDWORD	?		; 拡張モードフラグ
m_ptrOperations		PBYTE	?		; オペレーションテーブル
m_ptrColumnBuf		PSBYTE	?		; 列バッファ
m_ptrLineBuf		PSBYTE	?		; 行バッファ
m_ptrDecodeBuf		PSBYTE	?		; 展開バッファ
m_ptrArrangeBuf		PSBYTE	?		; 再配列用バッファ
m_pArrangeTable		PINT	4 DUP(?)	; 再配列用テーブル

; 非可逆展開用バッファ
m_nBlocksetCount	DWORD	?		; サブブロック数
m_ptrVertBufLOT		PREAL32	?		; 重複変換用バッファ
m_ptrHorzBufLOT		PREAL32	?
m_ptrBlocksetBuf	PREAL32	16 DUP(?)	; ブロックセットバッファ
m_ptrMatrixBuf		PREAL32	?
m_ptrIQParamBuf		PREAL32	?		; 逆量子化パラメータ
m_ptrIQParamTable	PBYTE	?

m_ptrBlockLineBuf	PSBYTE	?		; ブロック行中間バッファ
m_ptrNextBlockBuf	PSBYTE	?
m_ptrImageBuf		PSBYTE	?		; 画像信号バッファ
m_ptrYUVImage		PSBYTE	?		; YUV 画像出力バッファ
m_nYUVLineBytes		SDWORD	?		; YUV 画像ライン長
m_nYUVPixelBytes	DWORD	?		; YUV ピクセルのバイト数

m_ptrMovingVector	PSBYTE	?		; 動き補償ベクトル
m_ptrMoveVecFlags	PBYTE	?
m_ptrMovePrevBlocks	PVOID	?		; 参照ブロックへのポインタ
m_ptrNextPrevBlocks	PVOID	?
m_pPrevImageRef		PVOID	?		; 直前フレームへの参照
m_dwPrevLineBytes	SDWORD	?
m_pNextImageRef		PVOID	?		; 直後フレームへの参照
m_dwNextLineBytes	SDWORD	?
m_pFilterImageBuf	PVOID	?		; フィルタ処理用バッファ参照

m_pHuffmanTree		PVOID	?		; ハフマン木
m_pProbERISA		PVOID	?		; 統計情報

ERISADecoder	ENDS

PERISADecoder	TYPEDEF	PTR ERISADecoder

dfDifferential	EQU	02H
dfQuickDecode	EQU	100H


;
;	グローバル変数
; ----------------------------------------------------------------------------

ERI_USE_MMX_PENTIUM	EQU	0002H
ERI_USE_XMM_P3		EQU	0008H
ERI_USE_SSE2		EQU	0010H

EXTERNDEF	C ERI_EnabledProcessorType:DWORD


;
;	関数プロトタイプ
; ----------------------------------------------------------------------------

ERIBshfBuffer@@DecodeBuffer	TEXTEQU	\
	<?DecodeBuffer@ERIBshfBuffer@@QAEXXZ>

ERINA_HUFFMAN_TREE@@IncreaseOccuredCount	TEXTEQU	\
	<?IncreaseOccuedCount@ERINA_HUFFMAN_TREE@@QAEXH@Z>
ERINA_HUFFMAN_TREE@@Normalize		TEXTEQU	\
	<?Normalize@ERINA_HUFFMAN_TREE@@QAEXH@Z>
ERINA_HUFFMAN_TREE@@AddNewEntry		TEXTEQU	\
	<?AddNewEntry@ERINA_HUFFMAN_TREE@@QAEXH@Z>
ERINA_HUFFMAN_TREE@@RebuildBitMap	TEXTEQU	\
	<?RebuildBitMap@ERINA_HUFFMAN_TREE@@QAEXXZ>
ERINA_HUFFMAN_TREE@@HalfAndRebuild	TEXTEQU	\
	<?HalfAndRebuild@ERINA_HUFFMAN_TREE@@QAEXXZ>

ERISA_PROB_MODEL@@Initialize	TEXTEQU	\
	<?Initialize@ERISA_PROB_MODEL@@QAEXXZ>
ERISA_PROB_MODEL@@IncreaseSymbol	TEXTEQU	\
	<?IncreaseSymbol@ERISA_PROB_MODEL@@QAEHH@Z>
ERISA_PROB_MODEL@@HalfOccuredCount	TEXTEQU	\
	<?HalfOccuredCount@ERISA_PROB_MODEL@@QAEXXZ>

ERISADecodeContext@@PrefetchBuffer	TEXTEQU	\
	<?PrefetchBuffer@ERISADecodeContext@@QAE?AW4ESLError@@XZ>
ERISADecodeContext@@GetABit		TEXTEQU	\
	<?GetABit@ERISADecodeContext@@QAEHXZ>
ERISADecodeContext@@GetNBits		TEXTEQU	\
	<?GetNBits@ERISADecodeContext@@QAEIH@Z>
ERISADecodeContext@@GetGammaCode		TEXTEQU	\
	<?GetGammaCode@ERISADecodeContext@@QAEHXZ>
ERISADecodeContext@@DecodeGammaCodeBytes	TEXTEQU	\
	<?DecodeGammaCodeBytes@ERISADecodeContext@@QAEKPACK@Z>
ERISADecodeContext@@GetHuffmanCode	TEXTEQU	\
	<?GetHuffmanCode@ERISADecodeContext@@QAEHPAUERINA_HUFFMAN_TREE@@@Z>
ERISADecodeContext@@GetLengthHuffman	TEXTEQU	\
	<?GetLengthHuffman@ERISADecodeContext@@QAEHPAUERINA_HUFFMAN_TREE@@@Z>
ERISADecodeContext@@DecodeERINACodeBytes	TEXTEQU	\
	<?DecodeERINACodeBytes@ERISADecodeContext@@QAEKPACK@Z>
ERISADecodeContext@@IncreaseSymbol	TEXTEQU	\
	<?IncreaseSymbol@ERISA_PROB_MODEL@@QAEHH@Z>
ERISADecodeContext@@DecodeERISACodeIndex	TEXTEQU	\
	<?DecodeERISACodeIndex@ERISADecodeContext@@QAEHPAUERISA_PROB_MODEL@@@Z>
ERISADecodeContext@@DecodeERISACode	TEXTEQU	\
	<?DecodeERISACode@ERISADecodeContext@@QAEHPAUERISA_PROB_MODEL@@@Z>
ERISADecodeContext@@DecodeERISACodeBytes	TEXTEQU	\
	<?DecodeERISACodeBytes@ERISADecodeContext@@QAEKPACK@Z>

ERISADecoder@@m_pfnColorOperation	TEXTEQU	\
	<?m_pfnColorOperation@ERISADecoder@@1QBQ81@AEXXZB>
ERISADecoder@@PerformOperation	TEXTEQU	\
	<?PerformOperation@ERISADecoder@@IAEXKJPAC@Z>
ERISADecoder@@ColorOperation0000	TEXTEQU	\
	<?ColorOperation0000@ERISADecoder@@IAEXXZ>
ERISADecoder@@ColorOperation0101	TEXTEQU	\
	<?ColorOperation0101@ERISADecoder@@IAEXXZ>
ERISADecoder@@ColorOperation0110	TEXTEQU	\
	<?ColorOperation0110@ERISADecoder@@IAEXXZ>
ERISADecoder@@ColorOperation0111	TEXTEQU	\
	<?ColorOperation0111@ERISADecoder@@IAEXXZ>
ERISADecoder@@ColorOperation1001	TEXTEQU	\
	<?ColorOperation1001@ERISADecoder@@IAEXXZ>
ERISADecoder@@ColorOperation1010	TEXTEQU	\
	<?ColorOperation1010@ERISADecoder@@IAEXXZ>
ERISADecoder@@ColorOperation1011	TEXTEQU	\
	<?ColorOperation1011@ERISADecoder@@IAEXXZ>
ERISADecoder@@ColorOperation1101	TEXTEQU	\
	<?ColorOperation1101@ERISADecoder@@IAEXXZ>
ERISADecoder@@ColorOperation1110	TEXTEQU	\
	<?ColorOperation1110@ERISADecoder@@IAEXXZ>
ERISADecoder@@ColorOperation1111	TEXTEQU	\
	<?ColorOperation1111@ERISADecoder@@IAEXXZ>

ERISADecoder@@ArrangeAndIQuantumize	TEXTEQU	\
	<?ArrangeAndIQuantumize@ERISADecoder@@IAEXPBC0@Z>
ERISADecoder@@MatrixIDCT8x8	TEXTEQU	\
	<?MatrixIDCT8x8@ERISADecoder@@IAEXPAM@Z>
ERISADecoder@@MatrixILOT8x8	TEXTEQU	\
	<?MatrixILOT8x8@ERISADecoder@@IAEXPAM@Z>
ERISADecoder@@BlockScaling444	TEXTEQU	\
	<?BlockScaling444@ERISADecoder@@IAEXHHK@Z>
ERISADecoder@@BlockScaling411	TEXTEQU	\
	<?BlockScaling411@ERISADecoder@@IAEXHHK@Z>
ERISADecoder@@ConvertImageYUVtoRGB	TEXTEQU	\
	<?ConvertImageYUVtoRGB@ERISADecoder@@IAEXK@Z>
ERISADecoder@@MoveImageWithVector	TEXTEQU	\
	<?MoveImageWithVector@ERISADecoder@@IAEXXZ>
ERISADecoder@@LS_RestoreGray8	TEXTEQU	\
	<?LS_RestoreGray8@ERISADecoder@@IAEXXZ>
ERISADecoder@@LS_RestoreDeltaGray8	TEXTEQU	\
	<?LS_RestoreDeltaGray8@ERISADecoder@@IAEXXZ>
ERISADecoder@@LS_RestoreRGB24	TEXTEQU	\
	<?LS_RestoreRGB24@ERISADecoder@@IAEXXZ>
ERISADecoder@@LS_RestoreDeltaRGB24	TEXTEQU	\
	<?LS_RestoreDeltaRGB24@ERISADecoder@@IAEXXZ>
ERISADecoder@@LS_RestoreRGBA32	TEXTEQU	\
	<?LS_RestoreRGBA32@ERISADecoder@@IAEXXZ>
ERISADecoder@@LS_RestoreDeltaRGBA32	TEXTEQU	\
	<?LS_RestoreDeltaRGBA32@ERISADecoder@@IAEXXZ>


EXTERNDEF	ERISADecoder@@m_pfnColorOperation:PTR NEAR32

eriEnableMMX	PROTO	NEAR32 C , fForceEnable:DWORD
eriDisableMMX	PROTO	NEAR32 C , fForceDisable:DWORD
eriInitializeFPU	PROTO	NEAR32 C

ERIBshfBuffer@@DecodeBuffer	PROTO	NEAR32 SYSCALL

ERINA_HUFFMAN_TREE@@IncreaseOccuredCount	PROTO	NEAR32 SYSCALL ,
	iEntry:SDWORD
ERINA_HUFFMAN_TREE@@Normalize		PROTO	NEAR32 SYSCALL ,
	iEntry:SDWORD
ERINA_HUFFMAN_TREE@@AddNewEntry		PROTO	NEAR32 SYSCALL ,
	nNewCode:SDWORD
ERINA_HUFFMAN_TREE@@HalfAndRebuild	PROTO	NEAR32 SYSCALL

ERISA_PROB_MODEL@@Initialize	PROTO	NEAR32 SYSCALL
ERISA_PROB_MODEL@@IncreaseSymbol	PROTO	NEAR32 SYSCALL, index:SDWORD
ERISA_PROB_MODEL@@HalfOccuredCount	PROTO	NEAR32 SYSCALL

ERISADecodeContext@@PrefetchBuffer	PROTO	NEAR32 SYSCALL
ERISADecodeContext@@GetABit		PROTO	NEAR32 SYSCALL
ERISADecodeContext@@GetNBits		PROTO	NEAR32 SYSCALL ,
	n:SDWORD
ERISADecodeContext@@GetGammaCode	PROTO	NEAR32 SYSCALL
ERISADecodeContext@@DecodeGammaCodeBytes	PROTO	NEAR32 SYSCALL ,
	ptrDst:PSBYTE, nCount:DWORD

ERISADecodeContext@@GetAHuffmanCode	PROTO	NEAR32 SYSCALL ,
	tree:PERINA_HUFFMAN_TREE
ERISADecodeContext@@GetLengthHuffman	PROTO	NEAR32 SYSCALL ,
	tree:PERINA_HUFFMAN_TREE
ERISADecodeContext@@DecodeERINACodes	PROTO	NEAR32 SYSCALL ,
	ptrDst:PSBYTE, nCount:DWORD
ERISADecodeContext@@DecodeERISACodeIndex	PROTO	NEAR32 SYSCALL,
	pModel:PERISA_PROB_MODEL
ERISADecodeContext@@DecodeERISACode	PROTO	NEAR32 SYSCALL,
	pModel:PERISA_PROB_MODEL
ERISADecodeContext@@DecodeERISACodeBytes	PROTO	NEAR32 SYSCALL,
	ptrDst:PSBYTE, nCount:DWORD

ERISADecoder@@PerformOperation		PROTO	NEAR32 SYSCALL ,
	dwOpCode:DWORD, nAllBlockLines:DWORD, ptrNextLineBuf:PTR SBYTE
ERISADecoder@@ColorOperation0000	PROTO	NEAR32 SYSCALL
ERISADecoder@@ColorOperation0101	PROTO	NEAR32 SYSCALL
ERISADecoder@@ColorOperation0110	PROTO	NEAR32 SYSCALL
ERISADecoder@@ColorOperation0111	PROTO	NEAR32 SYSCALL
ERISADecoder@@ColorOperation1001	PROTO	NEAR32 SYSCALL
ERISADecoder@@ColorOperation1010	PROTO	NEAR32 SYSCALL
ERISADecoder@@ColorOperation1011	PROTO	NEAR32 SYSCALL
ERISADecoder@@ColorOperation1101	PROTO	NEAR32 SYSCALL
ERISADecoder@@ColorOperation1110	PROTO	NEAR32 SYSCALL
ERISADecoder@@ColorOperation1111	PROTO	NEAR32 SYSCALL

ERISADecoder@@ArrangeAndIQuantumize	PROTO	NEAR32 SYSCALL ,
	ptrSrcData:PTR SBYTE, ptrCoeffient:PTR SBYTE
ERISADecoder@@MatrixIDCT8x8	PROTO	NEAR32 SYSCALL ,
	ptrVertBufLOT:PTR REAL4
ERISADecoder@@MatrixILOT8x8	PROTO	NEAR32 SYSCALL ,
	ptrVertBufLOT:PTR REAL4
ERISADecoder@@BlockScaling444	PROTO	NEAR32 SYSCALL ,
	x:SDWORD, y:SDWORD, fdwFlags:DWORD
ERISADecoder@@BlockScaling411	PROTO	NEAR32 SYSCALL ,
	x:SDWORD, y:SDWORD, fdwFlags:DWORD
ERISADecoder@@ConvertImageYUVtoRGB	PROTO	NEAR32 SYSCALL ,
	fdwFlags:DWORD
ERISADecoder@@MoveImageWithVector	PROTO	NEAR32 SYSCALL
ERISADecoder@@LS_RestoreRGB24	PROTO	NEAR32 SYSCALL
ERISADecoder@@LS_RestoreDeltaRGB24	PROTO	NEAR32 SYSCALL
ERISADecoder@@LS_RestoreRGBA32	PROTO	NEAR32 SYSCALL
ERISADecoder@@LS_RestoreDeltaRGBA32	PROTO	NEAR32 SYSCALL


;
;	@TREE@@IncreaseOccuredCount マクロ
; ----------------------------------------------------------------------------
; @TREE@@IncreaseOccuredCount <decode_flag>
; ----------------------------------------------------------------------------
; 引数；
;	EBX register	: ERINA_HUFFMAN_TREE 構造体へのポインタ
;	EAX register	: 発生頻度を増加させる指標
;	decode_flag	: 復号専用コード指定フラグ
; レジスタ；
;	EBX, EBP, ESP レジスタは保証される
; ----------------------------------------------------------------------------

@TREE@@IncreaseOccuredCount	MACRO	decode_flag:=<0>

	mov	cx, [ebx].m_hnTree[eax * 8].m_weight
	inc	cx
	mov	[ebx].m_hnTree[eax * 8].m_weight, cx

	@TREE@@Normalize	decode_flag

	.IF	[ebx].m_hnTree[ERINA_HUFFMAN_ROOT*8].m_weight \
						>= ERINA_HUFFMAN_MAX
		IFNDEF	_BORLAND_CPP
			mov	ecx, ebx
			call	ERINA_HUFFMAN_TREE@@HalfAndRebuild
		ELSE
			push	ebx
			call	ERINA_HUFFMAN_TREE@@HalfAndRebuild
			add	esp, (SIZEOF DWORD)
		ENDIF
	.ENDIF

	ENDM


;
;	@TREE@@Normalize マクロ
; ----------------------------------------------------------------------------
; @TREE@@Normalize
; ----------------------------------------------------------------------------
; 引数；
;	EAX register	: 発生頻度を増加させる指標
;	EBX register	: ERINA_HUFFMAN_TREE 構造体へのポインタ
; レジスタ；
;	EBX, EBP, ESP レジスタは保証される
; ----------------------------------------------------------------------------

@TREE@@Normalize	MACRO	decode_flag:=<0>
	LOCAL	Loop_Begin

	movzx	edx, [ebx].m_hnTree[eax * 8].m_weight
	movzx	ecx, [ebx].m_hnTree[eax * 8].m_parent
	.IF	eax < ERINA_HUFFMAN_ROOT
	jmp	Loop_Begin
	ALIGN	10H
	.REPEAT
		;;
		;; 入れ替えるエントリを検索
Loop_Begin:	.IF	[ebx].m_hnTree[eax * 8 + 8].m_weight >= dx
			;;
			;; 親の出現頻度を増加させる
			movzx	edx, [ebx].m_hnTree[ecx * 8].m_weight
			inc	edx
			mov	eax, ecx
			mov	[ebx].m_hnTree[ecx * 8].m_weight, dx
			movzx	ecx, [ebx].m_hnTree[ecx * 8].m_parent
			cmp	eax, ERINA_HUFFMAN_ROOT
			jb	Loop_Begin
			.BREAK
		.ENDIF
		mov	ecx, eax			;; ecx = iSwap
		.REPEAT
			inc	ecx
		.UNTIL	[ebx].m_hnTree[ecx * 8 + 8].m_weight >= dx
		;;
		;; 入れ替え
		mov	edx, [ebx].m_hnTree[eax * 8].m_child_code
		.IF	!(edx & ERINA_CODE_FLAG)
			mov	[ebx].m_hnTree[edx * 8].m_parent, cx
			mov	[ebx].m_hnTree[edx * 8 + 8].m_parent, cx
		IFE	decode_flag
		.ELSE
			.IF	edx != (ERINA_CODE_FLAG OR ERINA_HUFFMAN_ESCAPE)
				and	edx, 0FFH
				mov	[ebx].m_iSymLookup[edx * 4], ecx
			.ELSE
				mov	[ebx].m_iEscape, ecx
			.ENDIF
		ENDIF
		.ENDIF
		;;
		mov	edx, [ebx].m_hnTree[ecx * 8].m_child_code
		.IF	!(edx & ERINA_CODE_FLAG)
			mov	[ebx].m_hnTree[edx * 8].m_parent, ax
			mov	[ebx].m_hnTree[edx * 8 + 8].m_parent, ax
		IFE	decode_flag
		.ELSE
			.IF	edx != (ERINA_CODE_FLAG Or ERINA_HUFFMAN_ESCAPE)
				and	edx, 0FFH
				mov	[ebx].m_iSymLookup[edx * 4], eax
			.ELSE
				mov	[ebx].m_iEscape, eax
			.ENDIF
		ENDIF
		.ENDIF
		;;
		mov	esi, [ebx].m_hnTree[eax * 8].m_child_code
		mov	edi, [ebx].m_hnTree[ecx * 8].m_child_code
		mov	dx, [ebx].m_hnTree[eax * 8].m_weight
		mov	[ebx].m_hnTree[ecx * 8].m_child_code, esi
		mov	si, [ebx].m_hnTree[ecx * 8].m_weight
		mov	[ebx].m_hnTree[eax * 8].m_child_code, edi
		mov	[ebx].m_hnTree[ecx * 8].m_weight, dx
		movzx	ecx, [ebx].m_hnTree[ecx * 8].m_parent
		mov	[ebx].m_hnTree[eax * 8].m_weight, si
		;;
		;; 親の重みを再計算する
		movzx	edx, [ebx].m_hnTree[ecx * 8].m_weight
		mov	eax, ecx
		inc	edx
		mov	[ebx].m_hnTree[ecx * 8].m_weight, dx
		movzx	ecx, [ebx].m_hnTree[ecx * 8].m_parent
	.UNTIL	eax >= ERINA_HUFFMAN_ROOT
	.ENDIF

	ENDM

