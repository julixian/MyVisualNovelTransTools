
; ----------------------------------------------------------------------------
;                          Entis Graphic Library
; ----------------------------------------------------------------------------
;   Copyright (c) 2002-2004 Leshade Entis, Entis-soft. Al rights reserved.
; ----------------------------------------------------------------------------


IFNDEF	__EGL_INC__
__EGL_INC__	EQU	1

INCLUDE	esl.inc

IFNDEF	NULL
NULL	EQU	0
ENDIF

IFNDEF	PVOID
PVOID	TYPEDEF	PTR
ENDIF

IFNDEF	HDC
HDC	TYPEDEF	PTR
ENDIF

IFNDEF	HBITMAP
HBITMAP	TYPEDEF	PTR
ENDIF


;
;	パレット共用体
; ----------------------------------------------------------------------------

EGL_PALETTE	UNION
dwPixelCode	DWORD	?		; パレット番号／ピクセルコード
rZOrder32	REAL4	?		; Z 値
STRUCT	rgb				; RGB24 フォーマット
	Blue		BYTE	?
	Green		BYTE	?
	Red		BYTE	?
	Reserved	BYTE	?
ENDS
STRUCT	rgba				; RGBA32 フォーマット
	Blue		BYTE	?
	Green		BYTE	?
	Red		BYTE	?
	Alpha		BYTE	?
ENDS
STRUCT	yuv				; YUV24 フォーマット
	Y		BYTE	?
	U		BYTE	?
	V		BYTE	?
	Reserved	BYTE	?
ENDS
STRUCT	hsb				; HSB24 フォーマット
	Brightness	BYTE	?
	Saturation	BYTE	?
	Hue		BYTE	?
	Reserved	BYTE	?
ENDS
EGL_PALETTE	ENDS
PEGL_PALETTE	TYPEDEF	PTR EGL_PALETTE
PCEGL_PALETTE	TYPEDEF	PTR EGL_PALETTE


;
;	画像情報構造体
; ----------------------------------------------------------------------------

EGL_IMAGE_INFO	STRUCT
dwInfoSize	DWORD		?
fdwFormatType	DWORD		?
ptrOffsetPixel	DWORD		?
ptrImageArray	PVOID		?
pPaletteEntries	PEGL_PALETTE	?
dwPaletteCount	DWORD		?
dwImageWidth	DWORD		?
dwImageHeight	DWORD		?
dwBitsPerPixel	DWORD		?
dwBytesPerLine	SDWORD		?
dwSizeOfImage	SDWORD		?
dwClippedPixel	DWORD		?
EGL_IMAGE_INFO	ENDS

PEGL_IMAGE_INFO		TYPEDEF	PTR EGL_IMAGE_INFO
PCEGL_IMAGE_INFO	TYPEDEF	PTR EGL_IMAGE_INFO ;

EIF_RGB_BITMAP		EQU	00000001H
EIF_RGBA_BITMAP		EQU	04000001H
EIF_GRAY_BITMAP		EQU	00000002H
EIF_YUV_BITMAP		EQU	00000004H
EIF_HSB_BITMAP		EQU	00000006H
EIF_Z_BUFFER_R4		EQU	00002005H
EIF_TYPE_MASK		EQU	00FFFFFFH
EIF_WITH_PALETTE	EQU	01000000H
EIF_WITH_CLIPPING	EQU	02000000H
EIF_WITH_ALPHA		EQU	04000000H

EGL_IMAGE_BUFF	STRUCT
dwInfoSize	DWORD		?
fdwFormatType	DWORD		?
ptrOffsetPixel	DWORD		?
ptrImageArray	PVOID		?
pPaletteEntries	PEGL_PALETTE	?
dwPaletteCount	DWORD		?
dwImageWidth	DWORD		?
dwImageHeight	DWORD		?
dwBitsPerPixel	DWORD		?
dwBytesPerLine	SDWORD		?
dwSizeOfImage	SDWORD		?
dwClippedPixel	DWORD		?
dwFlags		DWORD		?
hDC		HDC		?
hBitmap		HBITMAP		?
pLineAddrEntry	PVOID		?
dwWidthMask	DWORD		?
dwHeightMask	DWORD		?
EGL_IMAGE_BUFF	ENDS


;
;	座標・サイズ・矩形
; ----------------------------------------------------------------------------

EGL_POINT	STRUCT
x		SDWORD	?
y		SDWORD	?
EGL_POINT	ENDS
PEGL_POINT	TYPEDEF	PTR EGL_POINT
PCEGL_POINT	TYPEDEF	PTR EGL_POINT

EGL_SIZE	STRUCT
w		SDWORD	?
h		SDWORD	?
EGL_SIZE	ENDS
PEGL_SIZE	TYPEDEF	PTR EGL_SIZE
PCEGL_SIZE	TYPEDEF	PTR EGL_SIZE

EGL_RECT	STRUCT
left		SDWORD	?
top		SDWORD	?
right		SDWORD	?
bottom		SDWORD	?
EGL_RECT	ENDS
PEGL_RECT	TYPEDEF	PTR EGL_RECT
PCEGL_RECT	TYPEDEF	PTR EGL_RECT

EGL_IMAGE_RECT	STRUCT
x		SDWORD	?
y		SDWORD	?
w		SDWORD	?
h		SDWORD	?
EGL_IMAGE_RECT	ENDS
PEGL_IMAGE_RECT		TYPEDEF	PTR EGL_IMAGE_RECT
PCEGL_IMAGE_RECT	TYPEDEF	PTR EGL_IMAGE_RECT

EGL_IMAGE_AXES	STRUCT
STRUCT	xAxis
	x	REAL4	?
	y	REAL4	?
ENDS
STRUCT	yAxis
	x	REAL4	?
	y	REAL4	?
ENDS
EGL_IMAGE_AXES	ENDS
PEGL_IMAGE_AXES		TYPEDEF	PTR EGL_IMAGE_AXES
PCEGL_IMAGE_AXES	TYPEDEF	PTR EGL_IMAGE_AXES


;
;	グローバル変数
; ----------------------------------------------------------------------------

EXTERNDEF	C EGL_hImageHeap:HESLHEAP


;
;	画像バッファ管理
; ----------------------------------------------------------------------------

eglCreateImageBuffer	PROTO	NEAR32 C,
	fdwFormat:DWORD, dwWidth:DWORD, dwHeight:DWORD,
	dwBitsPerPixel:DWORD, dwFlags:DWORD
eglCreateTextureInfo	PROTO	NEAR32 C,
	pImageInf:PCEGL_IMAGE_INFO, pClipRect:PCEGL_RECT, dwFlags:DWORD
eglDuplicateImageBuffer	PROTO	NEAR32 C,
	pImageInf:PCEGL_IMAGE_INFO, dwFlags:DWORD
eglAddImageBufferRef	PROTO	NEAR32 C,
	pImageInf:PEGL_IMAGE_INFO
eglDeleteImageBuffer	PROTO	NEAR32 C,
	pImageInf:PEGL_IMAGE_INFO
eglDrawToDC		PROTO	NEAR32 C,
	hDstDC:HDC, pImageInf:PEGL_IMAGE_INFO,
	nPosX:SDWORD, nPosY:SDWORD,
	pSizeToDraw:PCEGL_SIZE, pViewRect:PCEGL_RECT
eglGetDC		PROTO	NEAR32 C,
	pImageInf:PEGL_IMAGE_INFO
eglFillImage		PROTO	NEAR32 C,
	pImageInf:PEGL_IMAGE_INFO, colorFill:EGL_PALETTE

EGL_DRAW_RADIAN		EQU	0001H

EGL_IMAGE_HAS_DC	EQU	0001H
EGL_IMAGE_NO_DUP	EQU	0010H


;
;	画像情報
; ----------------------------------------------------------------------------

eglGetClippedImageInfo		PROTO	NEAR32 C,
	pClippedImage:PEGL_IMAGE_INFO,
	pOriginalImage:PCEGL_IMAGE_INFO,
	pClippingRect:PCEGL_IMAGE_RECT
eglGetOverlappedRectangle	PROTO	NEAR32 C,
	pImageRect:PEGL_IMAGE_RECT,
	pDstViewRect:PCEGL_RECT, pSrcViewRect:PCEGL_RECT,
	pSrcViewOffset:PCEGL_POINT, pSrcViewMaxSize:PCEGL_SIZE
eglGetRevolvedAxes		PROTO	NEAR32 C,
	pImageAxes:PEGL_IMAGE_AXES,
	pBasePosition:PCEGL_POINT, pCenterOffset:PCEGL_POINT,
	rHorzRate:REAL4, rVertRate:REAL4,
	rRevAngle:REAL4, rCrossAngle:REAL4, nRadianFlag:DWORD
eglReverseVertically		PROTO	NEAR32 C,
	pImageInf:PEGL_IMAGE_INFO
eglGetPixel			PROTO	NEAR32 C,
	pImageInf:PCEGL_IMAGE_INFO, nPosX:SDWORD, nPosY:SDWORD
eglSetPixel			PROTO	NEAR32 C,
	pImageInf:PEGL_IMAGE_INFO,
	nPosX:SDWORD, nPosY:SDWORD, colorPixel:EGL_PALETTE


;
;	カラーコード変換
; ----------------------------------------------------------------------------

