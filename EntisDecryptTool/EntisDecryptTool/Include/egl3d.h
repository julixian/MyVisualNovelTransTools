
/*****************************************************************************
                          Entis Graphic Library
 -----------------------------------------------------------------------------
    Copyright (c) 2002-2004 Leshade Entis, Entis-soft. Al rights reserved.
 *****************************************************************************/


#if	!defined(__EGL3D_H__)
#define	__EGL3D_H__

#if	!defined(COMPACT_NOA_DECODER)

//////////////////////////////////////////////////////////////////////////////
// 画像オブジェクト
//////////////////////////////////////////////////////////////////////////////

class	EGLImage	: public	ESLObject
{
protected:
	enum	ImageOwnerFlag
	{
		iofRefInfo		= 0,
		iofRefBuffer,
		iofOwnBuffer
	} ;
	PEGL_IMAGE_INFO	m_pImage ;
	ImageOwnerFlag	m_iofOwnerFlag ;

public:
	// 画像展開コールバック
	typedef ESLError
		(__stdcall *PFUNC_CALLBACK_DECODE)
			( EGLImage * ptrImage, void * ptrData,
				DWORD dwDecoded, DWORD dwTotal ) ;
public:
	// 画像展開オブジェクト
	class	EGLImageDecoder	: public	ERISADecoder
	{
	public:
		EGLImage *				m_ptrImage ;
		PFUNC_CALLBACK_DECODE	m_pfnCallback ;
		void *					m_ptrCallbackData ;
	public:
		EGLImageDecoder
			( EGLImage * ptrImage,
				PFUNC_CALLBACK_DECODE pfnCallback, void * ptrData )
			: m_ptrImage( ptrImage ),
				m_pfnCallback( pfnCallback ), m_ptrCallbackData( ptrData ) { }
		virtual ESLError OnDecodedBlock
			( LONG line, LONG column, const EGL_IMAGE_RECT & rect ) ;
	} ;

public:
	// 構築関数
	EGLImage( void ) : m_pImage(NULL), m_iofOwnerFlag(iofRefInfo) { }
	// 消滅関数
	virtual ~EGLImage( void )
		{
			if ( m_pImage && m_iofOwnerFlag )
				DeleteImage( ) ;
		}
	// クラス情報
	DECLARE_CLASS_INFO( EGLImage, ESLObject )

public:
	// 画像バッファ関連付け
	virtual void AttachImage( PEGL_IMAGE_INFO pImage ) ;
	// 画像バッファの参照を設定
	virtual void SetImageView
		( PEGL_IMAGE_INFO pImage, PCEGL_RECT pViewRect = NULL ) ;
	// 画像バッファ作成
	virtual PEGL_IMAGE_INFO CreateImage
		( DWORD fdwFormat, DWORD dwWidth, DWORD dwHeight,
				DWORD dwBitsPerPixel, DWORD dwFlags = 0 ) ;
	// 画像バッファ複製
	virtual PEGL_IMAGE_INFO DuplicateImage
		( PCEGL_IMAGE_INFO pImage, DWORD dwFlags = 0 ) ;

	// 画像ファイルを読み込む
	virtual ESLError ReadImageFile
		( ESLFileObject & file,
			PFUNC_CALLBACK_DECODE pfnCallback = NULL,
			void * ptrCallbackData = NULL ) ;
	// 画像バッファ消去
	virtual void DeleteImage( void ) ;

	// プレビュー画像展開フラグ
	enum	PreviewFlags
	{
		pfForcePreview		= 0x0001,
		pfResizePreview		= 0x0002
	} ;
	// プレビュー画像を読み込む
	ESLError ReadPreviewImage
		( ESLFileObject & file,
			DWORD dwMaxWidth, DWORD dwMaxHeight,
			DWORD dwPreviewFlags = pfForcePreview,
			EGL_SIZE * pOriginalSize = NULL,
			PFUNC_CALLBACK_DECODE pfnCallback = NULL,
			void * ptrCallbackData = NULL ) ;

