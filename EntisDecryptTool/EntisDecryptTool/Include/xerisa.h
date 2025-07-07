
/*****************************************************************************
                         E R I S A - L i b r a r y
 -----------------------------------------------------------------------------
    Copyright (C) 2002-2004 Leshade Entis, Entis-soft. All rights reserved.
 *****************************************************************************/


#if	!defined(__ERISALIB_H__)
#define	__ERISALIB_H__

#if	!defined(ERITYPES_H_INCLUDED)
#include <eritypes.h>
#endif

#if	!defined(__ESL_H__)
#include <esl.h>
#endif


/*****************************************************************************
                       ライブラリ初期化・終了関数
 *****************************************************************************/

#if	defined(_M_IX86) && !defined(ERI_INTEL_X86)
#define	ERI_INTEL_X86
#endif

extern	"C"
{
	void eriInitializeLibrary( void ) ;
	void eriCloseLibrary( void ) ;
	void eriInitializeTask( void ) ;
	void eriCloseTask( void ) ;
#if	defined(ERI_INTEL_X86)
	void eriEnableMMX( int fForceEnable = 0 ) ;
	void eriDisableMMX( int fForceDisable = -1 ) ;
	void eriInitializeFPU( void ) ;
#endif
	void eriInitializeCodec( void ) ;
} ;

#if	defined(ERI_INTEL_X86)
#define	ERI_USE_MMX_PENTIUM	0x0002
#define	ERI_USE_XMM_P3		0x0008
#define	ERI_USE_SSE2		0x0010
extern	"C"	DWORD	ERI_EnabledProcessorType ;
#endif


/*****************************************************************************
                                画像情報
 *****************************************************************************/

struct	ERI_FILE_HEADER
{
	DWORD	dwVersion ;
	DWORD	dwContainedFlag ;
	DWORD	dwKeyFrameCount ;
	DWORD	dwFrameCount ;
	DWORD	dwAllFrameTime ;
} ;

struct	ERI_INFO_HEADER
{
	DWORD	dwVersion ;
	DWORD	fdwTransformation ;
	DWORD	dwArchitecture ;
	DWORD	fdwFormatType ;
	SDWORD	nImageWidth ;
	SDWORD	nImageHeight ;
	DWORD	dwBitsPerPixel ;
	DWORD	dwClippedPixel ;
	DWORD	dwSamplingFlags ;
	SDWORD	dwQuantumizedBits[2] ;
	DWORD	dwAllottedBits[2] ;
	DWORD	dwBlockingDegree ;
	DWORD	dwLappedBlock ;
	DWORD	dwFrameTransform ;
	DWORD	dwFrameDegree ;
} ;

#define	EFH_STANDARD_VERSION	0x00020100
#define	EFH_ENHANCED_VERSION	0x00020200

#define	EFH_CONTAIN_IMAGE		0x00000001
#define	EFH_CONTAIN_ALPHA		0x00000002
#define	EFH_CONTAIN_PALETTE		0x00000010
#define	EFH_CONTAIN_WAVE		0x00000100
#define	EFH_CONTAIN_SEQUENCE	0x00000200

#define	ERI_RGB_IMAGE			0x00000001
#define	ERI_RGBA_IMAGE			0x04000001
#define	ERI_GRAY_IMAGE			0x00000002
#define	ERI_TYPE_MASK			0x00FFFFFF
#define	ERI_WITH_PALETTE		0x01000000
#define	ERI_USE_CLIPPING		0x02000000
#define	ERI_WITH_ALPHA			0x04000000

#define	CVTYPE_LOSSLESS_ERI		0x03020000
#define	CVTYPE_DCT_ERI			0x00000001
#define	CVTYPE_LOT_ERI			0x00000005
#define	CVTYPE_LOT_ERI_MSS		0x00000105

#define	ERI_ARITHMETIC_CODE		32
#define	ERI_RUNLENGTH_GAMMA		0xFFFFFFFF
#define	ERI_RUNLENGTH_HUFFMAN	0xFFFFFFFC
#define	ERISA_NEMESIS_CODE		0xFFFFFFF0

#define	ERISF_YUV_4_4_4			0x00040404
#define	ERISF_YUV_4_2_2			0x00040202
#define	ERISF_YUV_4_1_1			0x00040101


/*****************************************************************************
                                音声情報
 *****************************************************************************/

struct	MIO_INFO_HEADER
{
	DWORD	dwVersion ;
	DWORD	fdwTransformation ;
	DWORD	dwArchitecture ;
	DWORD	dwChannelCount ;
	DWORD	dwSamplesPerSec ;
	DWORD	dwBlocksetCount ;
	DWORD	dwSubbandDegree ;
	DWORD	dwAllSampleCount ;
	DWORD	dwLappedDegree ;
	DWORD	dwBitsPerSample ;
} ;

struct	MIO_DATA_HEADER
{
	BYTE	bytVersion ;
	BYTE	bytFlags ;
	BYTE	bytReserved1 ;
	BYTE	bytReserved2 ;
	DWORD	dwSampleCount ;
} ;

#define	MIO_LEAD_BLOCK	0x01


/*****************************************************************************
                            アニメーション用関数
 *****************************************************************************/

#include <egl2d.h>

extern	"C"
{
	ESLError eriCopyImage
		( const EGL_IMAGE_INFO & eiiDst,
			const EGL_IMAGE_INFO & eiiSrc ) ;
	ESLError eriBlendHalfImage
		( const EGL_IMAGE_INFO & eiiDst,
			const EGL_IMAGE_INFO & eiiSrc1,
			const EGL_IMAGE_INFO & eiiSrc2 ) ;
	ESLError eriLLSubtractionOfFrame
		( const EGL_IMAGE_INFO & eiiDst,
			const EGL_IMAGE_INFO & eiiSrc ) ;
	int eriLSSubtractionOfFrame
		( const EGL_IMAGE_INFO & eiiDst,
			const EGL_IMAGE_INFO & eiiSrc ) ;
	DWORD eriSumAbsDifferenceOfBlock
		( const EGL_IMAGE_INFO & eiiDst,
			const EGL_IMAGE_INFO & eiiSrc ) ;
	void eriImageFilterLoop421
		( const EGL_IMAGE_INFO & eiiDst,
			const EGL_IMAGE_INFO * eiiSrc = NULL,
			SBYTE * pFlags = NULL, int nBlockSize = 16 ) ;

} ;


class	ERISADecodeContext ;
class	ERISAEncodeContext ;

class	ERISADecoder ;
class	ERISAEncoder ;

class	MIOEncoder ;
class	MIODecoder ;

class	EMCFile ;
	class	ERIFile ;
	class	ERISAArchive ;

class	MIODynamicPlayer ;
class	ERIAnimation ;
class	ERIAnimationWriter ;


#include <erisamatrix.h>
#include <erisacontext.h>
#include <erisaimage.h>

#if	!defined(__EGL_H__)
#include <egl.h>
#endif

#include <erisasound.h>
#include <erisafile.h>
#include <erisaplay.h>


#endif
