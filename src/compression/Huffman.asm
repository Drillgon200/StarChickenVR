huffSegment SEGMENT READONLY PAGE READ EXECUTE ALIAS('.hasm') 'CODE'


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
	movzx esi, extractControlLower
	and extractControl, 0FF07h
	shr esi, 3
	mov bitBuf, [readPtr + rsi]
	add readPtr, rsi
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
	
	;struct HuffScalarMultiStreamDecodeCallArgs {
	;	U16* decodeTable;
	;	Byte* writePtr;
	;	U64 outputLen;
	;	Byte* readPtr[4];
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

	REFILL_STEP_SCALAR r12, r8, r8b, rbx
	REFILL_STEP_SCALAR r13, r9, r9b, rdx
	REFILL_STEP_SCALAR r14, r10, r10b, rbp
	REFILL_STEP_SCALAR r15, r11, r11b, rcx

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

; The increase in the number of streams means we're going to run into register pressure here.
; Zen 2, 4, and 5 implement memory renaming, which should save from the latency cost of shuffling things in and out of stack slots here
huff_9_stream_decode PROC
	; save callee preserved GPRs
	push rbx
	push rbp
	push rdi
	push rsi
	push r12
	push r13
	push r14
	push r15
	sub rsp, 8 + 8 + 8 * 3 * 9 + 8 ; one for end ptr, one for length, 3 per stream, one for extra spill slot

	; 16 GPRs
	; 1 for stack ptr (rsp), 1 for write ptr (rdi), 1 for decode table (rax), 1 for scratch (rsi)
	; That leaves us 12 GPRs for data. We can spill the stream ptrs over the decode part, since stream ptrs are only needed for refills
	; We can fit 9 streams in GPRs then, with 3 for extra constants

	;struct HuffScalarMultiStreamDecodeCallArgs {
	;	U16* decodeTable;
	;	Byte* writePtr;
	;	U64 outputLen;
	;	Byte* readPtr[9];
	;};
	mov rax, [rcx] ; decodeTable
	mov rdi, [rcx + 8] ; writePtr
	mov rsi, [rcx + 16] ; length
	mov [rsp + 8], rsi

	mov r15, 8000000000000000h

	; Load readPtrs into stack slots [rsp + 16] to [rsp + 80]
	; Load bitBuf0-5 into rdx, rbx, rbp, rcx, r8, r9
	; Load bitBuf6-8 into stack slots [rsp + 136] to [rsp + 152]
	mov r11, [rcx + 24] ; readPtr0
	mov rdx, [r11] ; bitBuf0
	shl rdx, 1
	or rdx, r15
	mov [rsp + 16], r11
	mov r11, [rcx + 32] ; readPtr1
	mov rbx, [r11] ; bitBuf1
	shl rbx, 1
	or rbx, r15
	mov [rsp + 24], r11
	mov r11, [rcx + 40] ; readPtr2
	mov rbp, [r11] ; bitBuf2
	shl rbp, 1
	or rbp, r15
	mov [rsp + 32], r11
	mov r11, [rcx + 48] ; readPtr3
	mov r13, [r11] ; bitBuf3 (will be moved to rcx)
	shl r13, 1
	or r13, r15
	mov [rsp + 40], r11
	mov r11, [rcx + 56] ; readPtr4
	mov r8, [r11] ; bitBuf4
	shl r8, 1
	or r8, r15
	mov [rsp + 48], r11
	mov r11, [rcx + 64] ; readPtr5
	mov r9, [r11] ; bitBuf5
	shl r9, 1
	or r9, r15
	mov [rsp + 56], r11
	mov r11, [rcx + 72] ; readPtr6
	mov r10, [r11] ; bitBuf6
	shl r10, 1
	or r10, r15
	mov [rsp + 64], r11
	mov r14, [rcx + 80] ; readPtr7
	mov r11, [r14] ; bitBuf7
	shl r11, 1
	or r11, r15
	mov [rsp + 72], r14
	mov r14, [rcx + 88] ; readPtr8
	mov r12, [r14] ; bitBuf8
	shl r12, 1
	or r12, r15
	mov [rsp + 80], r14
	mov rcx, rsi ; length
	mov rsi, r13

	imul r13, rcx, 1813430637
	shr r13, 32
	sub rcx, r13
	shr ecx, 1
	add r13, rcx
	shr r13, 5 ; "Magic Constant" divide by 45 (multiply by fixed point reciprocal)

	imul rcx, r13, 45 ; Multiply by 45 again so we get an even multiple
	sub [rsp + 8], rcx ; Get residual for after the main loop
	add rcx, rdi
	mov [rsp], rcx ; loop end pointer

	mov r14, NOT (((1 SHL HUFFMAN_MAX_DEPTH) - 1) SHL 1)

	cmp rdi, [rsp] ; Length < 45
	je last45
decodeLoop:
writeOffset = 0
EXTRACT_STEP_SCALAR_SINGLE_REG macro idx, bitBuf
	;mov rcx, bitBuf
	; The bottom bit of the register isn't used. This is so that the table load can be done with simple addressing (no index register shift), which cuts a cycle of the load latency on Zen 5
	; This means an extra cycle of latency has to be added to the refill, but it still ends up being a small perf win in practice
	;and ecx, ((1 SHL HUFFMAN_MAX_DEPTH) - 1) SHL 1
	andn rcx, r14, bitBuf
	movzx ecx, word ptr [rax + rcx]
	shrx bitBuf, bitBuf, rcx
	; Theoretically a high byte register (ah, ch, dh, bh) could be used to avoid the shift, but there's a bug on some CPUs that prevents this
	; https://fgiesen.wordpress.com/2025/05/21/oodle-2-9-14-and-intel-13th-14th-gen-cpus/
	; For the 9 stream decoder, the extra ALU3/4/5 pressure this shift causes significantly reduces decompression throughput
	; I might just leave it out and hope intel's microcode updates have fixed it (either that or duplicate this code for 13/14th gen intel, though at that point I might just tune a separate decompressor for intel CPUs)
	;shr ecx, 8
	;mov [rdi + idx], cl
	mov [rdi + idx], ch
endm
REPEAT 4
	EXTRACT_STEP_SCALAR_SINGLE_REG writeOffset + 0, rdx
	EXTRACT_STEP_SCALAR_SINGLE_REG writeOffset + 1, rbx
	EXTRACT_STEP_SCALAR_SINGLE_REG writeOffset + 2, rbp
	EXTRACT_STEP_SCALAR_SINGLE_REG writeOffset + 3, rsi
	EXTRACT_STEP_SCALAR_SINGLE_REG writeOffset + 4, r8
	EXTRACT_STEP_SCALAR_SINGLE_REG writeOffset + 5, r9
	EXTRACT_STEP_SCALAR_SINGLE_REG writeOffset + 6, r10
	EXTRACT_STEP_SCALAR_SINGLE_REG writeOffset + 7, r11
	EXTRACT_STEP_SCALAR_SINGLE_REG writeOffset + 8, r12
writeOffset = writeOffset + 9
ENDM

EXTRACT_STEP_SCALAR_SINGLE_REG_LAST macro idx, bitBuf, bitBufLo
	; Same as the other one, but does the lzcnt early
	andn rcx, r14, bitBuf
	lzcnt bitBuf, bitBuf
	movzx ecx, word ptr [rax + rcx]
	add bitBufLo, cl
	mov [rdi + idx], ch
endm
	EXTRACT_STEP_SCALAR_SINGLE_REG_LAST writeOffset + 0, rdx, dl,
	EXTRACT_STEP_SCALAR_SINGLE_REG_LAST writeOffset + 1, rbx, bl,
	EXTRACT_STEP_SCALAR_SINGLE_REG_LAST writeOffset + 2, rbp, bpl
	EXTRACT_STEP_SCALAR_SINGLE_REG_LAST writeOffset + 3, rsi, sil
	EXTRACT_STEP_SCALAR_SINGLE_REG_LAST writeOffset + 4, r8, r8b
	EXTRACT_STEP_SCALAR_SINGLE_REG_LAST writeOffset + 5, r9, r9b
	EXTRACT_STEP_SCALAR_SINGLE_REG_LAST writeOffset + 6, r10, r10b
	EXTRACT_STEP_SCALAR_SINGLE_REG_LAST writeOffset + 7, r11, r11b
	EXTRACT_STEP_SCALAR_SINGLE_REG_LAST writeOffset + 8, r12, r12b

REFILL_STEP_SCALAR_SINGLE_REG macro bitBuf, readPtrStackSlot
	mov r13, [rsp + readPtrStackSlot]
	mov rcx, bitBuf
	shr bitBuf, 3
	and ecx, 7
	add r13, bitBuf
	mov bitBuf, [r13]
	add bitBuf, bitBuf ; shift left by 1 since we want it shifted for the extract step (add can run on more ports than shl)
	or bitBuf, r15 ; set high bit
	shrx bitBuf, bitBuf, rcx
	mov [rsp + readPtrStackSlot], r13
endm
	; readPtrs in stack slots [rsp + 16] to [rsp + 80]
	; Spilling the refill pointers hurts a little, but not all that much due to memory renaming
	REFILL_STEP_SCALAR_SINGLE_REG rdx, 16 ; readPtr0
	REFILL_STEP_SCALAR_SINGLE_REG rbx, 24 ; readPtr1
	REFILL_STEP_SCALAR_SINGLE_REG rbp, 32 ; readPtr2
	REFILL_STEP_SCALAR_SINGLE_REG rsi, 40 ; readPtr3
	REFILL_STEP_SCALAR_SINGLE_REG r8, 48 ; readPtr4
	REFILL_STEP_SCALAR_SINGLE_REG r9, 56 ; readPtr5
	REFILL_STEP_SCALAR_SINGLE_REG r10, 64 ; readPtr6
	REFILL_STEP_SCALAR_SINGLE_REG r11, 72 ; readPtr7
	REFILL_STEP_SCALAR_SINGLE_REG r12, 80 ; readPtr8

	add rdi, 45
	cmp rdi, [rsp]
	jne decodeLoop
last45:
	mov rcx, [rsp + 8] ; Get residual
	add [rsp], rcx
	cmp rdi, [rsp]
	je finished

REPEAT 5
	EXTRACT_STEP_SCALAR_SINGLE_REG 0, rdx
	inc rdi
	cmp rdi, [rsp]
	je finished
	EXTRACT_STEP_SCALAR_SINGLE_REG 0, rbx
	inc rdi
	cmp rdi, [rsp]
	je finished
	EXTRACT_STEP_SCALAR_SINGLE_REG 0, rbp
	inc rdi
	cmp rdi, [rsp]
	je finished
	EXTRACT_STEP_SCALAR_SINGLE_REG 0, rsi
	inc rdi
	cmp rdi, [rsp]
	je finished
	EXTRACT_STEP_SCALAR_SINGLE_REG 0, r8
	inc rdi
	cmp rdi, [rsp]
	je finished
	EXTRACT_STEP_SCALAR_SINGLE_REG 0, r9
	inc rdi
	cmp rdi, [rsp]
	je finished
	EXTRACT_STEP_SCALAR_SINGLE_REG 0, r10
	inc rdi
	cmp rdi, [rsp]
	je finished
	EXTRACT_STEP_SCALAR_SINGLE_REG 0, r11
	inc rdi
	cmp rdi, [rsp]
	je finished
	EXTRACT_STEP_SCALAR_SINGLE_REG 0, r12
	inc rdi
	cmp rdi, [rsp]
	je finished
ENDM

finished:

	mov rax, rdi ; return new writePtr

	add rsp, 8 + 8 + 8 * 3 * 9 + 8 ; one for end ptr, one for length, 3 per stream, one for extra spill slot
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
huff_9_stream_decode ENDP





; ymm15 has huff mask
; ymm13 has all ones mask
; xmm14 has shuffle
; xmm1 has low byte dword mask
; rdi has dst ptr
; ymm12/ymm0 scratch registers
EXTRACT_STEP_VECTOR macro tmp0, tmp1, idx, bitBuf, bitCount
	;vpcmpeqb ymm13, ymm13, ymm13 ; All ones mask
	vpand ymm0, ymm15, bitBuf
	;vpgatherqd xmm12, [rax + ymm0 * 2], xmm13

	vpxor xmm12, xmm12, xmm12
	vextracti128 xmm13, ymm0, 1
	vpextrq tmp1, xmm0, 1
	vmovq tmp0, xmm0
	vpinsrw xmm12, xmm12, word ptr [rax + tmp0 * 2], 0
	vpinsrw xmm12, xmm12, word ptr [rax + tmp1 * 2], 2
	vpextrq tmp1, xmm13, 1
	vmovq tmp0, xmm13
	vpinsrw xmm12, xmm12, word ptr [rax + tmp0 * 2], 4
	vpinsrw xmm12, xmm12, word ptr [rax + tmp1 * 2], 6

	vpand xmm0, xmm12, xmm1
	vpmovsxdq ymm0, xmm0
	vpsubq bitCount, bitCount, ymm0
	vpsrlvq bitBuf, bitBuf, ymm0
	vpshufb xmm12, xmm12, xmm14
	vmovd dword ptr [rdi + idx], xmm12
endm

; ymm12 has 64
; ymm15/ymm13 scratch register
REFILL_STEP_VECTOR macro data, tmp0, tmp1, readOffset, readOffsetXMM, bitBuf, bitCount
	;vpcmpeqb ymm13, ymm13, ymm13 ; All ones mask
	; Gather has a 6 cycle throughput, 18 cycle latency
	;vpgatherqq ymm15, [data + readOffset], ymm13

	vextracti128 xmm13, readOffset, 1
	vpextrq tmp1, readOffsetXMM, 1
	vmovq tmp0, readOffsetXMM
	vmovq xmm15, qword ptr [data + tmp0]
	vpinsrq xmm15, xmm15, qword ptr [data + tmp1], 1
	vpextrq tmp1, xmm13, 1
	vmovq tmp0, xmm13
	vmovq xmm13, qword ptr [data + tmp0]
	vpinsrq xmm13, xmm13, qword ptr [data + tmp1], 1
	vinserti128 ymm15, ymm15, xmm13, 1

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

huff_16_stream_vector_decode PROC
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
	vpxor ymm8, ymm8, ymm8 ; bitCount0to3
	vpxor ymm9, ymm9, ymm9 ; bitCount4to7
	vpxor ymm10, ymm10, ymm10 ; bitCount8to11
	vpxor ymm11, ymm11, ymm11 ; bitCount12to15
	vpbroadcastq ymm8, [sixtyFour]
	vpbroadcastq ymm9, [sixtyFour]
	vpbroadcastq ymm10, [sixtyFour]
	vpbroadcastq ymm11, [sixtyFour]
	add rdx, 8
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

	EXTRACT_STEP_VECTOR rcx, r8, 0, ymm4, ymm8
	EXTRACT_STEP_VECTOR rcx, r8, 4, ymm5, ymm9
	EXTRACT_STEP_VECTOR rcx, r8, 8, ymm6, ymm10
	EXTRACT_STEP_VECTOR rcx, r8, 12, ymm7, ymm11
	EXTRACT_STEP_VECTOR rcx, r8, 16, ymm4, ymm8
	EXTRACT_STEP_VECTOR rcx, r8, 20, ymm5, ymm9
	EXTRACT_STEP_VECTOR rcx, r8, 24, ymm6, ymm10
	EXTRACT_STEP_VECTOR rcx, r8, 28, ymm7, ymm11
	EXTRACT_STEP_VECTOR rcx, r8, 32, ymm4, ymm8
	EXTRACT_STEP_VECTOR rcx, r8, 36, ymm5, ymm9
	EXTRACT_STEP_VECTOR rcx, r8, 40, ymm6, ymm10
	EXTRACT_STEP_VECTOR rcx, r8, 44, ymm7, ymm11
	EXTRACT_STEP_VECTOR rcx, r8, 48, ymm4, ymm8
	EXTRACT_STEP_VECTOR rcx, r8, 52, ymm5, ymm9
	EXTRACT_STEP_VECTOR rcx, r8, 56, ymm6, ymm10
	EXTRACT_STEP_VECTOR rcx, r8, 60, ymm7, ymm11
	EXTRACT_STEP_VECTOR rcx, r8, 64, ymm4, ymm8
	EXTRACT_STEP_VECTOR rcx, r8, 68, ymm5, ymm9
	EXTRACT_STEP_VECTOR rcx, r8, 72, ymm6, ymm10
	EXTRACT_STEP_VECTOR rcx, r8, 76, ymm7, ymm11

	;vpcmpeqb ymm12, ymm12, ymm12
	;vpsrlq ymm12, ymm12, 64 - 3 ; 3 bit mask
	vpbroadcastq ymm12, [sixtyFour]
	vmovdqa ymm0, ymmword ptr [rsp]
	vmovdqa ymm1, ymmword ptr [rsp + 32]

	REFILL_STEP_VECTOR rdx, rcx, r8, ymm0, xmm0, ymm4, ymm8
	REFILL_STEP_VECTOR rdx, rcx, r8, ymm1, xmm1, ymm5, ymm9
	REFILL_STEP_VECTOR rdx, rcx, r8, ymm2, xmm2, ymm6, ymm10
	REFILL_STEP_VECTOR rdx, rcx, r8, ymm3, xmm3, ymm7, ymm11

	vpcmpeqb ymm13, ymm13, ymm13 ; All ones mask
	vpsrlq ymm15, ymm13, 64 - HUFFMAN_MAX_DEPTH

	add rdi, 80
	cmp rdi, rbx
	jne decodeLoop
last80:

	vpsrld xmm1, xmm13, 24 ; dword low byte mask
	cmp esi, 0
	je finished
	EXTRACT_STEP_VECTOR rcx, r8, 0, ymm4, ymm8
	cmp esi, 4
	jb finished
	EXTRACT_STEP_VECTOR rcx, r8, 4, ymm5, ymm9
	cmp esi, 8
	jb finished
	EXTRACT_STEP_VECTOR rcx, r8, 8, ymm6, ymm10
	cmp esi, 12
	jb finished
	EXTRACT_STEP_VECTOR rcx, r8, 12, ymm7, ymm11
	cmp esi, 16
	jb finished
	EXTRACT_STEP_VECTOR rcx, r8, 16, ymm4, ymm8
	cmp esi, 20
	jb finished
	EXTRACT_STEP_VECTOR rcx, r8, 20, ymm5, ymm9
	cmp esi, 24
	jb finished
	EXTRACT_STEP_VECTOR rcx, r8, 24, ymm6, ymm10
	cmp esi, 28
	jb finished
	EXTRACT_STEP_VECTOR rcx, r8, 28, ymm7, ymm11
	cmp esi, 32
	jb finished
	EXTRACT_STEP_VECTOR rcx, r8, 32, ymm4, ymm8
	cmp esi, 36
	jb finished
	EXTRACT_STEP_VECTOR rcx, r8, 36, ymm5, ymm9
	cmp esi, 40
	jb finished
	EXTRACT_STEP_VECTOR rcx, r8, 40, ymm6, ymm10
	cmp esi, 44
	jb finished
	EXTRACT_STEP_VECTOR rcx, r8, 44, ymm7, ymm11
	cmp esi, 48
	jb finished
	EXTRACT_STEP_VECTOR rcx, r8, 48, ymm4, ymm8
	cmp esi, 52
	jb finished
	EXTRACT_STEP_VECTOR rcx, r8, 52, ymm5, ymm9
	cmp esi, 56
	jb finished
	EXTRACT_STEP_VECTOR rcx, r8, 56, ymm6, ymm10
	cmp esi, 60
	jb finished
	EXTRACT_STEP_VECTOR rcx, r8, 60, ymm7, ymm11
	cmp esi, 64
	jb finished
	EXTRACT_STEP_VECTOR rcx, r8, 64, ymm4, ymm8
	cmp esi, 68
	jb finished
	EXTRACT_STEP_VECTOR rcx, r8, 68, ymm5, ymm9
	cmp esi, 72
	jb finished
	EXTRACT_STEP_VECTOR rcx, r8, 72, ymm6, ymm10
	cmp esi, 76
	jb finished
	EXTRACT_STEP_VECTOR rcx, r8, 76, ymm7, ymm11
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
huff_16_stream_vector_decode ENDP


; Combines the 9 stream scalar decoder with the 16 stream vector decoder to take advantage of Zen 5's separate FPU
huff_hybrid_9_plus_16_stream_decode PROC
	; save callee preserved GPRs
	push rbx
	push rbp
	push rdi
	push rsi
	push r12
	push r13
	push r14
	push r15
	mov rbp, rsp
	and rsp, NOT 31 ; align 32
	sub rsp, 64 + 10 * 16 ; space for the callee preserved xmm registers + 2 ymm spills
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

	sub rsp, (1 + 1 + 9 + 1) * 8 ; one for end ptr, one for length, one for each stream, one for saved rbp
	mov [rsp + (1 + 1 + 9) * 8], rbp

	; 16 GPRs
	; 1 for stack ptr (rsp), 1 for write ptr (rdi), 1 for decode table (rax), 1 for scratch (rsi)
	; That leaves us 12 GPRs for data. We can spill the stream ptrs over the decode part, since stream ptrs are only needed for refills
	; We can fit 9 streams in GPRs then, with 3 for extra constants

	;struct HuffHybrid9Plus16DecodeCallArgs {
	;	U16* decodeTable;
	;	Byte* writePtr;
	;	U64 outputLen;
	;	Byte* readPtr[9];
	;	Byte* vectorReadPtr;
	;	U64 readOffset[16];
	;};
	mov rax, [rcx] ; decodeTable
	mov rdi, [rcx + 8] ; writePtr
	mov rsi, [rcx + 16] ; length
	mov [rsp + 8], rsi

	mov r15, 8000000000000000h

	; Load readPtrs into stack slots [rsp + 16] to [rsp + 80]
	; Load bitBuf0-5 into rdx, rbx, rbp, rcx, r8, r9
	; Load bitBuf6-8 into stack slots [rsp + 136] to [rsp + 152]
	mov r11, [rcx + 24] ; readPtr0
	mov rdx, [r11] ; bitBuf0
	shl rdx, 1
	or rdx, r15
	mov [rsp + 16], r11
	mov r11, [rcx + 32] ; readPtr1
	mov rbx, [r11] ; bitBuf1
	shl rbx, 1
	or rbx, r15
	mov [rsp + 24], r11
	mov r11, [rcx + 40] ; readPtr2
	mov rbp, [r11] ; bitBuf2
	shl rbp, 1
	or rbp, r15
	mov [rsp + 32], r11
	mov r11, [rcx + 48] ; readPtr3
	mov r13, [r11] ; bitBuf3 (will be moved to rcx)
	shl r13, 1
	or r13, r15
	mov [rsp + 40], r11
	mov r11, [rcx + 56] ; readPtr4
	mov r8, [r11] ; bitBuf4
	shl r8, 1
	or r8, r15
	mov [rsp + 48], r11
	mov r11, [rcx + 64] ; readPtr5
	mov r9, [r11] ; bitBuf5
	shl r9, 1
	or r9, r15
	mov [rsp + 56], r11
	mov r11, [rcx + 72] ; readPtr6
	mov r10, [r11] ; bitBuf6
	shl r10, 1
	or r10, r15
	mov [rsp + 64], r11
	mov r14, [rcx + 80] ; readPtr7
	mov r11, [r14] ; bitBuf7
	shl r11, 1
	or r11, r15
	mov [rsp + 72], r14
	mov r14, [rcx + 88] ; readPtr8
	mov r12, [r14] ; bitBuf8
	shl r12, 1
	or r12, r15
	mov [rsp + 80], r14

	; Same as the 16 stream vector decode setup
	mov r14, [rcx + 96] ; vectorReadPtr
	vmovdqu ymm0, ymmword ptr [rcx + 104] ; readOffset0to3
	vmovdqu ymm1, ymmword ptr [rcx + 136] ; readOffset4to7
	vmovdqu ymm2, ymmword ptr [rcx + 168] ; readOffset8to11
	vmovdqu ymm3, ymmword ptr [rcx + 200] ; readOffset12to15
	vpcmpeqb ymm13, ymm13, ymm13 ; All ones mask
	vpgatherqq ymm4, [r14 + ymm0], ymm13 ; bitBuf0to3
	vpcmpeqb ymm13, ymm13, ymm13 ; All ones mask
	vpgatherqq ymm5, [r14 + ymm1], ymm13 ; bitBur4to7
	vpcmpeqb ymm13, ymm13, ymm13 ; All ones mask
	vpgatherqq ymm6, [r14 + ymm2], ymm13 ; bitBuf8to11
	vpcmpeqb ymm13, ymm13, ymm13 ; All ones mask
	vpgatherqq ymm7, [r14 + ymm3], ymm13 ; bitBuf12to15
	vpcmpeqb ymm13, ymm13, ymm13 ; All ones mask
	vpxor ymm8, ymm8, ymm8 ; bitCount0to3
	vpxor ymm9, ymm9, ymm9 ; bitCount4to7
	vpxor ymm10, ymm10, ymm10 ; bitCount8to11
	vpxor ymm11, ymm11, ymm11 ; bitCount12to15
	vpbroadcastq ymm8, [sixtyFour]
	vpbroadcastq ymm9, [sixtyFour]
	vpbroadcastq ymm10, [sixtyFour]
	vpbroadcastq ymm11, [sixtyFour]
	add r14, 8
	vmovdqa xmm14, xmmword ptr [shuffleLowDwordBytesContiguous]
	vpsrlq ymm15, ymm13, 64 - HUFFMAN_MAX_DEPTH

	mov rcx, rsi ; length
	mov rsi, r13

	; 170 bytes per loop iteration (9 stream scalar decodes twice, 16 stream vector decodes once, unrolled 5 times before refill, (9 + 9 + 16) * 5 == 170)
	mov r15, 3233857729
	mov r13, rcx
	imul r13, r15
	shr r13, 39 ; "Magic Constant" divide by 170 (multiply by fixed point reciprocal)

	imul rcx, r13, 170 ; Multiply by 170 again so we get an even multiple
	sub [rsp + 8], rcx ; Get residual for after the main loop
	add rcx, rdi
	mov [rsp], rcx ; loop end pointer

	cmp rdi, [rsp] ; Length < 170
	je last170
EXTRACT_STEP_SCALAR_SINGLE_REG macro idx, bitBuf
	;mov rcx, bitBuf
	; The bottom bit of the register isn't used. This is so that the table load can be done with simple addressing (no index register shift), which cuts a cycle of the load latency on Zen 5
	; This means an extra cycle of latency has to be added to the refill, but it still ends up being a small perf win in practice
	;and ecx, ((1 SHL HUFFMAN_MAX_DEPTH) - 1) SHL 1
	andn rcx, r15, bitBuf
	movzx ecx, word ptr [rax + rcx]
	shrx bitBuf, bitBuf, rcx
	; Theoretically a high byte register (ah, ch, dh, bh) could be used to avoid the shift, but there's a bug on some CPUs that prevents this
	; https://fgiesen.wordpress.com/2025/05/21/oodle-2-9-14-and-intel-13th-14th-gen-cpus/
	; For the 9 stream decoder, the extra ALU3/4/5 pressure this shift causes significantly reduces decompression throughput
	; I might just leave it out and hope intel's microcode updates have fixed it (either that or duplicate this code for 13/14th gen intel, though at that point I might just tune a separate decompressor for intel CPUs)
	;shr ecx, 8
	;mov [rdi + idx], cl
	mov [rdi + idx], ch
endm
EXTRACT_STEP_SCALAR_SINGLE_REG_LAST macro idx, bitBuf, bitBufLo
	; Same as the other one, but does the lzcnt early
	andn rcx, r15, bitBuf
	lzcnt bitBuf, bitBuf
	movzx ecx, word ptr [rax + rcx]
	add bitBufLo, cl
	mov [rdi + idx], ch
endm
REFILL_STEP_SCALAR_SINGLE_REG macro bitBuf, readPtrStackSlot
	mov r13, [rsp + readPtrStackSlot]
	mov rcx, bitBuf
	shr bitBuf, 3
	and ecx, 7
	add r13, bitBuf
	mov bitBuf, [r13]
	add bitBuf, bitBuf ; shift left by 1 since we want it shifted for the extract step (add can run on more ports than shl)
	or bitBuf, r15 ; set high bit
	shrx bitBuf, bitBuf, rcx
	mov [rsp + readPtrStackSlot], r13
endm
DO_HYBRID_SCALAR_EXTRACTS macro
	EXTRACT_STEP_SCALAR_SINGLE_REG writeOffset + 0, rdx
	EXTRACT_STEP_SCALAR_SINGLE_REG writeOffset + 1, rbx
	EXTRACT_STEP_SCALAR_SINGLE_REG writeOffset + 2, rbp
	EXTRACT_STEP_SCALAR_SINGLE_REG writeOffset + 3, rsi
	EXTRACT_STEP_SCALAR_SINGLE_REG writeOffset + 4, r8
	EXTRACT_STEP_SCALAR_SINGLE_REG writeOffset + 5, r9
	EXTRACT_STEP_SCALAR_SINGLE_REG writeOffset + 6, r10
	EXTRACT_STEP_SCALAR_SINGLE_REG writeOffset + 7, r11
	EXTRACT_STEP_SCALAR_SINGLE_REG writeOffset + 8, r12
endm
DO_HYBRID_SCALAR_EXTRACTS_LAST macro
	EXTRACT_STEP_SCALAR_SINGLE_REG_LAST writeOffset + 0, rdx, dl
	EXTRACT_STEP_SCALAR_SINGLE_REG_LAST writeOffset + 1, rbx, bl
	EXTRACT_STEP_SCALAR_SINGLE_REG_LAST writeOffset + 2, rbp, bpl
	EXTRACT_STEP_SCALAR_SINGLE_REG_LAST writeOffset + 3, rsi, sil
	EXTRACT_STEP_SCALAR_SINGLE_REG_LAST writeOffset + 4, r8, r8b
	EXTRACT_STEP_SCALAR_SINGLE_REG_LAST writeOffset + 5, r9, r9b
	EXTRACT_STEP_SCALAR_SINGLE_REG_LAST writeOffset + 6, r10, r10b
	EXTRACT_STEP_SCALAR_SINGLE_REG_LAST writeOffset + 7, r11, r11b
	EXTRACT_STEP_SCALAR_SINGLE_REG_LAST writeOffset + 8, r12, r12b
endm
DO_HYBRID_SCALAR_REFILLS macro
	; readPtrs in stack slots [rsp + 16] to [rsp + 80]
	; Spilling the refill pointers hurts a little, but not all that much due to memory renaming
	REFILL_STEP_SCALAR_SINGLE_REG rdx, 16 ; readPtr0
	REFILL_STEP_SCALAR_SINGLE_REG rbx, 24 ; readPtr1
	REFILL_STEP_SCALAR_SINGLE_REG rbp, 32 ; readPtr2
	REFILL_STEP_SCALAR_SINGLE_REG rsi, 40 ; readPtr3
	REFILL_STEP_SCALAR_SINGLE_REG r8, 48 ; readPtr4
	REFILL_STEP_SCALAR_SINGLE_REG r9, 56 ; readPtr5
	REFILL_STEP_SCALAR_SINGLE_REG r10, 64 ; readPtr6
	REFILL_STEP_SCALAR_SINGLE_REG r11, 72 ; readPtr7
	REFILL_STEP_SCALAR_SINGLE_REG r12, 80 ; readPtr8
endm
DO_HYBRID_VECTOR_EXTRACTS macro
	EXTRACT_STEP_VECTOR rcx, r13, writeOffset + 0, ymm4, ymm8
	EXTRACT_STEP_VECTOR rcx, r13, writeOffset + 4, ymm5, ymm9
	EXTRACT_STEP_VECTOR rcx, r13, writeOffset + 8, ymm6, ymm10
	EXTRACT_STEP_VECTOR rcx, r13, writeOffset + 12, ymm7, ymm11
endm

decodeLoop:
	vmovdqa ymmword ptr [rsp + (1 + 1 + 9 + 1) * 8], ymm0
	vmovdqa ymmword ptr [rsp + (1 + 1 + 9 + 1) * 8 + 32], ymm1
	vpsrld xmm1, xmm13, 24 ; dword low byte mask

	mov r15, NOT (((1 SHL HUFFMAN_MAX_DEPTH) - 1) SHL 1)
	writeOffset = 0
	DO_HYBRID_SCALAR_EXTRACTS
	writeOffset = writeOffset + 9
	DO_HYBRID_SCALAR_EXTRACTS
	writeOffset = writeOffset + 9
	DO_HYBRID_VECTOR_EXTRACTS
	writeOffset = writeOffset + 16
	DO_HYBRID_SCALAR_EXTRACTS
	writeOffset = writeOffset + 9
	DO_HYBRID_SCALAR_EXTRACTS
	writeOffset = writeOffset + 9
	DO_HYBRID_VECTOR_EXTRACTS
	writeOffset = writeOffset + 16
	DO_HYBRID_SCALAR_EXTRACTS_LAST
	writeOffset = writeOffset + 9
	mov r15, 8000000000000000h
	DO_HYBRID_SCALAR_REFILLS
	mov r15, NOT (((1 SHL HUFFMAN_MAX_DEPTH) - 1) SHL 1)
	DO_HYBRID_SCALAR_EXTRACTS
	writeOffset = writeOffset + 9
	DO_HYBRID_VECTOR_EXTRACTS
	writeOffset = writeOffset + 16
	DO_HYBRID_SCALAR_EXTRACTS
	writeOffset = writeOffset + 9
	DO_HYBRID_SCALAR_EXTRACTS
	writeOffset = writeOffset + 9
	DO_HYBRID_VECTOR_EXTRACTS
	writeOffset = writeOffset + 16
	DO_HYBRID_SCALAR_EXTRACTS
	writeOffset = writeOffset + 9
	DO_HYBRID_SCALAR_EXTRACTS_LAST
	writeOffset = writeOffset + 9
	DO_HYBRID_VECTOR_EXTRACTS
	writeOffset = writeOffset + 16
	mov r15, 8000000000000000h
	DO_HYBRID_SCALAR_REFILLS

	vpbroadcastq ymm12, [sixtyFour]
	vmovdqa ymm0, ymmword ptr [rsp + (1 + 1 + 9 + 1) * 8]
	vmovdqa ymm1, ymmword ptr [rsp + (1 + 1 + 9 + 1) * 8 + 32]

	REFILL_STEP_VECTOR r14, rcx, r13, ymm0, xmm0, ymm4, ymm8
	REFILL_STEP_VECTOR r14, rcx, r13, ymm1, xmm1, ymm5, ymm9
	REFILL_STEP_VECTOR r14, rcx, r13, ymm2, xmm2, ymm6, ymm10
	REFILL_STEP_VECTOR r14, rcx, r13, ymm3, xmm3, ymm7, ymm11

	vpcmpeqb ymm13, ymm13, ymm13 ; All ones mask
	vpsrlq ymm15, ymm13, 64 - HUFFMAN_MAX_DEPTH

	add rdi, 170
	cmp rdi, [rsp]
	jne decodeLoop
last170:
	mov rcx, [rsp + 8] ; Get residual
	add [rsp], rcx
	cmp rdi, [rsp]
	je finished

	mov r15, NOT (((1 SHL HUFFMAN_MAX_DEPTH) - 1) SHL 1)
	vpsrld xmm1, xmm13, 24 ; dword low byte mask

FINISHED_CHECK macro amountWritten
	add rdi, amountWritten
	cmp rdi, [rsp]
	jae finished
endm

	mov r15, NOT (((1 SHL HUFFMAN_MAX_DEPTH) - 1) SHL 1)
	writeOffset = 0
	DO_HYBRID_SCALAR_EXTRACTS
	FINISHED_CHECK 9
	DO_HYBRID_SCALAR_EXTRACTS
	FINISHED_CHECK 9
	DO_HYBRID_VECTOR_EXTRACTS
	FINISHED_CHECK 16
	DO_HYBRID_SCALAR_EXTRACTS
	FINISHED_CHECK 9
	DO_HYBRID_SCALAR_EXTRACTS
	FINISHED_CHECK 9
	DO_HYBRID_VECTOR_EXTRACTS
	FINISHED_CHECK 16
	DO_HYBRID_SCALAR_EXTRACTS_LAST
	FINISHED_CHECK 9
	mov r15, 8000000000000000h
	DO_HYBRID_SCALAR_REFILLS
	mov r15, NOT (((1 SHL HUFFMAN_MAX_DEPTH) - 1) SHL 1)
	DO_HYBRID_SCALAR_EXTRACTS
	FINISHED_CHECK 9
	DO_HYBRID_VECTOR_EXTRACTS
	FINISHED_CHECK 16
	DO_HYBRID_SCALAR_EXTRACTS
	FINISHED_CHECK 9
	DO_HYBRID_SCALAR_EXTRACTS
	FINISHED_CHECK 9
	DO_HYBRID_VECTOR_EXTRACTS
	FINISHED_CHECK 16
	DO_HYBRID_SCALAR_EXTRACTS
	FINISHED_CHECK 9
	DO_HYBRID_SCALAR_EXTRACTS_LAST
	FINISHED_CHECK 9
	DO_HYBRID_VECTOR_EXTRACTS

finished:

	mov rax, [rsp] ; return endPtr

	mov rbp, [rsp + (1 + 1 + 9) * 8]
	add rsp, (1 + 1 + 9 + 1) * 8 ; one for end ptr, one for length, one for each stream, one for saved rbp
	; restore callee preserved GPRs
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
	pop r15
	pop r14
	pop r13
	pop r12
	pop rsi
	pop rdi
	pop rbp
	pop rbx
	ret
huff_hybrid_9_plus_16_stream_decode ENDP




test_ports PROC
	push rbx
	push rbp
	push rdi
	push rsi
	sub rsp, 1024
	xor r12d, r12d
	push r12
	lfence
	rdtsc
	shl rdx, 32
	or rax, rdx
	mov rcx, rax
	mov edx, 10000000
	vpxor xmm0, xmm0, xmm0
	mov [rsp + 160], rax
	mov [rsp + 176], rcx
	mov rax, rsp
	;mov [rsp], rax
	xor ebx, ebx
	ALIGN 32
testLoop:
REPEAT 16
	add rax, rax
	add rbp, rbp
	add rdi, rdi
	add rsi, rsi
	;lzcnt r8, r8
	;lzcnt r9, r9
ENDM

	dec edx
	jnz testLoop
	lfence
	rdtsc
	shl rdx, 32
	or rax, rdx
	sub rax, rcx
	pop r12
	add rsp, 1024
	pop rsi
	pop rdi
	pop rbp
	pop rbx
	vzeroupper
	ret
test_ports ENDP

huffSegment ENDS
END