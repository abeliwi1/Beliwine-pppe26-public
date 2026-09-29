	.file	"loop_fission.cpp"
	.intel_syntax noprefix
	.text
#APP
	.globl _ZSt21ios_base_library_initv
#NO_APP
	.type	_ZL14core_clock_ghzv, @function
_ZL14core_clock_ghzv:
.LFB3397:
	.cfi_startproc
	push	rbx
	.cfi_def_cfa_offset 16
	.cfi_offset 3, -16
	call	_ZNSt6chrono3_V212steady_clock3nowEv@PLT
	mov	rbx, rax
	mov	edx, 12500000
	mov	eax, 0
.L2:
#APP
# 136 "loop_fission.cpp" 1
	addq $1,rax
	addq $1,rax
	addq $1,rax
	addq $1,rax
	addq $1,rax
	addq $1,rax
	addq $1,rax
	addq $1,rax
# 0 "" 2
#NO_APP
	sub	rdx, 1
	jne	.L2
	call	_ZNSt6chrono3_V212steady_clock3nowEv@PLT
	sub	rax, rbx
	pxor	xmm1, xmm1
	cvtsi2sd	xmm1, rax
	movsd	xmm2, QWORD PTR .LC0[rip]
	divsd	xmm1, xmm2
	movsd	xmm0, QWORD PTR .LC1[rip]
	divsd	xmm0, xmm1
	divsd	xmm0, xmm2
	pop	rbx
	.cfi_def_cfa_offset 8
	ret
	.cfi_endproc
.LFE3397:
	.size	_ZL14core_clock_ghzv, .-_ZL14core_clock_ghzv
	.globl	_Z15correlate_fusedPKfPKS0_Pfi
	.type	_Z15correlate_fusedPKfPKS0_Pfi, @function
_Z15correlate_fusedPKfPKS0_Pfi:
.LFB3404:
	.cfi_startproc
	endbr64
	push	r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	push	r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	push	r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	push	r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	push	rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	push	rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	add	rsp, -128
	.cfi_def_cfa_offset 184
	test	ecx, ecx
	jle	.L8
	mov	rax, rsi
	mov	rbx, QWORD PTR [rsi]
	mov	QWORD PTR -48[rsp], rbx
	mov	rbx, QWORD PTR 8[rsi]
	mov	QWORD PTR -40[rsp], rbx
	mov	rsi, QWORD PTR 16[rsi]
	mov	QWORD PTR -32[rsp], rsi
	mov	rbx, QWORD PTR 24[rax]
	mov	QWORD PTR -24[rsp], rbx
	mov	rsi, QWORD PTR 32[rax]
	mov	QWORD PTR -16[rsp], rsi
	mov	rbx, QWORD PTR 40[rax]
	mov	QWORD PTR -8[rsp], rbx
	mov	rsi, QWORD PTR 48[rax]
	mov	QWORD PTR [rsp], rsi
	mov	rbx, QWORD PTR 56[rax]
	mov	QWORD PTR 8[rsp], rbx
	mov	rsi, QWORD PTR 64[rax]
	mov	QWORD PTR 16[rsp], rsi
	mov	rbx, QWORD PTR 72[rax]
	mov	QWORD PTR 24[rsp], rbx
	mov	rsi, QWORD PTR 80[rax]
	mov	QWORD PTR 32[rsp], rsi
	mov	rbx, QWORD PTR 88[rax]
	mov	QWORD PTR 40[rsp], rbx
	mov	rsi, QWORD PTR 96[rax]
	mov	QWORD PTR 48[rsp], rsi
	mov	rbx, QWORD PTR 104[rax]
	mov	QWORD PTR 56[rsp], rbx
	mov	rsi, QWORD PTR 112[rax]
	mov	QWORD PTR 64[rsp], rsi
	mov	rbx, QWORD PTR 120[rax]
	mov	QWORD PTR 72[rsp], rbx
	mov	rsi, QWORD PTR 128[rax]
	mov	QWORD PTR 80[rsp], rsi
	mov	r14, QWORD PTR 136[rax]
	mov	r13, QWORD PTR 144[rax]
	mov	rbx, QWORD PTR 152[rax]
	mov	QWORD PTR 88[rsp], rbx
	mov	rsi, QWORD PTR 160[rax]
	mov	QWORD PTR 96[rsp], rsi
	mov	rbx, QWORD PTR 168[rax]
	mov	QWORD PTR 104[rsp], rbx
	mov	rsi, QWORD PTR 176[rax]
	mov	QWORD PTR 112[rsp], rsi
	mov	r12, QWORD PTR 184[rax]
	mov	rbp, QWORD PTR 192[rax]
	mov	rbx, QWORD PTR 200[rax]
	mov	r11, QWORD PTR 208[rax]
	mov	r10, QWORD PTR 216[rax]
	mov	r9, QWORD PTR 224[rax]
	mov	r8, QWORD PTR 232[rax]
	mov	rsi, QWORD PTR 240[rax]
	mov	r15, QWORD PTR 248[rax]
	movsx	rcx, ecx
	sal	rcx, 2
	mov	eax, 0
	pxor	xmm1, xmm1
	movaps	xmm2, xmm1
	movaps	xmm3, xmm1
	movaps	xmm4, xmm1
	movaps	xmm5, xmm1
	movaps	xmm6, xmm1
	movaps	xmm7, xmm1
	movaps	xmm8, xmm1
	movaps	xmm9, xmm1
	movss	DWORD PTR -52[rsp], xmm1
	movaps	xmm10, xmm1
	movss	DWORD PTR -56[rsp], xmm1
	movss	DWORD PTR -60[rsp], xmm1
	movss	DWORD PTR -64[rsp], xmm1
	movss	DWORD PTR -68[rsp], xmm1
	movss	DWORD PTR -72[rsp], xmm1
	movss	DWORD PTR -76[rsp], xmm1
	movss	DWORD PTR -80[rsp], xmm1
	movss	DWORD PTR -84[rsp], xmm1
	movss	DWORD PTR -88[rsp], xmm1
	movss	DWORD PTR -92[rsp], xmm1
	movss	DWORD PTR -96[rsp], xmm1
	movss	DWORD PTR -100[rsp], xmm1
	movss	DWORD PTR -104[rsp], xmm1
	movss	DWORD PTR -108[rsp], xmm1
	movss	DWORD PTR -112[rsp], xmm1
	movss	DWORD PTR -116[rsp], xmm1
	movss	DWORD PTR -120[rsp], xmm1
	movaps	xmm11, xmm1
	movaps	xmm12, xmm1
	movaps	xmm13, xmm1
	movaps	xmm14, xmm1
	mov	QWORD PTR 120[rsp], rdx
