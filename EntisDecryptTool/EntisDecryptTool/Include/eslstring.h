
/*****************************************************************************
                   Entis Standard Library declarations
 ----------------------------------------------------------------------------

	In this file, the strings classes declarations.

	Copyright (C) 1998-2004 Leshade Entis. All rights reserved.

 ****************************************************************************/


#if	!defined(__ESLSTRING_H__)
#define	__ESLSTRING_H__	1


/*****************************************************************************
                         文字列クラステンプレート
 ****************************************************************************/

template <class _Type, class _Obj>	class	EGenString	: public	ESLObject
{
protected:
	// データメンバ
	_Type *			m_pszString ;
	unsigned int	m_nLength ;
	unsigned int	m_nBufLimit ;

public:
	// 構築関数
	EGenString( void )
		: m_pszString( NULL ), m_nLength( 0 ), m_nBufLimit( 0 ) { }
	EGenString( _Type chrCode )
		: m_pszString( NULL ), m_nLength( 1 )
		{
			AllocString( 0x0F ) ;
			m_pszString[0] = chrCode ;
			m_pszString[1] = 0 ;
		}
	EGenString( const EGenString<_Type,_Obj> & strObject )
		:	m_pszString( NULL ), m_nLength( strObject.m_nLength )
		{
			AllocString( strObject.m_nBufLimit ) ;
			unsigned int	i ;
			for ( i = 0; i < m_nLength; i ++ )
				m_pszString[i] = strObject.m_pszString[i] ;
			m_pszString[i] = 0 ;
		}
	EGenString( const _Type * pszString, int nLength )
		: m_pszString( NULL ), m_nLength( nLength ), m_nBufLimit( 0 )
		{
			if ( nLength < 0 )
			{
				nLength = 0 ;
				if ( pszString != NULL )
				{
					while ( pszString[nLength] )
						nLength ++ ;
				}
				m_nLength = nLength ;
			}
			if ( nLength > 0 )
			{
				AllocString( m_nLength + (m_nLength >> 1) + 1 ) ;
				unsigned int	i ;
				for ( i = 0; i < m_nLength; i ++ )
					m_pszString[i] = pszString[i] ;
				m_pszString[i] = 0 ;
			}
		}
	EGenString( const _Type * pszString )
		: m_pszString( NULL ), m_nBufLimit( 0 )
		{
			m_nLength = 0 ;
			if ( pszString != NULL )
			{
				while( pszString[m_nLength] )
					m_nLength ++ ;
				AllocString( m_nLength + (m_nLength >> 1) + 1 ) ;
				unsigned int	i ;
				for ( i = 0; i < m_nLength; i ++ )
					m_pszString[i] = pszString[i] ;
				m_pszString[i] = 0 ;
			}
		}
	~EGenString( void )
		{
			if ( m_pszString != NULL )
				FreeString( ) ;
		}

public:
	// メモリアロケーション
	virtual void AllocString( unsigned int nLength )
		{
			if ( m_pszString != NULL )
			{
				m_pszString = (_Type*)
					::eslHeapReallocate
						( NULL, m_pszString, ((nLength + 1) * sizeof(_Type)), 0 ) ;
			}
			else
			{
				m_pszString = (_Type*)
					::eslHeapAllocate( NULL, ((nLength + 1) * sizeof(_Type)), 0 ) ;
				m_pszString[0] = '\0' ;
			}
			m_nBufLimit = nLength ;
		}
	virtual void FreeString( void )
		{
			if ( m_pszString != NULL )
			{
				::eslHeapFree( NULL, m_pszString ) ;
				m_pszString = NULL ;
				m_nLength = 0 ;
				m_nBufLimit = 0 ;
			}
		}

public:
	// 属性取得
	unsigned int GetLength( void ) const
		{
			return	m_nLength ;
		}
	bool IsEmpty( void ) const
		{
			return	(m_nLength == 0) ;
		}

public:
	// 文字列取得
	_Type GetAt( int nIndex ) const
		{
			if ( (m_pszString != NULL) &&
					((unsigned int) nIndex < m_nLength) )
				return	m_pszString[nIndex] ;
			else
				return	(_Type) 0 ;
		}
	void SetAt( int nIndex, _Type chrCode )
		{
			ESLAssert( (m_pszString != NULL) && ((unsigned int) nIndex < m_nLength) ) ;
			m_pszString[nIndex] = chrCode ;
		}
	_Type operator [] ( int nIndex ) const
		{
			if ( (m_pszString != NULL) &&
					((unsigned int) nIndex < m_nLength) )
				return	m_pszString[nIndex] ;
			else
				return	(_Type) 0 ;
		}
	_Type & operator [] ( int nIndex )
		{
			ESLAssert( (unsigned int) nIndex < m_nLength ) ;
			return	m_pszString[nIndex] ;
		}
	operator const _Type * ( void ) const
		{
			return	m_pszString ;
		}
	const _Type * CharPtr( void ) const
		{
			return	m_pszString ;
		}

public:
	// 文字列比較
	int Compare( const _Type * pszString ) const
		{
			if ( m_nLength == 0 )
				return	((pszString != NULL) ? - (int) pszString[0] : 0) ;
			else if ( pszString == NULL )
				return	(int) m_pszString[0] ;
			unsigned int	i ;
			for ( i = 0; i < m_nLength; i ++ )
			{
				int	nCompare = (int) m_pszString[i] - (int) pszString[i] ;
				if ( nCompare != 0 )
					return	nCompare ;
			}
			return	 - (int) pszString[i] ;
		}
	int CompareNoCase( const _Type * pszString ) const
		{
			if ( m_nLength == 0 )
				return	((pszString != NULL) ? - (int) pszString[0] : 0) ;
			else if ( pszString == NULL )
				return	(int) m_pszString[0] ;
			unsigned int	i ;
			for ( i = 0; i < m_nLength; i ++ )
			{
				_Type	c1 = m_pszString[i] ;
				_Type	c2 = pszString[i] ;
				if ( c1 != c2 )
				{
					if ( ('a' <= c1) && (c1 <= 'z') )
						c1 += ('A' - 'a') ;
					if ( ('a' <= c2) && (c2 <= 'z') )
						c2 += ('A' - 'a') ;
					int	nCompare = (int) c1 - (int) c2 ;
					if ( nCompare != 0 )
					{
						return	nCompare ;
					}
				}
			}
			return	 - (int) pszString[i] ;
		}
	bool operator == ( const _Type * pszString ) const
		{
			return	!Compare( pszString ) ;
		}
	bool operator != ( const _Type * pszString ) const
		{
			return	(Compare( pszString ) != 0) ;
		}
	bool operator < ( const _Type * pszString ) const
		{
			return	(Compare(pszString) < 0) ;
		}
	bool operator > ( const _Type * pszString ) const
		{
			return	(Compare(pszString) > 0) ;
		}
	bool operator <= ( const _Type * pszString ) const
		{
			return	(Compare(pszString) <= 0) ;
		}
	bool operator >= ( const _Type * pszString ) const
		{
			return	(Compare(pszString) >= 0) ;
		}

public:
	// 文字列操作（部分抽出）
	_Obj Middle
		( unsigned int nFirst, unsigned int nCount = -1 ) const
		{
			if ( m_pszString == NULL )
				return	_Obj( ) ;
			if ( nCount == -1 )
			{
				if ( nFirst < m_nLength )
					nCount = m_nLength - nFirst ;
			}
			if ( m_nLength < (nFirst + nCount) )
			{
				if ( m_nLength <= nFirst )
					return	_Obj( ) ;
				nCount = (m_nLength - nFirst) ;
			}
			return	_Obj( (m_pszString + nFirst), nCount ) ;
		}
	_Obj Left( unsigned int nCount ) const
		{
			if ( m_pszString == NULL )
				return	_Obj( ) ;
			if ( m_nLength < nCount )
				return	_Obj( m_pszString, m_nLength ) ;
			else
				return	_Obj( m_pszString, nCount ) ;
		}
	_Obj Right( unsigned int nCount ) const
		{
			if ( m_pszString == NULL )
				return	_Obj( ) ;
			if ( m_nLength < nCount )
				return	_Obj( m_pszString, m_nLength ) ;
			else
				return	_Obj( (m_pszString + m_nLength - nCount), nCount ) ;
		}
	// 文字列置き換え
	_Obj Replace( const _Type * pszOld, const _Type * pszNew )
		{
			_Obj	strReplaced ;
			strReplaced.AllocString( m_nLength + 1 ) ;
			unsigned int	iLast = 0 ;
			int				cDummy = 0 ;
			if ( pszNew == NULL )
			{
				pszNew = (const _Type *) &cDummy ;
			}
			for ( unsigned int i = 0; i < m_nLength; i ++ )
			{
				unsigned int	j ;
				for ( j = 0; pszOld[j] && (m_pszString[i+j] == pszOld[j]); j ++ ) ;
				if ( pszOld[j] == 0 )
				{
					strReplaced +=
						EGenString<_Type,_Obj>
							( m_pszString + iLast, i - iLast ) ;
					strReplaced += pszNew ;
					iLast = i + j ;
					i = iLast - 1 ;
				}
			}
			if ( iLast < m_nLength )
			{
				strReplaced +=
					EGenString<_Type,_Obj>
						( m_pszString + iLast, m_nLength - iLast ) ;
			}
			return	strReplaced ;
		}

public:
	// ファイル名（URL）操作
	const _Type * GetFileNamePart( void ) const
		{
			unsigned int	l = 0 ;
			if ( m_pszString == NULL )
			{
				((EGenString<_Type,_Obj>*)this)->AllocString( 0x10 ) ;
			}
			for ( unsigned int i = 0; i < m_nLength; i ++ )
			{
				_Type	c = m_pszString[i] ;
				if ( (c == '\\') || (c == '/') )
				{
					l = i + 1 ;
				}
			}
			return	m_pszString + l ;
		}
	const _Type * GetFileExtensionPart( void ) const
		{
			const _Type *	pszFileName = GetFileNamePart( ) ;
			for ( int i = 0; pszFileName[i]; i ++ )
			{
				if ( pszFileName[i] == '.' )
				{
					return	pszFileName + i + 1 ;
				}
			}
			return	m_pszString + m_nLength ;
		}
	_Obj GetFileTitlePart( void ) const
		{
			const _Type *	pszFileName = GetFileNamePart( ) ;
			for ( int i = 0; pszFileName[i]; i ++ )
			{
				if ( pszFileName[i] == '.' )
				{
					return	_Obj( pszFileName, i ) ;
				}
			}
			return	pszFileName ;
		}
	_Obj GetFileDirectoryPart( void ) const
		{
			unsigned int	l = 0 ;
			for ( unsigned int i = 0; i < m_nLength; i ++ )
			{
				_Type	c = m_pszString[i] ;
				if ( (c == '\\') || (c == '/') )
				{
					l = i + 1 ;
				}
			}
			return	_Obj( m_pszString, l ) ;
		}
	// 数値変換
	long int AsInteger( bool fSign = true, bool * pError = NULL )
		{
			long int	nValue = 0x80000000 ;
			long int	nSign = 0 ;
			bool		fErr = true ;
			for ( unsigned int i = 0; i < m_nLength; i ++ )
			{
				unsigned int	c = m_pszString[i] ;
				if ( (c >= '0') & (c <= '9') )
				{
					nValue = nValue * 10 + (c - '0') ;
					fErr = false ;
				}
				else if ( fSign && ((c == '+') | (c == '-')) )
				{
					fSign = false ;
					nSign = - (int) (c == '-') ;
				}
				else if ( c > (unsigned int) ' ' )
				{
					break ;
				}
			}
			if ( pError != NULL )
			{
				*pError = (fErr || (m_nLength == 0)) ;
			}
			return	(nValue ^ nSign) - nSign ;
		}
	// アルファベットを大文字に変換
	void MakeUpper( void )
		{
			for ( unsigned int i = 0; i < m_nLength; i ++ )
			{
				_Type	c = m_pszString[i] ;
				if ( ('a' <= c) && (c <= 'z') )
				{
					m_pszString[i] = c - ('a' - 'A') ;
				}
			}
		}
	// アルファベットを小文字に変換
	void MakeLower( void )
		{
			for ( unsigned int i = 0; i < m_nLength; i ++ )
			{
				_Type	c = m_pszString[i] ;
				if ( ('A' <= c) && (c <= 'Z') )
				{
					m_pszString[i] = c + ('a' - 'A') ;
				}
			}
		}
	// 文字列の左右反転
	void MakeReverse( void )
		{
			int	nLength = m_nLength ;
			int	nHalfLength = nLength / 2 ;
			nLength -- ;
			//
			for ( int i = 0; i < nHalfLength; i ++ )
			{
				wchar_t	wch = m_pszString[i] ;
				m_pszString[i] = m_pszString[nLength - i] ;
				m_pszString[nLength - i] = wch ;
			}
		}

public:
	// 文字検索
	int Find( _Type chrCode, int nIndex = 0 ) const
		{
			while ( (unsigned int) nIndex < m_nLength )
			{
				if ( m_pszString[nIndex] == chrCode )
				{
					return	nIndex ;
				}
				nIndex ++ ;
			}
			return	-1 ;
		}
	int Find( const _Type * pszString, int nIndex = 0 ) const
		{
			for ( unsigned int i = nIndex; i < m_nLength; i ++ )
			{
				unsigned int	j ;
				for ( j = 0; pszString[j]; j ++ )
				{
					if ( m_pszString[i + j] != pszString[j] )
						break ;
				}
				if ( pszString[j] == 0 )
				{
					return	i ;
				}
			}
			return	-1 ;
		}

public:
	// 空白除去
	void TrimLeft( void )
		{
			if ( m_pszString != NULL )
			{
				unsigned int	i ;
				for ( i = 0; i < m_nLength; i ++ )
				{
					if ( m_pszString[i] > ' ' )
					{
						if ( i > 0 )
						{
							m_nLength -= i ;
							unsigned int	j ;
							for( j = 0; j < m_nLength; j ++ )
							{
								m_pszString[j] = m_pszString[i + j] ;
							}
							m_pszString[j] = 0 ;
						}
						return ;
					}
				}
				if ( i >= m_nLength )
				{
					m_nLength = 0 ;
					m_pszString[0] = 0 ;
				}
			}
		}
	void TrimRight( void )
		{
			if ( m_pszString != NULL )
			{
				int	i ;
				for ( i = (m_nLength - 1); i >= 0; i -- )
				{
					if ( m_pszString[i] > ' ' )
					{
						m_nLength = (++ i) ;
						m_pszString[i] = 0 ;
						return ;
					}
				}
				if ( i < 0 )
				{
					m_nLength = 0 ;
					m_pszString[0] = 0 ;
				}
			}
		}

public:
	// 代入操作
	const EGenString<_Type,_Obj> & operator = ( _Type chrCode )
		{
			m_nLength = 1 ;
			AllocString( 0x0F ) ;
			m_pszString[0] = chrCode ;
			m_pszString[1] = 0 ;
			return	*this ;
		}
	const EGenString<_Type,_Obj> & operator =
			( const EGenString<_Type,_Obj> & strObject )
		{
			if ( this == &strObject )
				return	*this ;
			m_nLength = strObject.m_nLength ;
			if ( m_nLength == 0 )
			{
				if ( m_pszString != NULL )
					m_pszString[0] = 0 ;
			}
			else
			{
				AllocString( strObject.m_nBufLimit ) ;
				unsigned int	i ;
				for ( i = 0; i < m_nLength; i ++ )
					m_pszString[i] = strObject.m_pszString[i] ;
				m_pszString[i] = 0 ;
			}
			return	*this ;
		}
	const EGenString<_Type,_Obj> & operator = ( const _Type * pszString )
		{
			if ( pszString == m_pszString )
				return	*this ;
			if ( pszString == NULL )
			{
				if ( m_nLength == 0 )
					return	*this ;
				m_nLength = 0 ;
			}
			else
				for ( m_nLength = 0; pszString[m_nLength]; m_nLength ++ ) ;
			AllocString( m_nLength + (m_nLength >> 1) + 1 ) ;
			unsigned int	i ;
			for ( i = 0; i < m_nLength; i ++ )
				m_pszString[i] = pszString[i] ;
			m_pszString[i] = 0 ;
			return	*this ;
		}

public:
	// 文字列結合
	const EGenString<_Type,_Obj> & operator +=
			( const EGenString<_Type,_Obj> & strObject )
		{
			unsigned int	nLength = m_nLength + strObject.m_nLength ;
			if ( nLength >= m_nBufLimit )
				AllocString( nLength + (nLength >> 1) + 1 ) ;
			for ( unsigned int i = 0; i < strObject.m_nLength; i ++ )
				m_pszString[m_nLength + i] = strObject.m_pszString[i] ;
			m_pszString[(m_nLength = nLength)] = 0 ;
			return	*this ;
		}
	const EGenString<_Type,_Obj> & operator += ( _Type chrCode )
		{
			if ( (++ m_nLength) >= m_nBufLimit )
				AllocString( m_nLength + (m_nLength >> 1) + 1 ) ;
			m_pszString[m_nLength - 1] = chrCode ;
			m_pszString[m_nLength] = 0 ;
			return	*this ;
		}
	const EGenString<_Type,_Obj> & operator += ( const _Type * ptrString )
		{
			int	nLength = 0 ;
			if ( ptrString != NULL )
			{
				while( ptrString[nLength] )
					nLength ++ ;
			}
			unsigned int	nBase = m_nLength ;
			m_nLength += nLength ;
			if( m_nLength >= m_nBufLimit )
				AllocString( m_nLength + (m_nLength >> 1) + 1 ) ;
			for( int i = 0; i < nLength; i ++ )
				m_pszString[nBase + i] = ptrString[i] ;
			m_pszString[m_nLength] = 0 ;
			return	*this ;
		}
	_Obj operator + ( const EGenString<_Type,_Obj> & strObject ) const
		{
			_Obj	objString( *this ) ;
			objString += strObject ;
			return	objString ;
		}
	_Obj operator + ( _Type chrCode ) const
		{
			_Obj	strObject( *this ) ;
			strObject += chrCode ;
			return	strObject ;
		}
	_Obj operator + ( const _Type * pszString ) const
		{
			_Obj	strObject( *this ) ;
			strObject += pszString ;
			return	strObject ;
		}
	friend _Obj operator +
		( const _Type * pszString, const EGenString<_Type,_Obj> & strObject )
		{
			_Obj	objString( pszString ) ;
			objString += strObject ;
			return	objString ;
		}
	_Obj operator * ( int nRepeatCount ) const
		{
			_Obj	objString ;
			_Obj	objSource = *this ;
			if ( nRepeatCount < 0 )
			{
				nRepeatCount = - nRepeatCount ;
				objSource.MakeReverse( ) ;
			}
			objString.AllocString( m_nLength * nRepeatCount ) ;
			for ( int i = 0; i < nRepeatCount; i ++ )
				objString += objSource ;
			return	objString ;
		}

public:
	// 内部シーケンスへのアクセス
	_Type * GetBuffer( unsigned int nBufferSize )
		{
			if ( nBufferSize >= m_nBufLimit )
				AllocString( nBufferSize + 1 ) ;
			return	m_pszString ;
		}
	void ReleaseBuffer( int nLength = -1 )
		{
			if ( nLength < 0 )
			{
				unsigned int	i ;
				for ( i = 0; m_pszString[i] && (i < m_nBufLimit); i ++ ) ;
				m_nLength = i - (int) (i >= m_nBufLimit) ;
				m_pszString[m_nLength] = 0 ;
			}
			else
			{
				ESLAssert( (unsigned int) nLength < m_nBufLimit ) ;
				m_pszString[nLength] = 0 ;
				m_nLength = nLength ;
			}
		}

} ;


