	.file	"loop_fission.cpp"
	.text
#APP
	.globl _ZSt21ios_base_library_initv
#NO_APP
	.type	_ZL14core_clock_ghzv, @function
_ZL14core_clock_ghzv:
.LFB3397:
	.cfi_startproc
	pushq	%rbx
	.cfi_def_cfa_offset 16
	.cfi_offset 3, -16
	call	_ZNSt6chrono3_V212steady_clock3nowEv@PLT
	movq	%rax, %rbx
	movl	$12500000, %edx
	movl	$0, %eax
.L2:
#APP
# 136 "loop_fission.cpp" 1
	addq $1,%rax
	addq $1,%rax
	addq $1,%rax
	addq $1,%rax
	addq $1,%rax
	addq $1,%rax
	addq $1,%rax
	addq $1,%rax
# 0 "" 2
#NO_APP
	subq	$1, %rdx
	jne	.L2
	call	_ZNSt6chrono3_V212steady_clock3nowEv@PLT
	subq	%rbx, %rax
	pxor	%xmm1, %xmm1
	cvtsi2sdq	%rax, %xmm1
	movsd	.LC0(%rip), %xmm2
	divsd	%xmm2, %xmm1
	movsd	.LC1(%rip), %xmm0
	divsd	%xmm1, %xmm0
	divsd	%xmm2, %xmm0
	popq	%rbx
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
	pushq	%r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	pushq	%r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	pushq	%r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	pushq	%r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	pushq	%rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	addq	$-128, %rsp
	.cfi_def_cfa_offset 184
	testl	%ecx, %ecx
	jle	.L8
	movq	%rsi, %rax
	movq	(%rsi), %rbx
	movq	%rbx, -48(%rsp)
	movq	8(%rsi), %rbx
	movq	%rbx, -40(%rsp)
	movq	16(%rsi), %rsi
	movq	%rsi, -32(%rsp)
	movq	24(%rax), %rbx
	movq	%rbx, -24(%rsp)
	movq	32(%rax), %rsi
	movq	%rsi, -16(%rsp)
	movq	40(%rax), %rbx
	movq	%rbx, -8(%rsp)
	movq	48(%rax), %rsi
	movq	%rsi, (%rsp)
	movq	56(%rax), %rbx
	movq	%rbx, 8(%rsp)
	movq	64(%rax), %rsi
	movq	%rsi, 16(%rsp)
	movq	72(%rax), %rbx
	movq	%rbx, 24(%rsp)
	movq	80(%rax), %rsi
	movq	%rsi, 32(%rsp)
	movq	88(%rax), %rbx
	movq	%rbx, 40(%rsp)
	movq	96(%rax), %rsi
	movq	%rsi, 48(%rsp)
	movq	104(%rax), %rbx
	movq	%rbx, 56(%rsp)
	movq	112(%rax), %rsi
	movq	%rsi, 64(%rsp)
	movq	120(%rax), %rbx
	movq	%rbx, 72(%rsp)
	movq	128(%rax), %rsi
	movq	%rsi, 80(%rsp)
	movq	136(%rax), %r14
	movq	144(%rax), %r13
	movq	152(%rax), %rbx
	movq	%rbx, 88(%rsp)
	movq	160(%rax), %rsi
	movq	%rsi, 96(%rsp)
	movq	168(%rax), %rbx
	movq	%rbx, 104(%rsp)
	movq	176(%rax), %rsi
	movq	%rsi, 112(%rsp)
	movq	184(%rax), %r12
	movq	192(%rax), %rbp
	movq	200(%rax), %rbx
	movq	208(%rax), %r11
	movq	216(%rax), %r10
	movq	224(%rax), %r9
	movq	232(%rax), %r8
	movq	240(%rax), %rsi
	movq	248(%rax), %r15
	movslq	%ecx, %rcx
	salq	$2, %rcx
	movl	$0, %eax
	pxor	%xmm1, %xmm1
	movaps	%xmm1, %xmm2
	movaps	%xmm1, %xmm3
	movaps	%xmm1, %xmm4
	movaps	%xmm1, %xmm5
	movaps	%xmm1, %xmm6
	movaps	%xmm1, %xmm7
	movaps	%xmm1, %xmm8
	movaps	%xmm1, %xmm9
	movss	%xmm1, -52(%rsp)
	movaps	%xmm1, %xmm10
	movss	%xmm1, -56(%rsp)
	movss	%xmm1, -60(%rsp)
	movss	%xmm1, -64(%rsp)
	movss	%xmm1, -68(%rsp)
	movss	%xmm1, -72(%rsp)
	movss	%xmm1, -76(%rsp)
	movss	%xmm1, -80(%rsp)
	movss	%xmm1, -84(%rsp)
	movss	%xmm1, -88(%rsp)
	movss	%xmm1, -92(%rsp)
	movss	%xmm1, -96(%rsp)
	movss	%xmm1, -100(%rsp)
	movss	%xmm1, -104(%rsp)
	movss	%xmm1, -108(%rsp)
	movss	%xmm1, -112(%rsp)
	movss	%xmm1, -116(%rsp)
	movss	%xmm1, -120(%rsp)
	movaps	%xmm1, %xmm11
	movaps	%xmm1, %xmm12
	movaps	%xmm1, %xmm13
	movaps	%xmm1, %xmm14
	movq	%rdx, 120(%rsp)
