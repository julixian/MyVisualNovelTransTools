
/*****************************************************************************
                         E R I S A - L i b r a r y
 -----------------------------------------------------------------------------
   Copyright (C) 2003-2004 Leshade Entis, Entis-soft. All rights reserved.
 *****************************************************************************/


#if	!defined(__ERISA_IMAGE_H__)
#define	__ERISA_IMAGE_H__	1


/*****************************************************************************
                            画像展開オブジェクト
 *****************************************************************************/

class	ERISADecoder	: public	ESLObject
{
protected:
	ERI_INFO_HEADER			m_eihInfo ;				// 画像情報ヘッダ

	// 展開用パラメータ
	UINT					m_nBlockSize ;			// ブロッキングサイズ
	ULONG					m_nBlockArea ;			// ブロック面積
	ULONG					m_nBlockSamples ;		// ブロックのサンプル数
	UINT					m_nChannelCount ;		// チャネル数
	ULONG					m_nWidthBlocks ;		// 画像の幅（ブロック数）
	ULONG					m_nHeightBlocks ;		// 画像の高さ（ブロック数）

	// 展開先ブロックパラメータ
	BYTE *					m_ptrDstBlock ;			// 出力先アドレス
	LONG					m_nDstLineBytes ;		// 出力先ライン長
	UINT					m_nDstPixelBytes ;		// 1 ピクセルのバイト数
	UINT					m_nDstWidth ;			// 出力先ブロック幅
	UINT					m_nDstHeight ;			// 出力先ブロック高
	DWORD					m_fdwDecFlags ;			// 復号フラグ

	// 可逆展開用バッファ
	int						m_fEnhancedMode ;		// 拡張モードフラグ
	PBYTE					m_ptrOperations ;		// オペレーションテーブル
	SBYTE *					m_ptrColumnBuf ;		// 列バッファ
	SBYTE *					m_ptrLineBuf ;			// 行バッファ
	SBYTE *					m_ptrDecodeBuf ;		// 展開バッファ
	SBYTE *					m_ptrArrangeBuf ;		// 再配列用バッファ
	PINT					m_pArrangeTable[4] ;	// 再配列用テーブル

	// 非可逆展開用バッファ
	UINT					m_nBlocksetCount ;		// サブブロック数
	REAL32 *				m_ptrVertBufLOT ;		// 重複変換用バッファ
	REAL32 *				m_ptrHorzBufLOT ;
	REAL32 *				m_ptrBlocksetBuf[16] ;	// ブロックセットバッファ
	REAL32 *				m_ptrMatrixBuf ;
	REAL32 *				m_ptrIQParamBuf ;		// 逆量子化パラメータ
	BYTE *					m_ptrIQParamTable ;

	SBYTE *					m_ptrBlockLineBuf ;		// ブロック行中間バッファ
	SBYTE *					m_ptrNextBlockBuf ;
	SBYTE *					m_ptrImageBuf ;			// 画像信号バッファ
	SBYTE *					m_ptrYUVImage ;			// YUV 画像出力バッファ
	LONG					m_nYUVLineBytes ;		// YUV 画像ライン長
	UINT					m_nYUVPixelBytes ;		// YUV ピクセルのバイト数

	SBYTE *					m_ptrMovingVector ;		// 動き補償ベクトル
	SBYTE *					m_ptrMoveVecFlags ;
	PVOID *					m_ptrMovePrevBlocks ;	// 参照ブロックへのポインタ
	PVOID *					m_ptrNextPrevBlocks ;
	PEGL_IMAGE_INFO			m_pPrevImageRef ;		// 直前フレームへの参照
	SDWORD					m_dwPrevLineBytes ;		// 直前フレームラインステップ
	PEGL_IMAGE_INFO			m_pNextImageRef ;		// 直後フレームへの参照
	SDWORD					m_dwNextLineBytes ;		// 直後フレームラインステップ
	PEGL_IMAGE_INFO			m_pFilterImageBuf ;		// フィルタ処理用バッファ参照

