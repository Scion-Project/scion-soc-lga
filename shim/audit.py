#!/usr/bin/env python3
"""
Build-time audit of the shim's write paths: fails the build if the code that runs before the kernel can write to storage.

  audit.py [dir]       the shim's sources in dir (default: here); exit 1 and a line per violation, or "audit: ok"

What it holds the sources to (comments are not read):
  1. a store through a device pointer is in mmio_write32 and nowhere else (the one place with an allow list: shim.c);
     the only other volatile pointer is the framebuffer's `row`, which is RAM;
  2. no name of storage, the UFS host or fuses, no block I/O protocol: the loader's block service is "not found";
  3. no inline-assembly store, no cache-line zeroing;
  4. SMC is made in two places: psci() with a PSCI function number, and EL2's forwarder after it has checked the number
     is PSCI's or an SMCCC architecture call.
It reads text, not machine code: it catches a change by mistake, not an adversary.
"""
import re
import sys
from pathlib import Path

FORBIDDEN = ['0x3c400000', '0x3c410000', '0x3c700000', 'ufs', 'blockio', 'block_io', 'block-io', 'flash', 'fuse',
             'otp', 'nvme', 'emmc', 'sdhci', 'partition', 'gsa']


def strip(text):
    text = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def function_span(src, name):
    m = re.search(r'^%s\([^)]*\)\n\{' % re.escape(name), src, flags=re.M)
    if not m:
        return None
    depth, i = 0, m.end() - 1
    while i < len(src):
        depth += {'{': 1, '}': -1}.get(src[i], 0)
        i += 1
        if depth == 0:
            return m.start(), i
    return None


def audit(d):
    bad = []
    files = [p for p in sorted(d.iterdir()) if p.suffix in ('.c', '.h', '.S') and p.name != 'fdt-test.c']
    for p in files:
        src = strip(p.read_text())
        low = src.lower()
        for word in FORBIDDEN:
            for m in re.finditer(r'\b' + re.escape(word) + r'\b' if word.isalpha() else re.escape(word), low):
                bad.append('%s: "%s" in code (storage, fuses and the UFS host are not the shim\'s)' % (p.name, word))
                break
        if p.suffix == '.S':
            continue
        allowed = function_span(src, 'mmio_write32') if p.name == 'shim.c' else None
        for m in re.finditer(r'\*\s*\(\s*volatile[^;]*?\)[^;=]*?[^=!<>+\-*/&|^]=(?!=)', src):
            if allowed is None or not allowed[0] <= m.start() < allowed[1]:
                bad.append('%s: store through a device pointer outside mmio_write32: %s' % (p.name, m.group(0).strip()[:60]))
        for m in re.finditer(r'volatile\s+\w+\s*\*\s*(\w+)', src):
            if m.group(1) != 'row':
                bad.append('%s: volatile pointer "%s" (only the framebuffer\'s row may be written through one)' % (p.name, m.group(1)))
        for m in re.finditer(r'__asm__[^;]*;', src, flags=re.S):
            if re.search(r'\b(str|stp|stnp|stlr|stxr|dc\s+zva)\b', m.group(0)):
                bad.append('%s: a store in inline assembly: %s' % (p.name, ' '.join(m.group(0).split())[:60]))
            if re.search(r'\bsmc\b', m.group(0)) and p.name != 'shim.c':
                bad.append('%s: SMC outside psci()' % p.name)
        if p.name == 'shim.c':
            for m in re.finditer(r'(?<!\n)\bpsci\(([^,]*),', src):
                toks = re.findall(r'[A-Za-z_]\w*', m.group(1))
                if not all(t.startswith('PSCI_') or t in ('type', 'fn') for t in toks):
                    bad.append('shim.c: psci() called with %s' % m.group(1).strip())
            for m in re.finditer(r'#define\s+(PSCI_\w+)\s+(0x[0-9a-fA-F]+)u?', src):
                v = int(m.group(2), 16)
                if (v & ~0x1f) not in (0x84000000, 0xC4000000):
                    bad.append('shim.c: %s is not a PSCI function number' % m.group(1))
            smc = function_span(src, 'psci')
            for m in re.finditer(r'\bsmc\b', src):
                if smc is None or not smc[0] <= m.start() < smc[1]:
                    bad.append('shim.c: SMC outside psci()')
    s = d / 'start.S'
    if s.exists():
        lines = strip(s.read_text()).splitlines()
        gate = next((i for i, l in enumerate(lines) if re.search(r'#0xc400', l)), None)
        for i, l in enumerate(lines):
            if re.search(r'\bsmc\b', l) and (gate is None or i < gate):
                bad.append('start.S: SMC before the PSCI function-number check (line %d)' % (i + 1))
        for i, l in enumerate(lines):
            if re.search(r'0x[0-9a-fA-F]{7,}', l) and not l.lstrip().startswith('#define PSCI_'):
                bad.append('start.S: a device-sized literal on line %d: %s' % (i + 1, l.strip()))
    return bad


if __name__ == '__main__':
    problems = audit(Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).parent))
    for line in problems:
        print('audit:', line)
    print('audit: ok' if not problems else 'audit: FAILED, %d problem(s)' % len(problems))
    sys.exit(1 if problems else 0)
