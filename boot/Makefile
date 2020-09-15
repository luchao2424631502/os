#用来编译boot和loader
ASM		= nasm 
ASMFLAGS= -I include/

TARGET	= boot.bin loader.bin 

.PHONY: everything clean all

everything:$(TARGET)

clean:
	rm -f $(TARGET)

all:clean everything

boot.bin: boot.asm include/load.inc include/fat12hdr.inc 
	$(ASM) $(ASMFLAGS) -o $@ $<

loader.bin:loader.asm include/load.inc include/fat12hdr.inc include/pm.inc
	$(ASM) $(ASMFLAGS) -o $@ $<