	ERINA_HUFFMAN_TREE *	m_pHuffmanTree ;		// ハフマン木
	ERISA_PROB_MODEL *		m_pProbERISA ;			// 統計情報

public:
	typedef	void (ERISADecoder::*PTR_PROCEDURE)( void ) ;

protected:
	static const PTR_PROCEDURE	m_pfnColorOperation[0x10] ;

public:
	// 構築関数
	ERISADecoder( void ) ;
	// 消滅関数
	virtual ~ERISADecoder( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( ERISADecoder, ESLObject )

public:
	enum	DecodeFlag
	{
		dfTopDown		= 0x0001,
		dfDifferential	= 0x0002,
		dfQuickDecode	= 0x0100,
		dfQualityDecode	= 0x0200,
		dfNoLoopFilter	= 0x0400,
		dfUseLoopFilter	= 0x0800,
		dfPreviewDecode	= 0x1000
	} ;
	// 初期化（パラメータの設定）
	virtual ESLError Initialize( const ERI_INFO_HEADER & infhdr ) ;
	// 終了（メモリの解放など）
	virtual void Delete( void ) ;
	// 画像を展開
	virtual ESLError DecodeImage
		( const EGL_IMAGE_INFO & dstimginf,
			ERISADecodeContext & context, DWORD fdwFlags = dfTopDown ) ;
	// 直前フレームへの参照を設定
	void SetRefPreviousFrame
		( PEGL_IMAGE_INFO pPrevFrame, PEGL_IMAGE_INFO pNextFrame = NULL ) ;
	// フィルタ処理された画像を受け取る
	PEGL_IMAGE_INFO GetFilteredImageBuffer( void ) const
		{
			return	m_pFilterImageBuf ;
		}
	// フィルタ処理画像を受け取るバッファを設定する
	void SetFilteredImageBuffer( PEGL_IMAGE_INFO pImageBuf ) ;

protected:
	// 展開進行状況通知関数
	virtual ESLError OnDecodedBlock
		( LONG line, LONG column, const EGL_IMAGE_RECT & rect ) ;

protected:
	// 可逆画像展開
	ESLError DecodeLosslessImage
		( const EGL_IMAGE_INFO & imginf,
			ERISADecodeContext & context, DWORD fdwFlags ) ;
	// アレンジテーブルの初期化
	void InitializeArrangeTable( void ) ;
	// オペレーション実行
	void PerformOperation
		( DWORD dwOpCode, LONG nAllBlockLines, SBYTE * pNextLineBuf ) ;
	// カラーオペレーション関数群
	void ColorOperation0000( void ) ;
	void ColorOperation0101( void ) ;
	void ColorOperation0110( void ) ;
	void ColorOperation0111( void ) ;
	void ColorOperation1001( void ) ;
	void ColorOperation1010( void ) ;
	void ColorOperation1011( void ) ;
	void ColorOperation1101( void ) ;
	void ColorOperation1110( void ) ;
	void ColorOperation1111( void ) ;
	// グレイ画像／256色画像の出力
	void RestoreGray8( void ) ;
	// RGB 画像（15ビット）の出力
	void RestoreRGB16( void ) ;
	// RGB 画像の出力
	void RestoreRGB24( void ) ;
	// RGBA 画像の出力
	void RestoreRGBA32( void ) ;
	// RGB 画像の差分出力
	void LL_RestoreDeltaRGB24( void ) ;
	// RGBA 画像の差分出力
	void LL_RestoreDeltaRGBA32( void ) ;
	// 画像出力関数取得
	virtual PTR_PROCEDURE GetLLRestoreFunc
		( DWORD fdwFormatType, DWORD dwBitsPerPixel, DWORD fdwFlags ) ;

protected:
	// 非可逆画像展開
	ESLError DecodeLossyImage
		( const EGL_IMAGE_INFO & imginf,
			ERISADecodeContext & context, DWORD fdwFlags ) ;
	// ブロック単位での画面サイズを計算する
	void CalcImageSizeInBlocks( DWORD fdwTransformation ) ;
	// サンプリングテーブルの初期化
	void InitializeZigZagTable( void ) ;
	// 動きベクトルをセットアップする
	void SetupMovingVector( void ) ;
	// 逆量子化
	void ArrangeAndIQuantumize
		( const SBYTE * ptrSrcData, const SBYTE * ptrCoefficient ) ;
	// 逆 DCT 変換
	void MatrixIDCT8x8( REAL32 * ) ;
	// 逆 LOT 変換
	void MatrixILOT8x8( REAL32 * ptrVertBufLOT ) ;
	// 4:4:4 スケーリング
	void BlockScaling444( int x, int y, DWORD fdwFlags ) ;
	// 4:1:1 スケーリング
	void BlockScaling411( int x, int y, DWORD fdwFlags ) ;
	// 中間画像バッファに 1 チャネル書き出す
	void StoreYUVImageChannel( int xBlock, int yBlock, int iChannel ) ;
	// 中間画像バッファに 1 チャネル書き出す（スケーリング）
	void StoreYUVImageChannelX2( int xBlock, int yBlock, int iChannel ) ;
	// 中間バッファを YUV から RGB 形式へ変換
	void ConvertImageYUVtoRGB( DWORD fdwFlags ) ;
	// 動き補償を適用した上で画像を複製する
	void MoveImageWithVector( void ) ;
	// グレイ画像の出力
	void LS_RestoreGray8( void ) ;
	// RGB 画像の出力
	void LS_RestoreRGB24( void ) ;
	// RGBA 画像の出力
	void LS_RestoreRGBA32( void ) ;
	// グレイ画像の差分出力
	void LS_RestoreDeltaGray8( void ) ;
	// RGB 画像の差分出力
	void LS_RestoreDeltaRGB24( void ) ;
	// RGBA 画像の差分出力
	void LS_RestoreDeltaRGBA32( void ) ;
	// 画像出力関数取得
	virtual PTR_PROCEDURE GetLSRestoreFunc
		( DWORD fdwFormatType, DWORD dwBitsPerPixel, DWORD fdwFlags ) ;

} ;


/*****************************************************************************
                            画像圧縮オブジェクト
 *****************************************************************************/

class	ERISAEncoder	: public	ESLObject
{
protected:
	ERI_INFO_HEADER		m_eihInfo ;				// 画像情報ヘッダ