	// 圧縮方式フラグ
	enum	CompressTypeFlag
	{
		ctfCompatibleFormat,
		ctfExtendedFormat,
		ctfSuperiorArchitecure
	} ;
	// 画像ファイルへ書き出す
	ESLError WriteImageFile
		( ESLFileObject & file,
			CompressTypeFlag ctfType = ctfCompatibleFormat,
			DWORD dwFlags = ERISAEncoder::efNormalCmpr,
			const ERISAEncoder::PARAMETER * periep = NULL ) ;

public:
	// 画像情報取得
	PEGL_IMAGE_INFO GetInfo( void ) const
		{
			return	m_pImage ;
		}
	operator PCEGL_IMAGE_INFO ( void ) const
		{
			return	m_pImage ;
		}
	operator PEGL_IMAGE_INFO ( void )
		{
			return	m_pImage ;
		}
	operator const EGL_IMAGE_INFO & ( void ) const
		{
			ESLAssert( m_pImage != NULL ) ;
			return	*m_pImage ;
		}
	// 画像フォーマット取得
	DWORD GetFormatType( void ) const
		{
			return	m_pImage ? m_pImage->fdwFormatType : 0 ;
		}
	// 画像サイズ取得
	EGLSize GetSize( void ) const
		{
			return	EGLSize( GetWidth(), GetHeight() ) ;
		}
	DWORD GetWidth( void ) const
		{
			return	m_pImage ? m_pImage->dwImageWidth : 0 ;
		}
	DWORD GetHeight( void ) const
		{
			return	m_pImage ? m_pImage->dwImageHeight : 0 ;
		}
	// ビット深度取得
	DWORD GetBitsPerPixel( void ) const
		{
			return	m_pImage ? m_pImage->dwBitsPerPixel : 0 ;
		}

public:
	// 画像の上下反転
	ESLError ReverseVertically( void )
		{
			ESLAssert( m_pImage != NULL ) ;
			if ( m_pImage == NULL )
				return	eslErrGeneral ;
			return	::eglReverseVertically( m_pImage ) ;
		}
	// 画像をデバイスコンテキストに描画
	ESLError DrawToDC( HDC hDstDC, int nPosX, int nPosY,
					PCEGL_SIZE pSizeToDraw, PCEGL_RECT pViewRect ) const
		{
			ESLAssert( m_pImage != NULL ) ;
			if ( m_pImage == NULL )
				return	eslErrGeneral ;
			return	::eglDrawToDC
				( hDstDC, m_pImage,
					nPosX, nPosY, pSizeToDraw, pViewRect ) ;
		}
	// 画像を指定の値で塗りつぶす
	ESLError FillImage( EGL_PALETTE colorFill )
		{
			ESLAssert( m_pImage != NULL ) ;
			if ( m_pImage == NULL )
				return	eslErrGeneral ;
			return	::eglFillImage( m_pImage, colorFill ) ;
		}
	// 指定ピクセルを取得
	EGL_PALETTE GetPixel( int nPosX, int nPosY ) const
		{
			ESLAssert( m_pImage != NULL ) ;
			if ( m_pImage == NULL )
				return	(EGL_PALETTE) EGLPalette( 0UL ) ;
			return	::eglGetPixel( m_pImage, nPosX, nPosY ) ;
		}
	// 指定ピクセルに設定
	ESLError SetPixel( int nPosX, int nPosY, EGL_PALETTE colorPixel )
		{
			ESLAssert( m_pImage != NULL ) ;
			if ( m_pImage == NULL )
				return	eslErrGeneral ;
			return	::eglSetPixel( m_pImage, nPosX, nPosY, colorPixel ) ;
		}

public:
	// フォーマット変換
	ESLError ConvertFrom( PCEGL_IMAGE_INFO pSrcImage )
		{
			ESLAssert( m_pImage != NULL ) ;
			if ( m_pImage == NULL )
				return	eslErrGeneral ;
			return	::eglConvertFormat( m_pImage, pSrcImage ) ;
		}
	// トーンフィルタ適用
	ESLError ApplyToneTable
		( PCEGL_IMAGE_INFO pSrcImage,
			const void * pBlueTone, const void * pGreenTone,
			const void * pRedTone, const void * pAlphaTone )
		{
			ESLAssert( m_pImage != NULL ) ;
			if ( m_pImage == NULL )
				return	eslErrGeneral ;
			return	::eglApplyToneTable
				( m_pImage, pSrcImage,
					pBlueTone, pGreenTone, pRedTone, pAlphaTone ) ;
		}
	// 輝度フィルタ適用
	ESLError SetColorTone
		( PCEGL_IMAGE_INFO pSrcImage,
			int nBlueTone, int nGreenTone, int nRedTone, int nAlphaTone )
		{
			ESLAssert( m_pImage != NULL ) ;
			if ( m_pImage == NULL )
				return	eslErrGeneral ;
			return	::eglSetColorTone
				( m_pImage, pSrcImage,
					nBlueTone, nGreenTone, nRedTone, nAlphaTone ) ;
		}
	// 2倍に拡大処理
	ESLError EnlargeDouble( PCEGL_IMAGE_INFO pSrcImage, DWORD dwFlags = 0 )
		{
			ESLAssert( m_pImage != NULL ) ;
			if ( m_pImage == NULL )
				return	eslErrGeneral ;
			return	::eglEnlargeDouble( m_pImage, pSrcImage, dwFlags ) ;
		}
	// αチャネル合成
	ESLError BlendAlphaChannel
		( PCEGL_IMAGE_INFO pSrcRGB, PCEGL_IMAGE_INFO pSrcAlpha,
			DWORD dwFlags = 0, SDWORD nAlphaBase = 0, DWORD nCoefficient = 0x1000 )
		{
			ESLAssert( m_pImage != NULL ) ;
			if ( m_pImage == NULL )
				return	eslErrGeneral ;
			return	::eglBlendAlphaChannel
				( m_pImage, pSrcRGB, pSrcAlpha,
					dwFlags, nAlphaBase, nCoefficient ) ;
		}
	// αチャネル分離
	ESLError UnpackAlphaChannel
			( PEGL_IMAGE_INFO pDstRGB,
				PEGL_IMAGE_INFO pDstAlpha, DWORD dwFlags = 0 ) const
		{
			ESLAssert( m_pImage != NULL ) ;
			if ( m_pImage == NULL )
				return	eslErrGeneral ;
			return	::eglUnpackAlphaChannel
						( pDstRGB, pDstAlpha, m_pImage, dwFlags ) ;
		}

} ;


//////////////////////////////////////////////////////////////////////////////
// アニメーション画像オブジェクト
//////////////////////////////////////////////////////////////////////////////

