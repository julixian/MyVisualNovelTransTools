
/*****************************************************************************
                         E R I S A - L i b r a r y
 -----------------------------------------------------------------------------
    Copyright (C) 2002-2003 Leshade Entis, Entis-soft. All rights reserved.
 *****************************************************************************/


#if	!defined(__ERISA_FILE_H__)
#define	__ERISA_FILE_H__	1


/*****************************************************************************
                  EMC (Entis Media Complex) ファイル形式
 *****************************************************************************/

class	EMCFile	: public	ESLFileObject
{
public:
	// 構築関数
	EMCFile( void ) ;
	// 消滅関数
	virtual ~EMCFile( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( EMCFile, ESLFileObject )

public:
	enum	FileIdentity
	{
		fidArchive			= 0x02000400,
		fidRasterizedImage	= 0x03000100,
		fidEGL3DSurface		= 0x03001100,
		fidEGL3DModel		= 0x03001200,
		fidUndefinedEMC		= -1
	} ;
	struct	FILE_HEADER
	{
		char	cHeader[8] ;			// ファイルシグネチャ
		DWORD	dwFileID ;				// ファイル識別子
		DWORD	dwReserved ;			// 予約＝０
		char	cFormatDesc[0x30] ;		// フォーマット名
	} ;
protected:
	struct	RECORD_HEADER
	{
		UINT64			nRecordID ;		// レコードの識別子
		UINT64			nRecLength ;	// レコードの長さ
	} ;
	struct	RECORD_INFO
	{
		RECORD_INFO *	pParent ;		// 親レコード
		DWORD			dwWriteFlag ;	// 書き込みフラグ
		UINT64			qwBasePos ;		// レコードの基準位置
		RECORD_HEADER	rechdr ;		// レコードヘッダ
	} ;

	ESLFileObject *	m_pFile ;			// ファイルオブジェクト
	RECORD_INFO *	m_pRecord ;			// 現在のレコード
	FILE_HEADER		m_fhHeader ;		// ファイルヘッダ

	static char	m_cDefSignature[8] ;

public:
	// ファイルを開く
	ESLError Open( ESLFileObject * pfile,
				const FILE_HEADER * pfhHeader = NULL ) ;
	// ファイルを閉じる
	void Close( void ) ;
	// EMC ファイルを複製
	virtual ESLFileObject * Duplicate( void ) const ;

public:
	// ファイルヘッダ取得
	const FILE_HEADER & GetFileHeader( void ) const
		{
			return	m_fhHeader ;
		}
	// ファイルヘッダ初期化
	static void SetFileHeader
		( FILE_HEADER & fhHeader,
			DWORD dwFileID, const char * pszDesc = NULL ) ;
	// デフォルトファイルシグネチャを取得
	static void GetFileSignature( char cHeader[8] ) ;
	// デフォルトファイルシグネチャを設定
	static void SetFileSignature( const char cHeader[8] ) ;

public:
	// レコードを開く
	virtual ESLError DescendRecord( const UINT64 * pRecID = NULL ) ;
	// レコードを閉じる
	virtual ESLError AscendRecord( void ) ;
	// レコード識別子を取得
	UINT64 GetRecordID( void ) const
		{
			ESLAssert( m_pRecord != NULL ) ;
			return	m_pRecord->rechdr.nRecordID ;
		}
	// レコード長を取得
	UINT64 GetRecordLength( void ) const
		{
			ESLAssert( m_pRecord != NULL ) ;
			return	m_pRecord->rechdr.nRecLength ;
		}

public:
	// データを読み込む
	virtual unsigned long int Read
		( void * ptrBuffer, unsigned long int nBytes ) ;
	// データを書き出す
	virtual unsigned long int Write
		( const void * ptrBuffer, unsigned long int nBytes ) ;
	// レコードの長さを取得する
	virtual UINT64 GetLargeLength( void ) const ;
	virtual unsigned long int GetLength( void ) const ;
	// ファイルポインタを移動する
	virtual UINT64 SeekLarge
		( INT64 nOffsetPos, SeekOrigin fSeekFrom ) ;
	virtual unsigned long int Seek
		( long int nOffsetPos, SeekOrigin fSeekFrom ) ;
	// ファイルポインタを取得する
	virtual UINT64 GetLargePosition( void ) const ;
	virtual unsigned long int GetPosition( void ) const ;
	// ファイルの終端を現在の位置に設定する
	virtual ESLError SetEndOfFile( void ) ;

} ;


/*****************************************************************************
                        ERI ファイルインターフェース
 *****************************************************************************/

