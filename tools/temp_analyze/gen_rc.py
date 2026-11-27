#!/usr/bin/env python3
"""Generate .rc resource scripts for a saver from the decoded resource metadata."""
import json, os, sys

SEP = '/' * 80

def esc(s):
    return s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\t', '\\t')

STYLE_MAP = {
    0x50010001: 'BS_DEFPUSHBUTTON', 0x50010000: 'BS_PUSHBUTTON',
    0x50010003: 'BS_AUTOCHECKBOX',  0x50000007: 'BS_GROUPBOX',
    0x50000009: 'BS_AUTORADIOBUTTON', 0x50020002: 'SS_CENTER',
    0x50020000: 'SS_LEFT', 0x50021221: 'SS_LEFTNOWORDWRAP | WS_GROUP',
    0x50021001: 'SS_LEFT | WS_GROUP', 0x50021021: 'SS_LEFTNOWORDWRAP | WS_GROUP',
    0x50010018: 'TBS_HORZ | TBS_AUTOTICKS', 0x40020000: 'SS_LEFT',
    0x50000000: 'WS_CHILD',
}

def style_flags(style):
    return STYLE_MAP.get(style, f'{style:#010x}')

def gen_dialog_rc(dlg, dlg_id, out):
    out.append(SEP)
    out.append('// DLGTEMPLATEEX %s: "%s" (%d controls)' % (dlg_id, dlg['title'], len(dlg['controls'])))
    out.append('%s DIALOGEX %d, %d, %d, %d' % (dlg_id, dlg['x'], dlg['y'], dlg['w'], dlg['h']))
    out.append('STYLE %s' % hex(dlg['style']))
    out.append('EXSTYLE %s' % hex(dlg['exstyle']))
    out.append('CAPTION "%s"' % esc(dlg['title']))
    font = dlg.get('font') or {}
    out.append('FONT %d, "%s", 400, 0, 0x0' % (font.get('pt', 8), font.get('face', 'MS Shell Dlg')))
    out.append('BEGIN')
    for c in dlg['controls']:
        cid = c['id'] if c['id'] != 0xFFFFFFFF else -1
        title = c['title'] if c['title'] else ''
        cls = c['cls']
        # CONTROL form: the style word is emitted verbatim from the decoded
        # DLGTEMPLATEEX (it already carries WS_CHILD | WS_VISIBLE | class style).
        out.append('    CONTROL "%s", %d, "%s", %s, %d, %d, %d, %d' % (
            esc(title), cid, cls, hex(c['style']),
            c['x'], c['y'], c['w'], c['h']))
    out.append('END')

def gen_strings(meta, out):
    out.append(SEP)
    out.append('// RT_STRING blocks of the .mui satellite. The MUI loader maps LoadStringW(id)')
    out.append('// onto satellite block (id/16)+1, entry id%16 - the block numbers below are')
    out.append('// therefore +1 versus the language-neutral IDs documented in src/resources.h.')
    blocks = sorted(meta['STRING'].items(), key=lambda kv: int(kv[0].split(':')[0]))
    for key, entry in blocks:
        block = int(key.split(':')[0])
        strs = entry['decoded']
        used = [i for i, s in enumerate(strs) if s]
        if not used:
            continue
        lo, hi = min(used), max(used)
        out.append('')
        out.append('// block %d (language-neutral IDs %d..%d)' % (block, (block-1)*16+lo, (block-1)*16+hi))
        out.append('STRINGTABLE')
        out.append('LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US')
        out.append('BEGIN')
        for i in range(lo, hi + 1):
            lid = (block - 1) * 16 + i
            s = strs[i] if strs[i] else '\\x01'
            out.append('    %-6d "%s"' % (lid, esc(s)))
        out.append('END')

