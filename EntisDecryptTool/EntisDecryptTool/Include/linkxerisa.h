
//#pragma	comment( lib, "comctl32.lib" )

#if	defined(_DEBUG)
	#if	!defined(_MFC_VER)
		#pragma	comment( lib, "xerisa_db.lib" )
	#else
		#pragma	comment( lib, "xerisa_mfcdb.lib" )
	#endif
#else
	#if	!defined(_MFC_VER)
		#pragma	comment( lib, "xerisa.lib" )
	#else
		#pragma	comment( lib, "xerisa_mfc.lib" )
	#endif
#endif