eglRGBtoYUV	PROTO	NEAR32 C, pColor:PTR EGL_PALETTE
eglYUVtoRGB	PROTO	NEAR32 C, pColor:PTR EGL_PALETTE
eglRGBtoHSB	PROTO	NEAR32 C, pColor:PTR EGL_PALETTE
eglHSBtoRGB	PROTO	NEAR32 C, pColor:PTR EGL_PALETTE
eglBlendRGBA	PROTO	NEAR32 C, rgba1:EGL_PALETTE, rgba2:EGL_PALETTE


;
;	フィルタリング関数
; ----------------------------------------------------------------------------

eglConvertFormat	PROTO	NEAR32 C,
	pDstImage:PEGL_IMAGE_INFO, pSrcImage:PCEGL_IMAGE_INFO, dwFlags:DWORD
eglCalculateToneTable	PROTO	NEAR32 C,
	pToneBuf:PTR, nTone:SDWORD, nFlag:DWORD
eglApplyToneTable	PROTO	NEAR32 C,
	pDstImage:PEGL_IMAGE_INFO, pSrcImage:PCEGL_IMAGE_INFO,
	pBlueTone:PTR, pGreenTone:PTR, pRedTone:PTR, pAlphaTone:PTR
eglSetColorTone		PROTO	NEAR32 C,
	pDstImage:PEGL_IMAGE_INFO, pSrcImage:PCEGL_IMAGE_INFO,
	nBlueTone:SDWORD, nGreenTone:SDWORD, nRedTone:SDWORD, nAlphaTone:SDWORD
eglEnlargeDouble	PROTO	NEAR32 C,
	pDstImage:PEGL_IMAGE_INFO,
	pSrcImage:PCEGL_IMAGE_INFO, dwFlags:DWORD
eglBlendAlphaChannel	PROTO	NEAR32 C,
	pRGBA32:PEGL_IMAGE_INFO, pSrcRGB:PCEGL_IMAGE_INFO,
	pSrcAlpha:PCEGL_IMAGE_INFO, dwFlags:DWORD,
	nAlphaBase:SDWORD, nCoefficient:DWORD
eglUnpackAlphaChannel	PROTO	NEAR32 C,
	pDstRGB:PEGL_IMAGE_INFO, pDstAlpha:PEGL_IMAGE_INFO,
	pSrcRGBA32:PCEGL_IMAGE_INFO, dwFlags:DWORD

EGL_TONE_BRIGHTNESS	EQU	0000H
EGL_TONE_INVERSION	EQU	0001H
EGL_TONE_LIGHT		EQU	0002H

EGL_BAC_MULTIPLY	EQU	0001H
EGL_BAC_ADD_ALPHA	EQU	0002H
EGL_BAC_MULTIPLY_ALPHA	EQU	0004H


;
;	シェーディング色情報
; ----------------------------------------------------------------------------
E3D_COLOR	STRUCT
rgbMul		EGL_PALETTE	{ }
rgbAdd		EGL_PALETTE	{ }
E3D_COLOR	ENDS
PE3D_COLOR	TYPEDEF	PTR E3D_COLOR
PCE3D_COLOR	TYPEDEF	PTR E3D_COLOR

;
;	リージョン
; ----------------------------------------------------------------------------

E3D_POLY_LINE_REGION	STRUCT
nLeft			SDWORD		?
dwReserved1		DWORD		?
nRight			SDWORD		?
dwReserved2		DWORD		?
rgbaLeft		E3D_COLOR	{ }
rgbaRight		E3D_COLOR	{ }
E3D_POLY_LINE_REGION	ENDS
PE3D_POLY_LINE_REGION	TYPEDEF	PTR E3D_POLY_LINE_REGION

E3D_POLYGON_REGION	STRUCT
nTopLine		SDWORD			?
nBottomLine		SDWORD			?
plrLineRgn		E3D_POLY_LINE_REGION	1 DUP({})
E3D_POLYGON_REGION	ENDS
PE3D_POLYGON_REGION	TYPEDEF	PTR E3D_POLYGON_REGION


;
;	画像描画
; ----------------------------------------------------------------------------

EGL_DRAW_DEST	STRUCT
pDstImage	PEGL_IMAGE_INFO		?
rectDstClip	EGL_RECT		{}
pZBuffer	PEGL_IMAGE_INFO		?
EGL_DRAW_DEST	ENDS

EGL_DRAW_PARAM	STRUCT
dwFlags		DWORD			?
ptBasePos	EGL_POINT		{}
pSrcImage	PEGL_IMAGE_INFO		?
pViewRect	PCEGL_RECT		?
rgbDimColor	EGL_PALETTE		{}
rgbLightColor	EGL_PALETTE		{}
nTransparency	DWORD			?
rZOrder		REAL4			?
pImageAxes	PCEGL_IMAGE_AXES	?
rgbColorParam1	EGL_PALETTE		{}
EGL_DRAW_PARAM	ENDS

EGL_DRAW_BLEND_ALPHA	EQU	0001H
EGL_DRAW_GLOW_LIGHT	EQU	0002H
EGL_WITH_Z_ORDER	EQU	0004H
EGL_WITH_AXES		EQU	0008H
EGL_SMOOTH_STRETCH	EQU	0010H
EGL_UNSMOOTH_STRETCH	EQU	0020H
EGL_FIXED_POSITION	EQU	0040H

EGL_APPLY_C_ADD		EQU	00800000H
EGL_APPLY_C_MUL		EQU	00820000H
EGL_APPLY_A_MUL		EQU	00880000H
EGL_APPLY_C_MASK	EQU	00890000H
EGL_DRAW_F_ADD		EQU	80000000H
EGL_DRAW_F_SUB		EQU	81000000H
EGL_DRAW_F_MUL		EQU	82000000H
EGL_DRAW_F_DIV		EQU	83000000H
EGL_DRAW_A_MOVE		EQU	88000000H
EGL_DRAW_A_MUL		EQU	89000000H

EGL_FILL_INVERSION	EQU	00000100H

HEGL_DRAW_IMAGE	TYPEDEF	PTR
PTR_NEAR32	TYPEDEF	PTR NEAR32

DrawImage@Release		TYPEDEF	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
DrawImage@Initialize		TYPEDEF	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE, pDstImage:PEGL_IMAGE_INFO,
	pClipRect:PCEGL_RECT, pZBuffer:PEGL_IMAGE_INFO
DrawImage@GetDestination	TYPEDEF	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE, pDrawDest:PTR EGL_DRAW_DEST
DrawImage@GetFunctionFlags	TYPEDEF	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
DrawImage@SetFunctionFlags	TYPEDEF	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE, dwFlags:DWORD
DrawImage@PrepareDraw		TYPEDEF	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE, pDrawParam:PTR EGL_DRAW_PARAM
DrawImage@DrawImage		TYPEDEF	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
DrawImage@PrepareLine		TYPEDEF	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE,
	x1:SDWORD, y1:SDWORD, x2:SDWORD, y2:SDWORD,
	colorDraw:EGL_PALETTE, nTransparency:DWORD, dwFlags:DWORD
DrawImage@PrepareFillRect	TYPEDEF	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE,
	pDrawRect:PCEGL_RECT, colorDraw:EGL_PALETTE,
	nTransparency:DWORD, dwFlags:DWORD
DrawImage@PrepareFillEllipse	TYPEDEF	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE,
	pCenter:PCEGL_POINT, pRadius:PCEGL_SIZE,
	colorDraw:EGL_PALETTE, nTransparency:DWORD, dwFlags:DWORD
DrawImage@PrepareFillPolygon	TYPEDEF	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE,
	pVertexes:PCEGL_POINT, nCount:DWORD,
	colorDraw:EGL_PALETTE, nTransparency:DWORD, dwFlags:DWORD
DrawImage@FillRegion		TYPEDEF	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
DrawImage@DrawRegion		TYPEDEF	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE

PDrawImage@Release		TYPEDEF	PTR DrawImage@Release
PDrawImage@Initialize		TYPEDEF	PTR DrawImage@Initialize
PDrawImage@GetDestination	TYPEDEF	PTR DrawImage@GetDestination
PDrawImage@GetFunctionFlags	TYPEDEF	PTR DrawImage@GetFunctionFlags
PDrawImage@SetFunctionFlags	TYPEDEF	PTR DrawImage@SetFunctionFlags
PDrawImage@PrepareDraw		TYPEDEF	PTR DrawImage@PrepareDraw
PDrawImage@DrawImage		TYPEDEF	PTR DrawImage@DrawImage
PDrawImage@PrepareLine		TYPEDEF	PTR DrawImage@PrepareLine
PDrawImage@PrepareFillRect	TYPEDEF	PTR DrawImage@PrepareFillRect
PDrawImage@PrepareFillEllipse	TYPEDEF	PTR DrawImage@PrepareFillEllipse
PDrawImage@PrepareFillPolygon	TYPEDEF	PTR DrawImage@PrepareFillPolygon
PDrawImage@FillRegion		TYPEDEF	PTR DrawImage@FillRegion
PDrawImage@DrawRegion		TYPEDEF	PTR DrawImage@DrawRegion

EGL_DRAW_IMAGE		STRUCT
pfnRelease		PDrawImage@Release		?
pfnInitialize		PDrawImage@Initialize		?
pfnGetDestination	PDrawImage@GetDestination	?
pfnGetFunctionFlags	PDrawImage@GetFunctionFlags	?
pfnSetFunctionFlags	PDrawImage@SetFunctionFlags	?
pfnPrepareDraw		PDrawImage@PrepareDraw		?
pfnDrawImage		PDrawImage@DrawImage		?
			DWORD				?
