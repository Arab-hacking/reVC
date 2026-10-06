import zipfile, struct, sys, os

def btx_rgba(w, h, mips=1, rgb=(200,120,40)):
    """a .btx in the raw RGBA8 shape BR writes (u32 flags + KTX11 + mips)"""
    out = struct.pack('<I', 0)
    kv = 0
    out += struct.pack('<12sIIIIIIIIIIIII',
        b'\xabKTX 11\xbb\r\n\x1a\n',
        0x04030201,          # endianness
        0x1401,              # glType = GL_UNSIGNED_BYTE
        0,                   # glTypeSize
        0x1908,              # glFormat = GL_RGBA
        0x8058,              # glInternal = GL_RGBA8
        0x1908,              # glBaseInternal
        w, h, 0, 0, 1, mips, kv)
    mw, mh = w, h
    for m in range(mips):
        data = bytearray()
        for y in range(mh):
            for x in range(mw):
                data += bytes(((rgb[0]+x) & 0xff, (rgb[1]+y) & 0xff, rgb[2], 0xff))
        pad = (4 - (len(data) % 4)) % 4
        out += struct.pack('<I', len(data) + pad) + bytes(data) + b'\0'*pad
        mw, mh = max(1, mw//2), max(1, mh//2)
    return bytes(out)

z = sys.argv[1]
mod = open(sys.argv[2], 'rb').read()
with zipfile.ZipFile(z, 'w') as f:
    f.writestr(zipfile.ZipInfo('glendale.mod'), mod, zipfile.ZIP_STORED)
    f.writestr(zipfile.ZipInfo('infernus/body.btx'), btx_rgba(32, 32, 5), zipfile.ZIP_DEFLATED)
    f.writestr(zipfile.ZipInfo('infernus/wheel.btx'), btx_rgba(33, 17, 1, (10, 200, 60)), zipfile.ZIP_DEFLATED)
    f.writestr(zipfile.ZipInfo('infernus/readme.txt'), b'not interesting')
print('zip written', z, os.path.getsize(z))
