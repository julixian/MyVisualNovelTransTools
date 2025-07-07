
/*****************************************************************************
                    Entis Standard Library declarations
 ----------------------------------------------------------------------------

	In this file, the array classes definitions.

	Copyright (C) 1998-2003 Leshade Entis. All rights reserved.

 ****************************************************************************/


#if	!defined(__ESLARRAY_H__)
#define	__ESLARRAY_H__	1


/****************************************************************************
                          ポインタ配列クラス
 ****************************************************************************/

class	EPtrArray	: public	ESLObject
{
protected:
	void **			m_ptrArray ;
	unsigned int	m_nLength ;
	unsigned int	m_nMaxSize ;
	unsigned int	m_nGrowAlign ;

public:
	// 構築関数
	EPtrArray( void )
		: m_ptrArray( NULL ), m_nLength( 0 ),
			m_nMaxSize( 0 ), m_nGrowAlign( 0 ) { }
	EPtrArray
		( const EPtrArray & array,
			unsigned int nFirst = 0, unsigned int nCount = -1 ) ;
	// 消滅関数
	~EPtrArray( void )
		{
			if ( m_ptrArray != NULL )
				::eslHeapFree( NULL, m_ptrArray ) ;
		}
	// クラス情報
	DECLARE_CLASS_INFO( EPtrArray, ESLObject )

protected:
	// メモリ確保
	virtual void AlloBuffer( unsigned int nSize ) ;
	// メモリ開放
	virtual void FreeBuffer( void ) ;

public:
	// 配列へのポインタを取得
	void ** const GetData( void ) const
		{
			return	m_ptrArray ;
		}
	// 配列の長さを取得
	unsigned int GetSize( void ) const
		{
			return m_nLength ;
		}
	// 配列の内部バッファの長さを取得
	unsigned int GetLimit( void ) const
		{
			return	m_nMaxSize ;
		}
	// 配列のサイズを設定
	void SetSize( unsigned int nSize, unsigned int nGrowAlign = 0 ) ;
	// 配列の内部バッファのサイズを設定
	void SetLimit( unsigned int nLimit, unsigned int nGrowAlign = 0 ) ;
	// 要素を取得
	void * GetAt( unsigned int nIndex ) const
		{
			if ( nIndex < m_nLength )
				return	m_ptrArray[nIndex] ;
			else
				return	NULL ;
		}
	// 要素を設定
	void SetAt( unsigned int nIndex, void * ptrData )
		{
			if ( nIndex >= m_nLength )
				SetSize( nIndex + 1 ) ;
			m_ptrArray[nIndex] = ptrData ;
		}
	// 要素を挿入
	void InsertAt( unsigned int nIndex, void * ptrData ) ;
	// 指定範囲の要素を削除
	void RemoveBetween( unsigned int nFirst, unsigned int nCount ) ;
	// 指定の要素を削除
	void RemoveAt( unsigned int nIndex )
		{
			RemoveBetween( nIndex, 1 ) ;
		}
	// 全要素を削除
	void RemoveAll( void ) ;
	// 指定の要素を入れ替える
	void Swap( unsigned int nIndex1, unsigned int nIndex2 ) ;
	// 配列の終端に要素を追加
	unsigned int Add( void * ptrData )
		{
			unsigned int	nIndex = m_nLength ;
			SetAt( nIndex, ptrData ) ;
			return	nIndex ;
		}
	// 配列を結合する
	void Merge
		( int nIndex, const EPtrArray & array,
			unsigned int nFirst = 0, unsigned int nCount = -1 ) ;

public:
	// スタックへプッシュ
	unsigned int Push( void * ptrData )
		{
			unsigned int	nIndex = m_nLength ;
			SetAt( nIndex, ptrData ) ;
			return	nIndex ;
		}
	// スタックからポップ
	void * Pop( void )
		{
			if ( m_nLength > 0 )
				return	m_ptrArray[-- m_nLength] ;
			return	NULL ;
		}
	// スタック上の配列アクセス
	void * GetLastAt( unsigned int nIndex = 0 ) const
		{
			if ( nIndex < m_nLength )
				return	m_ptrArray[m_nLength - nIndex - 1] ;
			else
				return	NULL ;
		}

} ;


