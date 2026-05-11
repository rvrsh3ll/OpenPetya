NASM := nasm
CC := gcc
LD := ld
QEMU := qemu-system-i386

MBR_DIR := mbr
BL_DIR := bootloader
BUILD := build

MBR_BIN := $(BUILD)/mbr.bin
STAGE2 := $(BUILD)/stage2.bin
DISK_IMG := $(BUILD)/disk.img

SRCS := $(wildcard $(BL_DIR)/*.c)
OBJS := $(patsubst $(BL_DIR)/%.c, $(BUILD)/%.o, $(SRCS))
ALL_OBJS := $(BUILD)/entry.o $(OBJS)

CFLAGS := -m32 -ffreestanding -nostdlib -nostartfiles -fno-stack-protector -fno-pic -O2 -Wall -Wextra
LDFLAGS := -m elf_i386 -T $(BL_DIR)/linker.ld

.PHONY: all clean run dirs

all: dirs $(DISK_IMG)
	@echo "Build complete! Run: make run"

dirs:
	@mkdir -p $(BUILD)

$(MBR_BIN): $(MBR_DIR)/mbr.asm
	$(NASM) -f bin -o $@ $<
	@[ $$(wc -c < $@) -eq 512 ] || (echo "MBR not 512 bytes!" && exit 1)

$(BUILD)/entry.o: $(BL_DIR)/entry.asm
	$(NASM) -f elf32 -o $@ $<

$(BUILD)/%.o: $(BL_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(STAGE2): $(ALL_OBJS)
	@echo "Linking stage2 as flat binary..."
	$(LD) -m elf_i386 -T $(BL_DIR)/linker.ld -o $@ --oformat binary $^
	@size=$$(stat -c%s $@); \
	if [ $$size -lt 8192 ]; then \
		dd if=/dev/zero bs=1 count=$$((8192-$$size)) >> $@; \
	fi

$(DISK_IMG): $(MBR_BIN) $(STAGE2)
	dd if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	dd if=$(MBR_BIN) of=$@ seek=0 bs=512 conv=notrunc 2>/dev/null
	dd if=$(STAGE2) of=$@ seek=1 bs=512 conv=notrunc 2>/dev/null

run: $(DISK_IMG)
	$(QEMU) -drive file=$(DISK_IMG),format=raw,if=ide -m 32M -no-reboot -no-shutdown -serial stdio

clean:
	rm -rf $(BUILD)