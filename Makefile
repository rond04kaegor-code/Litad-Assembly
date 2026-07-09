# Makefile for LASM
CC = gcc
CFLAGS = -O2 -Wall -Wextra
TARGET = lasm
LIB = liblasm

.PHONY: all clean install build_all test

all: lasm liblasm.so liblasm.a test.bin

lasm: lasm.c
	$(CC) $(CFLAGS) -o $@ $< -static

liblasm.o: liblasm.c liblasm.h
	$(CC) $(CFLAGS) -c -fPIC $<

liblasm.so: liblasm.o
	$(CC) -shared -o $@ $<

liblasm.a: liblasm.o
	ar rcs $@ $<

test.bin: lasm test.asm
	./lasm test.asm test.bin
	./lasm test.asm test.elf --elf

install:
	install -m 755 lasm /usr/local/bin/
	install -m 644 liblasm.so /usr/local/lib/
	install -m 644 liblasm.a /usr/local/lib/
	install -m 644 liblasm.h /usr/local/include/

build_all:
	./build_all.sh

clean:
	rm -f lasm lasm_clang *.o *.so *.a *.bin *.elf *.img
	rm -f *.deb *.rpm *.pkg.tar.* *.exe
	rm -rf build/
