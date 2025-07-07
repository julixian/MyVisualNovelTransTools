
/*****************************************************************************
                          Entis Graphic Library
 -----------------------------------------------------------------------------
     Copyright (c) 2002-2004 Leshade Entis, Entis-soft. Al rights reserved.
 *****************************************************************************/


#if	!defined(__EGL2D_H__)
#define	__EGL2D_H__

//
// パレット共用体
//
union	EGL_PALETTE
{
	DWORD	dwPixelCode ;			// パレット番号／ピクセルコード
	REAL32	rZOrder32 ;				// Z 値
	struct
	{
		BYTE	Blue ;
		BYTE	Green ;
		BYTE	Red ;
		BYTE	Reserved ;
	}		rgb ;					// RGB24 フォーマット
	struct
	{
		BYTE	Blue ;
		BYTE	Green ;
		BYTE	Red ;
		BYTE	Alpha ;
	}		rgba ;					// RGBA32 フォーマット
	struct
	{
		BYTE	Y ;
		BYTE	U ;
		BYTE	V ;
		BYTE	Reserved ;
	}		yuv ;					// YUV24 フォーマット
	struct
	{
		BYTE	Brightness ;
		BYTE	Saturation ;
		BYTE	Hue ;
		BYTE	Reserved ;
	}		hsb ;					// HSB24 フォーマット
} ;

class	EGLPalette
{
public:
	EGL_PALETTE	m_palette ;
public:
	EGLPalette( void ) { }
	EGLPalette( const EGL_PALETTE & palette )
		{ m_palette = palette ; }
	EGLPalette( DWORD dwRGBA )
		{ m_palette.dwPixelCode = dwRGBA ; }
	operator EGL_PALETTE ( void ) const
		{	return	m_palette ;	}
	operator EGL_PALETTE & ( void )
		{	return	m_palette ;	}
	operator const EGL_PALETTE * ( void ) const
		{	return	&m_palette ;	}
	operator EGL_PALETTE * ( void )
		{	return	&m_palette ;	}
	const EGLPalette & operator = ( const EGL_PALETTE & palette )
		{
			m_palette = palette ;
			return	*this ;
		}
	const EGLPalette & operator += ( const EGL_PALETTE & rgbColor )
		{
			DWORD	aria = (m_palette.dwPixelCode & 0x00FF00FF)
							+ (rgbColor.dwPixelCode & 0x00FF00FF) ;
			DWORD	maria = ((m_palette.dwPixelCode >> 8) & 0x00FF00FF)
							+ ((rgbColor.dwPixelCode >> 8) & 0x00FF00FF) ;
			DWORD	arisa = (((0x00FF00FF - aria) & 0xFF00FF00) >> 8)
								| ((0x00FF00FF - maria) & 0xFF00FF00) ;
			m_palette.dwPixelCode =
				(aria & 0x00FF00FF) | ((maria & 0x00FF00FF) << 8) | arisa ;
			return	*this ;
		}
	const EGLPalette & operator *= ( unsigned int x )
		{
			if ( x >= 0x100 )
			{
				x = 0x100 ;
			}
			DWORD	aria = (m_palette.dwPixelCode & 0x00FF00FF) * x ;
			DWORD	maria = ((m_palette.dwPixelCode >> 8) & 0x00FF00FF) * x ;
			m_palette.dwPixelCode =
				((aria & 0xFF00FF00) >> 8) | (maria & 0xFF00FF00) ;
			return	*this ;
		}
	EGLPalette operator + ( const EGL_PALETTE & rgbColor ) const
		{
			EGLPalette	t = *this ;
			t += rgbColor ;
			return	t ;
		}
	EGLPalette operator * ( unsigned int x ) const
		{
			EGLPalette	t = *this ;
			t *= x ;
			return	t ;
		}
} ;

typedef	EGL_PALETTE *	PEGL_PALETTE ;
typedef	const EGL_PALETTE *	PCEGL_PALETTE ;

//
// 画像情報構造体
//
struct	EGL_IMAGE_INFO
{
	DWORD				dwInfoSize ;
	DWORD				fdwFormatType ;
	DWORD				ptrOffsetPixel ;
	void *				ptrImageArray ;
	PEGL_PALETTE		pPaletteEntries ;
	DWORD				dwPaletteCount ;
	DWORD				dwImageWidth ;
	DWORD				dwImageHeight ;
	DWORD				dwBitsPerPixel ;
	SDWORD				dwBytesPerLine ;
	SDWORD				dwSizeOfImage ;
	DWORD				dwClippedPixel ;
} ;

typedef	EGL_IMAGE_INFO *	PEGL_IMAGE_INFO ;
typedef	const EGL_IMAGE_INFO *	PCEGL_IMAGE_INFO ;

#define	EIF_RGB_BITMAP		0x00000001
#define	EIF_RGBA_BITMAP		0x04000001
#define	EIF_GRAY_BITMAP		0x00000002
#define	EIF_YUV_BITMAP		0x00000004
#define	EIF_HSB_BITMAP		0x00000006
#define	EIF_Z_BUFFER_R4		0x00002005
#define	EIF_TYPE_MASK		0x00FFFFFF
#define	EIF_WITH_PALETTE	0x01000000
#define	EIF_WITH_CLIPPING	0x02000000
#define	EIF_WITH_ALPHA		0x04000000

//
// 座標・サイズ・矩形
//
struct	EGL_POINT
{
	SDWORD	x ;
	SDWORD	y ;
} ;
typedef	EGL_POINT *	PEGL_POINT ;
typedef	const EGL_POINT *	PCEGL_POINT ;

class	EGLPoint	: public	EGL_POINT
{
public:
	EGLPoint( void ) { }
	EGLPoint( SDWORD xPos, SDWORD yPos ) { x = xPos ;  y = yPos ; }
	EGLPoint( const EGL_POINT & point ) { x = point.x ;  y = point.y ; }
	const EGLPoint & operator = ( const EGL_POINT & point )
		{
			x = point.x ;
			y = point.y ;
			return	*this ;
		}
	bool operator == ( const EGL_POINT & point ) const
		{
			return	(x == point.x) && (y == point.y) ;
		}
	bool operator != ( const EGL_POINT & point ) const
		{
			return	(x != point.x) || (y != point.y) ;
		}
	const EGLPoint & operator += ( const EGL_POINT & point )
		{
			x += point.x ;
			y += point.y ;
			return	*this ;
		}
	const EGLPoint & operator -= ( const EGL_POINT & point )
		{
			x -= point.x ;
			y -= point.y ;
			return	*this ;
		}
	EGLPoint operator + ( const EGL_POINT & point ) const
		{
			EGLPoint	ptResult = *this ;
			ptResult += point ;
			return	ptResult ;
		}
	EGLPoint operator - ( const EGL_POINT & point ) const
		{
			EGLPoint	ptResult = *this ;
			ptResult -= point ;
			return	ptResult ;
		}
} ;

struct	EGL_SIZE
{
	SDWORD	w ;
	SDWORD	h ;
} ;
typedef	EGL_SIZE *	PEGL_SIZE ;
typedef	const EGL_SIZE *	PCEGL_SIZE ;

class	EGLSize	: public	EGL_SIZE
{
public:
	EGLSize( void ) { }
	EGLSize( SDWORD width, SDWORD height ) { w = width ;  h = height ; }
	EGLSize( const EGL_SIZE & size ) { w = size.w ;  h = size.h ; }
	const EGLSize & operator = ( const EGL_SIZE & size )
		{
			w = size.w ;
			h = size.h ;
			return	*this ;
		}
	bool operator == ( const EGL_SIZE & size ) const
		{
			return	(w == size.w) && (h == size.h) ;
		}
	bool operator != ( const EGL_SIZE & size ) const
		{
			return	(w != size.w) || (h != size.h) ;
		}
	const EGLSize & operator += ( const EGL_SIZE & size )
		{
			w += size.w ;
			h += size.h ;
			return	*this ;
		}
	const EGLSize & operator -= ( const EGL_SIZE & size )
		{
			w -= size.w ;
			h -= size.h ;
			return	*this ;
		}
	EGLSize operator + ( const EGL_SIZE & size ) const
		{
			EGLSize	sizeResult = *this ;
			sizeResult += size ;
			return	sizeResult ;
		}
	EGLSize operator - ( const EGL_SIZE & size ) const
		{
			EGLSize	sizeResult = *this ;
			sizeResult -= size ;
			return	sizeResult ;
		}
} ;

