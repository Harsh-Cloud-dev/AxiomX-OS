org  0x9000
bits 32
KERNEL_LBA   equ 12
KERNEL_DEST  equ 0x100000
KERNEL_SECTS equ 137       
_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov esp, 0x9FC00
    lidt [idt_desc]
    mov word [0xB8004], 0x1F53
    mov word [0xB8006], 0x1F32
    call ata_load
    mov word [0xB8008], 0x1F4B
    mov word [0xB800A], 0x1F4C
    jmp 0xA000
ata_load:
    mov edi, KERNEL_DEST
    mov esi, KERNEL_LBA
    mov ecx, KERNEL_SECTS
.sector:
    push ecx
    push esi
    mov  dx, 0x1F7
.bsy:
    in   al, dx
    test al, 0x80
    jnz  .bsy
    mov dx, 0x1F2
    mov al, 1
    out dx, al
    mov eax, esi
    mov dx, 0x1F3
    out dx, al
    shr eax, 8
    mov dx, 0x1F4
    out dx, al
    shr eax, 8
    mov dx, 0x1F5
    out dx, al
    shr eax, 8
    and al, 0x0F
    or  al, 0xE0
    mov dx, 0x1F6
    out dx, al
    mov dx, 0x1F7
    mov al, 0x20
    out dx, al
.drq:
    in   al, dx
    test al, 0x08
    jz   .drq
    mov dx,  0x1F0
    mov ecx, 256
.word:
    in  ax, dx
    mov [edi], ax
    add edi, 2
    loop .word
    pop esi
    pop ecx
    inc esi
    loop .sector
    ret
idt_desc:
    dw 0
    dd 0