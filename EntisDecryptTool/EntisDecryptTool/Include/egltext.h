
/*****************************************************************************
                          Entis Graphic Library
 -----------------------------------------------------------------------------
    Copyright (c) 2002-2004 Leshade Entis, Entis-soft. Al rights reserved.
 *****************************************************************************/


#if	!defined(__EGLTEXT_H__)
#define	__EGLTEXT_H__

#if	!defined(COMPACT_NOA_DECODER)

/*****************************************************************************
						GDI フォントオブジェクト
 ****************************************************************************/

class	EFontObject : public	ESLObject
{
public:		// Constructer and destructer
	EFontObject( void ) ;
	EFontObject( int nHeight, int nWidth = 0,
		int nEscapement = 0, int nOrientation = 0,
		int nWeight = FW_DONTCARE, bool fItalic = false,
		bool fUnderline = false, bool fStrikeOut = false,
		BYTE fCharSet = DEFAULT_CHARSET,
		BYTE fOutPrecision = OUT_DEFAULT_PRECIS,
		BYTE fClipPrecision = CLIP_DEFAULT_PRECIS,
		BYTE fQuality = DEFAULT_QUALITY,
		BYTE fPitchAndFamily = (DEFAULT_PITCH | FF_DONTCARE),
		const char * pszFaceName = NULL ) ;
	EFontObject( const LOGFONT & logfont ) ;
	virtual ~EFontObject( void ) ;
	// class information
	DECLARE_CLASS_INFO( EFontObject, ESLObject )

protected:	// Data members
	HFONT	m_hFont ;
	LOGFONT	m_LogFont ;

public:		// Attribute
	// Font object handle
	operator HFONT ( void ) const
		{	return	m_hFont ;	}
	// Font log information
	LOGFONT & LogFont( void )
		{	return	m_LogFont ;	}
	// Font size
	int GetSize( int * pWidth = NULL ) const ;
	void SetSize( int nHeight, int nWidth = 0 ) ;
	// Weight
	int GetWeight( void ) const ;
	void SetWeight( int nWeight ) ;
	// Italic
	bool IsItalic( void ) const ;
	void SetItalic( bool fItalic = true ) ;
	// Underline
	bool IsUnderline( void ) const ;
	void SetUnderline( bool fUnderline = true ) ;
	// Strike out
	bool IsStrikeOut( void ) const ;
	void SetStrikeOut( bool fStrikeOut = true ) ;
	// Character set
	BYTE GetCharSet( void ) const ;
	void SetCharSet( BYTE fCharSet ) ;
	// Pitch and family
	BYTE GetPitchAndFamily( void ) const ;
	void SetPitchAndFamily( BYTE fPitchAndFamily ) ;
	// Font face name
	const char * GetFaceName( void ) const ;
	void SetFaceName( const char * pszFaceName ) ;

public:		// Operations
	// Create font object
	HFONT Create( void ) ;
	// Delete font object
	void Delete( void ) ;

} ;


/*****************************************************************************
						文字描画オブジェクト
 ****************************************************************************/

class	ERealFontImage	: public	ESLObject
{
public:
	// Constructer
	ERealFontImage( void ) ;
	// Destructer
	virtual ~ERealFontImage( void ) ;
	// class information
	DECLARE_CLASS_INFO( ERealFontImage, ESLObject )

protected:	// Data member
	HFONT			m_fontText ;		// font to drawing
	EObjArray<EImageSprite> *
					m_listCharacters ;	// character images
	EGL_RECT		m_rectView ;		// view rectangle
	EGL_PALETTE		m_colorText ;		// pixel code to draw
	unsigned int	m_nTransparency ;	// transparency to draw
	EGL_POINT		m_posCursor ;		// cursor position
	unsigned int	m_nLineHeight ;		// line height (or width) pixels
	unsigned int	m_nIndentWidth ;	// indent width (or height) pixels
	unsigned int	m_minHyphening ;	// minimum hyphening length
	EWideString		m_wstrProhibit ;	// prohibit characters

	typedef	DWORD (WINAPI *GET_GLYPH_OUTLINE_API)
		(HDC, UINT, UINT, LPGLYPHMETRICS, DWORD, LPVOID, CONST MAT2 *);
	typedef	BOOL (WINAPI *GET_CHAR_WIDTH_API)(HDC, UINT, UINT, LPINT);
	typedef	BOOL (APIENTRY *GET_CHAR_ABC_WIDTHS_API)(HDC, UINT, UINT, LPABC);

	bool					m_fVerticalWriting ;
	bool					m_fSmoothing ;

	static bool				m_fEnableSmoothing ;

