#if [[ ! -d "../lib" || ! -d "../../build" ]];then
#	echo "dependent dir don't exist"
#	cwd=$(pwd)
#	cwd=${cwd##*/}
#	cwd=${cwd%/}
#	if [[ $cwd != "command" ]];then
#		echo -e "you \d better in command dir\n"
#	fi
#	exit
#fi

BUILD_DIR="../build"
BOUT="string.o syscall.o stdio.o"
BIN="prog_no_arg.c string.c syscall.c stdio.c"
CFALGS="-m32 -Wall -c -fno-buildin -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers -fno-builtin"
LIB="./"
OBJS="string.o syscall.o  stdio.o"
#OBJS="$BUILD_DIR/init.o $BUILD_DIR/interrupt.o $BUILD_DIR/timer.o $BUILD_DIR/kernel.o \
#	   $BUILD_DIR/print.o $BUILD_DIR/debug.o $BUILD_DIR/string.o $BUILD_DIR/bitmap.o $BUILD_DIR/memory.o \
#	   $BUILD_DIR/list.o $BUILD_DIR/thread.o $BUILD_DIR/switch.o $BUILD_DIR/sync.o $BUILD_DIR/console.o \
#	   $BUILD_DIR/keyboard.o $BUILD_DIR/ioqueue.o $BUILD_DIR/tss.o $BUILD_DIR/process.o $BUILD_DIR/syscall.o \
#	   $BUILD_DIR/syscall-init.o $BUILD_DIR/stdio-kernel.o $BUILD_DIR/stdio.o $BUILD_DIR/ide.o $BUILD_DIR/file.o \
#	   $BUILD_DIR/fs.o $BUILD_DIR/inode.o $BUILD_DIR/dir.o $BUILD_DIR/fork.o "
DD_IN="prog_no_arg"
DD_OUT="/home/ankun/公共的/OS/hd60M.img"
gcc -Wall -m32 -c -fno-builtin -fno-stack-protector -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers -fno-builtin\
	  -I $LIB -o prog_no_arg.o prog_no_arg.c

gcc -Wall -m32 -c -fno-builtin -fno-stack-protector -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers -fno-builtin\
	  -I $LIB -o string.o string.c

gcc -Wall -m32 -c -fno-builtin -fno-stack-protector -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers -fno-builtin\
	  -I $LIB -o syscall.o syscall.c

gcc -Wall -m32 -c -fno-builtin -fno-stack-protector -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers -fno-builtin\
	  -I $LIB -o stdio.o stdio.c

nasm -f elf ./start.S -o ./start.o
ar rcs simple_crt.a $BOUT start.o

#ld -m elf_i386 -e main $BIN".o" $OBJS -o $BIN
ld -m elf_i386 -e main prog_no_arg.o simple_crt.a -o $DD_IN
SEC_CNT=$(ls -l $DD_IN|awk '{printf("%d", ($5+511)/512)}')

echo $SEC_CNT
if [[ -f $DD_IN ]];then
	dd if=./$DD_IN of=$DD_OUT bs=512 count=$SEC_CNT seek=300 conv=notrunc
fi