pfnPrepareLine		PDrawImage@PrepareLine		?
pfnPrepareFillRect	PDrawImage@PrepareFillRect	?
pfnPrepareFillEllipse	PDrawImage@PrepareFillEllipse	?
pfnPrepareFillPolygon	PDrawImage@PrepareFillPolygon	?
pfnFillRegion		PDrawImage@FillRegion		?
pfnDrawRegion		PDrawImage@DrawRegion		?
EGL_DRAW_IMAGE		ENDS

eglCreateDrawImage		PROTO	NEAR32 C


;
;	画像描画用バッファ
; ----------------------------------------------------------------------------

EGL_DRAW_IMAGE_BUF	STRUCT

pfnRelease		PDrawImage@Release		?
pfnInitialize		PDrawImage@Initialize		?
pfnGetDestination	PDrawImage@GetDestination	?
pfnGetFunctionFlags	PDrawImage@GetFunctionFlags	?
pfnSetFunctionFlags	PDrawImage@SetFunctionFlags	?
pfnPrepareDraw		PDrawImage@PrepareDraw		?
pfnDrawImage		PDrawImage@DrawImage		?
			DWORD				?
pfnPrepareLine		PDrawImage@PrepareLine		?
pfnPrepareFillRect	PDrawImage@PrepareFillRect	?
pfnPrepareFillEllipse	PDrawImage@PrepareFillEllipse	?
pfnPrepareFillPolygon	PDrawImage@PrepareFillPolygon	?
pfnFillRegion		PDrawImage@FillRegion		?
pfnDrawRegion		PDrawImage@DrawRegion		?

pfnDrawSampling		PTR_NEAR32			?
pfnDrawApplying		PTR_NEAR32			?
pfnDrawRendering	PTR_NEAR32			?
pfnDrawRegionLine	PTR_NEAR32			?
dwDrawFlags		DWORD				?

ALIGN	10H
rgbaColor	EGL_PALETTE		100H DUP({})
nBlueTone	BYTE			100H DUP(?)
nGreenTone	BYTE			100H DUP(?)
nRedTone	BYTE			100H DUP(?)
dwTemp		DWORD			10H DUP(?)

ptBasePos	EGL_POINT		{ }
ptLineBase	EGL_POINT		{ }
ptDltScanX	EGL_POINT		{ }
ptDltScanY	EGL_POINT		{ }
colorDraw	EGL_PALETTE		{ }

tmxSizeMask	WORD			4 DUP( ? )	; MMX 用画像サイズ飽和マスク（x10000H fixed）
tmxMulAddr	WORD			4 DUP( ? )	; MMX 用画像アドレス算出用
tmxSrcImageAddr	DWORD			2 DUP( ? )

hHeap		HESLHEAP		?
dwSaveRegEBP	DWORD			?
pDstImage	PEGL_IMAGE_INFO		?
rectClip	EGL_RECT		{ }
pZBuffer	PEGL_IMAGE_INFO		?

pRegion		PE3D_POLYGON_REGION	?
pSrcLineAddr	PVOID			?, ?

dwFlags		DWORD			?
dstimg		EGL_IMAGE_INFO		{ }
zbuf		EGL_IMAGE_INFO		{ }
srcimg		EGL_IMAGE_INFO		{ }
nTrans		DWORD			?
rZOrder		REAL4			?

xPos		SDWORD			?
yPos		SDWORD			?
nLeftWidth	DWORD			?
nLeftHeight	DWORD			?
nBytesPerPixel	DWORD			?, ?
ptrDstLine	PVOID			?
ptrZBufLine	PVOID			?
ptrSrcLine	PVOID			?
ptrRegionLine	PE3D_POLY_LINE_REGION	?
pDrawLineBuf	PVOID			?, ?

EGL_DRAW_IMAGE_BUF	ENDS

eglDrawImage@Release			PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@Initialize			PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE, pDstImage:PEGL_IMAGE_INFO,
	pClipRect:PCEGL_RECT, pZBuffer:PEGL_IMAGE_INFO
eglDrawImage@GetDestination		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE, pDrawDest:PTR EGL_DRAW_DEST
eglDrawImage@GetFunctionFlags		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@SetFunctionFlags		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE, dwFlags:DWORD
eglDrawImage@PrepareDraw		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE, pDrawParam:PTR EGL_DRAW_PARAM
eglDrawImage@DrawImage_NotSupported	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_NoDraw		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_8to8		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_8to8_Clip	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_8to24		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_8to24_Trans	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_8to24_Clip	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_8to24_CTrans	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_GlowAdd		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_GlowAdd_SSE	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_Glow		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_Glow_SSE		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_GlowBlend	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_24to24		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_24to24_SSE	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_24to24_Trans	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGB32_Trans_SSE	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGBA		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGBA_SSE		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGBA_Trans	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGBA_Trans_SSE	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGBA_Blend	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGBA_Blend_SSE	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGBA_BTrans	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGBA_BTrans_SSE	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGB_X		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGB_XTrans	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGB_XTZBuf	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGB_X_SSE	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGB_XTrans_SSE	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGB_XZBuf_SSE	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGB_XTZBuf_SSE	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGBA_X		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGBA_XTZBuf	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGBA_X_SSE	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGBA_XTrans_SSE	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGBA_XZBuf_SSE	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_RGBA_XTZBuf_SSE	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE

eglDrawImage@DrawImage_ConvertFormat	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_DirectRGBA32	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_Normal		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawImage_Transform	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE

EXTERNDEF	C eglDrawImage@Sample_Index8:NEAR32
EXTERNDEF	C eglDrawImage@Sample_Gray8:NEAR32
EXTERNDEF	C eglDrawImage@Sample_RGB16:NEAR32
EXTERNDEF	C eglDrawImage@Sample_RGB24:NEAR32
EXTERNDEF	C eglDrawImage@Sample_RGB32:NEAR32

EXTERNDEF	C eglDrawImage@Sample_Index8_X:NEAR32
EXTERNDEF	C eglDrawImage@Sample_Gray8_X:NEAR32
EXTERNDEF	C eglDrawImage@Sample_RGB16_X:NEAR32
EXTERNDEF	C eglDrawImage@Sample_RGB24_X:NEAR32
EXTERNDEF	C eglDrawImage@Sample_RGB32_X:NEAR32
EXTERNDEF	C eglDrawImage@Sample_RGB32_X_MMX:NEAR32
EXTERNDEF	C eglDrawImage@Sample_RGB32_XS_MMX:NEAR32
EXTERNDEF	C eglDrawImage@Sample_RGB32_XS_SSE:NEAR32
EXTERNDEF	C eglDrawImage@Sample_RGB32_XS_SSE2:NEAR32

EXTERNDEF	C eglDrawImage@Render_Gray8:NEAR32
EXTERNDEF	C eglDrawImage@Render_RGB24:NEAR32
EXTERNDEF	C eglDrawImage@Render_RGB24_B:NEAR32
EXTERNDEF	C eglDrawImage@Render_RGB24_BT:NEAR32
EXTERNDEF	C eglDrawImage@Render_RGB24_BTZ:NEAR32
EXTERNDEF	C eglDrawImage@Render_RGB32:NEAR32
EXTERNDEF	C eglDrawImage@Render_RGBA32_B:NEAR32
EXTERNDEF	C eglDrawImage@Render_RGBA32_BT:NEAR32
EXTERNDEF	C eglDrawImage@Render_RGBA32_BTZ:NEAR32
EXTERNDEF	C eglDrawImage@Render_RGB32_SSE:NEAR32
EXTERNDEF	C eglDrawImage@Render_RGBA32_B_MMX:NEAR32
EXTERNDEF	C eglDrawImage@Render_RGBA32_B_SSE:NEAR32
EXTERNDEF	C eglDrawImage@Render_RGBA32_BT_SSE:NEAR32
EXTERNDEF	C eglDrawImage@Render_RGBA32_BZ_SSE:NEAR32
EXTERNDEF	C eglDrawImage@Render_RGBA32_BTZ_SSE:NEAR32

EXTERNDEF	C eglDrawImage@Apply_nothing:NEAR32
EXTERNDEF	C eglDrawImage@Apply_Transparency:NEAR32
EXTERNDEF	C eglDrawImage@Apply_Transparency_MMX:NEAR32
EXTERNDEF	C eglDrawImage@Apply_ITransparency:NEAR32
EXTERNDEF	C eglDrawImage@Apply_ITransparency_MMX:NEAR32
EXTERNDEF	C eglDrawImage@Apply_MulColor:NEAR32
EXTERNDEF	C eglDrawImage@Apply_MulColor_MMX:NEAR32
EXTERNDEF	C eglDrawImage@Apply_AddColor:NEAR32
EXTERNDEF	C eglDrawImage@Apply_AddColor_MMX:NEAR32
EXTERNDEF	C eglDrawImage@Product_Alpha:NEAR32
EXTERNDEF	C eglDrawImage@Product_Alpha_MMX:NEAR32
EXTERNDEF	C eglDrawImage@Apply_ColorMask:NEAR32
EXTERNDEF	C eglDrawImage@Apply_ColorMask_MMX:NEAR32

