bits 16
org 0x7C00

start:
    cli

    xor ax, ax
    mov ds, ax
    mov ss, ax

    mov sp, 0x7C00

    mov [boot_drive], dl

    call load_kernel

    ; Le kernel est à 0x1000:0000
    jmp 0x1000:0000


; --------------------------------------------------
; Charger le kernel
; --------------------------------------------------

load_kernel:

    ; ES:BX = adresse où écrire le kernel
    mov ax, 0x1000
    mov es, ax
    xor bx, bx

    ; BIOS INT 13h / AH=02h
    mov ah, 0x02
    mov al, 1
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]

    int 0x13

    jc disk_error

    jmp 0x1000:0000


; --------------------------------------------------
; Erreur disque
; --------------------------------------------------

disk_error:

    mov si, error_message

.print:
    lodsb

    cmp al, 0
    je $

    mov ah, 0x0E
    int 0x10

    jmp .print


boot_drive:
    db 0

error_message:
    db "Disk error!", 0


; --------------------------------------------------
; Signature boot
; --------------------------------------------------

times 510 - ($ - $$) db 0
dw 0xAA55