struct	EGL_RECT
{
	SDWORD	left ;
	SDWORD	top ;
	SDWORD	right ;
	SDWORD	bottom ;
} ;
typedef	EGL_RECT *	PEGL_RECT ;
typedef	const EGL_RECT *	PCEGL_RECT ;

struct	EGL_IMAGE_RECT
{
	SDWORD	x ;
	SDWORD	y ;
	SDWORD	w ;
	SDWORD	h ;
} ;
typedef	EGL_IMAGE_RECT *	PEGL_IMAGE_RECT ;
typedef	const EGL_IMAGE_RECT *	PCEGL_IMAGE_RECT ;

class	EGLRect	: public	EGL_RECT
{
public:
	EGLRect( void ) { }
	EGLRect( SDWORD x1, SDWORD y1, SDWORD x2, SDWORD y2 )
		{
			left = x1 ;
			top = y1 ;
			right = x2 ;
			bottom = y2 ;
		}
	EGLRect( const EGL_RECT & rect )
		{
			left = rect.left ;
			top = rect.top ;
			right = rect.right ;
			bottom = rect.bottom ;
		}
	EGLRect( const struct EGL_IMAGE_RECT & rect )
		{
			left = rect.x ;
			top = rect.y ;
			right = rect.x + rect.w - 1 ;
			bottom = rect.y + rect.h - 1 ;
		}
	const EGLRect & operator = ( const EGL_RECT & rect )
		{
			left = rect.left ;
			top = rect.top ;
			right = rect.right ;
			bottom = rect.bottom ;
			return	*this ;
		}
	const EGLRect & operator = ( const struct EGL_IMAGE_RECT & rect )
		{
			left = rect.x ;
			top = rect.y ;
			right = rect.x + rect.w - 1 ;
			bottom = rect.y + rect.h - 1 ;
			return	*this ;
		}
	bool operator &= ( const EGL_RECT & rect )
		{
			if ( (left > rect.right) || (top > rect.bottom)
					|| (right < rect.left) || (bottom < rect.top) )
			{
				left = top = 0 ;
				right = bottom = -1 ;
				return	false ;
			}
			else
			{
				if ( left < rect.left )
					left = rect.left ;
				if ( top < rect.top )
					top = rect.top ;
				if ( right > rect.right )
					right = rect.right ;
				if ( bottom > rect.bottom )
					bottom = rect.bottom ;
				return	true ;
			}
		}
	int operator == (const EGL_RECT & rect ) const
		{
			return	(left == rect.left) & (top == rect.top)
				& (right == rect.right) & (bottom == rect.bottom) ;
		}
	int operator != (const EGL_RECT & rect ) const
		{
			return	(left != rect.left) | (top != rect.top)
				| (right != rect.right) | (bottom != rect.bottom) ;
		}
} ;

class	EGLImageRect	: public	EGL_IMAGE_RECT
{
public:
	EGLImageRect( void ) { }
	EGLImageRect( SDWORD xPos, SDWORD yPos, SDWORD nWidth, SDWORD nHeight )
		{
			x = xPos ;
			y = yPos ;
			w = nWidth ;
			h = nHeight ;
		}
	EGLImageRect( const EGL_IMAGE_RECT & rect )
		{
			x = rect.x ;
			y = rect.y ;
			w = rect.w ;
			h = rect.h ;
		}
	EGLImageRect( const EGL_RECT & rect )
		{
			x = rect.left ;
			y = rect.top ;
			w = rect.right - rect.left + 1 ;
			h = rect.bottom - rect.top + 1 ;
		}
	const EGLImageRect & operator = ( const EGL_IMAGE_RECT & rect )
		{
			x = rect.x ;
			y = rect.y ;
			w = rect.w ;
			h = rect.h ;
			return	*this ;
		}
	const EGLImageRect & operator = ( const EGL_RECT & rect )
		{
			x = rect.left ;
			y = rect.top ;
			w = rect.right - rect.left + 1 ;
			h = rect.bottom - rect.top + 1 ;
			return	*this ;
		}
} ;

struct	EGL_IMAGE_AXES
{
	struct
	{
		REAL32	x ;
		REAL32	y ;
	}			xAxis ;
	struct
	{
		REAL32	x ;
		REAL32	y ;
	}			yAxis ;
} ;
typedef	EGL_IMAGE_AXES *	PEGL_IMAGE_AXES ;
typedef	const EGL_IMAGE_AXES *	PCEGL_IMAGE_AXES ;

//
// シェーディング色情報
//
struct	E3D_COLOR
{
	EGL_PALETTE	rgbMul ;
	EGL_PALETTE	rgbAdd ;
} ;
typedef	E3D_COLOR *	PE3D_COLOR ;
typedef	const E3D_COLOR *	PCE3D_COLOR ;

//
// リージョン
//
struct	E3D_POLY_LINE_REGION
{
	SDWORD		nLeft ;
	DWORD		dwReserved1 ;
	SDWORD		nRight ;
	DWORD		dwReserved2 ;
	E3D_COLOR	rgbaLeft ;
	E3D_COLOR	rgbaRight ;
} ;

struct	E3D_POLYGON_REGION
{
	SDWORD					nTopLine ;
	SDWORD					nBottomLine ;
	E3D_POLY_LINE_REGION	plrLineRgn[1] ;
} ;
typedef	E3D_POLYGON_REGION *	PE3D_POLYGON_REGION ;

//
// 画像バッファ管理
//
extern	"C"
{
	PEGL_IMAGE_INFO eglCreateImageBuffer
		( DWORD fdwFormat,
			DWORD dwWidth, DWORD dwHeight,
			DWORD dwBitsPerPixel, DWORD dwFlags = 0 ) ;
	PEGL_IMAGE_INFO eglCreateTextureInfo
		( PCEGL_IMAGE_INFO pImageInf,
			PCEGL_RECT pClipRect = NULL, DWORD dwFlags = 0 ) ;
	PEGL_IMAGE_INFO eglDuplicateImageBuffer
		( PCEGL_IMAGE_INFO pImageInf, DWORD dwFlags = 0 ) ;
	ESLError eglAddImageBufferRef( PEGL_IMAGE_INFO pImageInf ) ;
	ESLError eglDeleteImageBuffer( PEGL_IMAGE_INFO pImageInf ) ;
	ESLError eglDrawToDC
		( HDC hDstDC, PCEGL_IMAGE_INFO pImageInf,
			int nPosX, int nPosY,
			PCEGL_SIZE pSizeToDraw, PCEGL_RECT pViewRect ) ;
	HDC eglGetDC( PCEGL_IMAGE_INFO pImageInf ) ;
	ESLError eglFillImage
		( PEGL_IMAGE_INFO pImageInf, EGL_PALETTE colorFill ) ;
	ESLError eglGetClippedImageInfo
		( PEGL_IMAGE_INFO pClippedImage,
			PCEGL_IMAGE_INFO pOriginalImage,
			PCEGL_IMAGE_RECT pClippingRect ) ;
	PEGL_IMAGE_RECT eglGetOverlappedRectangle
		( PEGL_IMAGE_RECT pImageRect,
			PCEGL_RECT pDstViewRect,
			PCEGL_RECT pSrcViewRect,
			PCEGL_POINT pSrcViewOffset,
			PCEGL_SIZE pSrcViewMaxSize = NULL ) ;
	ESLError eglGetRevolvedAxes
		( PEGL_IMAGE_AXES pImageAxes,
			PEGL_POINT pBasePosition,
			PCEGL_POINT pCenterOffset,
			REAL32 rHorizontalRate = 1,
			REAL32 rVerticalRate = 1,
			REAL32 rRevolutionAngle = 0,
			REAL32 rCrossingAngle = 90,
			unsigned int nFlag = 0 ) ;
	ESLError eglReverseVertically( PEGL_IMAGE_INFO pImageInf ) ;
	EGL_PALETTE eglGetPixel
		( PCEGL_IMAGE_INFO pImageInf, int nPosX, int nPosY ) ;
	ESLError eglSetPixel
		( PEGL_IMAGE_INFO pImageInf,
			int nPosX, int nPosY, EGL_PALETTE colorPixel ) ;
} ;