class	EGLAnimation	: public	EGLImage
{
protected:
	DWORD				m_dwTotalTime ;
	DWORD				m_dwCurrent ;
	ENumArray<UINT>		m_lstSequence ;
	EPtrObjArray<EGL_IMAGE_INFO>
						m_lstFrames ;
	EGL_POINT			m_ptHotSpot ;
	long int			m_nResolution ;

public:
	// 構築関数
	EGLAnimation( void ) ;
	// 消滅関数
	virtual ~EGLAnimation( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( EGLAnimation, EGLImage )

public:
	// 画像バッファの参照を設定
	virtual void SetImageView
		( PEGL_IMAGE_INFO pImage, PCEGL_RECT pViewRect = NULL ) ;
	// 画像バッファ作成
	virtual PEGL_IMAGE_INFO CreateImage
		( DWORD fdwFormat, DWORD dwWidth, DWORD dwHeight,
				DWORD dwBitsPerPixel, DWORD dwFlags = 0 ) ;
	// 画像バッファ複製
	virtual PEGL_IMAGE_INFO DuplicateImage
		( PCEGL_IMAGE_INFO pImage, DWORD dwFlags = 0 ) ;
	// 画像ファイルを読み込む
	virtual ESLError ReadImageFile
		( ESLFileObject & file,
			PFUNC_CALLBACK_DECODE pfnCallback = NULL,
			void * ptrCallbackData = NULL ) ;
	// 画像バッファ消去
	virtual void DeleteImage( void ) ;
	// 画像ファイルへ書き出す
	ESLError WriteImageFile
		( ESLFileObject & file,
			CompressTypeFlag ctfType = ctfCompatibleFormat,
			DWORD dwFlags = ERISAEncoder::efNormalCmpr,
			const ERISAEncoder::PARAMETER * periep = NULL ) ;

public:
	// シーケンステーブルを取得
	ENumArray<UINT> & GetSequenceTable( void )
		{
			return	m_lstSequence ;
		}
	// フレームを削除
	void RemoveFrameAt( int iFrame ) ;
	// フレームを追加
	void AddFrame( PEGL_IMAGE_INFO pImage ) ;
	// フレームを挿入
	void InsertFrame( int iFrame, PEGL_IMAGE_INFO pImage ) ;

public:
	// 全時間を取得
	DWORD GetTotalTime( void ) const
		{
			return	m_dwTotalTime ;
		}
	// 全フレーム数を設定
	void SetTotalTime( DWORD dwTotalTime )
		{
			m_dwTotalTime = dwTotalTime ;
		}
	// 全フレーム数を取得
	DWORD GetTotalFrameCount( void ) const
		{
			return	m_lstFrames.GetSize( ) ;
		}
	// 全シーケンス長を取得
	DWORD GetSequenceLength( void ) const
		{
			return	m_lstSequence.GetSize( ) ;
		}
	// 現在のシーケンス番号を取得
	DWORD GetCurrentSequence( void ) const
		{
			return	m_dwCurrent ;
		}
	// ホットスポットを取得
	EGL_POINT GetHotSpot( void ) const
		{
			return	m_ptHotSpot ;
		}
	// ホットスポットを設定
	void SetHotSpot( EGL_POINT ptHotSpot )
		{
			m_ptHotSpot = ptHotSpot ;
		}
	// 解像度取得
	long int GetResolution( void ) const
		{
			return	m_nResolution ;
		}
	// 解像度設定
	void SetResolution( long int nResolution )
		{
			m_nResolution = nResolution ;
		}
	// 時間からシーケンス番号に変換
	DWORD TimeToSequence( DWORD dwMilliSec ) const ;
	// シーケンス番号から時間に変換
	DWORD SequenceToTime( DWORD dwSequence ) const ;
	// シーケンス番号からフレーム番号に変換
	DWORD SequenceToFrame( DWORD dwSequence ) const ;
	// 指定フレームの画像を取得
	PEGL_IMAGE_INFO GetFrameAt( DWORD dwFrame ) ;
	// 現在のシーケンス画像を設定する
	ESLError SetCurrentSequence( DWORD dwSequence ) ;

} ;


//////////////////////////////////////////////////////////////////////////////
// 画像メディア読み込みクラス
//////////////////////////////////////////////////////////////////////////////

namespace	Gdiplus
{
	class	GpBitmap ;
} ;
class	EGLMediaLoader	: public	EGLAnimation
{
public:
	// 構築関数
	EGLMediaLoader( void ) ;
	// 消滅関数
	virtual ~EGLMediaLoader( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( EGLMediaLoader, EGLAnimation )

public:
	// 画像ファイルを読み込む
	ESLError LoadMediaFile
		( const char * pszFileName,
			DWORD fdwFormat = 0, DWORD dwBitsPerPixel = 0,
			DWORD dwLimitFrames = 0, DWORD dwLimitSize = 0 ) ;
	ESLError ReadMediaFile
		( ESLFileObject & file,
			DWORD fdwFormat = 0, DWORD dwBitsPerPixel = 0 ) ;
	// プレビュー画像を読み込む
	ESLError LoadPreviewImage
		( const char * pszFileName,
			DWORD dwWidth, DWORD dwHeight,
			DWORD dwPreviewFlags = pfForcePreview,
			EGL_SIZE * pOriginalSize = NULL,
			PFUNC_CALLBACK_DECODE pfnCallback = NULL,
			void * ptrCallbackData = NULL ) ;

public:
	// Window Bitmap ファイルを読み込む
	ESLError ReadBitmapFile
		( ESLFileObject & file,
			DWORD fdwFormat = 0, DWORD dwBitsPerPixel = 0 ) ;
	// Photoshop PSD ファイルを読み込む
	ESLError ReadPhotoshopPSDFile
		( ESLFileObject & file,
			DWORD fdwFormat = 0, DWORD dwBitsPerPixel = 0,
			DWORD dwLimitFrames = 0, DWORD dwLimitSize = 0 ) ;
	// AVI ファイルを読み込む
	ESLError LoadAviFile
		( const char * pszFileName,
			DWORD fdwFormat = 0, DWORD dwBitsPerPixel = 0,
			DWORD dwLimitFrames = 0, DWORD dwLimitSize = 0 ) ;
	// GDI+ を利用して読み込む
	ESLError LoadWithGDIplus
		( const char * pszFileName,
			DWORD fdwFormat = 0, DWORD dwBitsPerPixel = 0 ) ;
	ESLError ReadWithGDIplus
		( ESLFileObject & file,
			DWORD fdwFormat = 0, DWORD dwBitsPerPixel = 0 ) ;
	// 画像フォーマットを変換する
	ESLError ConvertFormatTo
		( DWORD fdwFormat, DWORD dwBitsPerPixel ) ;

public:
	// ビットマップデータから画像オブジェクトを作成する
	ESLError CreateFromBitmap
		( const BITMAPINFO * pbmi,
			void * ptrBitmap = NULL, DWORD dwFlags = 0 ) ;
	// 画像オブジェクトからパックドDIBを作成する
	BITMAPINFO * CreatePackedDIB( void ** ppBitmap = NULL ) const ;
	// GDI+ オブジェクトから画像オブジェクトへ変換する
	ESLError ConvertFromGDIplus
		( Gdiplus::GpBitmap * pbitmap,
			DWORD fdwFormat = 0, DWORD dwBitsPerPixel = 0 ) ;
	// 画像オブジェクトから GDI+ オブジェクトへ変換する
	ESLError CreateGDIplusBitmap( Gdiplus::GpBitmap *& pbitmap ) ;

public:
	// Windows Bitmap ファイルを書き出す
	ESLError WriteBitmapFile( ESLFileObject & file ) ;
	// Photoshop PSD ファイルを書き出す
	ESLError WritePhotoshopPSDFile( ESLFileObject & file ) ;
	// GDI+ を利用して書き出す
	ESLError SaveWithGDIplus
		( const char * pszFileName, const wchar_t * pwszMimeType ) ;
	ESLError WriteWithGDIplus
		( ESLFileObject & file, const wchar_t * pwszMimeType ) ;

protected:
	// MIME タイプから CLSID を取得
	static ESLError GetEncoderClsid
		( const wchar_t * pwszMimeType, CLSID & clsidEncoder ) ;

public:
	struct	GDIP_CODEC
	{
		EWideString	wstrCodecName ;
		EWideString	wstrDllName ;
		EWideString	wstrDescription ;
		EWideString	wstrFileExtension ;
		EWideString	wstrMimeType ;
	} ;
	// GDI+ で利用可能なデコーダーを列挙する
	static ESLError EnumGDIplusDecoderList
		( EObjArray<GDIP_CODEC> & lstCodec ) ;
	// GDI+ で利用可能なエンコーダーを列挙する
	static ESLError EnumGDIplusEncoderList
		( EObjArray<GDIP_CODEC> & lstCodec ) ;

public:
	// 初期化
	static void Initialize( bool fComMTA = true ) ;
	// 終了
	static void Close( void ) ;
	// GDI+ は有効か？
	static bool IsInstalledGDIplus( void ) ;

} ;


//////////////////////////////////////////////////////////////////////////////
// テクスチャ画像ライブラリ
//////////////////////////////////////////////////////////////////////////////

class	E3DTextureLibrary	: public	EWStrTagArray<EGLImage>
{
protected:
	E3DTextureLibrary *	m_pParent ;

public:
	// 構築関数
	E3DTextureLibrary( void ) : m_pParent( NULL ) { }
	// 消滅関数
	virtual ~E3DTextureLibrary( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( E3DTextureLibrary, EPtrArray )

public:
	// 関連親ライブラリを取得
	E3DTextureLibrary * GetParent( void ) const
		{
			return	m_pParent ;
		}
	// 関連親ライブラリを設定
	void SetParent( E3DTextureLibrary * pParent )
		{
			m_pParent = pParent ;
		}
	// テクスチャ画像を取得
	PEGL_IMAGE_INFO GetTextureAs
		( const wchar_t * pwszName, bool fSearchParent = true ) const ;
	// 画像ポインタからテクスチャ名を取得
	ESLError GetTextureName
		( EWideString & wstrName,
			PEGL_IMAGE_INFO pTexture, bool fSearchParent = true ) const ;

} ;


//////////////////////////////////////////////////////////////////////////////
// 表面属性ライブラリ
//////////////////////////////////////////////////////////////////////////////

class	E3DSurfaceLibrary	: public	EWStrTagArray<E3D_SURFACE_ATTRIBUTE>
{
protected:
	E3DSurfaceLibrary *	m_pParent ;

public:
	// 構築関数
	E3DSurfaceLibrary( void ) : m_pParent( NULL ) { }
	// 消滅関数
	virtual ~E3DSurfaceLibrary( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( E3DSurfaceLibrary, EPtrArray )

public:
	// 関連親ライブラリを取得
	E3DSurfaceLibrary * GetParent( void ) const
		{
			return	m_pParent ;
		}
	// 関連親ライブラリを設定
	void SetParent( E3DSurfaceLibrary * pParent )
		{
			m_pParent = pParent ;
		}
	// 属性を取得する
	E3D_SURFACE_ATTRIBUTE * GetAttributeAs
		( const wchar_t * pszName, bool fSearchParent = true ) const ;
	// 属性ポインタから属性名を取得
	ESLError GetAttributeName
		( EWideString & wstrName,
			E3D_SURFACE_ATTRIBUTE * pAttr, bool fSearchParent = true ) const ;

} ;


//////////////////////////////////////////////////////////////////////////////
// モデルオブジェクト
//////////////////////////////////////////////////////////////////////////////

class	E3DPolygonModel	: public	ESLObject
{
public:
	// 構築関数
	E3DPolygonModel( void ) ;
	// 消滅関数
	virtual ~E3DPolygonModel( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( E3DPolygonModel, ESLObject )

protected:
	E3DTextureLibrary			m_txlib ;
	E3DSurfaceLibrary			m_sflib ;

	unsigned int				m_nVertexCount ;
	PE3D_VECTOR4				m_pVertexes ;
	PE3D_VECTOR4				m_pVertexesBuf ;
	unsigned int				m_nNormalCount ;
	PE3D_VECTOR4				m_pNormals ;
	PE3D_VECTOR4				m_pNormalsBuf ;
	EPtrObjArray<E3D_PRIMITIVE_POLYGON>
								m_lstPrimitives ;

public:
	// モデルデータを読み込む
	virtual ESLError ReadModel( ESLFileObject & file ) ;
protected:
	// テクスチャデータを読み込む
	virtual ESLError ReadTextureRecord( EMCFile & file ) ;
	// 表面属性データを読み込む
	virtual ESLError ReadSurfaceRecord( EMCFile & file ) ;
	// モデルデータを読み込む
	virtual ESLError ReadModelRecord( EMCFile & file ) ;
	// ユーザー定義のレコードを読み込む
	virtual ESLError ReadUserRecord( EMCFile & file, UINT64 idRec ) ;

public:
	// モデルデータを書き出す
	ESLError WriteModel( ESLFileObject & file ) ;
protected:
	// テクスチャデータを書き出す
	virtual ESLError WriteTextureRecord( EMCFile & file ) ;
	// 表面属性データを書き出す
	virtual ESLError WriteSurfaceRecord( EMCFile & file ) ;
	// モデルデータを書き出す
	virtual ESLError WriteModelRecord( EMCFile & file ) ;
	// ユーザー定義のレコードを書き出す
	virtual ESLError WriteUserRecord( EMCFile & file ) ;

public:
	// モデルデータを削除する
	virtual void DeleteContents( void ) ;
	// テクスチャ画像ライブラリを参照する
	E3DTextureLibrary & TextureLibrary( void )
		{
			return	m_txlib ;
		}
	// 表面属性ライブラリを参照する
	E3DSurfaceLibrary & SurfaceLibrary( void )
		{
			return	m_sflib ;
		}

public:
	// 頂点の数を取得する
	unsigned int GetVertexCount( void ) const
		{
			return	m_nVertexCount ;
		}
	// 頂点バッファのアドレスを取得する
	PE3D_VECTOR4 GetVertexList( unsigned int nIndex = 0 )
		{
			ESLAssert( m_pVertexes != NULL ) ;
			ESLAssert( nIndex < m_nVertexCount ) ;
			return	m_pVertexes + nIndex ;
		}
	PE3D_VECTOR4 GetVertexBuffer( void )
		{
			return	m_pVertexesBuf ;
		}
	// 法線の数を取得する
	unsigned int GetNormalCount( void ) const
		{
			return	m_nNormalCount ;
		}
	// 法線バッファのアドレスを取得する
	PE3D_VECTOR4 GetNormalList( unsigned int nIndex = 0 )
		{
			ESLAssert( m_pNormals != NULL ) ;
			ESLAssert( nIndex < m_nNormalCount ) ;
			return	m_pNormals + nIndex ;
		}
	PE3D_VECTOR4 GetNormalBuffer( void )
		{
			return	m_pNormalsBuf ;
		}
	// 頂点バッファを確保する
	virtual void AllocateVertexBuffer( unsigned int nCount ) ;
	// 法線バッファを確保する
	virtual void AllocateNormalBuffer( unsigned int nCount ) ;

public:
	// プリミティブ数を取得
	unsigned int GetPrimitiveCount( void ) const
		{
			return	m_lstPrimitives.GetSize( ) ;
		}
	// プリミティブを取得
	E3D_PRIMITIVE_POLYGON * GetPrimitiveAt( unsigned int nIndex )
		{
			return	m_lstPrimitives.GetAt( nIndex ) ;
		}
	// プリミティブ配列を取得
	PE3D_PRIMITIVE_POLYGON * const GetPrimitiveList( void ) const
		{
			return	m_lstPrimitives.GetData( ) ;
		}
	// ポリゴンプリミティブを追加
	void AddPolygon
		( DWORD dwTypeFlag, PE3D_SURFACE_ATTRIBUTE pSurfAttr,
			DWORD dwVertexCount, const E3D_PRIMITIVE_VERTEX * pVertexes ) ;
	// プリミティブを追加
	void AddPrimitives
		( DWORD dwPrimitiveCount,
			const PE3D_PRIMITIVE_POLYGON * ppPrimitives ) ;
	// プリミティブを削除
	void RemovePrimitives( unsigned int nFirst, unsigned int nCount ) ;
	// プリミティブを全て削除
	void RemoveAllPrimitive( void ) ;
	// 画像モデルを作成する
	void CreateImagePolygon
		( PEGL_IMAGE_INFO pImage,
			PCEGL_RECT pView = NULL,
			const E3D_VECTOR_2D * pCenter = NULL,
			PCE3D_SURFACE_ATTRIBUTE pSurfAttr = NULL,
			REAL32 rFogDeepness = 0.0F,
			const EGL_PALETTE * pFogColor = NULL ) ;
	// プリミティブデータのバイト数を計算する
	static DWORD CalcPrimitiveDataSize
		( const E3D_PRIMITIVE_POLYGON * pPrimitive ) ;

	friend	E3DModelJoint ;
} ;


//////////////////////////////////////////////////////////////////////////////
// モデルジョイント
//////////////////////////////////////////////////////////////////////////////

class	E3DModelJoint	: public	ESLObject
{
public:
	E3DModelJoint *	m_parent ;		// 親ジョイント

