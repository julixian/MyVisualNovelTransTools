
/*****************************************************************************
                    Entis Standard Library declarations
 ----------------------------------------------------------------------------
        Copyright (c) 2002-2003 Leshade Entis. All rights reserved.
 *****************************************************************************/


#if	!defined(__ESL_H__)
#define	__ESL_H__

#if	!defined(GLSEXPORT)
	#if	defined(_IMPORT_GLS)
		#define	GLSEXPORT	__declspec(dllimport)
	#else
		#define	GLSEXPORT	__declspec(dllexport)
	#endif
#endif


//////////////////////////////////////////////////////////////////////////////
// デバッグサポート関数
//////////////////////////////////////////////////////////////////////////////

#if	defined(_DEBUG)

extern "C"
{
	void ESLTrace( const char * pszTrace, ... ) ;
	void _ESLAssert
		( const char * pszExpr, const char * pszFile, int nLine ) ;
}

#define	ESLVerify(expr)		\
	if ( !(expr) )	_ESLAssert( #expr, __FILE__, __LINE__ )
#define	ESLAssert(expr)		\
	if ( !(expr) )	_ESLAssert( #expr, __FILE__, __LINE__ )

#else

inline void ESLTrace( const char * pszTrace, ... ) { }
#define	ESLVerify(expr)	(expr)
#define	ESLAssert(expr)	((void)0)

#endif


//////////////////////////////////////////////////////////////////////////////
// 共通エラーコード
//////////////////////////////////////////////////////////////////////////////

enum	ESLError
{
	eslErrSuccess		= 0,
	eslErrNotSupported	= -1,
	eslErrGeneral		= 1,
	eslErrAbort			= 2,
	eslErrInvalidParam	= 3,
	eslErrTimeout		= 4,
	eslErrPending		= 5,
	eslErrContinue		= 6,
	eslErrDummy			= 0xFFFFFFFF
} ;

inline ESLError ESLErrorMsg( const char * pszMsg )
{
	return	(ESLError) (long int) pszMsg ;
}

const char * GetESLErrorMsg( ESLError err ) ;


//////////////////////////////////////////////////////////////////////////////
// ヒープ関数
//////////////////////////////////////////////////////////////////////////////

#include	<eslheap.h>

#if	!defined(_MFC_VER) && !defined(_DISABLE_ESL_NEW)

void * operator new ( size_t stObj ) ;
// void * operator new ( size_t stObj, void * ptrObj ) ;
#if	defined(_DEBUG)
void * operator new ( size_t stObj, const char * pszFileName, int nLine ) ;
#endif
void operator delete ( void * ptrObj ) ;

#endif


//////////////////////////////////////////////////////////////////////////////
// 基底クラス
//////////////////////////////////////////////////////////////////////////////

class	ESLObject
{
public:
	static const char *const	m_pszClassName ;
	ESLObject( void ) { }
	virtual ~ESLObject( void ) { }
	virtual const char * GetClassName( void ) const ;
	virtual int IsKindOf( const char * pszClassName ) const ;
	virtual ESLObject * DynamicCast( const char * pszType ) const ;
	int IsSameClassAs( ESLObject * pObj ) const ;
	static void * operator new ( size_t stObj ) ;
	static void * operator new ( size_t stObj, void * ptrObj ) ;
	static void * operator new ( size_t stObj, const char * pszFileName, int nLine ) ;
	static void operator delete ( void * ptrObj ) ;

} ;

template <class T1, class T2> const T1 * ESLTypeCast( const T2 * pObj )
{
	if ( pObj == NULL )
		return	NULL ;
	try
	{
		return	(const T1 *) (void*) pObj->DynamicCast( T1::m_pszClassName ) ;
	}
	catch ( ... )
	{
	}
	return	NULL ;
}

template <class T1, class T2> T1 * ESLTypeCast( T2 * pObj )
{
	if ( pObj == NULL )
		return	NULL ;
	try
	{
		return	(T1*) (void*) pObj->DynamicCast( T1::m_pszClassName ) ;
	}
	catch ( ... )
	{
	}
	return	NULL ;
}

#define	DECLARE_CLASS_INFO( class_name, parent_class )					\
	public:	static const char *const	m_pszClassName ;				\
	virtual const char * GetClassName( void ) const ;					\
	virtual int IsKindOf( const char * pszClassName ) const ;			\
	virtual ESLObject * DynamicCast( const char * pszClassName ) const ;

#define	DECLARE_CLASS_INFO2( class_name, parent_class1, parent_class2 )	\
	public:	static const char *const	m_pszClassName ;				\
	virtual const char * GetClassName( void ) const ;					\
	virtual int IsKindOf( const char * pszClassName ) const ;			\
	virtual ESLObject * DynamicCast( const char * pszClassName ) const ;	\
	void * operator new ( size_t stObj )								\
		{	return 	parent_class1::operator new ( stObj ) ;	}			\
	void * operator new ( size_t stObj, void * ptrObj )					\
		{	return 	parent_class1::operator new ( stObj, ptrObj ) ;	}	\
	void * operator new ( size_t stObj, const char * pszFileName, int nLine )	\
		{	return 	parent_class1::operator new ( stObj, pszFileName, nLine ) ;	}	\
	void operator delete ( void * ptrObj )								\
		{	parent_class1::operator delete ( ptrObj ) ;	}

#define	IMPLEMENT_CLASS_INFO( class_name, parent_class )				\
	const char *const	class_name::m_pszClassName = #class_name ;		\
	const char * class_name::GetClassName( void ) const					\
		{																\
			return	class_name::m_pszClassName ;						\
		}																\
	int class_name::IsKindOf( const char * pszClassName ) const			\
		{																\
			if ( !EString::Compare( pszClassName, class_name::m_pszClassName ) )		\
				return	1 ;												\
			return	parent_class::IsKindOf( pszClassName ) ;			\
		}																\
	ESLObject * class_name::DynamicCast( const char * pszType ) const	\
		{																\
			if ( !EString::Compare( pszType, class_name::m_pszClassName ) )		\
				return	(ESLObject*) (DWORD) this ;						\
			return	parent_class::DynamicCast( pszType ) ;				\
		}

#define	IMPLEMENT_CLASS_INFO2( class_name, parent_class1, parent_class2 )	\
	const char *const	class_name::m_pszClassName = #class_name ;		\
	const char * class_name::GetClassName( void ) const					\
		{																\
			return	class_name::m_pszClassName ;						\
		}																\
	int class_name::IsKindOf( const char * pszClassName ) const			\
		{																\
			if ( !EString::Compare( pszClassName, class_name::m_pszClassName ) )		\
				return	1 ;												\
			else if ( parent_class1::IsKindOf( pszClassName ) )			\
				return	1 ;												\
			return	parent_class2::IsKindOf( pszClassName ) ;			\
		}																\
	ESLObject * class_name::DynamicCast( const char * pszType ) const	\
		{																\
			if ( !EString::Compare( pszType, class_name::m_pszClassName ) )		\
				return	(ESLObject*) (DWORD) this ;						\
			if ( parent_class1::IsKindOf( pszType ) )					\
				return	parent_class1::DynamicCast( pszType ) ;			\
			if ( parent_class2::IsKindOf( pszType ) )					\
				return	parent_class2::DynamicCast( pszType ) ;			\
			return	NULL ;												\
		}


//////////////////////////////////////////////////////////////////////////////
// Entis Library 低水準クラスライブラリ
//////////////////////////////////////////////////////////////////////////////

template <class _Type, class _Obj> class	EGenString ;
	class	EString ;
	class	EWideString ;
		class	EStreamWideString ;

class	EPtrBuffer ;
class	EStreamBuffer ;

class	EPtrArray ;
	template <class> class	ENumArray ;
	template <class> class	EPtrObjArray ;
		template <class> class	EObjArray ;
			template <class TagType, class ObjType> class	ETagSortArray ;
				template <class>	class	EIntTagArray ;
				template <class>	class	EStrTagArray ;
				template <class>	class	EWStrTagArray ;
template <class TagType, class ObjType> class	ETaggedElement ;

class	ESLFileObject ;
	class	ERawFile ;
	class	EMemoryFile ;
	class	EStreamFileBuffer ;
	class	ESyncStreamFile ;

class	EDescription ;

#include	<eslarray.h>
#include	<eslstring.h>
#include	<eslfile.h>
#include	<esldesc.h>


#endif

