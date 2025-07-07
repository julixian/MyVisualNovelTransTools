
/*****************************************************************************
                  Adobe(R) Photoshop(R) ファイルローダー
 *****************************************************************************/

namespace	PSD
{

//////////////////////////////////////////////////////////////////////////////
// 構造体
//////////////////////////////////////////////////////////////////////////////

#pragma	pack( push, __PSDFILE_ALIGN__, 1 )

enum	ImageMode
{
	modeBitmap,
	modeGrayscale,
	modeIndexed,
	modeRGB,
	modeCMYK,
	modeMultichannel,
	modeDuotone,
	modeLab
} ;

struct	FILE_HEADER
{
	DWORD	dwSignature ;			// must be "8BPS"
	WORD	wVersion ;				// must be 1
	BYTE	bytReserved[6] ;		// must be zero
	WORD	wChannels ;				// チャネル数 [1,24]
	DWORD	dwHeight ;				// 高さ
	DWORD	dwWidth ;				// 幅
	WORD	wDepth ;				// ビット深度 {1,8,16}
	WORD	wMode ;					// 画像モード (ImageMode)
} ;

struct	COLOR_MODE_DATA				// modeIndexed, modeDuotone 以外では０
{
	DWORD	dwLength ;
	BYTE	bytData[1] ;
} ;

struct	IMAGE_RESOURCE_SECTION
{
	DWORD	dwLength ;
	BYTE	bytData[1] ;
} ;

enum	ChannelID
{
	chidRed = 0, chidGreen = 1, chidBlue = 2,
	chidTransparencyMask = -1, chidUserSuppliedMask = -2
} ;

struct	CHANNEL_LENGTH_INFO
{
	WORD	wChannelID ;		// enum ChannelID
	DWORD	dwLength ;			// チャネルデータ長
} ;

struct	ADJUST_LAYER_DATA
{
	DWORD	dwSize ;
	DWORD	dwTop ;				// Rectangle enclosing layer mask
	DWORD	dwLeft ;
	DWORD	dwBottom ;
	DWORD	dwRight ;
	BYTE	bytDefColor ;		// 0 or 255
	BYTE	Flags ;				// bit 0 = position relative to layer
								// bit 1 = layer mask disabled
								// bit 2 = inver layer mask when blending
	WORD	wPadding ;			// Zeros
} ;

enum	LayerBlendMode
{
	blendNormal		= 'norm',
	blendDarken		= 'dark',
	blendLighten	= 'lite',
	blendHue		= 'hue ',
	blendSat		= 'sat ',
	blendColor		= 'colr',
	blendLuminosity	= 'lum ',
	blendMultiply	= 'mul ',
	blendDivision	= 'div ',
	blendScreen		= 'scrn',
	blendDissolve	= 'diss',
	blendOverlay	= 'over',
	blendHardLight	= 'hLit',
	blendSoftLight	= 'sLit',
	blendDifference	= 'diff'
} ;

enum	LayerFlag
{
	flagTransparencyProtected	= 0x01,
	flagInvisible				= 0x02
} ;

struct	LAYER_RECORD
{
	DWORD				dwTop ;				// レイヤー領域
	DWORD				dwLeft ;
	DWORD				dwBottom ;
	DWORD				dwRight ;
	WORD				wChannels ;			// チャネル数
	CHANNEL_LENGTH_INFO	chlen[24] ;			// チャネル長
	DWORD				dwSignature ;		// must be "8BIM"
	DWORD				dwBlendMode ;		// 'norm' etc...
	BYTE				bytOpacity ;		// 0:透明 ～ 255:不透明
	BYTE				bytClipping ;		// 0:base, 1:それ以外
	BYTE				bytFlags ;			// bit0 : 透明部分の保護
											// bit1 : 不可視
	BYTE				bytFilter ;			// must be zero
	DWORD				dwExtraSize ;		// これ以降のバイト数
	ADJUST_LAYER_DATA	adjdata ;
	EString				strLayerName ;
} ;

#pragma	pack( pop, __PSDFILE_ALIGN__ )


//////////////////////////////////////////////////////////////////////////////
// エンディアン変換関数
//////////////////////////////////////////////////////////////////////////////

inline DWORD dwswap( DWORD dwData )
{
#if	defined(ERI_INTEL_X86)
	__asm
	{
		mov	eax, dwData
		bswap	eax
		mov	dwData, eax
	}
	return	dwData ;
#else
	return	(dwData >> 24) | ((dwData >> 8) & 0xFF00)
			| ((dwData << 8) & 0x00FF0000) | (dwData << 24) ;
#endif
}

inline WORD wswap( WORD wData )
{
	return	(wData << 8) | (wData >> 8) ;
}


//////////////////////////////////////////////////////////////////////////////
// PSD ファイル
//////////////////////////////////////////////////////////////////////////////

class	File	: public ESLObject
{
protected:
	ESLFileObject *			m_pfile ;			// PSD ファイル
	FILE_HEADER				m_fhHeader ;		// ファイルヘッダ
	EStreamBuffer			m_bufColorMode ;	// Color Mode Data
	EObjArray<LAYER_RECORD>	m_lstLayer ;		// レイヤー情報
	DWORD					m_dwLayerDataPos ;	// レイヤー画像の位置
	DWORD					m_dwBaseDataPos ;	// ベース画像の位置

