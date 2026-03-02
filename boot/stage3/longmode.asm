org  0xA000
bits 32

%define PML4 0x1000
%define PDPT 0x2000
%define PD   0x3000

_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    ; zero page tables
    xor  eax, eax
    mov  edi, PML4
    mov  ecx, (3 * 4096) / 4
    rep  stosd

    mov dword [PML4],   (PDPT | 3)
    mov dword [PML4+4], 0
    mov dword [PDPT],   (PD | 3)
    mov dword [PDPT+4], 0
    mov dword [PD],     0x83
    mov dword [PD+4],   0

    mov eax, cr4
    or  eax, (1 << 5)
    mov cr4, eax

    mov eax, PML4
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or  eax, (1 << 8)
    wrmsr

    mov eax, cr0
    or  eax, 0x80000000
    mov cr0, eax

    lgdt [gdt64_desc]
    jmp  dword 0x08:lm_entry

align 8
gdt64_start:
    dq 0
    dq 0x00209A0000000000
    dq 0x0000920000000000
gdt64_end:

gdt64_desc:
    dw gdt64_end - gdt64_start - 1
    dd gdt64_start

bits 64
lm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    xor ax, ax
    mov fs, ax
    mov gs, ax
    mov rsp, 0x9FC00

    mov word [0xB800C], 0x1F4C
    mov word [0xB800E], 0x1F4D

    mov rax, 0x100000
    jmp rax