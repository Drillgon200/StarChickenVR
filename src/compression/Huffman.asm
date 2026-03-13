.code

HUFFMAN_MAX_DEPTH equ 11

EXTRACT_STEP_SCALAR macro idx, bitBuf, extractControl, extractControlLower
	bextr rsi, bitBuf, extractControl
	movzx esi, word ptr [rax + rsi * 2]
	add extractControlLower, sil
	; Theoretically a high byte register (ah, ch, dh, bh) could be used to avoid the shift, but there's a bug on some CPUs that prevents this
	; https://fgiesen.wordpress.com/2025/05/21/oodle-2-9-14-and-intel-13th-14th-gen-cpus/
	shr esi, 8
	mov [rdi + idx], sil
endm

REFILL_STEP_SCALAR macro bitBuf, extractControl, extractControlLower, readPtr
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

; MSVC register allocation/instruction selection didn't do a great job for refills in the C++ version, so I decided to just do it in assembly
; This is around 20% faster than the C++ version
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
	
	;struct Huff4StreamDecodeCallArgs {
	;	U16* decodeTable;
	;	Byte* writePtr;
	;	U64 outputLen;
	;	Byte* readPtr0;
	;	Byte* readPtr1;
	;	Byte* readPtr2;
	;	Byte* readPtr3;
	;};
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
	EXTRACT_STEP_SCALAR 0, r12, r8, r8b
	EXTRACT_STEP_SCALAR 1, r13, r9, r9b
	EXTRACT_STEP_SCALAR 2, r14, r10, r10b
	EXTRACT_STEP_SCALAR 3, r15, r11, r11b
	EXTRACT_STEP_SCALAR 4, r12, r8, r8b
	EXTRACT_STEP_SCALAR 5, r13, r9, r9b
	EXTRACT_STEP_SCALAR 6, r14, r10, r10b
	EXTRACT_STEP_SCALAR 7, r15, r11, r11b
	EXTRACT_STEP_SCALAR 8, r12, r8, r8b
	EXTRACT_STEP_SCALAR 9, r13, r9, r9b
	EXTRACT_STEP_SCALAR 10, r14, r10, r10b
	EXTRACT_STEP_SCALAR 11, r15, r11, r11b
	EXTRACT_STEP_SCALAR 12, r12, r8, r8b
	EXTRACT_STEP_SCALAR 13, r13, r9, r9b
	EXTRACT_STEP_SCALAR 14, r14, r10, r10b
	EXTRACT_STEP_SCALAR 15, r15, r11, r11b
	EXTRACT_STEP_SCALAR 16, r12, r8, r8b
	EXTRACT_STEP_SCALAR 17, r13, r9, r9b
	EXTRACT_STEP_SCALAR 18, r14, r10, r10b
	EXTRACT_STEP_SCALAR 19, r15, r11, r11b

	push rax
	REFILL_STEP_SCALAR r12, r8, r8b, rbx
	REFILL_STEP_SCALAR r13, r9, r9b, rdx
	REFILL_STEP_SCALAR r14, r10, r10b, rbp
	REFILL_STEP_SCALAR r15, r11, r11b, rcx
	pop rax

	add rdi, 20
	cmp [rsp], rdi
	jne decodeLoop
