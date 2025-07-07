
/*****************************************************************************
                         E R I S A - L i b r a r y
 -----------------------------------------------------------------------------
    Copyright (C) 2002-2003 Leshade Entis, Entis-soft. All rights reserved.
 *****************************************************************************/


#if	!defined(__ERISA_PLAY_H__)
#define	__ERISA_PLAY_H__	1

#if	!defined(COMPACT_NOA_DECODER)


/*****************************************************************************
                    MIOファイルストリーム再生オブジェクト
 *****************************************************************************/

class	MIODynamicPlayer	: public	ESLObject
{
public:
	// 構築関数
	MIODynamicPlayer( void ) ;
	// 消滅関数
	virtual ~MIODynamicPlayer( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( MIODynamicPlayer, ESLObject )

protected:
	//
	// レコード先読みオブジェクト
	class	EPreloadBuffer	: public	EMemoryFile
	{
	public:
		BYTE *			m_ptrBuffer ;
		ULONG			m_nKeySample ;
		MIO_DATA_HEADER	m_miodh ;
	public:
		// 構築関数
		EPreloadBuffer( DWORD dwLength ) ;
		// 消滅関数
		virtual ~EPreloadBuffer( void ) ;
	} ;
	//
	// キーフレームポイントオブジェクト
	class	EKeyPoint	: public	ESLObject
	{
	public:
		ULONG	m_nKeySample ;
		DWORD	m_dwRecOffset ;
	public:
		// 構築関数
		EKeyPoint( void ) { }
		EKeyPoint( const EKeyPoint & key )
			: m_nKeySample(key.m_nKeySample),
				m_dwRecOffset(key.m_dwRecOffset ) { }
	} ;
	//
	// スレッド用メッセージ
	enum	ThreadMessage
	{
		tmSeekSound	= WM_USER,
		tmQuit
	} ;

	// ERIアニメーションファイル
	ERIFile					m_erif ;
	// 展開オブジェクト
	ERISADecodeContext *	m_pmioc ;
	MIODecoder *			m_pmiod ;
	// 先読みスレッド
	HANDLE					m_hThread ;
	DWORD					m_idThread ;
	// 先読みキュー
	EObjArray<EPreloadBuffer>	m_queueSound ;
	HANDLE					m_hQueueFull ;		// 先読みキューが一杯
	HANDLE					m_hQueueNotEmpty ;	// 先読みキューが空でない
	HANDLE					m_hQueueSpace ;		// 先読みキューに読み込み可能
	unsigned int			m_nCurrentSample ;	// 現在読み込んでいるサンプル数
	// 音声シーク用キーポイント配列
	EObjArray<EKeyPoint>	m_listKeySample ;
	// 排他的処理
	CRITICAL_SECTION		m_cs ;

public:
	// MIO ファイルを開く
	ESLError Open( ESLFileObject * pFile, unsigned int nPreloadSize = 0 ) ;
	// MIO ファイルを閉じる
	void Close( void ) ;

	// 指定サンプルへ移動し、初めのブロックのデータを取得する
	virtual void * GetWaveBufferFrom
		( ULONG nSample, DWORD & dwBytes, DWORD & dwOffsetBytes ) ;
	// 次の音声データがストリームの先頭であるか？
	virtual bool IsNextDataRewound( void ) ;
	// 次の音声データを取得
	virtual void * GetNextWaveBuffer( DWORD & dwBytes ) ;
	// 音声バッファ確保
	virtual void * AllocateWaveBuffer( DWORD dwBytes ) ;
	// 音声データ破棄許可
	virtual void DeleteWaveBuffer( void * ptrWaveBuf ) ;
	// 音声展開オブジェクトを生成
	virtual MIODecoder * CreateMIODecoder( void ) ;

public:
	// ERIFile オブジェクトを取得する
	const ERIFile & GetERIFile( void ) const ;
	// チャネル数を取得する
	DWORD GetChannelCount( void ) const ;
	// サンプリング周波数を取得する
	DWORD GetFrequency( void ) const ;
	// サンプリングビット分解能を取得する
	DWORD GetBitsPerSample( void ) const ;
	// 全体の長さ（サンプル数）を取得する
	DWORD GetTotalSampleCount( void ) const ;

protected:
	// 先読みバッファを取得する
	EPreloadBuffer * GetPreloadBuffer( void ) ;
	// 先読みバッファに追加する
	void AddPreloadBuffer( EPreloadBuffer * pBuffer ) ;

protected:
	// スレッド関数
	static DWORD WINAPI ThreadProc( LPVOID parameter ) ;
	DWORD LoadingThreadProc( void ) ;
	// 音声データレコードを読み込む
	EPreloadBuffer * LoadSoundStream( unsigned int & nCurrentSample ) ;
	// キーフレームポイントを追加する
	void AddKeySample( const EKeyPoint & key ) ;
	// 指定のキーフレームを検索する
	EKeyPoint * SearchKeySample( unsigned int nKeySample ) ;
	// 指定のサンプルを含むブロックを読み込む
	void SeekKeySample( ULONG nSample, unsigned int & nCurrentSample ) ;

public:
	// 排他処理（クリティカルセクション）
	void Lock( void ) ;
	void Unlock( void ) ;

} ;


/*****************************************************************************
                    ERIアニメーションファイルオブジェクト
 *****************************************************************************/

class	ERIAnimation	: public	ESLObject
{
public:
	// 構築関数
	ERIAnimation( void ) ;
	// 消滅関数
	virtual ~ERIAnimation( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( ERIAnimation, ESLObject )

protected:
	//
	// レコード先読みオブジェクト
	class	EPreloadBuffer	: public	EMemoryFile
	{
	public:
		BYTE *	m_ptrBuffer ;
		ULONG	m_iFrameIndex ;
		UINT64	m_ui64RecType ;
	public:
		// 構築関数
		EPreloadBuffer( DWORD dwLength ) ;
		// 消滅関数
		virtual ~EPreloadBuffer( void ) ;
	} ;
	//
	// キーフレームポイント構造体
	class	EKeyPoint	: public	ESLObject
	{
	public:
		ULONG	m_iKeyFrame ;
		ULONG	m_dwSubSample ;
		DWORD	m_dwRecOffset ;
	public:
		// 構築関数
		EKeyPoint( void ) { }
		EKeyPoint( const EKeyPoint & key )
			: m_iKeyFrame(key.m_iKeyFrame),
				m_dwSubSample(key.m_dwSubSample),
				m_dwRecOffset(key.m_dwRecOffset ) { }
	} ;
	//
	// スレッド用メッセージ
	enum	ThreadMessage
	{
		tmSeekFrame	= WM_USER,
		tmSeekSound,
		tmQuit
	} ;
	//
	// フレームタイプ
	enum	FrameType
	{
		ftOtherData		= -1,		// 画像以外
		ftIntraFrame,				// 独立フレーム（I ピクチャ）
		ftPredictionalFrame,		// 差分フレーム（P ピクチャ）
		ftBidirectionalFrame		// 双差分フレーム（B ピクチャ）
	} ;

	// ERIアニメーションファイル
	bool					m_fTopDown ;
	bool					m_fWaveOutput ;
	bool					m_fWaveStreaming ;
	ERIFile					m_erif ;
	// 展開オブジェクト
	DWORD					m_fdwDecFlags ;
	ERISADecodeContext *	m_peric ;
	ERISADecoder *			m_perid ;
	ERISADecodeContext *	m_pmioc ;
	MIODecoder *			m_pmiod ;
	// 画像バッファ
	unsigned int			m_iCurrentFrame ;
	unsigned int			m_iDstBufIndex ;	// 直後フレームの指標
	unsigned int			m_nCacheBFrames ;	// 現在の先読みキューで
												// キャッシュされた B フレーム数
												// -1 の時には B フレームに
												// 対応していないフォーマット
	EGL_IMAGE_INFO *		m_pDstImage[5] ;	// 0,1 : I, P フレーム
												// 2   : B フレーム展開用
												// 3,4 : フィルタ処理用
	unsigned int			m_iDstFrameIndex[5] ;
												// m_pDstImage に対応する
												// フレーム番号
	// 先読みスレッド
	HANDLE					m_hThread ;
	DWORD					m_idThread ;
	unsigned int			m_iPreloadFrame ;
	unsigned long int		m_nPreloadWaveSamples ;
	// 画像先読みキュー
	unsigned int			m_nPreloadLimit ;
	EObjArray<EPreloadBuffer>	m_queueImage ;
	HANDLE					m_hQueueNotEmpty ;	// 先読みキューが空でない
	HANDLE					m_hQueueSpace ;		// 先読みキューに読み込み可能
	// フレームシーク用キーフレームポイント配列
	typedef	EObjArray<EKeyPoint>	EKeyPointList ;
	EKeyPointList			m_listKeyFrame ;
	EKeyPointList			m_listKeyWave ;
	// 排他的処理
	CRITICAL_SECTION		m_cs ;

protected:
	// 画像展開出力バッファ要求
	virtual EGL_IMAGE_INFO * CreateImageBuffer
		( DWORD format, SDWORD width, SDWORD height, DWORD bpp ) ;
	// 画像展開出力バッファ消去
	virtual void DeleteImageBuffer( EGL_IMAGE_INFO * peii ) ;
	// 画像展開オブジェクト生成
	virtual ERISADecoder * CreateERIDecoder( void ) ;
	// 音声展開オブジェクト生成
	virtual MIODecoder * CreateMIODecoder( void ) ;
	// 音声出力要求
	virtual bool RequestWaveOut
		( DWORD channels, DWORD frequency, DWORD bps ) ;
	// 音声出力終了
	virtual void CloseWaveOut( void ) ;
	// 音声データ出力
	virtual void PushWaveBuffer( void * ptrWaveBuf, DWORD dwBytes ) ;
public:
	// 音声バッファ確保
	virtual void * AllocateWaveBuffer( DWORD dwBytes ) ;
	// 音声データ破棄許可
	virtual void DeleteWaveBuffer( void * ptrWaveBuf ) ;
	// 音声ストリーミング開始
	virtual void BeginWaveStreaming( void ) ;
	// 音声ストリーミング終了
	virtual void EndWaveStreaming( void ) ;

public:
	// アニメーションファイルを開く
	ESLError Open
		( ESLFileObject * pFile,
			unsigned int nPreloadSize = 0, DWORD fdwFlags = 0 ) ;
	// アニメーションファイルを閉じる
	void Close( void ) ;

	// 先頭フレームへ移動
	ESLError SeekToBegin( void ) ;
	// 次のフレームへ移動
	ESLError SeekToNextFrame( int nSkipFrame = 0 ) ;
	// 指定のフレームに移動
	ESLError SeekToFrame( unsigned int iFrameIndex ) ;
	// 指定のフレームはキーフレームか？
	bool IsKeyFrame( unsigned int iFrameIndex ) ;
	// 最適なフレームスキップ数を取得する
	unsigned int GetBestSkipFrames( unsigned int nCurrentTime ) ;

protected:
	// フレームを展開する
	ESLError DecodeFrame( EPreloadBuffer * pFrame, DWORD fdwFlags = 0 ) ;
	// パレットテーブルを適用する
	void ApplyPaletteTable( EPreloadBuffer * pBuffer ) ;
	// 先読みバッファを取得する
	EPreloadBuffer * GetPreloadBuffer( void ) ;
	// 先読みバッファに追加する
	void AddPreloadBuffer( EPreloadBuffer * pBuffer ) ;
	// 指定のフレームが I, P, B ピクチャか判定する
	int GetFrameBufferType( EPreloadBuffer * pBuffer ) ;

public:
	// ERIFile オブジェクトを取得する
	const ERIFile & GetERIFile( void ) const ;
	// カレントフレームのインデックスを取得する
	unsigned int CurrentIndex( void ) const ;
	// カレントフレームの画像を取得
	const EGL_IMAGE_INFO * GetImageInfo( void ) const ;
	// パレットテーブル取得
	const EGL_PALETTE * GetPaletteEntries( void ) const ;
	// キーフレームを取得
	unsigned int GetKeyFrameCount( void ) const ;
	// 全フレーム数を取得
	unsigned int GetAllFrameCount( void ) const ;
	// 全アニメーション時間を取得
	unsigned int GetTotalTime( void ) const ;
	// フレーム番号から時間へ変換
	unsigned int FrameIndexToTime( unsigned int iFrameIndex ) const ;
	// 時間からフレーム番号へ変換
	unsigned int TimeToFrameIndex( unsigned int nMilliSec ) const ;

protected:
	// スレッド関数
	static DWORD WINAPI ThreadProc( LPVOID parameter ) ;
	DWORD LoadingThreadProc( void ) ;
	// 動画像ストリームを読み込む
	EPreloadBuffer * LoadMovieStream( unsigned int & iCurrentFrame ) ;
	// キーフレームポイントを追加する
	void AddKeyPoint( EKeyPointList & list, const EKeyPoint & key ) ;
	// 指定のキーフレームを検索する
	EKeyPoint * SearchKeyPoint
		( EKeyPointList & list, unsigned int iKeyFrame ) ;
	// 指定のフレームにシークする
	void SeekKeyPoint
		( EKeyPointList & list,
			unsigned int iFrame, unsigned int & iCurtrentFrame ) ;
	// 指定の音声データまでシークしてストリーミング出力する
	void SeekKeyWave
		( EKeyPointList & list, unsigned int iFrame ) ;

public:
	// 排他処理（クリティカルセクション）
	void Lock( void ) ;
	void Unlock( void ) ;

} ;


/*****************************************************************************
                  ERIアニメーションファイル出力オブジェクト
 *****************************************************************************/