.L7:
	movss	xmm0, DWORD PTR [rdi+rax]
	mov	rdx, QWORD PTR -48[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm14, xmm15
	mov	rdx, QWORD PTR -40[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm13, xmm15
	mov	rdx, QWORD PTR -32[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm12, xmm15
	mov	rdx, QWORD PTR -24[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm11, xmm15
	mov	rdx, QWORD PTR -16[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm15, DWORD PTR -120[rsp]
	movss	DWORD PTR -120[rsp], xmm15
	mov	rdx, QWORD PTR -8[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm15, DWORD PTR -116[rsp]
	movss	DWORD PTR -116[rsp], xmm15
	mov	rdx, QWORD PTR [rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm15, DWORD PTR -112[rsp]
	movss	DWORD PTR -112[rsp], xmm15
	mov	rdx, QWORD PTR 8[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm15, DWORD PTR -108[rsp]
	movss	DWORD PTR -108[rsp], xmm15
	mov	rdx, QWORD PTR 16[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm15, DWORD PTR -104[rsp]
	movss	DWORD PTR -104[rsp], xmm15
	mov	rdx, QWORD PTR 24[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm15, DWORD PTR -100[rsp]
	movss	DWORD PTR -100[rsp], xmm15
	mov	rdx, QWORD PTR 32[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm15, DWORD PTR -96[rsp]
	movss	DWORD PTR -96[rsp], xmm15
	mov	rdx, QWORD PTR 40[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm15, DWORD PTR -92[rsp]
	movss	DWORD PTR -92[rsp], xmm15
	mov	rdx, QWORD PTR 48[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm15, DWORD PTR -88[rsp]
	movss	DWORD PTR -88[rsp], xmm15
	mov	rdx, QWORD PTR 56[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm15, DWORD PTR -84[rsp]
	movss	DWORD PTR -84[rsp], xmm15
	mov	rdx, QWORD PTR 64[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm15, DWORD PTR -80[rsp]
	movss	DWORD PTR -80[rsp], xmm15
	mov	rdx, QWORD PTR 72[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm15, DWORD PTR -76[rsp]
	movss	DWORD PTR -76[rsp], xmm15
	mov	rdx, QWORD PTR 80[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm15, DWORD PTR -72[rsp]
	movss	DWORD PTR -72[rsp], xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r14+rax]
	addss	xmm15, DWORD PTR -68[rsp]
	movss	DWORD PTR -68[rsp], xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR 0[r13+rax]
	addss	xmm15, DWORD PTR -64[rsp]
	movss	DWORD PTR -64[rsp], xmm15
	mov	rdx, QWORD PTR 88[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm15, DWORD PTR -60[rsp]
	movss	DWORD PTR -60[rsp], xmm15
	mov	rdx, QWORD PTR 96[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm15, DWORD PTR -56[rsp]
	movss	DWORD PTR -56[rsp], xmm15
	mov	rdx, QWORD PTR 104[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm10, xmm15
	movaps	xmm15, xmm0
	mov	rdx, QWORD PTR 112[rsp]
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm15, DWORD PTR -52[rsp]
	movss	DWORD PTR -52[rsp], xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r12+rax]
	addss	xmm9, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR 0[rbp+rax]
	addss	xmm8, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rbx+rax]
	addss	xmm7, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r11+rax]
	addss	xmm6, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r10+rax]
	addss	xmm5, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r9+rax]
	addss	xmm4, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r8+rax]
	addss	xmm3, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rsi+rax]
	addss	xmm2, xmm15
	mulss	xmm0, DWORD PTR [r15+rax]
	addss	xmm1, xmm0
	add	rax, 4
	cmp	rax, rcx
	jne	.L7
	mov	rdx, QWORD PTR 120[rsp]
.L6:
	movss	DWORD PTR [rdx], xmm14
	movss	DWORD PTR 4[rdx], xmm13
	movss	DWORD PTR 8[rdx], xmm12
	movss	DWORD PTR 12[rdx], xmm11
	movss	xmm0, DWORD PTR -120[rsp]
	movss	DWORD PTR 16[rdx], xmm0
	movss	xmm0, DWORD PTR -116[rsp]
	movss	DWORD PTR 20[rdx], xmm0
	movss	xmm0, DWORD PTR -112[rsp]
	movss	DWORD PTR 24[rdx], xmm0
	movss	xmm0, DWORD PTR -108[rsp]
	movss	DWORD PTR 28[rdx], xmm0
	movss	xmm0, DWORD PTR -104[rsp]
	movss	DWORD PTR 32[rdx], xmm0
	movss	xmm0, DWORD PTR -100[rsp]
	movss	DWORD PTR 36[rdx], xmm0
	movss	xmm0, DWORD PTR -96[rsp]
	movss	DWORD PTR 40[rdx], xmm0
	movss	xmm0, DWORD PTR -92[rsp]
	movss	DWORD PTR 44[rdx], xmm0
	movss	xmm0, DWORD PTR -88[rsp]
	movss	DWORD PTR 48[rdx], xmm0
	movss	xmm0, DWORD PTR -84[rsp]
	movss	DWORD PTR 52[rdx], xmm0
	movss	xmm0, DWORD PTR -80[rsp]
	movss	DWORD PTR 56[rdx], xmm0
	movss	xmm0, DWORD PTR -76[rsp]
	movss	DWORD PTR 60[rdx], xmm0
	movss	xmm0, DWORD PTR -72[rsp]
	movss	DWORD PTR 64[rdx], xmm0
	movss	xmm0, DWORD PTR -68[rsp]
	movss	DWORD PTR 68[rdx], xmm0
	movss	xmm0, DWORD PTR -64[rsp]
	movss	DWORD PTR 72[rdx], xmm0
	movss	xmm0, DWORD PTR -60[rsp]
	movss	DWORD PTR 76[rdx], xmm0
	movss	xmm0, DWORD PTR -56[rsp]
	movss	DWORD PTR 80[rdx], xmm0
	movss	DWORD PTR 84[rdx], xmm10
	movss	xmm0, DWORD PTR -52[rsp]
	movss	DWORD PTR 88[rdx], xmm0
	movss	DWORD PTR 92[rdx], xmm9
	movss	DWORD PTR 96[rdx], xmm8
	movss	DWORD PTR 100[rdx], xmm7
	movss	DWORD PTR 104[rdx], xmm6
	movss	DWORD PTR 108[rdx], xmm5
	movss	DWORD PTR 112[rdx], xmm4
	movss	DWORD PTR 116[rdx], xmm3
	movss	DWORD PTR 120[rdx], xmm2
	movss	DWORD PTR 124[rdx], xmm1
	sub	rsp, -128
	.cfi_remember_state
	.cfi_def_cfa_offset 56
	pop	rbx
	.cfi_def_cfa_offset 48
	pop	rbp
	.cfi_def_cfa_offset 40
	pop	r12
	.cfi_def_cfa_offset 32
	pop	r13
	.cfi_def_cfa_offset 24
	pop	r14
	.cfi_def_cfa_offset 16
	pop	r15
	.cfi_def_cfa_offset 8
	ret
.L8:
	.cfi_restore_state
	pxor	xmm1, xmm1
	movaps	xmm2, xmm1
	movaps	xmm3, xmm1
	movaps	xmm4, xmm1
	movaps	xmm5, xmm1
	movaps	xmm6, xmm1
	movaps	xmm7, xmm1
	movaps	xmm8, xmm1
	movaps	xmm9, xmm1
	movss	DWORD PTR -52[rsp], xmm1
	movaps	xmm10, xmm1
	movss	DWORD PTR -56[rsp], xmm1
	movss	DWORD PTR -60[rsp], xmm1
	movss	DWORD PTR -64[rsp], xmm1
	movss	DWORD PTR -68[rsp], xmm1
	movss	DWORD PTR -72[rsp], xmm1
	movss	DWORD PTR -76[rsp], xmm1
	movss	DWORD PTR -80[rsp], xmm1
	movss	DWORD PTR -84[rsp], xmm1
	movss	DWORD PTR -88[rsp], xmm1
	movss	DWORD PTR -92[rsp], xmm1
	movss	DWORD PTR -96[rsp], xmm1
	movss	DWORD PTR -100[rsp], xmm1
	movss	DWORD PTR -104[rsp], xmm1
	movss	DWORD PTR -108[rsp], xmm1
	movss	DWORD PTR -112[rsp], xmm1
	movss	DWORD PTR -116[rsp], xmm1
	movss	DWORD PTR -120[rsp], xmm1
	movaps	xmm11, xmm1
	movaps	xmm12, xmm1
	movaps	xmm13, xmm1
	movaps	xmm14, xmm1
	jmp	.L6
	.cfi_endproc
.LFE3404:
	.size	_Z15correlate_fusedPKfPKS0_Pfi, .-_Z15correlate_fusedPKfPKS0_Pfi
	.globl	_Z17correlate_split16PKfPKS0_Pfi
	.type	_Z17correlate_split16PKfPKS0_Pfi, @function
_Z17correlate_split16PKfPKS0_Pfi:
.LFB3405:
	.cfi_startproc
	endbr64
	push	r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	push	r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	push	r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	push	r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	push	rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	push	rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	mov	QWORD PTR -32[rsp], rdi
	mov	DWORD PTR -24[rsp], ecx
	test	ecx, ecx
	jle	.L16
	mov	eax, ecx
	mov	r15, QWORD PTR [rsi]
	mov	r14, QWORD PTR 8[rsi]
	mov	r13, QWORD PTR 16[rsi]
	mov	r12, QWORD PTR 24[rsi]
	mov	rbp, QWORD PTR 32[rsi]
	mov	rbx, QWORD PTR 40[rsi]
	mov	r11, QWORD PTR 48[rsi]
	mov	r10, QWORD PTR 56[rsi]
	mov	r9, QWORD PTR 64[rsi]
	mov	r8, QWORD PTR 72[rsi]
	mov	rcx, QWORD PTR 80[rsi]
	mov	rdi, QWORD PTR 88[rsi]
	mov	QWORD PTR -72[rsp], rdi
	mov	rdi, QWORD PTR 96[rsi]
	mov	QWORD PTR -64[rsp], rdi
	mov	rdi, QWORD PTR 104[rsi]
	mov	QWORD PTR -56[rsp], rdi
	mov	rdi, QWORD PTR 112[rsi]
	mov	QWORD PTR -48[rsp], rdi
	mov	rdi, QWORD PTR 120[rsi]
	mov	QWORD PTR -40[rsp], rdi
	cdqe
	sal	rax, 2
	mov	QWORD PTR -16[rsp], rax
	mov	eax, 0
	mov	DWORD PTR -76[rsp], 0x00000000
	mov	DWORD PTR -80[rsp], 0x00000000
	pxor	xmm1, xmm1
	movaps	xmm2, xmm1
	movaps	xmm3, xmm1
	movaps	xmm4, xmm1
	movaps	xmm5, xmm1
	movaps	xmm6, xmm1
	movaps	xmm7, xmm1
	movaps	xmm8, xmm1
	movaps	xmm9, xmm1
	movaps	xmm10, xmm1
	movaps	xmm11, xmm1
	movaps	xmm12, xmm1
	movaps	xmm13, xmm1
	movaps	xmm14, xmm1
	mov	rdi, QWORD PTR -32[rsp]
	mov	QWORD PTR -8[rsp], rsi
	mov	rsi, QWORD PTR -16[rsp]
	mov	QWORD PTR -16[rsp], rdx
.L13:
	movss	xmm0, DWORD PTR [rdi+rax]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r15+rax]
	addss	xmm14, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r14+rax]
	addss	xmm13, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR 0[r13+rax]
	addss	xmm12, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r12+rax]
	addss	xmm11, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR 0[rbp+rax]
	addss	xmm10, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rbx+rax]
	addss	xmm9, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r11+rax]
	addss	xmm8, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r10+rax]
	addss	xmm7, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r9+rax]
	addss	xmm6, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r8+rax]
	addss	xmm5, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rcx+rax]
	addss	xmm4, xmm15
	mov	rdx, QWORD PTR -72[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm3, xmm15
	mov	rdx, QWORD PTR -64[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm2, xmm15
	mov	rdx, QWORD PTR -56[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm1, xmm15
	mov	rdx, QWORD PTR -48[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rdx+rax]
	addss	xmm15, DWORD PTR -80[rsp]
	movss	DWORD PTR -80[rsp], xmm15
	mov	rdx, QWORD PTR -40[rsp]
	mulss	xmm0, DWORD PTR [rdx+rax]
	addss	xmm0, DWORD PTR -76[rsp]
	movss	DWORD PTR -76[rsp], xmm0
	add	rax, 4
	cmp	rax, rsi
	jne	.L13
	mov	QWORD PTR -32[rsp], rdi
	mov	rsi, QWORD PTR -8[rsp]
	mov	rdx, QWORD PTR -16[rsp]
.L12:
	movss	DWORD PTR [rdx], xmm14
	movss	DWORD PTR 4[rdx], xmm13
	movss	DWORD PTR 8[rdx], xmm12
	movss	DWORD PTR 12[rdx], xmm11
	movss	DWORD PTR 16[rdx], xmm10
	movss	DWORD PTR 20[rdx], xmm9
	movss	DWORD PTR 24[rdx], xmm8
	movss	DWORD PTR 28[rdx], xmm7
	movss	DWORD PTR 32[rdx], xmm6
	movss	DWORD PTR 36[rdx], xmm5
	movss	DWORD PTR 40[rdx], xmm4
	movss	DWORD PTR 44[rdx], xmm3
	movss	DWORD PTR 48[rdx], xmm2
	movss	DWORD PTR 52[rdx], xmm1
	movss	xmm1, DWORD PTR -80[rsp]
	movss	DWORD PTR 56[rdx], xmm1
	movss	xmm2, DWORD PTR -76[rsp]
	movss	DWORD PTR 60[rdx], xmm2
	cmp	DWORD PTR -24[rsp], 0
	jle	.L17
	mov	r15, QWORD PTR 128[rsi]
	mov	r14, QWORD PTR 136[rsi]
	mov	r13, QWORD PTR 144[rsi]
	mov	r12, QWORD PTR 152[rsi]
	mov	rbp, QWORD PTR 160[rsi]
	mov	rbx, QWORD PTR 168[rsi]
	mov	r11, QWORD PTR 176[rsi]
	mov	r10, QWORD PTR 184[rsi]
	mov	r9, QWORD PTR 192[rsi]
	mov	r8, QWORD PTR 200[rsi]
	mov	rcx, QWORD PTR 208[rsi]
	mov	rax, QWORD PTR 216[rsi]
	mov	rdi, QWORD PTR 224[rsi]
	mov	QWORD PTR -72[rsp], rdi
	mov	rdi, QWORD PTR 232[rsi]
	mov	QWORD PTR -64[rsp], rdi
	mov	rdi, QWORD PTR 240[rsi]
	mov	QWORD PTR -56[rsp], rdi
	mov	rdi, QWORD PTR 248[rsi]
	mov	QWORD PTR -48[rsp], rdi
	movsx	rsi, DWORD PTR -24[rsp]
	lea	rdi, 0[0+rsi*4]
	mov	QWORD PTR -24[rsp], rdi
	mov	esi, 0
	mov	DWORD PTR -76[rsp], 0x00000000
	mov	DWORD PTR -80[rsp], 0x00000000
	pxor	xmm1, xmm1
	movaps	xmm2, xmm1
	movaps	xmm3, xmm1
	movaps	xmm4, xmm1
	movaps	xmm5, xmm1
	movaps	xmm6, xmm1
	movaps	xmm7, xmm1
	movaps	xmm8, xmm1
	movaps	xmm9, xmm1
	movaps	xmm10, xmm1
	movaps	xmm11, xmm1
	movaps	xmm12, xmm1
	movaps	xmm13, xmm1
	movaps	xmm14, xmm1
	mov	QWORD PTR -40[rsp], rax
	mov	rdi, QWORD PTR -32[rsp]
	mov	QWORD PTR -32[rsp], rdx
	mov	rdx, QWORD PTR -24[rsp]
.L15:
	movss	xmm0, DWORD PTR [rdi+rsi]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r15+rsi]
	addss	xmm14, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r14+rsi]
	addss	xmm13, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR 0[r13+rsi]
	addss	xmm12, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r12+rsi]
	addss	xmm11, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR 0[rbp+rsi]
	addss	xmm10, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rbx+rsi]
	addss	xmm9, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r11+rsi]
	addss	xmm8, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r10+rsi]
	addss	xmm7, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r9+rsi]
	addss	xmm6, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [r8+rsi]
	addss	xmm5, xmm15
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rcx+rsi]
	addss	xmm4, xmm15
	movaps	xmm15, xmm0
	mov	rax, QWORD PTR -40[rsp]
	mulss	xmm15, DWORD PTR [rax+rsi]
	addss	xmm3, xmm15
	mov	rax, QWORD PTR -72[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rax+rsi]
	addss	xmm2, xmm15
	mov	rax, QWORD PTR -64[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rax+rsi]
	addss	xmm1, xmm15
	mov	rax, QWORD PTR -56[rsp]
	movaps	xmm15, xmm0
	mulss	xmm15, DWORD PTR [rax+rsi]
	addss	xmm15, DWORD PTR -80[rsp]
	movss	DWORD PTR -80[rsp], xmm15
	mov	rax, QWORD PTR -48[rsp]
	mulss	xmm0, DWORD PTR [rax+rsi]
	addss	xmm0, DWORD PTR -76[rsp]
	movss	DWORD PTR -76[rsp], xmm0
	add	rsi, 4
	cmp	rsi, rdx
	jne	.L15
	mov	rdx, QWORD PTR -32[rsp]