last20:

	mov ebx, [rsp + 16] ; Get residual
	cmp ebx, 0
	je finished
	EXTRACT_STEP_SCALAR 0, r12, r8, r8b
	cmp ebx, 1
	je finished
	EXTRACT_STEP_SCALAR 1, r13, r9, r9b
	cmp ebx, 2
	je finished
	EXTRACT_STEP_SCALAR 2, r14, r10, r10b
	cmp ebx, 3
	je finished
	EXTRACT_STEP_SCALAR 3, r15, r11, r11b
	cmp ebx, 4
	je finished
	EXTRACT_STEP_SCALAR 4, r12, r8, r8b
	cmp ebx, 5
	je finished
	EXTRACT_STEP_SCALAR 5, r13, r9, r9b
	cmp ebx, 6
	je finished
	EXTRACT_STEP_SCALAR 6, r14, r10, r10b
	cmp ebx, 7
	je finished
	EXTRACT_STEP_SCALAR 7, r15, r11, r11b
	cmp ebx, 8
	je finished
	EXTRACT_STEP_SCALAR 8, r12, r8, r8b
	cmp ebx, 9
	je finished
	EXTRACT_STEP_SCALAR 9, r13, r9, r9b
	cmp ebx, 10
	je finished
	EXTRACT_STEP_SCALAR 10, r14, r10, r10b
	cmp ebx, 11
	je finished
	EXTRACT_STEP_SCALAR 11, r15, r11, r11b
	cmp ebx, 12
	je finished
	EXTRACT_STEP_SCALAR 12, r12, r8, r8b
	cmp ebx, 13
	je finished
	EXTRACT_STEP_SCALAR 13, r13, r9, r9b
	cmp ebx, 14
	je finished
	EXTRACT_STEP_SCALAR 14, r14, r10, r10b
	cmp ebx, 15
	je finished
	EXTRACT_STEP_SCALAR 15, r15, r11, r11b
	cmp ebx, 16
	je finished
	EXTRACT_STEP_SCALAR 16, r12, r8, r8b
	cmp ebx, 17
	je finished
	EXTRACT_STEP_SCALAR 17, r13, r9, r9b
	cmp ebx, 18
	je finished
	EXTRACT_STEP_SCALAR 18, r14, r10, r10b
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


; ymm15 has huff mask
; ymm13 has all ones mask
; xmm14 has shuffle
; xmm1 has low byte dword mask
; ymm12/ymm0 scratch registers
EXTRACT_STEP_VECTOR macro idx, bitBuf, bitCount
	vpcmpeqb ymm13, ymm13, ymm13 ; All ones mask
	vpand ymm0, ymm15, bitBuf
	vpgatherqd xmm12, [rax + ymm0 * 2], xmm13
	vpand xmm0, xmm12, xmm1
	vpmovsxdq ymm0, xmm0
	vpsubq bitCount, bitCount, ymm0
	vpsrlvq bitBuf, bitBuf, ymm0
	vpshufb xmm12, xmm12, xmm14
	vmovd dword ptr [rdi + idx], xmm12
endm

; ymm12 has 64
; ymm13 has all ones mask
; ymm15 scratch register
REFILL_STEP_VECTOR macro readOffset, bitBuf, bitCount
	vpcmpeqb ymm13, ymm13, ymm13 ; All ones mask
	vpgatherqq ymm15, [rdx + readOffset], ymm13
	vpsllvq ymm15, ymm15, bitCount
	vpor bitBuf, bitBuf, ymm15
	vpsubq ymm15, ymm12, bitCount
	vpsrlq ymm15, ymm15, 3
	vpaddq readOffset, readOffset, ymm15
	vpsllq ymm15, ymm15, 3
	vpaddq bitCount, bitCount, ymm15
endm

ALIGN 16
shuffleLowDwordBytesContiguous BYTE 1, 5, 9, 13, 80h, 80h, 80h, 80h, 80h, 80h, 80h, 80h, 80h, 80h, 80h, 80h
sixtyFour QWORD 64
lowByteMask DWORD 000000FFh

