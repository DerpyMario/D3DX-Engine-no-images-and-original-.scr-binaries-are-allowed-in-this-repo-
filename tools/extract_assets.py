#!/usr/bin/env python3
"""
extract_assets.py - pull the binary assets out of YOUR OWN copies of the
Windows Vista screen savers, and of the Windows XP / Windows Server 2003 ones,
so that the reconstructed sources can be built with their original artwork.

This repository deliberately contains no images and none of the original
binaries.  Everything this script writes goes into an output directory that is
ignored by git (default: <repository>/assets).

    python tools/extract_assets.py C:\\Windows\\System32            (all savers found there)
    python tools/extract_assets.py path\\to\\Aurora.scr -o assets
    python tools/extract_assets.py --list path\\to\\ssBranded.scr   (just show the resources)

The Windows XP / Server 2003 files go into <output>/winxp (the x64 editions
name the 32-bit Direct3D savers wss*.scr, the 32-bit ones ss*.scr).  Where
both releases use the same file name (logon.scr, sstext3d.scr), the file
version tells them apart.

The build then picks the files up through SCRNSAVE_ASSET_DIR (see BUILDING.md).

Only the Python standard library is needed.
"""
import argparse
import os
import struct
import sys

RT_NAMES = {1: "CURSOR", 2: "BITMAP", 3: "ICON", 4: "MENU", 5: "DIALOG", 6: "STRING", 9: "ACCELERATOR",
            10: "RCDATA", 11: "MESSAGETABLE", 12: "GROUP_CURSOR", 14: "GROUP_ICON", 16: "VERSION",
            24: "MANIFEST"}


# ---------------------------------------------------------------------------
# Minimal PE resource reader
# ---------------------------------------------------------------------------
class PEResources:
    def __init__(self, path):
        with open(path, "rb") as f:
            self.data = f.read()
        d = self.data
        if d[:2] != b"MZ":
            raise ValueError("%s: not an executable" % path)
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        if d[pe:pe + 4] != b"PE\0\0":
            raise ValueError("%s: not a PE image" % path)
        nsec, = struct.unpack_from("<H", d, pe + 6)
        optsize, = struct.unpack_from("<H", d, pe + 20)
        opt = pe + 24
        magic, = struct.unpack_from("<H", d, opt)
        ddir = opt + (112 if magic == 0x20B else 96)
        self.sections = []
        sec = opt + optsize
        for i in range(nsec):
            vsize, va, rsize, raw = struct.unpack_from("<IIII", d, sec + i * 40 + 8)
            self.sections.append((va, max(vsize, rsize), raw))
        rva, size = struct.unpack_from("<II", d, ddir + 2 * 8)
        self.entries = []        # (type, name, lang, bytes)
        if rva:
            self.base = self.off(rva)
            self._walk(0, [])

    def off(self, rva):
        for va, size, raw in self.sections:
            if va <= rva < va + size:
                return rva - va + raw
        raise ValueError("RVA 0x%x is not inside any section" % rva)

    def _name(self, v):
        if v & 0x80000000:
            p = self.base + (v & 0x7FFFFFFF)
            n, = struct.unpack_from("<H", self.data, p)
            return self.data[p + 2:p + 2 + n * 2].decode("utf-16-le")
        return v

    def _walk(self, rel, path):
        d = self.data
        p = self.base + rel
        nnamed, nid = struct.unpack_from("<HH", d, p + 12)
        for i in range(nnamed + nid):
            name, offset = struct.unpack_from("<II", d, p + 16 + i * 8)
            key = self._name(name)
            if offset & 0x80000000:
                self._walk(offset & 0x7FFFFFFF, path + [key])
            else:
                rva, size = struct.unpack_from("<II", d, self.base + offset)
                o = self.off(rva)
                self.entries.append((path[0], path[1], key, d[o:o + size]))

    def find(self, rtype, name):
        for t, n, lang, blob in self.entries:
            if t == rtype and n == name:
                return blob
        return None

    def all_of_type(self, rtype):
        return [(n, blob) for t, n, lang, blob in self.entries if t == rtype]


    def file_version_major(self):
        """Major number of the file version (5: Windows XP / 2003, 6: Vista)."""
        blob = self.find(16, 1)
        if blob is None:
            return None
        i = blob.find(struct.pack("<I", 0xFEEF04BD))
        if i < 0:
            return None
        ms, = struct.unpack_from("<I", blob, i + 8)
        return ms >> 16


