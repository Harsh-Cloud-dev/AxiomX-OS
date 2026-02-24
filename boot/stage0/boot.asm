BITS 16
ORG 0x7C00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [BOOT_DRIVE], dl

    ; print message
    mov si, msg
.print:
    lodsb
    or al, al
    jz load_stage1
    mov ah, 0x0E
    int 0x10
    jmp .print

load_stage1:
    mov bx, 0x8000       ; load address
    mov dh, 4            ; sectors to read
    mov dl, [BOOT_DRIVE]

    mov ah, 0x02         ; BIOS read sectors
    mov al, dh
    mov ch, 0x00
    mov cl, 0x02         ; sector 2 (after boot)
    mov dh, 0x00
    int 0x13
    jc disk_fail

    jmp 0x0000:0x8000

disk_fail:
    cli
    hlt

BOOT_DRIVE db 0
msg db "Stage0 OK", 0

times 510 - ($ - $$) db 0
dw 0xAA55