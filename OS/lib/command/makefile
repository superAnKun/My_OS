BUILD_DIR = .
ENTRY_POINT = 0x8048000
AS = nasm
CC = gcc
LD = ld
LIB = -I lib/ -I lib/kernel/ -I lib/device/ -I lib/thread
DD_OUT=/home/ankun/公共的/OS/hd60M.img
ASFLAGS = -f elf
CFLAGS = -Wall -m32 -c -fno-stack-protector -W -Wstrict-prototypes \
		 -Wmissing-prototypes -fno-builtin -Wsystem-headers
CFLAGS2 = -m32  -c -fno-builtin
#LDFLAGS = -m elf_i386 -Ttext $(ENTRY_POINT) -e main -Map $(BUILD_DIR)/kernel.map
LDFLAGS = -m elf_i386 -e _start -Map $(BUILD_DIR)/kernel.map
OBJS = $(BUILD_DIR)/string.o $(BUILD_DIR)/syscall.o $(BUILD_DIR)/stdio.o $(BUILD_DIR)/start.o

$(BUILD_DIR)/stdio.o: ./stdio.c ./stdio.h ./string.h ./syscall.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/syscall.o: ./syscall.c ./syscall.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/string.o: ./string.c ./string.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/prog_no_arg.o: ./prog_no_arg.c ./stdio.h ./syscall.h ./string.h
	$(CC) $(CFLAGS) $< -o $@


#ld -m elf_i386 -e main prog_no_arga.o simple_crt.a -o $DD_IN

#SEC_CNT=$(ls -l $DD_IN|awk '{printf("%d", ($5+511)/512)}')

$(BUILD_DIR)/start.o: start.S 
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/simple_crt.a: $(OBJS)
	ar rcs simple_crt.a $(OBJS)

$(BUILD_DIR)/prog_no_arg: $(BUILD_DIR)/prog_no_arg.o $(BUILD_DIR)/simple_crt.a
	$(LD) $(LDFLAGS) $^ -o $@

hd:
	dd if=$(BUILD_DIR)/prog_no_arg of=$(DD_OUT) bs=512 count=20 seek=300 conv=notrunc

build: $(BUILD_DIR)/prog_no_arg

all:
	make build 
	make hd
