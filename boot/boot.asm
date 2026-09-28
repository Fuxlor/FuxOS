bits 16
org 0x7C00

STAGE2_ADDR    equ 0x8000
STAGE2_SECTORS equ 16

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    ; Lire STAGE2_SECTORS secteurs (à partir du secteur 2) vers 0x0000:0x8000
    mov bx, STAGE2_ADDR
    mov ah, 0x02
    mov al, STAGE2_SECTORS
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    ; Sauter dans le stage 2
    mov dl, [boot_drive]
    jmp 0x0000:STAGE2_ADDR


disk_error:
    mov si, disk_error_message

.print:
    lodsb
    test al, al
    jz .halt
    mov ah, 0x0E
    int 0x10
    jmp .print

.halt:
    cli
    hlt
    jmp .halt


boot_drive:
    db 0

disk_error_message:
    db "DISK ERROR!", 0

times 510 - ($ - $$) db 0
dw 0xAA55