huff_16_stream_decode PROC
	; save callee preserved registers
	push rbx
	push rbp
	push rdi
	push rsi
	push r12
	push r13
	push r14
	push r15
	sub rsp, 8
	mov rbp, rsp
	and rsp, NOT 31 ; align 32
	sub rsp, 64 + 10 * 16 ; space for two registers spilled in the loop as well as the callee preserved xmm registers
	vmovaps [rsp + 64 + 0 * 16], xmm6
	vmovaps [rsp + 64 + 1 * 16], xmm7
	vmovaps [rsp + 64 + 2 * 16], xmm8
	vmovaps [rsp + 64 + 3 * 16], xmm9
	vmovaps [rsp + 64 + 4 * 16], xmm10
	vmovaps [rsp + 64 + 5 * 16], xmm11
	vmovaps [rsp + 64 + 6 * 16], xmm12
	vmovaps [rsp + 64 + 7 * 16], xmm13
	vmovaps [rsp + 64 + 8 * 16], xmm14
	vmovaps [rsp + 64 + 9 * 16], xmm15

	;struct Huff16StreamDecodeCallArgs {
	;	U16* decodeTable;
	;	Byte* writePtr;
	;	U64 outputLen;
	;	Byte* readPtr;
	;	U64 readOffset[16];
	;};
	mov rax, [rcx] ; decodeTable
	mov rdi, [rcx + 8] ; writePtr
	mov rsi, [rcx + 16] ; outputLen
	mov rdx, [rcx + 24] ; readPtr
	vmovdqu ymm0, ymmword ptr [rcx + 32] ; readOffset0to3
	vmovdqu ymm1, ymmword ptr [rcx + 64] ; readOffset4to7
	vmovdqu ymm2, ymmword ptr [rcx + 96] ; readOffset8to11
	vmovdqu ymm3, ymmword ptr [rcx + 128] ; readOffset12to15

	vpcmpeqb ymm13, ymm13, ymm13 ; All ones mask
	vpgatherqq ymm4, [rdx + ymm0], ymm13 ; bitBuf0to3
	vpcmpeqb ymm13, ymm13, ymm13 ; All ones mask
	vpgatherqq ymm5, [rdx + ymm1], ymm13 ; bitBur4to7
	vpcmpeqb ymm13, ymm13, ymm13 ; All ones mask
	vpgatherqq ymm6, [rdx + ymm2], ymm13 ; bitBuf8to11
	vpcmpeqb ymm13, ymm13, ymm13 ; All ones mask
	vpgatherqq ymm7, [rdx + ymm3], ymm13 ; bitBuf12to15
	vpcmpeqb ymm13, ymm13, ymm13 ; All ones mask
	add rdx, 8
	vpbroadcastq ymm12, qword ptr [sixtyFour]
	vmovdqa ymm8, ymm12 ; bitCount0to3
	vmovdqa ymm9, ymm12 ; bitCount4to7
	vmovdqa ymm10, ymm12 ; bitCount8to11
	vmovdqa ymm11, ymm12 ; bitCount12to15
	vmovdqa xmm14, xmmword ptr [shuffleLowDwordBytesContiguous]
	vpsrlq ymm15, ymm13, 64 - HUFFMAN_MAX_DEPTH

	push rax
	mov eax, 3435973837
	imul rbx, rax
	shr rbx, 38 ; "Magic Constant" divide by 80 (multiply by fixed point reciprocal)
	imul ebx, ebx, 80 ; Multiply by 80 again so we get an even multiple
	sub esi, ebx ; Get residual for after the main loop
	mov [rsp + 16], eax
	add rbx, rdi ; loop end pointer
	pop rax

	cmp rdi, rbx ; writePtr == loopEndPtr
	je last80
decodeLoop:
	vmovdqa ymmword ptr [rsp], ymm0
	vmovdqa ymmword ptr [rsp + 32], ymm1
	vpsrld xmm1, xmm13, 24 ; dword low byte mask

	EXTRACT_STEP_VECTOR 0, ymm4, ymm8
	EXTRACT_STEP_VECTOR 4, ymm5, ymm9
	EXTRACT_STEP_VECTOR 8, ymm6, ymm10
	EXTRACT_STEP_VECTOR 12, ymm7, ymm11
	EXTRACT_STEP_VECTOR 16, ymm4, ymm8
	EXTRACT_STEP_VECTOR 20, ymm5, ymm9
	EXTRACT_STEP_VECTOR 24, ymm6, ymm10
	EXTRACT_STEP_VECTOR 28, ymm7, ymm11
	EXTRACT_STEP_VECTOR 32, ymm4, ymm8
	EXTRACT_STEP_VECTOR 36, ymm5, ymm9
	EXTRACT_STEP_VECTOR 40, ymm6, ymm10
	EXTRACT_STEP_VECTOR 44, ymm7, ymm11
	EXTRACT_STEP_VECTOR 48, ymm4, ymm8
	EXTRACT_STEP_VECTOR 52, ymm5, ymm9
	EXTRACT_STEP_VECTOR 56, ymm6, ymm10
	EXTRACT_STEP_VECTOR 60, ymm7, ymm11
	EXTRACT_STEP_VECTOR 64, ymm4, ymm8
	EXTRACT_STEP_VECTOR 68, ymm5, ymm9
	EXTRACT_STEP_VECTOR 72, ymm6, ymm10
	EXTRACT_STEP_VECTOR 76, ymm7, ymm11

	vpbroadcastq ymm12, qword ptr [sixtyFour]
	vmovdqa ymm0, ymmword ptr [rsp]
	vmovdqa ymm1, ymmword ptr [rsp + 32]

	REFILL_STEP_VECTOR ymm0, ymm4, ymm8
	REFILL_STEP_VECTOR ymm1, ymm5, ymm9
	REFILL_STEP_VECTOR ymm2, ymm6, ymm10
	REFILL_STEP_VECTOR ymm3, ymm7, ymm11

	vpcmpeqb ymm13, ymm13, ymm13 ; All ones mask
	vpsrlq ymm15, ymm13, 64 - HUFFMAN_MAX_DEPTH

	add rdi, 80
	cmp rdi, rbx
	jne decodeLoop