/*****************************************************************************
                       文字列クラスのインスタンス化
 ****************************************************************************/

class	EString : public	EGenString<char,EString>
{
public:
	// 構築関数
	EString( void )
		: EGenString<char,EString>( ) { }
	EString( char chrCode )
		: EGenString<char,EString>( chrCode ) { }
	EString( const EGenString<char,EString> & strObject )
		: EGenString<char,EString>( strObject ) { }
	EString( const char * pszString, unsigned int nLength )
		: EGenString<char,EString>( pszString, nLength ) { }
	EString( const char * pszString )
		: EGenString<char,EString>( pszString ) { }
	EString( const wchar_t * pwszString, unsigned int nLength = -1 ) ;
	EString( int nValue, int nPrec = 0 ) ;
	EString( DWORD dwValue, int nPrec = 0 ) ;
	EString( double rValue, int nPrec = 0 ) ;
	// クラス情報
	DECLARE_CLASS_INFO( EString, ESLObject )

public:
	// 文字列比較
	int Compare( const char * pszString ) const ;
	int CompareNoCase( const char * pszString ) const ;
	static int Compare( const char * pstr1, const char * pstr2 ) ;
	static int CompareNoCase( const char * pstr1, const char * pstr2 ) ;

	// 代入操作
	const EString & operator = ( char chrCode )
		{
			EGenString<char,EString>::operator = ( chrCode ) ;
			return	*this ;
		}
	const EString & operator = ( const char * pszString )
		{
			EGenString<char,EString>::operator = ( pszString ) ;
			return	*this ;
		}
	const EString & operator = ( const EGenString<char,EString> & strObject )
		{
			EGenString<char,EString>::operator = ( strObject ) ;
			return	*this ;
		}
	const EString & operator = ( const EString & strObject )
		{
			EGenString<char,EString>::operator = ( strObject ) ;
			return	*this ;
		}
	const EString & operator = ( const wchar_t * pwszString ) ;