 #if	!defined(COMPACT_NOA_DECODER)

class	ERIFile	: public	EMCFile
{
public:
	//
	// タグ情報インデックス
	enum	TagIndex
	{
		tagTitle,				// 曲名
		tagVocalPlayer,			// 歌手・演奏者
		tagComposer,			// 作曲者
		tagArranger,			// 編曲者
		tagSource,				// 出展・アルバム
		tagTrack,				// トラック
		tagReleaseDate,			// リリース年月日
		tagGenre,				// ジャンル
		tagRewindPoint,			// ループポイント
		tagHotSpot,				// ホットスポット
		tagResolution,			// 解像度
		tagComment,				// コメント
		tagWords,				// 歌詞
		tagMax
	} ;
	//
	// タグ情報文字列
	static const wchar_t *	m_pwszTagName[tagMax] ;
	//
	// タグ情報オブジェクト
	class	ETagObject
	{
	public:
		EWideString		m_tag ;
		EWideString		m_contents ;
	public:
		// 構築関数
		ETagObject( void ) ;
		// 消滅関数
		~ETagObject( void ) ;
	} ;
	//
	// タグ情報解析オブジェクト
	class	ETagInfo
	{
	public:
		EObjArray<ETagObject>	m_lstTags ;
	public:
		// 構築関数
		ETagInfo( void ) ;
		// 消滅関数
		~ETagInfo( void ) ;
		// タグ情報解析
		void CreateTagInfo( const wchar_t * pwszDesc ) ;
		// タグ情報をフォーマット
		void FormatDescription( EWideString & wstrDesc ) ;
		// タグを追加する
		void AddTag( TagIndex tagIndex, const wchar_t * pwszContents ) ;
		// タグ情報のクリア
		void DeleteContents( void ) ;
		// タグ情報取得
		const wchar_t * GetTagContents( const wchar_t * pwszTag ) const ;
		const wchar_t * GetTagContents( TagIndex tagIndex ) const ;
		// トラック番号を取得
		int GetTrackNumber( void ) const ;
		// リリース年月日を取得
		ESLError GetReleaseDate( int & year, int & month, int & day ) const ;
		// ループポイントを取得
		int GetRewindPoint( void ) const ;
		// ホットスポットを取得
		EGL_POINT GetHotSpot( void ) const ;
		// 解像度を取得
		long int GetResolution( void ) const ;
	} ;
	//
	// シーケンス構造体
	struct	SEQUENCE_DELTA
	{
		DWORD	dwFrameIndex ;
		DWORD	dwDuration ;
	} ;

