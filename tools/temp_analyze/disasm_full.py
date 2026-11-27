#!/usr/bin/env python3
"""Full-function disassembler for Ribbons/Mystify .scr files (v2).
Uses capstone's op_str and post-processes rip-relative / immediates.
"""
import pefile, capstone, struct, json, sys, re, os

class Analyzer:
    def __init__(self, path):
        self.path = path
        self.pe = pefile.PE(path)
        self.image_base = self.pe.OPTIONAL_HEADER.ImageBase
        self.sections = [(s.Name.decode().rstrip('\0'), s.VirtualAddress, s.Misc_VirtualSize)
                         for s in self.pe.sections]
        self.iat = {}
        if hasattr(self.pe, 'DIRECTORY_ENTRY_IMPORT'):
            for dll in self.pe.DIRECTORY_ENTRY_IMPORT:
                for i in dll.imports:
                    # i.address is the IAT slot VA; store keyed by RVA
                    self.iat[i.address - self.image_base] = f"{dll.dll.decode()}!{i.name.decode() if i.name else 'ord'+str(i.ordinal)}"
        pdata = [s for s in self.pe.sections if s.Name.decode().rstrip('\0') == '.pdata'][0]
        data = self.pe.get_data(pdata.VirtualAddress, pdata.Misc_VirtualSize)
        self.funcs = []
        for i in range(0, len(data) - 11, 12):
            start, end, unwind = struct.unpack_from('<III', data, i)
            self.funcs.append([start, end])
        self.funcs.sort()
        self.funcset = set(range(self.funcs[0][0], self.funcs[-1][1]))
        self.strings = {}
        for secname in ('.rdata', '.data'):
            try:
                sec = [s for s in self.pe.sections if s.Name.decode().rstrip('\0') == secname][0]
            except IndexError:
                continue
            blob = self.pe.get_data(sec.VirtualAddress, sec.Misc_VirtualSize)
            for m in re.finditer(rb'[\x20-\x7e]{4,}', blob):
                self.strings[sec.VirtualAddress + m.start()] = m.group().decode()
            off = 0
            while off + 2 <= len(blob):
                end = off; s = ''
                while end + 1 < len(blob):
                    lo, hi = blob[end], blob[end+1]
                    if hi == 0 and 0x20 <= lo < 0x7f:
                        s += chr(lo); end += 2
                    else:
                        break
                if len(s) >= 4:
                    self.strings[sec.VirtualAddress + off] = s
                    off = end + 2
                else:
                    off += 2

    def sec_of(self, rva):
        for name, va, vsz in self.sections:
            if va <= rva < va + max(vsz, 0x1000):
                return name
        return '?'

    def rva2va(self, rva):
        return self.image_base + rva

    def lookup_addr(self, rva):
        """Return annotation for an address (import / string / func)."""
        if rva in self.iat:
            return self.iat[rva]
        if rva in self.strings:
            s = self.strings[rva]
            return f'"{s[:80]}"'
        cs = None
        try:
            data = self.pe.get_data(rva, 4)
            if data[:2] and data[1] == 0:
                cw = self.pe.get_data(rva, 512)
                s = ''
                for i in range(0, min(len(cw), 512), 2):
                    lo, hi = cw[i], cw[i+1]
                    if hi == 0 and 0x20 <= lo < 0x7f:
                        s += chr(lo)
                    elif lo == 0 and hi == 0:
                        break
                    else:
                        s = ''
                        break
                if len(s) >= 4:
                    cs = s
        except Exception:
            pass
        if cs:
            return f'W"{cs[:80]}"'
        for fs, fe in self.funcs:
            if fs == rva:
                return f'sub_{fs:x}'
        return None

    def annotate_line(self, insn):
        opstr = insn.op_str
        def repl_rip(m):
            disp = int(m.group(1), 16)
            if m.group(1).startswith('-') or m.group(1).lstrip('-').startswith('0x') and m.group(1)[0] == '-':
                pass
            target = insn.address + insn.size + disp
            ann = self.lookup_addr(target)
            va = self.rva2va(target)
            base = f'[{va:#x}]' if ann and not ann.startswith('sub_') else f'[{target:#x}]'
            return f'{base}' + (f' /* {ann} */' if ann else '')
        opstr = re.sub(r'\[rip ([+-]) 0x([0-9a-f]+)\]',
                       lambda m: repl_rip(m) if self._signed(m) is not None else m.group(0),
                       opstr)
        # capstone prints "rip + 0x1234" or "rip - 0x1234"
        def repl2(m):
            sign = -1 if m.group(1) == '-' else 1
            disp = sign * int(m.group(2), 16)
            target = insn.address + insn.size + disp
            ann = self.lookup_addr(target)
            if ann and not ann.startswith('sub_'):
                return f'[{self.rva2va(target):#x}] /* {ann} */'
            elif ann:
                return f'sub_{target:x}'
            else:
                return f'[{target:#x}] /* rva */'
        opstr = re.sub(r'\[rip ([+-]) 0x([0-9a-fA-F]+)\]', repl2, opstr)
        # annotate direct call/jmp targets
        if insn.mnemonic in ('call', 'jmp') and '0x' in opstr:
            m = re.search(r'0x([0-9a-fA-F]+)', opstr)
            t = int(m.group(1), 16)
            ann = self.lookup_addr(t)
            if ann:
                opstr = re.sub(r'0x[0-9a-fA-F]+', ann, opstr, count=1)
            else:
                opstr += f'  ; -> rva {t:#x}'
        return opstr

    @staticmethod
    def _signed(m):
        return None

    def disasm_function(self, start, end):
        code = self.pe.get_data(start, end - start)
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        md.detail = True
        out = []
        calls = []
        for insn in md.disasm(code, start):
            ann = self.annotate_line(insn)
            out.append(f'{insn.address:#010x}: {insn.mnemonic:<10s} {ann}')
            if insn.mnemonic in ('call', 'jmp') and insn.operands and \
               insn.operands[0].type == capstone.x86.X86_OP_IMM:
                t = insn.operands[0].imm
                if self.sec_of(t) == '.text' and (insn.mnemonic == 'call'):
                    calls.append(t)
                elif insn.mnemonic == 'jmp':
                    # tail-call
                    calls.append(t)
        return out, calls

