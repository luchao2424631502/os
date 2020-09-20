HD	= ../80m.img
ASM	= nasm
CC	= gcc
LD	= ld
ASMFLAGS= -I ../include/ -f elf 
CFLAGS	= -m32 -I ../include/ -c -fno-builtin -fno-stack-protector -Wall
LDFLAGS	= -Ttext 0x1000 -m elf_i386
LIB	= ../lib/orangescrt.a
BIN	= echo pwd 

.PHONY: everything final clean realclean all install

everything: $(BIN)

install : all clean
	cp ../kernel.bin ./ -v
	tar vcf inst.tar kernel.bin $(BIN)
	dd if=inst.tar of=$(HD) seek=27131392 bs=1 count=92160 conv=notrunc

all : realclean everything

final: all clean

clean: 
	rm -f *.o

realclean:
	rm -f $(BIN) *.o

kernel.bin:
	cp ../kernel.bin ./

start.o: start.asm
	$(ASM) $(ASMFLAGS) -o $@ $<

echo.o : echo.c ../include/type.h ../include/stdio.h
	$(CC) $(CFLAGS) -o $@ $<

echo: echo.o start.o $(LIB)
	$(LD) $(LDFLAGS) -o $@ $?

pwd.o:pwd.c ../include/type.h ../include/stdio.h
	$(CC) $(CFLAGS) -o $@ $<

pwd:pwd.o start.o $(LIB)
	$(LD) $(LDFLAGS) -o $@ $?

