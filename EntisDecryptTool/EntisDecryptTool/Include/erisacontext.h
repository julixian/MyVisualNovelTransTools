
/*****************************************************************************
                         E R I S A - L i b r a r y
													last update 2002/12/12
 -----------------------------------------------------------------------------
      Copyright (C) 2002 Leshade Entis, Entis-soft. All rights reserved.
 *****************************************************************************/


#if	!defined(__ERISA_CONTEXT_H__)
#define	__ERISA_CONTEXT_H__


/*****************************************************************************
                         ハフマン・ツリー・構造体
 *****************************************************************************/

#define	ERINA_CODE_FLAG			0x80000000U
#define	ERINA_HUFFMAN_ESCAPE	0x7FFFFFFF
#define	ERINA_HUFFMAN_NULL		0x8000U
#define	ERINA_HUFFMAN_MAX		0x4000
#define	ERINA_HUFFMAN_ROOT		0x200

struct	ERINA_HUFFMAN_NODE
{
	WORD	m_weight ;
	WORD	m_parent ;
	DWORD	m_child_code ;
} ;

struct	ERINA_HUFFMAN_TREE
{
	ERINA_HUFFMAN_NODE	m_hnTree[0x201] ;
	int					m_iSymLookup[0x100] ;
	int					m_iEscape ;
	int					m_iTreePointer ;

	// ツリーの初期化
	void Initialize( void ) ;
	// 発生頻度をインクリメント
	void IncreaseOccuedCount( int iEntry ) ;
	// 親の重みを再計算する
	void RecountOccuredCount( int iParent ) ;
	// ツリーの正規化
	void Normalize( int iEntry ) ;
	// 新しいフリーエントリを作成して追加
	void AddNewEntry( int nNewCode ) ;
	// 各出現数を2分の1にして木を再構成
	void HalfAndRebuild( void ) ;

} ;


/*****************************************************************************
                             算術符号統計モデル
 *****************************************************************************/

#define	ERISA_TOTAL_LIMIT	0x2000		// 母数の限界値
#define	ERISA_SYMBOL_SORTS	0x101		// シンボルの種類
#define	ERISA_SUB_SORT_MAX	0x80
#define	ERISA_PROB_SLOT_MAX	0x800		// 統計モデルの最大スロット数
#define	ERISA_ESC_CODE		(-1)		// エスケープ記号

struct	ERISA_CODE_SYMBOL
{
	WORD	wOccured ;					// シンボルの出現回数
	SWORD	wSymbol ;					// シンボル（下位8ビットのみ）
} ;

struct	ERISA_PROB_MODEL
{
	DWORD				dwTotalCount ;			// 母数 < 2000H
	DWORD				dwSymbolSorts ;			// シンボルの種類数
	DWORD				dwReserved[2] ;
	ERISA_CODE_SYMBOL	acsSymTable[ERISA_SYMBOL_SORTS] ;
												// 統計モデル
	DWORD				dwReserved2[3] ;
	ERISA_CODE_SYMBOL	acsSubModel[ERISA_SUB_SORT_MAX] ;
												// サブ統計モデル（指標）

	// 統計情報の初期化
	void Initialize( void ) ;
	// 所要ビット数を計算する
	int AccumulateProb( SWORD wSymbol ) ;
	// シンボルの生起数を 1/2 にする
	void HalfOccuredCount( void ) ;
	// 指定のシンボルの生起数をインクリメントする
	int IncreaseSymbol( int index ) ;
	// 指定のシンボルの指標を取得する
	int FindSymbol( SWORD wSymbol ) ;
	// 指定のシンボルを追加する
	int AddSymbol( SWORD wSymbol ) ;
} ;

struct	ERISA_PROB_BASE
{
	ERISA_PROB_MODEL *	ptrProbWork ;			// 統計データ用ワーク
	DWORD				dwWorkUsed ;			// 使用されているスロット数
	DWORD				dwReserved[2] ;			// 16 バイトアライン
	ERISA_PROB_MODEL	epmBaseModel ;			// ベース統計モデル
	ERISA_PROB_MODEL *	ptrProbIndex[ERISA_PROB_SLOT_MAX] ;

