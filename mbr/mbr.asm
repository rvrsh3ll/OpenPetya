; mbr.asm
; MBR (Master Boot Record) code to load the second stage bootloader.
; Environment 16-bit Real Mode, BIOS loads this code at 0x7C00
; Size: Has to be 512 bytes, while the last two bytes is 0x55AA, the boot signature
; Functionality: Load stage 2 bootloader and jump to it.

[BITS 16]
[ORG 0x7C00]

STAGE2_LOAD_SEG  equ 0x0000
STAGE2_LOAD_OFF  equ 0x8000

; 64 sectors = 32KB
STAGE2_SECTORS   equ 64

STAGE2_START_LBA equ 1

STACK_TOP        equ 0x7000

start:
    cli

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax

    mov sp, STACK_TOP

    jmp 0x0000:main

main:
    mov [boot_drive], dl

    mov si, msg_mbr
    call print_string

    ; Check INT13 Extensions
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]

    int 0x13
    jc no_ext

    cmp bx, 0xAA55
    jne no_ext

    mov si, msg_loading
    call print_string

    ; DAP structure
    mov si, dap

    mov ah, 0x42
    mov dl, [boot_drive]

    int 0x13
    jc disk_error

    mov si, msg_ok
    call print_string

    mov dl, [boot_drive]

    jmp STAGE2_LOAD_SEG:STAGE2_LOAD_OFF

no_ext:
    mov si, msg_no_ext
    call print_string
    jmp hang

disk_error:
    mov si, msg_disk_err
    call print_string
    jmp hang

hang:
    cli

.hlt_loop:
    hlt
    jmp .hlt_loop

print_string:
    pusha

.loop:
    lodsb
    test al, al
    jz .done

    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x07

    int 0x10

    jmp .loop

.done:
    popa
    ret

; Disk Address Packet
dap:
    db 0x10
    db 0x00
    dw STAGE2_SECTORS
    dw STAGE2_LOAD_OFF
    dw STAGE2_LOAD_SEG
    dd STAGE2_START_LBA
    dd 0

boot_drive db 0

msg_mbr      db "MBR: Booting...",13,10,0
msg_loading  db "MBR: Loading Stage2...",13,10,0
msg_ok       db "MBR: Jumping to Stage2...",13,10,0
msg_disk_err db "MBR: Disk read error!",13,10,0
msg_no_ext   db "MBR: INT13 extensions missing!",13,10,0

times 510-($-$$) db 0
dw 0xAA55