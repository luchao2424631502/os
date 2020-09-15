#!/bin/sh
nasm -f elf -o kernel.o kernel.asm
nasm -f elf -o string.o string.asm
nasm -f elf -o kliba.o kliba.asm 
gcc -m32 -c -fno-builtin -o start.o start.c 
ld -s -Ttext 0x30400 -m elf_i386 -o kernel.bin kernel.o string.o start.o kliba.o