EXTERNDEF	C eglDrawImage@Render_AddColor:NEAR32
EXTERNDEF	C eglDrawImage@Render_AddColor_MMX:NEAR32
EXTERNDEF	C eglDrawImage@Render_SubColor:NEAR32
EXTERNDEF	C eglDrawImage@Render_SubColor_MMX:NEAR32
EXTERNDEF	C eglDrawImage@Render_MulColor:NEAR32
EXTERNDEF	C eglDrawImage@Render_MulColor_MMX:NEAR32
EXTERNDEF	C eglDrawImage@Render_DivColor:NEAR32
EXTERNDEF	C eglDrawImage@Render_DivColor_MMX:NEAR32
EXTERNDEF	C eglDrawImage@Render_MoveColor:NEAR32
EXTERNDEF	C eglDrawImage@Render_MoveColor_MMX:NEAR32
EXTERNDEF	C eglDrawImage@Render_AMulColor:NEAR32
EXTERNDEF	C eglDrawImage@Render_AMulColor_MMX:NEAR32


eglDrawImage@PrepareLine		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE,
	x1:SDWORD, y1:SDWORD, x2:SDWORD, y2:SDWORD,
	colorDraw:EGL_PALETTE, nTransparency:DWORD, dwFlags:DWORD
eglDrawImage@PrepareFillRect		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE,
	pDrawRect:PCEGL_RECT, colorDraw:EGL_PALETTE,
	nTransparency:DWORD, dwFlags:DWORD
eglDrawImage@PrepareFillEllipse		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE,
	pCenter:PCEGL_POINT, pRadius:PCEGL_SIZE,
	colorDraw:EGL_PALETTE, nTransparency:DWORD, dwFlags:DWORD
eglDrawImage@PrepareFillPolygon		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE,
	pVertexes:PCEGL_POINT, nCount:DWORD,
	colorDraw:EGL_PALETTE, nTransparency:DWORD, dwFlags:DWORD
eglDrawImage@FillRegion_Error		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@FillRegion_NoDraw		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@FillRegion_486		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@FillRegion_Blend_486	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@FillRegion_Blend_MMX	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@FillRegion_Inversion_486	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawRegion_486		PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawRegion_Blend_486	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@DrawRegion_Inversion_486	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE



;
;	n 次元ベクトル
; ----------------------------------------------------------------------------

E3D_VECTOR_2D	STRUCT
x		REAL4	?
y		REAL4	?
E3D_VECTOR_2D	ENDS
PE3D_VECTOR_2D	TYPEDEF	PTR E3D_VECTOR_2D
PCE3D_VECTOR_2D	TYPEDEF	PTR E3D_VECTOR_2D

E3D_VECTOR	STRUCT
x		REAL4	?
y		REAL4	?
z		REAL4	?
E3D_VECTOR	ENDS
PE3D_VECTOR	TYPEDEF	PTR E3D_VECTOR
PCE3D_VECTOR	TYPEDEF	PTR E3D_VECTOR

E3D_VECTOR4	STRUCT
x		REAL4	?
y		REAL4	?
z		REAL4	?
d		REAL4	?
E3D_VECTOR4	ENDS
PE3D_VECTOR4	TYPEDEF	PTR E3D_VECTOR4
PCE3D_VECTOR4	TYPEDEF	PTR E3D_VECTOR4


;
;	3x3 変換行列
;	（アラインのため 4x4 ）
; ----------------------------------------------------------------------------

E3D_REV_MATRIX	STRUCT
matrix		REAL4	4 DUP( 4 DUP(?) )
E3D_REV_MATRIX	ENDS
PE3D_REV_MATRIX		TYPEDEF	PTR E3D_REV_MATRIX
PCE3D_REV_MATRIX	TYPEDEF	PTR E3D_REV_MATRIX


;
;	ポリゴンの表面属性
; ----------------------------------------------------------------------------

E3D_ENVIRONMENT_MAPPING	STRUCT
pRoundImage		PEGL_IMAGE_INFO	 ?	; 球形画像
pRoundLuminous		PEGL_IMAGE_INFO	 ?
pUpperImage		PEGL_IMAGE_INFO	 ?	; 上半球
pUpperLuminous		PEGL_IMAGE_INFO	 ?
pUnderImage		PEGL_IMAGE_INFO	 ?	; 下半球
pUnderLuminous		PEGL_IMAGE_INFO	 ?
E3D_ENVIRONMENT_MAPPING	ENDS

E3D_TEXTURE_MAPPING	STRUCT
pTextureImage		PEGL_IMAGE_INFO	?	; テクスチャ画像
pLuminousImage		PEGL_IMAGE_INFO	?	; 発光テクスチャ
rThresholdZ		REAL4		?	; 縮小画像に切り替えるための閾値
nSmallScale		DWORD		?	; 縮小画像のスケール (1/2^n)
pSmallImage		PEGL_IMAGE_INFO	?	; 縮小テクスチャ画像
pSmallLuminous		PEGL_IMAGE_INFO	?	; 縮小発光テクスチャ
E3D_TEXTURE_MAPPING	ENDS

E3D_SURFACE_ATTRIBUTE	STRUCT
dwShadingFlags		DWORD		?	; シェーディングフラグ
			DWORD		?
rgbaColor		E3D_COLOR	{}	; 基本色
UNION
	txmap	E3D_TEXTURE_MAPPING	{}	; テクスチャマッピング
	envmap	E3D_ENVIRONMENT_MAPPING	{}	; 環境マッピング
ENDS
nTextureApply		DWORD		?	; テクスチャ適用度
nLiminousApply		DWORD		?	; 発光テクスチャ適用度
nAmbient		SDWORD		?	; 環境光
nDiffusion		SDWORD		?	; 拡散光
nSpecular		SDWORD		?	; 反射光
nSpecularSize		SDWORD		?
nTransparency		SDWORD		?	; 透明度
nDeepness		SDWORD		?	; 透明深度係数
E3D_SURFACE_ATTRIBUTE	ENDS
PE3D_SURFACE_ATTRIBUTE	TYPEDEF	PTR E3D_SURFACE_ATTRIBUTE
PCE3D_SURFACE_ATTRIBUTE	TYPEDEF	PTR E3D_SURFACE_ATTRIBUTE

E3DSAF_NO_SHADING		EQU	00000000H	; シェーディング無し
E3DSAF_FLAT_SHADE		EQU	00000001H	; フラットシェーディング（未使用）
E3DSAF_GOURAUD_SHADE		EQU	00000002H	; グーローシェーディング
E3DSAF_PHONG_SHADE		EQU	00000003H	; フォンシェーディング（未使用）
E3DSAF_VERTEX_COLOR		EQU	00000004H	; 頂点色
E3DSAF_SHADING_MASK		EQU	000000FFH
E3DSAF_TEXTURE_TILING		EQU	00000100H	; テクスチャをタイリング
E3DSAF_TEXTURE_TRIM		EQU	00000200H	; トリミングテクスチャ
E3DSAF_TEXTURE_SMOOTH		EQU	00000400H	; テクスチャ補完拡大
E3DSAF_TEXTURE_MAPPING		EQU	00001000H	; テクスチャマッピング
E3DSAF_ENVIRONMENT_MAP		EQU	00002000H	; 環境マッピング
E3DSAF_GENVIRONMENT_MAP		EQU	00004000H	; グローバル環境マッピング
E3DSAF_SINGLE_SIDE_PLANE	EQU	00010000H	; 片面ポリゴン
E3DSAF_NO_ZBUFFER		EQU	00020000H	; ｚ比較を行わないで描画


;
;	光源
; ----------------------------------------------------------------------------

E3D_LIGHT_ENTRY	STRUCT
dwLightType	DWORD		?
rgbColor	EGL_PALETTE	{}
rBrightness	REAL4		?
dwReserved	DWORD		?
vecLight	E3D_VECTOR4	{}
E3D_LIGHT_ENTRY	ENDS
PE3D_LIGHT_ENTRY	TYPEDEF	PTR E3D_LIGHT_ENTRY
PCE3D_LIGHT_ENTRY	TYPEDEF	PTR E3D_LIGHT_ENTRY

E3D_FOG_LIGHT		EQU	00000000H	; 擬似フォッグ
E3D_AMBIENT_LIGHT	EQU	00000001H	; 環境光
E3D_VECTOR_LIGHT	EQU	00000002H	; 無限遠光源
E3D_POINT_LIGHT		EQU	00000004H	; 点光源
E3D_SENSITIVITY_LIGHT	EQU	00000010H	; 光感度


;
;	テクスチャマッピング用パラメータ
; ----------------------------------------------------------------------------

E3D_TEXTURE_MAPINFO	STRUCT
pTextureImage		PEGL_IMAGE_INFO	?	; 通常テクスチャ
pLuminousImage		PEGL_IMAGE_INFO	?	; 発光テクスチャ
nTextureApply		DWORD		?	; テクスチャ適用度
nLiminousApply		DWORD		?	; 発光テクスチャ適用度
vOriginPos		E3D_VECTOR4	{}	; マッピング用パラメータ
vAxisX			E3D_VECTOR4	{}
vAxisY			E3D_VECTOR4	{}
rFogDeepness		REAL4		?	; 擬似フォッグ用
rgbFogColor		EGL_PALETTE	{}
E3D_TEXTURE_MAPINFO	ENDS