	// ファイル名（URL）操作
	const char * GetFileNamePart( void ) const ;
	const char * GetFileExtensionPart( void ) const ;
	EString GetFileTitlePart( void ) const ;
	EString GetFileDirectoryPart( void ) const ;

	// アルファベットを大文字に変換
	void MakeUpper( void ) ;
	// アルファベットを小文字に変換
	void MakeLower( void ) ;
	// 文字列の左右反転
	void MakeReverse( void ) ;

public:
	// 文字検索
	int Find( char chrCode, int nIndex = 0 ) const ;
	int Find( const char * pszString, int nIndex = 0 ) const ;

public:
	// 空白除去
	void TrimLeft( void ) ;
	void TrimRight( void ) ;

} ;


#if	!defined(COMPACT_NOA_DECODER)

class	EWideString : public	EGenString<wchar_t,EWideString>
{
public:
	// 構築関数
	EWideString( void )
		: EGenString<wchar_t,EWideString>( ) { }
	EWideString( wchar_t wchCode )
		: EGenString<wchar_t,EWideString>( wchCode ) { }
	EWideString( const EGenString<wchar_t,EWideString> & strObject )
		: EGenString<wchar_t,EWideString>( strObject ) { }
	EWideString( const wchar_t * pwszString, unsigned int nLength )
		: EGenString<wchar_t,EWideString>( pwszString, nLength ) { }
	EWideString( const wchar_t * pwszString )
		: EGenString<wchar_t,EWideString>( pwszString ) { }
	EWideString( const char * pszString, unsigned int nLength = -1 ) ;
	EWideString( int nValue, int nPrec = 0 ) ;
	EWideString( DWORD dwValue, int nPrec = 0 ) ;
	EWideString( double rValue, int nPrec = 0 ) ;
	// クラス情報
	DECLARE_CLASS_INFO( EWideString, ESLObject )

public:
	// 代入操作
	const EWideString & operator = ( wchar_t wchCode )
		{
			EGenString<wchar_t,EWideString>::operator = ( wchCode ) ;
			return	*this ;
		}
	const EWideString & operator = ( const wchar_t * pwszString )
		{
			EGenString<wchar_t,EWideString>::operator = ( pwszString ) ;
			return	*this ;
		}
	const EWideString & operator = ( const EGenString<wchar_t,EWideString> & strObject )
		{
			EGenString<wchar_t,EWideString>::operator = ( strObject ) ;
			return	*this ;
		}
	const EWideString & operator = ( const EWideString & strObject )
		{
			EGenString<wchar_t,EWideString>::operator = ( strObject ) ;
			return	*this ;
		}
	const EWideString & operator = ( const char * pszString ) ;

} ;


