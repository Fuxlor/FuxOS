bits 16
org 0x7C00

PML4     equ 0x1000
PDPT     equ 0x2000
PAGE_DIR equ 0x3000

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

    ; Préparer la GDT
    cli
    lgdt [gdt_descriptor]

    ; Activer Protected Mode
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

    ; saut lointain pour recharger CS
    jmp 0x08:protected_mode


; ==================================================
; Chargement du kernel
; ==================================================

load_kernel:

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

gdt_null:
    dq 0

gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_code64:
    dw 0x0000
    dw 0x0000
    db 0x00
    db 10011010b
    db 00100000b
    db 0x00

gdt_end:


gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start


; ==================================================
; Protected Mode (32-bit)
; ==================================================

bits 32

protected_mode:

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x90000

    mov esi, protected_message
    mov edi, 0xB8000

.print:
    lodsb
    test al, al
    jz .after_print

    mov [edi], al
    inc edi
    mov byte [edi], 0x0F
    inc edi

    jmp .print

.after_print:
    call setup_long_mode

.halt:
    cli
    hlt
    jmp .halt


; ==================================================
; Setup Long Mode (PAE paging + EFER + saut 64-bit)
; ==================================================

setup_long_mode:

    ; --------------------------------------------------
    ; Vider PML4, PDPT, PAGE_DIR (3 x 4 Ko)
    ; --------------------------------------------------
    mov edi, PML4
    xor eax, eax
    mov ecx, 3 * 1024
    rep stosd

    ; --------------------------------------------------
    ; PML4[0] -> PDPT
    ; --------------------------------------------------
    mov eax, PDPT
    or  eax, 0x3
    mov [PML4], eax

    ; --------------------------------------------------
    ; PDPT[0] -> PAGE_DIR
    ; --------------------------------------------------
    mov eax, PAGE_DIR
    or  eax, 0x3
    mov [PDPT], eax

    ; --------------------------------------------------
    ; PAGE_DIR : 512 entrées, pages de 2 Mo, mapping 1:1
    ; --------------------------------------------------
    mov edi, PAGE_DIR
    xor eax, eax

.fill_pd:
    mov ebx, eax
    mov edx, 0x200000
    mul edx
    or  eax, 0x83
    mov [edi], eax
    mov dword [edi+4], 0

    mov eax, ebx
    add edi, 8
    inc eax
    cmp eax, 512
    jl .fill_pd

    ; --------------------------------------------------
    ; Activer PAE (CR4, bit 5)
    ; --------------------------------------------------
    mov eax, cr4
    or  eax, 0x20
    mov cr4, eax

    ; --------------------------------------------------
    ; Charger CR3 avec l'adresse du PML4
    ; --------------------------------------------------
    mov eax, PML4
    mov cr3, eax

    ; --------------------------------------------------
    ; Activer LME dans EFER (MSR 0xC0000080)
    ; --------------------------------------------------
    mov ecx, 0xC0000080
    rdmsr
    or  eax, 0x100
    wrmsr

    ; --------------------------------------------------
    ; Activer le paging (CR0, bit 31)
    ; --------------------------------------------------
    mov eax, cr0
    or  eax, 0x80000000
    mov cr0, eax

    ; --------------------------------------------------
    ; Afficher "Paging OK!" sur la ligne suivante
    ; --------------------------------------------------
    mov esi, paging_message
    mov edi, 0xB8000 + 160

.print_paging:
    lodsb
    test al, al
    jz .to_long_mode

    mov [edi], al
    inc edi
    mov byte [edi], 0x0F
    inc edi

    jmp .print_paging

.to_long_mode:

    ; saut lointain vers le segment de code 64-bit
    jmp 0x18:long_mode


; ==================================================
; Long Mode (64-bit)
; ==================================================

bits 64

long_mode:

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    mov rsi, longmode_message
    mov rdi, 0xB8000 + 320

.print_lm:
    lodsb
    test al, al
    jz .halt

    mov [rdi], al
    inc rdi
    mov byte [rdi], 0x0F
    inc rdi

    jmp .print_lm

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

paging_message:
    db "Paging OK!", 0


bits 64

longmode_message:
    db "FuxOS 64-bit!", 0


; ==================================================
; Padding
; ==================================================

bits 16

times 510 - ($ - $$) db 0
dw 0xAA55