	bool					m_fCanUseUnicode ;
	GET_GLYPH_OUTLINE_API	m_apiGetGlyphOutLine ;
	GET_CHAR_WIDTH_API		m_apiGetCharWidth ;
	GET_CHAR_ABC_WIDTHS_API	m_apiGetCharABCWidths ;

public:		// Drawing text
	// Get text width
	unsigned int GetTextWidth( const wchar_t * pwszText ) ;

	// Drawt text (multi-line text)
	virtual unsigned int DrawText( const wchar_t * pwszText ) ;
	// Draw text with fitting (single-line text)
	virtual ESLError FitTextToWidth
		( const wchar_t * pwszText,
			int nPosX, int nPosY, unsigned int nWidth ) ;
	// Drawing text callback function
	virtual EImageSprite * CreateNewCharacter( void ) ;
	virtual void OnDrewCharacter( EImageSprite * pCharObj ) ;

protected:
	EImageSprite * RevolveCharacter( EImageSprite * pis ) ;
	EImageSprite * CreateFromBitmap
		( const BITMAPINFO * pbmi, const void * ptrBitmap ) ;

public:		// Attribute
	// List of characters
	unsigned int GetCharacterCount( void ) const ;
	EImageSprite * GetCharacterAt( unsigned int nIndex ) ;
	void RemoveAllCharacter( void ) ;
	// Drawing characters
	ESLError DrawCharacter
		( HEGL_RENDER_POLYGON hRender, int iFirst = 0, int iEnd = -1 ) ;
	// Font object
	HFONT GetFont( void ) const
		{	return	m_fontText ;	}
	void SetFont( HFONT fontText )
		{	m_fontText = fontText ;	}
	// View rectangle
	const EGL_RECT & GetViewRect( void ) const
		{	return	m_rectView ;	}
	void SetViewRect( const EGL_RECT & rectView )
		{	m_rectView = rectView ;	}
	// Vertically writing
	bool IsVerticalWriting( void ) const
		{	return	m_fVerticalWriting ;	}
	void SetVerticalWriting( bool fVert = true )
		{	m_fVerticalWriting = fVert ;	}
	// Smoothing font
	bool IsFontSmoothing( void ) const
		{	return	m_fSmoothing && m_fEnableSmoothing ;	}
	void SetFontSmoothing( bool fSmooth = true )
		{	m_fSmoothing = fSmooth ;	}
	static bool IsEnabledFontSmoothing( void )
		{	return	m_fEnableSmoothing ;	}
	static void EnableFontSmoothing( bool fSmooth = true )
		{	m_fEnableSmoothing = fSmooth ;	}
	// Pixel code
	EGL_PALETTE GetColor( void ) const
		{	return	m_colorText ;	}
	void SetColor( EGL_PALETTE colorText )
		{	m_colorText = colorText ;	}
	// Transparency
	unsigned int GetTransparency( void ) const
		{	return	m_nTransparency ;	}
	void SetTransparency( unsigned int nTransparency )
		{	m_nTransparency = nTransparency ;	}
	// Cursor position
	EGL_POINT GetCursorPos( void ) const
		{	return	m_posCursor ;	}
	void MoveCursorPos( const EGL_POINT & posCursor )
		{	m_posCursor = posCursor ;	}
	void MoveToNextLine( unsigned int nIndent = 0 ) ;
	// Line height (or width)
	unsigned int GetLineHeight( void ) const
		{	return	m_nLineHeight ;	}
	void SetLineHeight( unsigned int nLineHeight )
		{	m_nLineHeight = nLineHeight ;	}
	// Indention width (or height)
	unsigned int GetIndentWidth( void ) const
		{	return	m_nIndentWidth ;	}
	void SetIndentWidth( unsigned int nIndentWidth )
		{	m_nIndentWidth = nIndentWidth ;	}
	// Hyphening character length
	unsigned int GetMinHyphening( void ) const
		{	return	m_minHyphening ;	}
	void SetMinHyphening( unsigned int minHyphening )
		{	m_minHyphening = minHyphening ;	}
	// Prohibit characters
	const wchar_t * GetProhibitChar( void ) const
		{	return	m_wstrProhibit ;	}
	void SetProhibitChar( const wchar_t * pwszProhibit ) ;
	bool IsProhibitChar( wchar_t wchar ) const ;
	// Check alphabet character
	bool IsAlphabetChar( wchar_t wchar ) const
		{
			return	((L'A' <= wchar) && (wchar <= L'Z'))
					|| ((L'a' <= wchar) && (wchar <= L'z'));
		}

} ;


#endif	//	!defined(COMPACT_NOA_DECODER)

#endif
