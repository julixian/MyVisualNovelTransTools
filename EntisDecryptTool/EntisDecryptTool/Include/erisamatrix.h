
/*****************************************************************************
                         E R I S A - L i b r a r y
                                                      最終更新 2003/01/28
 ----------------------------------------------------------------------------
    Copyright (C) 2000-2003 Leshade Entis, Entis-soft. All rights reserved.
 *****************************************************************************/


#if	!defined(__ERISA_MATRIX_H__)
#define	__ERISA_MATRIX_H__	1


/*****************************************************************************
                            DCT行列演算用定数
 *****************************************************************************/

#define	MIN_DCT_DEGREE	2
#define	MAX_DCT_DEGREE	12


/*****************************************************************************
                              実数丸め関数
 *****************************************************************************/

extern	"C"
{
	// 32 ビット浮動小数点を 32 ビット整数に変換
	int eriRoundR32ToInt( REAL32 r ) ;
	// 64 ビット浮動小数点を 64 ビット整数に変換
	INT64 eriRoundR64ToLInt( REAL64 r ) ;
	// 32 ビット浮動小数点配列を 16 ビット整数配列に変換
	void eriRoundR32ToWordArray
		( SWORD * ptrDst, int nStep, const REAL32 * ptrSrc, int nCount ) ;
	// 8 ビット符号無し整数配列を 32 ビット浮動小数点配列に変換
	void eriConvertArrayByteToR32
		( REAL32 * ptrDst, const BYTE * ptrSrc, int nCount ) ;
	// 8 ビット符号有り整数配列を 32 ビット浮動小数点配列に変換
	void eriConvertArraySByteToR32
		( REAL32 * ptrDst, const SBYTE * ptrSrc, int nCount ) ;
	// 32 ビット浮動小数点配列を 8 ビット符号なし整数配列に変換
	void eriConvertArrayR32ToByte
		( BYTE * ptrDst, const REAL32 * ptrSrc, int nCount ) ;
	// 32 ビット浮動小数点配列を 8 ビット符号あり整数配列に変換
	void eriConvertArrayR32ToSByte
		( SBYTE * ptrDst, const REAL32 * ptrSrc, int nCount ) ;
} ;


/*****************************************************************************
                             回転行列構造体
 *****************************************************************************/

struct	ERI_SIN_COS
{
	REAL32	rSin ;
	REAL32	rCos ;
} ;


/*****************************************************************************
                             行列演算関数
 *****************************************************************************/

extern	"C"
{
	// 行列初期化
	void eriInitializeMatrix( void ) ;

	// スカラ乗算
	void eriScalarMultiply
		( REAL32 * ptrDst, REAL32 rScalar, unsigned int nCount ) ;
	// ベクトル乗算
	void eriVectorMultiply
		( REAL32 * ptrDst, const REAL32 * ptrSrc, unsigned int nCount ) ;

	// 2 点回転変換
	void eriRevolve2x2
		( REAL32 * ptrBuf1, REAL32 * ptrBuf2,
			REAL32 rSin, REAL32 rCos,
			unsigned int nStep, unsigned int nCount ) ;

	// 高速 DCT 変換
	void eriFastDCT
		(
			REAL32 *		ptrDst,
			unsigned int	nDstInterval,
			REAL32 *		ptrSrc,
			REAL32 *		ptrWorkBuf,
			unsigned int	nDegreeDCT
		) ;
	// 高速逆 DCT 変換
	void eriFastIDCT
		(
			REAL32 *		ptrDst,
			REAL32 *		ptrSrc,
			unsigned int	nSrcInterval,
			REAL32 *		ptrWorkBuf,
			unsigned int	nDegreeDCT
		) ;

	// 回転行列を生成する
	ERI_SIN_COS * eriCreateRevolveParameter( unsigned int nDegreeDCT ) ;

	// LOT 変換事前行列
	void eriFastPLOT
		(
			REAL32 *		ptrSrc,
			unsigned int	nDegreeDCT
		) ;
	// LOT 変換
	void eriFastLOT
		(
			REAL32 *		ptrDst,
			const REAL32 *	ptrSrc1,
			const REAL32 *	ptrSrc2,
			unsigned int	nDegreeDCT
		) ;

	// Givens 回転行列
	void eriOddGivensMatrix
		(
			REAL32 *			ptrDst,
			const ERI_SIN_COS *	ptrRevolve,
			unsigned int		nDegreeDCT
		) ;
	// 逆 Givens 回転行列
	void eriOddGivensInverseMatrix
		(
			REAL32 *			ptrSrc,
			const ERI_SIN_COS *	ptrRevolve,
			unsigned int		nDegreeDCT
		) ;

	// 逆 LOT 変換事前行列
	void eriFastIPLOT
		(
			REAL32 *		ptrSrc,
			unsigned int	nDegreeDCT
		) ;
	// 逆 LOT 変換
	void eriFastILOT
		(
			REAL32 *		ptrDst,
			const REAL32 *	ptrSrc1,
			const REAL32 *	ptrSrc2,
			unsigned int	nDegreeDCT
		) ;

	// 高速2次元 DCT 変換
	void eriFastDCT8x8( REAL32 * ptrDst ) ;
	// 高速2次元逆 DCT 変換
	void eriFastIDCT8x8( REAL32 * ptrDst ) ;
	// 高速2次元 LOT 変換
	void eriFastLOT8x8
		(
			REAL32 *		ptrDst,
			REAL32 *		ptrHorzCur,
			REAL32 *		ptrVertCur
		) ;
	// 高速2次元逆 LOT 変換
	void eriFastILOT8x8
		(
			REAL32 *		ptrDst,
			REAL32 *		ptrHorzCur,
			REAL32 *		ptrVertCur
		) ;

	// RGB-YUV 色空間変換
	void eriConvertRGBtoYUV
		( REAL32 * ptrBuf1, REAL32 * ptrBuf2,
			REAL32 * ptrBuf3, unsigned int nCount ) ;

} ;


#endif