	// 圧縮パラメータ
	UINT				m_nBlockSize ;			// ブロッキングサイズ
	ULONG				m_nBlockArea ;			// ブロック面積
	ULONG				m_nBlockSamples ;		// ブロックのサンプル数
	UINT				m_nChannelCount ;		// チャネル数
	ULONG				m_nWidthBlocks ;		// 画像の幅（ブロック数）
	ULONG				m_nHeightBlocks ;		// 画像の高さ（ブロック数）

	// 入力ブロックパラメータ
	BYTE *				m_ptrSrcBlock ;			// 入力元アドレス
	LONG				m_nSrcLineBytes ;		// 入力元ライン長
	UINT				m_nSrcPixelBytes ;		// 1 ピクセルのバイト数
	UINT				m_nSrcWidth ;			// 入力元ブロック幅
	UINT				m_nSrcHeight ;			// 入力元ブロック高
	DWORD				m_fdwEncFlags ;			// 符号化フラグ

	// 可逆圧縮用パラメータ
	SBYTE *				m_ptrColumnBuf ;		// 列バッファ
	SBYTE *				m_ptrLineBuf ;			// 行バッファ
	SBYTE *				m_ptrEncodeBuf ;		// 圧縮バッファ
	SBYTE *				m_ptrArrangeBuf ;		// 再配列用バッファ
	PINT				m_pArrangeTable[4] ;	// 再配列用テーブル

	// 非可逆圧縮用バッファ
	UINT				m_nBlocksetCount ;		// サブブロック数
	REAL32 *			m_ptrVertBufLOT ;		// 重複変換用バッファ
	REAL32 *			m_ptrHorzBufLOT ;
	REAL32 *			m_ptrBlocksetBuf[36] ;	// ブロックセットバッファ
	REAL32 *			m_ptrMatrixBuf[16] ;	// 行列変換用バッファ
	REAL32 *			m_pQuantumizeScale[2] ;	// 量子化テーブル
	BYTE *				m_pQuantumizeTable ;

	LONG				m_nMovingVector ;		// 動き補償ベクトルフラグ
	BYTE *				m_pMoveVecFlags ;		// 動き補償ベクトルフラグ
	SBYTE *				m_pMovingVector ;		// 動き補償ベクトル
	INT					m_fPredFrameType ;		// フレームタイプ
	LONG				m_nIntraBlockCount ;	// イントラブロック数
	REAL32				m_rDiffDeflectBlock ;	// 平均差分偏差値
	REAL32				m_rMaxDeflectBlock ;	// 最大差分偏差値

	SBYTE *				m_ptrCoefficient ;		// 量子化パラメータ出力バッファ
	BYTE *				m_ptrImageDst ;			// 画像信号出力バッファ
	REAL32 *			m_ptrSignalBuf ;

