;;
;; Copyright (c) 2018, Intel Corporation
;;
;; Redistribution and use in source and binary forms, with or without
;; modification, are permitted provided that the following conditions are met:
;;
;;     * Redistributions of source code must retain the above copyright notice,
;;       this list of conditions and the following disclaimer.
;;     * Redistributions in binary form must reproduce the above copyright
;;       notice, this list of conditions and the following disclaimer in the
;;       documentation and/or other materials provided with the distribution.
;;     * Neither the name of Intel Corporation nor the names of its contributors
;;       may be used to endorse or promote products derived from this software
;;       without specific prior written permission.
;;
;; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
;; AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
;; IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
;; DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
;; FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
;; DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
;; SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
;; CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
;; OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
;; OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;;

%ifndef _AESNI_EMU_INC_
%define _AESNI_EMU_INC_

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Utility macros and defines to assist AESNI translation macros
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

%define GP0  rax
%define GP1  rbx
%define GP2  rcx
%define GP3  rdx
%define GP4  rbp
%define GP5  rsi
%define GP6  rdi
%define GP7  r8
%define GP8  r9
%define GP9  r10
%define GP10 r11
%define GP11 r12
%define GP12 r13
%define GP13 r14
%define GP14 r15
%define NUM_GP_REGS 15

%define ARG1_OFFSET 8*16
%define ARG2_OFFSET 8*18

%define GP(x) GP %+ x
%define XMM(x) xmm  %+ x

;; Reserve enough stack space to store all GP
;; registers and emulation function arguments
;; e.g. void emulate_AESXXX(xmm_reg *dst, xmm_reg *src);
%define RES_STACK_SZ (ARG2_OFFSET + 16)

;; Allocate stack space and save GP registers
%macro SAVE_GP_REGS 0
        push    rax
        mov     rax, rsp
        sub	rsp, RES_STACK_SZ
        and     rsp, -16
%assign i 0
%rep    NUM_GP_REGS
        mov	[rsp + 8*i], GP(i)
%assign i i+1
%endrep
%endmacro

;; Restore GP registers and stack pointer
%macro RESTORE_GP_REGS 0
%assign i 0
%rep    NUM_GP_REGS
        mov	GP(i), [rsp + 8*i]
%assign i i+1
%endrep
        mov     rsp, rax
        pop     rax
%endmacro


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Generic macro to translate AESNI instructions to AESNI emulation functions
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

%macro EMULATE_AESNI 4
%define %%func    %1
%define %%src_dst %2
%define %%key     %3
%define %%imm     %4

%ifdef LINUX
%define %%arg1 rdi
%define %%arg2 rsi
%define %%arg3 rdx
%else
%define %%arg1 rcx
%define %%arg2 rdx
%define %%arg3 r8
%endif

;; Check if key is reg or ptr
%assign IS_REG 0
%assign i 0
%rep    16
%ifidni %%key, XMM(i)
       %assign IS_REG 1
       %exitrep
%endif
%assign i i+1
%endrep
        ;; save GP registers to stack
        SAVE_GP_REGS

;; move XMM registers to stack
%if IS_REG
        ;; move 32 bytes on stack for src_dst and key
        movdqa  [rsp + ARG1_OFFSET], %%src_dst
        movdqa  [rsp + ARG2_OFFSET], %%key
        lea     %%arg1, [rsp + ARG1_OFFSET]
        lea     %%arg2, [rsp + ARG2_OFFSET]
%else
        ;; move 16 bytes on stack for src_dst
        movdqa  [rsp + ARG1_OFFSET], %%src_dst
        lea     %%arg1, [rsp + ARG1_OFFSET]
        lea     %%arg2, %%key
%endif

        ;; move 8 bit imm rcon for aeskeygenassist
%ifnum  %%imm
        mov     BYTE(%%arg3), %%imm
%endif

;; reserve space on stack for up to 4 arguments on the stack (windows only)
%ifndef LINUX
        sub     rsp, 32
%endif
        ;; call emulation function
        call    %%func
%ifndef LINUX
        add     rsp, 32
%endif
        movdqa  %%src_dst, [rsp + ARG1_OFFSET]

        RESTORE_GP_REGS
%endmacro


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Macros to translate AESNI instructions to AESNI emulation functions
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; AESENC translation macro
%macro EMULATE_AESENC 2
%define %%src_dst %1
%define %%key     %2
        EMULATE_AESNI emulate_AESENC, %%src_dst, %%key, ""
%endmacro

;; AESENCLAST translation macro
%macro EMULATE_AESENCLAST 2
%define %%src_dst %1
%define %%key     %2
        EMULATE_AESNI emulate_AESENCLAST, %%src_dst, %%key, ""
%endmacro

;; AESDEC translation macro
%macro EMULATE_AESDEC 2
%define %%src_dst %1
%define %%key     %2
        EMULATE_AESNI emulate_AESDEC, %%src_dst, %%key, ""
%endmacro

;; AESDECLAST translation macro
%macro EMULATE_AESDECLAST 2
%define %%src_dst %1
%define %%key     %2
        EMULATE_AESNI emulate_AESDECLAST, %%src_dst, %%key, ""
%endmacro

;; AESIMC translation macro
%macro EMULATE_AESIMC 2
%define %%src_dst %1
%define %%key     %2
        EMULATE_AESNI emulate_AESIMC, %%src_dst, %%key, ""
%endmacro

;; AESKEYGENASSIST translation macro
%macro EMULATE_AESKEYGENASSIST 3
%define %%src_dst %1
%define %%key     %2
%define %%imm     %3
        EMULATE_AESNI emulate_AESKEYGENASSIST, %%src_dst, %%key, %%imm
%endmacro


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; AESNI defines
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

%define aesenc          EMULATE_AESENC
%define aesenclast      EMULATE_AESENCLAST
%define aesdec          EMULATE_AESDEC
%define aesdeclast      EMULATE_AESDECLAST
%define aesimc          EMULATE_AESIMC
%define aeskeygenassist EMULATE_AESKEYGENASSIST

extern emulate_AESENC
extern emulate_AESENCLAST
extern emulate_AESDEC
extern emulate_AESDECLAST
extern emulate_AESIMC
extern emulate_AESKEYGENASSIST

%endif ; end ifndef _AESNI_EMU_INC_