;
;	ポリゴンプリミティブ情報
; ----------------------------------------------------------------------------

E3D_PRIMITIVE_VERTEX	STRUCT
vertex			PE3D_VECTOR4	?	; 頂点座標
normal			PE3D_VECTOR4	?	; 法線（スムースポリゴンのみ）
UNION
	uv_map		E3D_VECTOR_2D	{}	; UV 座標（テクスチャポリゴンのみ）
	color		E3D_COLOR	{}	; 頂点色（頂点色ポリゴンのみ）
ENDS
E3D_PRIMITIVE_VERTEX	ENDS

E3D_PRIMITIVE_INFINITE_PLANE	STRUCT
vertex				PE3D_VECTOR4	?	; 平面基準点
xAxis				PE3D_VECTOR4	?	; テクスチャｘ軸
yAxis				PE3D_VECTOR4	?	; テクスチャｙ軸
rFogDeepness			REAL4		?	; 擬似フォッグ用
rgbFogColor			EGL_PALETTE	{}
E3D_PRIMITIVE_INFINITE_PLANE	ENDS

E3D_PRIMITIVE_IMAGE	STRUCT
vCenter			PE3D_VECTOR4	?	; 中心座標
vEnlarge		PE3D_VECTOR4	?	; 拡大率
vImageBase		E3D_VECTOR_2D	{}	; 画像の基準座標
dwTransparency		DWORD		?	; 透明度
rRevolveAngle		REAL4		?	; 回転角度 [rad]
pImageInf		PEGL_IMAGE_INFO	?	; 画像バッファ（必要であれば）
E3D_PRIMITIVE_IMAGE	ENDS

E3D_PRIMITIVE_POLYGON	STRUCT
dwTypeFlag		DWORD			?
pSurfaceAttr		PE3D_SURFACE_ATTRIBUTE	?
dwVertexCount		DWORD			?
dwReserved		DWORD			?
UNION
	polygon		E3D_PRIMITIVE_VERTEX		1 DUP({})
	infinite_plane	E3D_PRIMITIVE_INFINITE_PLANE	{}
	image		E3D_PRIMITIVE_IMAGE		{}
ENDS
E3D_PRIMITIVE_POLYGON	ENDS

PE3D_PRIMITIVE_POLYGON	TYPEDEF	PTR E3D_PRIMITIVE_POLYGON
PCE3D_PRIMITIVE_POLYGON	TYPEDEF	PTR E3D_PRIMITIVE_POLYGON

E3D_FLAT_POLYGON		EQU	00000000H
E3D_SMOOTH_POLYGON		EQU	00000001H
E3D_TEXTURE_POLYGON		EQU	00000002H
E3D_VERTEX_COLOR_POLYGON	EQU	00000004H
E3D_POLYGON_PRIMITIVE_MASK	EQU	00000007H
E3D_INFINITE_PLANE		EQU	0000000AH
E3D_IMAGE_PRIMITIVE		EQU	00000100H

;
;	平面パラメータ
; ----------------------------------------------------------------------------

E3D_PLANE_PARAMETER	TYPEDEF	E3D_VECTOR4
PE3D_PLANE_PARAMETER	TYPEDEF	PTR E3D_PLANE_PARAMETER
PCE3D_PLANE_PARAMETER	TYPEDEF	PTR E3D_PLANE_PARAMETER


;
;	レンダリングポリゴン情報
; ----------------------------------------------------------------------------

E3D_POLYGON_ENTRY	STRUCT
dwTypeFlag		DWORD			?
dwTransparency		DWORD			?
UNION	surface
	STRUCT	poly
		txmap		E3D_TEXTURE_MAPINFO	{}
		pAttr		PE3D_SURFACE_ATTRIBUTE	?
	ENDS
	STRUCT	image
		pInfo		PEGL_IMAGE_INFO		?
		rRevolveAngle	REAL4			?
		vCenter		E3D_VECTOR_2D		{}
		vEnlarge	E3D_VECTOR_2D		{}
		vImageBase	E3D_VECTOR_2D		{}
		pAttr		PE3D_SURFACE_ATTRIBUTE	?
	ENDS
ENDS
plane			E3D_PLANE_PARAMETER	{}
vCenter			E3D_VECTOR4		{}
dwVertexCount		DWORD			?
pVertexes		PE3D_VECTOR4		?
pNormals		PE3D_VECTOR4		?
dwProjectedCount	DWORD			?
pProjVertexes		PE3D_VECTOR_2D		?
pVertexColors		PE3D_COLOR		?
E3D_POLYGON_ENTRY	ENDS
PE3D_POLYGON_ENTRY	TYPEDEF	PTR E3D_POLYGON_ENTRY
PCE3D_POLYGON_ENTRY	TYPEDEF	PTR E3D_POLYGON_ENTRY


;
; 3D 描画パラメータ
; ----------------------------------------------------------------------------

E3D_RENDER_PARAM	STRUCT
dwFlags			DWORD			?
dwTransparency		DWORD			?
rgbaColor		E3D_COLOR		{}
pSrcImage		PEGL_IMAGE_INFO		?
pLuminousImage		PEGL_IMAGE_INFO		?
nTextureApply		DWORD			?
nLiminousApply		DWORD			?
UNION
	STRUCT	rev
		vRenderPos	E3D_VECTOR		{}
		vRevCenter	E3D_VECTOR_2D		{}
		nViewVertexes	SDWORD			?
		pViewVertexes	PCE3D_VECTOR_2D		?
		pVertexColors	PCE3D_COLOR		?
		pRevMatrix	PCE3D_REV_MATRIX	?
	ENDS
	STRUCT	poly
		nViewVertexes	SDWORD			?
		pVertexes	PCE3D_VECTOR4		?
		pViewVertexes	PCE3D_VECTOR_2D		?
		pVertexColors	PCE3D_COLOR		?
	ENDS
ENDS
E3D_RENDER_PARAM	ENDS
PE3D_RENDER_PARAM	TYPEDEF	PTR E3D_RENDER_PARAM
PCE3D_RENDER_PARAM	TYPEDEF	PTR E3D_RENDER_PARAM

E3DRP_RENDER_IMAGE	EQU	0010H
E3DRP_RENDER_POLYGON	EQU	0000H
E3DRP_Z_ORDER_SCREEN	EQU	0020H
E3DRP_NO_SCREEN_ORIGIN	EQU	0040H


;
;	ポリゴン描画
; ----------------------------------------------------------------------------

HEGL_RENDER_POLYGON	TYPEDEF	PTR

RenderPoly@Release		TYPEDEF	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON
RenderPoly@Initialize		TYPEDEF	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	pDstImage:PEGL_IMAGE_INFO, pClipRect:PCEGL_RECT,
	pZBuffer:PEGL_IMAGE_INFO, pScreenPos:PCE3D_VECTOR
RenderPoly@GetDrawImage		TYPEDEF	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON
RenderPoly@GetFunctionFlags	TYPEDEF	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON
RenderPoly@SetFunctionFlags	TYPEDEF	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON, dwFlags:DWORD
RenderPoly@PrepareMatrix	TYPEDEF	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	matrix:PTR E3D_REV_MATRIX,
	pOrigin:PCE3D_VECTOR, pEnlarge:PCE3D_VECTOR
RenderPoly@RevolveMatrix	TYPEDEF	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	pDst:PE3D_VECTOR4, pSrc:PCE3D_VECTOR4, nVectorCount:DWORD
RenderPoly@SetZClipRange	TYPEDEF	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON, rMin:REAL4, rMax:REAL4
RenderPoly@SetEnvironmentMapping	TYPEDEF	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	matrix:PTR E3D_REV_MATRIX,
	envmap:PTR E3D_ENVIRONMENT_MAPPING
RenderPoly@PrepareLight		TYPEDEF	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON, hStackHeap:HSTACKHEAP,
	nLightCount:DWORD, pLightEntries:PCE3D_LIGHT_ENTRY
RenderPoly@CretaePolygonEntry	TYPEDEF	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	hStackHeap:HSTACKHEAP, pPrimitive:PCE3D_PRIMITIVE_POLYGON
RenderPoly@MakeUpPolygon	TYPEDEF	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	hStackHeap:HSTACKHEAP, pPolyEntry:PTR E3D_POLYGON_ENTRY
RenderPoly@ApplyAttribute	TYPEDEF	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	pPolyEntry:PTR E3D_POLYGON_ENTRY,
	pColor:PCE3D_COLOR, nTransparency:DWORD
RenderPoly@GetExternalRect	TYPEDEF	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	pExtRect:PTR EGL_RECT,
	pPolyEntry:PTR PCE3D_POLYGON_ENTRY, nCount:DWORD
RenderPoly@ShadeReflectLights	TYPEDEF	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	pSurfaceAttribute:PCE3D_SURFACE_ATTRIBUTE,
	pPlaneParameter:PCE3D_PLANE_PARAMETER,
	pFocusPoint:PCE3D_VECTOR, pColorLooks:PTR E3D_COLOR
RenderPoly@ProjectScreen	TYPEDEF	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	pDst:PE3D_VECTOR_2D, pSrc:PCE3D_VECTOR4, nVertexCount:DWORD
RenderPoly@SortPolygonEntry	TYPEDEF PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	ppPolygons:PTR PE3D_POLYGON_ENTRY,
	nPolygonCount:DWORD, dwSortingFlags:DWORD