#define	EGL_DRAW_RADIAN		0x0001

#define	EGL_IMAGE_HAS_DC	0x0001
#define	EGL_IMAGE_NO_DUP	0x0010


//
// カラーコード変換
//
extern	"C"
{
	void eglRGBtoYUV( EGL_PALETTE * pColor ) ;
	void eglYUVtoRGB( EGL_PALETTE * pColor ) ;
	void eglRGBtoHSB( EGL_PALETTE * pColor ) ;
	void eglHSBtoRGB( EGL_PALETTE * pColor ) ;
	DWORD eglBlendRGBA( EGL_PALETTE rgba1, EGL_PALETTE rgba2 ) ;
} ;

//
// フィルタリング関数
//
extern	"C"
{
	ESLError eglConvertFormat
		( PEGL_IMAGE_INFO pDstImage,
			PCEGL_IMAGE_INFO pSrcImage, DWORD dwFlags = 0 ) ;
	ESLError eglCalculateToneTable
		( void * pToneBuf, signed int nTone, int nFlag = 0 ) ;
	ESLError eglApplyToneTable
		( PEGL_IMAGE_INFO pDstImage, PCEGL_IMAGE_INFO pSrcImage,
			const void * pBlueTone, const void * pGreenTone,
			const void * pRedTone, const void * pAlphaTone ) ;
	ESLError eglSetColorTone
		( PEGL_IMAGE_INFO pDstImage, PCEGL_IMAGE_INFO pSrcImage,
			int nBlueTone, int nGreenTone, int nRedTone, int nAlphaTone ) ;
	ESLError eglEnlargeDouble
		( PEGL_IMAGE_INFO pDstImage,
			PCEGL_IMAGE_INFO pSrcImage, DWORD dwFlags = 0 ) ;
	ESLError eglBlendAlphaChannel
		( PEGL_IMAGE_INFO pRGBA32,
			PCEGL_IMAGE_INFO pSrcRGB,
			PCEGL_IMAGE_INFO pSrcAlpha,
			DWORD dwFlags = 0,
			SDWORD nAlphaBase = 0,
			DWORD nCoefficient = 0x10 ) ;
	ESLError eglUnpackAlphaChannel
		( PEGL_IMAGE_INFO pDstRGB, PEGL_IMAGE_INFO pDstAlpha,
			PCEGL_IMAGE_INFO pSrcRGBA32, DWORD dwFlags = 0 ) ;
} ;

#define	EGL_TONE_BRIGHTNESS		0x0000
#define	EGL_TONE_INVERSION		0x0001
#define	EGL_TONE_LIGHT			0x0002

#define	EGL_BAC_MULTIPLY		0x0001
#define	EGL_BAC_ADD_ALPHA		0x0002
#define	EGL_BAC_MULTIPLY_ALPHA	0x0004

//
// 画像描画
//
struct	EGL_DRAW_DEST
{
	PEGL_IMAGE_INFO	pDstImage ;
	EGL_RECT		rectDstClip ;
	PEGL_IMAGE_INFO	pZBuffer ;
} ;

struct	EGL_DRAW_PARAM
{
	DWORD				dwFlags ;
	EGL_POINT			ptBasePos ;
	PEGL_IMAGE_INFO		pSrcImage ;
	PCEGL_RECT			pViewRect ;
	EGL_PALETTE			rgbDimColor ;
	EGL_PALETTE			rgbLightColor ;
	unsigned int		nTransparency ;
	REAL32				rZOrder ;
	PCEGL_IMAGE_AXES	pImageAxes ;
	EGL_PALETTE			rgbColorParam1 ;
} ;

#define	EGL_DRAW_BLEND_ALPHA	0x0001
#define	EGL_DRAW_GLOW_LIGHT		0x0002
#define	EGL_WITH_Z_ORDER		0x0004
#define	EGL_SMOOTH_STRETCH		0x0010
#define	EGL_UNSMOOTH_STRETCH	0x0020
#define	EGL_FIXED_POSITION		0x0040

inline DWORD EGL_DRAW_APPLY( int x )
{
	return	(x & 0x7F) << 16 ;
}
inline DWORD EGL_DRAW_FUNC( int x )
{
	return	(x & 0x7F) << 24 ;
}

#define	EGL_APPLY_C_ADD		0x00800000
#define	EGL_APPLY_C_MUL		0x00820000
#define	EGL_APPLY_A_MUL		0x00880000
#define	EGL_APPLY_C_MASK	0x00890000
#define	EGL_DRAW_F_ADD		0x80000000
#define	EGL_DRAW_F_SUB		0x81000000
#define	EGL_DRAW_F_MUL		0x82000000
#define	EGL_DRAW_F_DIV		0x83000000
#define	EGL_DRAW_A_MOVE		0x88000000
#define	EGL_DRAW_A_MUL		0x89000000

#define	EGL_FILL_INVERSION	0x00000100

typedef	struct EGL_DRAW_IMAGE *	HEGL_DRAW_IMAGE ;

struct	EGL_DRAW_IMAGE
{
	ESLError (*pfnRelease)( HEGL_DRAW_IMAGE hDrawImage ) ;
	ESLError (*pfnInitialize)
		( HEGL_DRAW_IMAGE hDrawImage, PEGL_IMAGE_INFO pDstImage,
					PCEGL_RECT pClipRect, PEGL_IMAGE_INFO pZBuffer ) ;
	ESLError (*pfnGetDestination)
		( HEGL_DRAW_IMAGE hDrawImage, EGL_DRAW_DEST * pDrawDest ) ;
	DWORD (*pfnGetFunctionFlags)( HEGL_DRAW_IMAGE hDrawImage ) ;
	DWORD (*pfnSetFunctionFlags)
		( HEGL_DRAW_IMAGE hDrawImage, DWORD dwFlags ) ;
	ESLError (*pfnPrepareDraw)
		( HEGL_DRAW_IMAGE hDrawImage, const EGL_DRAW_PARAM * pDrawParam ) ;
	ESLError (*pfnDrawImage)( HEGL_DRAW_IMAGE hDrawImage ) ;
	void *	ptrReserved ;
	ESLError (*pfnPrepareLine)
		( HEGL_DRAW_IMAGE hDrawImage,
			int x1, int y1, int x2, int y2,
			EGL_PALETTE colorDraw,
			unsigned int nTranparency, DWORD dwFlags ) ;
	ESLError (*pfnPrepareFillRect)
		( HEGL_DRAW_IMAGE hDrawImage,
			PCEGL_RECT pDrawRect, EGL_PALETTE colorDraw,
			unsigned int nTranparency, DWORD dwFlags ) ;
	ESLError (*pfnPrepareFillEllipse)
		( HEGL_DRAW_IMAGE hDrawImage,
			PCEGL_POINT pCenter, PCEGL_SIZE pRadius,
			EGL_PALETTE colorDraw,
			unsigned int nTranparency, DWORD dwFlags ) ;
	ESLError (*pfnPrepareFillPolygon)
		( HEGL_DRAW_IMAGE hDrawImage,
			PCEGL_POINT pVertexes, unsigned int nCount,
			EGL_PALETTE colorDraw,
			unsigned int nTranparency, DWORD dwFlags ) ;
	ESLError (*pfnFillRegion)( HEGL_DRAW_IMAGE hDrawImage ) ;
	ESLError (*pfnDrawRegion)( HEGL_DRAW_IMAGE hDrawImage ) ;

