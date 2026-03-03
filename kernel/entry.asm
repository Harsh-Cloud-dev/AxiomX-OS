[bits 64]
[global _start]
[extern kernel_main]

section .text
_start:
    mov rsp, 0x9FC00
    call kernel_main
.hang:
    hlt
    jmp .hang