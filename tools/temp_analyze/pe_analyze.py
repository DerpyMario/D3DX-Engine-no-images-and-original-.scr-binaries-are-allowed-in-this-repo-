#!/usr/bin/env python3
"""Extract .rdata strings (wide+ascii), dump custom resources, enumerate .pdata functions."""
import pefile, struct, sys, re

def dump_strings(pe, sec_name='.rdata'):
    sec = [s for s in pe.sections if s.Name.decode().rstrip('\0') == sec_name][0]
    data = pe.get_data(sec.VirtualAddress, sec.Misc_VirtualSize)
    rva = sec.VirtualAddress
    print(f"== ASCII strings in {sec_name} ==")
    for m in re.finditer(rb'[\x20-\x7e]{5,}', data):
        print(f"  {hex(rva + m.start())}: {m.group().decode()}")
    print(f"== Wide strings in {sec_name} ==")
    off = 0
    while off + 2 <= len(data):
        # find UTF-16LE strings: sequence of chars with 00 high byte
        end = off
        s = ''
        while end + 1 < len(data):
            lo, hi = data[end], data[end+1]
            if hi == 0 and 0x20 <= lo < 0x7f:
                s += chr(lo); end += 2
            else:
                break
        if len(s) >= 5:
            print(f"  {hex(rva + off)}: {s!r}")
            off = end + 2
        else:
            off += 2

def dump_pdata(pe):
    print("== .pdata RUNTIME_FUNCTION map ==")
    pdata = [s for s in pe.sections if s.Name.decode().rstrip('\0') == '.pdata'][0]
    data = pe.get_data(pdata.VirtualAddress, pdata.Misc_VirtualSize)
    funcs = []
    for i in range(0, len(data) - 11, 12):
        start, end, unwind = struct.unpack_from('<III', data, i)
        funcs.append((start, end, unwind))
    funcs.sort()
    for start, end, unwind in funcs:
        print(f"  {hex(start)} - {hex(end)}  size={end-start:#x}")
    return funcs

def dump_custom(pe, type_id, names=None):
    print(f"== custom resource type {type_id} ==")
    for e in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        tid = e.id if e.id is not None else (e.name.string.decode(errors='replace') if e.name else None)
        if str(tid) == str(type_id) or tid == type_id:
            for ide in e.directory.entries:
                for lang in ide.directory.entries:
                    data = pe.get_data(lang.data.struct.OffsetToData, lang.data.struct.Size)
                    print(f"  id={ide.id} size={len(data)} magic={data[:8].hex()} ascii={data[:16]!r}")
                    with open(f"/home/z/my-project/work/res_type{type_id}_{ide.id}.bin", 'wb') as f:
                        f.write(data)

def manifest(pe):
    for e in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if e.id == 24:
            for ide in e.directory.entries:
                for lang in ide.directory.entries:
                    data = pe.get_data(lang.data.struct.OffsetToData, lang.data.struct.Size)
                    print(data.decode(errors='replace'))

def mui_record(pe):
    for e in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        # RT_MUI = type 15? actually "MUI" string type
        nm = e.name.string.decode(errors='replace') if e.name else str(e.id)
        if nm in ('MUI',) or e.id == 15:
            for ide in e.directory.entries:
                for lang in ide.directory.entries:
                    data = pe.get_data(lang.data.struct.OffsetToData, lang.data.struct.Size)
                    print(f"== RT_MUI record ({len(data)} bytes) ==")
                    print(data[:64].hex(' '))
                    # decode: signature 0xFECDFECD, version, pathType...
                    sig, sz, typ, hdrsz = struct.unpack_from('<IIII', data, 0)
                    print(f"sig={sig:#x} size={sz:#x} type={typ:#x} hdrsize={hdrsz:#x}")

if __name__ == '__main__':
    path = sys.argv[1]
    mode = sys.argv[2] if len(sys.argv) > 2 else 'all'
    pe = pefile.PE(path)
    if mode in ('strings', 'all'): dump_strings(pe)
    if mode in ('pdata', 'all'): dump_pdata(pe)
    if mode == 'custom': dump_custom(pe, sys.argv[3])
    if mode == 'manifest': manifest(pe)
    if mode == 'mui': mui_record(pe)