	// 画像描画オブジェクト解放
	inline ESLError Release( void )
		{
			return	(*pfnRelease)( this ) ;
		}
	// 画像描画オブジェクト初期化
	inline ESLError Initialize
		( PEGL_IMAGE_INFO pDstImage,
			PCEGL_RECT pClipRect, PEGL_IMAGE_INFO pZBuffer )
		{
			return	(*pfnInitialize)( this, pDstImage, pClipRect, pZBuffer ) ;
		}
	// 画像描画先取得
	inline ESLError GetDestination( EGL_DRAW_DEST * pDrawDest )
		{
			return	(*pfnGetDestination)( this, pDrawDest ) ;
		}
	// 描画機能フラグ取得
	inline DWORD GetFunctionFlags( void )
		{
			return	(*pfnGetFunctionFlags)( this ) ;
		}
	// 描画機能フラグ設定
	inline DWORD SetFunctionFlags( DWORD dwFlags )
		{
			return	(*pfnSetFunctionFlags)( this, dwFlags ) ;
		}
	// 画像描画準備
	inline ESLError PrepareDraw( const EGL_DRAW_PARAM * pDrawParam )
		{
			return	(*pfnPrepareDraw)( this, pDrawParam ) ;
		}
	// 画像描画
	inline ESLError DrawImage( void )
		{
			return	(*pfnDrawImage)( this ) ;
		}
	// 直線描画準備
	inline ESLError PrepareLine
		( int x1, int y1, int x2, int y2,
			EGL_PALETTE colorDraw,
			unsigned int nTransparency, DWORD dwFlags = 0 )
		{
			return	(*pfnPrepareLine)
				( this, x1, y1, x2, y2, colorDraw, nTransparency, dwFlags ) ;
		}
	// 矩形描画準備
	inline ESLError PrepareFillRect
		( PCEGL_RECT pDrawRect, EGL_PALETTE colorDraw,
			unsigned int nTransparency, DWORD dwFlags = 0 )
		{
			return	(*pfnPrepareFillRect)
				( this, pDrawRect, colorDraw, nTransparency, dwFlags ) ;
		}
	// 楕円描画準備
	inline ESLError PrepareFillEllipse
		( PCEGL_POINT pCenter, PCEGL_SIZE pRadius,
			EGL_PALETTE colorDraw,
			unsigned int nTransparency, DWORD dwFlags = 0 )
		{
			return	(*pfnPrepareFillEllipse)
				( this, pCenter, pRadius, colorDraw, nTransparency, dwFlags ) ;
		}
	// 多角形描画準備
	inline ESLError PrepareFillPolygon
		( PCEGL_POINT pVertexes, unsigned int nCount,
			EGL_PALETTE colorDraw,
			unsigned int nTransparency, DWORD dwFlags = 0 )
		{
			return	(*pfnPrepareFillPolygon)
				( this, pVertexes, nCount, colorDraw, nTransparency, dwFlags ) ;
		}
	// 図形描画
	inline ESLError FillRegion( void )
		{
			return	(*pfnFillRegion)( this ) ;
		}
	// 図形輪郭描画
	inline ESLError DrawRegion( void )
		{
			return	(*pfnDrawRegion)( this ) ;
		}

} ;

extern	"C"
{
	HEGL_DRAW_IMAGE eglCreateDrawImage( void ) ;
} ;

//
// 2次元ベクトル
//
struct	E3D_VECTOR_2D
{
public:
	REAL32	x ;
	REAL32	y ;
public:
	bool operator == ( const E3D_VECTOR_2D & vector ) const
		{
			return	(x == vector.x) && (y == vector.y) ;
		}
	bool operator != ( const E3D_VECTOR_2D & vector ) const
		{
			return	(x != vector.x) && (y != vector.y) ;
		}
	const E3D_VECTOR_2D & operator += ( const E3D_VECTOR_2D & vector )
		{
			x += vector.x ;
			y += vector.y ;
			return	*this ;
		}
	const E3D_VECTOR_2D & operator -= ( const E3D_VECTOR_2D & vector )
		{
			x -= vector.x ;
			y -= vector.y ;
			return	*this ;
		}
	const E3D_VECTOR_2D & operator *= ( double num )
		{
			x *= (REAL32) num ;
			y *= (REAL32) num ;
			return	*this ;
		}
	E3D_VECTOR_2D operator + ( const E3D_VECTOR_2D & vector ) const
		{
			E3D_VECTOR_2D	result = *this ;
			result += vector ;
			return	result ;
		}
	E3D_VECTOR_2D operator - ( void ) const
		{
			E3D_VECTOR_2D	result = { - x, - y } ;
			return	result ;
		}
	E3D_VECTOR_2D operator - ( const E3D_VECTOR_2D & vector ) const
		{
			E3D_VECTOR_2D	result = *this ;
			result -= vector ;
			return	result ;
		}
	E3D_VECTOR_2D operator * ( double num ) const
		{
			E3D_VECTOR_2D	result ;
			result.x = (REAL32) (x * num) ;
			result.y = (REAL32) (y * num) ;
			return	result ;
		}
	E3D_VECTOR_2D operator * ( const E3D_VECTOR_2D & vector ) const
		{
			E3D_VECTOR_2D	result ;
			result.x = x * vector.x - y * vector.y ;
			result.y = x * vector.y + y * vector.x ;
			return	result ;
		}
	E3D_VECTOR_2D operator / ( const E3D_VECTOR_2D & vector ) const
		{
			E3D_VECTOR_2D	result ;
			REAL32	temp_num
				= vector.x * vector.x + vector.y * vector.y ;
			result.x = (x * vector.x + y * vector.y) / temp_num ;
			result.y = (y * vector.x - y * vector.x) / temp_num ;
			return	result ;
		}
	const E3D_VECTOR_2D & operator *= ( const E3D_VECTOR_2D & vector )
		{
			return	(*this = *this * vector) ;
		}
	const E3D_VECTOR_2D & operator /= ( const E3D_VECTOR_2D & vector )
		{
			return	(*this = *this / vector) ;
		}
} ;

class	E3DVector2D	: public	E3D_VECTOR_2D
{
public:
	E3DVector2D( void ) { }
	E3DVector2D( double vx, double vy ) { x = (REAL32) vx;  y = (REAL32) vy ; }
	E3DVector2D( const E3D_VECTOR_2D & v ) { x = v.x ;  y = v.y ; }
	E3DVector2D( const EGL_POINT & point )
		{ x = (REAL32) point.x ;  y = (REAL32) point.y ; }
	const E3DVector2D & operator = ( const E3D_VECTOR_2D & vector )
		{
			x = vector.x ;
			y = vector.y ;
			return	*this ;
		}

} ;

typedef	E3D_VECTOR_2D *			PE3D_VECTOR_2D ;
typedef	const E3D_VECTOR_2D *	PCE3D_VECTOR_2D ;