	EStreamBuffer				m_bufLayerInfo ;	// 書き出し用レイヤー情報
	EObjArray<EStreamBuffer>	m_lstLayerData ;	// 書き出し用レイヤー画像
	EStreamBuffer				m_bufBaseData ;		// 書き出し用ベース画像

public:
	class	LayerInfo
	{
	public:
		EGL_IMAGE_RECT		irRect ;
		EGL_IMAGE_RECT		irMask ;
		int					nChannels ;
		CHANNEL_LENGTH_INFO	chlen[24] ;
		DWORD				dwBlendMode ;
		unsigned int		nTransparency ;
		DWORD				dwFlags ;
		EString				strLayerName ;
	} ;

public:
	// 構築関数
	File( void ) ;
	// 消滅関数
	virtual ~File( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( PSD::File, ESLObject )

public:
	// ファイルを開く
	ESLError Open( ESLFileObject & file ) ;
	// ファイルを閉じる
	void Close( void ) ;

public:
	// ヘッダ情報を取得する
	void GetFileHeader( FILE_HEADER & fhHeader ) const ;
	// カラーモード情報を取得する
	void GetColorModeData( EStreamBuffer bufColorMode ) const ;
	// レイヤーの総数を取得する
	unsigned int GetLayerCount( void ) const ;
	// レイヤー情報を取得する
	ESLError GetLayerInfo
		( LayerInfo & liLayer, unsigned int iLayer ) const ;
	// レイヤー画像を取得する
	ESLError LoadLayerImage
		( EGLImage & imgBuf, unsigned int iLayer ) ;
	// ベース画像を取得する
	ESLError LoadBaseImage( EGLImage & imgBuf ) ;

public:
	// RLE の展開
	static void UnpackBits
		( BYTE * ptrBuf, int nBufSize,
			const BYTE * ptrRLE, int nRLESize ) ;

public:
	// PSD ファイル書き出しの準備を始める
	ESLError PrepareToWrite
		( ESLFileObject & file, PCEGL_IMAGE_INFO pBase ) ;
	// レイヤーを追加する
	ESLError AddLayerImage
		( PCEGL_IMAGE_INFO pLayer,
			const char * pszName = NULL,
			int xPos = 0, int yPos = 0,
			unsigned int nTransparency = 0,
			DWORD dwBlendMode = blendNormal ) ;
	// ファイルへの書き出しを終了する
	ESLError FinishToWrite( void ) ;

public:
	// レイヤー画像を圧縮する
	static ESLError PackLayerImage
		( EStreamBuffer & bufImage,
			LAYER_RECORD & lrLayer, PCEGL_IMAGE_INFO pLayer ) ;
	// ベース画像を圧縮する
	static ESLError PackBaseImage
		( EStreamBuffer & bufImage, PCEGL_IMAGE_INFO pBase ) ;
	// RLE の圧縮
	static int PackBits
		( BYTE * ptrRLE, int nRLESize,
			const BYTE * ptrBuf, int nBufSize ) ;

} ;

} ;