	static const int	m_nShiftCount[4] ;
	static const int	m_nNewProbLimit[4] ;
} ;


/*****************************************************************************
                        スライド辞書参照テーブル
 *****************************************************************************/

#define	NEMESIS_BUF_SIZE	0x10000
#define	NEMESIS_BUF_MASK	0xFFFF
#define	NEMESIS_INDEX_LIMIT	0x100
#define	NEMESIS_INDEX_MASK	0xFF

struct	ERISAN_PHRASE_LOOKUP
{
	DWORD	first ;
	DWORD	index[NEMESIS_INDEX_LIMIT] ;
} ;


/*****************************************************************************
                             BSHF 符号バッファ
 *****************************************************************************/

class	ERIBshfBuffer	: public	ESLObject
{
public:
	EString	m_strPassword ;				// パスワード
	DWORD	m_dwPassOffset ;			// パスワードオフセット
	BYTE	m_bufBSHF[32] ;				// BSHF 符号用バッファ
	BYTE	m_srcBSHF[32] ;
	BYTE	m_maskBSHF[32] ;

public:
	// 構築関数
	ERIBshfBuffer( void ) ;
	// 消滅関数
	virtual ~ERIBshfBuffer( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( ERIBshfBuffer, ESLObject )

public:
	// 符号化
	void EncodeBuffer( void ) ;
	// 復号
	void DecodeBuffer( void ) ;

} ;


/*****************************************************************************
                        ERISA 符号展開コンテキスト
 *****************************************************************************/

class	ERISADecodeContext	: public	ESLObject
{
protected:
	// ビットストリームバッファ
	int		m_nIntBufCount ;	// 中間入力バッファに蓄積されているビット数
	DWORD	m_dwIntBuffer ;		// 中間入力バッファ
	ULONG	m_nBufferingSize ;	// バッファリングするバイト数
	ULONG	m_nBufCount ;		// バッファの残りバイト数
	PBYTE	m_ptrBuffer ;		// 入力バッファの先頭へのポインタ
	PBYTE	m_ptrNextBuf ;		// 次に読み込むべき入力バッファへのポインタ

	ESLFileObject *	m_pFile ;	// 入力オブジェクト
	ERISADecodeContext *	m_pContext ;

	// 現在使用されている符号の展開関数
	ULONG (ERISADecodeContext::*m_pfnDecodeSymbolBytes)
						( SBYTE * ptrDst, ULONG nCount ) ;

	// ランレングスガンマ符号コンテキスト
	int		m_flgZero ;			// ゼロフラグ
	ULONG	m_nLength ;			// ランレングス

	// ERINA（ハフマン）符号コンテキスト
	DWORD					m_dwERINAFlags ;
	ERINA_HUFFMAN_TREE *	m_pLastHuffmanTree ;
	ERINA_HUFFMAN_TREE **	m_ppHuffmanTree ;

	// ERISA（算術）符号コンテキスト
	DWORD	m_dwCodeRegister ;			// コードレジスタ（16 bit）
	DWORD	m_dwAugendRegister ;		// オージェンドレジスタ（16 bit）
	int		m_nPostBitCount ;			// 終端ビットバッファカウンタ
	BYTE	m_bytLastSymbol[4] ;		// 最近の生起シンボル
	int		m_iLastSymbol ;
	ERISA_PROB_BASE *	m_pProbERISA ;
	// ERISA-N 符号コンテキスト
	int		m_nNemesisLeft ;			// スライド辞書復号カウンタ
	int		m_nNemesisNext ;
	BYTE *	m_pNemesisBuf ;				// スライド辞書用バッファ
	int		m_nNemesisIndex ;
	ERISAN_PHRASE_LOOKUP *	m_pNemesisLookup ;
	ERISA_PROB_MODEL *	m_pPhraseLenProb ;
	ERISA_PROB_MODEL *	m_pPhraseIndexProb ;
	ERISA_PROB_MODEL *	m_pRunLenProb ;
	ERISA_PROB_MODEL *	m_pLastERISAProb ;
	ERISA_PROB_MODEL **	m_ppTableERISA ;
	int		m_nFlagEOF ;