# ---------------------------------------------------------------------------
# Converters
# ---------------------------------------------------------------------------
def build_ico(res, group_name):
    """Re-assemble an .ico file from RT_GROUP_ICON + RT_ICON resources."""
    grp = res.find(14, group_name)
    if grp is None:
        return None
    reserved, typ, count = struct.unpack_from("<HHH", grp, 0)
    images = []
    for i in range(count):
        w, h, colors, res0, planes, bits, size, icon_id = struct.unpack_from("<BBBBHHIH", grp, 6 + i * 14)
        blob = res.find(3, icon_id)
        if blob is None:
            raise ValueError("icon %d referenced by group %r is missing" % (icon_id, group_name))
        images.append((w, h, colors, res0, planes, bits, blob))
    out = [struct.pack("<HHH", 0, 1, len(images))]
    offset = 6 + 16 * len(images)
    for w, h, colors, res0, planes, bits, blob in images:
        out.append(struct.pack("<BBBBHHII", w, h, colors, res0, planes, bits, len(blob), offset))
        offset += len(blob)
    out += [img[6] for img in images]
    return b"".join(out)


def build_bmp(dib):
    """Turn an RT_BITMAP resource (a packed DIB) into a .bmp file."""
    hdr_size, = struct.unpack_from("<I", dib, 0)
    bits, = struct.unpack_from("<H", dib, 14)
    compression, = struct.unpack_from("<I", dib, 16)
    colors, = struct.unpack_from("<I", dib, 32)
    palette = 0
    if bits <= 8:
        palette = (colors or (1 << bits)) * 4
    elif compression == 3 and hdr_size == 40:
        palette = 12                                    # BI_BITFIELDS masks
    off_bits = 14 + hdr_size + palette
    return struct.pack("<2sIHHI", b"BM", 14 + len(dib), 0, 0, off_bits) + dib


# ---------------------------------------------------------------------------
# What to extract, and where it goes (relative to the output directory).
# The relative paths are the ones the .rc files of this tree refer to.
#
#   (resource type, resource name, converter, output path)
# ---------------------------------------------------------------------------
def numbered(rtype, first, last, converter, pattern, start=None):
    return [(rtype, n, converter, pattern % ((n - first) + (first if start is None else start)))
            for n in range(first, last + 1)]


ASSETS = {
    "aurora.scr": [
        (14, 101, "ico", "aurora/res/aurora.ico"),
        ("BMP", 102, "raw", "aurora/res/arc.tga"),
    ],
    "bubbles.scr": [
        (14, 101, "ico", "bubbles/res/bubbles.ico"),
        ("BMP", 117, "raw", "bubbles/res/shadow.tga"),
        ("BMP", 118, "raw", "bubbles/res/sphere_lit.tga"),
        ("BMP", 119, "raw", "bubbles/res/sphere_solid.tga"),
        ("BMP", 120, "raw", "bubbles/res/sphere_highlight.tga"),
        ("BMP", 121, "raw", "bubbles/res/sphere_glass.tga"),
    ],
    "sstext3d.scr": [
        (14, 101, "ico", "text3d/res/text3d.ico"),
        ("JPG", 102, "raw", "text3d/res/texture.jpg"),
        ("JPG", 103, "raw", "text3d/res/environment.jpg"),
    ],
    "ssbranded.scr":
        [(14, "IDI_MAIN_ICON", "ico", "branded/res/ssbranded.ico"),
         (10, 10001, "raw", "branded/res/crosswipe.tga")] +
        [(10, 10003 + n, "raw", "branded/res/logo%02d.tga" % n) for n in range(50)] +
        [(10, 10055, "raw", "branded/res/msflarebgnd.tga")] +
        [(10, 10056 + n, "raw", "branded/res/msflare%02d.tga" % n) for n in range(62)] +
        [(10, 10119 + n, "raw", "branded/res/wipe%d.tga" % n) for n in range(5)] +
        [(10, 10125 + n, "raw", "branded/res/aurora%d.tga" % n) for n in range(3)] +
        [(10, 10130, "raw", "branded/res/flare.tga"),
         (10, 10133, "raw", "branded/res/pill.tga"),
         (10, 10153, "raw", "branded/res/unused.tga")],
    "logon.scr":
        [(2, "IDB_FLARE%02d" % n, "bmp", "logon/res/flare%02d.bmp" % n) for n in range(1, 46)] +
        [(2, "IDB_SMALLFLARE%02d" % n, "bmp", "logon/res/smallflare%02d.bmp" % n) for n in range(1, 46)],
}


def xp(entries):
    """The output paths of the Windows XP / Server 2003 savers."""
    return [(t, n, c, "winxp/" + rel) for t, n, c, rel in entries]


XP_LOGON = xp(
    [(2, 200 + n, "bmp", "logon/res/%s.bmp" % name) for n, name in
     enumerate(["standard", "professional", "enterprise", "datacenter", "home", "embedded",
                "smallbusiness", "web", "tabletpc"])] +
    [(2, 212, "bmp", "logon/res/appliance.bmp"),
     (14, 209, "ico", "logon/res/logon.ico"),
     (14, 210, "ico", "logon/res/logon16.ico")])

XP_TEXT3D = xp([
    (14, 101, "ico", "text3d/res/text3d.ico"),
    ("JPG", 102, "raw", "text3d/res/texture.jpg"),
    ("JPG", 103, "raw", "text3d/res/environment.jpg"),
])