	ERINA_HUFFMAN_TREE *	m_pHuffmanTree ;	// ハフマン木
	ERISA_PROB_MODEL *		m_pProbERISA ;		// 統計情報

public:
	typedef	void (ERISAEncoder::*PTR_PROCEDURE)( void ) ;

protected:
	static const PTR_PROCEDURE	m_pfnColorOperation[0x10] ;

public:
	enum	EncodeFlag
	{
		efTopDown		= 0x0001,
		efDifferential	= 0x0002,
		efNoMoveVector	= 0x0004,
		efBestCmpr		= 0x0000,
		efHighCmpr		= 0x0010,
		efNormalCmpr	= 0x0020,
		efLowCmpr		= 0x0030,
		efCmprModeMask	= 0x0030
	} ;
	enum	PresetParameter
	{
		ppClearQuality,
		ppHighQuality,
		ppAboveQuality,
		ppStandardQuality,
		ppBelowQuality,
		ppLowQuality,
		ppLowestQuality,
		ppMax
	} ;
	enum	ParameterFlag
	{
		pfUseLoopFilter		= 0x0001,
		pfLocalLoopFilter	= 0x0002
	} ;
	struct	PARAMETER
	{
		DWORD			m_dwFlags ;				// フラグ
		REAL32			m_rYScaleDC ;			// 輝度直流成分の量子化係数
		REAL32			m_rCScaleDC ;			// 色差直流成分の量子化係数
		REAL32			m_rYScaleLow ;			// 輝度低周波成分の量子化係数
		REAL32			m_rCScaleLow ;			// 色差低周波成分の量子化係数
		REAL32			m_rYScaleHigh ;			// 輝度高周波成分の量子化係数
		REAL32			m_rCScaleHigh ;			// 色差高周波成分の量子化係数
		int				m_nYThreshold ;			// 輝度成分の閾値
		int				m_nCThreshold ;			// 色差成分の閾値
		int				m_nYLPFThreshold ;		// 輝度成分 LPF 指標
		int				m_nCLPFThreshold ;		// 色差成分 LPF 指標
		int				m_nAMDFThreshold ;		// フレーム間最大差分閾値
		REAL32			m_rPFrameScale ;		// P フレームの量子化係数比
		REAL32			m_rBFrameScale ;		// B フレームの量子化係数比
		ULONG			m_nMaxFrameSize ;		// 最大バイト数
		ULONG			m_nMinFrameSize	;		// 最小バイト数