/*****************************************************************************
                        構文解析用文字列クラス
 ****************************************************************************/

class	EStreamWideString : public	EWideString
{
public:
	// 構築関数
	EStreamWideString( void ) ;
	EStreamWideString( const EStreamWideString & swidestr ) ;
	EStreamWideString( const EGenString<wchar_t,EWideString> & widestr ) ;
	EStreamWideString( const wchar_t * pwszString, unsigned int nLength ) ;
	EStreamWideString( const wchar_t * pwszString ) ;
	EStreamWideString( const char * pszString, unsigned int nLength = -1 ) ;
	// クラス情報
	DECLARE_CLASS_INFO( EStreamWideString, EWideString )

public:
	// 消滅関数
	virtual ~EStreamWideString( void ) ;
	virtual void DeleteContents( void ) ;

protected:
	// データメンバ
	unsigned int		m_nIndex ;
	struct	PUNCTUATION_MASK
	{
		DWORD	m_bit32[4] ;
	} ;
	PUNCTUATION_MASK	m_maskPunctuation ;
	PUNCTUATION_MASK	m_maskSpecialPunc ;

private:
	static const PUNCTUATION_MASK	m_defPunctuation ;
	static const PUNCTUATION_MASK	m_defSpecialPunc ;

public:
	// 代入操作
	const EStreamWideString &
				operator = ( const EStreamWideString & swstr ) ;
	const EStreamWideString &
				operator = ( const EGenString<wchar_t,EWideString> & widestr ) ;
	const EStreamWideString &
				operator = ( const wchar_t * pwszString ) ;
	const EStreamWideString &
				operator = ( const char * pszString ) ;

public:
	// ファイルから読み込む
	virtual ESLError ReadTextFile( ESLFileObject & file ) ;
	virtual ESLError ReadJiscode( ESLFileObject & file ) ;
	virtual ESLError ReadUnicode( ESLFileObject & file ) ;

public:
	// 指標に対する操作
	unsigned int GetIndex( void ) const
		{
			return	m_nIndex ;
		}
	unsigned int MoveIndex( unsigned int nIndex )
		{
			if ( nIndex > m_nLength )
				return	(m_nIndex = m_nLength) ;
			else
				return	(m_nIndex = nIndex) ;
		}

public:		// ストリーミング操作
	// 次の1文字を取得
	wchar_t GetCharacter( void )
		{
			if ( m_nIndex >= m_nLength )
				return	L'\0' ;
			return	m_pszString[(m_nIndex ++)] ;
		}
	wchar_t CurrentCharacter( void ) const
		{
			if ( m_nIndex >= m_nLength )
				return	L'\0' ;
			return	m_pszString[m_nIndex] ;
		}

