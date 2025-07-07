
; ----------------------------------------------------------------------------
;                          Entis Standard Library
; ----------------------------------------------------------------------------
;  Copyright (C) 2002-2003 Leshade Entis, Entis-soft. All rights reserved.
; ----------------------------------------------------------------------------


IFNDEF	__ESL_INC__
__ESL_INC__	EQU	1

;
;	デバッグサポート関数
; ----------------------------------------------------------------------------

IFDEF	_DEBUG

ESLTrace	PROTO	NEAR32 C,
	pszTrace:PTR SBYTE, :VARARG
_ESLAssert	PROTO	NEAR32 C,
	pszExpr:PTR SBYTE, pszFile:PTR SBYTE, nLine:SDWORD

TRACE	MACRO	out_str:REQ, param:VARARG
	LOCAL	Label1, szOutputFormat
	pushad
	mov	eax, OFFSET szOutputFormat
	IFB	<param>
		INVOKE	ESLTrace , eax
	ELSE
		INVOKE	ESLTrace , eax, param
	ENDIF
	jmp	Label1
szOutputFormat	BYTE	out_str, 0
	ALIGN	4
Label1:
	popad
ENDM

ASSERT	MACRO	expr:REQ, msg_str:REQ
	LOCAL	Label1, szExplanation, szFileName
	.IF	!(expr)
		pushad
		push	@Line
		push	OFFSET szFileName
		push	OFFSET szExplanation
		call	_ESLAssert
		add	esp, (SIZEOF DWORD) * 3
		popad
		jmp	Label1
szFileName	BYTE	@CatStr(<!">,%@FileCur,<!">), 0
szExplanation	BYTE	msg_str, 0
	ALIGN	4
Label1:
	.ENDIF
ENDM

ELSE

TRACE	MACRO	out_str:REQ, param:VARARG
ENDM

ASSERT	MACRO	expr:REQ, msg_str:REQ
ENDM

ENDIF


;
;	エラーコード
; ----------------------------------------------------------------------------

eslErrSuccess		EQU	0
eslErrNotSupported	EQU	-1
eslErrGeneral		EQU	1
eslErrAbort		EQU	2
eslErrInvalidParam	EQU	3
eslErrTimeout		EQU	4
eslErrPending		EQU	5
eslErrContinue		EQU	6


;
;	ヒープ関数
; ----------------------------------------------------------------------------

HSTACKHEAP	TYPEDEF	PTR
HESLHEAP	TYPEDEF	PTR

;
; メモリ操作関数
;

eslFillMemory	PROTO	NEAR32 C,
	ptrMem:PTR, bytData:BYTE, dwLength:DWORD
eslMoveMemory	PROTO	NEAR32 C,
	ptrDst:PTR, ptrSrc:PTR, dwLength:DWORD


;
; スタック式ヒープ
;

eslStackHeapCreate	PROTO	NEAR32 C,
	dwInitSize:DWORD, dwGrowSize:DWORD, dwFlags:DWORD
eslStackHeapDestroy	PROTO	NEAR32 C,
	hHeap:HSTACKHEAP
eslStackHeapAllocate	PROTO	NEAR32 C,
	hHeap:HSTACKHEAP, dwSize:DWORD
eslStackHeapFree	PROTO	NEAR32 C,
	hHeap:HSTACKHEAP


;
; 汎用ヒープ
;

eslGetGlobalHeap	PROTO	NEAR32 C
eslHeapCreate		PROTO	NEAR32 C,
	dwInitSize:DWORD, dwGrowSize:DWORD,
	dwFlags:DWORD, hParentHeap:HESLHEAP
eslHeapDestroy		PROTO	NEAR32 C,
	hHeap:HESLHEAP
eslHeapAllocate		PROTO	NEAR32 C,
	hHeap:HESLHEAP, dwSize:DWORD, dwFlags:DWORD
eslHeapFree		PROTO	NEAR32 C,
	hHeap:HESLHEAP, ptrObj:PTR, dwFlags:DWORD
eslHeapReallocate	PROTO	NEAR32 C,
	hHeap:HESLHEAP, ptrObj:PTR, dwSize:DWORD, dwFlags:DWORD
eslHeapGetLength	PROTO	NEAR32 C,
	hHeap:HESLHEAP, ptrObj:PTR
eslVerifyHeapChain	PROTO	NEAR32 C, hHeap:HESLHEAP
eslHeapLock		PROTO	NEAR32 C,
	hHeap:HESLHEAP
eslHeapUnlock		PROTO	NEAR32 C,
	hHeap:HESLHEAP
eslHeapDump		PROTO	NEAR32 C,
	hHeap:HESLHEAP, nLimit:SDWORD

ESL_HEAP_ZERO_INIT	EQU	(00000001H)
ESL_HEAP_NO_SERIALIZE	EQU	(00000002H)
ESL_INVALID_HEAP	EQU	(0FFFFFFFFH)


ENDIF
