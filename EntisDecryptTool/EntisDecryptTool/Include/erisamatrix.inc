
; ****************************************************************************
;                        E R I S A - L i b r a r y
; ----------------------------------------------------------------------------
;         Copyright (C) 2000-2003 Leshade Entis. All rights reserved.
; ****************************************************************************



;
;	DCT行列演算用定数
; ----------------------------------------------------------------------------

MIN_DCT_DEGREE	EQU	2
MAX_DCT_DEGREE	EQU	12


;
;	実数丸め関数
; ----------------------------------------------------------------------------

eriRoundR32ToInt	PROTO	NEAR32 C , r:REAL4
eriRoundR64ToLInt	PROTO	NEAR32 C , r:REAL8
eriRoundR32ToWordArray	PROTO	NEAR32 C ,
	ptrDst:PTR SWORD, nStep:DWORD, ptrSrc:PTR REAL4, nCount:DWORD
eriConvertArrayByteToR32	PROTO	NEAR32 C ,
	ptrDst:PTR REAL4, ptrSrc:PTR BYTE, nCount:SDWORD
eriConvertArraySByteToR32	PROTO	NEAR32 C ,
	ptrDst:PTR REAL4, ptrSrc:PTR SBYTE, nCount:SDWORD
eriConvertArrayR32ToByte	PROTO	NEAR32 C ,
	ptrDst:PTR BYTE, ptrSrc:PTR REAL4, nCount:SDWORD
eriConvertArrayR32ToSByte	PROTO	NEAR32 C ,
	ptrDst:PTR SBYTE, ptrSrc:PTR REAL4, nCount:SDWORD


;
;	回転行列構造体
; ----------------------------------------------------------------------------

SIN_COS	STRUCT

rSin	REAL4	?
rCos	REAL4	?

SIN_COS	ENDS


;
;	DCT 行列演算関数
; ----------------------------------------------------------------------------

eriInitializeMatrix	PROTO	NEAR32 C

eriScalarMultiply	PROTO	NEAR32 C ,
		ptrDst:PTR REAL4,
		rScalar:REAL4,
		nCount:DWORD

eriVectorMultiply	PROTO	NEAR32 C ,
		ptrDst:PTR REAL4,
		ptrSrc:PTR REAL4,
		nCount:DWORD

eriFastDCT_FPU		PROTO	NEAR32 C ,
		ptrDst:PTR REAL4,
		nDstInterval:DWORD,
		ptrSrc:PTR REAL4,
		ptrWorkBuf:PTR REAL4,
		nDegreeDCT:DWORD

eriFastDCT_XMM		PROTO	NEAR32 C ,
		ptrDst:PTR REAL4,
		nDstInterval:DWORD,
		ptrSrc:PTR REAL4,
		ptrWorkBuf:PTR REAL4,
		nDegreeDCT:DWORD

eriFastIDCT_FPU		PROTO	NEAR32 C ,
		ptrDst:PTR REAL4,
		ptrSrc:PTR REAL4,
		nSrcInterval:DWORD,
		ptrWorkBuf:PTR REAL4,
		nDegreeDCT:DWORD

eriFastIDCT_XMM		PROTO	NEAR32 C ,
		ptrDst:PTR REAL4,
		ptrSrc:PTR REAL4,
		nSrcInterval:DWORD,
		ptrWorkBuf:PTR REAL4,
		nDegreeDCT:DWORD

eriFastDCT		PROTO	NEAR32 C ,
		ptrDst:PTR REAL4,
		nDstInterval:DWORD,
		ptrSrc:PTR REAL4,
		ptrWorkBuf:PTR REAL4,
		nDegreeDCT:DWORD

eriFastIDCT		PROTO	NEAR32 C ,
		ptrDst:PTR REAL4,
		ptrSrc:PTR REAL4,
		nSrcInterval:DWORD,
		ptrWorkBuf:PTR REAL4,
		nDegreeDCT:DWORD

eriFastLOT8x8		PROTO	NEAR32 C,
		ptrDst:PTR REAL4,
		ptrHorzCur:PTR REAL4,
		ptrVertCur:PTR REAL4

eriFastILOT8x8		PROTO	NEAR32 C,
		ptrDst:PTR REAL4,
		ptrHorzCur:PTR REAL4,
		ptrVertCur:PTR REAL4