	// 空白文字を読み飛ばす
	virtual int DisregardSpace( void ) ;

	// 次の行へ移動
	virtual int MoveToNextLine( void ) ;

	// 現在の文字列（区切り記号は無視）を通過する
	virtual void PassString( void ) ;
	virtual void PassEnclosedString
		( wchar_t wchClose, int flagCtrlCode = FALSE ) ;

	// 現在のトークンを通過する
	virtual void PassAToken( int * pTokenType = NULL ) ;
	virtual void PassAExpressionTerm( void ) ;

public:		// 文字列取得
	// 次の文字列取得（区切り記号は無視）
	virtual EWideString GetString( void ) ;
	virtual EWideString GetEnclosedString
		( wchar_t wchClose, int flagCtrlCode = FALSE ) ;

	// 次のトークンを取得
	virtual EWideString GetAToken( int * pTokenType = NULL ) ;

	// 次の数値を取得
	virtual int GetNumberRadix( void ) ;
	virtual REAL64 GetRealNumber( int nRadix = 0 ) ;
	virtual INT64 GetLargeInteger( int nRadix = 0 ) ;
	virtual int GetInteger( int nRadix = 0 ) ;

	// 次に来るべき文字（文字列）を通過する
	wchar_t HasToComeChar( const wchar_t * pwszNext ) ;
	bool HasToComeToken( const wchar_t * pwszNext ) ;

public:		// 高度な書式解析
	// 書式構文の構造体
	enum	UsageType
	{
		utToken,				// トークン
		utString,				// 文字列
		utSpace,				// 任意の空白
		utEnclosed,				// 指定文字コードで閉じられた任意文字列
		utWildCardToken,		// 任意のトークン
		utWildCardUsage,		// 任意の文字列
		utWildCardExpression,	// 任意の式
		utNumberUsage,			// 10進数文字列
		utEndOfUsage,			// 文字列の終端
		utCharacters,			// 指定の文字
		utRepeat,				// 反復
		utBeginParam,			// パラメータの開始
		utEndParam,				// パラメータの終了
		utOmittable,			// 省略可能
		utSelectable,			// 書式の選択
		utList
	} ;
	enum	UsageFlag
	{
		ufNoCase	= 0x0001,
		ufNegative	= 0x0002,
		ufRepeat	= 0x0004
	} ;
	class	EUsage	: public	EObjArray<EUsage>
	{
	public:
		int					m_type ;
		int					m_flags ;
		EUsage *			m_parent ;
		EWideString			m_wstr ;
		DWORD				m_mask[4] ;
	public:
		EUsage( int type = utList, EUsage * parent = NULL )
			: m_type(type), m_flags(0), m_parent(parent) { }
		~EUsage( void ) { }
		int GetUsageParamCount( void ) const ;
		int GetChildIndex( EUsage * pUsage ) const ;
	} ;