RenderPoly@PrepareRender	TYPEDEF	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON, pPolyEntry:PCE3D_POLYGON_ENTRY
RenderPoly@PrepareRenderParam	TYPEDEF	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON, pRenderParam:PCE3D_RENDER_PARAM
RenderPoly@RenderPolygon	TYPEDEF	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON
RenderPoly@RenderPolygonSub	TYPEDEF	PROTO	NEAR32 C

PRenderPoly@Release		TYPEDEF	PTR RenderPoly@Release
PRenderPoly@Initialize		TYPEDEF	PTR RenderPoly@Initialize
PRenderPoly@GetDrawImage	TYPEDEF	PTR RenderPoly@GetDrawImage
PRenderPoly@GetFunctionFlags	TYPEDEF	PTR RenderPoly@GetFunctionFlags
PRenderPoly@SetFunctionFlags	TYPEDEF	PTR RenderPoly@SetFunctionFlags
PRenderPoly@PrepareMatrix	TYPEDEF	PTR RenderPoly@PrepareMatrix
PRenderPoly@RevolveMatrix	TYPEDEF	PTR RenderPoly@RevolveMatrix
PRenderPoly@SetZClipRange	TYPEDEF	PTR RenderPoly@SetZClipRange
PRenderPoly@SetEnvironmentMapping	\
				TYPEDEF	PTR RenderPoly@SetEnvironmentMapping
PRenderPoly@PrepareLight	TYPEDEF	PTR RenderPoly@PrepareLight
PRenderPoly@CretaePolygonEntry	TYPEDEF	PTR RenderPoly@CretaePolygonEntry
PRenderPoly@MakeUpPolygon	TYPEDEF	PTR RenderPoly@MakeUpPolygon
PRenderPoly@ApplyAttribute	TYPEDEF	PTR RenderPoly@ApplyAttribute
PRenderPoly@GetExternalRect	TYPEDEF	PTR RenderPoly@GetExternalRect
PRenderPoly@ShadeReflectLights	TYPEDEF	PTR RenderPoly@ShadeReflectLights
PRenderPoly@ProjectScreen	TYPEDEF	PTR RenderPoly@ProjectScreen
PRenderPoly@SortPolygonEntry	TYPEDEF	PTR RenderPoly@SortPolygonEntry
PRenderPoly@PrepareRender	TYPEDEF	PTR RenderPoly@PrepareRender
PRenderPoly@PrepareRenderParam	TYPEDEF	PTR RenderPoly@PrepareRenderParam
PRenderPoly@RenderPolygon	TYPEDEF	PTR RenderPoly@RenderPolygon
PRenderPoly@RenderPolygonSub	TYPEDEF	PTR PRenderPoly@RenderPolygonSub

EGL_RENDER_POLYGON		STRUCT
pfnRelease			PRenderPoly@Release			?
pfnInitialize			PRenderPoly@Initialize			?
pfnGetDrawImage			PRenderPoly@GetDrawImage		?
pfnGetFunctionFlags		PRenderPoly@GetFunctionFlags		?
pfnSetFunctionFlags		PRenderPoly@SetFunctionFlags		?
pfnPrepareMatrix		PRenderPoly@PrepareMatrix		?
pfnRevolveMatrix		PRenderPoly@RevolveMatrix		?
pfnSetZClipRange		PRenderPoly@SetZClipRange		?
pfnSetEnvironmentMapping	PRenderPoly@SetEnvironmentMapping	?
pfnPrepareLight			PRenderPoly@PrepareLight		?
pfnCreatePolygonEntry		PRenderPoly@CretaePolygonEntry		?
pfnMakeUpPolygon		PRenderPoly@MakeUpPolygon		?
pfnApplyAttribute		PRenderPoly@ApplyAttribute		?
pfnGetExternalRect		PRenderPoly@GetExternalRect		?
pfnShadeReflectLights		PRenderPoly@ShadeReflectLights		?
pfnProjectScreen		PRenderPoly@ProjectScreen		?
pfnSortPolygonEntry		PRenderPoly@SortPolygonEntry		?
pfnPrepareRender		PRenderPoly@PrepareRender		?
pfnPrepareRenderParam		PRenderPoly@PrepareRenderParam		?
pfnRenderPolygon		PRenderPoly@RenderPolygon		?
EGL_RENDER_POLYGON		ENDS

E3D_FLAG_ANTIALIAS_SIDE_EDGE	EQU	0001H
E3D_FLAG_TEXTURE_SMOOTHING	EQU	0002H

E3D_SORT_TRANSPARENT	EQU	0001H
E3D_SORT_OPAQUE		EQU	0002H

eglNormalizePolygonRegion	PROTO	NEAR32 C,
	pPolyRegion:PE3D_POLYGON_REGION,
	pClipRect:PCEGL_RECT, nVertexCount:DWORD,
	pPolyVertexes:PCE3D_VECTOR_2D, pVertexColors:PCE3D_COLOR
eglCreateRenderPolygon		PROTO	NEAR32 C
eglCurrentRenderPolygon		PROTO	NEAR32 C

;
; 逆数テーブル用
;
PACK_INT_REAL	STRUCT	1
int_part	SDWORD	?
real_part	REAL4	?
PACK_INT_REAL	ENDS
tblRcpLookup	TEXTEQU	<tblRcpTableLookup256>
EXTERNDEF	C tblRcpLookup:PACK_INT_REAL


;
;	ポリゴン描画用バッファ
; ----------------------------------------------------------------------------

E3D_TRANS_LINE_BUF	STRUCT
rgbMul			EGL_PALETTE	2 DUP({})
rgbAdd			EGL_PALETTE	2 DUP({})
rZValue			REAL4		2 DUP(?)
E3D_TRANS_LINE_BUF	ENDS

E3D_PACKED_LIGHT	STRUCT
vLights			E3D_VECTOR4	3 DUP({})
vBrightness		E3D_VECTOR4	{}
rgbColor		EGL_PALETTE	4 DUP({})
E3D_PACKED_LIGHT	ENDS
PE3D_PACKED_LIGHT	TYPEDEF	PTR E3D_PACKED_LIGHT

EGL_RENDER_POLYGON_BUF	STRUCT

; RenderPolygon インターフェース
rp			EGL_RENDER_POLYGON	{}
pfnLineFunc		PRenderPoly@RenderPolygonSub	?
pfnStoreFunc		PRenderPoly@RenderPolygonSub	?
pfnAntialius		PRenderPoly@RenderPolygonSub	?

; DrawImage インターフェース
ALIGN	10H
dib			EGL_DRAW_IMAGE_BUF	{}

; 座標変換行列パラメータ
ALIGN	10H
revmat			E3D_REV_MATRIX		{}
vRevOrg			E3D_VECTOR4		{}

; 画面情報
vScreenPos		E3D_VECTOR		{}
dwFunctionFlags		DWORD			?
rZMinClip		REAL4			?
rZMaxClip		REAL4			?

; グローバル環境マッピングパラメータ
ALIGN	10H
envmat			E3D_REV_MATRIX		{}
genvmap			E3D_ENVIRONMENT_MAPPING	{}

; 光源情報
nAmbientLight		SDWORD			?, ?, ?, ?
nVectorLightCount	DWORD			?
pVectorLights		PE3D_PACKED_LIGHT	?, ?
nPointLightCount	DWORD			?
pPointLights		PE3D_PACKED_LIGHT	?, ?
rgbFogColor		EGL_PALETTE		{}
rFogDeepness		REAL4			?

; ソート用バッファ
pSortTable		PVOID			?
nSortTableLength	DWORD			?

; 一時ｚバッファ用バッファ
pTempZBuffer		PVOID			?, ?

; 簡易パラメータ描画用バッファ
hStackHeap		HSTACKHEAP		?
rpSurfAttr		E3D_SURFACE_ATTRIBUTE	{}

; レンダリングパラメータ
txtimg			EGL_IMAGE_INFO		{}	; 通常テクスチャ
lmnimg			EGL_IMAGE_INFO		{}	; 発光テクスチャ
pTxLineAddr		PVOID			?
pLmLineAddr		PVOID			?
nTextureApply		DWORD			?	; テクスチャ適用度
nLiminousApply		DWORD			?	; 発光テクスチャ適用度
dwShadingFlags		DWORD			?	; シェーディングフラグ
rRenderFogDeepness	REAL4			?
nRenderFogApply		SDWORD			?, ?
ALIGN	8
rgbRenderFogColor	WORD			4 DUP(?)

; テクスチャパラメータ
ALIGN	10H
vTxo			E3D_VECTOR4		{}	; 基準点
vTxx			E3D_VECTOR4		{}	; ｘ軸
vTxy			E3D_VECTOR4		{}	; ｙ軸
vTxxy			E3D_VECTOR4		{}	; [vTxx,vTxy] 外積
rTxoxy			REAL4			?	; (vTxo,vTxxy) 内積
rTxtxy			REAL4			?	; (vTxBase,vTxxy)
			REAL4			?
rTxoxyr			REAL4			?	; rTxoxy * r
rRcpTxoxyr		REAL4			?	; 1 / rTxoxyr
ALIGN	10H
vTxBase			E3D_VECTOR4		{}
vTxBasePos		E3D_VECTOR_2D		{}	; テクスチャ座標
vTxDeltaX		E3D_VECTOR_2D		{}	; ｘ座標差分
vTxDeltaY		E3D_VECTOR_2D		{}	; ｙ座標差分
rDeltaTxxy		REAL4			?	; 母係数
rRcpTxxy		REAL4			?	; 母数係数の逆数
			REAL4			?, ?
