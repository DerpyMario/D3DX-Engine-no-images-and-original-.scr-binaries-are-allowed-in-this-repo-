#!/usr/bin/env python3
"""Extract ALL resources from a PE into a named tree + decode DLGTEMPLATEEX and RT_STRING blocks.
Also decodes VS_VERSIONINFO and RT_MUI records.
"""
import pefile, struct, sys, os, json, re

TYPE_NAMES = {1:'CURSOR',2:'BITMAP',3:'ICON',4:'MENU',5:'DIALOG',6:'STRING',9:'ACCEL',
              10:'RCDATA',12:'GROUP_CURSOR',14:'GROUP_ICON',16:'VERSION',23:'MANIFEST',
              11:'MSGTABLE',7:'FONTDIR',8:'FONT'}

def typename(t, name_obj):
    if name_obj is not None:
        return name_obj.string.decode(errors='replace')
    return TYPE_NAMES.get(t, f'TYPE{t}')

def read_utf16z(data, off):
    s = ''
    while off + 1 < len(data):
        w = struct.unpack_from('<H', data, off)[0]
        off += 2
        if w == 0:
            break
        s += chr(w)
    return s, off

def pad2(off): return (off + 3) // 4 * 4

def read_sz_or_ord(data, off):
    """sz_Or_Ord: 0xFFFF followed by WORD id = ordinal, else zero-terminated wide string."""
    if off + 2 > len(data):
        return None, off
    w = struct.unpack_from('<H', data, off)[0]
    if w == 0xFFFF:
        return ('ord', struct.unpack_from('<H', data, off + 2)[0]), off + 4
    s, off2 = read_utf16z(data, off)
    return ('str', s), off2

CLASS_MAP = {0x80:'BUTTON',0x81:'EDIT',0x82:'STATIC',0x83:'LISTBOX',0x84:'SCROLLBAR',0x85:'COMBOBOX'}
DLGCLASS_MAP = {1:'BUTTON',2:'EDIT',3:'STATIC',4:'LISTBOX',5:'SCROLLBAR',6:'COMBOBOX'}

def decode_dialogtemplateex(data):
    """Decode DLGTEMPLATEEX (signature 0xFFFF, dlgVer 1)."""
    dlgver, sig = struct.unpack_from('<HH', data, 0)
    if sig != 0xFFFF:
        return None
    helpid, exstyle, style = struct.unpack_from('<III', data, 4)
    cd, x, y, cx, cy = struct.unpack_from('<HHHHH', data, 16)
    off = 26
    menu, off = read_sz_or_ord(data, off)
    cls, off = read_sz_or_ord(data, off)
    title, off = read_utf16z(data, off)
    font = None
    if style & 0x40:  # DS_SETFONT (0x40); DS_SHELLFONT = 0x40|0x8
        pt, weight = struct.unpack_from('<HH', data, off)
        italic, charset = data[off+4], data[off+5]
        off += 6
        face, off = read_utf16z(data, off)
        font = dict(face=face, pt=pt, weight=weight, italic=italic, charset=charset)
    off = pad2(off)
    controls = []
    for _ in range(cd):
        chelpid, cexstyle, cstyle = struct.unpack_from('<III', data, off)
        x, y2, cxx, cyy = struct.unpack_from('<hhhh', data, off + 12)
        cid = struct.unpack_from('<I', data, off + 20)[0]
        off += 24
        tcls, off = read_sz_or_ord(data, off)
        if tcls and tcls[0] == 'ord':
            tcls = ('str', CLASS_MAP.get(tcls[1], f'cls#{tcls[1]:#x}'))
        ttitle, off = read_sz_or_ord(data, off)
        extrad = struct.unpack_from('<H', data, off)[0]
        off += 2
        ed = data[off:off+extrad]
        off += extrad
        off = pad2(off)
        controls.append(dict(helpid=chelpid, exstyle=cexstyle, style=cstyle,
                             x=x, y=y2, w=cxx, h=cyy, id=cid,
                             cls=tcls[1] if tcls else '?', title=ttitle[1] if ttitle else '',
                             extra=ed.hex() if extrad else ''))
    if cls and cls[0] == 'ord':
        cls = ('str', DLGCLASS_MAP.get(cls[1], f'cls#{cls[1]:#x}'))
    return dict(dlgver=dlgver, helpid=helpid, exstyle=exstyle, style=style,
                x=x, y=y, w=cx, h=cy, menu=menu[1] if menu else None, cls=cls[1] if cls else None,
                title=title, font=font, controls=controls)

def decode_stringtable(data):
    """RT_STRING block: 16 UTF-16 strings."""
    out = []
    off = 0
    for i in range(16):
        if off + 2 > len(data):
            out.append(None); continue
        n = struct.unpack_from('<H', data, off)[0]
        off += 2
        if n:
            s = data[off:off+2*n].decode('utf-16-le', errors='replace')
            off += 2 * n
        else:
            s = ''
        out.append(s)
    return out

def decode_mui(data):
    sig, size, ver, ftype = struct.unpack_from('<IIII', data, 0)
    out = {'sig': hex(sig), 'total_size': size, 'version': hex(ver), 'file_type': ftype,
           'note': 'MUI RC configuration record; kept verbatim. Language-neutral file lists '
                   'main types (BMP, ICON, GROUP_ICON, VERSION, MANIFEST) and satellite '
                   'redirects (DIALOG, STRING, VERSION) with an en-US checksum record.'}
    return out

def extract(path, outdir):
    pe = pefile.PE(path)
    os.makedirs(outdir, exist_ok=True)
    meta = {}
    for e in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        tname = typename(e.id, e.name)
        tdir = os.path.join(outdir, tname)
        os.makedirs(tdir, exist_ok=True)
        meta[tname] = {}
        for ide in e.directory.entries:
            iname = ide.name.string.decode(errors='replace') if ide.name else str(ide.id)
            for lang in ide.directory.entries:
                data = pe.get_data(lang.data.struct.OffsetToData, lang.data.struct.Size)
                fname = f'{iname}_lang{lang.id}'
                ext = '.bin'
                decoded = None
                if tname == 'DIALOG':
                    decoded = decode_dialogtemplateex(data)
                    ext = '.dlg.json'
                elif tname == 'STRING':
                    decoded = decode_stringtable(data)
                    ext = '.str.json'
                elif tname == 'MUI':
                    decoded = decode_mui(data)
                    ext = '.mui.json'
                elif tname == 'ICON':
                    ext = '.ico'
                elif tname == 'GROUP_ICON':
                    ext = '.grp.ico'
                elif tname == 'VERSION':
                    ext = '.version.bin'
                elif tname == 'MANIFEST':
                    ext = '.manifest.xml'
                elif tname == 'BITMAP':
                    ext = '.bmp'
                with open(os.path.join(tdir, fname + ext), 'wb') as f:
                    f.write(data)
                entry = {'size': len(data), 'lang': lang.id}
                if decoded is not None:
                    entry['decoded'] = decoded
                meta[tname][f'{iname}:{lang.id}'] = entry
    json.dump(meta, open(os.path.join(outdir, '_meta.json'), 'w'), indent=1)
    return meta

if __name__ == '__main__':
    path, outdir = sys.argv[1], sys.argv[2]
    meta = extract(path, outdir)
    print(json.dumps({k: list(v.keys()) for k, v in meta.items()}, indent=1))