//
// 3次元ベクトル
//
struct	E3D_VECTOR
{
public:
	REAL32	x ;
	REAL32	y ;
	REAL32	z ;
public:
	bool operator == ( const E3D_VECTOR & vector ) const
		{
			return	(x == vector.x) && (y == vector.y) && (z == vector.z) ;
		}
	bool operator != ( const E3D_VECTOR & vector ) const
		{
			return	(x != vector.x) || (y != vector.y) || (z != vector.z) ;
		}
	const E3D_VECTOR & operator += ( const E3D_VECTOR & vector )
		{
			x += vector.x ;
			y += vector.y ;
			z += vector.z ;
			return	*this ;
		}
	const E3D_VECTOR & operator -= ( const E3D_VECTOR & vector )
		{
			x -= vector.x ;
			y -= vector.y ;
			z -= vector.z ;
			return	*this ;
		}
	const E3D_VECTOR & operator *= ( double number )
		{
			x *= (REAL32) number ;
			y *= (REAL32) number ;
			z *= (REAL32) number ;
			return	*this ;
		}
	const E3D_VECTOR & operator *= ( const E3D_VECTOR & vector )
		{
			ExteriorProduct( vector ) ;
			return	*this ;
		}
	const E3D_VECTOR & operator /= ( double number )
		{
			x /= (REAL32) number ;
			y /= (REAL32) number ;
			z /= (REAL32) number ;
			return	*this ;
		}
	E3D_VECTOR operator + ( const E3D_VECTOR & vector ) const
		{
			E3D_VECTOR	result = *this ;
			result += vector ;
			return	result ;
		}
	E3D_VECTOR operator - ( void ) const
		{
			E3D_VECTOR	result = { - x, - y, - z } ;
			return	result ;
		}
	E3D_VECTOR operator - ( const E3D_VECTOR & vector ) const
		{
			E3D_VECTOR	result = *this ;
			result -= vector ;
			return	result ;
		}
	E3D_VECTOR operator * ( const E3D_VECTOR & vector ) const
		{
			E3D_VECTOR	result = *this ;
			result.ExteriorProduct( vector ) ;
			return	result ;
		}
	E3D_VECTOR operator * ( double number ) const
		{
			E3D_VECTOR	result = *this ;
			result *= number ;
			return	result ;
		}
	E3D_VECTOR operator / ( double number ) const
		{
			E3D_VECTOR	result = *this ;
			result /= number ;
			return	result ;
		}
	REAL64 Absolute( void ) const ;
	void ExteriorProduct( const E3D_VECTOR & vector ) ;
	REAL64 InnerProduct( const E3D_VECTOR & vector ) const ;
	void RoundTo1( void ) ;
} ;

class	E3DVector	: public	E3D_VECTOR
{
public:
	E3DVector( void ) { }
	E3DVector( double vx, double vy, double vz )
		{ x = (REAL32) vx ;  y = (REAL32) vy ;  z = (REAL32) vz ; }
	E3DVector( const E3D_VECTOR & v )
		{ x = v.x ;  y = v.y ;  z = v.z ; }
	const E3DVector & operator = ( const E3D_VECTOR & vector )
		{
			x = vector.x ;
			y = vector.y ;
			z = vector.z ;
			return	*this ;
		}

} ;

typedef	E3D_VECTOR *		PE3D_VECTOR ;
typedef	const E3D_VECTOR *	PCE3D_VECTOR ;

struct	E3D_VECTOR4	: public	E3D_VECTOR
{
	REAL32	d ;
} ;

class	E3DVector4	: public	E3D_VECTOR4
{
public:
	E3DVector4( void ) { }
	E3DVector4( double vx, double vy, double vz, double vd = 0 )
		{ x = (REAL32) vx ;  y = (REAL32) vy ;
			z = (REAL32) vz ;  d = (REAL32) vd ; }
	E3DVector4( const E3D_VECTOR & v )
		{ x = v.x ;  y = v.y ;  z = v.z ;  d = 0 ; }
	E3DVector4( const E3D_VECTOR4 & v )
		{ x = v.x ;  y = v.y ;  z = v.z ;  d = v.d ; }
	const E3DVector4 & operator = ( const E3D_VECTOR & vector )
		{
			x = vector.x ;
			y = vector.y ;
			z = vector.z ;
			d = 0 ;
			return	*this ;
		}
	const E3DVector4 & operator = ( const E3D_VECTOR4 & vector )
		{
			x = vector.x ;
			y = vector.y ;
			z = vector.z ;
			d = vector.d ;
			return	*this ;
		}

} ;

typedef	E3D_VECTOR4 *		PE3D_VECTOR4 ;
typedef	const E3D_VECTOR4 *	PCE3D_VECTOR4 ;


//
// 3x3 変換行列
// （アラインのため 4x4 ）
//
struct	E3D_REV_MATRIX
{
public:
	REAL32	matrix[4][4] ;
public:
	void InitializeMatrix( const E3D_VECTOR & vector ) ;
	void RevolveOnX( double rSin, double rCos ) ;
	void RevolveOnY( double rSin, double rCos ) ;
	void RevolveOnZ( double rSin, double rCos ) ;
	void RevolveByAngleOn( const E3D_VECTOR & angle ) ;
	void RevolveForAngle( const E3D_VECTOR & angle ) ;
	void MagnifyByVector( const E3D_VECTOR & vector ) ;
	void RevolveMatrix( E3D_REV_MATRIX & matDst ) const ;
	void RevolveByMatrix( const E3D_REV_MATRIX & matSrc ) ;
	void RevolveVector( E3D_VECTOR & vector ) const ;
} ;

typedef	E3D_REV_MATRIX *		PE3D_REV_MATRIX ;
typedef	const E3D_REV_MATRIX *	PCE3D_REV_MATRIX ;

//
// 環境マッピング
//
struct	E3D_ENVIRONMENT_MAPPING
{
	PEGL_IMAGE_INFO	pRoundImage ;		// 球形画像
	PEGL_IMAGE_INFO	pRoundLuminous ;
	PEGL_IMAGE_INFO	pUpperImage ;		// 上半球
	PEGL_IMAGE_INFO	pUpperLuminous ;
	PEGL_IMAGE_INFO	pUnderImage ;		// 下半球
	PEGL_IMAGE_INFO	pUnderLuminous ;
} ;

//
// テクスチャーマッピング
//
struct	E3D_TEXTURE_MAPPING
{
	PEGL_IMAGE_INFO	pTextureImage ;		// テクスチャ画像
	PEGL_IMAGE_INFO	pLuminousImage ;	// 発光テクスチャ
	REAL32			rThresholdZ ;		// 縮小画像に切り替えるための閾値
	DWORD			nSmallScale ;		// 縮小画像のスケール（1/2^n）
	PEGL_IMAGE_INFO	pSmallImage ;		// 縮小テクスチャ画像
	PEGL_IMAGE_INFO	pSmallLuminous ;	// 縮小発光テクスチャ
} ;

//
// ポリゴンの表面属性
//
struct	E3D_SURFACE_ATTRIBUTE
{
	DWORD			dwShadingFlags ;	// シェーディングフラグ
	DWORD			dwReserved ;
	E3D_COLOR		rgbaColor ;			// 基本色
	union
	{
		E3D_TEXTURE_MAPPING		txmap ;
		E3D_ENVIRONMENT_MAPPING	envmap ;
	} ;
	DWORD			nTextureApply ;		// テクスチャ適用度（未使用）
	DWORD			nLiminousApply ;	// 発光テクスチャ適用度
	SDWORD			nAmbient ;			// 環境光
	SDWORD			nDiffusion ;		// 拡散光
	SDWORD			nSpecular ;			// 反射光
	SDWORD			nSpecularSize ;
	SDWORD			nTransparency ;		// 透明度
	SDWORD			nDeepness ;			// 透明深度係数
} ;
typedef	E3D_SURFACE_ATTRIBUTE *			PE3D_SURFACE_ATTRIBUTE ;
typedef	const E3D_SURFACE_ATTRIBUTE *	PCE3D_SURFACE_ATTRIBUTE ;