txSizeMask		WORD			4 DUP( ? )	; サイズマスク
			WORD			4 DUP( ? )
txSizeMaskDW		EGL_SIZE		{}
lmSizeMask		WORD			4 DUP( ? )
			WORD			4 DUP( ? )
lmSizeMaskDW		EGL_SIZE		{}
txMulAddr		SWORD			4 DUP(?)
txImageAddr		DWORD			?, ?
lmMulAddr		SWORD			4 DUP(?)
lmImageAddr		DWORD			?, ?

ALIGN	10H
vTxLinePos		E3D_VECTOR_2D		{}
vTxNextPos		E3D_VECTOR_2D		{}
ptTexturePos		EGL_POINT		{}
rTxLineMod		REAL4			?
rTxNextMod		REAL4			?
nLineLeft		SDWORD			?, ?
nLineRight		SDWORD			?, ?
nLeftDecimal		DWORD			?		; x10000H FIXED
nRightDecimal		DWORD			?		; x10000H FIXED
rgbNextColor		SWORD			8 DUP( ? )	; x80H FIXED
rgbDeltaColor		SWORD			8 DUP( ? )	; x80H FIXED

; 描画用ラインバッファ
pLineBuf		PVOID			?, ?
 
EGL_RENDER_POLYGON_BUF	ENDS

eglDrawImage@Release_Dummy	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE
eglDrawImage@Initialize_Dummy	PROTO	NEAR32 C,
	hDrawImage:HEGL_DRAW_IMAGE, pDstImage:PEGL_IMAGE_INFO,
	pClipRect:PCEGL_RECT, pZBuffer:PEGL_IMAGE_INFO
eglRenderPoly@Release		PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON
eglRenderPoly@Initialize	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	pDstImage:PEGL_IMAGE_INFO, pClipRect:PCEGL_RECT,
	pZBuffer:PEGL_IMAGE_INFO, pScreenPos:PCE3D_VECTOR
eglRenderPoly@GetDrawImage	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON
eglRenderPoly@GetFunctionFlags	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON
eglRenderPoly@SetFunctionFlags	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON, dwFlags:DWORD
eglRenderPoly@PrepareMatrix486	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	matrix:PTR E3D_REV_MATRIX,
	pOrigin:PCE3D_VECTOR, pEnlarge:PCE3D_VECTOR
eglRenderPoly@RevolveMatrix486	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	pDst:PE3D_VECTOR4, pSrc:PCE3D_VECTOR4, nVectorCount:DWORD
eglRenderPoly@PrepareMatrixSSE	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	matrix:PTR E3D_REV_MATRIX,
	pOrigin:PCE3D_VECTOR, pEnlarge:PCE3D_VECTOR
eglRenderPoly@RevolveMatrixSSE	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	pDst:PE3D_VECTOR4, pSrc:PCE3D_VECTOR4, nVectorCount:DWORD
eglRenderPoly@SetZClipRange	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON, rMin:REAL4, rMax:REAL4
eglRenderPoly@SetEnvironmentMapping	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	matrix:PTR E3D_REV_MATRIX,
	envmap:PTR E3D_ENVIRONMENT_MAPPING
eglRenderPoly@PrepareLight	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON, hStackHeap:HSTACKHEAP,
	nLightCount:DWORD, pLightEntries:PCE3D_LIGHT_ENTRY
eglRenderPoly@CretaePolygonEntry486	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	hStackHeap:HSTACKHEAP, pPrimitive:PCE3D_PRIMITIVE_POLYGON
eglRenderPoly@CretaePolygonEntrySSE	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	hStackHeap:HSTACKHEAP, pPrimitive:PCE3D_PRIMITIVE_POLYGON
eglRenderPoly@MakeUpPolygon486	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	hStackHeap:HSTACKHEAP, pPolyEntry:PTR E3D_POLYGON_ENTRY
eglRenderPoly@MakeUpPolygonSSE	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	hStackHeap:HSTACKHEAP, pPolyEntry:PTR E3D_POLYGON_ENTRY
eglRenderPoly@ApplyAttribute486	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	pPolyEntry:PTR E3D_POLYGON_ENTRY,
	pColor:PCE3D_COLOR, nTransparency:DWORD
eglRenderPoly@ApplyAttributeSSE	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	pPolyEntry:PTR E3D_POLYGON_ENTRY,
	pColor:PCE3D_COLOR, nTransparency:DWORD
eglRenderPoly@GetExternalRect486	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	pExtRect:PTR EGL_RECT,
	pPolyEntry:PTR PCE3D_POLYGON_ENTRY, nCount:DWORD
eglRenderPoly@GetExternalRectSSE	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	pExtRect:PTR EGL_RECT,
	pPolyEntry:PTR PCE3D_POLYGON_ENTRY, nCount:DWORD
eglRenderPoly@ShadeReflectLights486	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	pSurfaceAttribute:PCE3D_SURFACE_ATTRIBUTE,
	pPlaneParameter:PCE3D_PLANE_PARAMETER,
	pFocusPoint:PCE3D_VECTOR, pColorLooks:PTR E3D_COLOR
eglRenderPoly@ShadeReflectLightsSSE	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	pSurfaceAttribute:PCE3D_SURFACE_ATTRIBUTE,
	pPlaneParameter:PCE3D_PLANE_PARAMETER,
	pFocusPoint:PCE3D_VECTOR, pColorLooks:PTR E3D_COLOR
eglRenderPoly@ProjectScreen486	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	pDst:PE3D_VECTOR_2D, pSrc:PCE3D_VECTOR4, nVertexCount:DWORD
eglRenderPoly@ProjectScreenSSE	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	pDst:PE3D_VECTOR_2D, pSrc:PCE3D_VECTOR4, nVertexCount:DWORD
eglRenderPoly@SortPolygonEntry486	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON,
	ppPolygons:PTR PE3D_POLYGON_ENTRY,
	nPolygonCount:DWORD, dwSortingFlags:DWORD
eglRenderPoly@PrepareRender	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON, pPolyEntry:PCE3D_POLYGON_ENTRY
eglRenderPoly@PrepareRenderParam	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON, pRenderParam:PCE3D_RENDER_PARAM
eglRenderPoly@RenderPolygon_Err	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON
eglRenderPoly@RenderPolygon_NoDraw	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON
eglRenderPoly@RenderPolygon_Image	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON
eglRenderPoly@RenderPolygon_Plane486	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON
eglRenderPoly@RenderPolygon_PlaneSSE	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON
eglRenderPoly@RenderPolygon_Texture486	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON
eglRenderPoly@RenderPolygon_TxtNZ486	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON
eglRenderPoly@RenderPolygon_TextureSSE	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON
eglRenderPoly@RenderPolygon_TxtNZSSE	PROTO	NEAR32 C,
	hRenderPoly:HEGL_RENDER_POLYGON

; ポリゴンラインセットアップ関数
eglRenderPoly@RenderPolygon_LineNTX486	PROTO	NEAR32 C	; テクスチャなし
eglRenderPoly@RenderPolygon_LineTX486	PROTO	NEAR32 C	; テクスチャ（タイリングなし）
eglRenderPoly@RenderPolygon_LineATX486	PROTO	NEAR32 C	; RGBA テクスチャ（タイリングなし）
eglRenderPoly@RenderPolygon_LineATXNS486	PROTO	NEAR32 C	; RGBA テクスチャ（タイリングなし・シェーディングなし）
eglRenderPoly@RenderPolygon_LineTTX486	PROTO	NEAR32 C	; テクスチャ
eglRenderPoly@RenderPolygon_LineTATX486	PROTO	NEAR32 C	; RGBA テクスチャ
eglRenderPoly@RenderPolygon_LineTLTX486	PROTO	NEAR32 C	; 発光テクスチャ
; ポリゴンラインストア関数
eglRenderPoly@RenderPolygon_Store486	PROTO	NEAR32 C	; 通常
eglRenderPoly@RenderPolygon_StoreT486	PROTO	NEAR32 C	; 透明度付
eglRenderPoly@RenderPolygon_StoreM486	PROTO	NEAR32 C	; マスク付
eglRenderPoly@RenderPolygon_StoreMT486	PROTO	NEAR32 C	; マスク透明度付
eglRenderPoly@RenderPolygon_StoreA486	PROTO	NEAR32 C	; アルファ付
eglRenderPoly@RenderPolygon_StoreAT486	PROTO	NEAR32 C	; アルファ透明度付
eglRenderPoly@RenderPolygon_StoreNZ486	PROTO	NEAR32 C	; 通常（ｚ無）
; アンチエイリアス関数
eglRenderPoly@RenderPolygon_Antialias486	PROTO	NEAR32 C

