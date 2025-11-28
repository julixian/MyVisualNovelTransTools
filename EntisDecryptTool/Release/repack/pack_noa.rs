

// 引数 : <destination-noa-file> <source-files(can be wild-card)>


Console	con = System.console() ;

String[]	g_IgnoreCompressExt =
[
	"eri", "mei", "mio", "noa", "png", "jpg", "jpeg",
	"avi", "mpg", "mpeg", "mp3", "mp4", "wma", "wmv",
] ;
HashMap<boolean>	g_mapIgnoreCompressExt = HashMap<boolean>() ;


int main( String[] arg )
{
	if ( arg.length() < 2 )
	{
		con.printf( "引数を指定してください。\n"
					+ "引数 : <出力ファイル> <入力ファイル>\n" ) ;
		return	-1 ;
	}
	for ( int i = 0; i < g_IgnoreCompressExt.length(); i ++ )
	{
		g_mapIgnoreCompressExt.put( g_IgnoreCompressExt[i], true ) ;
	}
	String	strDstFile = arg[0] ;
	String	strSrcFiles = arg[1] ;
	String	strSrcDir = strSrcFiles.getFileDirectoryPart() ;
	//
	RandomAccessFile	file = null ;
	try
	{
		file = new RandomAccessFile( strDstFile, "rw" ) ;
	}
	catch ( Exception e )
	{
		con.printf( "ファイルを開けませんでした : %s\n", strDstFile ) ;
		return	1 ;
	}
	NoaFileArchiver.FileInfo[]	dirFiles = listFileInfo( strSrcFiles ) ;
	NoaFileArchiver	noafile = new NoaFileArchiver() ;
	if ( !noafile.createArchive( file, dirFiles ) )
	{
		con.printf( "ファイルへの書き出しに失敗しました : %s\n", strDstFile ) ;
		return	1 ;
	}
	int	nErrors = archiveDirectory( noafile, strSrcDir, "", dirFiles ) ;
	//
	noafile.close() ;
	//
	if ( nErrors == 0 )
	{
		con.printf( "%s への書庫化が完了しました。\n", strDstFile ) ;
	}
	else
	{
		con.printf( "%s への書庫化 : %d errors\n", strDstFile, nErrors ) ;
	}
	return	nErrors ;
}

// ディレクトリのファイルリスト作成
NoaFileArchiver.FileInfo[] listFileInfo( String strFiles )
{
	String		strDirPath = strFiles.getFileDirectoryPart() ;
	String		strFileName = strFiles.getFileNamePart() ;
	File		fileDir = new File( strDirPath ) ;
	String[]	files = fileDir.list( strFileName ) ;
	NoaFileArchiver.FileInfo[]
			fileInfos = new NoaFileArchiver.FileInfo[] ;
	for ( int i = 0; i < files.length(); i ++ )
	{
		if ( (files[i] == ".") || (files[i] == "..") )
		{
			continue ;
		}
		File	file = new File( strDirPath.offsetFilePath(files[i]) ) ;
		if ( !file.exists() )
		{
			continue ;
		}
		NoaFileArchiver.FileInfo	info = new NoaFileArchiver.FileInfo() ;
		if ( file.isDirectory() )
		{
			info.dtFileTime = new Date() ;
			info.strFilename  = file.getName() ;
			info.nAttribute = NoaFileArchiver.attrDirectory ;
		}
		else
		{
			info.dtFileTime = new Date( file.lastModified() ) ;
			info.nBytes = file.length() ;
			info.strFilename  = file.getName() ;
			info.nEncodeType = NoaFileArchiver.encodeRaw ;
			if ( g_mapIgnoreCompressExt.isEmpty
					( info.strFilename.getFileExtensionPart().toLowerCase() ) )
			{
				info.nEncodeType = NoaFileArchiver.encodeERISA ;
			}
		}
		fileInfos.add( info ) ;
	}
	return	fileInfos ;
}

// ディレクトリの書庫化
int archiveDirectory
	( NoaFileArchiver noafile,
		String strSrcDir, String strNoaDir,
		NoaFileArchiver.FileInfo[] dirFiles )
{
	int				nErrors = 0 ;
	const int		nBufSize = 0x10000 ;
	Uint8Pointer	buf = new Uint8Pointer( nBufSize ) ;
	for ( int i = 0; i < dirFiles.length(); i ++ )
	{
		const String	strFileName = dirFiles[i].strFilename ;
		const String	strSrcFile = strSrcDir.offsetFilePath( strFileName ) ;
		if ( dirFiles[i].nAttribute & NoaFileArchiver.attrDirectory )
		{
			// ディレクトリ
			NoaFileArchiver.FileInfo[]	dirSubFiles =
				listFileInfo( strSrcFile.offsetFilePath( "*.*" ) ) ;
			//
			if ( !noafile.createDirectory( strFileName, dirSubFiles ) )
			{
				con.printf( "書庫内ディレクトリ作成失敗 : %s\n", strFileName ) ;
				nErrors ++ ;
				continue ;
			}
			nErrors += archiveDirectory
				( noafile, strSrcFile,
					strNoaDir.offsetFilePath( strFileName ), dirSubFiles ) ;
			noafile.ascendDirectory() ;
		}
		else
		{
			// ファイル
			String	strArcFilePath = strNoaDir.offsetFilePath( strFileName ) ;
			InputStream	is = null ;
			try
			{
				is = new InputStream( strSrcFile ) ;
			}
			catch ( Exception e )
			{
				con.printf( "%s を開けませんでした。\n", strArcFilePath ) ;
				nErrors ++ ;
				continue ;
			}
			if ( !noafile.descendFile( strFileName, null, true ) )
			{
				con.printf( "%s の書き込みを開始できません。\n", strArcFilePath ) ;
				nErrors ++ ;
				continue ;
			}
			con.printf( "\x1b[s%s...", strArcFilePath ) ;
			//
			long	nTotalBytes = 0 ;
			try
			{
				for ( ; ; )
				{
					int	nReadBytes = is.read( buf, 0, nBufSize ) ;
					if ( nReadBytes == 0 )
					{
						break ;
					}
					noafile.write( buf, 0, nReadBytes ) ;
					nTotalBytes += nReadBytes ;
					con.printf( "\r\x1b[u%s...%d [bytes]", strArcFilePath, nTotalBytes ) ;
				}
				con.printf( "\r\x1b[u%s...%d [bytes] done.\n", strArcFilePath, nTotalBytes ) ;
			}
			catch ( Exception e )
			{
				con.printf( "\n%s の書き込みに失敗しました。\n", strArcFilePath ) ;
				nErrors ++ ;
			}
			noafile.ascendFile() ;
		}
	}
	return	nErrors ;
}