#define	E3DSAF_NO_SHADING			0x00000000		// シェーディング無し
#define	E3DSAF_FLAT_SHADE			0x00000001		// フラットシェーディング（未使用）
#define	E3DSAF_GOURAUD_SHADE		0x00000002		// グーローシェーディング
#define	E3DSAF_PHONG_SHADE			0x00000003		// フォンシェーディング（未使用）
#define	E3DSAF_SHADING_MASK			0x000000FF
#define	E3DSAF_TEXTURE_TILING		0x00000100		// テクスチャをタイリング
#define	E3DSAF_TEXTURE_TRIM			0x00000200		// トリミングテクスチャ
#define	E3DSAF_TEXTURE_SMOOTH		0x00000400		// テクスチャ補完拡大
#define	E3DSAF_TEXTURE_MAPPING		0x00001000		// テクスチャマッピング
#define	E3DSAF_ENVIRONMENT_MAP		0x00002000		// 環境マッピング
#define	E3DSAF_GENVIRONMENT_MAP		0x00004000		// グローバル環境マッピング
#define	E3DSAF_SINGLE_SIDE_PLANE	0x00010000		// 片面ポリゴン
#define	E3DSAF_NO_ZBUFFER			0x00020000		// ｚ比較を行わないで描画

//
// 光源
//
struct	E3D_LIGHT_ENTRY
{
	DWORD		dwLightType ;
	EGL_PALETTE	rgbColor ;
	REAL32		rBrightness ;
	DWORD		dwReserved ;
	E3D_VECTOR4	vecLight ;
} ;
typedef	E3D_LIGHT_ENTRY *	PE3D_LIGHT_ENTRY ;
typedef	const E3D_LIGHT_ENTRY *	PCE3D_LIGHT_ENTRY ;

#define	E3D_FOG_LIGHT			0x00000000		// 擬似フォッグ
#define	E3D_AMBIENT_LIGHT		0x00000001		// 環境光
#define	E3D_VECTOR_LIGHT		0x00000002		// 無限遠光源
#define	E3D_POINT_LIGHT			0x00000004		// 点光源

//
// テクスチャマッピング用パラメータ
//
struct	E3D_TEXTURE_MAPINFO
{
	PEGL_IMAGE_INFO	pTextureImage ;		// 通常テクスチャ
	PEGL_IMAGE_INFO	pLuminousImage ;	// 発光テクスチャ
	DWORD			nTextureApply ;		// テクスチャ適用度
	DWORD			nLiminousApply ;	// 発光テクスチャ適用度
	E3D_VECTOR4		vOriginPos ;		// マッピングパラメータ
	E3D_VECTOR4		vAxisX ;
	E3D_VECTOR4		vAxisY ;
	REAL32			rFogDeepness ;		// 擬似フォッグ用
	EGL_PALETTE		rgbFogColor ;
} ;

//
// ポリゴンプリミティブ情報
//
struct	E3D_PRIMITIVE_VERTEX
{
	PE3D_VECTOR4		vertex ;	// 頂点座標
	PE3D_VECTOR4		normal ;	// 法線（スムースポリゴンのみ）
	union
	{
		E3D_VECTOR_2D	uv_map ;	// UV 座標（テクスチャポリゴンのみ）
		E3D_COLOR		color ;		// 頂点色（頂点色ポリゴンのみ）
	} ;
} ;

struct	E3D_PRIMITIVE_INFINITE_PLANE
{
	PE3D_VECTOR4	vertex ;		// 平面基準点
	PE3D_VECTOR4	xAxis ;			// テクスチャｘ軸
	PE3D_VECTOR4	yAxis ;			// テクスチャｙ軸
	REAL32			rFogDeepness ;	// 擬似フォッグ用
	EGL_PALETTE		rgbFogColor ;
} ;

struct	E3D_PRIMITIVE_IMAGE
{
	PE3D_VECTOR4	vCenter ;			// 中心座標
	PE3D_VECTOR4	vEnlarge ;			// 拡大率
	E3D_VECTOR_2D	vImageBase ;		// 画像の基準座標
	DWORD			dwTransparency ;	// 透明度
	REAL32			rRevolveAngle ;		// 回転角 [rad]
	PEGL_IMAGE_INFO	pImageInf ;			// 画像バッファ（必要であれば）
} ;

struct	E3D_PRIMITIVE_POLYGON
{
	DWORD					dwTypeFlag ;
	PE3D_SURFACE_ATTRIBUTE	pSurfaceAttr ;
	DWORD					dwVertexCount ;
	DWORD					dwReserved ;
	union
	{
		// ポリゴン
		E3D_PRIMITIVE_VERTEX			polygon[1] ;
		// 無限平面（UV付）
		E3D_PRIMITIVE_INFINITE_PLANE	infinite_plane ;
		// 画像
		E3D_PRIMITIVE_IMAGE				image ;
	} ;
} ;
typedef	E3D_PRIMITIVE_POLYGON *	PE3D_PRIMITIVE_POLYGON ;
typedef	const E3D_PRIMITIVE_POLYGON *	PCE3D_PRIMITIVE_POLYGON ;

#define	E3D_FLAT_POLYGON			0x00000000
#define	E3D_SMOOTH_POLYGON			0x00000001
#define	E3D_TEXTURE_POLYGON			0x00000002
#define	E3D_VERTEX_COLOR_POLYGON	0x00000004
#define	E3D_POLYGON_PRIMITIVE_MASK	0x00000007
#define	E3D_INFINITE_PLANE			0x0000000A
#define	E3D_IMAGE_PRIMITIVE			0x00000100

//
// 平面パラメータ
//
typedef	E3D_VECTOR4	E3D_PLANE_PARAMETER ;
typedef	E3D_PLANE_PARAMETER *		PE3D_PLANE_PARAMETER ;
typedef	const E3D_PLANE_PARAMETER *	PCE3D_PLANE_PARAMETER ;

//
// レンダリングポリゴン情報
//
struct	E3D_POLYGON_ENTRY
{
	DWORD				dwTypeFlag ;
	DWORD				dwTransparency ;
	union
	{
		struct
		{
			E3D_TEXTURE_MAPINFO		txmap ;
			PE3D_SURFACE_ATTRIBUTE	pAttr ;
		}					poly ;
		struct
		{
			PEGL_IMAGE_INFO			pInfo ;
			REAL32					rRevolveAngle ;
			E3D_VECTOR_2D			vCenter ;
			E3D_VECTOR_2D			vEnlarge ;
			E3D_VECTOR_2D			vImageBase ;
			PE3D_SURFACE_ATTRIBUTE	pAttr ;
		}					image ;
	}					surface ;
	E3D_PLANE_PARAMETER	plane ;
	E3D_VECTOR4			vCenter ;
	DWORD				dwVertexCount ;
	PE3D_VECTOR4		pVertexes ;
	PE3D_VECTOR4		pNormals ;
	DWORD				dwProjectedCount ;
	PE3D_VECTOR_2D		pProjVertexes ;
	PE3D_COLOR			pVertexColors ;
} ;
typedef	E3D_POLYGON_ENTRY *			PE3D_POLYGON_ENTRY ;
typedef	const E3D_POLYGON_ENTRY *	PCE3D_POLYGON_ENTRY ;

//
// 3D 描画パラメータ
//
struct	E3D_RENDER_PARAM
{
	DWORD				dwFlags ;
	DWORD				dwTransparency ;
	E3D_COLOR			rgbaColor ;
	PEGL_IMAGE_INFO		pSrcImage ;
	PEGL_IMAGE_INFO		pLuminousImage ;
	DWORD				nTextureApply ;		// テクスチャ適用度（未使用）
	DWORD				nLiminousApply ;	// 発光テクスチャ適用度
	union
	{
		struct
		{
			E3D_VECTOR			vRenderPos ;
			E3D_VECTOR_2D		vRevCenter ;
			int					nViewVertexes ;
			PCE3D_VECTOR_2D		pViewVertexes ;
			PCE3D_COLOR			pVertexColors ;
			PCE3D_REV_MATRIX	pRevMatrix ;
		}		rev ;
		struct
		{
			int					nViewVertexes ;
			PCE3D_VECTOR4		pVertexes ;
			PCE3D_VECTOR_2D		pViewVertexes ;
			PCE3D_COLOR			pVertexColors ;
		}		poly ;
	} ;
} ;
typedef	E3D_RENDER_PARAM *			PE3D_RENDER_PARAM ;
typedef	const E3D_RENDER_PARAM *	PCE3D_RENDER_PARAM ;