/****************************************************************************
                             数値配列クラス
 ****************************************************************************/

template <class T> class	ENumArray : public	EPtrArray
{
public:
	// 構築関数
	ENumArray( void ) { }
	ENumArray
		( const ENumArray<T> & array,
			unsigned int nFirst = 0, unsigned int nCount = -1 )
		: EPtrArray( array, nFirst, nCount ) { }

public:
	// 指定要素を取得
	T GetAt( unsigned int nIndex ) const
		{
			DWORD	n = (DWORD) EPtrArray::GetAt( nIndex ) ;
			return	*((T*)&n) ;
		}
	T operator [] ( unsigned int nIndex ) const
		{
			DWORD	n = (DWORD) EPtrArray::GetAt( nIndex ) ;
			return	*((T*)&n) ;
		}
	// 指定要素に設定
	void SetAt( unsigned int nIndex, T numData )
		{
			DWORD	n = *((DWORD*)&numData) ;
			EPtrArray::SetAt( nIndex, (void*) n ) ;
		}
	// 指定要素に挿入
	void InsertAt( unsigned int nIndex, T numData )
		{
			DWORD	n = *((DWORD*)&numData) ;
			EPtrArray::InsertAt( nIndex, (void*) n ) ;
		}
	// 配列の終端に追加
	unsigned int Add( T numData )
		{
			DWORD	n = *((DWORD*)&numData) ;
			return	EPtrArray::Add( (void*) n ) ;
		}
	// スタックへプッシュ
	unsigned int Push( T numData )
		{
			DWORD	n = *((DWORD*)&numData) ;
			return	EPtrArray::Push( (void*) n ) ;
		}
	// スタックからポップ
	T Pop( void )
		{
			DWORD	n = (DWORD) EPtrArray::Pop() ;
			return	*((T*)&n) ;
		}
	// スタック上の配列アクセス
	T GetLastAt( unsigned int nIndex = 0 ) const
		{
			DWORD	n = (DWORD) EPtrArray::GetLastAt( nIndex ) ;
			return	*((T*)&n) ;
		}

} ;


/****************************************************************************
                      オブジェクト参照配列クラス
 ****************************************************************************/

template <class T> class	EPtrObjArray : public	EPtrArray
{
public:
	// 構築関数
	EPtrObjArray( void ) { }
	EPtrObjArray
		( const EPtrObjArray<T> & array,
			unsigned int nFirst = 0, unsigned int nCount = -1 )
		: EPtrArray( array, nFirst, nCount ) { }

public:
	// 配列へのポインタを取得
	T ** const GetData( void ) const
		{
			return	(T**) m_ptrArray ;
		}
	// 指定要素を取得
	T * GetAt( unsigned int nIndex ) const
		{
			return	(T*) EPtrArray::GetAt( nIndex ) ;
		}
	T & operator [] ( unsigned int nIndex ) const
		{
			T *	ptrObj = (T*) EPtrArray::GetAt( nIndex ) ;
			ESLAssert( ptrObj != NULL ) ;
			return	*ptrObj ;
		}
	// 指定要素に設定
	void SetAt( unsigned int nIndex, T * ptrObj )
		{
			EPtrArray::SetAt( nIndex, (void*) ptrObj ) ;
		}
	// 指定要素に挿入
	void InsertAt( unsigned int nIndex, T * ptrObj )
		{
			EPtrArray::InsertAt( nIndex, (void*) ptrObj ) ;
		}
	// 配列の終端に追加
	unsigned int Add( T * ptrObj )
		{
			return	EPtrArray::Add( (void*) ptrObj ) ;
		}
	// スタックへプッシュ
	unsigned int Push( T * ptrObj )
		{
			return	EPtrArray::Push( (void*) ptrObj ) ;
		}
	// スタックからポップ
	T * Pop( void )
		{
			return	(T*) EPtrArray::Pop( ) ;
		}
	// スタック上の配列アクセス
	T * GetLastAt( unsigned int nIndex = 0 ) const
		{
			return	(T*) EPtrArray::GetLastAt( nIndex ) ;
		}
	// 空の要素を削除
	void TrimEmpty( void )
		{
			unsigned int	i = 0, j = 0 ;
			while ( i < m_nLength )
			{
				if ( m_ptrArray[i] != NULL )
				{
					m_ptrArray[j ++] = m_ptrArray[i] ;
				}
				i ++ ;
			}
			m_nLength = j ;
		}

} ;


