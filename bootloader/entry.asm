; entry.asm
; Stage 2 entry point
; Switch 16-bit Real Mode to 32-bit Protected Mode, call bootloader_main() written in C

[BITS 16]

extern bootloader_main
extern __bss_start
extern __bss_end

global _start

_start:
    mov [boot_drive_store], dl

    mov si, msg_stage2
    call print_string

    cli

    ; call A32 line via port 20
    in al, 0x92
    or al, 2
    out 0x92, al

    ; load GDT
    lgdt [gdt_descriptor]

    ; Set CR0.PE = 1, entering Protected Mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; far jump: clear all pipelines and load 32-bit code segment selector
    jmp 0x08:protected_mode_entry

print_string:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp .loop
.done:
    popa
    ret

; GDT
gdt_start:

gdt_null:
    dq 0

; Code Segment (selector 0x08): Base=0, Limit=4GB, Ring0, 32-bit
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

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

boot_drive_store: db 0
msg_stage2: db "Stage2: Entry (16-bit OK)", 0x0D, 0x0A, 0

; 32-bit Protected Mode
[BITS 32]

protected_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, 0x90000 ; stack

    ; clear BSS (C environment requires all global variables to be zero)
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    rep stosd

    movzx eax, byte [boot_drive_store]
    push eax
    call bootloader_main

    cli

.hang:
    hlt
    jmp .hang