[org  0x8000]
[bits 16]

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [BOOT_DRIVE], dl

    mov  si, msg_s1
    call print16

    ; Load Stage2 -> 0x9000
    xor ax, ax
    mov es, ax
    mov bx, 0x9000
    mov ah, 0x02
    mov al, 2
    mov ch, 0
    mov cl, 5
    mov dh, 0
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc  .err

    ; Load Stage3 -> 0xA000
    xor ax, ax
    mov es, ax
    mov bx, 0xA000
    mov ah, 0x02
    mov al, 2
    mov ch, 0
    mov cl, 9
    mov dh, 0
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc  .err

    in  al, 0x92
    or  al, 2
    out 0x92, al

    lgdt [gdt_desc]

    mov eax, cr0
    or  eax, 1
    mov cr0, eax
    jmp 0x08:pm_entry

.err:
    mov  si, msg_err
    call print16
.hang:
    cli
    hlt
    jmp .hang

print16:
    lodsb
    or  al, al
    jz  .done
    mov ah, 0x0E
    int 0x10
    jmp print16
.done:
    ret

align 8
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:

gdt_desc:
    dw gdt_end - gdt_start - 1
    dd gdt_start

BOOT_DRIVE db 0
msg_s1     db "S1OK", 13, 10, 0
msg_err    db "S1ERR", 13, 10, 0

[bits 32]
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov esp, 0x9FC00
    mov word [0xB8000], 0x2F50
    mov word [0xB8002], 0x2F4D
    jmp 0x9000