	E3D_REV_MATRIX	m_rvmat ;		// 回転行列（階層化されたパラメータ）
	E3DVector		m_vmove ;		// 平行移動

	EPtrObjArray<E3DPolygonModel>
					m_models ;		// モデルデータへの参照リスト
	EObjArray<E3DModelJoint>
					m_joints ;		// サブジョイント

protected:
	E3D_REV_MATRIX	m_rvmatParam ;	// 回転行列（階層化されていないパラメータ）
	E3DVector		m_vmoveParam ;	// 平行移動

public:
	// 構築関数
	E3DModelJoint( void ) ;
	E3DModelJoint( E3DPolygonModel * pmodel ) ;
	// 消滅関数
	virtual ~E3DModelJoint( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( E3DModelJoint, ESLObject )

public:
	// ジョイント生成
	virtual E3DModelJoint * CreateJoint( void ) const ;
	// パラメータ反映
	virtual void RefreshJoint( void ) ;
	// ジョイント回転処理
	virtual void TransformJoint( const E3DModelJoint & mjParent ) ;
	// モデル回転処理
	virtual void TransformModel
		( HEGL_RENDER_POLYGON hRender, E3DPolygonModel & model ) ;
	// 全てのジョイントを削除
	void DeleteContents( void ) ;
	// ジョイント複製
	void CopyModelJoint( const E3DModelJoint & model ) ;

public:
	// 基準座標
	E3DVector & Position( void )
		{
			return	m_vmoveParam ;
		}
	// 変換行列
	E3D_REV_MATRIX & Matrix( void )
		{
			return	m_rvmatParam ;
		}
	// 変換行列を初期化
	void InitializeMatrix( void )
		{
			m_rvmatParam.InitializeMatrix( E3DVector( 1, 1, 1 ) ) ;
		}
	// ｘ軸回転
	void RevolveOnX( double rDeg ) ;
	// ｙ軸回転
	void RevolveOnY( double rDeg ) ;
	// ｚ軸回転
	void RevolveOnZ( double rDeg ) ;
	// ベクトルの方向に向く
	void RevolveByAngleOn( const E3D_VECTOR & angle )
		{
			m_rvmatParam.RevolveByAngleOn( angle ) ;
		}
	void RevolveForAngle( const E3D_VECTOR & angle )
		{
			m_rvmatParam.RevolveForAngle( angle ) ;
		}
	// 拡大
	void MagnifyByVector( const E3D_VECTOR & vector )
		{
			m_rvmatParam.MagnifyByVector( vector ) ;
		}

public:
	// モデルデータ追加
	void AddModelRef( E3DPolygonModel * pmodel )
		{
			m_models.Add( pmodel ) ;
		}
	// モデルデータ配列を参照
	EPtrObjArray<E3DPolygonModel> & ModelList( void )
		{
			return	m_models ;
		}
	// ジョイント追加
	void AddSubJoint( E3DModelJoint * pJoint )
		{
			pJoint->m_parent = this ;
			m_joints.Add( pJoint ) ;
		}
	// ジョイント配列を参照
	EObjArray<E3DModelJoint> & JointList( void )
		{
			return	m_joints ;
		}

} ;


//////////////////////////////////////////////////////////////////////////////
// 骨組み込みモデルオブジェクト
//////////////////////////////////////////////////////////////////////////////

class	E3DBonePolygonModel	: public	E3DPolygonModel
{
public:
	// 構築関数
	E3DBonePolygonModel( void ) ;
	// 消滅関数
	virtual ~E3DBonePolygonModel( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( E3DBonePolygonModel, E3DPolygonModel )

public:
	// ボーンジョイント
	class	E3DBoneJoint	: public	E3DModelJoint
	{
	public:
		// 構築関数
		E3DBoneJoint( void ) ;
		// 消滅関数
		virtual ~E3DBoneJoint( void ) ;
		// クラス情報
		DECLARE_CLASS_INFO( E3DBonePolygonModel::E3DBoneJoint, E3DModelJoint )
	public:
		EWideString		m_wstrName ;		// ボーンの名前
		E3DVector		m_vCenter ;			// ボーンの基準点
		unsigned int	m_iRefVertex ;		// 頂点参照
		unsigned int	m_nRefVertexCount ;
		REAL32 *		m_pVertexApply ;
		PE3D_VECTOR4	m_pVertexBuf ;
		unsigned int	m_iRefNormal ;		// 法線参照
		unsigned int	m_nRefNormalCount ;
		REAL32 *		m_pNormalApply ;
		PE3D_VECTOR4	m_pNormalBuf ;
	public:
		// ジョイント生成
		virtual E3DModelJoint * CreateJoint( void ) const ;
		// パラメータ反映
		virtual void RefreshJoint( void ) ;
		// ジョイント回転処理
		virtual void TransformJoint( const E3DModelJoint & mjParent ) ;
		// モデル回転処理
		virtual void TransformModel
			( HEGL_RENDER_POLYGON hRender, E3DPolygonModel & model ) ;
		// 全てのジョイントを削除
		void DeleteContents( void ) ;
	public:
		// ボーンを読み込む
		ESLError ReadBoneData( ESLFileObject & file ) ;
		// ボーンを書き出す
		ESLError WriteBoneData( ESLFileObject & file ) ;
		// 頂点用バッファ確保
		void AllocateVertexBuffer
			( unsigned int iFirst, unsigned int nCount ) ;
		// 法線用バッファ確保
		void AllocateNormalBuffer
			( unsigned int iFirst, unsigned int nCount ) ;
		// ボーンを名前で検索
		E3DBoneJoint * FindBoneAs( const wchar_t * pwszName ) ;
	} ;

public:
	EObjArray<E3DBoneJoint>
					m_lstBones ;	// ボーン配列（ルートに直接属するもの）
	PE3D_VECTOR4	m_pOrgVertexes ;
	PE3D_VECTOR4	m_pOrgNormals ;

public:
	// モデルデータを削除する
	virtual void DeleteContents( void ) ;
	// 頂点バッファを確保する
	virtual void AllocateVertexBuffer( unsigned int nCount ) ;
	// 法線バッファを確保する
	virtual void AllocateNormalBuffer( unsigned int nCount ) ;

public:
	// モデルデータを読み込む
	virtual ESLError ReadModel( ESLFileObject & file ) ;
protected:
	// ユーザー定義のレコードを読み込む
	virtual ESLError ReadUserRecord( EMCFile & file, UINT64 idRec ) ;
public:
	// モデルデータを書き出す
	ESLError WriteModel( ESLFileObject & file ) ;
protected:
	// ユーザー定義のレコードを書き出す
	virtual ESLError WriteUserRecord( EMCFile & file ) ;

public:
	// ボーンの現在のパラメータをモデルに反映する
	virtual void TransformAccordingAsBone( void ) ;
	// ボーンを検索する
	E3DBoneJoint * FindBoneAs( const wchar_t * pwszName ) ;

} ;


//////////////////////////////////////////////////////////////////////////////
// カメラオブジェクト
//////////////////////////////////////////////////////////////////////////////

class	E3DViewPointJoint	: public	E3DModelJoint
{
public:
	// 構築関数
	E3DViewPointJoint( void ) ;
	// 消滅関数
	virtual ~E3DViewPointJoint( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( E3DViewPointJoint, E3DModelJoint )

protected:
	E3DVector		m_vViewAngle ;		// 視線ベクトル
	E3DVector		m_vViewPoint ;		// 視点
	double			m_rRevAngleZ ;		// ｚ軸回転

public:
	// パラメータ反映
	virtual void RefreshJoint( void ) ;

public:
	// 視線ベクトルを取得
	const E3DVector & GetViewAngle( void ) const
		{
			return	m_vViewAngle ;
		}
	// 視点を取得
	const E3DVector & GetViewPoint( void ) const
		{
			return	m_vViewPoint ;
		}
	// z 軸回転角度を取得
	double GetRevolveZ( void ) const
		{
			return	m_rRevAngleZ ;
		}
	// 注視点設定
	void SetTarget( const E3D_VECTOR & vTarget ) ;
	// 視線ベクトルを設定
	void SetViewAngle( const E3D_VECTOR & vViewAngle ) ;
	// 視点を設定
	void SetViewPoint( const E3D_VECTOR & vViewPoint ) ;
	// ｚ軸回転角度を設定 [deg]
	void SetRevolveZ( double rDegAngle ) ;

} ;


//////////////////////////////////////////////////////////////////////////////
// カメラ補完曲線
//////////////////////////////////////////////////////////////////////////////

class	E3DViewAngleCurve	: public	ESLObject
{
public:
	// 構築関数
	E3DViewAngleCurve( void ) ;
	// 消滅関数
	virtual ~E3DViewAngleCurve( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( E3DViewAngleCurve, ESLObject )

public:
	// 補完フラグ
	enum	CurveType
	{
		ctOnlyAngle		= 0x0001
	} ;

protected:
	E3DVector	m_vTarget[2] ;
	E3DVector	m_vViewPoint[2] ;
	double		m_rRevAngleZ[2] ;

public:
	// カメラアングル設定
	void SetViewPoint
		( int nIndex, const E3D_VECTOR & vViewPoint,
			const E3D_VECTOR & vTarget, double rRevAngleZ = 0.0 ) ;
	// カメラアングル取得
	void GetViewPoint
		( double t, E3D_VECTOR & vViewPoint,
			E3D_VECTOR & vViewAngle, double & rRevAngleZ, DWORD dwFlags = 0 ) ;
	// カメラアングル設定
	void SetViewCurveFor
		( E3DViewPointJoint & vpjoint, double t, DWORD dwFlags = 0 ) ;

} ;


//////////////////////////////////////////////////////////////////////////////
// レンダリングオブジェクト
//////////////////////////////////////////////////////////////////////////////

class	E3DRenderPolygon	: public	ESLObject
{
public:
	// 構築関数
	E3DRenderPolygon( void ) ;
	// 消滅関数
	virtual ~E3DRenderPolygon( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( E3DRenderPolygon, ESLObject )

protected:
	// スレッドメッセージ
	enum	RenderThreadMessage
	{
		rtmQuit		= WM_QUIT,
		rtmRender	= WM_USER + 1
	} ;

public:
	// 列挙型
	enum	MultiRenderingFlag
	{
		mrfAuto,
		mrfSingle,
		mrfDual
	} ;

protected:
	HEGL_RENDER_POLYGON		m_hRenderPoly ;		// 主レンダリングオブジェクト
	DWORD					m_dwSortingFlags ;	// ソートフラグ

	HESLHEAP				m_hLocalHeap ;		// ローカルヒープ
	HSTACKHEAP				m_hStackHeap ;		// スタック式ヒープ

	DWORD					m_dwPolyCount ;		// ポリゴンエントリ数
	DWORD					m_dwPolyLimit ;		// ポリゴンエントリ限界数
	PE3D_POLYGON_ENTRY *	m_pPolyEntries ;	// ポリゴンエントリ

	E3DViewPointJoint		m_vpjView ;			// カメラオブジェクト

	PEGL_IMAGE_INFO			m_pDstImage ;		// レンダリング情報
	PEGL_IMAGE_INFO			m_pZBuffer ;
	EGL_RECT				m_rectDst ;
	E3D_VECTOR				m_vScreenPos ;

	MultiRenderingFlag		m_mrfRenderingFlag ;
	HEGL_RENDER_POLYGON		m_hPrimaryRender ;
	HEGL_RENDER_POLYGON		m_hSecondaryRender ;

	HANDLE					m_hThread ;			// レンダリング用スレッド
	DWORD					m_dwThreadID ;
	HANDLE					m_hRenderEvent ;	// レンダリング完了通知

public:
	// 初期化
	virtual ESLError Initialize
		( PEGL_IMAGE_INFO pDstImage, PCEGL_RECT pClipRect,
			PEGL_IMAGE_INFO pZBuffer, PCE3D_VECTOR pScreenPos,
			DWORD dwHeapSize = 0x100000, DWORD dwPolyLimit = 0x8000,
			MultiRenderingFlag mrfFlag = mrfAuto ) ;
	// リソース開放
	virtual ESLError Release( void ) ;
	// マルチプロセッサ対応取得
	MultiRenderingFlag GetMultiRenderingFlag( void ) const
		{
			return	m_mrfRenderingFlag ;
		}

public:
	// モデル追加
	virtual ESLError AddModel
		( E3DModelJoint & model,
			const E3D_COLOR * pColor = NULL,
			unsigned int nTransparency = 0 ) ;
	// プリミティブ追加
	virtual ESLError AddPrimitive
		( const E3D_PRIMITIVE_POLYGON * pPrimitive,
			const E3D_COLOR * pColor = NULL,
			unsigned int nTransparency = 0 ) ;
	// レンダリング準備
	virtual ESLError PrepareRendering( void ) ;
	// レンダリング実行
	virtual ESLError
		RenderAllPolygon( HEGL_RENDER_POLYGON hRenderPoly = NULL ) ;
	// ポリゴンを消去
	virtual ESLError FlushAllPolygon( void ) ;

protected:
	// レンダリングスレッド
	static DWORD WINAPI RenderThreadProc( LPVOID param ) ;
	DWORD RenderingThread( void ) ;

public:
	// カメラオブジェクト取得
	E3DViewPointJoint & ViewPoint( void )
		{
			return	m_vpjView ;
		}
	// カメラ設定
	void SetViewPoint
		( const E3D_VECTOR & vViewPoint,
			const E3D_VECTOR & vTarget, double rDegAngle )
		{
			m_vpjView.SetViewPoint( vViewPoint ) ;
			m_vpjView.SetTarget( vTarget ) ;
			m_vpjView.SetRevolveZ( rDegAngle ) ;
			m_vpjView.RefreshJoint( ) ;
		}
	// スクリーン座標取得
	const E3D_VECTOR & GetScreenPos( void ) const
		{
			return	m_vScreenPos ;
		}
	// レンダリングオブジェクト取得
	operator HEGL_RENDER_POLYGON ( void ) const
		{
			return	m_hRenderPoly ;
		}
	// 描画オブジェクト取得
	HEGL_DRAW_IMAGE GetDrawImage( void )
		{
			ESLAssert( m_hRenderPoly != NULL ) ;
			return	m_hRenderPoly->GetDrawImage( ) ;
		}
	// 機能フラグを変更
	void ModifyFunctionFlags( DWORD dwAddFlags, DWORD dwRemoveFlags )
		{
			ESLAssert( m_hRenderPoly != NULL ) ;
			m_hRenderPoly->SetFunctionFlags
				( (m_hRenderPoly->GetFunctionFlags()
							& ~dwRemoveFlags) | dwAddFlags ) ;
		}
	// 機能フラグを取得
	DWORD GetFunctionFlags( void ) const
		{
			ESLAssert( m_hRenderPoly != NULL ) ;
			return	m_hRenderPoly->GetFunctionFlags( ) ;
		}
	// 機能フラグを設定
	DWORD SetFunctionFlags( DWORD dwFlags )
		{
			ESLAssert( m_hRenderPoly != NULL ) ;
			return	m_hRenderPoly->SetFunctionFlags( dwFlags ) ;
		}
	// Z クリップ値を設定
	ESLError SetZClipRange( REAL32 rMin, REAL32 rMax )
		{
			ESLAssert( m_hRenderPoly != NULL ) ;
			return	m_hRenderPoly->SetZClipRange( rMin, rMax ) ;
		}
	// ライトを設定
	ESLError SetLightEntries
			( unsigned int nLightCount, PCE3D_LIGHT_ENTRY pLightEntries ) ;
	// 最小外接矩形を取得
	ESLError GetExternalRect( EGL_RECT * pExtRect ) const
		{
			ESLAssert( m_hRenderPoly != NULL ) ;
			return	m_hRenderPoly->GetExternalRect
						( pExtRect, (PCE3D_POLYGON_ENTRY*)
										m_pPolyEntries, m_dwPolyCount ) ;
		}
	// ソートフラグを取得する
	DWORD GetSortingFlags( void ) const
		{
			return	m_dwSortingFlags ;
		}
	// ソートフラグを設定する
	void SetSortingFlags( DWORD dwSortingFlags )
		{
			m_dwSortingFlags = dwSortingFlags ;
		}

} ;


#endif	//	!defined(COMPACT_NOA_DECODER)

#endif