XP_ASSETS = {
    "scrnsave.scr": xp([(14, "IW", "ico", "blank/res/blank.ico")]),
    "ssstars.scr": xp([(14, 100, "ico", "stars/res/stars.ico")]),
    "ssmyst.scr": xp([(14, 100, "ico", "mystify/res/mystify.ico")]),
    "ssbezier.scr": xp([(14, 100, "ico", "bezier/res/bezier.ico")]),
    "ssmarque.scr": xp([(14, 100, "ico", "marquee/res/marquee.ico")]),
    "ssflwbox.scr": xp([(14, 101, "ico", "flowerbox/res/flowerbox.ico")]),
    "sspipes.scr": xp([
        (14, 101, "ico", "pipes/res/pipes.ico"),
        (2, 107, "bmp", "pipes/res/deftex.bmp"),
    ]),
    "ss3dfo.scr": xp([
        (14, 101, "ico", "3dfo/res/ss3dfo.ico"),
        ("JPG", 3000, "raw", "3dfo/res/flag.jpg"),
        ("DDS", 3001, "raw", "3dfo/res/logo.dds"),
        ("PNG", 3002, "raw", "3dfo/res/logo.png"),
    ]),
    "ssmypics.scr": xp([
        (14, 2005, "ico", "mypics/res/ssmypics.ico"),
        (2, 2009, "bmp", "mypics/res/pixel.bmp"),
    ]),
}
for _n in ("ssflwbox.scr", "sspipes.scr", "ss3dfo.scr"):
    XP_ASSETS["w" + _n] = XP_ASSETS[_n]          # x64 editions (WOW64)
XP_ASSETS["wsstext3d.scr"] = XP_TEXT3D

# File names both releases use: told apart by the file version
SHARED = {
    "logon.scr": XP_LOGON,
    "sstext3d.scr": XP_TEXT3D,
}


def table_for(key, res):
    if key in SHARED:
        major = res.file_version_major()
        if major is not None and major < 6:
            return SHARED[key]
    if key in ASSETS:
        return ASSETS[key]
    return XP_ASSETS.get(key)


def classify(blob):
    if blob[:2] == b"BM":
        return "bmp"
    if blob[:3] == b"\xff\xd8\xff":
        return "jpg"
    if blob[:8] == b"\x89PNG\r\n\x1a\n":
        return "png"
    if blob[:4] == b"xof ":
        return "x"
    if blob[:4] == b"DDS ":
        return "dds"
    if len(blob) > 18 and blob[0] == 0 and blob[1] == 0 and blob[2] in (2, 10) and blob[16] in (24, 32):
        return "tga"
    head = blob[:256]
    if head and all(32 <= c < 127 or c in (9, 10, 13) for c in head):
        return "txt"
    return "bin"


def type_name(t):
    return RT_NAMES.get(t, str(t)) if isinstance(t, int) else t


def list_resources(path):
    res = PEResources(path)
    print("%s" % path)
    for t, n, lang, blob in res.entries:
        print("  %-12s %-28s lang %-5s %9d bytes  %s" % (type_name(t), n, lang, len(blob), classify(blob)))


def extract(path, outdir, verbose=True):
    key = os.path.basename(path).lower()
    res = PEResources(path)
    table = table_for(key, res)
    if table is None:
        print("%s: no asset table for this file (use --list to inspect it)" % path)
        return 0
    count = 0
    for rtype, name, conv, rel in table:
        if conv == "ico":
            blob = build_ico(res, name)
        else:
            blob = res.find(rtype, name)
            if blob is not None and conv == "bmp":
                blob = build_bmp(blob)
        if blob is None:
            print("  ! %s: resource %s/%s not found" % (key, type_name(rtype), name))
            continue
        dest = os.path.join(outdir, rel.replace("/", os.sep))
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "wb") as f:
            f.write(blob)
        count += 1
        if verbose:
            print("  %-40s %9d bytes" % (rel, len(blob)))
    print("%s: %d asset(s) written to %s" % (os.path.basename(path), count, outdir))
    return count


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("inputs", nargs="+", help="original .scr files, or a directory that contains them")
    ap.add_argument("-o", "--output", default=os.path.join(os.path.dirname(here), "assets"),
                    help="output directory (default: <repository>/assets)")
    ap.add_argument("--list", action="store_true", help="list the resources instead of extracting")
    ap.add_argument("-q", "--quiet", action="store_true")
    args = ap.parse_args()

    files = []
    for inp in args.inputs:
        if os.path.isdir(inp):
            for fn in sorted(os.listdir(inp)):
                if fn.lower() in ASSETS or fn.lower() in XP_ASSETS:
                    files.append(os.path.join(inp, fn))
        else:
            files.append(inp)
    if not files:
        sys.exit("no screen savers found in the given location(s)")

    total = 0
    for f in files:
        if args.list:
            list_resources(f)
        else:
            total += extract(f, args.output, not args.quiet)
    if not args.list:
        print("done: %d file(s) in %s" % (total, args.output))


if __name__ == "__main__":
    main()