.L14:
	movss	DWORD PTR 64[rdx], xmm14
	movss	DWORD PTR 68[rdx], xmm13
	movss	DWORD PTR 72[rdx], xmm12
	movss	DWORD PTR 76[rdx], xmm11
	movss	DWORD PTR 80[rdx], xmm10
	movss	DWORD PTR 84[rdx], xmm9
	movss	DWORD PTR 88[rdx], xmm8
	movss	DWORD PTR 92[rdx], xmm7
	movss	DWORD PTR 96[rdx], xmm6
	movss	DWORD PTR 100[rdx], xmm5
	movss	DWORD PTR 104[rdx], xmm4
	movss	DWORD PTR 108[rdx], xmm3
	movss	DWORD PTR 112[rdx], xmm2
	movss	DWORD PTR 116[rdx], xmm1
	movss	xmm3, DWORD PTR -80[rsp]
	movss	DWORD PTR 120[rdx], xmm3
	movss	xmm4, DWORD PTR -76[rsp]
	movss	DWORD PTR 124[rdx], xmm4
	pop	rbx
	.cfi_remember_state
	.cfi_def_cfa_offset 48
	pop	rbp
	.cfi_def_cfa_offset 40
	pop	r12
	.cfi_def_cfa_offset 32
	pop	r13
	.cfi_def_cfa_offset 24
	pop	r14
	.cfi_def_cfa_offset 16
	pop	r15
	.cfi_def_cfa_offset 8
	ret
.L16:
	.cfi_restore_state
	mov	DWORD PTR -76[rsp], 0x00000000
	mov	DWORD PTR -80[rsp], 0x00000000
	pxor	xmm1, xmm1
	movaps	xmm2, xmm1
	movaps	xmm3, xmm1
	movaps	xmm4, xmm1
	movaps	xmm5, xmm1
	movaps	xmm6, xmm1
	movaps	xmm7, xmm1
	movaps	xmm8, xmm1
	movaps	xmm9, xmm1
	movaps	xmm10, xmm1
	movaps	xmm11, xmm1
	movaps	xmm12, xmm1
	movaps	xmm13, xmm1
	movaps	xmm14, xmm1
	jmp	.L12
.L17:
	mov	DWORD PTR -76[rsp], 0x00000000
	mov	DWORD PTR -80[rsp], 0x00000000
	pxor	xmm1, xmm1
	movaps	xmm2, xmm1
	movaps	xmm3, xmm1
	movaps	xmm4, xmm1
	movaps	xmm5, xmm1
	movaps	xmm6, xmm1
	movaps	xmm7, xmm1
	movaps	xmm8, xmm1
	movaps	xmm9, xmm1
	movaps	xmm10, xmm1
	movaps	xmm11, xmm1
	movaps	xmm12, xmm1
	movaps	xmm13, xmm1
	movaps	xmm14, xmm1
	jmp	.L14
	.cfi_endproc
.LFE3405:
	.size	_Z17correlate_split16PKfPKS0_Pfi, .-_Z17correlate_split16PKfPKS0_Pfi
	.globl	_Z16correlate_split8PKfPKS0_Pfi
	.type	_Z16correlate_split8PKfPKS0_Pfi, @function
_Z16correlate_split8PKfPKS0_Pfi:
.LFB3406:
	.cfi_startproc
	endbr64
	push	r14
	.cfi_def_cfa_offset 16
	.cfi_offset 14, -16
	push	r13
	.cfi_def_cfa_offset 24
	.cfi_offset 13, -24
	push	r12
	.cfi_def_cfa_offset 32
	.cfi_offset 12, -32
	push	rbp
	.cfi_def_cfa_offset 40
	.cfi_offset 6, -40
	push	rbx
	.cfi_def_cfa_offset 48
	.cfi_offset 3, -48
	mov	r8, rsi
	mov	rsi, rdx
	test	ecx, ecx
	jle	.L22
	mov	r14, QWORD PTR [r8]
	mov	r13, QWORD PTR 8[r8]
	mov	r12, QWORD PTR 16[r8]
	mov	rbp, QWORD PTR 24[r8]
	mov	rbx, QWORD PTR 32[r8]
	mov	r11, QWORD PTR 40[r8]
	mov	r10, QWORD PTR 48[r8]
	mov	r9, QWORD PTR 56[r8]
	movsx	rcx, ecx
	lea	rdx, 0[0+rcx*4]
	mov	eax, 0
	pxor	xmm1, xmm1
	movaps	xmm2, xmm1
	movaps	xmm3, xmm1
	movaps	xmm4, xmm1
	movaps	xmm5, xmm1
	movaps	xmm6, xmm1
	movaps	xmm7, xmm1
	movaps	xmm8, xmm1
.L23:
	movss	xmm0, DWORD PTR [rdi+rax]
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [r14+rax]
	addss	xmm8, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR 0[r13+rax]
	addss	xmm7, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [r12+rax]
	addss	xmm6, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR 0[rbp+rax]
	addss	xmm5, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [rbx+rax]
	addss	xmm4, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [r11+rax]
	addss	xmm3, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [r10+rax]
	addss	xmm2, xmm9
	mulss	xmm0, DWORD PTR [r9+rax]
	addss	xmm1, xmm0
	add	rax, 4
	cmp	rdx, rax
	jne	.L23
	movss	DWORD PTR [rsi], xmm8
	movss	DWORD PTR 4[rsi], xmm7
	movss	DWORD PTR 8[rsi], xmm6
	movss	DWORD PTR 12[rsi], xmm5
	movss	DWORD PTR 16[rsi], xmm4
	movss	DWORD PTR 20[rsi], xmm3
	movss	DWORD PTR 24[rsi], xmm2
	movss	DWORD PTR 28[rsi], xmm1
	mov	r13, QWORD PTR 64[r8]
	mov	r12, QWORD PTR 72[r8]
	mov	rbp, QWORD PTR 80[r8]
	mov	rbx, QWORD PTR 88[r8]
	mov	r11, QWORD PTR 96[r8]
	mov	r10, QWORD PTR 104[r8]
	mov	r9, QWORD PTR 112[r8]
	mov	rcx, QWORD PTR 120[r8]
	mov	eax, 0
	pxor	xmm1, xmm1
	movaps	xmm2, xmm1
	movaps	xmm3, xmm1
	movaps	xmm4, xmm1
	movaps	xmm5, xmm1
	movaps	xmm6, xmm1
	movaps	xmm7, xmm1
	movaps	xmm8, xmm1
.L24:
	movss	xmm0, DWORD PTR [rdi+rax]
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR 0[r13+rax]
	addss	xmm8, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [r12+rax]
	addss	xmm7, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR 0[rbp+rax]
	addss	xmm6, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [rbx+rax]
	addss	xmm5, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [r11+rax]
	addss	xmm4, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [r10+rax]
	addss	xmm3, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [r9+rax]
	addss	xmm2, xmm9
	mulss	xmm0, DWORD PTR [rcx+rax]
	addss	xmm1, xmm0
	add	rax, 4
	cmp	rdx, rax
	jne	.L24
	movss	DWORD PTR 32[rsi], xmm8
	movss	DWORD PTR 36[rsi], xmm7
	movss	DWORD PTR 40[rsi], xmm6
	movss	DWORD PTR 44[rsi], xmm5
	movss	DWORD PTR 48[rsi], xmm4
	movss	DWORD PTR 52[rsi], xmm3
	movss	DWORD PTR 56[rsi], xmm2
	movss	DWORD PTR 60[rsi], xmm1
	mov	r13, QWORD PTR 128[r8]
	mov	r12, QWORD PTR 136[r8]
	mov	rbp, QWORD PTR 144[r8]
	mov	rbx, QWORD PTR 152[r8]
	mov	r11, QWORD PTR 160[r8]
	mov	r10, QWORD PTR 168[r8]
	mov	r9, QWORD PTR 176[r8]
	mov	rcx, QWORD PTR 184[r8]
	mov	eax, 0
	pxor	xmm1, xmm1
	movaps	xmm2, xmm1
	movaps	xmm3, xmm1
	movaps	xmm4, xmm1
	movaps	xmm5, xmm1
	movaps	xmm6, xmm1
	movaps	xmm7, xmm1
	movaps	xmm8, xmm1
.L25:
	movss	xmm0, DWORD PTR [rdi+rax]
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR 0[r13+rax]
	addss	xmm8, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [r12+rax]
	addss	xmm7, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR 0[rbp+rax]
	addss	xmm6, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [rbx+rax]
	addss	xmm5, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [r11+rax]
	addss	xmm4, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [r10+rax]
	addss	xmm3, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [r9+rax]
	addss	xmm2, xmm9
	mulss	xmm0, DWORD PTR [rcx+rax]
	addss	xmm1, xmm0
	add	rax, 4
	cmp	rax, rdx
	jne	.L25
	movss	DWORD PTR 64[rsi], xmm8
	movss	DWORD PTR 68[rsi], xmm7
	movss	DWORD PTR 72[rsi], xmm6
	movss	DWORD PTR 76[rsi], xmm5
	movss	DWORD PTR 80[rsi], xmm4
	movss	DWORD PTR 84[rsi], xmm3
	movss	DWORD PTR 88[rsi], xmm2
	movss	DWORD PTR 92[rsi], xmm1
	mov	r13, QWORD PTR 192[r8]
	mov	r12, QWORD PTR 200[r8]
	mov	rbp, QWORD PTR 208[r8]
	mov	rbx, QWORD PTR 216[r8]
	mov	r11, QWORD PTR 224[r8]
	mov	r10, QWORD PTR 232[r8]
	mov	r9, QWORD PTR 240[r8]
	mov	rcx, QWORD PTR 248[r8]
	mov	eax, 0
	pxor	xmm8, xmm8
	movaps	xmm7, xmm8
	movaps	xmm6, xmm8
	movaps	xmm5, xmm8
	movaps	xmm4, xmm8
	movaps	xmm3, xmm8
	movaps	xmm2, xmm8
	movaps	xmm1, xmm8
.L26:
	movss	xmm0, DWORD PTR [rdi+rax]
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR 0[r13+rax]
	addss	xmm1, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [r12+rax]
	addss	xmm2, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR 0[rbp+rax]
	addss	xmm3, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [rbx+rax]
	addss	xmm4, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [r11+rax]
	addss	xmm5, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [r10+rax]
	addss	xmm6, xmm9
	movaps	xmm9, xmm0
	mulss	xmm9, DWORD PTR [r9+rax]
	addss	xmm7, xmm9
	mulss	xmm0, DWORD PTR [rcx+rax]
	addss	xmm8, xmm0
	add	rax, 4
	cmp	rax, rdx
	jne	.L26
.L27:
	movss	DWORD PTR 96[rsi], xmm1
	movss	DWORD PTR 100[rsi], xmm2
	movss	DWORD PTR 104[rsi], xmm3
	movss	DWORD PTR 108[rsi], xmm4
	movss	DWORD PTR 112[rsi], xmm5
	movss	DWORD PTR 116[rsi], xmm6
	movss	DWORD PTR 120[rsi], xmm7
	movss	DWORD PTR 124[rsi], xmm8
	pop	rbx
	.cfi_remember_state
	.cfi_def_cfa_offset 40
	pop	rbp
	.cfi_def_cfa_offset 32
	pop	r12
	.cfi_def_cfa_offset 24
	pop	r13
	.cfi_def_cfa_offset 16
	pop	r14
	.cfi_def_cfa_offset 8
	ret
.L22:
	.cfi_restore_state
	mov	DWORD PTR [rdx], 0x00000000
	mov	DWORD PTR 4[rdx], 0x00000000
	mov	DWORD PTR 8[rdx], 0x00000000
	mov	DWORD PTR 12[rdx], 0x00000000
	mov	DWORD PTR 16[rdx], 0x00000000
	mov	DWORD PTR 20[rdx], 0x00000000
	mov	DWORD PTR 24[rdx], 0x00000000
	mov	DWORD PTR 28[rdx], 0x00000000
	mov	DWORD PTR 32[rdx], 0x00000000
	mov	DWORD PTR 36[rdx], 0x00000000
	mov	DWORD PTR 40[rdx], 0x00000000
	mov	DWORD PTR 44[rdx], 0x00000000
	mov	DWORD PTR 48[rdx], 0x00000000
	mov	DWORD PTR 52[rdx], 0x00000000
	mov	DWORD PTR 56[rdx], 0x00000000
	mov	DWORD PTR 60[rdx], 0x00000000
	mov	DWORD PTR 64[rdx], 0x00000000
	mov	DWORD PTR 68[rdx], 0x00000000
	mov	DWORD PTR 72[rdx], 0x00000000
	mov	DWORD PTR 76[rdx], 0x00000000
	mov	DWORD PTR 80[rdx], 0x00000000
	mov	DWORD PTR 84[rdx], 0x00000000
	mov	DWORD PTR 88[rdx], 0x00000000
	mov	DWORD PTR 92[rdx], 0x00000000
	pxor	xmm8, xmm8
	movaps	xmm7, xmm8
	movaps	xmm6, xmm8
	movaps	xmm5, xmm8
	movaps	xmm4, xmm8
	movaps	xmm3, xmm8
	movaps	xmm2, xmm8
	movaps	xmm1, xmm8
	jmp	.L27
	.cfi_endproc