	// 読み込みマスク
	enum	ReadMask
	{
		rmFileHeader	= 0x00000001,
		rmPreviewInfo	= 0x00000002,
		rmImageInfo		= 0x00000004,
		rmSoundInfo		= 0x00000008,
		rmCopyright		= 0x00000010,
		rmDescription	= 0x00000020,
		rmPaletteTable	= 0x00000040,
		rmSequenceTable	= 0x00000080
	} ;
	DWORD			m_fdwReadMask ;
	// ファイルヘッダ
	ERI_FILE_HEADER	m_FileHeader ;
	// プレビュー画像情報ヘッダ
	ERI_INFO_HEADER	m_PreviewInfo ;
	// 画像情報ヘッダ
	ERI_INFO_HEADER	m_InfoHeader ;
	// 音声情報ヘッダ
	MIO_INFO_HEADER	m_MIOInfHdr ;
	// パレットテーブル
	EGL_PALETTE		m_PaletteTable[0x100] ;
	// 著作権情報
	EWideString		m_wstrCopyright ;
	// コメント
	EWideString		m_wstrDescription ;

protected:
	DWORD				m_dwSeqLength ;
	SEQUENCE_DELTA *	m_pSequence ;

public:
	// 構築関数
	ERIFile( void ) ;
	// 消滅関数
	virtual ~ERIFile( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( ERIFile, EMCFile )

public:
	// ファイルのオープン方法
	enum	OpenType
	{
		otOpenRoot,			// ルートレコードを開くだけ
		otReadHeader,		// 情報ヘッダレコードを読み込んで値を検証
		otOpenStream,		// ヘッダを読み込みストリームレコードを開く
		otOpenImageData		// 画像データレコードを開く
	} ;
	// ERI ファイルを開く
	ESLError Open( ESLFileObject * pFile, OpenType type = otOpenImageData ) ;

public:
	// シーケンステーブルを取得する
	const SEQUENCE_DELTA * GetSequenceTable( DWORD * pdwLength ) const ;

} ;

#endif	//	!defined(COMPACT_NOA_DECODER)


/*****************************************************************************
                    ERISA アーカイブ形式 (NOA 形式)
 *****************************************************************************/

class	ERISAArchive	: public	EMCFile
{
public:
	// 構築関数
	ERISAArchive( void ) ;
	// 消滅関数
	virtual ~ERISAArchive( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( ERISAArchive, EMCFile )

public:
	struct	FILE_TIME
	{
		BYTE	nSecond ;
		BYTE	nMinute ;
		BYTE	nHour ;
		BYTE	nWeek ;
		BYTE	nDay ;
		BYTE	nMonth ;
		WORD	nYear ;
	} ;
	struct	FILE_INFO
	{
		UINT64		nBytes ;
		DWORD		dwAttribute ;
		DWORD		dwEncodeType ;
		UINT64		nOffsetPos ;
		FILE_TIME	ftFileTime ;
		DWORD		dwExtraInfoBytes ;
		void *		ptrExtraInfo ;
		DWORD		dwFileNameLen ;
		char *		ptrFileName ;

		FILE_INFO( void ) ;
		FILE_INFO( const FILE_INFO & finfo ) ;
		~FILE_INFO( void ) ;
		void SetExtraInfo( const void * pInfo, DWORD dwBytes ) ;
		void SetFileName( const char * pszFileName ) ;
	} ;
	enum	FileAttribute
	{
		attrNormal			= 0x00000000,
		attrReadOnly		= 0x00000001,
		attrHidden			= 0x00000002,
		attrSystem			= 0x00000004,
		attrDirectory		= 0x00000010,
		attrEndOfDirectory	= 0x00000020,
		attrNextDirectory	= 0x00000040
	} ;
	enum	EncodeType
	{
		etRaw				= 0x00000000,
		etERISACode			= 0x80000010,
		etBSHFCrypt			= 0x40000000,
		etERISACrypt		= 0xC0000010
	} ;
	class	EDirectory	: public	EObjArray<FILE_INFO>
	{
	public:
		EDirectory *	m_pParent ;
		DWORD			m_dwWriteFlag ;
	public:
		EDirectory( void )
			: m_pParent( NULL ), m_dwWriteFlag( 0 ) { }
		~EDirectory( void ) { }
		void CopyDirectory( const EDirectory & dir ) ;
		FILE_INFO * GetFileInfo( const char * pszFileName ) ;
		void AddFileEntry
			( const char * pszFileName, DWORD dwAttribute = 0,
				DWORD dwEncodeType = etRaw, FILE_TIME * pTime = NULL ) ;
	} ;
	enum	OpenType
	{
		otNormal,
		otStream
	} ;

protected:
	EDirectory *			m_pCurDir ;			// 現在のディレクトリ
	FILE_INFO *				m_pCurFile ;		// 現在のファイル

	ERISADecodeContext *	m_pDecERISA ;		// ERISA 復号
	ERISADecodeContext *	m_pDecBSHF ;		// BSHF 復号
	EMemoryFile *			m_pBufFile ;		// 展開されたファイルデータ

#if	!defined(COMPACT_NOA_DECODER)
	ERISAEncodeContext *	m_pEncERISA ;		// ERISA 符号化
	ERISAEncodeContext *	m_pEncBSHF ;		// BSHF 暗号化
#endif
	UINT64					m_qwWrittenBytes ;	// 書き出されたバイト数
	BYTE					m_bufCRC[4] ;		// ビットチェック用
	int						m_iCRC ;

public:
	// アーカイブファイルを開く
	ESLError Open
		( ESLFileObject * pfile,
			const EDirectory * pRootDir = NULL ) ;
	// アーカイブファイルを閉じる
	void Close( void ) ;

protected:
	// レコードを開く
	virtual ESLError DescendRecord( const UINT64 * pRecID = NULL ) ;
	// レコードを閉じる
	virtual ESLError AscendRecord( void ) ;

public:
	// カレントディレクトリのファイルエントリを取得する
	ESLError GetFileEntries( EDirectory & dirFiles ) ;
	EDirectory & ReferFileEntries( void )
		{
			ESLAssert( m_pCurDir != NULL ) ;
			return	*m_pCurDir ;
		}
	// 指定のディレクトリを開く
	ESLError DescendDirectory
		( const char * pszDirName, const EDirectory * pDir = NULL ) ;
	// ディレクトリを閉じる
	ESLError AscendDirectory( void ) ;
	// ファイルを開く
	ESLError DescendFile
		( const char * pszFileName,
			const char * pszPassword = NULL, OpenType otType = otNormal ) ;
	// ファイルを閉じる
	ESLError AscendFile( void ) ;
	// ディレクトリを開く（パス指定）
	ESLError OpenDirectory( const char * pszDirPath ) ;
	// ファイルを開く（パス指定）
	ESLError OpenFile
		( const char * pszFilePath,
			const char * pszPassword = NULL, OpenType otType = otNormal ) ;

protected:
	// 現在の位置からディレクトリエントリレコードを書き出す
	ESLError WriteinDirectoryEntry( void ) ;
	// 現在の位置からディレクトリエントリレコードを読み込む
	ESLError LoadDirectoryEntry( void ) ;

protected:
	// 展開・復号の進行状況を通知する
	virtual ESLError OnProcessFileData( DWORD dwCurrent, DWORD dwTotal ) ;

public:
	// ファイルオブジェクトを複製する
	virtual ESLFileObject * Duplicate( void ) const ;
	// アーカイブからデータを読み込む
	virtual unsigned long int Read
		( void * ptrBuffer, unsigned long int nBytes ) ;
	// アーカイブにデータを書き出す
	virtual unsigned long int Write
		( const void * ptrBuffer, unsigned long int nBytes ) ;
	// ファイルの長さを取得する
	virtual UINT64 GetLargeLength( void ) const ;
	virtual unsigned long int GetLength( void ) const ;
	// ファイルポインタを移動する
	virtual UINT64 SeekLarge
		( INT64 nOffsetPos, SeekOrigin fSeekFrom ) ;
	virtual unsigned long int Seek
		( long int nOffsetPos, SeekOrigin fSeekFrom ) ;
	// ファイルポインタを取得する
	virtual UINT64 GetLargePosition( void ) const ;
	virtual unsigned long int GetPosition( void ) const ;

} ;


/*****************************************************************************
                    ERISA アーカイブ化ファイルリスト
 *****************************************************************************/

class	ERISAArchiveList
{
public:
	// 構築関数
	ERISAArchiveList( void ) ;
	// 消滅関数
	~ERISAArchiveList( void ) ;

public:
	class	EDirectory ;
	class	EFileEntry
	{
	public:
		EString			m_strFileName ;
		DWORD			m_dwAttribute ;
		DWORD			m_dwEncodeType ;
		EString			m_strPassword ;
		EDirectory *	m_pSubDir ;
	public:
		EFileEntry( void )
			: m_dwAttribute( 0 ), m_dwEncodeType( 0 ), m_pSubDir( NULL ) { }
		~EFileEntry( void )
			{
				delete	m_pSubDir ;
			}
	} ;
	class	EDirectory	: public	EObjArray<EFileEntry>
	{
	public:
		EDirectory *	m_pParentDir ;
	public:
		EDirectory( void ) : m_pParentDir( NULL ) { }
	} ;

protected:
	EDirectory		m_dirRoot ;
	EDirectory *	m_pCurDir ;

public:
	// ファイルリストを読み込む
	ESLError LoadFileList( ESLFileObject & file ) ;
	// ファイルリストを書き出す
	ESLError SaveFileList( ESLFileObject & file ) ;
	// 内容を全て削除
	void DeleteContents( void ) ;

protected:
	// 指定のディレクトリを読み込む
	static ESLError ReadListOnDirectory
		( EDirectory & flist, EDescription & desc ) ;
	// 指定のディレクトリを書き出す
	static ESLError WriteListOnDirectory
		( EDescription & desc, EDirectory & flist ) ;

public:
	// 現在のディレクトリにファイルを追加
	void AddFileEntry
		( const char * pszFilePath,
			DWORD dwEncodeType = ERISAArchive::etRaw,
			const char * pszPassword = NULL ) ;
	// 現在のディレクトリにサブディレクトリを追加
	void AddSubDirectory( const char * pszDirName ) ;
	// ルートディレクトリのファイルエントリを取得
	EDirectory * GetRootFileEntries( void ) ;
	// 現在のディレクトリのファイルエントリを取得
	EDirectory * GetCurrentFileEntries( void ) ;
	// サブディレクトリへ移動
	ESLError DescendDirectory( const char * pszDirName ) ;
	// 親ディレクトリへ移動
	ESLError AscendDirectory( void ) ;
	// 現在の絶対ディレクトリパスを取得
	EString GetCurrentDirectoryPath( void ) const ;
	// 現在のディレクトリを絶対パスを指定して移動
	ESLError MoveCurrentDirectory( const char * pszDirPath ) ;

} ;


#endif