	// BSHF 符号コンテキスト
	ERIBshfBuffer *	m_pBshfBuf ;
	DWORD			m_dwBufPos ;

public:
	// 構築関数
	ERISADecodeContext( ULONG nBufferingSize ) ;
	// 消滅関数
	virtual ~ERISADecodeContext( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( ERISADecodeContext, ESLObject )

public:
	// 入力ファイルオブジェクトを関連付ける
	void AttachInputFile( ESLFileObject * pfile ) ;
	// 入力コンテキストオブジェクトを関連付ける
	void AttachInputContext( ERISADecodeContext * pcontext ) ;
	// 次のデータを読み込む
	virtual ULONG ReadNextData( PBYTE ptrBuffer, ULONG nBytes ) ;

public:
	// バッファが空の時、次のデータを読み込む
	ESLError PrefetchBuffer( void ) ;
	// 1ビット取得する（ 0 又は－1を返す ）
	int GetABit( void ) ;
	// nビット取得する
	UINT GetNBits( int n ) ;
	// バッファをフラッシュする
	void FlushBuffer( void ) ;
	// データを復号する
	ULONG DecodeSymbolBytes( SBYTE * ptrDst, ULONG nCount )
		{
			ESLAssert( m_pfnDecodeSymbolBytes != NULL ) ;
			return	(this->*m_pfnDecodeSymbolBytes)( ptrDst, nCount ) ;
		}

#if	!defined(COMPACT_NOA_DECODER)
public:
	// ランレングスガンマ符号コンテキストを初期化する
	void PrepareGammaCode( void ) ;
	// ランレングスガンマ符号のゼロフラグを読み込む
	void InitGammaContext( void ) ;
	// ガンマコードを取得する
	int GetGammaCode( void ) ;
	// ランレングスガンマ符号を復号する
	ULONG DecodeGammaCodeBytes( SBYTE * ptrDst, ULONG nCount ) ;

public:
	// 圧縮方式
	enum	ERINAEncodingFlag
	{
		efERINAOrder0	= 0x0000,
		efERINAOrder1	= 0x0001
	} ;
	// ERINA 符号の復号の準備をする
	void PrepareToDecodeERINACode( DWORD dwFlags = efERINAOrder1 ) ;
	// ハフマン符号を取得する
	int GetHuffmanCode( ERINA_HUFFMAN_TREE * tree ) ;
	// 長さのハフマン符号を取得する
	int GetLengthHuffman( ERINA_HUFFMAN_TREE * tree ) ;
	// ERINA 符号を復号する
	ULONG DecodeERINACodeBytes( SBYTE * ptrDst, ULONG nCount ) ;

#endif	//	!defined(COMPACT_NOA_DECODER)

public:
	// ERISA 符号の復号の準備をする
	void PrepareToDecodeERISACode( void ) ;
	// ERISA-N 符号の復号の準備をする
	void PrepareToDecodeERISANCode( void ) ;
	// 算術符号の復号の初期化を行う
	void InitializeERISACode( void ) ;
	// 指定の統計モデルを使って1つの算術符号を復号
	int DecodeERISACode( ERISA_PROB_MODEL * pModel ) ;
	int DecodeERISACodeIndex( ERISA_PROB_MODEL * pModel ) ;
	// ERISA 符号を復号する
	ULONG DecodeERISACodeBytes( SBYTE * ptrDst, ULONG nCount ) ;
	ULONG DecodeERISACodeWords( SWORD * ptrDst, ULONG nCount ) ;
	// ERISA 符号を復号する
	ULONG DecodeERISANCodeBytes( SBYTE * ptrDst, ULONG nCount ) ;
	// EOF フラグを取得する
	int GetEOFFlag( void ) const
		{
			return	m_nFlagEOF ;
		}

public:
	// BSHF 符号の復号の準備をする
	void PrepareToDecodeBSHFCode( const char * pszPassword ) ;
	// BSHF 符号を復号する
	ULONG DecodeBSHFCodeBytes( SBYTE * ptrDst, ULONG nCount ) ;

} ;


/*****************************************************************************
                        ERISA 符号圧縮コンテキスト
 *****************************************************************************/

