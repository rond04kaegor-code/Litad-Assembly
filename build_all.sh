#!/bin/bash

# build_all.sh - LASM Build System
# Creates packages for Debian, Fedora, Arch, and Windows

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

VERSION="1.0"
PACKAGE_NAME="lasm"

echo "LASM Build System v${VERSION}"
echo "Building from: $SCRIPT_DIR"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

clean() {
    echo "Cleaning..."
    rm -rf build/ *.deb *.rpm *.pkg.tar.zst *.exe *.bin *.elf *.o *.so *.a
    rm -rf lasm_*.deb lasm-*.rpm lasm-*.pkg.tar*
}

build_compiler() {
    echo "Building LASM Compiler..."
    
    if command -v gcc &> /dev/null; then
        gcc -O2 -Wall -Wextra -o lasm lasm.c -static
        echo -e "${GREEN}Built with GCC${NC}"
    else
        echo -e "${RED}GCC not found${NC}"
        exit 1
    fi
    
    if command -v clang &> /dev/null; then
        clang -O2 -Wall -Wextra -o lasm_clang lasm.c -static
        echo -e "${GREEN}Built with Clang${NC}"
    fi
}

build_library() {
    echo "Building liblasm..."
    
    gcc -c -fPIC -O2 liblasm.c -o liblasm.o
    gcc -shared -o liblasm.so liblasm.o
    echo -e "${GREEN}liblasm.so built${NC}"
    
    ar rcs liblasm.a liblasm.o
    echo -e "${GREEN}liblasm.a built${NC}"
}

test_build() {
    echo "Testing LASM..."
    
    cat > test.asm << 'EOF'
BITS 64
[ORG 0x7990]

ECHO "HELLO WORLD FROM LASM!!!!!!!!!!"
VERSION

start:
    MOV RAX, 0x12345678
    XOR RAX, RAX
    LONG_MODE
    RET

BOOT()
EOF
    
    ./lasm test.asm test.bin
    ./lasm test.asm test.elf --elf
    
    echo -e "${GREEN}Test compilation successful${NC}"
}

# Debian package
create_deb() {
    echo "Creating Debian package..."
    
    PKG_DIR="lasm_${VERSION}_amd64"
    mkdir -p "$PKG_DIR/DEBIAN"
    mkdir -p "$PKG_DIR/usr/bin"
    mkdir -p "$PKG_DIR/usr/lib"
    mkdir -p "$PKG_DIR/usr/include"
    mkdir -p "$PKG_DIR/usr/share/lasm"
    mkdir -p "$PKG_DIR/usr/share/doc/lasm"
    
    cat > "$PKG_DIR/DEBIAN/control" << EOF
Package: lasm
Version: ${VERSION}
Section: devel
Priority: optional
Architecture: amd64
Depends: libc6
Maintainer: LASM Developer
Description: Litad Assembly - x86_64 Long Mode Assembler
 LASM is a powerful assembler for x86_64 architecture
 with full Long Mode support and time_t compatibility.
 .
 Supports Intel, AMD, Elbrus, Baikal, and MCST processors.
 Features Long Mode 64-bit support and time_t integration.
EOF
    
    cat > "$PKG_DIR/DEBIAN/postinst" << 'EOF'
#!/bin/bash
ldconfig
echo "LASM installed successfully"
EOF
    chmod 755 "$PKG_DIR/DEBIAN/postinst"
    
    cp lasm "$PKG_DIR/usr/bin/"
    cp liblasm.so "$PKG_DIR/usr/lib/"
    cp liblasm.a "$PKG_DIR/usr/lib/"
    cp liblasm.h "$PKG_DIR/usr/include/"
    cp test.asm "$PKG_DIR/usr/share/lasm/"
    
    cat > "$PKG_DIR/usr/share/doc/lasm/README" << EOF
LASM - Litad Assembly v${VERSION}
================================

Usage:
  lasm input.asm output.bin
  lasm input.asm output.elf --elf

Features:
  - Full Long Mode support
  - time_t compatibility
  - ELF64 binary output
  - Boot sector generation
EOF
    
    dpkg-deb --build "$PKG_DIR"
    rm -rf "$PKG_DIR"
    
    echo -e "${GREEN}Created lasm_${VERSION}_amd64.deb${NC}"
}