		PARAMETER( void ) ;
		void LoadPresetParam
			( PresetParameter ppIndex, ERI_INFO_HEADER & infhdr ) ;
	} ;

protected:
	PARAMETER			m_prmCmprOpt ;			// 圧縮パラメータ

public:
	// 構築関数
	ERISAEncoder( void ) ;
	// 消滅関数
	virtual ~ERISAEncoder( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( ERISAEncoder, ESLObject )

	// 初期化（パラメータの設定）
	virtual ESLError Initialize( const ERI_INFO_HEADER & infhdr ) ;
	// 終了（メモリの解放など）
	virtual void Delete( void ) ;
	// 画像を圧縮
	virtual ESLError EncodeImage
		( const EGL_IMAGE_INFO & srcimginf,
			ERISAEncodeContext & context, DWORD fdwFlags = efNormalCmpr ) ;
	// 圧縮オプションを設定
	void SetCompressionParameter( const PARAMETER & prmCmprOpt ) ;

protected:
	// 展開進行状況通知関数
	virtual ESLError OnEncodedBlock( LONG line, LONG column ) ;

public:
	// 動き補償パラメータを計算する
	ESLError ProcessMovingVector
		( const EGL_IMAGE_INFO & nextimg,
			const EGL_IMAGE_INFO & previmg, int & nAbsMaxDiff,
					const EGL_IMAGE_INFO * ppredimg2 = NULL ) ;
	// 動き補償パラメータをクリアする
	void ClearMovingVector( void ) ;
protected:
	// 動き補償ベクトルを計算し、予測画像との差分を計算する
	int PredictMovingVector
		( const EGL_IMAGE_INFO & nextimg,
			const EGL_IMAGE_INFO & previmg,
			int xBlock, int yBlock, EGL_POINT ptMoveVec[],
			int & nAbsMaxDiff, double & rDeflectBlock,
			PCEGL_IMAGE_INFO ppredimg = NULL ) ;
	// 動き補償ベクトルを計算する
	void SearchMovingVector
		( EGL_IMAGE_INFO & nextblock, EGL_IMAGE_INFO & predblock,
			const EGL_IMAGE_INFO & nextimg, const EGL_IMAGE_INFO & predimg,
						int xBlock, int yBlock, EGL_POINT & ptMoveVec ) ;
	// 画像ブロックの二乗偏差（合計）を求める
	static long int CalcSumDeflectBlock( const EGL_IMAGE_INFO & imgblock ) ;
	// 画像ブロックの差の二乗合計を求める
	static long int CalcSumSqrDifferenceBlock
		( const EGL_IMAGE_INFO & dstimg, const EGL_IMAGE_INFO & srcimg ) ;
	// 画像ブロックの輝度を半分にする
	static void MakeBlockValueHalf( const EGL_IMAGE_INFO & imgblock ) ;

protected:
	// フルカラー画像の圧縮
	ESLError EncodeLosslessImage
		( const EGL_IMAGE_INFO & imginf,
			ERISAEncodeContext & context, DWORD fdwFlags ) ;
	// サンプリングテーブルの初期化
	void InitializeSamplingTable( void ) ;
	// 差分処理
	void DifferentialOperation( LONG nAllBlockLines, SBYTE * pNextLineBuf ) ;
	// オペレーションコードを取得
	DWORD DecideOperationCode
		( DWORD fdwFlags, LONG nAllBlockLines, SBYTE * pNextLineBuf ) ;
	// カラーオペレーション関数群
	void ColorOperation0000( void ) ;
	void ColorOperation0001( void ) ;
	void ColorOperation0010( void ) ;
	void ColorOperation0011( void ) ;
	void ColorOperation0100( void ) ;
	void ColorOperation0101( void ) ;
	void ColorOperation0110( void ) ;
	void ColorOperation0111( void ) ;
	void ColorOperation1000( void ) ;
	void ColorOperation1001( void ) ;
	void ColorOperation1010( void ) ;
	void ColorOperation1011( void ) ;
	void ColorOperation1100( void ) ;
	void ColorOperation1101( void ) ;
	void ColorOperation1110( void ) ;
	void ColorOperation1111( void ) ;
	// グレイ画像のサンプリング
	void SamplingGray8( void ) ;
	// RGB 画像(15ビット)のサンプリング
	void SamplingRGB16( void ) ;
	// RGB 画像のサンプリング
	void SamplingRGB24( void ) ;
	// RGBA 画像のサンプリング
	void SamplingRGBA32( void ) ;
	// 画像をサンプリングする関数へのポインタを取得する
	virtual PTR_PROCEDURE GetLLSamplingFunc
		( DWORD fdwFormatType, DWORD dwBitsPerPixel, DWORD fdwFlags ) ;

protected:
	// フルカラー画像の圧縮
	ESLError EncodeLossyImage
		( const EGL_IMAGE_INFO & imginf,
			ERISAEncodeContext & context, DWORD fdwFlags ) ;
	// ブロック単位での画面サイズを計算する
	void CalcImageSizeInBlocks( DWORD fdwTransformation ) ;
	// サンプリングテーブルの初期化
	void InitializeZigZagTable( void ) ;
	// 量子化テーブルの生成
	void InitializeQuantumizeTable( double r = 1.0 ) ;
	// マクロブロックのサンプリング（4:4:4 形式）＆色空間変換
	void SamplingMacroBlock
		( int xBlock, int yBlock,
			int nLeftWidth, int nLeftHeight,
			DWORD dwBlockStepAddr, BYTE *& ptrSrcLineAddr,
			PTR_PROCEDURE pfnSamplingFunc ) ;
	// 半端領域に平均値を設定
	void FillBlockOddArea( DWORD dwFlags ) ;
	// 4:4:4 スケーリング
	void BlockScaling444( void ) ;
	// 4:1:1 スケーリング
	void BlockScaling411( void ) ;
	// DCT 変換を施す
	void MatrixDCT8x8( void ) ;
	// LOT 変換を施す
	void MatrixLOT8x8( REAL32 * ptrVertBufLOT ) ;
	// 量子化を施す
	void ArrangeAndQuantumize( SBYTE * ptrCoefficient, DWORD fdwFlags ) ;
	// 画像をサンプリングする関数へのポインタを取得する
	virtual PTR_PROCEDURE GetLSSamplingFunc
		( DWORD fdwFormatType, DWORD dwBitsPerPixel, DWORD fdwFlags ) ;

} ;


#endif