last80:

	vpsrld xmm1, xmm13, 24 ; dword low byte mask
	cmp esi, 0
	je finished
	EXTRACT_STEP_VECTOR 0, ymm4, ymm8
	cmp esi, 4
	jb finished
	EXTRACT_STEP_VECTOR 4, ymm5, ymm9
	cmp esi, 8
	jb finished
	EXTRACT_STEP_VECTOR 8, ymm6, ymm10
	cmp esi, 12
	jb finished
	EXTRACT_STEP_VECTOR 12, ymm7, ymm11
	cmp esi, 16
	jb finished
	EXTRACT_STEP_VECTOR 16, ymm4, ymm8
	cmp esi, 20
	jb finished
	EXTRACT_STEP_VECTOR 20, ymm5, ymm9
	cmp esi, 24
	jb finished
	EXTRACT_STEP_VECTOR 24, ymm6, ymm10
	cmp esi, 28
	jb finished
	EXTRACT_STEP_VECTOR 28, ymm7, ymm11
	cmp esi, 32
	jb finished
	EXTRACT_STEP_VECTOR 32, ymm4, ymm8
	cmp esi, 36
	jb finished
	EXTRACT_STEP_VECTOR 36, ymm5, ymm9
	cmp esi, 40
	jb finished
	EXTRACT_STEP_VECTOR 40, ymm6, ymm10
	cmp esi, 44
	jb finished
	EXTRACT_STEP_VECTOR 44, ymm7, ymm11
	cmp esi, 48
	jb finished
	EXTRACT_STEP_VECTOR 48, ymm4, ymm8
	cmp esi, 52
	jb finished
	EXTRACT_STEP_VECTOR 52, ymm5, ymm9
	cmp esi, 56
	jb finished
	EXTRACT_STEP_VECTOR 56, ymm6, ymm10
	cmp esi, 60
	jb finished
	EXTRACT_STEP_VECTOR 60, ymm7, ymm11
	cmp esi, 64
	jb finished
	EXTRACT_STEP_VECTOR 64, ymm4, ymm8
	cmp esi, 68
	jb finished
	EXTRACT_STEP_VECTOR 68, ymm5, ymm9
	cmp esi, 72
	jb finished
	EXTRACT_STEP_VECTOR 72, ymm6, ymm10
	cmp esi, 76
	jb finished
	EXTRACT_STEP_VECTOR 76, ymm7, ymm11
finished:

	lea rax, [rdi + rsi] ; return loop writePtr + residual

	; restore callee preserved registers
	vmovaps xmm6, [rsp + 64 + 0 * 16]
	vmovaps xmm7, [rsp + 64 + 1 * 16]
	vmovaps xmm8, [rsp + 64 + 2 * 16]
	vmovaps xmm9, [rsp + 64 + 3 * 16]
	vmovaps xmm10, [rsp + 64 + 4 * 16]
	vmovaps xmm11, [rsp + 64 + 5 * 16]
	vmovaps xmm12, [rsp + 64 + 6 * 16]
	vmovaps xmm13, [rsp + 64 + 7 * 16]
	vmovaps xmm14, [rsp + 64 + 8 * 16]
	vmovaps xmm15, [rsp + 64 + 9 * 16]
	mov rsp, rbp
	add rsp, 8
	pop r15
	pop r14
	pop r13
	pop r12
	pop rsi
	pop rdi
	pop rbp
	pop rbx
	vzeroupper
	ret
huff_16_stream_decode ENDP
END