/*	EGL_RENDER_PARAM::dwFlags フラグの取りうる組み合わせ
#define	EGL_WITH_Z_ORDER			0x0004
#define	E3DSAF_TEXTURE_TILING		0x00000100		// テクスチャをタイリング
#define	E3DSAF_TEXTURE_TRIM			0x00000200		// トリミングテクスチャ
#define	E3DSAF_TEXTURE_SMOOTH		0x00000400		// テクスチャ補完拡大
*/
#define	E3DRP_RENDER_REV_IMAGE		0x0010
#define	E3DRP_RENDER_POLYGON		0x0000
#define	E3DRP_Z_ORDER_SCREEN		0x0020
#define	E3DRP_NO_SCREEN_ORIGIN		0x0040

//
// ポリゴン描画
//
typedef	struct EGL_RENDER_POLYGON *	HEGL_RENDER_POLYGON ;

struct	EGL_RENDER_POLYGON
{
	ESLError (*pfnRelease)( HEGL_RENDER_POLYGON hRenderPoly ) ;
	ESLError (*pfnInitialize)
		( HEGL_RENDER_POLYGON hRenderPoly,
			PEGL_IMAGE_INFO pDstImage,
			PCEGL_RECT pClipRect,
			PEGL_IMAGE_INFO pZBuffer, PCE3D_VECTOR pScreenPos ) ;
	HEGL_DRAW_IMAGE (*pfnGetDrawImage)( HEGL_RENDER_POLYGON hRenderPoly ) ;
	DWORD (*pfnGetFunctionFlags)( HEGL_RENDER_POLYGON hRenderPoly ) ;
	DWORD (*pfnSetFunctionFlags)
		( HEGL_RENDER_POLYGON hRenderPoly, DWORD dwFlags ) ;
	ESLError (*pfnPrepareMatrix)
		( HEGL_RENDER_POLYGON hRenderPoly,
			const E3D_REV_MATRIX * matrix,
			PCE3D_VECTOR pOrigin, PCE3D_VECTOR pEnlarge ) ;
	ESLError (*pfnRevolveMatrix)
		( HEGL_RENDER_POLYGON hRenderPoly,
			PE3D_VECTOR4 pDst, PCE3D_VECTOR4 pSrc, unsigned int nVectorCount ) ;
	ESLError (*pfnSetZClipRange)
		( HEGL_RENDER_POLYGON hRenderPoly, REAL32 rMin, REAL32 rMax ) ;
	ESLError (*pfnSetEnvironmentMapping)
		( HEGL_RENDER_POLYGON hRenderPoly,
			const E3D_REV_MATRIX * matrix,
			const E3D_ENVIRONMENT_MAPPING * envmap ) ;
	ESLError (*pfnPrepareLight)
		( HEGL_RENDER_POLYGON hRenderPoly,
			HSTACKHEAP hStackHeap, unsigned int nLightCount,
								PCE3D_LIGHT_ENTRY pLightEntries ) ;
	E3D_POLYGON_ENTRY * (*pfnCreatePolygonEntry)
		( HEGL_RENDER_POLYGON hRenderPoly,
			HSTACKHEAP hStackHeap, PCE3D_PRIMITIVE_POLYGON pPrimitive ) ;
	E3D_POLYGON_ENTRY * (*pfnMakeUpPolygon)
		( HEGL_RENDER_POLYGON hRenderPoly,
			HSTACKHEAP hStackHeap, E3D_POLYGON_ENTRY * pPolyEntry ) ;
	ESLError (*pfnApplyAttribute)
		( HEGL_RENDER_POLYGON hRenderPoly,
			E3D_POLYGON_ENTRY * pPolyEntry,
				const E3D_COLOR * pColor, unsigned int nTransparency ) ;
	ESLError (*pfnGetExternalRect)
		( HEGL_RENDER_POLYGON hRenderPoly, EGL_RECT * pExtRect,
			PCE3D_POLYGON_ENTRY * pPolyEntry, unsigned int nCount ) ;
	ESLError (*pfnShadeReflectLights)
		( HEGL_RENDER_POLYGON hRenderPoly,
			PCE3D_SURFACE_ATTRIBUTE pSurfaceAttribute,
			PCE3D_PLANE_PARAMETER pPlaneParameter,
			PCE3D_VECTOR pFocusPoint, E3D_COLOR * pColorLooks ) ;
	ESLError (*pfnProjectScreen)
		( HEGL_RENDER_POLYGON hRenderPoly, PE3D_VECTOR_2D pDst,
				PCE3D_VECTOR4 pSrc, unsigned int nVertexCount ) ;
	ESLError (*pfnSortPolygonEntry)
		( HEGL_RENDER_POLYGON hRenderPoly,
			PE3D_POLYGON_ENTRY * ppPolygons,
			unsigned int nPolygonCount, DWORD dwSortingFlags ) ;
	ESLError (*pfnPrepareRender)
		( HEGL_RENDER_POLYGON hRenderPoly, PCE3D_POLYGON_ENTRY pPolyEntry ) ;
	ESLError (*pfnPrepareRenderParam)
		( HEGL_RENDER_POLYGON hRenderPoly, PCE3D_RENDER_PARAM pRenderParam ) ;
	ESLError (*pfnRenderPolygon)( HEGL_RENDER_POLYGON hRenderPoly ) ;

