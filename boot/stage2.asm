bits 16
org 0x8000

PML4           equ 0x1000
PDPT           equ 0x2000
PAGE_DIR       equ 0x3000

KERNEL_SEG     equ 0x1000       ; 0x1000:0000 = adresse physique 0x10000
KERNEL_ADDR    equ 0x10000
KERNEL_LBA     equ 17           ; premier secteur du kernel sur le disque
KERNEL_SECTORS equ 32           ; 16 Ko max


; ==================================================
; Point d'entrée (mode réel 16-bit)
; ==================================================

stage2_start:
    cli
    xor ax, ax
    mov ds, ax
    mov [boot_drive], dl        ; DL est transmis par le stage 1
    sti                         ; le BIOS a besoin des interruptions pour int 0x13

    call load_kernel

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or  eax, 0x1
    mov cr0, eax

    jmp 0x08:protected_mode


; ==================================================
; Chargement du kernel (mode réel, via le BIOS)
; ==================================================

load_kernel:
    mov ax, KERNEL_SEG
    mov es, ax
    xor bx, bx                  ; ES:BX = destination
    mov word [lba], KERNEL_LBA
    mov cx, KERNEL_SECTORS      ; nombre de secteurs restants

.next_sector:
    push cx

    ; Conversion LBA -> CHS (disquette : 18 secteurs/piste, 2 têtes)
    mov ax, [lba]
    xor dx, dx
    mov cx, 18
    div cx                      ; ax = lba / 18, dx = lba % 18
    mov cl, dl
    inc cl                      ; secteur (commence à 1)
    mov dh, al
    and dh, 1                   ; tête = (lba / 18) % 2
    shr ax, 1
    mov ch, al                  ; cylindre = lba / 36
    mov dl, [boot_drive]

    mov ax, 0x0201              ; AH=02 (lire), AL=1 secteur
    int 0x13
    jc disk_error

    pop cx
    inc word [lba]
    add bx, 512
    loop .next_sector
    ret


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


boot_drive:         db 0
lba:                dw 0
disk_error_message: db "DISK ERROR!", 0


; ==================================================
; GDT
; ==================================================

gdt_start:

gdt_null:
    dq 0

gdt_code:                       ; 0x08 : code 32-bit
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00

gdt_data:                       ; 0x10 : données
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_code64:                     ; 0x18 : code 64-bit
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
; Paging PAE + EFER + passage en 64-bit
; ==================================================

setup_long_mode:

    ; Mettre à zéro PML4, PDPT, PAGE_DIR (3 x 4 Ko)
    mov edi, PML4
    xor eax, eax
    mov ecx, 3 * 1024
    rep stosd

    ; PML4[0] -> PDPT
    mov eax, PDPT
    or  eax, 0x3
    mov [PML4], eax

    ; PDPT[0] -> PAGE_DIR
    mov eax, PAGE_DIR
    or  eax, 0x3
    mov [PDPT], eax

    ; PAGE_DIR : 512 entrées de 2 Mo, mapping 1:1
    mov edi, PAGE_DIR
    xor eax, eax

.fill_pd:
    mov ebx, eax
    mov edx, 0x200000
    mul edx
    or  eax, 0x83               ; Present + Writable + Page Size (2 Mo)
    mov [edi], eax
    mov dword [edi+4], 0

    mov eax, ebx
    add edi, 8
    inc eax
    cmp eax, 512
    jl .fill_pd

    ; PAE (CR4 bit 5)
    mov eax, cr4
    or  eax, 0x20
    mov cr4, eax

    ; CR3 -> PML4
    mov eax, PML4
    mov cr3, eax

    ; LME (EFER bit 8)
    mov ecx, 0xC0000080
    rdmsr
    or  eax, 0x100
    wrmsr

    ; Paging (CR0 bit 31)
    mov eax, cr0
    or  eax, 0x80000000
    mov cr0, eax

    ; Afficher "Paging OK!" sur la ligne suivante
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
    jz .start_kernel

    mov [rdi], al
    inc rdi
    mov byte [rdi], 0x0F
    inc rdi
    jmp .print_lm

.start_kernel:
    mov rsp, 0x90000
    mov rax, KERNEL_ADDR
    call rax                    ; -> kmain()

.halt:
    cli
    hlt
    jmp .halt


; ==================================================
; Données
; ==================================================

protected_message:  db "Protected Mode!", 0
paging_message:     db "Paging OK!", 0
longmode_message:   db "FuxOS 64-bit!", 0


; ==================================================
; Remplissage à 16 secteurs (8 Ko), erreur de compilation si trop gros
; ==================================================

times 8192 - ($ - $$) db 0