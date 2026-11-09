#!/usr/bin/env python3
"""Isolate core writable statics, excluding GCC emulated-TLS runtime metadata.
Use -fdata-sections so __emutls_v.* controls and __emutls_t.* templates have
individual COFF sections. These must survive resets: libgcc retains their TLS
indices and allocations for the process lifetime.
"""
import re
import subprocess
import sys
source, output = sys.argv[1:]
listing = subprocess.check_output(['x86_64-w64-mingw32-objdump', '-h', source], text=True)
sections = re.findall(r'^\s*\d+\s+(\S+)\s+[0-9a-fA-F]+\s', listing, re.M)
args = ['x86_64-w64-mingw32-objcopy']
for name in sections:
    if '__emutls' in name:
        continue
    if name == '.data' or name.startswith('.data$'):
        args += ['--rename-section', name + '=.rxdata']
    elif name == '.bss' or name.startswith('.bss$'):
        args += ['--rename-section', name + '=.rxbss']
subprocess.run(args + [source, output], check=True)
# Fail closed if compiler settings ever put emutls controls into the captured set.
symbols = subprocess.check_output(['x86_64-w64-mingw32-objdump', '-t', output], text=True)
listing = subprocess.check_output(['x86_64-w64-mingw32-objdump', '-h', output], text=True)
names = {int(i)+1: n for i, n in re.findall(r'^\s*(\d+)\s+(\S+)\s+[0-9a-fA-F]+\s', listing, re.M)}
for line in symbols.splitlines():
    if '__emutls_v.' in line or '__emutls_t.' in line:
        match = re.search(r'\(sec\s+(\d+)\)', line)
        if match and names.get(int(match[1])) in ('.rxdata', '.rxbss'):
            raise SystemExit('emutls metadata incorrectly included in core reset: ' + line)