def scr_rc(saver):
    return [
        SEP,
        '// %s.scr - resource script (compiles into %s.scr)' % (saver, saver),
        '//',
        '// Resource tree of the original %s.scr (PE32+ x64, ImageBase 0x140000000):' % saver,
        '//',
        '//   Type "BMP"        ID 102  lang 0x409   36311 bytes  128x128 RLE sprite texture',
        '//   Type RT_ICON      ID 1,2  lang 0x409   16x16 4bpp + 32x32 8bpp frames',
        '//   Type RT_GROUP_ICON ID 101 lang 0x409   icon group "%s"' % saver,
        '//   Type RT_VERSION   ID 1    lang 0x409   VS_VERSIONINFO',
        '//   Type RT_MANIFEST  ID 1    lang 0x409   Per-Monitor DPI aware manifest',
        '//   Type "MUI"        ID 1    lang 0x409   MUI configuration record (224 bytes)',
        '//',
        '// Strings/dialogs are NOT in the .scr: LoadStringW/FindResource resolve through',
        '// the MUI system into %s.scr.mui (see resources/mui_files/).' % saver,
        SEP,
        '',
        '#include "src/resources.h"',
        '',
        '// The multi-frame .ico below expands to RT_ICON 1 (32x32 1bpp) and',
        '// RT_ICON 2 (32x32 4bpp) plus RT_GROUP_ICON 101 - matching the original',
        '// resource directory. The raw RT_ICON frames are kept in resources/ICON/.',
        '101 ICON "resources/ICON/group_101.ico"',
        '',
        '// ------------------------------------------------------------------',
        '// Star sprite: custom RLE format, decoded at runtime by the engine',
        '// (analysis/texture_rle_format.md). The type name "BMP" is a *named*',
        '// type, not RT_BITMAP: FindResourceW(NULL, MAKEINTRESOURCEW(102), L"BMP").',
        '// ------------------------------------------------------------------',
        '102 BMP "resources/BMP/102_lang1033.texture.bin"',
        '',
        '1 VERSIONINFO',
        ' FILEVERSION 10,0,19041,4355',
        ' PRODUCTVERSION 10,0,19041,4355',
        ' FILEFLAGSMASK 0x3fL',
        ' FILEFLAGS 0x0L',
        ' FILEOS 0x40004L',
        ' FILETYPE 0x1L',
        ' FILESUBTYPE 0x0L',
        'BEGIN',
        '    BLOCK "StringFileInfo"',
        '    BEGIN',
        '        BLOCK "040904b0"',
        '        BEGIN',
        '            VALUE "CompanyName", "Microsoft Corporation"',
        '            VALUE "FileDescription", "%s Screen Saver"' % saver,
        '            VALUE "FileVersion", "10.0.19041.4355 (WinBuild.160101.0800)"',
        '            VALUE "InternalName", "%s"' % saver,
        '            VALUE "LegalCopyright", "\\xa9 Microsoft Corporation. All rights reserved."',
        '            VALUE "OriginalFilename", "%s"' % saver,
        '            VALUE "ProductName", "Microsoft\\xae Windows\\xae Operating System"',
        '            VALUE "ProductVersion", "10.0.19041.4355"',
        '        END',
        '    END',
        '    BLOCK "VarFileInfo"',
        '    BEGIN',
        '        VALUE "Translation", 0x409, 1200',
        '    END',
        'END',
        '',
        '1 24 "resources/MANIFEST/1_lang1033.manifest.xml"',
        '',
        SEP,
        '// MUI configuration record (RT_MUI, 224 bytes, signature 0xFECDFECD).',
        '// Kept verbatim; lists main-file types (BMP/ICON/GROUP_ICON/VERSION/MANIFEST)',
        '// and the satellite redirects (DIALOG/STRING/VERSION) with the en-US checksum.',
        SEP,
        '1 MUI "resources/MUI/1_lang1033.bin"',
        '',
    ]

def mui_rc(saver, meta):
    out = [
        SEP,
        '// %s.scr.mui - resource script (compiles into %s.scr.mui)' % (saver, saver),
        '//',
        '// Resource-only PE32 (x86) satellite; no entry point, no exports.',
        '// All resources are language 0x409 (en-US):',
        '//   RT_MUI     ID 1     configuration record pointing back to the .scr',
        '//   RT_DIALOG  ID 105   "%s Settings"' % saver,
        '//   RT_DIALOG  ID 200   "Display Settings" (single monitor)',
        '//   RT_DIALOG  ID 201   "Display Settings" (multi-monitor, tabbed)',
        '//   RT_STRING  blocks 1 / 19 / 132 / 133 / 138 / 139',
        '//   RT_VERSION ID 1     10.0.19041.1',
        SEP,
        '',
        '#include "src/resources.h"',
        '#include <winuser.h>   // windres: style constant definitions',
        '',
    ]
    for key in ('105:1033', '200:1033', '201:1033'):
        gen_dialog_rc(meta['DIALOG'][key]['decoded'], key.split(':')[0], out)
    gen_strings(meta, out)
    out += [
        '',
        '1 MUI "resources/mui_files/MUI/1_lang1033.bin"',
        '',
        '1 VERSIONINFO',
        ' FILEVERSION 10,0,19041,1',
        ' PRODUCTVERSION 10,0,19041,1',
        ' FILETYPE 0x2L',
        'BEGIN',
        '    BLOCK "StringFileInfo"',
        '    BEGIN',
        '        BLOCK "040904b0"',
        '        BEGIN',
        '            VALUE "FileDescription", "%s Screen Saver Resource DLL"' % saver,
        '            VALUE "FileVersion", "10.0.19041.1"',
        '        END',
        '    END',
        'END',
        '',
    ]
    return out

def main(saver, resdir):
    scr_meta = json.load(open(os.path.join(resdir, 'resources', '_meta.json')))
    mui_meta = json.load(open(os.path.join(resdir, 'resources', 'mui_files', '_meta.json')))
    with open('%s.scr.rc' % saver, 'w', encoding='utf-8') as f:
        f.write('\n'.join(scr_rc(saver)))
    with open('%s.scr.mui.rc' % saver, 'w', encoding='utf-8') as f:
        f.write('\n'.join(mui_rc(saver, mui_meta)))
    print('%s: wrote %s.scr.rc and %s.scr.mui.rc' % (saver, saver, saver))

if __name__ == '__main__':
    os.chdir(sys.argv[1])
    main(sys.argv[2], '.')
