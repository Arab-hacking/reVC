# -*- coding: utf-8 -*-
"""Поиск обращений (adrp+add / adrp+ldr|str) к заданному виртуальному адресу в .text."""
import sys, struct
sys.path.insert(0, '/home/user/an/client_so/tools')
from elfv import Elf

E = Elf('/home/user/an/client_so/libblackrussia-client.so')

def sx(v, bits):
    return v - (1 << bits) if v & (1 << (bits-1)) else v

def scan(targets, lo=None, hi=None):
    d = E.d
    text = [s for s in E.sections if s[0] == '.text'][0]
    taddr, tsize, toff = text[1], text[2], text[3]
    lo = lo or taddr
    hi = hi or (taddr + tsize)
    res = {}
    words = struct.unpack_from('<%dI' % ((hi-lo)//4), d, E.v2o(lo))
    # первый проход: все adrp
    adrps = {}
    for i, w in enumerate(words):
        if (w & 0x9F000000) == 0x90000000:
            pc = lo + i*4
            rd = w & 0x1f
            immlo = (w >> 29) & 3
            immhi = (w >> 5) & 0x7ffff
            imm = sx((immhi << 2) | immlo, 21) << 12
            adrps[i] = (rd, (pc & ~0xfff) + imm)
    for i, (rd, page) in adrps.items():
        for j in range(1, 5):
            k = i + j
            if k not in range(len(words)):
                break
            w = words[k]
            pc = lo + k*4
            # add xRd, xRn, #imm12  (64-бит)
            if (w & 0xFF800000) == 0x91000000:
                rn = (w >> 5) & 0x1f
                rdd = w & 0x1f
                imm12 = (w >> 10) & 0xfff
                if rn == rd and rdd == rd:
                    a = page + imm12
                    if a in targets:
                        res.setdefault(a, []).append((pc, 'add', hex(page)))
                    break
            # ldr/str xRt, [xRn, #imm12*8]
            if (w & 0xFFC00000) in (0xF9400000, 0xF9000000):
                rn = (w >> 5) & 0x1f
                imm12 = (w >> 10) & 0xfff
                if rn == rd:
                    a = page + imm12*8
                    if a in targets:
                        kind = 'ldr' if (w & 0xFFC00000) == 0xF9400000 else 'str'
                        res.setdefault(a, []).append((pc, kind, hex(page)))
                    break
            # ldr/str w
            if (w & 0xFFC00000) in (0xB9400000, 0xB9000000):
                rn = (w >> 5) & 0x1f
                imm12 = (w >> 10) & 0xfff
                if rn == rd:
                    a = page + imm12*4
                    if a in targets:
                        kind = 'ldrw' if (w & 0xFFC00000) == 0xB9400000 else 'strw'
                        res.setdefault(a, []).append((pc, kind, hex(page)))
                    break
            if (w & 0x9F000000) == 0x90000000:
                break
    return res

if __name__ == '__main__':
    tg = set(int(x, 0) for x in sys.argv[1:])
    r = scan(tg)
    for a in sorted(tg):
        print(hex(a), '->', ' '.join('%x(%s)' % (p, k) for p, k, _ in r.get(a, [])) or 'нет ссылок')