; ポリゴンラインセットアップ関数
eglRenderPoly@RenderPolygon_LineNTXSSE	PROTO	NEAR32 C	; テクスチャなし
eglRenderPoly@RenderPolygon_LineTXSSE	PROTO	NEAR32 C	; テクスチャ（タイリングなし）
eglRenderPoly@RenderPolygon_LineATXSSE	PROTO	NEAR32 C	; RGBA テクスチャ（タイリングなし）
eglRenderPoly@RenderPolygon_LineSXSSE	PROTO	NEAR32 C	; テクスチャ（タイリングなし補完）
eglRenderPoly@RenderPolygon_LineSAXSSE	PROTO	NEAR32 C	; RGBA テクスチャ（タイリングなし補完）
eglRenderPoly@RenderPolygon_LineTTXSSE	PROTO	NEAR32 C	; テクスチャ
eglRenderPoly@RenderPolygon_LineTATXSSE	PROTO	NEAR32 C	; RGBA テクスチャ
eglRenderPoly@RenderPolygon_LineTLTXSSE	PROTO	NEAR32 C	; 発光テクスチャ
eglRenderPoly@RenderPolygon_LineSTXSSE	PROTO	NEAR32 C	; テクスチャ（補完）
eglRenderPoly@RenderPolygon_LineSTAXSSE	PROTO	NEAR32 C	; RGBA テクスチャ（補完）
eglRenderPoly@RenderPolygon_LineSTLXSSE	PROTO	NEAR32 C	; 発光テクスチャ（補完）
; ポリゴンラインストア関数
eglRenderPoly@RenderPolygon_StoreSSE	PROTO	NEAR32 C	; 通常
eglRenderPoly@RenderPolygon_StoreNTXSSE	PROTO	NEAR32 C	; テクスチャなし通常
eglRenderPoly@RenderPolygon_StoreTSSE	PROTO	NEAR32 C	; 透明度付
eglRenderPoly@RenderPolygon_StoreMSSE	PROTO	NEAR32 C	; マスク付
eglRenderPoly@RenderPolygon_StoreMTSSE	PROTO	NEAR32 C	; マスク透明度付
eglRenderPoly@RenderPolygon_StoreASSE	PROTO	NEAR32 C	; アルファ付
eglRenderPoly@RenderPolygon_StoreATSSE	PROTO	NEAR32 C	; アルファ透明度付
eglRenderPoly@RenderPolygon_StoreNZSSE	PROTO	NEAR32 C	; 通常（ｚ無）
eglRenderPoly@RenderPolygon_StoreTNZSSE	PROTO	NEAR32 C	; 透明度付（ｚ無）
eglRenderPoly@RenderPolygon_StoreANZSSE	PROTO	NEAR32 C	; アルファ付（ｚ無）
; アンチエイリアス関数
eglRenderPoly@RenderPolygon_AntialiasSSE	PROTO	NEAR32 C


;
;	モデル当たり判定
; ----------------------------------------------------------------------------

HEGL_MODEL_MATRIX	TYPEDEF	PTR

ModelMatrix@Release			TYPEDEF	PROTO	NEAR32 C,
	hMatrix:HEGL_MODEL_MATRIX
ModelMatrix@Initialize			TYPEDEF	PROTO	NEAR32 C,
	hMatrix:HEGL_MODEL_MATRIX,
	pModel:PTR PCE3D_PRIMITIVE_POLYGON, nPolyCount:DWORD
ModelMatrix@IsHitInclusiveSphere	TYPEDEF	PROTO	NEAR32 C,
	hMatrix:HEGL_MODEL_MATRIX,
	pSphere:PTR E3D_VECTOR, rRadius:REAL4, pHitResult:PTR SDWORD
ModelMatrix@IsHitAgainstSphere		TYPEDEF	PROTO	NEAR32 C,
	hMatrix:HEGL_MODEL_MATRIX,
	pSphere:PTR E3D_VECTOR, rRadius:REAL4,
	pHitResult:PTR SDWORD, pHitPos:PTR E3D_VECTOR,
	pHitNormal:PTR E3D_VECTOR, pReflection:PTR E3D_VECTOR
ModelMatrix@IsCrossingSegment		TYPEDEF	PROTO	NEAR32 C,
	hMatrix:HEGL_MODEL_MATRIX,
	pPos0:PTR E3D_VECTOR, pPos1:PTR E3D_VECTOR, rRadius:REAL4,
	pHitResult:PTR SDWORD, pHitPos:PTR E3D_VECTOR,
	pHitNormal:PTR E3D_VECTOR, pReflection:PTR E3D_VECTOR

PModelMatrix@Release			TYPEDEF	PTR ModelMatrix@Release
PModelMatrix@Initialize			TYPEDEF	PTR ModelMatrix@Initialize
PModelMatrix@IsHitInclusiveSphere	TYPEDEF	PTR ModelMatrix@IsHitInclusiveSphere
PModelMatrix@IsHitAgainstSphere		TYPEDEF	PTR ModelMatrix@IsHitAgainstSphere
PModelMatrix@IsCrossingSegment		TYPEDEF	PTR ModelMatrix@IsCrossingSegment

EGL_MODEL_MATRIX	STRUCT
pfnRelease		PModelMatrix@Release			?
pfnInitialize		PModelMatrix@Initialize			?
pfnIsHitInclusiveSphere	PModelMatrix@IsHitInclusiveSphere	?
pfnIsHitAgainstSphere	PModelMatrix@IsHitAgainstSphere		?
pfnIsCrossingSegment	PModelMatrix@IsCrossingSegment		?
EGL_MODEL_MATRIX	ENDS


;
;	モデル当たり判定用バッファ
; ----------------------------------------------------------------------------

EGL_POLY_MATRIX_INFO	STRUCT
vertex			E3D_VECTOR4	3 DUP( {} )	; 頂点ABC, 及び辺の長さ
				; [0].x, y, z = 頂点Aの座標
				; [0].d       = 線分ABの長さ
normal			E3D_VECTOR4	{}		; 法線
cos_abc			REAL4		?		; cos (∠ABC)
cos_acb			REAL4		?		; cos (∠ACB)
			REAL4		?, ?
EGL_POLY_MATRIX_INFO	ENDS
PEGL_POLY_MATRIX_INFO	TYPEDEF	PTR EGL_POLY_MATRIX_INFO
@POLY_MATRIX_SIZE	EQU	(SIZEOF EGL_POLY_MATRIX_INFO)

EGL_MODEL_MATRIX_BUF	STRUCT

; EGL_MODEL_MATRIX インターフェース
mmat			EGL_MODEL_MATRIX	{}

; モデル情報
nPolygonCount		DWORD			?
pPolyMatrix		PEGL_POLY_MATRIX_INFO	?
pPolyMatrixBuf		PVOID			?

EGL_MODEL_MATRIX_BUF	ENDS

;
; 当たり判定関数
;
eglCreateModelMatrix			PROTO	NEAR32 C
eglModelMatrix@Release			PROTO	NEAR32 C,
	hMatrix:HEGL_MODEL_MATRIX
eglModelMatrix@Initialize486		PROTO	NEAR32 C,
	hMatrix:HEGL_MODEL_MATRIX,
	pModel:PTR PCE3D_PRIMITIVE_POLYGON, nPolyCount:DWORD
eglModelMatrix@IsHitInclusiveSphere486	PROTO	NEAR32 C,
	hMatrix:HEGL_MODEL_MATRIX,
	pSphere:PTR E3D_VECTOR, rRadius:REAL4, pHitResult:PTR SDWORD
eglModelMatrix@IsHitAgainstSphere486	PROTO	NEAR32 C,
	hMatrix:HEGL_MODEL_MATRIX,
	pSphere:PTR E3D_VECTOR, rRadius:REAL4,
	pHitResult:PTR SDWORD, pHitPos:PTR E3D_VECTOR,
	pHitNormal:PTR E3D_VECTOR, pReflection:PTR E3D_VECTOR
eglModelMatrix@IsCrossingSegment486	PROTO	NEAR32 C,
	hMatrix:HEGL_MODEL_MATRIX,
	pPos0:PTR E3D_VECTOR, pPos1:PTR E3D_VECTOR, rErrorGap:REAL4,
	pHitResult:PTR SDWORD, pHitPos:PTR E3D_VECTOR,
	pHitNormal:PTR E3D_VECTOR, pReflection:PTR E3D_VECTOR
eglModelMatrix@InitializeSSE		PROTO	NEAR32 C,
	hMatrix:HEGL_MODEL_MATRIX,
	pModel:PTR PCE3D_PRIMITIVE_POLYGON, nPolyCount:DWORD
eglModelMatrix@IsHitInclusiveSphereSSE	PROTO	NEAR32 C,
	hMatrix:HEGL_MODEL_MATRIX,
	pSphere:PTR E3D_VECTOR, rRadius:REAL4, pHitResult:PTR SDWORD
eglModelMatrix@IsHitAgainstSphereSSE	PROTO	NEAR32 C,
	hMatrix:HEGL_MODEL_MATRIX,
	pSphere:PTR E3D_VECTOR, rRadius:REAL4,
	pHitResult:PTR SDWORD, pHitPos:PTR E3D_VECTOR,
	pHitNormal:PTR E3D_VECTOR, pReflection:PTR E3D_VECTOR
eglModelMatrix@IsCrossingSegmentSSE	PROTO	NEAR32 C,
	hMatrix:HEGL_MODEL_MATRIX,
	pPos0:PTR E3D_VECTOR, pPos1:PTR E3D_VECTOR, rErrorGap:REAL4,
	pHitResult:PTR SDWORD, pHitPos:PTR E3D_VECTOR,
	pHitNormal:PTR E3D_VECTOR, pReflection:PTR E3D_VECTOR


ENDIF

