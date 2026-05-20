; mbr.asm
; MBR (Master Boot Record) code to load the second stage bootloader.
; Environment 16-bit Real Mode, BIOS loads this code at 0x7C00
; Size: Has to be 512 bytes, while the last two bytes is 0x55AA, the boot signature
; Functionality: Load stage 2 bootloader and jump to it.

[BITS 16]
[ORG 0x7C00]

STAGE2_LOAD_SEG  equ 0x0000
STAGE2_LOAD_OFF  equ 0x8000
STAGE2_SECTORS   equ 40
STAGE2_START_LBA equ 1

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    jmp 0x0000:main

main:
    mov [boot_drive], dl

    mov si, msg_mbr
    call print_string

    mov si, msg_loading
    call print_string

    mov ax, STAGE2_LOAD_SEG
    mov es, ax
    mov bx, STAGE2_LOAD_OFF

    mov ah, 0x02
    mov al, STAGE2_SECTORS
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13

    jc disk_error
    cmp al, STAGE2_SECTORS
    jne disk_error

    mov si, msg_ok
    call print_string

    mov dl, [boot_drive]
    jmp STAGE2_LOAD_SEG:STAGE2_LOAD_OFF

disk_error:
    mov si, msg_disk_err
    call print_string
    jmp hang

hang:
    cli
    hlt
    jmp hang

print_string:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
    int 0x10
    jmp .loop
.done:
    popa
    ret

boot_drive:  db 0

msg_mbr:        db "MBR: Booting...", 0x0D, 0x0A, 0
msg_loading:    db "MBR: Loading Stage2...", 0x0D, 0x0A, 0
msg_ok:         db "MBR: Stage2 loaded OK, jumping...", 0x0D, 0x0A, 0
msg_disk_err:   db "MBR: DISK READ ERROR!", 0x0D, 0x0A, 0

times 510 - ($ - $$) db 0
dw 0xAA55