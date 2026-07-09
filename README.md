# Litad Assembly (LASM)

A powerful x86_64 assembler with custom syntax, full Long Mode support, and cross-platform compatibility. LASM is designed for system programming, operating system development, and bare-metal applications.

## Features

- **Custom Syntax** - Unique, intuitive assembly language designed for readability
- **Long Mode Support** - Full 64-bit mode with paging, control registers, and system instructions
- **time_t Integration** - Built-in timestamp support for reproducible builds
- **Cross-Platform Output** - Generate raw binaries, ELF64 executables, and boot sectors
- **Multiple CPU Support** - Intel, AMD, Elbrus, Baikal, and MCST processors
- **Single-Pass Assembler** - Fast compilation with label resolution
- **Embedded Development** - Bootable image generation for OS development
- **Package Distribution** - Native packages for Debian, Fedora, Arch Linux, and Windows

## Installation FROM SOURCE

```bash
git clone https://github.com/yourusername/lasm.git
cd lasm
./build_all.sh
```
