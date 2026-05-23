; entry.asm
; Stage 2 entry point
; Switch 16-bit Real Mode to 32-bit Protected Mode, call bootloader_main() written in C

[BITS 16]

extern bootloader_main
extern __bss_start
extern __bss_end

global _start
global do_chainload
global do_reboot
global rm_call_int13

CODE_SEL equ 0x08
DATA_SEL equ 0x10

STACK_TOP equ 0x70000

%define SAVED_ESP_ADDR 0x1000

_start:
    cli

    mov [boot_drive_store], dl

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax

    mov sp, 0x7000

    ; Enable A20
    in al, 0x92
    or al, 00000010b
    and al, 11111110b
    out 0x92, al

    ; Load GDT
    lgdt [gdt_descriptor]

    ; Enter PM
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp CODE_SEL:protected_mode_entry

; Real-Mode print function
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

gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0
    db 10011010b
    db 11001111b
    db 0

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0
    db 10010010b
    db 11001111b
    db 0

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; 16-bit GDT
gdt16_start:

gdt16_null:
    dq 0

gdt16_code:
    dw 0xFFFF
    dw 0x0000
    db 0
    db 10011010b
    db 00001111b
    db 0

gdt16_data:
    dw 0xFFFF
    dw 0x0000
    db 0
    db 10010010b
    db 00001111b
    db 0

gdt16_end:

gdt16_descriptor:
    dw gdt16_end - gdt16_start - 1
    dd gdt16_start

boot_drive_store db 0

; Protected-Mode
[BITS 32]

protected_mode_entry:

    mov ax, DATA_SEL

    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, STACK_TOP

    ; Clear BSS
    mov edi, __bss_start

    mov ecx, __bss_end
    sub ecx, edi

    xor eax, eax

    shr ecx, 2

    rep stosd

    movzx eax, byte [boot_drive_store]

    push eax

    call bootloader_main

.dead:
    cli
    hlt
    jmp .dead

; Reboot
do_reboot:
    cli

.wait:
    in al, 0x64
    test al, 0x02
    jnz .wait

    mov al, 0xFE
    out 0x64, al

.hang:
    hlt
    jmp .hang

; Chainloading
do_chainload:
    cli

    ; use A20
    in al, 0x92
    or al, 00000010b
    out 0x92, al

    ; rebooting for chainloading
    mov al, 0xFE
    out 0x64, al

.hang:
    hlt
    jmp .hang

[BITS 16]

pm16_chain:

    mov ax, 0x10

    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov eax, cr0
    and eax, 0xFFFFFFFE
    mov cr0, eax

    jmp 0x0000:rm_chain

rm_chain:

    xor ax, ax

    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov sp, 0x7000

    ; Read original bootloader
    mov bx, 0x7A00

    mov ah, 0x02
    mov al, 1

    mov ch, 0
    mov cl, 63
    mov dh, 0

    mov dl, [boot_drive_store]

    int 0x13

    jc $

    mov dl, [boot_drive_store]

    jmp 0x0000:0x7A00

; INT13 Trampoline
[BITS 32]

rm_call_int13:

    mov [SAVED_ESP_ADDR], esp

    cli

    lgdt [gdt16_descriptor]

    jmp 0x08:pm16_int13

[BITS 16]

pm16_int13:

    mov ax, 0x10

    mov ds, ax
    mov es, ax
    mov ss, ax

    mov eax, cr0
    and eax, 0xFFFFFFFE
    mov cr0, eax

    jmp 0x0000:rm_int13

rm_int13:

    xor ax, ax

    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov sp, 0x7000

    mov si, 0x7E10

    mov ah, [0x7E00]
    mov dl, [0x7E01]

    int 0x13

    setc al

    mov [0x7E02], al
    mov [0x7E03], ah

    cli

    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp CODE_SEL:back32

[BITS 32]

back32:

    mov ax, DATA_SEL

    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, [SAVED_ESP_ADDR]

    ret