 #if	!defined(COMPACT_NOA_DECODER)

class	ERISAEncodeContext	: public	ESLObject
{
protected:
	// ビットストリームバッファ
	int		m_nIntBufCount ;	// 中間入力バッファに蓄積されているビット数
	DWORD	m_dwIntBuffer ;		// 中間入力バッファ
	ULONG	m_nBufferingSize ;	// バッファリングするバイト数
	ULONG	m_nBufCount ;		// バッファに蓄積されているバイト数
	PBYTE	m_ptrBuffer ;		// 出力バッファの先頭へのポインタ

	ESLFileObject *	m_pFile ;			// 出力オブジェクト
	ERISAEncodeContext *	m_pContext ;

	// 現在使用されている符号の符号化関数
	ULONG (ERISAEncodeContext::*m_pfnEncodeSymbolBytes)
						( const SBYTE * ptrSrc, ULONG nCount ) ;
	ESLError (ERISAEncodeContext::*m_pfnFinishEncoding)( void ) ;

	// ERINA（ハフマン）符号コンテキスト
	DWORD					m_dwERINAFlags ;
	ERINA_HUFFMAN_TREE *	m_pLastHuffmanTree ;
	ERINA_HUFFMAN_TREE **	m_ppHuffmanTree ;

	// ERISA（算術）符号コンテキスト
	DWORD	m_dwCodeRegister ;			// コードレジスタ（16 bit）
	DWORD	m_dwAugendRegister ;		// オージェンドレジスタ（16 bit）
	DWORD	m_dwCodeBuffer ;			// コードレジスタバッファ
	SDWORD	m_dwBitBufferCount ;		// 出力された'1'の数を蓄積する
			// '0' は、次に '0' が出力された時点で、すぐに送出される。
			// '1' は、'0' が出力されるまで蓄積される。
			// -1 は、空のビット列 '' を、
			// 0 は、ビット列 '0' を、
			// 1 は、ビット列 '01' を、
			// 2 は、ビット列 '011' を表現します。

	BYTE	m_bytLastSymbol[4] ;		// 最近の生起シンボル
	int		m_iLastSymbol ;
	ERISA_PROB_BASE *	m_pProbERISA ;	// 統計モデル用ワークメモリ
	// ERISA-N 符号コンテキスト
	DWORD	m_dwERISAFlags ;			// 圧縮方式
	BYTE *	m_pNemesisBuf ;				// スライド辞書用バッファ
	int		m_nNemesisIndex ;
	ERISAN_PHRASE_LOOKUP *	m_pNemesisLookup ;
	ERISA_PROB_MODEL *	m_pPhraseLenProb ;
	ERISA_PROB_MODEL *	m_pPhraseIndexProb ;
	ERISA_PROB_MODEL *	m_pRunLenProb ;
	ERISA_PROB_MODEL *	m_pLastERISAProb ;
	ERISA_PROB_MODEL **	m_ppTableERISA ;