.LFE3406:
	.size	_Z16correlate_split8PKfPKS0_Pfi, .-_Z16correlate_split8PKfPKS0_Pfi
	.globl	_Z16correlate_split4PKfPKS0_Pfi
	.type	_Z16correlate_split4PKfPKS0_Pfi, @function
_Z16correlate_split4PKfPKS0_Pfi:
.LFB3407:
	.cfi_startproc
	endbr64
	push	rbx
	.cfi_def_cfa_offset 16
	.cfi_offset 3, -16
	mov	r8, rsi
	mov	rsi, rdx
	test	ecx, ecx
	jle	.L35
	mov	rbx, QWORD PTR [r8]
	mov	r11, QWORD PTR 8[r8]
	mov	r10, QWORD PTR 16[r8]
	mov	r9, QWORD PTR 24[r8]
	movsx	rcx, ecx
	lea	rax, 0[0+rcx*4]
	mov	edx, 0
	pxor	xmm1, xmm1
	movaps	xmm2, xmm1
	movaps	xmm3, xmm1
	movaps	xmm4, xmm1
.L36:
	movss	xmm0, DWORD PTR [rdi+rdx]
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [rbx+rdx]
	addss	xmm4, xmm5
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r11+rdx]
	addss	xmm3, xmm5
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r10+rdx]
	addss	xmm2, xmm5
	mulss	xmm0, DWORD PTR [r9+rdx]
	addss	xmm1, xmm0
	add	rdx, 4
	cmp	rax, rdx
	jne	.L36
	movss	DWORD PTR [rsi], xmm4
	movss	DWORD PTR 4[rsi], xmm3
	movss	DWORD PTR 8[rsi], xmm2
	movss	DWORD PTR 12[rsi], xmm1
	mov	r11, QWORD PTR 32[r8]
	mov	r10, QWORD PTR 40[r8]
	mov	r9, QWORD PTR 48[r8]
	mov	rcx, QWORD PTR 56[r8]
	mov	edx, 0
	pxor	xmm1, xmm1
	movaps	xmm2, xmm1
	movaps	xmm3, xmm1
	movaps	xmm4, xmm1
.L37:
	movss	xmm0, DWORD PTR [rdi+rdx]
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r11+rdx]
	addss	xmm4, xmm5
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r10+rdx]
	addss	xmm3, xmm5
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r9+rdx]
	addss	xmm2, xmm5
	mulss	xmm0, DWORD PTR [rcx+rdx]
	addss	xmm1, xmm0
	add	rdx, 4
	cmp	rax, rdx
	jne	.L37
	movss	DWORD PTR 16[rsi], xmm4
	movss	DWORD PTR 20[rsi], xmm3
	movss	DWORD PTR 24[rsi], xmm2
	movss	DWORD PTR 28[rsi], xmm1
	mov	r11, QWORD PTR 64[r8]
	mov	r10, QWORD PTR 72[r8]
	mov	r9, QWORD PTR 80[r8]
	mov	rcx, QWORD PTR 88[r8]
	mov	edx, 0
	pxor	xmm1, xmm1
	movaps	xmm2, xmm1
	movaps	xmm3, xmm1
	movaps	xmm4, xmm1
.L38:
	movss	xmm0, DWORD PTR [rdi+rdx]
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r11+rdx]
	addss	xmm4, xmm5
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r10+rdx]
	addss	xmm3, xmm5
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r9+rdx]
	addss	xmm2, xmm5
	mulss	xmm0, DWORD PTR [rcx+rdx]
	addss	xmm1, xmm0
	add	rdx, 4
	cmp	rdx, rax
	jne	.L38
	movss	DWORD PTR 32[rsi], xmm4
	movss	DWORD PTR 36[rsi], xmm3
	movss	DWORD PTR 40[rsi], xmm2
	movss	DWORD PTR 44[rsi], xmm1
	mov	r11, QWORD PTR 96[r8]
	mov	r10, QWORD PTR 104[r8]
	mov	r9, QWORD PTR 112[r8]
	mov	rcx, QWORD PTR 120[r8]
	mov	edx, 0
	pxor	xmm1, xmm1
	movaps	xmm2, xmm1
	movaps	xmm3, xmm1
	movaps	xmm4, xmm1
.L39:
	movss	xmm0, DWORD PTR [rdi+rdx]
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r11+rdx]
	addss	xmm4, xmm5
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r10+rdx]
	addss	xmm3, xmm5
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r9+rdx]
	addss	xmm2, xmm5
	mulss	xmm0, DWORD PTR [rcx+rdx]
	addss	xmm1, xmm0
	add	rdx, 4
	cmp	rdx, rax
	jne	.L39
	movss	DWORD PTR 48[rsi], xmm4
	movss	DWORD PTR 52[rsi], xmm3
	movss	DWORD PTR 56[rsi], xmm2
	movss	DWORD PTR 60[rsi], xmm1
	mov	r11, QWORD PTR 128[r8]
	mov	r10, QWORD PTR 136[r8]
	mov	r9, QWORD PTR 144[r8]
	mov	rcx, QWORD PTR 152[r8]
	mov	edx, 0
	pxor	xmm1, xmm1
	movaps	xmm2, xmm1
	movaps	xmm3, xmm1
	movaps	xmm4, xmm1
.L40:
	movss	xmm0, DWORD PTR [rdi+rdx]
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r11+rdx]
	addss	xmm4, xmm5
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r10+rdx]
	addss	xmm3, xmm5
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r9+rdx]
	addss	xmm2, xmm5
	mulss	xmm0, DWORD PTR [rcx+rdx]
	addss	xmm1, xmm0
	add	rdx, 4
	cmp	rdx, rax
	jne	.L40
	movss	DWORD PTR 64[rsi], xmm4
	movss	DWORD PTR 68[rsi], xmm3
	movss	DWORD PTR 72[rsi], xmm2
	movss	DWORD PTR 76[rsi], xmm1
	mov	r11, QWORD PTR 160[r8]
	mov	r10, QWORD PTR 168[r8]
	mov	r9, QWORD PTR 176[r8]
	mov	rcx, QWORD PTR 184[r8]
	mov	edx, 0
	pxor	xmm1, xmm1
	movaps	xmm2, xmm1
	movaps	xmm3, xmm1
	movaps	xmm4, xmm1
.L41:
	movss	xmm0, DWORD PTR [rdi+rdx]
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r11+rdx]
	addss	xmm4, xmm5
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r10+rdx]
	addss	xmm3, xmm5
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r9+rdx]
	addss	xmm2, xmm5
	mulss	xmm0, DWORD PTR [rcx+rdx]
	addss	xmm1, xmm0
	add	rdx, 4
	cmp	rdx, rax
	jne	.L41
	movss	DWORD PTR 80[rsi], xmm4
	movss	DWORD PTR 84[rsi], xmm3
	movss	DWORD PTR 88[rsi], xmm2
	movss	DWORD PTR 92[rsi], xmm1
	mov	r11, QWORD PTR 192[r8]
	mov	r10, QWORD PTR 200[r8]
	mov	r9, QWORD PTR 208[r8]
	mov	rcx, QWORD PTR 216[r8]
	mov	edx, 0
	pxor	xmm1, xmm1
	movaps	xmm2, xmm1
	movaps	xmm3, xmm1
	movaps	xmm4, xmm1
.L42:
	movss	xmm0, DWORD PTR [rdi+rdx]
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r11+rdx]
	addss	xmm4, xmm5
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r10+rdx]
	addss	xmm3, xmm5
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r9+rdx]
	addss	xmm2, xmm5
	mulss	xmm0, DWORD PTR [rcx+rdx]
	addss	xmm1, xmm0
	add	rdx, 4
	cmp	rdx, rax
	jne	.L42
	movss	DWORD PTR 96[rsi], xmm4
	movss	DWORD PTR 100[rsi], xmm3
	movss	DWORD PTR 104[rsi], xmm2
	movss	DWORD PTR 108[rsi], xmm1
	mov	r11, QWORD PTR 224[r8]
	mov	r10, QWORD PTR 232[r8]
	mov	r9, QWORD PTR 240[r8]
	mov	rcx, QWORD PTR 248[r8]
	mov	edx, 0
	pxor	xmm4, xmm4
	movaps	xmm3, xmm4
	movaps	xmm2, xmm4
	movaps	xmm1, xmm4
.L43:
	movss	xmm0, DWORD PTR [rdi+rdx]
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r11+rdx]
	addss	xmm1, xmm5
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r10+rdx]
	addss	xmm2, xmm5
	movaps	xmm5, xmm0
	mulss	xmm5, DWORD PTR [r9+rdx]
	addss	xmm3, xmm5
	mulss	xmm0, DWORD PTR [rcx+rdx]
	addss	xmm4, xmm0
	add	rdx, 4
	cmp	rax, rdx
	jne	.L43
.L44:
	movss	DWORD PTR 112[rsi], xmm1
	movss	DWORD PTR 116[rsi], xmm2
	movss	DWORD PTR 120[rsi], xmm3
	movss	DWORD PTR 124[rsi], xmm4
	pop	rbx
	.cfi_remember_state
	.cfi_def_cfa_offset 8
	ret
.L35:
	.cfi_restore_state
	mov	DWORD PTR [rdx], 0x00000000
	mov	DWORD PTR 4[rdx], 0x00000000
	mov	DWORD PTR 8[rdx], 0x00000000
	mov	DWORD PTR 12[rdx], 0x00000000
	mov	DWORD PTR 16[rdx], 0x00000000
	mov	DWORD PTR 20[rdx], 0x00000000
	mov	DWORD PTR 24[rdx], 0x00000000
	mov	DWORD PTR 28[rdx], 0x00000000
	mov	DWORD PTR 32[rdx], 0x00000000
	mov	DWORD PTR 36[rdx], 0x00000000
	mov	DWORD PTR 40[rdx], 0x00000000
	mov	DWORD PTR 44[rdx], 0x00000000
	mov	DWORD PTR 48[rdx], 0x00000000
	mov	DWORD PTR 52[rdx], 0x00000000
	mov	DWORD PTR 56[rdx], 0x00000000
	mov	DWORD PTR 60[rdx], 0x00000000
	mov	DWORD PTR 64[rdx], 0x00000000
	mov	DWORD PTR 68[rdx], 0x00000000
	mov	DWORD PTR 72[rdx], 0x00000000
	mov	DWORD PTR 76[rdx], 0x00000000
	mov	DWORD PTR 80[rdx], 0x00000000
	mov	DWORD PTR 84[rdx], 0x00000000
	mov	DWORD PTR 88[rdx], 0x00000000
	mov	DWORD PTR 92[rdx], 0x00000000
	mov	DWORD PTR 96[rdx], 0x00000000
	mov	DWORD PTR 100[rdx], 0x00000000
	mov	DWORD PTR 104[rdx], 0x00000000
	mov	DWORD PTR 108[rdx], 0x00000000
	pxor	xmm4, xmm4
	movaps	xmm3, xmm4
	movaps	xmm2, xmm4
	movaps	xmm1, xmm4
	jmp	.L44
	.cfi_endproc
.LFE3407:
	.size	_Z16correlate_split4PKfPKS0_Pfi, .-_Z16correlate_split4PKfPKS0_Pfi
	.section	.rodata._ZNSt6vectorIfSaIfEEC2EmRKS0_.str1.8,"aMS",@progbits,1
	.align 8
.LC3:
	.string	"cannot create std::vector larger than max_size()"
	.section	.text._ZNSt6vectorIfSaIfEEC2EmRKS0_,"axG",@progbits,_ZNSt6vectorIfSaIfEEC5EmRKS0_,comdat
	.align 2
	.weak	_ZNSt6vectorIfSaIfEEC2EmRKS0_
	.type	_ZNSt6vectorIfSaIfEEC2EmRKS0_, @function
