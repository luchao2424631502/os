ASM     =nasm
ASMFLAGS=-I boot/include/

ORANGESBOOT = boot/boot.bin boot/loader.bin 

.PHONY:everything clean all 

everything: $(ORANGESBOOT)

clean:
	rm -f $(ORANGESBOOT)

all:clean everything

boot/boot.bin: boot/boot.asm boot/include/load.inc boot/include/fat12hdr.inc 
	$(ASM) $(ASMFLAGS) -o $@ $<

boot/loader.bin:boot/loader.asm boot/include/load.inc \
	boot/include/fat12hdr.inc boot/include/pm.inc
	$(ASM) $(ASMFLAGS) -o $@ $<