	// 次の書式判定
	virtual ESLError IsMatchUsage
		( EStreamWideString & swsUsage, EString & strErrMsg,
				EObjArray<EWideString> * plstParam = NULL ) ;
	// 書式の検索
	virtual int FindMatchUsage
		( EStreamWideString & swsUsage, UsageType utWildCardType = utToken ) ;
	// 書式の置き換え
	virtual ESLError ReplaceUsage
		( EWideString & wstrReplaced, EStreamWideString & swsUsage,
			const wchar_t * pwszReplace, UsageType utWildCardType = utToken ) ;
protected:
	// 書式の置き換え判定
	virtual ESLError OnReplaceUsage
		( EWideString & wstrReplace,
			const EObjArray<EWideString> & lstParam ) ;

public:
	// 書式の一致判定
	virtual ESLError IsMatchAUsage
		( EUsage * pUsage, EString * pstrErrMsg,
			EObjArray<EWideString> * plstParam = NULL ) ;
	virtual ESLError IsMatchUsageList
		( EUsage & usage, int iStart,
			EString & strErrMsg, EObjArray<EWideString> * plstParam = NULL ) ;
	virtual int FindMatchUsageList
		( EUsage & usage, int iStart, UsageType utWildCardType ) ;
	virtual int FindMatchAUsage
		( EUsage * pParent, int nIndex,
			EUsage * pUsage, UsageType utWildCardType ) ;
	// 書式構文の解析
	virtual ESLError ParseUsage
		( EUsage & lstUsage,
			EStreamWideString & swsUsage, EString & strErrMsg ) const ;

public:		// 状態問い合わせ
	// 指標がオーバーフローしているか？
	int IsIndexOverflow( void ) const
		{
			return	(m_nIndex >= m_nLength) ;
		}
	// 空白記号か？
	static int IsCharacterSpace( wchar_t wch )
		{
			return	(wch <= L' ') ;
		}
	// 区切り記号か？
	int IsPunctuation( wchar_t wch ) const
		{
			if	( wch & (~0x7F) )	return	0 ;
			return	(m_maskPunctuation.m_bit32
						[(wch >> 5)] & (1 << (wch & 0x1F))) ;
		}
	// 特殊区切り記号か？
	int IsSpecialPunctuaion( wchar_t wch ) const
		{
			if	( wch & (~0x7F) )	return	0 ;
			return	(m_maskSpecialPunc.m_bit32
						[(wch >> 5)] & (1 << (wch & 0x1F))) ;
		}

} ;