_ZNSt6vectorIfSaIfEEC2EmRKS0_:
.LFB3776:
	.cfi_startproc
	endbr64
	push	r12
	.cfi_def_cfa_offset 16
	.cfi_offset 12, -16
	push	rbp
	.cfi_def_cfa_offset 24
	.cfi_offset 6, -24
	push	rbx
	.cfi_def_cfa_offset 32
	.cfi_offset 3, -32
	mov	rax, rsi
	shr	rax, 61
	jne	.L65
	mov	rbx, rdi
	mov	rbp, rsi
	mov	QWORD PTR [rdi], 0
	mov	QWORD PTR 8[rdi], 0
	mov	QWORD PTR 16[rdi], 0
	test	rsi, rsi
	je	.L57
	lea	r12, 0[0+rsi*4]
	mov	rdi, r12
	call	_Znwm@PLT
	mov	QWORD PTR [rbx], rax
	mov	QWORD PTR 8[rbx], rax
	lea	rdx, [rax+r12]
	mov	QWORD PTR 16[rbx], rdx
	mov	DWORD PTR [rax], 0x00000000
	add	rax, 4
	cmp	rbp, 1
	je	.L60
	cmp	rdx, rax
	je	.L61
.L59:
	mov	DWORD PTR [rax], 0x00000000
	add	rax, 4
	cmp	rdx, rax
	jne	.L59
	jmp	.L58
.L65:
	lea	rdi, .LC3[rip]
	call	_ZSt20__throw_length_errorPKc@PLT
.L60:
	mov	rdx, rax
	jmp	.L58
.L61:
	mov	rdx, rax
	jmp	.L58
.L57:
	mov	QWORD PTR [rdi], 0
	mov	QWORD PTR 16[rdi], 0
	mov	edx, 0
.L58:
	mov	QWORD PTR 8[rbx], rdx
	pop	rbx
	.cfi_def_cfa_offset 24
	pop	rbp
	.cfi_def_cfa_offset 16
	pop	r12
	.cfi_def_cfa_offset 8
	ret
	.cfi_endproc
.LFE3776:
	.size	_ZNSt6vectorIfSaIfEEC2EmRKS0_, .-_ZNSt6vectorIfSaIfEEC2EmRKS0_
	.weak	_ZNSt6vectorIfSaIfEEC1EmRKS0_
	.set	_ZNSt6vectorIfSaIfEEC1EmRKS0_,_ZNSt6vectorIfSaIfEEC2EmRKS0_
	.section	.text._ZNSt6vectorIfSaIfEED2Ev,"axG",@progbits,_ZNSt6vectorIfSaIfEED5Ev,comdat
	.align 2
	.weak	_ZNSt6vectorIfSaIfEED2Ev
	.type	_ZNSt6vectorIfSaIfEED2Ev, @function
_ZNSt6vectorIfSaIfEED2Ev:
.LFB3779:
	.cfi_startproc
	endbr64
	mov	rax, QWORD PTR [rdi]
	test	rax, rax
	je	.L69
	sub	rsp, 8
	.cfi_def_cfa_offset 16
	mov	rsi, QWORD PTR 16[rdi]
	sub	rsi, rax
	mov	rdi, rax
	call	_ZdlPvm@PLT
	add	rsp, 8
	.cfi_def_cfa_offset 8
	ret
.L69:
	ret
	.cfi_endproc
.LFE3779:
	.size	_ZNSt6vectorIfSaIfEED2Ev, .-_ZNSt6vectorIfSaIfEED2Ev
	.weak	_ZNSt6vectorIfSaIfEED1Ev
	.set	_ZNSt6vectorIfSaIfEED1Ev,_ZNSt6vectorIfSaIfEED2Ev
	.section	.text._ZNSt6vectorIS_IfSaIfEESaIS1_EED2Ev,"axG",@progbits,_ZNSt6vectorIS_IfSaIfEESaIS1_EED5Ev,comdat
	.align 2
	.weak	_ZNSt6vectorIS_IfSaIfEESaIS1_EED2Ev
	.type	_ZNSt6vectorIS_IfSaIfEESaIS1_EED2Ev, @function
_ZNSt6vectorIS_IfSaIfEESaIS1_EED2Ev:
.LFB3791:
	.cfi_startproc
	endbr64
	push	r12
	.cfi_def_cfa_offset 16
	.cfi_offset 12, -16
	push	rbp
	.cfi_def_cfa_offset 24
	.cfi_offset 6, -24
	push	rbx
	.cfi_def_cfa_offset 32
	.cfi_offset 3, -32
	mov	r12, rdi
	mov	rbp, QWORD PTR 8[rdi]
	mov	rbx, QWORD PTR [rdi]
	cmp	rbp, rbx
	jne	.L75
.L73:
	mov	rdi, QWORD PTR [r12]
	test	rdi, rdi
	je	.L72
	mov	rsi, QWORD PTR 16[r12]
	sub	rsi, rdi
	call	_ZdlPvm@PLT
.L72:
	pop	rbx
	.cfi_remember_state
	.cfi_def_cfa_offset 24
	pop	rbp
	.cfi_def_cfa_offset 16
	pop	r12
	.cfi_def_cfa_offset 8
	ret
.L74:
	.cfi_restore_state
	add	rbx, 24
	cmp	rbp, rbx
	je	.L73
.L75:
	mov	rdi, QWORD PTR [rbx]
	test	rdi, rdi
	je	.L74
	mov	rsi, QWORD PTR 16[rbx]
	sub	rsi, rdi
	call	_ZdlPvm@PLT
	jmp	.L74
	.cfi_endproc
.LFE3791:
	.size	_ZNSt6vectorIS_IfSaIfEESaIS1_EED2Ev, .-_ZNSt6vectorIS_IfSaIfEESaIS1_EED2Ev
	.weak	_ZNSt6vectorIS_IfSaIfEESaIS1_EED1Ev
	.set	_ZNSt6vectorIS_IfSaIfEESaIS1_EED1Ev,_ZNSt6vectorIS_IfSaIfEESaIS1_EED2Ev
	.section	.text._ZNSt6vectorIPKfSaIS1_EED2Ev,"axG",@progbits,_ZNSt6vectorIPKfSaIS1_EED5Ev,comdat
	.align 2
	.weak	_ZNSt6vectorIPKfSaIS1_EED2Ev
	.type	_ZNSt6vectorIPKfSaIS1_EED2Ev, @function
_ZNSt6vectorIPKfSaIS1_EED2Ev:
.LFB3805:
	.cfi_startproc
	endbr64
	mov	rax, QWORD PTR [rdi]
	test	rax, rax
	je	.L82
	sub	rsp, 8
	.cfi_def_cfa_offset 16
	mov	rsi, QWORD PTR 16[rdi]
	sub	rsi, rax
	mov	rdi, rax
	call	_ZdlPvm@PLT
	add	rsp, 8
	.cfi_def_cfa_offset 8
	ret
.L82:
	ret
	.cfi_endproc
.LFE3805:
	.size	_ZNSt6vectorIPKfSaIS1_EED2Ev, .-_ZNSt6vectorIPKfSaIS1_EED2Ev
	.weak	_ZNSt6vectorIPKfSaIS1_EED1Ev
	.set	_ZNSt6vectorIPKfSaIS1_EED1Ev,_ZNSt6vectorIPKfSaIS1_EED2Ev
	.section	.text._ZSt18__do_uninit_fill_nIPSt6vectorIfSaIfEEmS2_ET_S4_T0_RKT1_,"axG",@progbits,_ZSt18__do_uninit_fill_nIPSt6vectorIfSaIfEEmS2_ET_S4_T0_RKT1_,comdat
	.weak	_ZSt18__do_uninit_fill_nIPSt6vectorIfSaIfEEmS2_ET_S4_T0_RKT1_
	.type	_ZSt18__do_uninit_fill_nIPSt6vectorIfSaIfEEmS2_ET_S4_T0_RKT1_, @function
_ZSt18__do_uninit_fill_nIPSt6vectorIfSaIfEEmS2_ET_S4_T0_RKT1_:
.LFB4348:
	.cfi_startproc
	.cfi_personality 0x9b,DW.ref.__gxx_personality_v0
	.cfi_lsda 0x1b,.LLSDA4348
	endbr64
	push	r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	push	r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	push	r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	push	r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	push	rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	push	rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	sub	rsp, 24
	.cfi_def_cfa_offset 80
	mov	QWORD PTR 8[rsp], rdi
	test	rsi, rsi
	je	.L96
	mov	r14, rsi
	mov	r13, rdx
	mov	rbx, rdi
	movabs	r15, 9223372036854775804
	jmp	.L91
.L102:
.LEHB0:
	call	_ZSt28__throw_bad_array_new_lengthv@PLT
.LEHE0:
.L98:
	endbr64
	mov	rdi, rax
	call	__cxa_begin_catch@PLT
.L93:
	cmp	QWORD PTR 8[rsp], rbx
	jne	.L94
.LEHB1:
	call	__cxa_rethrow@PLT
.LEHE1:
.L99:
	endbr64
	mov	rbx, rax
	call	__cxa_end_catch@PLT
	mov	rdi, rbx
.LEHB2:
	call	_Unwind_Resume@PLT
.LEHE2:
.L103:
	mov	rbp, rax
.L87:
	mov	QWORD PTR [rbx], rbp
	mov	QWORD PTR 8[rbx], rbp
	add	r12, rbp
	mov	QWORD PTR 16[rbx], r12
	mov	rsi, QWORD PTR 0[r13]
	mov	r12, QWORD PTR 8[r13]
	sub	r12, rsi
	cmp	r12, 4
	jle	.L89
	mov	rdx, r12
	mov	rdi, rbp
	call	memmove@PLT
.L90:
	add	rbp, r12
	mov	QWORD PTR 8[rbx], rbp
	add	rbx, 24
	sub	r14, 1
	je	.L85
.L91:
	mov	r12, QWORD PTR 8[r13]
	sub	r12, QWORD PTR 0[r13]
	mov	QWORD PTR [rbx], 0
	mov	QWORD PTR 8[rbx], 0
	mov	QWORD PTR 16[rbx], 0
	je	.L97
	cmp	r15, r12
	jb	.L102
	mov	rdi, r12
.LEHB3:
	call	_Znwm@PLT
.LEHE3:
	jmp	.L103
.L97:
	mov	ebp, 0
	jmp	.L87
.L89:
	jne	.L90
	movss	xmm0, DWORD PTR [rsi]
	movss	DWORD PTR 0[rbp], xmm0
	jmp	.L90
.L96:
	mov	rbx, QWORD PTR 8[rsp]
.L85:
	mov	rax, rbx
	add	rsp, 24
	.cfi_remember_state
	.cfi_def_cfa_offset 56
	pop	rbx
	.cfi_def_cfa_offset 48
	pop	rbp
	.cfi_def_cfa_offset 40
	pop	r12
	.cfi_def_cfa_offset 32
	pop	r13
	.cfi_def_cfa_offset 24
	pop	r14
	.cfi_def_cfa_offset 16
	pop	r15
	.cfi_def_cfa_offset 8
	ret
.L94:
	.cfi_restore_state
	mov	r15, QWORD PTR 8[rsp]
	mov	rdi, r15
	call	_ZNSt6vectorIfSaIfEED1Ev
	mov	rax, r15
	add	rax, 24
	mov	QWORD PTR 8[rsp], rax
	jmp	.L93
	.cfi_endproc
.LFE4348:
	.globl	__gxx_personality_v0
	.section	.gcc_except_table._ZSt18__do_uninit_fill_nIPSt6vectorIfSaIfEEmS2_ET_S4_T0_RKT1_,"aG",@progbits,_ZSt18__do_uninit_fill_nIPSt6vectorIfSaIfEEmS2_ET_S4_T0_RKT1_,comdat
	.align 4
.LLSDA4348:
	.byte	0xff
	.byte	0x9b
	.uleb128 .LLSDATT4348-.LLSDATTD4348
.LLSDATTD4348:
	.byte	0x1
	.uleb128 .LLSDACSE4348-.LLSDACSB4348
.LLSDACSB4348:
	.uleb128 .LEHB0-.LFB4348
	.uleb128 .LEHE0-.LEHB0
	.uleb128 .L98-.LFB4348
	.uleb128 0x1
	.uleb128 .LEHB1-.LFB4348
	.uleb128 .LEHE1-.LEHB1
	.uleb128 .L99-.LFB4348
	.uleb128 0
	.uleb128 .LEHB2-.LFB4348
	.uleb128 .LEHE2-.LEHB2
	.uleb128 0
	.uleb128 0
	.uleb128 .LEHB3-.LFB4348
	.uleb128 .LEHE3-.LEHB3
	.uleb128 .L98-.LFB4348
	.uleb128 0x1