# RPM package
create_rpm() {
    echo "Creating RPM package..."
    
    RPM_ROOT="rpmbuild"
    mkdir -p "$RPM_ROOT/BUILD" "$RPM_ROOT/RPMS" "$RPM_ROOT/SOURCES" "$RPM_ROOT/SPECS" "$RPM_ROOT/SRPMS"
    
    mkdir -p "lasm-${VERSION}"
    cp lasm liblasm.so liblasm.a liblasm.h test.asm "lasm-${VERSION}/"
    
    tar czf "$RPM_ROOT/SOURCES/lasm-${VERSION}.tar.gz" "lasm-${VERSION}"
    rm -rf "lasm-${VERSION}"
    
    cat > "$RPM_ROOT/SPECS/lasm.spec" << EOF
Name:           lasm
Version:        ${VERSION}
Release:        1
Summary:        Litad Assembly - x86_64 Long Mode Assembler

License:        MIT
Source0:        %{name}-%{version}.tar.gz

BuildArch:      x86_64
Requires:       glibc

%description
LASM is a powerful assembler for x86_64 architecture
with full Long Mode support and time_t compatibility.

%prep
%setup -q

%install
mkdir -p %{buildroot}/usr/bin
mkdir -p %{buildroot}/usr/lib
mkdir -p %{buildroot}/usr/include
mkdir -p %{buildroot}/usr/share/lasm

install -m 755 lasm %{buildroot}/usr/bin/lasm
install -m 644 liblasm.so %{buildroot}/usr/lib/liblasm.so
install -m 644 liblasm.a %{buildroot}/usr/lib/liblasm.a
install -m 644 liblasm.h %{buildroot}/usr/include/liblasm.h
install -m 644 test.asm %{buildroot}/usr/share/lasm/test.asm

%files
/usr/bin/lasm
/usr/lib/liblasm.so
/usr/lib/liblasm.a
/usr/include/liblasm.h
/usr/share/lasm/test.asm

%post
ldconfig

%postun
ldconfig
EOF
    
    if command -v rpmbuild &> /dev/null; then
        rpmbuild -bb "$RPM_ROOT/SPECS/lasm.spec" --define "_topdir $PWD/$RPM_ROOT"
        cp "$RPM_ROOT/RPMS/x86_64/"*.rpm . 2>/dev/null || true
    else
        tar czf "lasm-${VERSION}-1.x86_64.rpm.tar.gz" lasm liblasm.so liblasm.a liblasm.h
    fi
    
    rm -rf "$RPM_ROOT"
    echo -e "${GREEN}RPM package prepared${NC}"
}

# Arch Linux package
create_arch() {
    echo "Creating Arch Linux package..."
    
    mkdir -p pkg/usr/bin pkg/usr/lib pkg/usr/include pkg/usr/share/lasm
    
    cp lasm pkg/usr/bin/
    cp liblasm.so pkg/usr/lib/
    cp liblasm.a pkg/usr/lib/
    cp liblasm.h pkg/usr/include/
    cp test.asm pkg/usr/share/lasm/
    
    cat > PKGBUILD << EOF
pkgname=lasm
pkgver=${VERSION}
pkgrel=1
pkgdesc="Litad Assembly - x86_64 Long Mode Assembler"
arch=(x86_64)
license=(MIT)
depends=(glibc)
makedepends=(gcc)

package() {
    install -Dm755 lasm "\$pkgdir/usr/bin/lasm"
    install -Dm644 liblasm.so "\$pkgdir/usr/lib/liblasm.so"
    install -Dm644 liblasm.a "\$pkgdir/usr/lib/liblasm.a"
    install -Dm644 liblasm.h "\$pkgdir/usr/include/liblasm.h"
    install -Dm644 test.asm "\$pkgdir/usr/share/lasm/test.asm"
}
EOF
    
    if command -v makepkg &> /dev/null; then
        makepkg -si --noconfirm 2>/dev/null || true
    else
        tar czf "lasm-${VERSION}-1-x86_64.pkg.tar.gz" pkg/
    fi
    
    rm -rf pkg/
    echo -e "${GREEN}Arch Linux package created${NC}"
}

# Windows executable
create_windows() {
    echo "Creating Windows executable..."
    
    if command -v x86_64-w64-mingw32-gcc &> /dev/null; then
        x86_64-w64-mingw32-gcc -O2 -static -o lasm.exe lasm.c
        echo -e "${GREEN}lasm.exe created${NC}"
    elif command -v i686-w64-mingw32-gcc &> /dev/null; then
        i686-w64-mingw32-gcc -O2 -static -o lasm.exe lasm.c
        echo -e "${GREEN}lasm.exe created${NC}"
    else
        echo "Installing mingw-w64..."
        sudo apt-get update -qq
        sudo apt-get install -y mingw-w64 2>/dev/null || {
            echo "Cannot install mingw-w64"
        }
        if command -v x86_64-w64-mingw32-gcc &> /dev/null; then
            x86_64-w64-mingw32-gcc -O2 -static -o lasm.exe lasm.c
        fi
    fi
    
    mkdir -p windows_package
    cp lasm.exe windows_package/ 2>/dev/null || true
    cp liblasm.h windows_package/ 2>/dev/null || true
    
    cat > windows_package/INSTALL.txt << EOF
LASM v${VERSION} for Windows
============================
Copy lasm.exe to a directory in your PATH
Run: lasm.exe input.asm output.bin
EOF
    
    zip -q "lasm-${VERSION}-windows.zip" windows_package/* 2>/dev/null || true
    rm -rf windows_package
    
    echo -e "${GREEN}Windows package created${NC}"
}

show_results() {
    echo ""
    echo "Build Complete"
    echo "=============="
    echo ""
    echo "Generated files:"
    ls -lh lasm lasm.exe *.deb *.rpm *.tar.gz *.pkg.tar* *.zip *.bin *.elf 2>/dev/null || true
    echo ""
    echo "Installation:"
    echo "  Debian/Ubuntu:  sudo dpkg -i lasm_${VERSION}_amd64.deb"
    echo "  Fedora/RHEL:    sudo rpm -i lasm-${VERSION}-1.x86_64.rpm"
    echo "  Arch Linux:     makepkg -si"
    echo "  Windows:        unzip lasm-${VERSION}-windows.zip"
}

main() {
    clean
    build_compiler
    build_library
    test_build
    
    echo ""
    echo "Creating packages..."
    
    create_deb
    create_rpm
    create_arch
    create_windows
    
    show_results
}

main "$@"