.L7:
	movss	(%rdi,%rax), %xmm0
	movq	-48(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	%xmm15, %xmm14
	movq	-40(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	%xmm15, %xmm13
	movq	-32(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	%xmm15, %xmm12
	movq	-24(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	%xmm15, %xmm11
	movq	-16(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	-120(%rsp), %xmm15
	movss	%xmm15, -120(%rsp)
	movq	-8(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	-116(%rsp), %xmm15
	movss	%xmm15, -116(%rsp)
	movq	(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	-112(%rsp), %xmm15
	movss	%xmm15, -112(%rsp)
	movq	8(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	-108(%rsp), %xmm15
	movss	%xmm15, -108(%rsp)
	movq	16(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	-104(%rsp), %xmm15
	movss	%xmm15, -104(%rsp)
	movq	24(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	-100(%rsp), %xmm15
	movss	%xmm15, -100(%rsp)
	movq	32(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	-96(%rsp), %xmm15
	movss	%xmm15, -96(%rsp)
	movq	40(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	-92(%rsp), %xmm15
	movss	%xmm15, -92(%rsp)
	movq	48(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	-88(%rsp), %xmm15
	movss	%xmm15, -88(%rsp)
	movq	56(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	-84(%rsp), %xmm15
	movss	%xmm15, -84(%rsp)
	movq	64(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	-80(%rsp), %xmm15
	movss	%xmm15, -80(%rsp)
	movq	72(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	-76(%rsp), %xmm15
	movss	%xmm15, -76(%rsp)
	movq	80(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	-72(%rsp), %xmm15
	movss	%xmm15, -72(%rsp)
	movaps	%xmm0, %xmm15
	mulss	(%r14,%rax), %xmm15
	addss	-68(%rsp), %xmm15
	movss	%xmm15, -68(%rsp)
	movaps	%xmm0, %xmm15
	mulss	0(%r13,%rax), %xmm15
	addss	-64(%rsp), %xmm15
	movss	%xmm15, -64(%rsp)
	movq	88(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	-60(%rsp), %xmm15
	movss	%xmm15, -60(%rsp)
	movq	96(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	-56(%rsp), %xmm15
	movss	%xmm15, -56(%rsp)
	movq	104(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	%xmm15, %xmm10
	movaps	%xmm0, %xmm15
	movq	112(%rsp), %rdx
	mulss	(%rdx,%rax), %xmm15
	addss	-52(%rsp), %xmm15
	movss	%xmm15, -52(%rsp)
	movaps	%xmm0, %xmm15
	mulss	(%r12,%rax), %xmm15
	addss	%xmm15, %xmm9
	movaps	%xmm0, %xmm15
	mulss	0(%rbp,%rax), %xmm15
	addss	%xmm15, %xmm8
	movaps	%xmm0, %xmm15
	mulss	(%rbx,%rax), %xmm15
	addss	%xmm15, %xmm7
	movaps	%xmm0, %xmm15
	mulss	(%r11,%rax), %xmm15
	addss	%xmm15, %xmm6
	movaps	%xmm0, %xmm15
	mulss	(%r10,%rax), %xmm15
	addss	%xmm15, %xmm5
	movaps	%xmm0, %xmm15
	mulss	(%r9,%rax), %xmm15
	addss	%xmm15, %xmm4
	movaps	%xmm0, %xmm15
	mulss	(%r8,%rax), %xmm15
	addss	%xmm15, %xmm3
	movaps	%xmm0, %xmm15
	mulss	(%rsi,%rax), %xmm15
	addss	%xmm15, %xmm2
	mulss	(%r15,%rax), %xmm0
	addss	%xmm0, %xmm1
	addq	$4, %rax
	cmpq	%rcx, %rax
	jne	.L7
	movq	120(%rsp), %rdx
.L6:
	movss	%xmm14, (%rdx)
	movss	%xmm13, 4(%rdx)
	movss	%xmm12, 8(%rdx)
	movss	%xmm11, 12(%rdx)
	movss	-120(%rsp), %xmm0
	movss	%xmm0, 16(%rdx)
	movss	-116(%rsp), %xmm0
	movss	%xmm0, 20(%rdx)
	movss	-112(%rsp), %xmm0
	movss	%xmm0, 24(%rdx)
	movss	-108(%rsp), %xmm0
	movss	%xmm0, 28(%rdx)
	movss	-104(%rsp), %xmm0
	movss	%xmm0, 32(%rdx)
	movss	-100(%rsp), %xmm0
	movss	%xmm0, 36(%rdx)
	movss	-96(%rsp), %xmm0
	movss	%xmm0, 40(%rdx)
	movss	-92(%rsp), %xmm0
	movss	%xmm0, 44(%rdx)
	movss	-88(%rsp), %xmm0
	movss	%xmm0, 48(%rdx)
	movss	-84(%rsp), %xmm0
	movss	%xmm0, 52(%rdx)
	movss	-80(%rsp), %xmm0
	movss	%xmm0, 56(%rdx)
	movss	-76(%rsp), %xmm0
	movss	%xmm0, 60(%rdx)
	movss	-72(%rsp), %xmm0
	movss	%xmm0, 64(%rdx)
	movss	-68(%rsp), %xmm0
	movss	%xmm0, 68(%rdx)
	movss	-64(%rsp), %xmm0
	movss	%xmm0, 72(%rdx)
	movss	-60(%rsp), %xmm0
	movss	%xmm0, 76(%rdx)
	movss	-56(%rsp), %xmm0
	movss	%xmm0, 80(%rdx)
	movss	%xmm10, 84(%rdx)
	movss	-52(%rsp), %xmm0
	movss	%xmm0, 88(%rdx)
	movss	%xmm9, 92(%rdx)
	movss	%xmm8, 96(%rdx)
	movss	%xmm7, 100(%rdx)
	movss	%xmm6, 104(%rdx)
	movss	%xmm5, 108(%rdx)
	movss	%xmm4, 112(%rdx)
	movss	%xmm3, 116(%rdx)
	movss	%xmm2, 120(%rdx)
	movss	%xmm1, 124(%rdx)
	subq	$-128, %rsp
	.cfi_remember_state
	.cfi_def_cfa_offset 56
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%rbp
	.cfi_def_cfa_offset 40
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r13
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	ret
.L8:
	.cfi_restore_state
	pxor	%xmm1, %xmm1
	movaps	%xmm1, %xmm2
	movaps	%xmm1, %xmm3
	movaps	%xmm1, %xmm4
	movaps	%xmm1, %xmm5
	movaps	%xmm1, %xmm6
	movaps	%xmm1, %xmm7
	movaps	%xmm1, %xmm8
	movaps	%xmm1, %xmm9
	movss	%xmm1, -52(%rsp)
	movaps	%xmm1, %xmm10
	movss	%xmm1, -56(%rsp)
	movss	%xmm1, -60(%rsp)
	movss	%xmm1, -64(%rsp)
	movss	%xmm1, -68(%rsp)
	movss	%xmm1, -72(%rsp)
	movss	%xmm1, -76(%rsp)
	movss	%xmm1, -80(%rsp)
	movss	%xmm1, -84(%rsp)
	movss	%xmm1, -88(%rsp)
	movss	%xmm1, -92(%rsp)
	movss	%xmm1, -96(%rsp)
	movss	%xmm1, -100(%rsp)
	movss	%xmm1, -104(%rsp)
	movss	%xmm1, -108(%rsp)
	movss	%xmm1, -112(%rsp)
	movss	%xmm1, -116(%rsp)
	movss	%xmm1, -120(%rsp)
	movaps	%xmm1, %xmm11
	movaps	%xmm1, %xmm12
	movaps	%xmm1, %xmm13
	movaps	%xmm1, %xmm14
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
	pushq	%r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	pushq	%r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	pushq	%r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	pushq	%r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	pushq	%rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	movq	%rdi, -32(%rsp)
	movl	%ecx, -24(%rsp)
	testl	%ecx, %ecx
	jle	.L16
	movl	%ecx, %eax
	movq	(%rsi), %r15
	movq	8(%rsi), %r14
	movq	16(%rsi), %r13
	movq	24(%rsi), %r12
	movq	32(%rsi), %rbp
	movq	40(%rsi), %rbx
	movq	48(%rsi), %r11
	movq	56(%rsi), %r10
	movq	64(%rsi), %r9
	movq	72(%rsi), %r8
	movq	80(%rsi), %rcx
	movq	88(%rsi), %rdi
	movq	%rdi, -72(%rsp)
	movq	96(%rsi), %rdi
	movq	%rdi, -64(%rsp)
	movq	104(%rsi), %rdi
	movq	%rdi, -56(%rsp)
	movq	112(%rsi), %rdi
	movq	%rdi, -48(%rsp)
	movq	120(%rsi), %rdi
	movq	%rdi, -40(%rsp)
	cltq
	salq	$2, %rax
	movq	%rax, -16(%rsp)
	movl	$0, %eax
	movl	$0x00000000, -76(%rsp)
	movl	$0x00000000, -80(%rsp)
	pxor	%xmm1, %xmm1
	movaps	%xmm1, %xmm2
	movaps	%xmm1, %xmm3
	movaps	%xmm1, %xmm4
	movaps	%xmm1, %xmm5
	movaps	%xmm1, %xmm6
	movaps	%xmm1, %xmm7
	movaps	%xmm1, %xmm8
	movaps	%xmm1, %xmm9
	movaps	%xmm1, %xmm10
	movaps	%xmm1, %xmm11
	movaps	%xmm1, %xmm12
	movaps	%xmm1, %xmm13
	movaps	%xmm1, %xmm14
	movq	-32(%rsp), %rdi
	movq	%rsi, -8(%rsp)
	movq	-16(%rsp), %rsi
	movq	%rdx, -16(%rsp)
.L13:
	movss	(%rdi,%rax), %xmm0
	movaps	%xmm0, %xmm15
	mulss	(%r15,%rax), %xmm15
	addss	%xmm15, %xmm14
	movaps	%xmm0, %xmm15
	mulss	(%r14,%rax), %xmm15
	addss	%xmm15, %xmm13
	movaps	%xmm0, %xmm15
	mulss	0(%r13,%rax), %xmm15
	addss	%xmm15, %xmm12
	movaps	%xmm0, %xmm15
	mulss	(%r12,%rax), %xmm15
	addss	%xmm15, %xmm11
	movaps	%xmm0, %xmm15
	mulss	0(%rbp,%rax), %xmm15
	addss	%xmm15, %xmm10
	movaps	%xmm0, %xmm15
	mulss	(%rbx,%rax), %xmm15
	addss	%xmm15, %xmm9
	movaps	%xmm0, %xmm15
	mulss	(%r11,%rax), %xmm15
	addss	%xmm15, %xmm8
	movaps	%xmm0, %xmm15
	mulss	(%r10,%rax), %xmm15
	addss	%xmm15, %xmm7
	movaps	%xmm0, %xmm15
	mulss	(%r9,%rax), %xmm15
	addss	%xmm15, %xmm6
	movaps	%xmm0, %xmm15
	mulss	(%r8,%rax), %xmm15
	addss	%xmm15, %xmm5
	movaps	%xmm0, %xmm15
	mulss	(%rcx,%rax), %xmm15
	addss	%xmm15, %xmm4
	movq	-72(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	%xmm15, %xmm3
	movq	-64(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	%xmm15, %xmm2
	movq	-56(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	%xmm15, %xmm1
	movq	-48(%rsp), %rdx
	movaps	%xmm0, %xmm15
	mulss	(%rdx,%rax), %xmm15
	addss	-80(%rsp), %xmm15
	movss	%xmm15, -80(%rsp)
	movq	-40(%rsp), %rdx
	mulss	(%rdx,%rax), %xmm0
	addss	-76(%rsp), %xmm0
	movss	%xmm0, -76(%rsp)
	addq	$4, %rax
	cmpq	%rsi, %rax
	jne	.L13
	movq	%rdi, -32(%rsp)
	movq	-8(%rsp), %rsi
	movq	-16(%rsp), %rdx
.L12:
	movss	%xmm14, (%rdx)
	movss	%xmm13, 4(%rdx)
	movss	%xmm12, 8(%rdx)
	movss	%xmm11, 12(%rdx)
	movss	%xmm10, 16(%rdx)
	movss	%xmm9, 20(%rdx)
	movss	%xmm8, 24(%rdx)
	movss	%xmm7, 28(%rdx)
	movss	%xmm6, 32(%rdx)
	movss	%xmm5, 36(%rdx)
	movss	%xmm4, 40(%rdx)
	movss	%xmm3, 44(%rdx)
	movss	%xmm2, 48(%rdx)
	movss	%xmm1, 52(%rdx)
	movss	-80(%rsp), %xmm1
	movss	%xmm1, 56(%rdx)
	movss	-76(%rsp), %xmm2
	movss	%xmm2, 60(%rdx)
	cmpl	$0, -24(%rsp)
	jle	.L17
	movq	128(%rsi), %r15
	movq	136(%rsi), %r14
	movq	144(%rsi), %r13
	movq	152(%rsi), %r12
	movq	160(%rsi), %rbp
	movq	168(%rsi), %rbx
	movq	176(%rsi), %r11
	movq	184(%rsi), %r10
	movq	192(%rsi), %r9
	movq	200(%rsi), %r8
	movq	208(%rsi), %rcx
	movq	216(%rsi), %rax
	movq	224(%rsi), %rdi
	movq	%rdi, -72(%rsp)
	movq	232(%rsi), %rdi
	movq	%rdi, -64(%rsp)
	movq	240(%rsi), %rdi
	movq	%rdi, -56(%rsp)
	movq	248(%rsi), %rdi
	movq	%rdi, -48(%rsp)
	movslq	-24(%rsp), %rsi
	leaq	0(,%rsi,4), %rdi
	movq	%rdi, -24(%rsp)
	movl	$0, %esi
	movl	$0x00000000, -76(%rsp)
	movl	$0x00000000, -80(%rsp)
	pxor	%xmm1, %xmm1
	movaps	%xmm1, %xmm2
	movaps	%xmm1, %xmm3
	movaps	%xmm1, %xmm4
	movaps	%xmm1, %xmm5
	movaps	%xmm1, %xmm6
	movaps	%xmm1, %xmm7
	movaps	%xmm1, %xmm8
	movaps	%xmm1, %xmm9
	movaps	%xmm1, %xmm10
	movaps	%xmm1, %xmm11
	movaps	%xmm1, %xmm12
	movaps	%xmm1, %xmm13
	movaps	%xmm1, %xmm14
	movq	%rax, -40(%rsp)
	movq	-32(%rsp), %rdi
	movq	%rdx, -32(%rsp)
	movq	-24(%rsp), %rdx
.L15:
	movss	(%rdi,%rsi), %xmm0
	movaps	%xmm0, %xmm15
	mulss	(%r15,%rsi), %xmm15
	addss	%xmm15, %xmm14
	movaps	%xmm0, %xmm15
	mulss	(%r14,%rsi), %xmm15
	addss	%xmm15, %xmm13
	movaps	%xmm0, %xmm15
	mulss	0(%r13,%rsi), %xmm15
	addss	%xmm15, %xmm12
	movaps	%xmm0, %xmm15
	mulss	(%r12,%rsi), %xmm15
	addss	%xmm15, %xmm11
	movaps	%xmm0, %xmm15
	mulss	0(%rbp,%rsi), %xmm15
	addss	%xmm15, %xmm10
	movaps	%xmm0, %xmm15
	mulss	(%rbx,%rsi), %xmm15
	addss	%xmm15, %xmm9
	movaps	%xmm0, %xmm15
	mulss	(%r11,%rsi), %xmm15
	addss	%xmm15, %xmm8
	movaps	%xmm0, %xmm15
	mulss	(%r10,%rsi), %xmm15
	addss	%xmm15, %xmm7
	movaps	%xmm0, %xmm15
	mulss	(%r9,%rsi), %xmm15
	addss	%xmm15, %xmm6
	movaps	%xmm0, %xmm15
	mulss	(%r8,%rsi), %xmm15
	addss	%xmm15, %xmm5
	movaps	%xmm0, %xmm15
	mulss	(%rcx,%rsi), %xmm15
	addss	%xmm15, %xmm4
	movaps	%xmm0, %xmm15
	movq	-40(%rsp), %rax
	mulss	(%rax,%rsi), %xmm15
	addss	%xmm15, %xmm3
	movq	-72(%rsp), %rax
	movaps	%xmm0, %xmm15
	mulss	(%rax,%rsi), %xmm15
	addss	%xmm15, %xmm2
	movq	-64(%rsp), %rax
	movaps	%xmm0, %xmm15
	mulss	(%rax,%rsi), %xmm15
	addss	%xmm15, %xmm1
	movq	-56(%rsp), %rax
	movaps	%xmm0, %xmm15
	mulss	(%rax,%rsi), %xmm15
	addss	-80(%rsp), %xmm15
	movss	%xmm15, -80(%rsp)
	movq	-48(%rsp), %rax
	mulss	(%rax,%rsi), %xmm0
	addss	-76(%rsp), %xmm0
	movss	%xmm0, -76(%rsp)
	addq	$4, %rsi
	cmpq	%rdx, %rsi
	jne	.L15
	movq	-32(%rsp), %rdx
.L14:
	movss	%xmm14, 64(%rdx)
	movss	%xmm13, 68(%rdx)
	movss	%xmm12, 72(%rdx)
	movss	%xmm11, 76(%rdx)
	movss	%xmm10, 80(%rdx)
	movss	%xmm9, 84(%rdx)
	movss	%xmm8, 88(%rdx)
	movss	%xmm7, 92(%rdx)
	movss	%xmm6, 96(%rdx)
	movss	%xmm5, 100(%rdx)
	movss	%xmm4, 104(%rdx)
	movss	%xmm3, 108(%rdx)
	movss	%xmm2, 112(%rdx)
	movss	%xmm1, 116(%rdx)
	movss	-80(%rsp), %xmm3
	movss	%xmm3, 120(%rdx)
	movss	-76(%rsp), %xmm4
	movss	%xmm4, 124(%rdx)
	popq	%rbx
	.cfi_remember_state
	.cfi_def_cfa_offset 48
	popq	%rbp
	.cfi_def_cfa_offset 40
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r13
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	ret
.L16:
	.cfi_restore_state
	movl	$0x00000000, -76(%rsp)
	movl	$0x00000000, -80(%rsp)
	pxor	%xmm1, %xmm1
	movaps	%xmm1, %xmm2
	movaps	%xmm1, %xmm3
	movaps	%xmm1, %xmm4
	movaps	%xmm1, %xmm5
	movaps	%xmm1, %xmm6
	movaps	%xmm1, %xmm7
	movaps	%xmm1, %xmm8
	movaps	%xmm1, %xmm9
	movaps	%xmm1, %xmm10
	movaps	%xmm1, %xmm11
	movaps	%xmm1, %xmm12
	movaps	%xmm1, %xmm13
	movaps	%xmm1, %xmm14
	jmp	.L12
.L17:
	movl	$0x00000000, -76(%rsp)
	movl	$0x00000000, -80(%rsp)
	pxor	%xmm1, %xmm1
	movaps	%xmm1, %xmm2
	movaps	%xmm1, %xmm3
	movaps	%xmm1, %xmm4
	movaps	%xmm1, %xmm5
	movaps	%xmm1, %xmm6
	movaps	%xmm1, %xmm7
	movaps	%xmm1, %xmm8
	movaps	%xmm1, %xmm9
	movaps	%xmm1, %xmm10
	movaps	%xmm1, %xmm11
	movaps	%xmm1, %xmm12
	movaps	%xmm1, %xmm13
	movaps	%xmm1, %xmm14
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
	pushq	%r14
	.cfi_def_cfa_offset 16
	.cfi_offset 14, -16
	pushq	%r13
	.cfi_def_cfa_offset 24
	.cfi_offset 13, -24
	pushq	%r12
	.cfi_def_cfa_offset 32
	.cfi_offset 12, -32
	pushq	%rbp
	.cfi_def_cfa_offset 40
	.cfi_offset 6, -40
	pushq	%rbx
	.cfi_def_cfa_offset 48
	.cfi_offset 3, -48
	movq	%rsi, %r8
	movq	%rdx, %rsi
	testl	%ecx, %ecx
	jle	.L22
	movq	(%r8), %r14
	movq	8(%r8), %r13
	movq	16(%r8), %r12
	movq	24(%r8), %rbp
	movq	32(%r8), %rbx
	movq	40(%r8), %r11
	movq	48(%r8), %r10
	movq	56(%r8), %r9
	movslq	%ecx, %rcx
	leaq	0(,%rcx,4), %rdx
	movl	$0, %eax
	pxor	%xmm1, %xmm1
	movaps	%xmm1, %xmm2
	movaps	%xmm1, %xmm3
	movaps	%xmm1, %xmm4
	movaps	%xmm1, %xmm5
	movaps	%xmm1, %xmm6
	movaps	%xmm1, %xmm7
	movaps	%xmm1, %xmm8
.L23:
	movss	(%rdi,%rax), %xmm0
	movaps	%xmm0, %xmm9
	mulss	(%r14,%rax), %xmm9
	addss	%xmm9, %xmm8
	movaps	%xmm0, %xmm9
	mulss	0(%r13,%rax), %xmm9
	addss	%xmm9, %xmm7
	movaps	%xmm0, %xmm9
	mulss	(%r12,%rax), %xmm9
	addss	%xmm9, %xmm6
	movaps	%xmm0, %xmm9
	mulss	0(%rbp,%rax), %xmm9
	addss	%xmm9, %xmm5
	movaps	%xmm0, %xmm9
	mulss	(%rbx,%rax), %xmm9
	addss	%xmm9, %xmm4
	movaps	%xmm0, %xmm9
	mulss	(%r11,%rax), %xmm9
	addss	%xmm9, %xmm3
	movaps	%xmm0, %xmm9
	mulss	(%r10,%rax), %xmm9
	addss	%xmm9, %xmm2
	mulss	(%r9,%rax), %xmm0
	addss	%xmm0, %xmm1
	addq	$4, %rax
	cmpq	%rax, %rdx
	jne	.L23
	movss	%xmm8, (%rsi)
	movss	%xmm7, 4(%rsi)
	movss	%xmm6, 8(%rsi)
	movss	%xmm5, 12(%rsi)
	movss	%xmm4, 16(%rsi)
	movss	%xmm3, 20(%rsi)
	movss	%xmm2, 24(%rsi)
	movss	%xmm1, 28(%rsi)
	movq	64(%r8), %r13
	movq	72(%r8), %r12
	movq	80(%r8), %rbp
	movq	88(%r8), %rbx
	movq	96(%r8), %r11
	movq	104(%r8), %r10
	movq	112(%r8), %r9
	movq	120(%r8), %rcx
	movl	$0, %eax
	pxor	%xmm1, %xmm1
	movaps	%xmm1, %xmm2
	movaps	%xmm1, %xmm3
	movaps	%xmm1, %xmm4
	movaps	%xmm1, %xmm5
	movaps	%xmm1, %xmm6
	movaps	%xmm1, %xmm7
	movaps	%xmm1, %xmm8
.L24:
	movss	(%rdi,%rax), %xmm0
	movaps	%xmm0, %xmm9
	mulss	0(%r13,%rax), %xmm9
	addss	%xmm9, %xmm8
	movaps	%xmm0, %xmm9
	mulss	(%r12,%rax), %xmm9
	addss	%xmm9, %xmm7
	movaps	%xmm0, %xmm9
	mulss	0(%rbp,%rax), %xmm9
	addss	%xmm9, %xmm6
	movaps	%xmm0, %xmm9
	mulss	(%rbx,%rax), %xmm9
	addss	%xmm9, %xmm5
	movaps	%xmm0, %xmm9
	mulss	(%r11,%rax), %xmm9
	addss	%xmm9, %xmm4
	movaps	%xmm0, %xmm9
	mulss	(%r10,%rax), %xmm9
	addss	%xmm9, %xmm3
	movaps	%xmm0, %xmm9
	mulss	(%r9,%rax), %xmm9
	addss	%xmm9, %xmm2
	mulss	(%rcx,%rax), %xmm0
	addss	%xmm0, %xmm1
	addq	$4, %rax
	cmpq	%rax, %rdx
	jne	.L24
	movss	%xmm8, 32(%rsi)
	movss	%xmm7, 36(%rsi)
	movss	%xmm6, 40(%rsi)
	movss	%xmm5, 44(%rsi)
	movss	%xmm4, 48(%rsi)
	movss	%xmm3, 52(%rsi)
	movss	%xmm2, 56(%rsi)
	movss	%xmm1, 60(%rsi)
	movq	128(%r8), %r13
	movq	136(%r8), %r12
	movq	144(%r8), %rbp
	movq	152(%r8), %rbx
	movq	160(%r8), %r11
	movq	168(%r8), %r10
	movq	176(%r8), %r9
	movq	184(%r8), %rcx
	movl	$0, %eax
	pxor	%xmm1, %xmm1
	movaps	%xmm1, %xmm2
	movaps	%xmm1, %xmm3
	movaps	%xmm1, %xmm4
	movaps	%xmm1, %xmm5
	movaps	%xmm1, %xmm6
	movaps	%xmm1, %xmm7
	movaps	%xmm1, %xmm8
.L25:
	movss	(%rdi,%rax), %xmm0
	movaps	%xmm0, %xmm9
	mulss	0(%r13,%rax), %xmm9
	addss	%xmm9, %xmm8
	movaps	%xmm0, %xmm9
	mulss	(%r12,%rax), %xmm9
	addss	%xmm9, %xmm7
	movaps	%xmm0, %xmm9
	mulss	0(%rbp,%rax), %xmm9
	addss	%xmm9, %xmm6
	movaps	%xmm0, %xmm9
	mulss	(%rbx,%rax), %xmm9
	addss	%xmm9, %xmm5
	movaps	%xmm0, %xmm9
	mulss	(%r11,%rax), %xmm9
	addss	%xmm9, %xmm4
	movaps	%xmm0, %xmm9
	mulss	(%r10,%rax), %xmm9
	addss	%xmm9, %xmm3
	movaps	%xmm0, %xmm9
	mulss	(%r9,%rax), %xmm9
	addss	%xmm9, %xmm2
	mulss	(%rcx,%rax), %xmm0
	addss	%xmm0, %xmm1
	addq	$4, %rax
	cmpq	%rdx, %rax
	jne	.L25
	movss	%xmm8, 64(%rsi)
	movss	%xmm7, 68(%rsi)
	movss	%xmm6, 72(%rsi)
	movss	%xmm5, 76(%rsi)
	movss	%xmm4, 80(%rsi)
	movss	%xmm3, 84(%rsi)
	movss	%xmm2, 88(%rsi)
	movss	%xmm1, 92(%rsi)
	movq	192(%r8), %r13
	movq	200(%r8), %r12
	movq	208(%r8), %rbp
	movq	216(%r8), %rbx
	movq	224(%r8), %r11
	movq	232(%r8), %r10
	movq	240(%r8), %r9
	movq	248(%r8), %rcx
	movl	$0, %eax
	pxor	%xmm8, %xmm8
	movaps	%xmm8, %xmm7
	movaps	%xmm8, %xmm6
	movaps	%xmm8, %xmm5
	movaps	%xmm8, %xmm4
	movaps	%xmm8, %xmm3
	movaps	%xmm8, %xmm2
	movaps	%xmm8, %xmm1
.L26:
	movss	(%rdi,%rax), %xmm0
	movaps	%xmm0, %xmm9
	mulss	0(%r13,%rax), %xmm9
	addss	%xmm9, %xmm1
	movaps	%xmm0, %xmm9
	mulss	(%r12,%rax), %xmm9
	addss	%xmm9, %xmm2
	movaps	%xmm0, %xmm9
	mulss	0(%rbp,%rax), %xmm9
	addss	%xmm9, %xmm3
	movaps	%xmm0, %xmm9
	mulss	(%rbx,%rax), %xmm9
	addss	%xmm9, %xmm4
	movaps	%xmm0, %xmm9
	mulss	(%r11,%rax), %xmm9
	addss	%xmm9, %xmm5
	movaps	%xmm0, %xmm9
	mulss	(%r10,%rax), %xmm9
	addss	%xmm9, %xmm6
	movaps	%xmm0, %xmm9
	mulss	(%r9,%rax), %xmm9
	addss	%xmm9, %xmm7
	mulss	(%rcx,%rax), %xmm0
	addss	%xmm0, %xmm8
	addq	$4, %rax
	cmpq	%rdx, %rax
	jne	.L26
.L27:
	movss	%xmm1, 96(%rsi)
	movss	%xmm2, 100(%rsi)
	movss	%xmm3, 104(%rsi)
	movss	%xmm4, 108(%rsi)
	movss	%xmm5, 112(%rsi)
	movss	%xmm6, 116(%rsi)
	movss	%xmm7, 120(%rsi)
	movss	%xmm8, 124(%rsi)
	popq	%rbx
	.cfi_remember_state
	.cfi_def_cfa_offset 40
	popq	%rbp
	.cfi_def_cfa_offset 32
	popq	%r12
	.cfi_def_cfa_offset 24
	popq	%r13
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	ret
.L22:
	.cfi_restore_state
	movl	$0x00000000, (%rdx)
	movl	$0x00000000, 4(%rdx)
	movl	$0x00000000, 8(%rdx)
	movl	$0x00000000, 12(%rdx)
	movl	$0x00000000, 16(%rdx)
	movl	$0x00000000, 20(%rdx)
	movl	$0x00000000, 24(%rdx)
	movl	$0x00000000, 28(%rdx)
	movl	$0x00000000, 32(%rdx)
	movl	$0x00000000, 36(%rdx)
	movl	$0x00000000, 40(%rdx)
	movl	$0x00000000, 44(%rdx)
	movl	$0x00000000, 48(%rdx)
	movl	$0x00000000, 52(%rdx)
	movl	$0x00000000, 56(%rdx)
	movl	$0x00000000, 60(%rdx)
	movl	$0x00000000, 64(%rdx)
	movl	$0x00000000, 68(%rdx)
	movl	$0x00000000, 72(%rdx)
	movl	$0x00000000, 76(%rdx)
	movl	$0x00000000, 80(%rdx)
	movl	$0x00000000, 84(%rdx)
	movl	$0x00000000, 88(%rdx)
	movl	$0x00000000, 92(%rdx)
	pxor	%xmm8, %xmm8
	movaps	%xmm8, %xmm7
	movaps	%xmm8, %xmm6
	movaps	%xmm8, %xmm5
	movaps	%xmm8, %xmm4
	movaps	%xmm8, %xmm3
	movaps	%xmm8, %xmm2
	movaps	%xmm8, %xmm1
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
	pushq	%rbx
	.cfi_def_cfa_offset 16
	.cfi_offset 3, -16
	movq	%rsi, %r8
	movq	%rdx, %rsi
	testl	%ecx, %ecx
	jle	.L35
	movq	(%r8), %rbx
	movq	8(%r8), %r11
	movq	16(%r8), %r10
	movq	24(%r8), %r9
	movslq	%ecx, %rcx
	leaq	0(,%rcx,4), %rax
	movl	$0, %edx
	pxor	%xmm1, %xmm1
	movaps	%xmm1, %xmm2
	movaps	%xmm1, %xmm3
	movaps	%xmm1, %xmm4
.L36:
	movss	(%rdi,%rdx), %xmm0
	movaps	%xmm0, %xmm5
	mulss	(%rbx,%rdx), %xmm5
	addss	%xmm5, %xmm4
	movaps	%xmm0, %xmm5
	mulss	(%r11,%rdx), %xmm5
	addss	%xmm5, %xmm3
	movaps	%xmm0, %xmm5
	mulss	(%r10,%rdx), %xmm5
	addss	%xmm5, %xmm2
	mulss	(%r9,%rdx), %xmm0
	addss	%xmm0, %xmm1
	addq	$4, %rdx
	cmpq	%rdx, %rax
	jne	.L36
	movss	%xmm4, (%rsi)
	movss	%xmm3, 4(%rsi)
	movss	%xmm2, 8(%rsi)
	movss	%xmm1, 12(%rsi)
	movq	32(%r8), %r11
	movq	40(%r8), %r10
	movq	48(%r8), %r9
	movq	56(%r8), %rcx
	movl	$0, %edx
	pxor	%xmm1, %xmm1
	movaps	%xmm1, %xmm2
	movaps	%xmm1, %xmm3
	movaps	%xmm1, %xmm4
.L37:
	movss	(%rdi,%rdx), %xmm0
	movaps	%xmm0, %xmm5
	mulss	(%r11,%rdx), %xmm5
	addss	%xmm5, %xmm4
	movaps	%xmm0, %xmm5
	mulss	(%r10,%rdx), %xmm5
	addss	%xmm5, %xmm3
	movaps	%xmm0, %xmm5
	mulss	(%r9,%rdx), %xmm5
	addss	%xmm5, %xmm2
	mulss	(%rcx,%rdx), %xmm0
	addss	%xmm0, %xmm1
	addq	$4, %rdx
	cmpq	%rdx, %rax
	jne	.L37
	movss	%xmm4, 16(%rsi)
	movss	%xmm3, 20(%rsi)
	movss	%xmm2, 24(%rsi)
	movss	%xmm1, 28(%rsi)
	movq	64(%r8), %r11
	movq	72(%r8), %r10
	movq	80(%r8), %r9
	movq	88(%r8), %rcx
	movl	$0, %edx
	pxor	%xmm1, %xmm1
	movaps	%xmm1, %xmm2
	movaps	%xmm1, %xmm3
	movaps	%xmm1, %xmm4
.L38:
	movss	(%rdi,%rdx), %xmm0
	movaps	%xmm0, %xmm5
	mulss	(%r11,%rdx), %xmm5
	addss	%xmm5, %xmm4
	movaps	%xmm0, %xmm5
	mulss	(%r10,%rdx), %xmm5
	addss	%xmm5, %xmm3
	movaps	%xmm0, %xmm5
	mulss	(%r9,%rdx), %xmm5
	addss	%xmm5, %xmm2
	mulss	(%rcx,%rdx), %xmm0
	addss	%xmm0, %xmm1
	addq	$4, %rdx
	cmpq	%rax, %rdx
	jne	.L38
	movss	%xmm4, 32(%rsi)
	movss	%xmm3, 36(%rsi)
	movss	%xmm2, 40(%rsi)
	movss	%xmm1, 44(%rsi)
	movq	96(%r8), %r11
	movq	104(%r8), %r10
	movq	112(%r8), %r9
	movq	120(%r8), %rcx
	movl	$0, %edx
	pxor	%xmm1, %xmm1
	movaps	%xmm1, %xmm2
	movaps	%xmm1, %xmm3
	movaps	%xmm1, %xmm4
.L39:
	movss	(%rdi,%rdx), %xmm0
	movaps	%xmm0, %xmm5
	mulss	(%r11,%rdx), %xmm5
	addss	%xmm5, %xmm4
	movaps	%xmm0, %xmm5
	mulss	(%r10,%rdx), %xmm5
	addss	%xmm5, %xmm3
	movaps	%xmm0, %xmm5
	mulss	(%r9,%rdx), %xmm5
	addss	%xmm5, %xmm2
	mulss	(%rcx,%rdx), %xmm0
	addss	%xmm0, %xmm1
	addq	$4, %rdx
	cmpq	%rax, %rdx
	jne	.L39
	movss	%xmm4, 48(%rsi)
	movss	%xmm3, 52(%rsi)
	movss	%xmm2, 56(%rsi)
	movss	%xmm1, 60(%rsi)
	movq	128(%r8), %r11
	movq	136(%r8), %r10
	movq	144(%r8), %r9
	movq	152(%r8), %rcx
	movl	$0, %edx
	pxor	%xmm1, %xmm1
	movaps	%xmm1, %xmm2
	movaps	%xmm1, %xmm3
	movaps	%xmm1, %xmm4
.L40:
	movss	(%rdi,%rdx), %xmm0
	movaps	%xmm0, %xmm5
	mulss	(%r11,%rdx), %xmm5
	addss	%xmm5, %xmm4
	movaps	%xmm0, %xmm5
	mulss	(%r10,%rdx), %xmm5
	addss	%xmm5, %xmm3
	movaps	%xmm0, %xmm5
	mulss	(%r9,%rdx), %xmm5
	addss	%xmm5, %xmm2
	mulss	(%rcx,%rdx), %xmm0
	addss	%xmm0, %xmm1
	addq	$4, %rdx
	cmpq	%rax, %rdx
	jne	.L40
	movss	%xmm4, 64(%rsi)
	movss	%xmm3, 68(%rsi)
	movss	%xmm2, 72(%rsi)
	movss	%xmm1, 76(%rsi)
	movq	160(%r8), %r11
	movq	168(%r8), %r10
	movq	176(%r8), %r9
	movq	184(%r8), %rcx
	movl	$0, %edx
	pxor	%xmm1, %xmm1
	movaps	%xmm1, %xmm2
	movaps	%xmm1, %xmm3
	movaps	%xmm1, %xmm4
.L41:
	movss	(%rdi,%rdx), %xmm0
	movaps	%xmm0, %xmm5
	mulss	(%r11,%rdx), %xmm5
	addss	%xmm5, %xmm4
	movaps	%xmm0, %xmm5
	mulss	(%r10,%rdx), %xmm5
	addss	%xmm5, %xmm3
	movaps	%xmm0, %xmm5
	mulss	(%r9,%rdx), %xmm5
	addss	%xmm5, %xmm2
	mulss	(%rcx,%rdx), %xmm0
	addss	%xmm0, %xmm1
	addq	$4, %rdx
	cmpq	%rax, %rdx
	jne	.L41
	movss	%xmm4, 80(%rsi)
	movss	%xmm3, 84(%rsi)
	movss	%xmm2, 88(%rsi)
	movss	%xmm1, 92(%rsi)
	movq	192(%r8), %r11
	movq	200(%r8), %r10
	movq	208(%r8), %r9
	movq	216(%r8), %rcx
	movl	$0, %edx
	pxor	%xmm1, %xmm1
	movaps	%xmm1, %xmm2
	movaps	%xmm1, %xmm3
	movaps	%xmm1, %xmm4
.L42:
	movss	(%rdi,%rdx), %xmm0
	movaps	%xmm0, %xmm5
	mulss	(%r11,%rdx), %xmm5
	addss	%xmm5, %xmm4
	movaps	%xmm0, %xmm5
	mulss	(%r10,%rdx), %xmm5
	addss	%xmm5, %xmm3
	movaps	%xmm0, %xmm5
	mulss	(%r9,%rdx), %xmm5
	addss	%xmm5, %xmm2
	mulss	(%rcx,%rdx), %xmm0
	addss	%xmm0, %xmm1
	addq	$4, %rdx
	cmpq	%rax, %rdx
	jne	.L42
	movss	%xmm4, 96(%rsi)
	movss	%xmm3, 100(%rsi)
	movss	%xmm2, 104(%rsi)
	movss	%xmm1, 108(%rsi)
	movq	224(%r8), %r11
	movq	232(%r8), %r10
	movq	240(%r8), %r9
	movq	248(%r8), %rcx
	movl	$0, %edx
	pxor	%xmm4, %xmm4
	movaps	%xmm4, %xmm3
	movaps	%xmm4, %xmm2
	movaps	%xmm4, %xmm1
.L43:
	movss	(%rdi,%rdx), %xmm0
	movaps	%xmm0, %xmm5
	mulss	(%r11,%rdx), %xmm5
	addss	%xmm5, %xmm1
	movaps	%xmm0, %xmm5
	mulss	(%r10,%rdx), %xmm5
	addss	%xmm5, %xmm2
	movaps	%xmm0, %xmm5
	mulss	(%r9,%rdx), %xmm5
	addss	%xmm5, %xmm3
	mulss	(%rcx,%rdx), %xmm0
	addss	%xmm0, %xmm4
	addq	$4, %rdx
	cmpq	%rdx, %rax
	jne	.L43
.L44:
	movss	%xmm1, 112(%rsi)
	movss	%xmm2, 116(%rsi)
	movss	%xmm3, 120(%rsi)
	movss	%xmm4, 124(%rsi)
	popq	%rbx
	.cfi_remember_state
	.cfi_def_cfa_offset 8
	ret
.L35:
	.cfi_restore_state
	movl	$0x00000000, (%rdx)
	movl	$0x00000000, 4(%rdx)
	movl	$0x00000000, 8(%rdx)
	movl	$0x00000000, 12(%rdx)
	movl	$0x00000000, 16(%rdx)
	movl	$0x00000000, 20(%rdx)
	movl	$0x00000000, 24(%rdx)
	movl	$0x00000000, 28(%rdx)
	movl	$0x00000000, 32(%rdx)
	movl	$0x00000000, 36(%rdx)
	movl	$0x00000000, 40(%rdx)
	movl	$0x00000000, 44(%rdx)
	movl	$0x00000000, 48(%rdx)
	movl	$0x00000000, 52(%rdx)
	movl	$0x00000000, 56(%rdx)
	movl	$0x00000000, 60(%rdx)
	movl	$0x00000000, 64(%rdx)
	movl	$0x00000000, 68(%rdx)
	movl	$0x00000000, 72(%rdx)
	movl	$0x00000000, 76(%rdx)
	movl	$0x00000000, 80(%rdx)
	movl	$0x00000000, 84(%rdx)
	movl	$0x00000000, 88(%rdx)
	movl	$0x00000000, 92(%rdx)
	movl	$0x00000000, 96(%rdx)
	movl	$0x00000000, 100(%rdx)
	movl	$0x00000000, 104(%rdx)
	movl	$0x00000000, 108(%rdx)
	pxor	%xmm4, %xmm4
	movaps	%xmm4, %xmm3
	movaps	%xmm4, %xmm2
	movaps	%xmm4, %xmm1
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
	pushq	%r12
	.cfi_def_cfa_offset 16
	.cfi_offset 12, -16
	pushq	%rbp
	.cfi_def_cfa_offset 24
	.cfi_offset 6, -24
	pushq	%rbx
	.cfi_def_cfa_offset 32
	.cfi_offset 3, -32
	movq	%rsi, %rax
	shrq	$61, %rax
	jne	.L65
	movq	%rdi, %rbx
	movq	%rsi, %rbp
	movq	$0, (%rdi)
	movq	$0, 8(%rdi)
	movq	$0, 16(%rdi)
	testq	%rsi, %rsi
	je	.L57
	leaq	0(,%rsi,4), %r12
	movq	%r12, %rdi
	call	_Znwm@PLT
	movq	%rax, (%rbx)
	movq	%rax, 8(%rbx)
	leaq	(%rax,%r12), %rdx
	movq	%rdx, 16(%rbx)
	movl	$0x00000000, (%rax)
	addq	$4, %rax
	cmpq	$1, %rbp
	je	.L60
	cmpq	%rax, %rdx
	je	.L61
.L59:
	movl	$0x00000000, (%rax)
	addq	$4, %rax
	cmpq	%rax, %rdx
	jne	.L59
	jmp	.L58
.L65:
	leaq	.LC3(%rip), %rdi
	call	_ZSt20__throw_length_errorPKc@PLT
.L60:
	movq	%rax, %rdx
	jmp	.L58
.L61:
	movq	%rax, %rdx
	jmp	.L58
.L57:
	movq	$0, (%rdi)
	movq	$0, 16(%rdi)
	movl	$0, %edx
.L58:
	movq	%rdx, 8(%rbx)
	popq	%rbx
	.cfi_def_cfa_offset 24
	popq	%rbp
	.cfi_def_cfa_offset 16
	popq	%r12
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
	movq	(%rdi), %rax
	testq	%rax, %rax
	je	.L69
	subq	$8, %rsp
	.cfi_def_cfa_offset 16
	movq	16(%rdi), %rsi
	subq	%rax, %rsi
	movq	%rax, %rdi
	call	_ZdlPvm@PLT
	addq	$8, %rsp
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
	pushq	%r12
	.cfi_def_cfa_offset 16
	.cfi_offset 12, -16
	pushq	%rbp
	.cfi_def_cfa_offset 24
	.cfi_offset 6, -24
	pushq	%rbx
	.cfi_def_cfa_offset 32
	.cfi_offset 3, -32
	movq	%rdi, %r12
	movq	8(%rdi), %rbp
	movq	(%rdi), %rbx
	cmpq	%rbx, %rbp
	jne	.L75
.L73:
	movq	(%r12), %rdi
	testq	%rdi, %rdi
	je	.L72
	movq	16(%r12), %rsi
	subq	%rdi, %rsi
	call	_ZdlPvm@PLT
.L72:
	popq	%rbx
	.cfi_remember_state
	.cfi_def_cfa_offset 24
	popq	%rbp
	.cfi_def_cfa_offset 16
	popq	%r12
	.cfi_def_cfa_offset 8
	ret
.L74:
	.cfi_restore_state
	addq	$24, %rbx
	cmpq	%rbx, %rbp
	je	.L73
.L75:
	movq	(%rbx), %rdi
	testq	%rdi, %rdi
	je	.L74
	movq	16(%rbx), %rsi
	subq	%rdi, %rsi
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
	movq	(%rdi), %rax
	testq	%rax, %rax
	je	.L82
	subq	$8, %rsp
	.cfi_def_cfa_offset 16
	movq	16(%rdi), %rsi
	subq	%rax, %rsi
	movq	%rax, %rdi
	call	_ZdlPvm@PLT
	addq	$8, %rsp
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
	pushq	%r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	pushq	%r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	pushq	%r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	pushq	%r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	pushq	%rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	subq	$24, %rsp
	.cfi_def_cfa_offset 80
	movq	%rdi, 8(%rsp)
	testq	%rsi, %rsi
	je	.L96
	movq	%rsi, %r14
	movq	%rdx, %r13
	movq	%rdi, %rbx
	movabsq	$9223372036854775804, %r15
	jmp	.L91
.L102:
.LEHB0:
	call	_ZSt28__throw_bad_array_new_lengthv@PLT
.LEHE0:
.L98:
	endbr64
	movq	%rax, %rdi
	call	__cxa_begin_catch@PLT
.L93:
	cmpq	%rbx, 8(%rsp)
	jne	.L94
.LEHB1:
	call	__cxa_rethrow@PLT
.LEHE1:
.L99:
	endbr64
	movq	%rax, %rbx
	call	__cxa_end_catch@PLT
	movq	%rbx, %rdi
.LEHB2:
	call	_Unwind_Resume@PLT
.LEHE2:
.L103:
	movq	%rax, %rbp
.L87:
	movq	%rbp, (%rbx)
	movq	%rbp, 8(%rbx)
	addq	%rbp, %r12
	movq	%r12, 16(%rbx)
	movq	0(%r13), %rsi
	movq	8(%r13), %r12
	subq	%rsi, %r12
	cmpq	$4, %r12
	jle	.L89
	movq	%r12, %rdx
	movq	%rbp, %rdi
	call	memmove@PLT
.L90:
	addq	%r12, %rbp
	movq	%rbp, 8(%rbx)
	addq	$24, %rbx
	subq	$1, %r14
	je	.L85
.L91:
	movq	8(%r13), %r12
	subq	0(%r13), %r12
	movq	$0, (%rbx)
	movq	$0, 8(%rbx)
	movq	$0, 16(%rbx)
	je	.L97
	cmpq	%r12, %r15
	jb	.L102
	movq	%r12, %rdi
.LEHB3:
	call	_Znwm@PLT
.LEHE3:
	jmp	.L103
.L97:
	movl	$0, %ebp
	jmp	.L87
.L89:
	jne	.L90
	movss	(%rsi), %xmm0
	movss	%xmm0, 0(%rbp)
	jmp	.L90
.L96:
	movq	8(%rsp), %rbx
.L85:
	movq	%rbx, %rax
	addq	$24, %rsp
	.cfi_remember_state
	.cfi_def_cfa_offset 56
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%rbp
	.cfi_def_cfa_offset 40
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r13
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	ret
.L94:
	.cfi_restore_state
	movq	8(%rsp), %r15
	movq	%r15, %rdi
	call	_ZNSt6vectorIfSaIfEED1Ev
	movq	%r15, %rax
	addq	$24, %rax
	movq	%rax, 8(%rsp)
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
	.ascii	"ack traffic exactly:\n\n  g++ -O1 -S -o fission.s loop_fissi"
	.ascii	"on.cpp\n  for f in fused split16 split8 split4; do \\\n     "
	.ascii	" echo -n \"$f \"; sed -n \"/correlate_$f/,/^\\s*\\.size/p\" "
	.ascii	"fission.s \\\n      | grep -c '(%rsp)'; done\n\n    32 accum"
	.ascii	"ulators -> 134 stack refs      8 accumulators ->   0\n    16"
	.ascii	" accumulators ->  57 stack refs      4 accumulators ->   0\n"
	.ascii	"\nSixteen is not enough on this target: x86-64 has 16 archit"
	.ascii	"ectural XMM\nregisters, and 16 accumulators plus the referen"
	.ascii	"ce pointers and loop\nstate still overflow them.  On an ISA "
	.ascii	"with 32 FP registers it would\nfit.  The best split is an IS"
	.ascii	"A property -- re-measure it per target.\n\nSplitting past th"
	.ascii	"e knee buys nothing: at 4 accumulators there are no\nspills "
	.ascii	"left to remove, and eight passes over the signal cancel the "
	.ascii	"rest.\n\nOne measurement trap worth knowing: the reference v"
	.ascii	"ectors are 32\nseparate 40 MB allocations and malloc hands t"
	.ascii	"hem back with identical\nalignment, w"
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
	pushq	%r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	pushq	%r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	pushq	%r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	pushq	%r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	pushq	%rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	subq	$360, %rsp
	.cfi_def_cfa_offset 416
	movq	%fs:40, %rax
	movq	%rax, 344(%rsp)
	xorl	%eax, %eax
	leaq	.LC8(%rip), %rdi
	call	getenv@PLT
	testq	%rax, %rax
	je	.L105
	movq	%rax, %rdi
	movl	$10, %edx
	movl	$0, %esi
	call	__isoc23_strtol@PLT
	movq	%rax, %rdx
	movl	%eax, 16(%rsp)
	leaq	208(%rsp), %rsi
	movl	$32, %ecx
	movl	$0, %eax
	movq	%rsi, %rdi
	rep stosl
	movslq	%edx, %rax
	cmpq	$1023, %rax
	jbe	.L142
.L106:
	leaq	208(%rsp), %rdx
	movl	$128, %esi
	movl	$0, %edi
	call	sched_setaffinity@PLT
	testl	%eax, %eax
	jne	.L174
.L107:
	call	_ZNSt6chrono3_V212steady_clock3nowEv@PLT
	movq	%rax, 24(%rsp)
	movl	$0, %r15d
	movl	$0, %r14d
	movl	$0, %r13d
	movl	$0, %r12d
	movl	$0, %ebp
	movl	$0, %ebx
	movq	$0, 8(%rsp)
	movq	$0, (%rsp)
.L108:
	call	_ZNSt6chrono3_V212steady_clock3nowEv@PLT
	movq	24(%rsp), %rsi
	subq	%rsi, %rax
	pxor	%xmm0, %xmm0
	cvtsi2sdq	%rax, %xmm0
	divsd	.LC0(%rip), %xmm0
	movsd	.LC11(%rip), %xmm3
	comisd	%xmm0, %xmm3
	jbe	.L175
	movl	$200000, %ecx
.L109:
	movq	(%rsp), %rdx
	movq	8(%rsp), %rax
#APP
# 157 "loop_fission.cpp" 1
	addq $1,%rdx
	addq $1,%rax
	addq $1,%rbx
	addq $1,%rbp
	addq $1,%r12
	addq $1,%r13
	addq $1,%r14
	addq $1,%r15
# 0 "" 2
#NO_APP
	movq	%rdx, (%rsp)
	movq	%rax, 8(%rsp)
	subl	$1, %ecx
	jne	.L109
	jmp	.L108
.L174:
	leaq	.LC9(%rip), %rsi
	leaq	_ZSt4cerr(%rip), %rdi
.LEHB4:
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	movq	%rax, %rdi
	movl	16(%rsp), %esi
	call	_ZNSolsEi@PLT
	movq	%rax, %rdi
	leaq	.LC10(%rip), %rsi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	jmp	.L107
.L175:
	call	_ZL14core_clock_ghzv
	movsd	%xmm0, (%rsp)
	leaq	.LC12(%rip), %rsi
	leaq	_ZSt4cout(%rip), %rdi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	movq	%rax, %rdi
	leaq	.LC13(%rip), %rsi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	movq	%rax, %rdi
	leaq	.LC14(%rip), %rsi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	movq	%rax, %rdi
	movq	(%rax), %rax
	movq	%rdi, %rdx
	addq	-24(%rax), %rdx
	movl	24(%rdx), %eax
	andl	$-261, %eax
	orl	$4, %eax
	movl	%eax, 24(%rdx)
	movq	(%rdi), %rax
	movq	-24(%rax), %rax
	movq	$2, 8(%rdi,%rax)
	movsd	(%rsp), %xmm0
	call	_ZNSo9_M_insertIdEERSoT_@PLT
	movq	%rax, %rdi
	leaq	.LC15(%rip), %rsi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	movq	%rax, %rdi
	movl	16(%rsp), %esi
	call	_ZNSolsEi@PLT
	movq	%rax, %rdi
	leaq	.LC16(%rip), %rsi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	leaq	176(%rsp), %rbx
	leaq	48(%rsp), %rdi
	movq	%rbx, %rdx
	movl	$10000000, %esi
	call	_ZNSt6vectorIfSaIfEEC1EmRKS0_
.LEHE4:
	leaq	144(%rsp), %rdx
	movl	$10000000, %esi
	movq	%rbx, %rdi
.LEHB5:
	call	_ZNSt6vectorIfSaIfEEC1EmRKS0_
.LEHE5:
	movq	$0, 88(%rsp)
	movq	$0, 96(%rsp)
	movl	$768, %edi
.LEHB6:
	call	_Znwm@PLT
.LEHE6:
	movq	%rax, %r14
	movq	%rax, 80(%rsp)
	leaq	768(%rax), %rax
	movq	%rax, 96(%rsp)
	movq	%rbx, %rdx
	movl	$32, %esi
	movq	%r14, %rdi
.LEHB7:
	call	_ZSt18__do_uninit_fill_nIPSt6vectorIfSaIfEEmS2_ET_S4_T0_RKT1_
.LEHE7:
	movq	%rax, 88(%rsp)
	movq	%rbx, %rdi
	call	_ZNSt6vectorIfSaIfEED1Ev
	movq	48(%rsp), %rbp
	movl	$0, %ebx
.L110:
	pxor	%xmm0, %xmm0
	cvtsi2ssl	%ebx, %xmm0
	mulss	.LC17(%rip), %xmm0
	call	sinf@PLT
	movss	%xmm0, 0(%rbp,%rbx,4)
	addq	$1, %rbx
	cmpq	$10000000, %rbx
	jne	.L110
	movq	%r14, %r13
	movq	%r14, %r12
	movl	$1, %r15d
.L113:
	movl	$0, %ebx
	pxor	%xmm2, %xmm2
	cvtsi2ssl	%r15d, %xmm2
	movss	%xmm2, (%rsp)
.L114:
	pxor	%xmm0, %xmm0
	cvtsi2ssl	%ebx, %xmm0
	mulss	.LC17(%rip), %xmm0
	mulss	(%rsp), %xmm0
	call	sinf@PLT
	movq	(%r12), %rax
	movss	%xmm0, (%rax,%rbx,4)
	addq	$1, %rbx
	cmpq	$10000000, %rbx
	jne	.L114
	addl	$1, %r15d
	addq	$24, %r12
	cmpl	$33, %r15d
	jne	.L113
	movq	$0, 120(%rsp)
	movq	$0, 128(%rsp)
	movl	$256, %edi
.LEHB8:
	call	_Znwm@PLT
.LEHE8:
	jmp	.L176
.L159:
	endbr64
	movq	%rax, %rbx
	movl	$768, %esi
	movq	%r14, %rdi
	call	_ZdlPvm@PLT
.L112:
	leaq	176(%rsp), %rdi
	call	_ZNSt6vectorIfSaIfEED1Ev
.L133:
	leaq	48(%rsp), %rdi
	call	_ZNSt6vectorIfSaIfEED1Ev
	movq	344(%rsp), %rax
	subq	%fs:40, %rax
	je	.L141
	call	__stack_chk_fail@PLT
.L176:
	movq	%rax, %r12
	movq	%rax, 112(%rsp)
	leaq	256(%rax), %rdx
	movq	%rdx, 128(%rsp)
	movq	$0, (%rax)
	leaq	8(%rax), %rax
.L116:
	movq	$0, (%rax)
	addq	$8, %rax
	cmpq	%rax, %rdx
	jne	.L116
	movq	%rdx, 120(%rsp)
	movq	%r12, %rax
	addq	$768, %r14
.L117:
	movq	0(%r13), %rdx
	movq	%rdx, (%rax)
	addq	$24, %r13
	addq	$8, %rax
	cmpq	%r13, %r14
	jne	.L117
	leaq	176(%rsp), %rdx
	leaq	144(%rsp), %rdi
	movl	$32, %esi
.LEHB9:
	call	_ZNSt6vectorIfSaIfEEC1EmRKS0_
.LEHE9:
	leaq	40(%rsp), %rdx
	leaq	176(%rsp), %rdi
	movl	$32, %esi
.LEHB10:
	call	_ZNSt6vectorIfSaIfEEC1EmRKS0_
.LEHE10:
	movq	144(%rsp), %r14
	movl	$10000000, %ecx
	movq	%r14, %rdx
	movq	%r12, %rsi
	movq	%rbp, %rdi
	call	_Z15correlate_fusedPKfPKS0_Pfi
	movq	176(%rsp), %r13
	movl	$10000000, %ecx
	movq	%r13, %rdx
	movq	%r12, %rsi
	movq	%rbp, %rdi
	call	_Z16correlate_split8PKfPKS0_Pfi
	leaq	.LC18(%rip), %rsi
	leaq	_ZSt4cout(%rip), %rdi
.LEHB11:
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	movl	$0, %ebx
	movb	$1, (%rsp)
	leaq	_ZSt4cout(%rip), %r15
	jmp	.L120
.L178:
	movl	%ebx, %esi
	movq	%r15, %rdi
	call	_ZNSolsEi@PLT
	movq	%rax, %rdi
	movl	$1, %edx
	leaq	.LC10(%rip), %rsi
	call	_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l@PLT
	movb	$0, (%rsp)
.L118:
	addq	$1, %rbx
	cmpq	$32, %rbx
	je	.L177
.L120:
	movss	(%r14,%rbx,4), %xmm0
	subss	0(%r13,%rbx,4), %xmm0
	andps	.LC19(%rip), %xmm0
	comiss	.LC17(%rip), %xmm0
	jbe	.L118
	movl	$9, %edx
	leaq	.LC20(%rip), %rsi
	movq	%r15, %rdi
	call	_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l@PLT
	jmp	.L178
.L177:
	leaq	.LC21(%rip), %rsi
	leaq	_ZSt4cout(%rip), %rdi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	movq	%rax, %rdi
	cmpb	$0, (%rsp)
	leaq	.LC5(%rip), %rsi
	leaq	.LC4(%rip), %rax
	cmovne	%rax, %rsi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	movq	%rax, %rdi
	leaq	.LC10(%rip), %rsi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	movl	$5, %ebx
	movq	$0x000000000, (%rsp)
	movsd	.LC7(%rip), %xmm4
	movsd	%xmm4, 8(%rsp)
.L123:
	call	_ZNSt6chrono3_V212system_clock3nowEv@PLT
	movq	%rax, %r13
	movq	144(%rsp), %r14
	movl	$10000000, %ecx
	movq	%r14, %rdx
	movq	%r12, %rsi
	movq	%rbp, %rdi
	call	_Z15correlate_fusedPKfPKS0_Pfi
	pxor	%xmm0, %xmm0
	cvtss2sd	(%r14), %xmm0
	addsd	(%rsp), %xmm0
	movsd	%xmm0, (%rsp)
	call	_ZNSt6chrono3_V212system_clock3nowEv@PLT
	subq	%r13, %rax
	pxor	%xmm0, %xmm0
	cvtsi2sdq	%rax, %xmm0
	divsd	.LC22(%rip), %xmm0
	minsd	8(%rsp), %xmm0
	movsd	%xmm0, 8(%rsp)
	subl	$1, %ebx
	jne	.L123
	movl	$5, %r13d
	movq	.LC7(%rip), %rbx
.L125:
	call	_ZNSt6chrono3_V212system_clock3nowEv@PLT
	movq	%rax, %r14
	movq	176(%rsp), %r15
	movl	$10000000, %ecx
	movq	%r15, %rdx
	movq	%r12, %rsi
	movq	%rbp, %rdi
	call	_Z17correlate_split16PKfPKS0_Pfi
	pxor	%xmm0, %xmm0
	cvtss2sd	(%r15), %xmm0
	addsd	(%rsp), %xmm0
	movsd	%xmm0, (%rsp)
	call	_ZNSt6chrono3_V212system_clock3nowEv@PLT
	subq	%r14, %rax
	pxor	%xmm0, %xmm0
	cvtsi2sdq	%rax, %xmm0
	divsd	.LC22(%rip), %xmm0
	movq	%rbx, %xmm4
	minsd	%xmm4, %xmm0
	movq	%xmm0, %rbx
	subl	$1, %r13d
	jne	.L125
	movl	$5, %r13d
	movsd	.LC7(%rip), %xmm5
	movsd	%xmm5, 24(%rsp)
.L127:
	call	_ZNSt6chrono3_V212system_clock3nowEv@PLT
	movq	%rax, %r14
	movq	176(%rsp), %r15
	movl	$10000000, %ecx
	movq	%r15, %rdx
	movq	%r12, %rsi
	movq	%rbp, %rdi
	call	_Z16correlate_split8PKfPKS0_Pfi
	pxor	%xmm0, %xmm0
	cvtss2sd	(%r15), %xmm0
	addsd	(%rsp), %xmm0
	movsd	%xmm0, (%rsp)
	call	_ZNSt6chrono3_V212system_clock3nowEv@PLT
	subq	%r14, %rax
	pxor	%xmm0, %xmm0
	cvtsi2sdq	%rax, %xmm0
	divsd	.LC22(%rip), %xmm0
	minsd	24(%rsp), %xmm0
	movsd	%xmm0, 24(%rsp)
	subl	$1, %r13d
	jne	.L127
	movl	$5, %r13d
	movsd	.LC7(%rip), %xmm4
	movsd	%xmm4, 16(%rsp)
.L129:
	call	_ZNSt6chrono3_V212system_clock3nowEv@PLT
	movq	%rax, %r14
	movq	176(%rsp), %r15
	movl	$10000000, %ecx
	movq	%r15, %rdx
	movq	%r12, %rsi
	movq	%rbp, %rdi
	call	_Z16correlate_split4PKfPKS0_Pfi
	pxor	%xmm0, %xmm0
	cvtss2sd	(%r15), %xmm0
	addsd	(%rsp), %xmm0
	movsd	%xmm0, (%rsp)
	call	_ZNSt6chrono3_V212system_clock3nowEv@PLT
	subq	%r14, %rax
	pxor	%xmm0, %xmm0
	cvtsi2sdq	%rax, %xmm0
	divsd	.LC22(%rip), %xmm0
	minsd	16(%rsp), %xmm0
	movsd	%xmm0, 16(%rsp)
	subl	$1, %r13d
	jne	.L129
	leaq	.LC10(%rip), %rsi
	leaq	_ZSt4cout(%rip), %rdi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	movq	%rax, %rbp
	leaq	208(%rsp), %rdi
	leaq	224(%rsp), %rax
	movq	%rax, 208(%rsp)
	movl	$45, %edx
	movl	$68, %esi
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE12_M_constructEmc@PLT
.LEHE11:
	movq	216(%rsp), %rdx
	movq	208(%rsp), %rsi
	movq	%rbp, %rdi
.LEHB12:
	call	_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l@PLT
	movq	%rax, %rdi
	leaq	.LC10(%rip), %rsi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
.LEHE12:
	leaq	208(%rsp), %rdi
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv@PLT
	leaq	.LC23(%rip), %rsi
	leaq	_ZSt4cout(%rip), %rdi
.LEHB13:
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	movq	%rax, %rdi
	movl	$10, %esi
	call	_ZNSolsEi@PLT
	movq	%rax, %rdi
	leaq	.LC24(%rip), %rsi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	leaq	208(%rsp), %rdi
	leaq	224(%rsp), %rax
	movq	%rax, 208(%rsp)
	movl	$45, %edx
	movl	$68, %esi
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE12_M_constructEmc@PLT
.LEHE13:
	movq	216(%rsp), %rdx
	movq	208(%rsp), %rsi
	leaq	_ZSt4cout(%rip), %rdi
.LEHB14:
	call	_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l@PLT
	movq	%rax, %rdi
	leaq	.LC10(%rip), %rsi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
.LEHE14:
	leaq	208(%rsp), %rdi
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv@PLT
	leaq	_ZSt4cout(%rip), %rdi
	movq	_ZSt4cout(%rip), %rdx
	movq	%rdi, %rcx
	addq	-24(%rdx), %rcx
	movl	24(%rcx), %eax
	andb	$79, %al
	orl	$32, %eax
	movl	%eax, 24(%rcx)
	movq	-24(%rdx), %rax
	movq	$34, 16(%rdi,%rax)
	leaq	.LC25(%rip), %rsi
.LEHB15:
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	movq	%rax, %rdi
	movq	(%rax), %rax
	movq	%rdi, %rdx
	addq	-24(%rax), %rdx
	movl	24(%rdx), %eax
	andb	$79, %al
	orb	$-128, %al
	movl	%eax, 24(%rdx)
	movq	(%rdi), %rax
	movq	-24(%rax), %rax
	movq	$12, 16(%rdi,%rax)
	leaq	.LC26(%rip), %rsi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	movq	%rax, %rdi
	movq	(%rax), %rax
	movq	%rdi, %rdx
	addq	-24(%rax), %rdx
	movl	24(%rdx), %eax
	andb	$79, %al
	orb	$-128, %al
	movl	%eax, 24(%rdx)
	movq	(%rdi), %rax
	movq	-24(%rax), %rax
	movq	$11, 16(%rdi,%rax)
	leaq	.LC27(%rip), %rsi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	movq	%rax, %rdi
	movq	(%rax), %rax
	movq	%rdi, %rdx
	addq	-24(%rax), %rdx
	movl	24(%rdx), %eax
	andb	$79, %al
	orb	$-128, %al
	movl	%eax, 24(%rdx)
	movq	(%rdi), %rax
	movq	-24(%rax), %rax
	movq	$10, 16(%rdi,%rax)
	leaq	.LC28(%rip), %rsi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	movq	%rax, %rdi
	leaq	.LC10(%rip), %rsi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	leaq	208(%rsp), %rdi
	leaq	224(%rsp), %rax
	movq	%rax, 208(%rsp)
	movl	$45, %edx
	movl	$68, %esi
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE12_M_constructEmc@PLT
.LEHE15:
	movq	216(%rsp), %rdx
	movq	208(%rsp), %rsi
	leaq	_ZSt4cout(%rip), %rdi
.LEHB16:
	call	_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l@PLT
	movq	%rax, %rdi
	leaq	.LC10(%rip), %rsi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
.LEHE16:
	leaq	208(%rsp), %rbp
	movq	%rbp, %rdi
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv@PLT
	pxor	%xmm0, %xmm0
	movups	%xmm0, 220(%rsp)
	movups	%xmm0, 236(%rsp)
	movups	%xmm0, 268(%rsp)
	movups	%xmm0, 280(%rsp)
	leaq	.LC29(%rip), %rax
	movq	%rax, 208(%rsp)
	movl	$32, 216(%rsp)
	leaq	.LC30(%rip), %rax
	movq	%rax, 232(%rsp)
	movl	$16, 240(%rsp)
	leaq	.LC31(%rip), %rax
	movq	%rax, 256(%rsp)
	movl	$8, 264(%rsp)
	leaq	.LC32(%rip), %rax
	movq	%rax, 280(%rsp)
	movl	$4, 288(%rsp)
	movsd	8(%rsp), %xmm7
	movsd	%xmm7, 224(%rsp)
	movq	%rbx, 248(%rsp)
	movsd	24(%rsp), %xmm3
	movsd	%xmm3, 272(%rsp)
	movsd	16(%rsp), %xmm4
	movsd	%xmm4, 296(%rsp)
	leaq	304(%rsp), %r14
	leaq	_ZSt4cout(%rip), %rbx
	leaq	.LC10(%rip), %r15
	jmp	.L132
.L180:
	movq	%rbx, %rdi
	addq	-24(%rdx), %rdi
	movl	32(%rdi), %esi
	orl	$1, %esi
.LEHB17:
	call	_ZNSt9basic_iosIcSt11char_traitsIcEE5clearESt12_Ios_Iostate@PLT
.L131:
	movq	(%rbx), %rdx
	movq	%rbx, %rcx
	addq	-24(%rdx), %rcx
	movl	24(%rcx), %eax
	andb	$79, %al
	orb	$-128, %al
	movl	%eax, 24(%rcx)
	movq	-24(%rdx), %rax
	movq	$12, 16(%rbx,%rax)
	movl	8(%r13), %esi
	movq	%rbx, %rdi
	call	_ZNSolsEi@PLT
	movq	%rax, %rdi
	movq	(%rax), %rax
	movq	%rdi, %rdx
	addq	-24(%rax), %rdx
	movl	24(%rdx), %eax
	andb	$79, %al
	orb	$-128, %al
	movl	%eax, 24(%rdx)
	movq	(%rdi), %rax
	movq	-24(%rax), %rax
	movq	$8, 16(%rdi,%rax)
	movq	(%rdi), %rax
	movq	%rdi, %rdx
	addq	-24(%rax), %rdx
	movl	24(%rdx), %eax
	andl	$-261, %eax
	orl	$4, %eax
	movl	%eax, 24(%rdx)
	movq	(%rdi), %rax
	movq	-24(%rax), %rax
	movq	$1, 8(%rdi,%rax)
	movq	16(%r13), %r13
	movq	%r13, %xmm0
	call	_ZNSo9_M_insertIdEERSoT_@PLT
	movq	%rax, %r12
	movl	$3, %edx
	leaq	.LC33(%rip), %rsi
	movq	%rax, %rdi
	call	_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l@PLT
	movq	(%r12), %rax
	movq	%r12, %rdx
	addq	-24(%rax), %rdx
	movl	24(%rdx), %eax
	andb	$79, %al
	orb	$-128, %al
	movl	%eax, 24(%rdx)
	movq	(%r12), %rax
	movq	-24(%rax), %rax
	movq	$9, 16(%r12,%rax)
	movq	(%r12), %rax
	movq	%r12, %rdx
	addq	-24(%rax), %rdx
	movl	24(%rdx), %eax
	andl	$-261, %eax
	orl	$4, %eax
	movl	%eax, 24(%rdx)
	movq	(%r12), %rax
	movq	-24(%rax), %rax
	movq	$2, 8(%r12,%rax)
	movsd	8(%rsp), %xmm0
	movq	%r13, %xmm5
	divsd	%xmm5, %xmm0
	movq	%r12, %rdi
	call	_ZNSo9_M_insertIdEERSoT_@PLT
	movq	%rax, %r12
	movl	$1, %edx
	leaq	.LC34(%rip), %rsi
	movq	%rax, %rdi
	call	_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l@PLT
	movl	$1, %edx
	movq	%r15, %rsi
	movq	%r12, %rdi
	call	_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l@PLT
	addq	$24, %rbp
	cmpq	%rbp, %r14
	je	.L179
.L132:
	movq	(%rbx), %rdx
	movq	%rbx, %rcx
	addq	-24(%rdx), %rcx
	movl	24(%rcx), %eax
	andb	$79, %al
	orl	$32, %eax
	movl	%eax, 24(%rcx)
	movq	-24(%rdx), %rax
	movq	$34, 16(%rbx,%rax)
	movq	%rbp, %r13
	movq	0(%rbp), %r12
	testq	%r12, %r12
	je	.L180
	movq	%r12, %rdi
	call	strlen@PLT
	movq	%rax, %rdx
	movq	%r12, %rsi
	movq	%rbx, %rdi
	call	_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l@PLT
	jmp	.L131
.L179:
	leaq	.LC35(%rip), %rsi
	leaq	_ZSt4cout(%rip), %rdi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	leaq	.LC36(%rip), %rsi
	leaq	_ZSt4cout(%rip), %rdi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
	movq	%rax, %rbx
	movq	(%rax), %rax
	movq	%rbx, %rdx
	addq	-24(%rax), %rdx
	movl	24(%rdx), %eax
	andl	$-261, %eax
	orl	$4, %eax
	movl	%eax, 24(%rdx)
	movq	(%rbx), %rax
	movq	-24(%rax), %rax
	movq	$2, 8(%rbx,%rax)
	call	_ZL14core_clock_ghzv
	movq	%rbx, %rdi
	call	_ZNSo9_M_insertIdEERSoT_@PLT
	movq	%rax, %rdi
	leaq	.LC37(%rip), %rsi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc@PLT
.LEHE17:
	movsd	(%rsp), %xmm6
	movsd	%xmm6, 40(%rsp)
	movsd	40(%rsp), %xmm0
	leaq	176(%rsp), %rdi
	call	_ZNSt6vectorIfSaIfEED1Ev
	leaq	144(%rsp), %rdi
	call	_ZNSt6vectorIfSaIfEED1Ev
	leaq	112(%rsp), %rdi
	call	_ZNSt6vectorIPKfSaIS1_EED1Ev
	leaq	80(%rsp), %rdi
	call	_ZNSt6vectorIS_IfSaIfEESaIS1_EED1Ev
	leaq	48(%rsp), %rdi
	call	_ZNSt6vectorIfSaIfEED1Ev
	movq	344(%rsp), %rax
	subq	%fs:40, %rax
	jne	.L181
	movl	$0, %eax
	addq	$360, %rsp
	.cfi_remember_state
	.cfi_def_cfa_offset 56
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%rbp
	.cfi_def_cfa_offset 40
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r13
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	ret
.L150:
	.cfi_restore_state
	endbr64
	movq	%rax, %rbx
	jmp	.L112
.L156:
	endbr64
	movq	%rax, %rbx
	leaq	208(%rsp), %rdi
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv@PLT
.L135:
	leaq	176(%rsp), %rdi
	call	_ZNSt6vectorIfSaIfEED1Ev
.L138:
	leaq	144(%rsp), %rdi
	call	_ZNSt6vectorIfSaIfEED1Ev
.L139:
	leaq	112(%rsp), %rdi
	call	_ZNSt6vectorIPKfSaIS1_EED1Ev
.L140:
	leaq	80(%rsp), %rdi
	call	_ZNSt6vectorIS_IfSaIfEESaIS1_EED1Ev
	jmp	.L133
.L157:
	endbr64
	movq	%rax, %rbx
	leaq	208(%rsp), %rdi
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv@PLT
	jmp	.L135
.L158:
	endbr64
	movq	%rax, %rbx
	leaq	208(%rsp), %rdi
	call	_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE10_M_disposeEv@PLT
	jmp	.L135
.L155:
	endbr64
	movq	%rax, %rbx
	jmp	.L135
.L154:
	endbr64
	movq	%rax, %rbx
	jmp	.L138
.L153:
	endbr64
	movq	%rax, %rbx
	jmp	.L139
.L152:
	endbr64
	movq	%rax, %rbx
	jmp	.L140
.L151:
	endbr64
	movq	%rax, %rbx
	jmp	.L133
.L141:
	movq	%rbx, %rdi
.LEHB18:
	call	_Unwind_Resume@PLT
.LEHE18:
.L105:
	leaq	208(%rsp), %rdx
	movl	$32, %ecx
	movl	$0, %eax
	movq	%rdx, %rdi
	rep stosl
	movl	$0, %eax
	movl	$0, 16(%rsp)
.L142:
	movq	%rax, %rsi
	shrq	$6, %rsi
	movl	$1, %edx
	movl	%eax, %ecx
	salq	%cl, %rdx
	orq	%rdx, 208(%rsp,%rsi,8)
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