.LLSDACSE4348:
	.byte	0x1
	.byte	0
	.align 4
	.long	0

.LLSDATT4348:
	.section	.text._ZSt18__do_uninit_fill_nIPSt6vectorIfSaIfEEmS2_ET_S4_T0_RKT1_,"axG",@progbits,_ZSt18__do_uninit_fill_nIPSt6vectorIfSaIfEEmS2_ET_S4_T0_RKT1_,comdat
	.size	_ZSt18__do_uninit_fill_nIPSt6vectorIfSaIfEEmS2_ET_S4_T0_RKT1_, .-_ZSt18__do_uninit_fill_nIPSt6vectorIfSaIfEEmS2_ET_S4_T0_RKT1_
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC4:
	.string	"PASS"
.LC5:
	.string	"FAIL"
.LC8:
	.string	"PIN_CPU"
.LC9:
	.string	"warning: could not pin to cpu"
.LC10:
	.string	"\n"
.LC12:
	.string	"Build: "
.LC13:
	.string	"optimized"
.LC14:
	.string	"   core clock after warm-up: "
.LC15:
	.string	" GHz (cpu"
.LC16:
	.string	", measured)\n"
.LC18:
	.string	"Correctness check:\n"
.LC20:
	.string	"  FAIL k="
.LC21:
	.string	"  "
.LC23:
	.string	"32 correlations over "
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align 8
.LC24:
	.string	"M samples  --  register pressure vs loop fission\n"
	.section	.rodata.str1.1
.LC25:
	.string	"split"
.LC26:
	.string	"accums"
.LC27:
	.string	"time"
.LC28:
	.string	"speedup"
.LC29:
	.string	"1 loop    (unfissioned)"
.LC30:
	.string	"2 loops"
.LC31:
	.string	"4 loops   (spills reach zero)"
.LC32:
	.string	"8 loops"
.LC33:
	.string	" ms"
.LC34:
	.string	"x"
	.section	.rodata.str1.8
	.align 8
.LC35:
	.ascii	"\nThe knee is at 8 accumulators per loop, where the spills r"
	.ascii	"each zero.\nCount them yourself -- the speedup tracks the st"
	.ascii	"ack traffic exactly:\n\n  g++ -O1 -masm=intel -S -o fission."
	.ascii	"s loop_fission.cpp\n  for f in fused split16 split8 split4; "
	.ascii	"do \\\n      echo -n \"$f \"; sed -n \"/^_Z.*correlate_$f/,/"
	.ascii	"\\.size/p\" fission.s \\\n      | grep -c '\\[rsp'; done\n\n"
	.ascii	"    32 accumulators -> 134 stack refs      8 accumulators ->"
	.ascii	"   0\n    16 accumulators ->  57 stack refs      4 accumulat"
	.ascii	"ors ->   0\n\nSixteen is not enough on this target: x86-64 h"
	.ascii	"as 16 architectural XMM\nregisters, and 16 accumulators plus"
	.ascii	" the reference pointers and loop\nstate still overflow them."
	.ascii	"  On an ISA with 32 FP registers it would\nfit.  The best sp"
	.ascii	"lit is an ISA property -- re-measure it per target.\n\nSplit"
	.ascii	"ting past the knee buys nothing: at 4 accumulators there are"
	.ascii	" no\nspills left to remove, and eight passes over the signal"
	.ascii	" cancel the rest.\n\nOne measurement trap worth knowing: the"
	.ascii	" reference vectors are 32\nseparate 40 MB allocations and ma"
	.ascii	"lloc hands them back with identical\nalignment, w"
	.string	"hich makes the streams congruent in the cache and in the\nDRAM banks.  Staggering the allocations by a cache line each is worth\na large fraction of the 1-loop time on its own, and has nothing to do\nwith fission.  The times above use the natural allocation.\n"
	.section	.rodata.str1.1
.LC36:
	.string	"\nCore clock at end of run: "
.LC37:
	.string	" GHz\n"
	.text
	.globl	main
	.type	main, @function
main:
.LFB3408:
	.cfi_startproc
	.cfi_personality 0x9b,DW.ref.__gxx_personality_v0
	.cfi_lsda 0x1b,.LLSDA3408
	endbr64
	push	r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	push	r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	push	r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	push	r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	push	rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	push	rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	sub	rsp, 360
	.cfi_def_cfa_offset 416
	mov	rax, QWORD PTR fs:40
	mov	QWORD PTR 344[rsp], rax
	xor	eax, eax
	lea	rdi, .LC8[rip]
	call	getenv@PLT
	test	rax, rax
	je	.L105
	mov	rdi, rax
	mov	edx, 10
	mov	esi, 0
	call	__isoc23_strtol@PLT
	mov	rdx, rax
	mov	DWORD PTR 16[rsp], eax
	lea	rsi, 208[rsp]
	mov	ecx, 32
	mov	eax, 0
	mov	rdi, rsi
	rep stosd
	movsx	rax, edx
	cmp	rax, 1023
	jbe	.L142
.L106:
	lea	rdx, 208[rsp]
	mov	esi, 128
	mov	edi, 0
	call	sched_setaffinity@PLT
	test	eax, eax
	jne	.L174
.L107:
	call	_ZNSt6chrono3_V212steady_clock3nowEv@PLT
	mov	QWORD PTR 24[rsp], rax
	mov	r15d, 0
	mov	r14d, 0
	mov	r13d, 0
	mov	r12d, 0
	mov	ebp, 0
	mov	ebx, 0
	mov	QWORD PTR 8[rsp], 0
	mov	QWORD PTR [rsp], 0
.L108:
	call	_ZNSt6chrono3_V212steady_clock3nowEv@PLT
	mov	rsi, QWORD PTR 24[rsp]
	sub	rax, rsi
	pxor	xmm0, xmm0
	cvtsi2sd	xmm0, rax
	divsd	xmm0, QWORD PTR .LC0[rip]
	movsd	xmm3, QWORD PTR .LC11[rip]
	comisd	xmm3, xmm0
	jbe	.L175
	mov	ecx, 200000
.L109:
	mov	rdx, QWORD PTR [rsp]
	mov	rax, QWORD PTR 8[rsp]
#APP
# 157 "loop_fission.cpp" 1
	addq $1,rdx
	addq $1,rax
	addq $1,rbx
	addq $1,rbp
	addq $1,r12
	addq $1,r13
	addq $1,r14
	addq $1,r15
# 0 "" 2
#NO_APP
	mov	QWORD PTR [rsp], rdx
	mov	QWORD PTR 8[rsp], rax
	sub	ecx, 1
	jne	.L109
	jmp	.L108
.L174:
	lea	rsi, .LC9[rip]
	lea	rdi, _ZSt4cerr[rip]
