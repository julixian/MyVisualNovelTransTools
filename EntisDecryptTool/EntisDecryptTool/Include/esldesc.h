
/*****************************************************************************
                   Entis Standard Library declarations
 ----------------------------------------------------------------------------

	In this file, the description object classes declarations.

	Copyright (C) 2002-2004 Leshade Entis, Entis-soft. All rights reserved.

 ****************************************************************************/


#if	!defined(__ESLDESCRIPTION_H__)
#define	__ESLDESCRIPTION_H__	1

#if	!defined(COMPACT_NOA_DECODER)


/*****************************************************************************
                        構造物記述オブジェクト
 *****************************************************************************/

class	EDescription	: public	ESLObject
{
public:
	// 構築関数
	EDescription( void ) ;
	EDescription( const EDescription & desc ) ;
	// 消滅関数
	virtual ~EDescription( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( EDescription, ESLObject )

protected:
	EDescription *				m_pParent ;			// 親オブジェクト
	EWideString					m_wstrTag ;			// タグ名
	EWideString					m_wstrContents ;	// 内容（タグ名が空の時）
	EWStrTagArray<EWideString>	m_wstaAttr ;		// タグ属性
	EObjArray<EDescription>		m_listDesc ;		// 内容

public:
	//
	// 文字列解析クラス
	class	EStreamXMLString	: public	EStreamWideString
	{
	public:
		// 構築関数
		EStreamXMLString( void ) { }
		EStreamXMLString( const EStreamWideString & swidestr )
			: EStreamWideString( swidestr ) { }
		EStreamXMLString( const EGenString<wchar_t,EWideString> & widestr )
			: EStreamWideString( widestr ) { }
		EStreamXMLString( const wchar_t * pwszString, unsigned int nLength )
			: EStreamWideString( pwszString, nLength ) { }
		EStreamXMLString( const wchar_t * pwszString )
			: EStreamWideString( pwszString ) { }
		EStreamXMLString( const char * pszString, unsigned int nLength = -1 )
			: EStreamWideString( pszString, nLength ) { }
		// 代入操作
		const EStreamXMLString &
					operator = ( const EStreamWideString & swstr )
			{
				EStreamWideString::operator = ( swstr ) ;
				return	*this ;
			}
		const EStreamXMLString &
					operator = ( const wchar_t * pwszString )
			{
				EStreamWideString::operator = ( pwszString ) ;
				return	*this ;
			}
		const EStreamXMLString &
					operator = ( const char * pszString )
			{
				EStreamWideString::operator = ( pszString ) ;
				return	*this ;
			}
		// 現在の文字列（区切り記号は無視）を通過する
		virtual void PassEnclosedString
			( wchar_t wchClose, int flagCtrlCode = FALSE ) ;
		// 現在のトークンを通過する（XML仕様）
		virtual void PassAToken( int * pTokenType = NULL ) ;
	} ;

public:
	// 書式種類
	enum	DataFormatType
	{
		dftXML,
		dftXMLandCEsc,
		dftAuto
	} ;
	// 文字エンコーディング種類
	enum	CharacterEncoding
	{
		ceShiftJIS,
		ceUTF8,
		ceISO2022JP,
		ceEUCJP,
		ceUnknown	= -1
	} ;
	// 読みこみ
	ESLError ReadDescription
		( EStreamBuffer & buf,
			DataFormatType dftType = dftAuto,
			CharacterEncoding ceType = ceShiftJIS ) ;
	ESLError ReadDescription
		( EStreamWideString & swsDesc,
			DataFormatType dftType = dftXMLandCEsc ) ;
	// 書き出し
	ESLError WriteDescription
		( ESLFileObject & file, int nTabIndent = 0,
			DataFormatType dftType = dftAuto,
			CharacterEncoding ceType = ceShiftJIS ) ;

public:
	// 副タグを生成
	virtual EDescription * CreateDescription( void ) ;
	// エラー出力
	virtual ESLError OutputError( const char * pszErrMsg ) ;
	// 警告出力
	virtual ESLError OutputWarning( const char * pszErrMsg ) ;
	// 代入操作
	const EDescription & operator = ( const EDescription & desc ) ;
	// マージ操作
	const EDescription & operator += ( const EDescription & desc ) ;

public:	// タグ
	// 親取得
	EDescription * GetParent( void ) const ;
	// タグ名取得
	const EWideString & Tag( void ) const ;
	// タグ名設定
	void SetTag( const wchar_t * pwszTag ) ;
	// 有効な（意味のある）タグかどうかを判定する
	bool IsValid( void ) const
		{
			if ( this == NULL )
				return	false ;
			if ( m_wstrTag.IsEmpty() )
				return	false ;
			wchar_t	wch = m_wstrTag.GetAt(0) ;
			return	((wch >= L'A') && (wch <= L'Z'))
				|| ((wch >= L'a') && (wch <= L'z')) || (wch == L'_') ;
		}
	// コメントタグにする
	void SetCommentTag( const wchar_t * pwszComment = NULL ) ;
	// 内容取得
	const EWideString & Contents( void ) const ;
	// 内容設定
	void SetContents( const wchar_t * pwszContents ) ;

public:	// タグ属性
	// タグ属性取得（整数）
	int GetAttrInteger( const wchar_t * pwszAttr, int nDefValue ) const ;
	// タグ属性取得（実数）
	double GetAttrReal( const wchar_t * pwszAttr, double rDefValue ) const ;
	// タグ属性取得（文字列）
	EWideString GetAttrString
		( const wchar_t * pwszAttr, const wchar_t * pwszDefValue ) const ;
	// タグ属性設定（整数）
	void SetAttrInteger( const wchar_t * pwszAttr, int nValue ) ;
	// タグ属性設定（実数）
	void SetAttrReal( const wchar_t * pwszAttr, double rValue ) ;
	// タグ属性設定（文字列）
	void SetAttrString( const wchar_t * pwszAttr, const wchar_t * pwszValue ) ;
	// タグ属性の総数を取得
	int GetAttributeCount( void ) const ;
	// タグ属性の指標を取得
	int GetAttributeIndexAs( const wchar_t * pwszAttr ) ;
	// タグ属性の名前を取得
	EWideString GetAttributeNameAt( int nIndex ) const ;
	// タグ属性の値を取得
	EWideString GetAttributeValueAt( int nIndex ) const ;
	// タグ属性を削除
	void RemoveAttributeAt( int nIndex ) ;
	void RemoveAttributeAs( const wchar_t * pwszAttr ) ;
	// 全てのタグ属性を削除
	void RemoveAllAttribute( void ) ;

public:	// 内容物
	// 含まれているタグの数を取得
	int GetContentTagCount( void ) const ;
	// 含まれているタグを取得
	EDescription * GetContentTagAt( int nIndex ) const ;
	// タグを条件を指定して検索
	EDescription * SearchTagAs
		( int nFirst, const wchar_t * pwszAttr = NULL,
			const wchar_t * pwszValue = NULL,
			const wchar_t * pwszTag = NULL, int * pNext = NULL ) const ;
	// タグを検索して取得
	EDescription * GetContentTagAs
		( int nFirst, const wchar_t * pwszTag = NULL, int * pNext = NULL ) const ;
	// タグを生成
	EDescription * CreateContentTagAs( int nFirst, const wchar_t * pwszTag ) ;
	// タグを検索
	int FindContentTag( int nFirst, const wchar_t * pwszTag = NULL ) const ;
	// タグを追加
	void AddContentTag( EDescription * pDesc ) ;
	// タグを挿入
	void InsertContentTag( int nIndex, EDescription * pDesc ) ;
	// 文字列属性タグを取得
	EWideString GetContentString
		( const wchar_t * pwszTag, const wchar_t * pwszDefValue ) const ;
	// 文字列属性タグを設定
	ESLError SetContentString
		( const wchar_t * pwszTag, const wchar_t * pwszValue ) ;
	// タグを削除
	void RemoveContentTagAt( int nIndex ) ;
	// 全てのタグを削除
	void RemoveAllContentTag( void ) ;

public:	// 高級関数
	// タグを検索
	EDescription * GetContentTag( const wchar_t * pwszTagPath ) const ;
	// タグを生成
	EDescription * CreateContentTag( const wchar_t * pwszTagPath ) ;
	// 文字列データ取得
	EWideString GetStringAt
		( const wchar_t * pwszTagPath,
			const wchar_t * pwszAttr, const wchar_t * pwszDefValue ) const ;
	// 整数データ取得
	int GetIntegerAt
		( const wchar_t * pwszTagPath,
			const wchar_t * pwszAttr, int nDefValue ) const ;
	// 実数データ取得
	double GetRealAt
		( const wchar_t * pwszTagPath,
			const wchar_t * pwszAttr, double rDefValue ) const ;
	// バイナリデータ取得
	ESLError GetBinaryAt
		( EStreamBuffer & bufBinary,
			const wchar_t * pwszTagPath, const wchar_t * pwszAttr ) const ;
	// 文字列データ設定
	void SetStringAt( const wchar_t * pwszTagPath,
			const wchar_t * pwszAttr, const wchar_t * pwszValue ) ;
	// 整数データ設定
	void SetIntegerAt
		( const wchar_t * pwszTagPath,
			const wchar_t * pwszAttr, int nValue ) ;
	// 実数データ設定
	void SetRealAt
		( const wchar_t * pwszTagPath,
			const wchar_t * pwszAttr, double rValue ) ;
	// バイナリデータ設定
	void SetBinaryAt
		( const wchar_t * pwszTagPath, const wchar_t * pwszAttr,
			const void * ptrData, unsigned int nBytes ) ;

public:	// 文字エンコーディング
	// 文字エンコーディング種類を判別
	static CharacterEncoding GetCharaEncodingType( const char * pszType ) ;
	// 文字エンコーディング種類文字列を取得
	static const char * GetCharaEncodingName( CharacterEncoding ceType ) ;
	// 文字列を Unicode に復元
	static ESLError DecodeText
		( EWideString & wstrText,
			const EString & strSrc, CharacterEncoding ceEncoding ) ;
	// 文字列を Unicode から符号化
	static ESLError EncodeText
		( EString & strText,
			const EWideString & wstrSrc, CharacterEncoding ceEncoding ) ;
	// 適切な文字コードを選択する
	static CharacterEncoding GetCharacterEncoding( const EString & strText ) ;

public:	// データエンコーディング
	// C 言語エスケープシーケンスを復元
	static ESLError DecodeTextCEscSequence( EWideString & wstrText ) ;
	// C 言語エスケープシーケンスに変換
	static ESLError EncodeTextCEscSequence( EWideString & wstrText ) ;
	// 文字列コンテンツのデコード
	static ESLError DecodeTextContents( EWideString & wstrText ) ;
	// 文字列コンテンツのエンコーディング
	static ESLError EncodeTextContents
		( EWideString & wstrText,
			const EWideString & wstrSrc, int nTabIndent = 0 ) ;
	// base64 から復元
	static ESLError DecodeBase64( EStreamBuffer & buf, const char * pszText ) ;
	// base64 に符号化
	static ESLError EncodeBase64
		( EString & strText, const void * ptrBuf, unsigned int nBytes ) ;

} ;

#endif	//	!defined(COMPACT_NOA_DECODER)

#endif

