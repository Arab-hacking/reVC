# -*- coding: utf-8 -*-
"""Дизассемблер ARM64 (AArch64) для libblackrussia-client.so."""
import sys
sys.path.insert(0, '/home/user/an/client_so/tools')
from elfv import Elf
from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM

E = Elf('/home/user/an/client_so/libblackrussia-client.so')
MD = Cs(CS_ARCH_ARM64, CS_MODE_ARM)
MD.detail = False

def disasm(va, count=40, show=True):
    o = E.v2o(va)
    code = E.d[o:o+count*4]
    out = []
    for ins in MD.disasm(code, va):
        out.append((ins.address, ins.mnemonic, ins.op_str))
        if show:
            print('%x: %-8s %s' % (ins.address, ins.mnemonic, ins.op_str))
        if len(out) >= count:
            break
    return out

def adrp_target(va_of_insn, op_str):
    """adrp xN, #imm -> адрес страницы"""
    if '#' not in op_str:
        return None
    try:
        imm = int(op_str.split('#')[1].split(',')[0], 0)
    except Exception:
        return None
    return imm

if __name__ == '__main__':
    va = int(sys.argv[1], 0)
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 40
    disasm(va, n)