#endif	//	!defined(COMPACT_NOA_DECODER)


/*****************************************************************************
                           参照バッファクラス
 ****************************************************************************/

class	EPtrBuffer
{
protected:
	// 構築関数
	EPtrBuffer( void ) { }
public:
	EPtrBuffer( const void * ptrBuffer, unsigned int nLength )
		: m_ptrBuffer( ptrBuffer ), m_nLength( nLength ) { }
	EPtrBuffer( const EPtrBuffer & ptrbuf )
		: m_ptrBuffer( ptrbuf.m_ptrBuffer ), m_nLength( ptrbuf.m_nLength ) { }
	// 消滅関数
	~EPtrBuffer( void ) { }

protected:
	// データメンバ
	const void *	m_ptrBuffer ;
	unsigned int	m_nLength ;

public:
	// バッファの長さ取得
	unsigned int GetLength( void ) const
		{
			return	m_nLength ;
		}
	// バッファへのアドレス取得
	const void * GetBuffer( void ) const
		{
			return	m_ptrBuffer ;
		}

public:
	// 内部シーケンス取得
	operator const void * ( void ) const
		{
			return	m_ptrBuffer ;
		}
	// 代入操作
	EPtrBuffer & operator = ( const EPtrBuffer & ptrbuf )
		{
			m_ptrBuffer = ptrbuf.m_ptrBuffer ;
			m_nLength = ptrbuf.m_nLength ;
			return	*this ;
		}

} ;


