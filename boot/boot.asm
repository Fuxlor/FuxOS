bits 16
org 0x7C00

start:
    cli

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov [boot_drive], dl

    ; Charger le kernel
    call load_kernel

    ; Préparer la GDT Global Descriptor Table
    cli
    lgdt [gdt_descriptor]

    ; Activer Protected Mode
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

    ; IMPORTANT :
    ; saut lointain pour recharger CS
    jmp 0x08:protected_mode


; ==================================================
; Chargement du kernel
; ==================================================

load_kernel:

    ; ES:BX = 0x1000:0000
    mov ax, 0x1000
    mov es, ax
    xor bx, bx

    mov ah, 0x02
    mov al, 1
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]

    int 0x13

    jc disk_error

    ret


disk_error:

    mov si, disk_error_message

.print:
    lodsb
    test al, al
    jz $

    mov ah, 0x0E
    int 0x10

    jmp .print


; ==================================================
; GDT
; ==================================================

gdt_start:

; --------------------------------------------------
; Null descriptor
; --------------------------------------------------

gdt_null:
    dq 0


; --------------------------------------------------
; Code segment
; --------------------------------------------------

gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00

    ; Present
    ; Ring 0
    ; Code
    ; Readable
    db 10011010b

    ; 32-bit
    ; Granularity = 4 KiB
    db 11001111b

    db 0x00


; --------------------------------------------------
; Data segment
; --------------------------------------------------

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00

    ; Present
    ; Ring 0
    ; Data
    ; Writable
    db 10010010b

    db 11001111b

    db 0x00


gdt_end:


; ==================================================
; GDT Descriptor
; ==================================================

gdt_descriptor:

    ; Taille de la GDT - 1
    dw gdt_end - gdt_start - 1

    ; Adresse de la GDT
    dd gdt_start


; ==================================================
; Protected Mode
; ==================================================

bits 32

protected_mode:

    ; Data segment = 0x10
    mov ax, 0x10

    mov ds, ax
    mov es, ax
    mov ss, ax

    mov esp, 0x90000

    ; Maintenant on est réellement en 32-bit.

    mov esi, protected_message
    mov edi, 0xB8000

.print:

    lodsb

    test al, al
    jz .halt

    mov [edi], al
    inc edi

    ; Attribut texte
    mov byte [edi], 0x0F
    inc edi

    jmp .print


.halt:

    cli
    hlt
    jmp .halt


; ==================================================
; Messages
; ==================================================

bits 16

boot_drive:
    db 0

disk_error_message:
    db "DISK ERROR!", 0


bits 32

protected_message:
    db "Protected Mode!", 0


; ==================================================
; Padding
; ==================================================

bits 16

times 510 - ($ - $$) db 0
dw 0xAA55