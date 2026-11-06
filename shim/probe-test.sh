#!/bin/bash
# The display probe's flow in QEMU, with RAM where the display controller would be: the shim built with LGA_DPU at 0x1A0000000
# in a 6 GB machine whose tree says "google,lga". Nothing counts frames there, so the kick reports "did not move"; what this
# shows is that the probe's sequence, its bands, the audit's allow list and the RAM ring work, and that nothing else is stored to.
#
#   socs/lga/shim/probe-test.sh            needs qemu-system-aarch64, LLVM (Makefile) and the common repository (ROOT)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${OUT:-$HERE/out-probe}
DPU=0x1A0000000
mkdir -p "$OUT"
make -s -C "$HERE" clean
make -s -C "$HERE" shim.bin CFLAGS_EXTRA="-DLGA_DPU=${DPU}ULL" ${ROOT:+ROOT=$ROOT} ${LLVM:+LLVM=$LLVM} >/dev/null
cp "$HERE/shim.bin" "$OUT/shim-test.bin"
make -s -C "$HERE" clean
M=virt,virtualization=on,gic-version=4
qemu-system-aarch64 -M "$M,dumpdtb=$OUT/virt.dtb" -cpu cortex-a76 -smp 1 -m 6G -display none >/dev/null
# the tree's root compatible "linux,dummy-virt" becomes "google,lga" (same room: the shim reads the string list)
python3 -I -c "
import sys
b = open(sys.argv[1], 'rb').read()
old = b'linux,dummy-virt\0'
assert old in b
open(sys.argv[2], 'wb').write(b.replace(old, b'google,lga' + b'\0' * 7))" "$OUT/virt.dtb" "$OUT/lga.dtb"
timeout 60 qemu-system-aarch64 -M "$M" -cpu cortex-a76 -smp 1 -m 6G -kernel "$OUT/shim-test.bin" -dtb "$OUT/lga.dtb" \
	-append "scion.probe=2 scion.kick=1 scion.fb=0x100000000,1080,2424,4320" -display none -serial stdio -monitor none \
	-no-reboot 2>&1 | tr -d '\r' | head -c 20000 | tee "$OUT/probe.log"