/*****************************************************************************
                    ストリーミングバッファクラス
 ****************************************************************************/

class	EStreamBuffer	: public	ESLObject
{
public:
	// 構築関数
	EStreamBuffer( void ) ;
	EStreamBuffer( const void * ptrBuffer, unsigned int nLength ) ;
	EStreamBuffer( const EPtrBuffer & ptrbuf ) ;
	// 消滅関数
	virtual ~EStreamBuffer( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( EStreamBuffer, ESLObject )

protected:
	class	EBuffer
	{
	protected:
		EBuffer *		m_pNextBuf ;
		BYTE *			m_ptrBuf ;
		unsigned int	m_nBeginPos ;
		unsigned int	m_nLength ;
		unsigned int	m_nBufLimit ;
	public:
		// 構築関数
		EBuffer( void )
			: m_pNextBuf( NULL ),
				m_ptrBuf( NULL ), m_nBeginPos( 0 ),
				m_nLength( 0 ), m_nBufLimit( 0 ) { }
		// 消滅関数
		~EBuffer( void ) ;
		// 次のバッファ取得
		EBuffer * GetNextBuffer( void ) const
			{
				return	m_pNextBuf ;
			}
		void SetNextBuffer( EBuffer * pNext )
			{
				m_pNextBuf = pNext ;
			}
		// バッファの長さを取得
		unsigned int GetLength( void ) const
			{
				return	m_nLength ;
			}
		// バッファ取得
		EPtrBuffer GetBuffer( unsigned int nLength ) ;
		// バッファ解放
		void Release( unsigned int nLength ) ;
		// メモリ再確保無しで確保可能な最大の長さを取得
		unsigned int GetMaxWritableLength( void ) const
			{
				return	(m_nBufLimit - m_nLength) ;
			}
		// 出力のためにバッファ確保
		void * PutBuffer( unsigned int nLength ) ;
		// メモリ確定
		void Flush( unsigned int nLength ) ;
		// バッファ参照
		BYTE * ModifyBuffer( unsigned int nPosition )
			{
				return	m_ptrBuf + nPosition ;
			}
	} ;
	EBuffer *		m_pFirstBuf ;
	EBuffer *		m_pLastBuf ;
	unsigned int	m_nLength ;

public:
	// リソースを解放
	void Delete( void ) ;

public:		// バッファからの出力操作
	// バッファの長さ取得
	unsigned int GetLength( void ) const ;
	// バッファから取り出すためのメモリを確保
	EPtrBuffer GetBuffer( unsigned int nLength = 0 ) ;
	// GetBuffer によってロックされたメモリを解放
	void Release( unsigned int nLength ) ;
	// バッファからデータを読み出す
	unsigned int Read( void * ptrBuffer, unsigned int nLength ) ;

public:		// バッファへの入力操作
	// バッファへ書き出すためのメモリを確保
	void * PutBuffer( unsigned int nLength ) ;
	// PutBuffer によってロックされたメモリを確定
	void Flush( unsigned int nLength ) ;
	// バッファへデータを書き込む
	unsigned int Write( const void * ptrBuffer, unsigned int nLength ) ;

public:		// バッファへの変更操作
	// バッファの一部を参照
	void * ModifyBuffer( unsigned int nPosition, unsigned int nLength ) ;

public:
	// ファイルの終端まで読み込む
	ESLError ReadFromFile( ESLFileObject & file ) ;
	// バッファの内容をファイルに書き出す
	ESLError WriteToFile( ESLFileObject & file ) ;

} ;


#endif