	// ポリゴンレンダリングオブジェクト解放
	inline ESLError Release( void )
		{
			return	(*pfnRelease)( this ) ;
		}
	// ポリゴンレンダリングオブジェクト初期化
	inline ESLError Initialize
		( PEGL_IMAGE_INFO pDstImage, PCEGL_RECT pClipRect,
			PEGL_IMAGE_INFO pZBuffer, PCE3D_VECTOR pScreenPos )
		{
			return	(*pfnInitialize)
				( this, pDstImage, pClipRect, pZBuffer, pScreenPos ) ;
		}
	// 画像描画オブジェクト取得
	inline HEGL_DRAW_IMAGE GetDrawImage( void )
		{
			return	(*pfnGetDrawImage)( this ) ;
		}
	// 機能フラグを取得する
	inline DWORD GetFunctionFlags( void )
		{
			return	(*pfnGetFunctionFlags)( this ) ;
		}
	// 機能フラグを設定する
	inline DWORD SetFunctionFlags( DWORD dwFlags )
		{
			return	(*pfnSetFunctionFlags)( this, dwFlags ) ;
		}
	// 座標変換準備
	inline ESLError PrepareMatrix
		( const E3D_REV_MATRIX * matrix,
			PCE3D_VECTOR pOrigin, PCE3D_VECTOR pEnlarge )
		{
			return	(*pfnPrepareMatrix)( this, matrix, pOrigin, pEnlarge ) ;
		}
	// 座標変換
	inline ESLError RevolveMatrix
		( PE3D_VECTOR4 pDst, PCE3D_VECTOR4 pSrc, unsigned int nVectorCount )
		{
			return	(*pfnRevolveMatrix)( this, pDst, pSrc, nVectorCount ) ;
		}
	// Z クリッピング設定
	inline ESLError SetZClipRange( REAL32 rMin, REAL32 rMax )
		{
			return	(*pfnSetZClipRange)( this, rMin, rMax ) ;
		}
	// 環境マッピング設定
	inline ESLError SetEnvironmentMapping
		( const E3D_REV_MATRIX * matrix,
			const E3D_ENVIRONMENT_MAPPING * envmap )
		{
			return	(*pfnSetEnvironmentMapping)( this, matrix, envmap ) ;
		}
	// ライト設定
	inline ESLError PrepareLight
			( HSTACKHEAP hStackHeap,
				unsigned int nLightCount, PCE3D_LIGHT_ENTRY pLightEntries )
		{
			return	(*pfnPrepareLight)
				( this, hStackHeap, nLightCount, pLightEntries ) ;
		}
	// レンダリング用ポリゴン情報作成
	inline E3D_POLYGON_ENTRY * CreatePolygonEntry
			( HSTACKHEAP hStackHeap, PCE3D_PRIMITIVE_POLYGON pPrimitive )
		{
			return	(*pfnCreatePolygonEntry)( this, hStackHeap, pPrimitive ) ;
		}
	// ポリゴンにシェーディング・透視変換を施す
	inline E3D_POLYGON_ENTRY * MakeUpPolygon
			( HSTACKHEAP hStackHeap, E3D_POLYGON_ENTRY * pPolyEntry )
		{
			return	(*pfnMakeUpPolygon)( this, hStackHeap, pPolyEntry ) ;
		}
	// ポリゴンに属性を設定する
	inline ESLError ApplyAttribute
			( E3D_POLYGON_ENTRY * pPolyEntry,
				const E3D_COLOR * pColor, unsigned int nTransparency )
		{
			return	(*pfnApplyAttribute)
				( this, pPolyEntry, pColor, nTransparency ) ;
		}
	// 最小外接矩形を取得する
	inline ESLError GetExternalRect
			( EGL_RECT * pExtRect,
				PCE3D_POLYGON_ENTRY * pPolyEntry, unsigned int nCount )
		{
			return	(*pfnGetExternalRect)
				( this, pExtRect, pPolyEntry, nCount ) ;
		}
	// シェーディング
	inline ESLError ShadeReflectLights
		( PCE3D_SURFACE_ATTRIBUTE pSurfaceAttribute,
			PCE3D_PLANE_PARAMETER pPlaneParameter,
			PCE3D_VECTOR pFocusPoint, E3D_COLOR * pColorLooks )
		{
			return	(*pfnShadeReflectLights)
				( this, pSurfaceAttribute,
					pPlaneParameter, pFocusPoint, pColorLooks ) ;
		}
	// 透視変換
	inline ESLError ProjectScreen
			( PE3D_VECTOR_2D pDst,
				PCE3D_VECTOR4 pSrc, unsigned int nVertexCount )
		{
			return	(*pfnProjectScreen)( this, pDst, pSrc, nVertexCount ) ;
		}
	// ポリゴンをソート
	inline ESLError SortPolygonEntry
		( PE3D_POLYGON_ENTRY * ppPolygons,
			unsigned int nPolygonCount, DWORD dwSortingFlags )
		{
			return	(*pfnSortPolygonEntry)
				( this, ppPolygons, nPolygonCount, dwSortingFlags ) ;
		}
	// レンダリング準備
	inline ESLError PrepareRender( PCE3D_POLYGON_ENTRY pPolyEntry )
		{
			return	(*pfnPrepareRender)( this, pPolyEntry ) ;
		}
	// レンダリング準備
	inline ESLError PrepareRenderParam( PCE3D_RENDER_PARAM pRenderParam )
		{
			return	(*pfnPrepareRenderParam)( this, pRenderParam ) ;
		}
	// レンダリング
	inline ESLError RenderPolygon( void )
		{
			return	(*pfnRenderPolygon)( this ) ;
		}

} ;

#define	E3D_FLAG_ANTIALIAS_SIDE_EDGE	0x0001
#define	E3D_FLAG_TEXTURE_SMOOTHING		0x0002

#define	E3D_SORT_TRANSPARENT	0x0001
#define	E3D_SORT_OPAQUE			0x0002


//
// モデル当たり判定
//
typedef	struct EGL_MODEL_MATRIX *	HEGL_MODEL_MATRIX ;

struct	EGL_MODEL_MATRIX
{
	ESLError (*pfnRelease)( HEGL_MODEL_MATRIX hMatrix ) ;
	ESLError (*pfnInitialize)
		( HEGL_MODEL_MATRIX hMatrix,
			PCE3D_PRIMITIVE_POLYGON * pModel, unsigned int nPolyCount ) ;
	ESLError (*pfnIsHitInclusiveSphere)
		( HEGL_MODEL_MATRIX hMatrix,
			const E3D_VECTOR * pSphere, REAL32 rRadius, int * pHitResult ) ;
	ESLError (*pfnIsHitAgainstSphere)
		( HEGL_MODEL_MATRIX hMatrix,
			const E3D_VECTOR * pSphere, REAL32 rRadius,
				int * pHitResult, E3D_VECTOR * pHitPos,
				E3D_VECTOR * pHitNormal, E3D_VECTOR * pReflection ) ;
	ESLError (*pfnIsCrossingSegment)
		( HEGL_MODEL_MATRIX hMatrix,
			const E3D_VECTOR * pPos0, const E3D_VECTOR * pPos1,
				REAL32 rErrorGap, int * pHitResult, E3D_VECTOR * pHitPos,
				E3D_VECTOR * pHitNormal, E3D_VECTOR * pReflection ) ;

	// 当たり判定オブジェクト解放
	inline ESLError Release( void )
		{
			return	(*pfnRelease)( this ) ;
		}
	// 当たり判定オブジェクト初期化
	inline ESLError Initialize
			( PCE3D_PRIMITIVE_POLYGON * pModel, unsigned int nPolyCount )
		{
			return	(*pfnInitialize)( this, pModel, nPolyCount ) ;
		}
	// 内包空間と球体との交差判定
	inline ESLError IsHitInclusiveSphere
			( const E3D_VECTOR * pSphere, double rRadius, int * pHitResult )
		{
			return	(*pfnIsHitInclusiveSphere)
				( this, pSphere, (REAL32) rRadius, pHitResult ) ;
		}
	// ポリゴンと球体との交差判定
	inline ESLError IsHitAgainstSphere
		( const E3D_VECTOR * pSphere, double rRadius,
				int * pHitResult, E3D_VECTOR * pHitPos,
				E3D_VECTOR * pHitNormal, E3D_VECTOR * pReflection )
		{
			return	(*pfnIsHitAgainstSphere)
				( this, pSphere, (REAL32) rRadius,
					pHitResult, pHitPos, pHitNormal, pReflection ) ;
		}
	// ポリゴンと線分との交差判定
	inline ESLError IsCrossingSegment
		( const E3D_VECTOR * pPos0, const E3D_VECTOR * pPos1,
			double rErrorGap, int * pHitResult, E3D_VECTOR * pHitPos,
			E3D_VECTOR * pHitNormal, E3D_VECTOR * pReflection )
		{
			return	(*pfnIsCrossingSegment)
				( this, pPos0, pPos1, (REAL32) rErrorGap,
					pHitResult, pHitPos, pHitNormal, pReflection ) ;
		}
} ;

//
// EGL 関数
//
extern	"C"
{
	PE3D_POLYGON_REGION eglNormalizePolygonRegion
		( PE3D_POLYGON_REGION pPolyRegion,
			PCEGL_RECT pClipRect,
			unsigned int nVertexCount,
			PCE3D_VECTOR_2D pPolyVertexes,
			PCE3D_COLOR pVertexColors = NULL ) ;
	HEGL_RENDER_POLYGON eglCreateRenderPolygon( void ) ;
	HEGL_RENDER_POLYGON eglCurrentRenderPolygon( void ) ;
	HEGL_MODEL_MATRIX eglCreateModelMatrix( void ) ;
} ;


#endif