;
;	高速 DCT 変換マクロ
; ----------------------------------------------------------------------------
; パラメータ；
;	ESI register	: 入力データへのポインタ
;	EDI register	: 出力バッファへのポインタ
;	EBX register	: 中間バッファへのポインタ
;	EDX register	: 出力先のステップ数（バイト単位）
;	degree		: DCT 次数（即値）
;	xmm_flag	: 0 の時、XMM レジスタを使用しない（即値）
; レジスタ；
;	EBX, EDX, ESI, EDI, EBP, ESP レジスタの内容は保証される
; ----------------------------------------------------------------------------

@FastDCT	MACRO	degree:Req, xmm_flag:Req
	LOCAL	degree_num, half_degree

	IF	(degree LT MIN_DCT_DEGREE) OR (degree GT MAX_DCT_DEGREE)
		.ERR
	ENDIF
	;
	IF	degree EQ MIN_DCT_DEGREE
		;
		;	4次 DCT は特殊条件
		; ------------------------------------------------------------
		IF	xmm_flag
			;
			;	XMM 専用コード
			; ----------------------------------------------------
			lea	ecx, [edx + edx * 2]
			;
			movss	xmm0, [esi]
			movss	xmm1, [esi + 4]
			movss	xmm2, [esi + 8]
			shufps	xmm0, xmm0, 0
			movss	xmm3, [esi + 12]
			;
			shufps	xmm1, xmm1, 0
			mulps	xmm0, ERI_MatrixDCT4[0]
			shufps	xmm2, xmm2, 0
			mulps	xmm1, ERI_MatrixDCT4[16]
			shufps	xmm3, xmm3, 0
			;
			mulps	xmm2, ERI_MatrixDCT4[32]
			addps	xmm0, xmm1
			mulps	xmm3, ERI_MatrixDCT4[48]
			;
			addps	xmm2, xmm3
			addps	xmm0, xmm2
			movss	[edi], xmm0
			movaps	xmm1, xmm0
			;
			movaps	xmm2, xmm0
			shufps	xmm0, xmm0, 00111001B
			shufps	xmm1, xmm1, 01001110B
			movss	[edi + edx],     xmm0
			shufps	xmm2, xmm2, 10010011B
			movss	[edi + edx * 2], xmm1
			movss	[edi + ecx],     xmm2

		Else
			;
			;	FPU 互換コード
			; ----------------------------------------------------
			lea	ecx, [edx + edx * 2]
			;
			; 交差加減算（前半）
			;
			fld	REAL4 PTR [esi]
			fadd	REAL4 PTR [esi + 12]
			fld	REAL4 PTR [esi + 4]
			fadd	REAL4 PTR [esi + 8]
			;
			; 前半 ： A2 * DCT2
			;
			fld	st(1)
			fadd	st, st(1)
			fmul	ERI_rHalf
			fstp	REAL4 PTR [edi]
			fsubp	st(1), st
			fmul	ERI_rCosPI4
			fstp	REAL4 PTR [edi + edx * 2]
			;
			; 交差加減算（後半）
			;
			fld	REAL4 PTR [esi + 4]
			fsub	REAL4 PTR [esi + 8]
			fld	REAL4 PTR [esi]
			fsub	REAL4 PTR [esi + 12]
			;
			; 後半 ： R2 * 2 * A2 * DCT2 * K2
			;
			fld	ERI_DCTofK2[0]
			fld	ERI_DCTofK2[4]
			fxch	st(1)
			fmulp	st(2), st
			fmulp	st(2), st
			;
			fld	st(1)
			fadd	st, st(1)
			fxch	st(2)
			fsubp	st(1), st
			fmul	ERI_r2CosPI4
			;
			fsub	st, st(1)
			;
			fstp	REAL4 PTR [edi + ecx]
			fstp	REAL4 PTR [edi + edx]
		ENDIF

	Else
		;
		;	汎用 DCT 変換
		; ------------------------------------------------------------
		;              | I   J |
		; 交差加減算 = |       |
		;              | I  -J |
		degree_num = 1 shl degree
		half_degree = degree_num / 2
		xor	ecx, ecx
		IF	xmm_flag
			mov	eax, degree_num - 4
			IF	half_degree NE 4
			.REPEAT
			ENDIF
				movups	xmm1, [esi + eax * 4]
				movups	xmm0, [esi + ecx * 4]
				shufps	xmm1, xmm1, 00011011B
				movaps	xmm2, xmm0
				addps	xmm0, xmm1
				subps	xmm2, xmm1
				movups	[ebx + ecx * 4], xmm0
				movups	[ebx + ecx * 4 + half_degree * 4], xmm2
			IF	half_degree NE 4
				add	ecx, 4
				sub	eax, 4
			.UNTIL	ecx >= half_degree
			ENDIF
		Else
			mov	eax, degree_num - 1
			.REPEAT
				fld	REAL4 PTR [esi + eax * 4]
				fld	REAL4 PTR [esi + ecx * 4]
				fld	st(1)
				fadd	st, st(1)
				fxch	st(2)
				fsubp	st(1), st
				fxch	st(1)
				fstp	REAL4 PTR [ebx + ecx * 4]
				fstp	REAL4 PTR [ebx + ecx * 4 + half_degree * 4]
				dec	eax
				inc	ecx
			.UNTIL	ecx >= half_degree
		ENDIF
		;
		; 前半　DCT ： A * DCT
		;
		push	ebx
		push	esi
		push	edi
		shl	edx, 1
		xchg	ebx, esi
		@FastDCT	%(degree - 1), xmm_flag
		mov	ebx, esi
		shr	edx, 1
		;
		; 後半 DCT-IV ： R * 2 * A * DCT * K
		;
		mov	eax, ERI_pMatrixDCTofK[(degree - 1) * 4]
		add	esi, (half_degree) * (SIZEOF REAL4)
		add	edi, edx
		;
		IF	xmm_flag
			IF	degree EQ 3
				movups	xmm0, [esi]
				mulps	xmm0, [eax]
				movups	[esi], xmm0
			ElseIf	degree EQ 4
				movups	xmm0, [esi]
				movups	xmm1, [esi + 16]
				mulps	xmm0, [eax]
				mulps	xmm1, [eax + 16]
				movups	[esi],      xmm0
				movups	[esi + 16], xmm1
			Else
				xor	ecx, ecx
				.REPEAT
					movups	xmm0, [esi + ecx]
					movups	xmm1, [esi + ecx + 16]
					mulps	xmm0, [eax]
					mulps	xmm1, [eax + 16]
					movups	[esi + ecx],      xmm0
					movups	[esi + ecx + 16], xmm1
					add	ecx, (SIZEOF REAL4) * 8
					add	eax, (SIZEOF REAL4) * 8
				.UNTIL	ecx >= half_degree * (SIZEOF REAL4)
			ENDIF
		Else
			xor	ecx, ecx
			.REPEAT
				fld	REAL4 PTR [esi + ecx * 4]
				fld	REAL4 PTR [esi + ecx * 4 + 4]
				fxch	st(1)
				fmul	REAL4 PTR [eax]
				fxch	st(1)
				fmul	REAL4 PTR [eax + 4]
				fxch	st(1)
				add	eax, (SIZEOF REAL4) * 2
				fstp	REAL4 PTR [esi + ecx * 4]
				fstp	REAL4 PTR [esi + ecx * 4 + 4]
				add	ecx, 2
			.UNTIL	ecx >= half_degree
		ENDIF
		;
		shl	edx, 1
		@FastDCT	%(degree - 1), xmm_flag
		;
		IF	xmm_flag
			movss	xmm0, [edi]
			addss	xmm0, xmm0
			mov	ecx, half_degree - 1
			movss	[edi], xmm0
			add	edi, edx
			;
			.REPEAT
				movss	xmm1, [edi]
				addss	xmm1, xmm1
				subss	xmm1, xmm0
				movss	[edi], xmm1
				add	edi, edx
				movaps	xmm0, xmm1
				dec	ecx
			.UNTIL	ZERO?
		ELSE
			fld	REAL4 PTR [edi]
			fadd	st(0), st
			fst	REAL4 PTR [edi]
			add	edi, edx
			;
			mov	ecx, half_degree - 1
			.REPEAT
				fld	REAL4 PTR [edi]
				fadd	st(0), st
				fsubrp	st(1), st
				fst	REAL4 PTR [edi]
				add	edi, edx
				dec	ecx
			.UNTIL	ZERO?
			;
			fstp	st(0)
		ENDIF
		;
		shr	edx, 1
		pop	edi
		pop	esi
		pop	ebx

	ENDIF

		ENDM