/****************************************************************************
                      オブジェクト配列クラス
 ****************************************************************************/

template <class T> class	EObjArray : public	EPtrObjArray<T>
{
public:
	// 構築関数
	EObjArray( void ) { }
	EObjArray
		( EObjArray<T> & array,
			unsigned int nFirst = 0, unsigned int nCount = -1 )
		: EPtrObjArray<T>( (const EPtrObjArray<T> &) array, nFirst, nCount )
		{
			if ( nCount == -1 )
			{
				nCount = array.m_nLength ;
			}
			((EPtrArray&)array).RemoveBetween( nFirst, nCount ) ;
		}
	// 消滅関数
	~EObjArray( void )
		{
			RemoveAll( ) ;
		}

public:
	// 指定要素に設定
	void SetAt( unsigned int nIndex, T * ptrData )
		{
			delete	this->GetAt( nIndex ) ;
			EPtrObjArray<T>::SetAt( nIndex, ptrData ) ;
		}
	T & operator [] ( unsigned int nIndex ) const
		{
			T *	ptrObj = (T*) EPtrArray::GetAt( nIndex ) ;
			ESLAssert( ptrObj != NULL ) ;
			return	*ptrObj ;
		}
	T & operator [] ( unsigned int nIndex )
		{
			T *	ptrObj = (T*) EPtrArray::GetAt( nIndex ) ;
			if ( ptrObj == NULL )
			{
				ptrObj = new T ;
				EPtrArray::SetAt( nIndex, ptrObj ) ;
			}
			return	*ptrObj ;
		}
	// 配列のサイズを設定
	void SetSize( unsigned int nSize, unsigned int nGrowAlign = 0 )
		{
			for ( unsigned int i = this->GetSize(); i > nSize; i -- )
				SetAt( i - 1, NULL ) ;
			EPtrObjArray<T>::SetSize( nSize, nGrowAlign ) ;
		}
	// 指定区間削除
	void RemoveBetween( unsigned int nFirst, unsigned int nCount )
		{
			for ( int i = nFirst + nCount - 1; i >= (int) nFirst;
					i = (i >= (int)this->m_nLength) ? (this->m_nLength - 1) : (i - 1) )
				delete	this->GetAt( i ) ;
			EPtrObjArray<T>::RemoveBetween( nFirst, nCount ) ;
		}
	// 指定要素を削除
	void RemoveAt( unsigned int nIndex )
		{
			RemoveBetween( nIndex, 1 ) ;
		}
	// 全ての要素を削除
	void RemoveAll( void )
		{
			for ( int i = this->GetSize() - 1; i >= 0;
					i = (i >= (int)this->m_nLength) ? (this->m_nLength - 1) : (i - 1) )
				delete	this->GetAt( i ) ;
			EPtrObjArray<T>::RemoveAll( ) ;
		}
	// 指定区間を分離
	void DetachBetween( unsigned int nFirst, unsigned int nCount )
		{
			EPtrObjArray<T>::RemoveBetween( nFirst, nCount ) ;
		}
	// 指定要素を分離
	void DetachAt( unsigned int nIndex )
		{
			EPtrObjArray<T>::RemoveAt( nIndex ) ;
		}
	// 全ての要素を分離
	void DetachAll( void )
		{
			EPtrObjArray<T>::RemoveAll( ) ;
		}
	// 配列を結合する
	void Merge
		( int nIndex, EObjArray<T> & array,
			unsigned int nFirst = 0, unsigned int nCount = -1 )
		{
			EPtrObjArray<T>::Merge( nIndex, array, nFirst, nCount ) ;
			array.DetachBetween( nFirst, nCount ) ;
		}

} ;


