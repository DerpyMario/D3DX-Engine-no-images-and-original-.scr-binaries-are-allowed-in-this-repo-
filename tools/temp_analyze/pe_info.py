#!/usr/bin/env python3
"""Dump PE overview: headers, sections, imports, exports, resources tree, version info."""
import pefile, sys, struct

def main(path):
    pe = pefile.PE(path, fast_load=False)
    print(f"=== {path} ===")
    oh = pe.OPTIONAL_HEADER
    fh = pe.FILE_HEADER
    print(f"Machine: {hex(fh.Machine)}  Magic: {hex(oh.Magic)}")
    print(f"ImageBase: {hex(oh.ImageBase)}  EntryPoint RVA: {hex(oh.AddressOfEntryPoint)}")
    print(f"LinkerVersion: {oh.MajorLinkerVersion}.{oh.MinorLinkerVersion}")
    print(f"SizeOfImage: {hex(oh.SizeOfImage)}  CheckSum: {hex(oh.CheckSum)}")
    if hasattr(pe, 'DIRECTORY_ENTRY_DEBUG'):
        for d in pe.DIRECTORY_ENTRY_DEBUG:
            if d.struct.Type == 2:  # CODEVIEW
                data = pe.get_data(d.struct.AddressOfRawData, d.struct.SizeOfData)
                if data[:4] == b'RSDS':
                    pdb = data[24:].split(b'\x00')[0].decode(errors='replace')
                    guid = data[4:20]
                    print(f"PDB: {pdb}  GUID: {guid.hex()}  age: {struct.unpack_from('<I', data, 20)[0]}")
    print("\n-- Sections --")
    for s in pe.sections:
        print(f"  {s.Name.decode().rstrip(chr(0)):8s} VA={hex(s.VirtualAddress):9s} VSz={hex(s.Misc_VirtualSize):8s} RSz={hex(s.SizeOfRawData):8s} Char={hex(s.Characteristics)}")
    print("\n-- Exports --")
    if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT'):
        for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
            print(f"  ordinal {e.ordinal}: {e.name}")
    else:
        print("  (none)")
    print("\n-- Imports --")
    if hasattr(pe, 'DIRECTORY_ENTRY_IMPORT'):
        for imp in pe.DIRECTORY_ENTRY_IMPORT:
            names = []
            for i in imp.imports:
                if i.name:
                    names.append(i.name.decode())
                else:
                    names.append(f"ord#{i.ordinal}")
            print(f"  {imp.dll.decode()} ({len(names)}): {', '.join(names)}")
    print("\n-- Resources --")
    def walk(dir_, depth=0):
        typemap = {1:'CURSOR',2:'BITMAP',3:'ICON',4:'MENU',5:'DIALOG',6:'STRING',9:'ACCEL',10:'RCDATA',12:'GROUP_CURSOR',14:'GROUP_ICON',16:'VERSION',23:'MANIFEST'}
        for e in dir_.entries:
            name = e.name.string.decode() if e.name else (str(e.id) if e.id is not None else '?')
            label = typemap.get(e.id, str(e.id)) if depth == 0 else name
            if hasattr(e, 'directory'):
                print(f"{'  '*depth}{label}/")
                walk(e.directory, depth+1)
            else:
                d = e.data
                print(f"{'  '*depth}{label}  size={d.struct.Size}")
    if hasattr(pe, 'DIRECTORY_ENTRY_RESOURCE'):
        walk(pe.DIRECTORY_ENTRY_RESOURCE, 0)

    try:
        vi = pe.FileInfo[0][0] if hasattr(pe, 'FileInfo') else None
        if vi is not None and vi.Key.decode() == 'StringFileInfo':
            print("\n-- Version strings --")
            for st in vi.StringTable:
                for k, v in st.entries.items():
                    print(f"  {k.decode()}: {v.decode(errors='replace')}")
    except Exception as ex:
        print("no version strings:", ex)

    try:
        rich = pe.parse_rich_header()
        if rich and 'clear_data' in rich:
            blob = rich['clear_data']
            vals = struct.unpack_from('<%dI' % (len(blob)//4), blob)
            print("\n-- Rich header product IDs --")
            seen = {}
            for i in range(1, len(vals)-1, 2):
                prod, cnt = vals[i] ^ rich['key'], vals[i+1] ^ rich['key']
                if prod:
                    seen[hex(prod)] = seen.get(hex(prod), 0) + cnt
            for k, v in seen.items():
                print(f"  prod {k}: count {v}")
    except Exception:
        pass

if __name__ == '__main__':
    for p in sys.argv[1:]:
        main(p)
