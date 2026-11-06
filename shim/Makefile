# The Tensor G5's boot shim (README.md, "The boot shim"): shim.bin, an arm64 Linux Image that starts xnuloader without a UEFI.
# Needs LLVM (clang, ld.lld, llvm-objcopy for an ELF target: Apple's clang compiles, Homebrew's llvm links) and the common
# repository around it (socs/lga in scion-kernel): the UEFI declarations are xnuloader/efi.h, the loader's own.
#
#   make            shim.bin
#   make test       the device-tree and memory-map host test (fdt-test), on tests/*.dts
#   make audit      audit.py: no write path to storage in what runs before the kernel (shim.bin needs it)
#   make audit-test the audit fails on a store it should refuse
#   make clean

ROOT     ?= $(abspath ../../..)
LLVM     ?= $(firstword $(wildcard /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin /usr/lib/llvm-19/bin /usr/lib/llvm-18/bin) /usr/bin)
CC        = $(LLVM)/clang
LD        = $(firstword $(wildcard $(LLVM)/ld.lld) $(shell command -v ld.lld))
OBJCOPY   = $(firstword $(wildcard $(LLVM)/llvm-objcopy) llvm-objcopy)
HOSTCC   ?= cc
DTC      ?= dtc

CFLAGS    = --target=aarch64-none-elf -ffreestanding -fno-builtin -nostdlib -fPIE -fvisibility=hidden \
            -mgeneral-regs-only -mstrict-align -O2 -Wall -Wextra -Wno-unused-parameter -Wno-pointer-to-int-cast \
            -I$(ROOT)/xnuloader -I. $(CFLAGS_EXTRA)
OBJS      = start.o shim.o dpu.o fdt.o memmap.o

all: shim.bin

%.o: %.c fdt.h memmap.h dpu.h shim.h $(ROOT)/xnuloader/efi.h
	$(CC) $(CFLAGS) -c $< -o $@

start.o: start.S
	$(CC) $(CFLAGS) -c $< -o $@

shim.elf: $(OBJS) shim.ld audit
	$(LD) -pie --no-dynamic-linker -z norelro -z max-page-size=4096 --hash-style=sysv -T shim.ld $(OBJS) -o $@

# .bss as contents: the payload is appended behind the whole image (shim.ld)
shim.bin: shim.elf
	$(OBJCOPY) --set-section-flags .bss=alloc,load,contents -O binary $< $@
	@size=$$(wc -c < $@ | tr -d ' '); end=$$(($$($(LLVM)/llvm-nm $< | awk '/ __image_end$$/ { print "0x" $$1 }'))); \
	  [ "$$size" -eq "$$end" ] || { echo "shim.bin is $$size bytes, __image_end is $$end"; rm -f $@; exit 1; }; \
	  echo "shim.bin: $$size bytes"

audit:
	python3 -I audit.py .

audit-test:
	@d=$$(mktemp -d) && cp *.c *.h *.S $$d && echo 'static void x(void) { *(volatile unsigned *)0x3c400000 = 1; }' >> $$d/dpu.c && \
	  if python3 -I audit.py $$d >/dev/null; then echo "audit-test: the audit took a store to the UFS host"; rm -rf $$d; exit 1; fi; \
	  echo "audit-test: refused, as it should"; rm -rf $$d

fdt-test: fdt-test.c fdt.c memmap.c fdt.h memmap.h
	$(HOSTCC) -O1 -Wall -Wextra -I. fdt-test.c fdt.c memmap.c -o $@

tests/%.dtb: tests/%.dts
	$(DTC) -q -I dts -O dtb -o $@ $<

test: fdt-test tests/frankel-like.dtb
	./fdt-test tests/frankel-like.dtb

clean:
	rm -f *.o shim.elf shim.bin fdt-test tests/*.dtb

.PHONY: all test audit audit-test clean