/****************************************************************************
                          連想配列用コンテナ
 ****************************************************************************/

template <class TagType, class ObjType> class	ETaggedElement
{
private:
	TagType		m_tag ;
	ObjType *	m_obj ;

public:
	// 構築関数
	ETaggedElement( void )
		: m_obj( NULL ) { }
	ETaggedElement( TagType tag, ObjType * obj )
		: m_tag( tag ), m_obj( obj ) { }
	// 消滅関数
	~ETaggedElement( void )
		{
			delete	m_obj ;
		}
	// タグを取得
	TagType & Tag( void )
		{
			return	m_tag ;
		}
	// オブジェクトを取得
	ObjType * GetObject( void ) const
		{
			return	m_obj ;
		}
	// オブジェクトを設定
	void SetObject( ObjType * obj )
		{
			delete	m_obj ;
			m_obj = obj ;
		}
	// オブジェクトを分離
	ObjType * DetachObject( void )
		{
			ObjType *	obj = m_obj ;
			m_obj = NULL ;
			return	obj ;
		}

} ;


/****************************************************************************
                     ソートによる連想配列クラス
 ****************************************************************************/

template < class TagType, class ObjType >	class	ETagSortArray
			: public	EObjArray< ETaggedElement<TagType,ObjType> >
{
public:
	// 構築関数
	ETagSortArray( void ) { }
	// 要素検索
	ObjType * GetAs
		( TagType tag, unsigned int * pIndex = NULL ) const
		{
			int		iFirst, iEnd, iMiddle ;
			ETaggedElement<TagType,ObjType> *	pElement ;
			iFirst = 0 ;
			iEnd = this->GetSize() - 1 ;
			//
			while ( iFirst <= iEnd )
			{
				iMiddle = ((iFirst + iEnd) >> 1) ;
				pElement = this->GetAt( iMiddle ) ;
				ESLAssert( pElement != NULL ) ;
				//
				if ( tag < pElement->Tag() )
				{
					iEnd = iMiddle - 1 ;
				}
				else if ( tag > pElement->Tag() )
				{
					iFirst = iMiddle + 1 ;
				}
				else
				{
					if ( pIndex != NULL )
					{
						*pIndex = iMiddle ;
					}
					return	pElement->GetObject( ) ;
				}
			}
			//
			return	NULL ;
		}
	// 要素追加（同一要素があった場合、上書きする）
	unsigned int SetAs( TagType tag, ObjType * obj )
		{
			int		iFirst, iEnd, iMiddle = 0 ;
			ETaggedElement<TagType,ObjType> *	pElement ;
			iFirst = 0 ;
			iEnd = this->GetSize() - 1 ;
			//
			while ( iFirst <= iEnd )
			{
				iMiddle = ((iFirst + iEnd) >> 1) ;
				pElement = this->GetAt( iMiddle ) ;
				ESLAssert( pElement != NULL ) ;
				//
				if ( tag < pElement->Tag() )
				{
					iEnd = iMiddle - 1 ;
				}
				else if ( tag > pElement->Tag() )
				{
					iFirst = iMiddle + 1 ;
				}
				else
				{
					pElement->SetObject( obj ) ;
					return	iMiddle ;
				}
			}
			//
			pElement = new ETaggedElement<TagType,ObjType>( tag, obj ) ;
			InsertAt( iFirst, pElement ) ;
			//
			return	iFirst ;
		}
	// 要素追加（同一要素があった場合、書き換えない）
	unsigned int Add( TagType tag, ObjType * obj )
		{
			int		iFirst, iEnd, iMiddle = 0 ;
			ETaggedElement<TagType,ObjType> *	pElement ;
			iFirst = 0 ;
			iEnd = this->GetSize() - 1 ;
			//
			while ( iFirst <= iEnd )
			{
				iMiddle = ((iFirst + iEnd) >> 1) ;
				pElement = this->GetAt( iMiddle ) ;
				ESLAssert( pElement != NULL ) ;
				//
				if ( tag < pElement->Tag() )
				{
					iEnd = iMiddle - 1 ;
				}
				else
				{
					iFirst = iMiddle + 1 ;
				}
			}
			//
			pElement = new ETaggedElement<TagType,ObjType>( tag, obj ) ;
			InsertAt( iFirst, pElement ) ;
			//
			return	iFirst ;
		}
	// 要素逆引き
	ETaggedElement<TagType,ObjType> *
			SearchAs( ObjType * obj, unsigned int * pIndex = NULL )
		{
			ETaggedElement<TagType,ObjType> *	pElement ;
			for ( unsigned int i = 0; i < this->GetSize(); i ++ )
			{
				pElement = this->GetAt( i ) ;
				ESLAssert( pElement != NULL ) ;
				if ( pElement == NULL )
					continue ;
				//
				if ( pElement->GetObject() == obj )
				{
					if ( pIndex != NULL )
						*pIndex = i ;
					return	pElement ;
				}
			}
			return	NULL ;
		}
	// 要素削除
	void RemoveAs( TagType tag )
		{
			unsigned int	index = -1 ;
			GetAs( tag, &index ) ;
			if ( index != (unsigned int) -1 )
			{
				this->RemoveAt( index ) ;
			}
		}
	// 要素分離
	void DetachAs( TagType tag )
		{
			unsigned int	index ;
			if ( GetAs( tag, &index ) != NULL )
			{
				ETaggedElement<TagType,ObjType> *
							pElement = this->GetAt( index ) ;
				ESLAssert( pElement != NULL ) ;
				pElement->DetachObject( ) ;
				this->RemoveAt( index ) ;
			}
		}
	// 全ての要素を分離
	void DetachAll( void )
		{
			for ( unsigned int i = 0; i < this->GetSize(); i ++ )
			{
				ETaggedElement<TagType,ObjType> *
							pElement = this->GetAt( i ) ;
				pElement->DetachObject( ) ;
			}
			this->RemoveAll( ) ;
		}
	// 要素アクセス
	ObjType * GetObjectAt( unsigned int nIndex ) const
		{
			ETaggedElement<TagType,ObjType> * pElement = this->GetAt( nIndex ) ;
			if ( pElement == NULL )
			{
				return	NULL ;
			}
			return	pElement->GetObject( ) ;
		}
	const ObjType & operator [] ( TagType tag ) const
		{
			const ObjType *	ptrObj = GetAs( tag, NULL ) ;
			ESLAssert( ptrObj != NULL ) ;
			return	*ptrObj ;
		}
	ObjType & operator [] ( TagType tag )
		{
			ObjType *	ptrObj = GetAs( tag, NULL ) ;
			if ( ptrObj == NULL )
			{
				ptrObj = new ObjType ;
				Add( tag, ptrObj ) ;
			}
			return	*ptrObj ;
		}

} ;


//////////////////////////////////////////////////////////////////////////////
// 数値による連想配列
//////////////////////////////////////////////////////////////////////////////

template <class T>	class	EIntTagArray
					: public	ETagSortArray<int,T>
{
public:
	// 構築関数
	EIntTagArray( void ) { }

} ;


//////////////////////////////////////////////////////////////////////////////
// 文字列による連想配列
//////////////////////////////////////////////////////////////////////////////

template <class T>	class	EStrTagArray
					: public	ETagSortArray<EString,T>
{
public:
	// 構築関数
	EStrTagArray( void ) { }

} ;

template <class T>	class	EWStrTagArray
					: public	ETagSortArray<EWideString,T>
{
public:
	// 構築関数
	EWStrTagArray( void ) { }

} ;


#endif
