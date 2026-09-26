bits 16
org 0x0000

start:
    mov si, message

.print:
    lodsb

    cmp al, 0
    je .halt

    mov ah, 0x0E
    int 0x10

    jmp .print

.halt:
    cli
    hlt
    jmp .halt

message:
    db "Hello from FuxOS kernel!", 0

times 512 - ($ - $$) db 0