def main():
    path = sys.argv[1]
    outdir = sys.argv[2]
    os.makedirs(outdir, exist_ok=True)
    a = Analyzer(path)
    callgraph = {}
    fmap = {}
    os.makedirs(os.path.join(outdir, 'asm'), exist_ok=True)
    for start, end in a.funcs:
        try:
            lines, calls = a.disasm_function(start, end)
        except Exception as e:
            lines, calls = [f'; ERROR: {e}'], []
        fmap[start] = {'end': end, 'size': end - start, 'calls': sorted(set(calls))}
        callgraph[start] = sorted(set(calls))
        with open(os.path.join(outdir, 'asm', f'func_{start:05x}.asm'), 'w') as f:
            f.write(f'; function {start:#x}..{end:#x} (size {end-start:#x})\n')
            f.write('\n'.join(lines) + '\n')
    json.dump({'funcs': {f'{k:#x}': v for k, v in fmap.items()},
               'callgraph': {f'{k:#x}': [f'{c:#x}' for c in v] for k, v in callgraph.items()}},
              open(os.path.join(outdir, 'functions.json'), 'w'), indent=1)
    callers = {f'{k:#x}': [] for k in callgraph}
    for f, cs in callgraph.items():
        for c in cs:
            callers.setdefault(f'{c:#x}', []).append(f'{f:#x}')
    json.dump(callers, open(os.path.join(outdir, 'callers.json'), 'w'), indent=1)
    print(f'{len(a.funcs)} functions; callgraph written to {outdir}')

if __name__ == '__main__':
    main()