 class	ERIAnimationWriter	: public	ESLObject
{
public:
	// 構築関数
	ERIAnimationWriter( void ) ;
	// 消滅関数
	virtual ~ERIAnimationWriter( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( ERIAnimationWriter, ESLObject )

public:
	//
	// ファイルタイプ
	enum	FileIdentity
	{
		fidImage,
		fidSound,
		fidMovie
	} ;

protected:
	//
	// 圧縮コンテキスト
	class	EEncodeContext	: public	ERISAEncodeContext
	{
	public:
		EStreamBuffer	m_buf ;
	public:
		// 構築関数
		EEncodeContext( void ) : ERISAEncodeContext( 0x10000 ) { }
		// 消滅関数
		virtual ~EEncodeContext( void ) ;
		// データ消去
		void Delete( void ) { m_buf.Delete( ) ; }
		// 次のデータを書き出す
		virtual ULONG WriteNextData( const BYTE * ptrBuffer, ULONG nBytes ) ;
	} ;
	//
	// スレッドメッセージ
	enum	ThreadMessage
	{
		tmEncodeImage	= WM_USER,
		tmQuit
	} ;

	// 書き出しステータス
	enum	WriterStatus
	{
		wsNotOpened,
		wsOpened,
		wsWritingHeader,
		wsWritingStream
	} ;
	WriterStatus			m_wsStatus ;		// ステータス
	// ファイルオブジェクト
	EMCFile					m_eriwf ;			// ファイルオブジェクト
	// ヘッダ情報
	DWORD					m_dwHeaderBytes ;	// ヘッダレコードのバイト数
	ERI_FILE_HEADER			m_efh ;				// ファイルヘッダ
	ERI_INFO_HEADER			m_prvwih ;			// プレビュー画像情報ヘッダ
	ERI_INFO_HEADER			m_eih ;				// 画像情報ヘッダ
	MIO_INFO_HEADER			m_mih ;				// 音声情報ヘッダ
	// フレーム番号
	bool					m_fWithSeqTable ;	// シーケンステーブル使用
	DWORD					m_dwKeyFrame ;		// キーフレーム
	DWORD					m_dwBidirectKey ;	// B ピクチャ間隔
	DWORD					m_dwKeyWave ;		// キーウェーブ
	DWORD					m_dwFrameCount ;	// 出力済みフレーム総数
	DWORD					m_dwWaveCount ;		// 出力済みウェーブ総数
	DWORD					m_dwDiffFrames ;	// 差分フレームオフセット
	// 音声出力情報
	DWORD					m_dwMioHeaderPos ;		// 音声情報ヘッダの位置
	DWORD					m_dwOutputWaveSamples ;	// 出力済みサンプル数
	// 圧縮オブジェクト
	EEncodeContext			m_eric1 ;
	EEncodeContext			m_eric2 ;
	ERISAEncoder *			m_perie1 ;
	ERISAEncoder *			m_perie2 ;
	ERISAEncodeContext *	m_pmioc ;
	MIOEncoder *			m_pmioe ;
	// 差分処理用バッファ
	EGL_IMAGE_INFO *		m_peiiLast ;
	EGL_IMAGE_INFO *		m_peiiNext ;
	EGL_IMAGE_INFO *		m_peiiBuf ;
	ENumArray<DWORD>		m_lstEncFlags ;
	EPtrObjArray<EGL_IMAGE_INFO>
							m_lstFrameBuf ;		// B ピクチャ用画像バッファ
	// 音声出力バッファ
	bool					m_fKeyWaveBlock ;
	EStreamBuffer			m_bufWaveBuffer ;
	DWORD					m_dwWaveBufSamples ;
	// 画像圧縮スレッド
	bool					m_fDualEncoding ;
	bool					m_fCompressSuccessed ;
	HANDLE					m_hCompressed ;
	HANDLE					m_hThread ;
	DWORD					m_idThread ;
	// 画像の圧縮パラメータ
	ERISAEncoder::PARAMETER	m_eriep_i ;
	ERISAEncoder::PARAMETER	m_eriep_p ;
	ERISAEncoder::PARAMETER	m_eriep_b ;
	// 音声の圧縮パラメータ
	MIOEncoder::PARAMETER	m_mioep ;

public:
	// ファイルを開く
	ESLError Open( ESLFileObject * pFile, FileIdentity fidType ) ;
	// ファイルを閉じる
	void Close( void ) ;

public:
	// ファイルヘッダを開く
	ESLError BeginFileHeader
		( DWORD dwKeyFrame, DWORD dwKeyWave, DWORD dwBidirectKey = 3 ) ;
	// プレビュー画像情報ヘッダを書き出す
	ESLError WritePreviewInfo( const ERI_INFO_HEADER & eih ) ;
	// 画像情報ヘッダを書き出す
	ESLError WriteEriInfoHeader( const ERI_INFO_HEADER & eih ) ;
	// 音声情報ヘッダを書き出す
	ESLError WriteMioInfoHeader( const MIO_INFO_HEADER & mih ) ;
	// 著作権情報を書き出す
	ESLError WriteCopyright( const void * ptrCopyright, DWORD dwBytes ) ;
	// コメントを書き出す
	ESLError WriteDescription( const void * ptrDescription, DWORD dwBytes ) ;
	// シーケンステーブルを書き出す
	ESLError WriteSequenceTable
		( ERIFile::SEQUENCE_DELTA * pSequence, DWORD dwLength ) ;
	// ファイルヘッダを閉じる
	void EndFileHeader( void ) ;

public:
	// 画像の圧縮パラメータを設定する
	void SetImageCompressionParameter
			( const ERISAEncoder::PARAMETER & eriep ) ;
	// 音声の圧縮パラメータを設定する
	void SetSoundCompressionParameter
			( const MIOEncoder::PARAMETER & mioep ) ;

public:
	// ストリームを開始する
	ESLError BeginStream( void ) ;
	// パレットテーブルを書き出す
	ESLError WritePaletteTable
		( const EGL_PALETTE * paltbl, unsigned int nLength ) ;
	// プレビュー画像を出力する
	ESLError WritePreviewData( const EGL_IMAGE_INFO & eii, DWORD fdwFlags ) ;
	// 音声データを出力する
	ESLError WriteWaveData( const void * ptrWaveBuf, DWORD dwSampleCount ) ;
	// 画像データを出力する
	ESLError WriteImageData( const EGL_IMAGE_INFO & eii, DWORD fdwFlags ) ;
	// ストリームを閉じる
	ESLError EndStream( DWORD dwTotalTime ) ;

protected:
	// B ピクチャを圧縮して書き出す
	ESLError WriteBirectionalFrames( void ) ;
	// 音声データを圧縮して書き出す
	ESLError WriteWaveBuffer( void ) ;
	// 画像バッファを生成
	EGL_IMAGE_INFO * CreateImageBuffer( const ERI_INFO_HEADER & eih ) ;
	// 画像バッファを消去
	void DeleteImageBuffer( EGL_IMAGE_INFO * peii ) ;
	// 画像圧縮オブジェクトを生成
	virtual ERISAEncoder * CreateERIEncoder( void ) ;
	// 音声圧縮オブジェクトを生成
	virtual MIOEncoder * CreateMIOEncoder( void ) ;

public:
	// 出力された画像の枚数を取得する
	DWORD GetWrittenFrameCount( void ) const ;
	// マルチスレッド圧縮を有効化
	void EnableDualEncoding( bool fDualEncoding ) ;

protected:
	// スレッド関数
	static DWORD WINAPI ThreadProc( LPVOID parameter ) ;
	DWORD EncodingThreadProc( void ) ;

} ;


#endif

#endif
