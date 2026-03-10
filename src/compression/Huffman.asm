.code

HUFFMAN_MAX_DEPTH equ 11

EXTRACT_STEP macro idx, bitBuf, extractControl, extractControlLower
	bextr rsi, bitBuf, extractControl
	movzx esi, word ptr [rax + rsi * 2]
	add extractControlLower, sil
	shr esi, 8
	mov [rdi + idx], sil
endm

REFILL_STEP macro bitBuf, extractControl, extractControlLower, readPtr
	LOCAL noRefill
	test extractControlLower, 0F8h
	jz noRefill
	mov rax, extractControl
	and eax, 0F8h
	shrx bitBuf, bitBuf, rax
	mov esi, 64
	sub esi, eax
	shlx rsi, [readPtr], rsi
	or bitBuf, rsi
	and extractControl, 0FF07h
	shr eax, 3
	add readPtr, rax
noRefill:
endm

huff_4_stream_decode PROC
	; save callee preserved GPRs
	push rbx
	push rbp
	push rdi
	push rsi
	push r12
	push r13
	push r14
	push r15
	
	mov rax, [rcx] ; decodeTable
	mov rdi, [rcx + 8] ; writePtr
	mov rsi, [rcx + 16] ; outputLen
	sub rsp, 8 ; space for residual
	push rsi
	mov rbx, [rcx + 24] ; readPtr0
	mov rdx, [rcx + 32] ; readPtr1
	mov rbp, [rcx + 40] ; readPtr2
	mov rcx, [rcx + 48] ; readPtr3
	mov r8d, HUFFMAN_MAX_DEPTH SHL 8 ; bextrControl0
	mov r9, r8 ; bextrControl1
	mov r10, r8 ; bextrControl2
	mov r11, r8 ; bextrControl3
	mov r12, [rbx] ; bitBuf0
	mov r13, [rdx] ; bitBuf1
	mov r14, [rbp] ; bitBuf2
	mov r15, [rcx] ; bitBuf3
	add rbx, 8
	add rdx, 8
	add rbp, 8
	add rcx, 8

	push rax
	mov eax, 3435973837
	imul rsi, rax
	shr rsi, 36 ; "Magic Constant" divide by 20 (multiply by fixed point reciprocal)
	imul esi, esi, 20 ; Multiply by 20 again so we get an even multiple
	mov eax, [rsp + 8]
	sub eax, esi ; Get residual for after the main loop
	mov [rsp + 16], eax
	add rsi, rdi
	pop rax
	push rsi ; loop end pointer

	cmp dword ptr [rsp + 8], 20 ; Length < 20
	jb last20
decodeLoop:
	EXTRACT_STEP 0, r12, r8, r8b
	EXTRACT_STEP 1, r13, r9, r9b
	EXTRACT_STEP 2, r14, r10, r10b
	EXTRACT_STEP 3, r15, r11, r11b
	EXTRACT_STEP 4, r12, r8, r8b
	EXTRACT_STEP 5, r13, r9, r9b
	EXTRACT_STEP 6, r14, r10, r10b
	EXTRACT_STEP 7, r15, r11, r11b
	EXTRACT_STEP 8, r12, r8, r8b
	EXTRACT_STEP 9, r13, r9, r9b
	EXTRACT_STEP 10, r14, r10, r10b
	EXTRACT_STEP 11, r15, r11, r11b
	EXTRACT_STEP 12, r12, r8, r8b
	EXTRACT_STEP 13, r13, r9, r9b
	EXTRACT_STEP 14, r14, r10, r10b
	EXTRACT_STEP 15, r15, r11, r11b
	EXTRACT_STEP 16, r12, r8, r8b
	EXTRACT_STEP 17, r13, r9, r9b
	EXTRACT_STEP 18, r14, r10, r10b
	EXTRACT_STEP 19, r15, r11, r11b

	push rax
	REFILL_STEP r12, r8, r8b, rbx
	REFILL_STEP r13, r9, r9b, rdx
	REFILL_STEP r14, r10, r10b, rbp
	REFILL_STEP r15, r11, r11b, rcx
	pop rax

	add rdi, 20
	cmp [rsp], rdi
	jne decodeLoop
last20:

	mov ebx, [rsp + 16] ; Get residual
	cmp ebx, 0
	je finished
	EXTRACT_STEP 0, r12, r8, r8b
	cmp ebx, 1
	je finished
	EXTRACT_STEP 1, r13, r9, r9b
	cmp ebx, 2
	je finished
	EXTRACT_STEP 2, r14, r10, r10b
	cmp ebx, 3
	je finished
	EXTRACT_STEP 3, r15, r11, r11b
	cmp ebx, 4
	je finished
	EXTRACT_STEP 4, r12, r8, r8b
	cmp ebx, 5
	je finished
	EXTRACT_STEP 5, r13, r9, r9b
	cmp ebx, 6
	je finished
	EXTRACT_STEP 6, r14, r10, r10b
	cmp ebx, 7
	je finished
	EXTRACT_STEP 7, r15, r11, r11b
	cmp ebx, 8
	je finished
	EXTRACT_STEP 8, r12, r8, r8b
	cmp ebx, 9
	je finished
	EXTRACT_STEP 9, r13, r9, r9b
	cmp ebx, 10
	je finished
	EXTRACT_STEP 10, r14, r10, r10b
	cmp ebx, 11
	je finished
	EXTRACT_STEP 11, r15, r11, r11b
	cmp ebx, 12
	je finished
	EXTRACT_STEP 12, r12, r8, r8b
	cmp ebx, 13
	je finished
	EXTRACT_STEP 13, r13, r9, r9b
	cmp ebx, 14
	je finished
	EXTRACT_STEP 14, r14, r10, r10b
	cmp ebx, 15
	je finished
	EXTRACT_STEP 15, r15, r11, r11b
	cmp ebx, 16
	je finished
	EXTRACT_STEP 16, r12, r8, r8b
	cmp ebx, 17
	je finished
	EXTRACT_STEP 17, r13, r9, r9b
	cmp ebx, 18
	je finished
	EXTRACT_STEP 18, r14, r10, r10b
finished:

	lea rax, [rdi + rbx] ; return new writePtr

	pop rsi
	pop rsi
	add rsp, 8

	; restore callee preserved GPRs
	pop r15
	pop r14
	pop r13
	pop r12
	pop rsi
	pop rdi
	pop rbp
	pop rbx
	ret
huff_4_stream_decode ENDP
END