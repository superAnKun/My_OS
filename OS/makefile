BUILD_DIR = ./build
ENTRY_POINT = 0xc0001500
AS = nasm
CC = gcc
LD = ld
LIB = -I lib/ -I lib/kernel/ -I lib/device/ -I lib/thread
ASFLAGS = -f elf
CFLAGS = -Wall $(LIB) -m32 -c -fno-stack-protector -W -Wstrict-prototypes \
		 -Wmissing-prototypes -fno-builtin 
CFLAGS2 = -m32  -c -fno-builtin
LDFLAGS = -m elf_i386 -Ttext $(ENTRY_POINT) -e main -Map $(BUILD_DIR)/kernel.map
OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/init.o $(BUILD_DIR)/interrupt.o $(BUILD_DIR)/timer.o $(BUILD_DIR)/kernel.o \
	   $(BUILD_DIR)/print.o $(BUILD_DIR)/debug.o $(BUILD_DIR)/string.o $(BUILD_DIR)/bitmap.o $(BUILD_DIR)/memory.o \
	   $(BUILD_DIR)/list.o $(BUILD_DIR)/thread.o $(BUILD_DIR)/switch.o $(BUILD_DIR)/sync.o $(BUILD_DIR)/console.o \
	   $(BUILD_DIR)/keyboard.o

$(BUILD_DIR)/main.o: lib/kernel/main.c lib/kernel/print.h lib/stdint.h lib/kernel/init.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/init.o: lib/kernel/init.c lib/kernel/init.h lib/kernel/print.h lib/stdint.h lib/kernel/interrupt.h lib/device/timer.h  \
	lib/kernel/memory.h lib/thread/thread.h lib/device/keyboard.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/interrupt.o: lib/kernel/interrupt.c lib/kernel/interrupt.h lib/kernel/global.h lib/stdint.h lib/kernel/io.h \
	lib/kernel/print.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/timer.o: lib/device/timer.c lib/device/timer.h lib/kernel/io.h lib/stdint.h lib/kernel/print.h  \
	lib/kernel/interrupt.h  lib/thread/thread.h  lib/kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/debug.o: lib/kernel/debug.c lib/kernel/debug.h lib/kernel/print.h lib/kernel/interrupt.h lib/stdint.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/memory.o: lib/kernel/memory.c lib/kernel/debug.h lib/kernel/print.h lib/stdint.h lib/kernel/bitmap.h \
	lib/kernel/memory.h lib/kernel/string.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/string.o: lib/kernel/string.c lib/kernel/debug.h lib/kernel/global.h lib/stdint.h  lib/kernel/string.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/bitmap.o: lib/kernel/bitmap.c lib/kernel/debug.h lib/kernel/string.h lib/stdint.h lib/kernel/interrupt.h lib/kernel/print.h \
	lib/kernel/global.h lib/kernel/bitmap.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/thread.o: lib/thread/thread.c lib/thread/thread.h lib/kernel/string.h lib/stdint.h lib/kernel/global.h lib/kernel/memory.h \
	lib/kernel/interrupt.h  lib/kernel/list.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/list.o: lib/kernel/list.c lib/kernel/list.h lib/kernel/global.h
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/sync.o: lib/thread/sync.c lib/thread/sync.h lib/kernel/print.h lib/kernel/list.h lib/stdint.h lib/kernel/interrupt.h \
	lib/kernel/list.h lib/kernel/global.h lib/thread/thread.h lib/kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/console.o: lib/device/console.c  lib/device/console.h lib/kernel/print.h lib/stdint.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/keyboard.o: lib/device/keyboard.c lib/device/keyboard.h lib/stdint.h lib/kernel/print.h lib/kernel/interrupt.h \
	lib/kernel/io.h lib/kernel/global.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/kernel.o: lib/kernel/kernel.S
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/print.o: lib/kernel/print.S
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/switch.o: lib/thread/switch.S
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@



hd: dd if=$(BUILD_DIR)/kernel.bin of=hd60M.img bs=512 count=200 seek=10 conv=notrunc

build: $(BUILD_DIR)/kernel.bin