.LEHB4:
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	mov	rdi, rax
	mov	esi, DWORD PTR 16[rsp]
	call	_ZNSolsEi@PLT
	mov	rdi, rax
	lea	rsi, .LC10[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	jmp	.L107
.L175:
	call	_ZL14core_clock_ghzv
	movsd	QWORD PTR [rsp], xmm0
	lea	rsi, .LC12[rip]
	lea	rdi, _ZSt4cout[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	mov	rdi, rax
	lea	rsi, .LC13[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	mov	rdi, rax
	lea	rsi, .LC14[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	mov	rdi, rax
	mov	rax, QWORD PTR [rax]
	mov	rdx, rdi
	add	rdx, QWORD PTR -24[rax]
	mov	eax, DWORD PTR 24[rdx]
	and	eax, -261
	or	eax, 4
	mov	DWORD PTR 24[rdx], eax
	mov	rax, QWORD PTR [rdi]
	mov	rax, QWORD PTR -24[rax]
	mov	QWORD PTR 8[rdi+rax], 2
	movsd	xmm0, QWORD PTR [rsp]
	call	_ZNSo9_M_insertIdEERSoT_@PLT
	mov	rdi, rax
	lea	rsi, .LC15[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	mov	rdi, rax
	mov	esi, DWORD PTR 16[rsp]
	call	_ZNSolsEi@PLT
	mov	rdi, rax
	lea	rsi, .LC16[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	lea	rbx, 176[rsp]
	lea	rdi, 48[rsp]
	mov	rdx, rbx
	mov	esi, 10000000
	call	_ZNSt6vectorIfSaIfEEC1EmRKS0_
.LEHE4:
	lea	rdx, 144[rsp]
	mov	esi, 10000000
	mov	rdi, rbx
.LEHB5:
	call	_ZNSt6vectorIfSaIfEEC1EmRKS0_
.LEHE5:
	mov	QWORD PTR 88[rsp], 0
	mov	QWORD PTR 96[rsp], 0
	mov	edi, 768
.LEHB6:
	call	_Znwm@PLT
.LEHE6:
	mov	r14, rax
	mov	QWORD PTR 80[rsp], rax
	lea	rax, 768[rax]
	mov	QWORD PTR 96[rsp], rax
	mov	rdx, rbx
	mov	esi, 32
	mov	rdi, r14
.LEHB7:
	call	_ZSt18__do_uninit_fill_nIPSt6vectorIfSaIfEEmS2_ET_S4_T0_RKT1_
.LEHE7:
	mov	QWORD PTR 88[rsp], rax
	mov	rdi, rbx
	call	_ZNSt6vectorIfSaIfEED1Ev
	mov	rbp, QWORD PTR 48[rsp]
	mov	ebx, 0
.L110:
	pxor	xmm0, xmm0
	cvtsi2ss	xmm0, ebx
	mulss	xmm0, DWORD PTR .LC17[rip]
	call	sinf@PLT
	movss	DWORD PTR 0[rbp+rbx*4], xmm0
	add	rbx, 1
	cmp	rbx, 10000000
	jne	.L110
	mov	r13, r14
	mov	r12, r14
	mov	r15d, 1
.L113:
	mov	ebx, 0
	pxor	xmm2, xmm2
	cvtsi2ss	xmm2, r15d
	movss	DWORD PTR [rsp], xmm2
.L114:
	pxor	xmm0, xmm0
	cvtsi2ss	xmm0, ebx
	mulss	xmm0, DWORD PTR .LC17[rip]
	mulss	xmm0, DWORD PTR [rsp]
	call	sinf@PLT
	mov	rax, QWORD PTR [r12]
	movss	DWORD PTR [rax+rbx*4], xmm0
	add	rbx, 1
	cmp	rbx, 10000000
	jne	.L114
	add	r15d, 1
	add	r12, 24
	cmp	r15d, 33
	jne	.L113
	mov	QWORD PTR 120[rsp], 0
	mov	QWORD PTR 128[rsp], 0
	mov	edi, 256
.LEHB8:
	call	_Znwm@PLT
.LEHE8:
	jmp	.L176
.L159:
	endbr64
	mov	rbx, rax
	mov	esi, 768
	mov	rdi, r14
	call	_ZdlPvm@PLT
.L112:
	lea	rdi, 176[rsp]
	call	_ZNSt6vectorIfSaIfEED1Ev
.L133:
	lea	rdi, 48[rsp]
	call	_ZNSt6vectorIfSaIfEED1Ev
	mov	rax, QWORD PTR 344[rsp]
	sub	rax, QWORD PTR fs:40
	je	.L141
	call	__stack_chk_fail@PLT
.L176:
	mov	r12, rax
	mov	QWORD PTR 112[rsp], rax
	lea	rdx, 256[rax]
	mov	QWORD PTR 128[rsp], rdx
	mov	QWORD PTR [rax], 0
	lea	rax, 8[rax]
.L116:
	mov	QWORD PTR [rax], 0
	add	rax, 8
	cmp	rdx, rax
	jne	.L116
	mov	QWORD PTR 120[rsp], rdx
	mov	rax, r12
	add	r14, 768
.L117:
	mov	rdx, QWORD PTR 0[r13]
	mov	QWORD PTR [rax], rdx
	add	r13, 24
	add	rax, 8
	cmp	r14, r13
	jne	.L117
	lea	rdx, 176[rsp]
	lea	rdi, 144[rsp]
	mov	esi, 32
.LEHB9:
	call	_ZNSt6vectorIfSaIfEEC1EmRKS0_
.LEHE9:
	lea	rdx, 40[rsp]
	lea	rdi, 176[rsp]
	mov	esi, 32
.LEHB10:
	call	_ZNSt6vectorIfSaIfEEC1EmRKS0_
.LEHE10:
	mov	r14, QWORD PTR 144[rsp]
	mov	ecx, 10000000
	mov	rdx, r14
	mov	rsi, r12
	mov	rdi, rbp
	call	_Z15correlate_fusedPKfPKS0_Pfi
	mov	r13, QWORD PTR 176[rsp]
	mov	ecx, 10000000
	mov	rdx, r13
	mov	rsi, r12
	mov	rdi, rbp
	call	_Z16correlate_split8PKfPKS0_Pfi
	lea	rsi, .LC18[rip]
	lea	rdi, _ZSt4cout[rip]
.LEHB11:
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	mov	ebx, 0
	mov	BYTE PTR [rsp], 1
	lea	r15, _ZSt4cout[rip]
	jmp	.L120
.L178:
	mov	esi, ebx
	mov	rdi, r15
	call	_ZNSolsEi@PLT
	mov	rdi, rax
	mov	edx, 1
	lea	rsi, .LC10[rip]
	call	_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l@PLT
	mov	BYTE PTR [rsp], 0
.L118:
	add	rbx, 1
	cmp	rbx, 32
	je	.L177
.L120:
	movss	xmm0, DWORD PTR [r14+rbx*4]
	subss	xmm0, DWORD PTR 0[r13+rbx*4]
	andps	xmm0, XMMWORD PTR .LC19[rip]
	comiss	xmm0, DWORD PTR .LC17[rip]
	jbe	.L118
	mov	edx, 9
	lea	rsi, .LC20[rip]
	mov	rdi, r15
	call	_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l@PLT
	jmp	.L178
.L177:
	lea	rsi, .LC21[rip]
	lea	rdi, _ZSt4cout[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	mov	rdi, rax
	cmp	BYTE PTR [rsp], 0
	lea	rsi, .LC5[rip]
	lea	rax, .LC4[rip]
	cmovne	rsi, rax
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	mov	rdi, rax
	lea	rsi, .LC10[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	mov	ebx, 5
	mov	QWORD PTR [rsp], 0x000000000
	movsd	xmm4, QWORD PTR .LC7[rip]
	movsd	QWORD PTR 8[rsp], xmm4
.L123:
	call	_ZNSt6chrono3_V212system_clock3nowEv@PLT
	mov	r13, rax
	mov	r14, QWORD PTR 144[rsp]
	mov	ecx, 10000000
	mov	rdx, r14
	mov	rsi, r12
	mov	rdi, rbp
	call	_Z15correlate_fusedPKfPKS0_Pfi
	pxor	xmm0, xmm0
	cvtss2sd	xmm0, DWORD PTR [r14]
	addsd	xmm0, QWORD PTR [rsp]
	movsd	QWORD PTR [rsp], xmm0
	call	_ZNSt6chrono3_V212system_clock3nowEv@PLT
	sub	rax, r13
	pxor	xmm0, xmm0
	cvtsi2sd	xmm0, rax
	divsd	xmm0, QWORD PTR .LC22[rip]
	minsd	xmm0, QWORD PTR 8[rsp]
	movsd	QWORD PTR 8[rsp], xmm0
	sub	ebx, 1
	jne	.L123
	mov	r13d, 5
	mov	rbx, QWORD PTR .LC7[rip]
.L125:
	call	_ZNSt6chrono3_V212system_clock3nowEv@PLT
	mov	r14, rax
	mov	r15, QWORD PTR 176[rsp]
	mov	ecx, 10000000
	mov	rdx, r15
	mov	rsi, r12
	mov	rdi, rbp
	call	_Z17correlate_split16PKfPKS0_Pfi
	pxor	xmm0, xmm0
	cvtss2sd	xmm0, DWORD PTR [r15]
	addsd	xmm0, QWORD PTR [rsp]
	movsd	QWORD PTR [rsp], xmm0
	call	_ZNSt6chrono3_V212system_clock3nowEv@PLT
	sub	rax, r14
	pxor	xmm0, xmm0
	cvtsi2sd	xmm0, rax
	divsd	xmm0, QWORD PTR .LC22[rip]
	movq	xmm4, rbx
	minsd	xmm0, xmm4
	movq	rbx, xmm0
	sub	r13d, 1
	jne	.L125
	mov	r13d, 5
	movsd	xmm5, QWORD PTR .LC7[rip]
	movsd	QWORD PTR 24[rsp], xmm5
.L127:
	call	_ZNSt6chrono3_V212system_clock3nowEv@PLT
	mov	r14, rax
	mov	r15, QWORD PTR 176[rsp]
	mov	ecx, 10000000
	mov	rdx, r15
	mov	rsi, r12
	mov	rdi, rbp
	call	_Z16correlate_split8PKfPKS0_Pfi
	pxor	xmm0, xmm0
	cvtss2sd	xmm0, DWORD PTR [r15]
	addsd	xmm0, QWORD PTR [rsp]
	movsd	QWORD PTR [rsp], xmm0
	call	_ZNSt6chrono3_V212system_clock3nowEv@PLT
	sub	rax, r14
	pxor	xmm0, xmm0
	cvtsi2sd	xmm0, rax
	divsd	xmm0, QWORD PTR .LC22[rip]
	minsd	xmm0, QWORD PTR 24[rsp]
	movsd	QWORD PTR 24[rsp], xmm0
	sub	r13d, 1
	jne	.L127
	mov	r13d, 5
	movsd	xmm4, QWORD PTR .LC7[rip]
	movsd	QWORD PTR 16[rsp], xmm4
.L129:
	call	_ZNSt6chrono3_V212system_clock3nowEv@PLT
	mov	r14, rax
	mov	r15, QWORD PTR 176[rsp]
	mov	ecx, 10000000
	mov	rdx, r15
	mov	rsi, r12
	mov	rdi, rbp
	call	_Z16correlate_split4PKfPKS0_Pfi
	pxor	xmm0, xmm0
	cvtss2sd	xmm0, DWORD PTR [r15]
	addsd	xmm0, QWORD PTR [rsp]
	movsd	QWORD PTR [rsp], xmm0
	call	_ZNSt6chrono3_V212system_clock3nowEv@PLT
	sub	rax, r14
	pxor	xmm0, xmm0
	cvtsi2sd	xmm0, rax
	divsd	xmm0, QWORD PTR .LC22[rip]
	minsd	xmm0, QWORD PTR 16[rsp]
	movsd	QWORD PTR 16[rsp], xmm0
	sub	r13d, 1
	jne	.L129
	lea	rsi, .LC10[rip]
	lea	rdi, _ZSt4cout[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	mov	rbp, rax
	lea	rdi, 208[rsp]
	lea	rax, 224[rsp]
	mov	QWORD PTR 208[rsp], rax
	mov	edx, 45
	mov	esi, 68
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE12_M_constructEmc@PLT
.LEHE11:
	mov	rdx, QWORD PTR 216[rsp]
	mov	rsi, QWORD PTR 208[rsp]
	mov	rdi, rbp
.LEHB12:
	call	_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l@PLT
	mov	rdi, rax
	lea	rsi, .LC10[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
.LEHE12:
	lea	rdi, 208[rsp]
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv@PLT
	lea	rsi, .LC23[rip]
	lea	rdi, _ZSt4cout[rip]
.LEHB13:
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	mov	rdi, rax
	mov	esi, 10
	call	_ZNSolsEi@PLT
	mov	rdi, rax
	lea	rsi, .LC24[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	lea	rdi, 208[rsp]
	lea	rax, 224[rsp]
	mov	QWORD PTR 208[rsp], rax
	mov	edx, 45
	mov	esi, 68
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE12_M_constructEmc@PLT
.LEHE13:
	mov	rdx, QWORD PTR 216[rsp]
	mov	rsi, QWORD PTR 208[rsp]
	lea	rdi, _ZSt4cout[rip]
.LEHB14:
	call	_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l@PLT
	mov	rdi, rax
	lea	rsi, .LC10[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
.LEHE14:
	lea	rdi, 208[rsp]
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv@PLT
	lea	rdi, _ZSt4cout[rip]
	mov	rdx, QWORD PTR _ZSt4cout[rip]
	mov	rcx, rdi
	add	rcx, QWORD PTR -24[rdx]
	mov	eax, DWORD PTR 24[rcx]
	and	al, 79
	or	eax, 32
	mov	DWORD PTR 24[rcx], eax
	mov	rax, QWORD PTR -24[rdx]
	mov	QWORD PTR 16[rdi+rax], 34
	lea	rsi, .LC25[rip]
.LEHB15:
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	mov	rdi, rax
	mov	rax, QWORD PTR [rax]
	mov	rdx, rdi
	add	rdx, QWORD PTR -24[rax]
	mov	eax, DWORD PTR 24[rdx]
	and	al, 79
	or	al, -128
	mov	DWORD PTR 24[rdx], eax
	mov	rax, QWORD PTR [rdi]
	mov	rax, QWORD PTR -24[rax]
	mov	QWORD PTR 16[rdi+rax], 12
	lea	rsi, .LC26[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	mov	rdi, rax
	mov	rax, QWORD PTR [rax]
	mov	rdx, rdi
	add	rdx, QWORD PTR -24[rax]
	mov	eax, DWORD PTR 24[rdx]
	and	al, 79
	or	al, -128
	mov	DWORD PTR 24[rdx], eax
	mov	rax, QWORD PTR [rdi]
	mov	rax, QWORD PTR -24[rax]
	mov	QWORD PTR 16[rdi+rax], 11
	lea	rsi, .LC27[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	mov	rdi, rax
	mov	rax, QWORD PTR [rax]
	mov	rdx, rdi
	add	rdx, QWORD PTR -24[rax]
	mov	eax, DWORD PTR 24[rdx]
	and	al, 79
	or	al, -128
	mov	DWORD PTR 24[rdx], eax
	mov	rax, QWORD PTR [rdi]
	mov	rax, QWORD PTR -24[rax]
	mov	QWORD PTR 16[rdi+rax], 10
	lea	rsi, .LC28[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	mov	rdi, rax
	lea	rsi, .LC10[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	lea	rdi, 208[rsp]
	lea	rax, 224[rsp]
	mov	QWORD PTR 208[rsp], rax
	mov	edx, 45
	mov	esi, 68
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE12_M_constructEmc@PLT
.LEHE15:
	mov	rdx, QWORD PTR 216[rsp]
	mov	rsi, QWORD PTR 208[rsp]
	lea	rdi, _ZSt4cout[rip]
.LEHB16:
	call	_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l@PLT
	mov	rdi, rax
	lea	rsi, .LC10[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
.LEHE16:
	lea	rbp, 208[rsp]
	mov	rdi, rbp
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv@PLT
	pxor	xmm0, xmm0
	movups	XMMWORD PTR 220[rsp], xmm0
	movups	XMMWORD PTR 236[rsp], xmm0
	movups	XMMWORD PTR 268[rsp], xmm0
	movups	XMMWORD PTR 280[rsp], xmm0
	lea	rax, .LC29[rip]
	mov	QWORD PTR 208[rsp], rax
	mov	DWORD PTR 216[rsp], 32
	lea	rax, .LC30[rip]
	mov	QWORD PTR 232[rsp], rax
	mov	DWORD PTR 240[rsp], 16
	lea	rax, .LC31[rip]
	mov	QWORD PTR 256[rsp], rax
	mov	DWORD PTR 264[rsp], 8
	lea	rax, .LC32[rip]
	mov	QWORD PTR 280[rsp], rax
	mov	DWORD PTR 288[rsp], 4
	movsd	xmm7, QWORD PTR 8[rsp]
	movsd	QWORD PTR 224[rsp], xmm7
	mov	QWORD PTR 248[rsp], rbx
	movsd	xmm3, QWORD PTR 24[rsp]
	movsd	QWORD PTR 272[rsp], xmm3
	movsd	xmm4, QWORD PTR 16[rsp]
	movsd	QWORD PTR 296[rsp], xmm4
	lea	r14, 304[rsp]
	lea	rbx, _ZSt4cout[rip]
	lea	r15, .LC10[rip]
	jmp	.L132
.L180:
	mov	rdi, rbx
	add	rdi, QWORD PTR -24[rdx]
	mov	esi, DWORD PTR 32[rdi]
	or	esi, 1
.LEHB17:
	call	_ZNSt9basic_iosIcSt11char_traitsIcEE5clearESt12_Ios_Iostate@PLT
.L131:
	mov	rdx, QWORD PTR [rbx]
	mov	rcx, rbx
	add	rcx, QWORD PTR -24[rdx]
	mov	eax, DWORD PTR 24[rcx]
	and	al, 79
	or	al, -128
	mov	DWORD PTR 24[rcx], eax
	mov	rax, QWORD PTR -24[rdx]
	mov	QWORD PTR 16[rbx+rax], 12
	mov	esi, DWORD PTR 8[r13]
	mov	rdi, rbx
	call	_ZNSolsEi@PLT
	mov	rdi, rax
	mov	rax, QWORD PTR [rax]
	mov	rdx, rdi
	add	rdx, QWORD PTR -24[rax]
	mov	eax, DWORD PTR 24[rdx]
	and	al, 79
	or	al, -128
	mov	DWORD PTR 24[rdx], eax
	mov	rax, QWORD PTR [rdi]
	mov	rax, QWORD PTR -24[rax]
	mov	QWORD PTR 16[rdi+rax], 8
	mov	rax, QWORD PTR [rdi]
	mov	rdx, rdi
	add	rdx, QWORD PTR -24[rax]
	mov	eax, DWORD PTR 24[rdx]
	and	eax, -261
	or	eax, 4
	mov	DWORD PTR 24[rdx], eax
	mov	rax, QWORD PTR [rdi]
	mov	rax, QWORD PTR -24[rax]
	mov	QWORD PTR 8[rdi+rax], 1
	mov	r13, QWORD PTR 16[r13]
	movq	xmm0, r13
	call	_ZNSo9_M_insertIdEERSoT_@PLT
	mov	r12, rax
	mov	edx, 3
	lea	rsi, .LC33[rip]
	mov	rdi, rax
	call	_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l@PLT
	mov	rax, QWORD PTR [r12]
	mov	rdx, r12
	add	rdx, QWORD PTR -24[rax]
	mov	eax, DWORD PTR 24[rdx]
	and	al, 79
	or	al, -128
	mov	DWORD PTR 24[rdx], eax
	mov	rax, QWORD PTR [r12]
	mov	rax, QWORD PTR -24[rax]
	mov	QWORD PTR 16[r12+rax], 9
	mov	rax, QWORD PTR [r12]
	mov	rdx, r12
	add	rdx, QWORD PTR -24[rax]
	mov	eax, DWORD PTR 24[rdx]
	and	eax, -261
	or	eax, 4
	mov	DWORD PTR 24[rdx], eax
	mov	rax, QWORD PTR [r12]
	mov	rax, QWORD PTR -24[rax]
	mov	QWORD PTR 8[r12+rax], 2
	movsd	xmm0, QWORD PTR 8[rsp]
	movq	xmm5, r13
	divsd	xmm0, xmm5
	mov	rdi, r12
	call	_ZNSo9_M_insertIdEERSoT_@PLT
	mov	r12, rax
	mov	edx, 1
	lea	rsi, .LC34[rip]
	mov	rdi, rax
	call	_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l@PLT
	mov	edx, 1
	mov	rsi, r15
	mov	rdi, r12
	call	_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l@PLT
	add	rbp, 24
	cmp	r14, rbp
	je	.L179
.L132:
	mov	rdx, QWORD PTR [rbx]
	mov	rcx, rbx
	add	rcx, QWORD PTR -24[rdx]
	mov	eax, DWORD PTR 24[rcx]
	and	al, 79
	or	eax, 32
	mov	DWORD PTR 24[rcx], eax
	mov	rax, QWORD PTR -24[rdx]
	mov	QWORD PTR 16[rbx+rax], 34
	mov	r13, rbp
	mov	r12, QWORD PTR 0[rbp]
	test	r12, r12
	je	.L180
	mov	rdi, r12
	call	strlen@PLT
	mov	rdx, rax
	mov	rsi, r12
	mov	rdi, rbx
	call	_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l@PLT
	jmp	.L131
.L179:
	lea	rsi, .LC35[rip]
	lea	rdi, _ZSt4cout[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	lea	rsi, .LC36[rip]
	lea	rdi, _ZSt4cout[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	mov	rbx, rax
	mov	rax, QWORD PTR [rax]
	mov	rdx, rbx
	add	rdx, QWORD PTR -24[rax]
	mov	eax, DWORD PTR 24[rdx]
	and	eax, -261
	or	eax, 4
	mov	DWORD PTR 24[rdx], eax
	mov	rax, QWORD PTR [rbx]
	mov	rax, QWORD PTR -24[rax]
	mov	QWORD PTR 8[rbx+rax], 2
	call	_ZL14core_clock_ghzv
	mov	rdi, rbx
	call	_ZNSo9_M_insertIdEERSoT_@PLT
	mov	rdi, rax
	lea	rsi, .LC37[rip]
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
.LEHE17:
	movsd	xmm6, QWORD PTR [rsp]
	movsd	QWORD PTR 40[rsp], xmm6
	movsd	xmm0, QWORD PTR 40[rsp]
	lea	rdi, 176[rsp]
	call	_ZNSt6vectorIfSaIfEED1Ev
	lea	rdi, 144[rsp]
	call	_ZNSt6vectorIfSaIfEED1Ev
	lea	rdi, 112[rsp]
	call	_ZNSt6vectorIPKfSaIS1_EED1Ev
	lea	rdi, 80[rsp]
	call	_ZNSt6vectorIS_IfSaIfEESaIS1_EED1Ev
	lea	rdi, 48[rsp]
	call	_ZNSt6vectorIfSaIfEED1Ev
	mov	rax, QWORD PTR 344[rsp]
	sub	rax, QWORD PTR fs:40
	jne	.L181
	mov	eax, 0
	add	rsp, 360
	.cfi_remember_state
	.cfi_def_cfa_offset 56
	pop	rbx
	.cfi_def_cfa_offset 48
	pop	rbp
	.cfi_def_cfa_offset 40
	pop	r12
	.cfi_def_cfa_offset 32
	pop	r13
	.cfi_def_cfa_offset 24
	pop	r14
	.cfi_def_cfa_offset 16
	pop	r15
	.cfi_def_cfa_offset 8
	ret
.L150:
	.cfi_restore_state
	endbr64
	mov	rbx, rax
	jmp	.L112
.L156:
	endbr64
	mov	rbx, rax
	lea	rdi, 208[rsp]
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv@PLT
.L135:
	lea	rdi, 176[rsp]
	call	_ZNSt6vectorIfSaIfEED1Ev
.L138:
	lea	rdi, 144[rsp]
	call	_ZNSt6vectorIfSaIfEED1Ev
.L139:
	lea	rdi, 112[rsp]
	call	_ZNSt6vectorIPKfSaIS1_EED1Ev
.L140:
	lea	rdi, 80[rsp]
	call	_ZNSt6vectorIS_IfSaIfEESaIS1_EED1Ev
	jmp	.L133
.L157:
	endbr64
	mov	rbx, rax
	lea	rdi, 208[rsp]
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv@PLT
	jmp	.L135
.L158:
	endbr64
	mov	rbx, rax
	lea	rdi, 208[rsp]
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv@PLT
	jmp	.L135
.L155:
	endbr64
	mov	rbx, rax
	jmp	.L135
.L154:
	endbr64
	mov	rbx, rax
	jmp	.L138
.L153:
	endbr64
	mov	rbx, rax
	jmp	.L139
.L152:
	endbr64
	mov	rbx, rax
	jmp	.L140
.L151:
	endbr64
	mov	rbx, rax
	jmp	.L133
.L141:
	mov	rdi, rbx
.LEHB18:
	call	_Unwind_Resume@PLT
.LEHE18:
.L105:
	lea	rdx, 208[rsp]
	mov	ecx, 32
	mov	eax, 0
	mov	rdi, rdx
	rep stosd
	mov	eax, 0
	mov	DWORD PTR 16[rsp], 0
.L142:
	mov	rsi, rax
	shr	rsi, 6
	mov	edx, 1
	mov	ecx, eax
	sal	rdx, cl
	or	QWORD PTR 208[rsp+rsi*8], rdx
	jmp	.L106
.L181:
	call	__stack_chk_fail@PLT
	.cfi_endproc
.LFE3408:
	.section	.gcc_except_table,"a",@progbits
.LLSDA3408:
	.byte	0xff
	.byte	0xff
	.byte	0x1
	.uleb128 .LLSDACSE3408-.LLSDACSB3408
.LLSDACSB3408:
	.uleb128 .LEHB4-.LFB3408
	.uleb128 .LEHE4-.LEHB4
	.uleb128 0
	.uleb128 0
	.uleb128 .LEHB5-.LFB3408
	.uleb128 .LEHE5-.LEHB5
	.uleb128 .L151-.LFB3408
	.uleb128 0
	.uleb128 .LEHB6-.LFB3408
	.uleb128 .LEHE6-.LEHB6
	.uleb128 .L150-.LFB3408
	.uleb128 0
	.uleb128 .LEHB7-.LFB3408
	.uleb128 .LEHE7-.LEHB7
	.uleb128 .L159-.LFB3408
	.uleb128 0
	.uleb128 .LEHB8-.LFB3408
	.uleb128 .LEHE8-.LEHB8
	.uleb128 .L152-.LFB3408
	.uleb128 0
	.uleb128 .LEHB9-.LFB3408
	.uleb128 .LEHE9-.LEHB9
	.uleb128 .L153-.LFB3408
	.uleb128 0
	.uleb128 .LEHB10-.LFB3408
	.uleb128 .LEHE10-.LEHB10
	.uleb128 .L154-.LFB3408
	.uleb128 0
	.uleb128 .LEHB11-.LFB3408
	.uleb128 .LEHE11-.LEHB11
	.uleb128 .L155-.LFB3408
	.uleb128 0
	.uleb128 .LEHB12-.LFB3408
	.uleb128 .LEHE12-.LEHB12
	.uleb128 .L156-.LFB3408
	.uleb128 0
	.uleb128 .LEHB13-.LFB3408
	.uleb128 .LEHE13-.LEHB13
	.uleb128 .L155-.LFB3408
	.uleb128 0
	.uleb128 .LEHB14-.LFB3408
	.uleb128 .LEHE14-.LEHB14
	.uleb128 .L157-.LFB3408
	.uleb128 0
	.uleb128 .LEHB15-.LFB3408
	.uleb128 .LEHE15-.LEHB15
	.uleb128 .L155-.LFB3408
	.uleb128 0
	.uleb128 .LEHB16-.LFB3408
	.uleb128 .LEHE16-.LEHB16
	.uleb128 .L158-.LFB3408
	.uleb128 0
	.uleb128 .LEHB17-.LFB3408
	.uleb128 .LEHE17-.LEHB17
	.uleb128 .L155-.LFB3408
	.uleb128 0
	.uleb128 .LEHB18-.LFB3408
	.uleb128 .LEHE18-.LEHB18
	.uleb128 0
	.uleb128 0
.LLSDACSE3408:
	.text
	.size	main, .-main
	.section	.rodata.cst8,"aM",@progbits,8
	.align 8
.LC0:
	.long	0
	.long	1104006501
	.align 8
.LC1:
	.long	0
	.long	1100470148
	.align 8
.LC7:
	.long	-2013235812
	.long	2117592124
	.align 8
.LC11:
	.long	-1717986918
	.long	1072273817
	.section	.rodata.cst4,"aM",@progbits,4
	.align 4
.LC17:
	.long	981668463
	.section	.rodata.cst16,"aM",@progbits,16
	.align 16
.LC19:
	.long	2147483647
	.long	0
	.long	0
	.long	0
	.section	.rodata.cst8
	.align 8
.LC22:
	.long	0
	.long	1093567616
	.hidden	DW.ref.__gxx_personality_v0
	.weak	DW.ref.__gxx_personality_v0
	.section	.data.rel.local.DW.ref.__gxx_personality_v0,"awG",@progbits,DW.ref.__gxx_personality_v0,comdat
	.align 8
	.type	DW.ref.__gxx_personality_v0, @object
	.size	DW.ref.__gxx_personality_v0, 8
DW.ref.__gxx_personality_v0:
	.quad	__gxx_personality_v0
	.ident	"GCC: (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0"
	.section	.note.GNU-stack,"",@progbits
	.section	.note.gnu.property,"a"
	.align 8
	.long	1f - 0f
	.long	4f - 1f
	.long	5
0:
	.string	"GNU"
1:
	.align 8
	.long	0xc0000002
	.long	3f - 2f
2:
	.long	0x3
3:
	.align 8
4:
