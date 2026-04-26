[bits 64]
[global _start]
[extern kernel_main]
[extern bss_start]
[extern kernel_end]

section .text
_start:
    mov rsp, 0x200000

    ; zero BSS before any C code runs
    mov rdi, bss_start
    mov rcx, kernel_end
    sub rcx, rdi
    test rcx, rcx
    jz .bss_done
    shr rcx, 3
    xor rax, rax
    rep stosq
.bss_done:
    call kernel_main
.hang:
    hlt
    jmp .hang