;
;	高速 IDCT 変換マクロ
; ----------------------------------------------------------------------------
; パラメータ；
;	ESI register	: 入力データへのポインタ
;	EDI register	: 出力バッファへのポインタ
;	EBX register	: 中間バッファへのポインタ
;	EDX register	: ソースのステップ数（バイト単位）
;	degree		: DCT 次数（即値）
;	xmm_flag	: 0 の時、XMM レジスタを使用しない（即値）
; レジスタ；
;	EBX, EDX, ESI, EDI, EBP, ESP レジスタの内容は保証される
; ----------------------------------------------------------------------------

@FastIDCT	MACRO	degree:Req, xmm_flag:Req
	LOCAL	degree_num, half_degree

	IF	(degree LT MIN_DCT_DEGREE) OR (degree GT MAX_DCT_DEGREE)
		.ERR
	ENDIF
	;
	IF	degree EQ MIN_DCT_DEGREE
		;
		;	4次 DCT は特殊条件
		; ------------------------------------------------------------
		IF	xmm_flag
			;
			;	XMM 専用コード
			; ----------------------------------------------------
			lea	ecx, [edx + edx * 2]
			;
			movss	xmm0, [esi]
			movss	xmm1, [esi + edx]
			movss	xmm2, [esi + edx * 2]
			shufps	xmm0, xmm0, 0
			movss	xmm3, [esi + ecx]
			;
			shufps	xmm1, xmm1, 0
			mulps	xmm0, ERI_MatrixIDCT4[0]
			shufps	xmm2, xmm2, 0
			mulps	xmm1, ERI_MatrixIDCT4[16]
			shufps	xmm3, xmm3, 0
			;
			mulps	xmm2, ERI_MatrixIDCT4[32]
			addps	xmm0, xmm1
			mulps	xmm3, ERI_MatrixIDCT4[48]
			;
			addps	xmm2, xmm3
			addps	xmm0, xmm2
			;
			movups	[edi], xmm0

		Else
			;
			;	FPU 互換コード
			; ----------------------------------------------------
			lea	ecx, [edx + edx * 2]
			;
			; 偶数行 ： IDCT2
			;
			fld	REAL4 PTR [esi + edx * 2]
			fmul	ERI_rCosPI4
			fld	REAL4 PTR [esi]
			;
			fld	st(1)
			fadd	st, st(1)
			fxch	st(2)
			fsubp	st(1), st
			;
			; 奇数行 ： R * 2 * A * DCT * K
			;
			fld	REAL4 PTR [esi + ecx]
			fmul	ERI_DCTofK2[4]
			fld	REAL4 PTR [esi + edx]
			fmul	ERI_DCTofK2[0]
			;
			fld	st(1)
			fadd	st, st(1)
			fxch	st(2)
			fsubp	st(1), st
			fmul	ERI_r2CosPI4
			;
			fsub	st, st(1)
			;
			; 交差加減算
			;
			fld	st(0)
			fadd	st, st(3)
			fstp	REAL4 PTR [edi + 4]
			fsubp	st(2), st
			;
			fld	st(0)
			fadd	st, st(3)
			fstp	REAL4 PTR [edi]
			fsubp	st(2), st
			;
			fstp	REAL4 PTR [edi + 8]
			fstp	REAL4 PTR [edi + 12]
		ENDIF

	Else
		;
		;	汎用 IDCT 変換
		; ------------------------------------------------------------
		;
		; 偶数行 ： IDCT
		;
		degree_num = 1 shl degree
		half_degree = degree_num / 2
		;
		push	edx
		shl	edx, 1
		@FastIDCT	%(degree - 1), xmm_flag
		;
		; 奇数行 ： R * 2 * A * DCT * K
		;
		push	esi
		push	edi
		shr	edx, 1
		mov	eax, ERI_pMatrixDCTofK[(degree - 1) * 4]
		add	esi, edx
		add	edi, half_degree * (SIZEOF REAL4)
		;
		shl	edx, 1
		xor	ecx, ecx
		IF	xmm_flag
			IF	half_degree NE 4
			.REPEAT
			ENDIF
				movss		xmm0, [esi]
				movss		xmm1, [esi + edx]
				lea		esi, [esi + edx * 2]
				movaps		xmm4, [eax]
				unpcklps	xmm0, xmm1
				movss		xmm2, [esi]
				movss		xmm3, [esi + edx]
				lea		esi, [esi + edx * 2]
				unpcklps	xmm2, xmm3
				add		eax, 10H
				movlhps		xmm0, xmm2
				mulps		xmm0, xmm4
				movups		[ebx + ecx * 4], xmm0
			IF	half_degree NE 4
				add		ecx, 4
			.UNTIL	ecx >= half_degree
			ENDIF
		ELSE
			.REPEAT
				fld	REAL4 PTR [esi]
				fld	REAL4 PTR [esi + edx]
				lea	esi, [esi + edx * 2]
				fxch	st(1)
				fmul	REAL4 PTR [eax]
				fxch	st(1)
				fmul	REAL4 PTR [eax + 4]
				fxch	st(1)
				add	eax, (SIZEOF REAL4) * 2
				fstp	REAL4 PTR [ebx + ecx * 4]
				fstp	REAL4 PTR [ebx + ecx * 4 + 4]
				add	ecx, 2
			.UNTIL	ecx >= half_degree
		ENDIF
		;
		mov	esi, ebx
		add	ebx, half_degree * (SIZEOF REAL4)
		mov	edx, (SIZEOF REAL4)
		@FastDCT	%(degree - 1), xmm_flag
		sub	ebx, half_degree * (SIZEOF REAL4)
		;
		IF	xmm_flag
			movss	xmm0, [edi]
			addss	xmm0, xmm0
			mov	ecx, 1
			movss	[edi], xmm0
			.REPEAT
				movss	xmm1, [edi + ecx * 4]
				addss	xmm1, xmm1
				subss	xmm1, xmm0
				movss	[edi + ecx * 4], xmm1
				inc	ecx
				movaps	xmm0, xmm1
			.UNTIL	ecx >= half_degree
		ELSE
			fld	REAL4 PTR [edi]
			fadd	st, st(0)
			fst	REAL4 PTR [edi]
			mov	ecx, 1
			.REPEAT
				fld	REAL4 PTR [edi + ecx * 4]
				fadd	st, st(0)
				fsubrp	st(1), st
				fst	REAL4 PTR [edi + ecx * 4]
				inc	ecx
			.UNTIL	ecx >= half_degree
			;
			fstp	st(0)
		ENDIF
		;              | I   I |
		; 交差加減算 = |       |
		;              | J  -J |
		IF	xmm_flag
			sub	edi, half_degree * (SIZEOF REAL4)
			IF	half_degree EQ 4
				movups	xmm0, [edi]
				movups	xmm1, [edi + 10H]
				movaps	xmm2, xmm0
				subps	xmm0, xmm1
				addps	xmm2, xmm1
				shufps	xmm0, xmm0, 00011011B
				movups	[edi], xmm2
				movups	[edi + 10H], xmm0
			ELSE
				xor	eax, eax
				mov	ecx, -4
				IF	half_degree NE 8
				.REPEAT
				ENDIF
					movups	xmm0, [edi + eax * 4]
					movups	xmm1, [edi + eax * 4 + (half_degree * 4)]
					movups	xmm4, [edi + ecx * 4 + (half_degree * 4)]
					movups	xmm5, [edi + ecx * 4 + (degree_num * 4)]
					movaps	xmm2, xmm0
					subps	xmm0, xmm1
					movaps	xmm6, xmm4
					subps	xmm4, xmm5
					shufps	xmm0, xmm0, 00011011B
					addps	xmm2, xmm1
					movups	[edi + ecx * 4 + (degree_num * 4)], xmm0
					shufps	xmm4, xmm4, 00011011B
					movups	[edi + eax * 4], xmm2
					addps	xmm6, xmm5
					movups	[edi + eax * 4 + (half_degree * 4)], xmm4
					add	eax, 4
					movups	[edi + ecx * 4 + (half_degree * 4)], xmm6
					sub	ecx, 4
				IF	half_degree NE 8
				.UNTIL	eax >= (half_degree / 2)
				ENDIF
			ENDIF
		ELSE
			sub	edi, half_degree * (SIZEOF REAL4)
			xor	eax, eax
			mov	ecx, -1
			.REPEAT
				fld	REAL4 PTR [edi + eax * 4 + (half_degree * 4)]
				fld	REAL4 PTR [edi + eax * 4]
				fld	st(1)
				fadd	st, st(1)
				fxch	st(2)
				fsubp	st(1), st
				;
				fld	REAL4 PTR [edi + ecx * 4 + (degree_num * 4)]
				fld	REAL4 PTR [edi + ecx * 4 + (half_degree * 4)]
				fld	st(1)
				fadd	st, st(1)
				fxch	st(2)
				fsubp	st(1), st
				;
				fstp	REAL4 PTR [edi + eax * 4 + (half_degree * 4)]
				fstp	REAL4 PTR [edi + ecx * 4 + (half_degree * 4)]
				fstp	REAL4 PTR [edi + ecx * 4 + (degree_num * 4)]
				fstp	REAL4 PTR [edi + eax * 4]
				;
				inc	eax
				dec	ecx
			.UNTIL	eax >= (half_degree / 2)
		ENDIF
		;
		pop	edi
		pop	esi
		pop	edx

	ENDIF

		ENDM