	// BSHF 符号コンテキスト
	ERIBshfBuffer *	m_pBshfBuf ;
	DWORD			m_dwBufPos ;

public:
	// 構築関数
	ERISAEncodeContext( ULONG nBufferingSize ) ;
	// 消滅関数
	virtual ~ERISAEncodeContext( void ) ;
	// クラス情報
	DECLARE_CLASS_INFO( ERISAEncodeContext, ESLObject )

public:
	// 出力ファイルオブジェクトを関連付ける
	void AttachOutputFile( ESLFileObject * pfile ) ;
	// 出力コンテキストオブジェクトを関連付ける
	void AttachOutputContext( ERISAEncodeContext * pcontext ) ;
	// バッファの内容を出力する
	virtual ULONG WriteNextData( const BYTE * ptrBuffer, ULONG nBytes ) ;

public:
	// ｎビット出力する
	ESLError OutNBits( DWORD dwData, int nBits ) ;
	// バッファの内容を出力して空にする
	ESLError Flushout( void ) ;
	// データを符号化する
	ULONG EncodeSymbolBytes( const SBYTE * ptrSrc, ULONG nCount )
		{
			ESLAssert( m_pfnEncodeSymbolBytes != NULL ) ;
			return	(this->*m_pfnEncodeSymbolBytes)( ptrSrc, nCount ) ;
		}
	// 符号化を終了する
	ESLError FinishEncoding( void )
		{
			ESLAssert( m_pfnFinishEncoding != NULL ) ;
			return	(this->*m_pfnFinishEncoding)( ) ;
		}

public:
	// ガンマ符号の符号化の準備をする
	void PrepareToEncodeGammaCode( void ) ;
	// ガンマコードに符号化した際のビット数を計算
	static ULONG EstimateGammaCode( int num ) ;
	// ランレングスガンマ符号に符号化した際のサイズ（ビット数）を計算
	static ULONG EstimateGammaCodeBytes( const SBYTE * ptrSrc, ULONG nCount ) ;
	// ガンマコードを出力する
	ESLError OutGammaCode( int num ) ;
	// ランレングスガンマ符号に符号化して出力する
	ULONG EncodeGammaCodeBytes( const SBYTE * ptrSrc, ULONG nCount ) ;

public:
	// 圧縮方式
	enum	ERINAEncodingFlag
	{
		efERINAOrder0	= 0x0000,
		efERINAOrder1	= 0x0001
	} ;
	// ERINA 符号の符号化の準備をする
	void PrepareToEncodeERINACode( DWORD dwFlags = efERINAOrder1 ) ;
	// ハフマン符号で出力する
	ESLError OutHuffmanCode( ERINA_HUFFMAN_TREE * tree, int num ) ;
	// 長さをハフマン符号で出力する
	ESLError OutLengthHuffman( ERINA_HUFFMAN_TREE * tree, int length ) ;
	// ERINA 符号に符号化して出力する
	ULONG EncodeERINACodeBytes( const SBYTE * ptrSrc, ULONG nCount ) ;

public:
	// 圧縮方式
	enum	ERISAEncodingFlag
	{
		efSimple		= 0x0000,
		efNemesis		= 0x0001,
		efRunLength		= 0x0002,
		efRLNemesis		= 0x0003
	} ;
	// ERISA 符号の符号化の準備をする
	void PrepareToEncodeERISACode( void ) ;
	// ERISA-N 符号の符号化の準備をする
	void PrepareToEncodeERISANCode( DWORD dwFlags = efNemesis ) ;
	// 指定の統計モデルを使って1つの算術符号を出力
	int EncodeERISACodeSymbol
		( ERISA_PROB_MODEL * pModel, SWORD wSymbol ) ;
	int EncodeERISACodeIndex
		( ERISA_PROB_MODEL * pModel, int iSym, WORD wFs ) ;
	// ERISA 符号に符号化して出力する
	ULONG EncodeERISACodeBytes( const SBYTE * ptrSrc, ULONG nCount ) ;
	ULONG EncodeERISACodeWords( const SWORD * ptrSrc, ULONG nCount ) ;
	// ERISA-N 符号に符号化して出力する
	ULONG EncodeERISANCodeBytes( const SBYTE * ptrSrc, ULONG nCount ) ;
	// ERISA-N 符号の EOF を出力する
	void EncodeERISANCodeEOF( void ) ;
	// ERISA 符号を完了する
	ESLError FinishERISACode( void ) ;

public:
	// BSHF 符号の暗号化の準備をする
	void PrepareToEncodeBSHFCode( const char * pszPassword ) ;
	// BSHF 符号に暗号化する
	ULONG EncodeBSHFCodeBytes( const SBYTE * ptrSrc, ULONG nCount ) ;
	// BSHF 符号を完了する
	ESLError FinishBSHFCode( void ) ;

} ;

#endif	// !defined(COMPACT_NOA_DECODER)

#endif
