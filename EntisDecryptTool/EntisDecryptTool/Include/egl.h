
/*****************************************************************************
                          Entis Graphic Library
													last update 2002/10/20
 -----------------------------------------------------------------------------
       Copyright (c) 2002 Leshade Entis, Entis-soft. Al rights reserved.
 *****************************************************************************/


#if	!defined(__EGL_H__)
#define	__EGL_H__

#if	!defined(_WINDOWS_)
#if	!defined(_WIN32_WINNT)
#define	_WIN32_WINNT	0x0400
#endif
#define	STRICT	1
#include <windows.h>
#endif

#if	!defined(ERITYPES_H_INCLUDED)
#include <eritypes.h>
#endif

#if	!defined(__ESL_H__)
#include <esl.h>
#endif

class	EGLPalette ;
class	E3DVector2D ;
class	E3DVector ;
class	E3DVector4 ;

class	EGLImage ;
class	E3DTextureLibrary ;
class	E3DSurfaceAttribute ;
class	E3DSurfaceLibrary ;
class	E3DPolygonModel ;
class	E3DModelJoint ;
class	E3DBonePolygonModel ;
class	E3DViewPointJoint ;
class	E3DViewAngleCurve ;
class	E3DRenderPolygon ;

template <class> class	EBezierCurve ;
	class	EBezierR64 ;
	class	EBezier2D ;
	class	EBezier3D ;
class	ESprite ;
	class	EImageSprite ;
	class	EFilterSprite ;
	class	EShapeSprite ;
		class	ELineSprite ;
		class	ERectangleSprite ;
		class	EEllipseSprite ;
		class	EPolygonSprite ;
	class	E3DRenderSprite ;
	class	ESpriteServer ;

class	EFontObject ;
class	ERealFontImage ;

#include <egl2d.h>

#if	!defined(__ERISALIB_H__)
#include <xerisa.h>
#endif

#include <egl3d.h>
#include <eglsprite.h>
#include <egltext.h>

#endif
