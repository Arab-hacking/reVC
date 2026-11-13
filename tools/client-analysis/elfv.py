# -*- coding: utf-8 -*-
"""Чтение ELF64: перевод виртуальных адресов в смещения файла, дамп данных, строки."""
import struct

class Elf:
    def __init__(self, path):
        self.path = path
        self.d = open(path, 'rb').read()
        d = self.d
        assert d[:4] == b'\x7fELF', 'не ELF'
        self.is64 = d[4] == 2
        self.le = d[5] == 1
        e = '<' if self.le else '>'
        self.e = e
        if self.is64:
            self.machine = struct.unpack_from(e+'H', d, 18)[0]
            self.entry = struct.unpack_from(e+'Q', d, 24)[0]
            self.phoff = struct.unpack_from(e+'Q', d, 32)[0]
            self.shoff = struct.unpack_from(e+'Q', d, 40)[0]
            self.phentsize, self.phnum = struct.unpack_from(e+'HH', d, 54)
            self.shentsize, self.shnum = struct.unpack_from(e+'HH', d, 58)
        else:
            self.machine = struct.unpack_from(e+'H', d, 18)[0]
            self.entry = struct.unpack_from(e+'I', d, 24)[0]
            self.phoff = struct.unpack_from(e+'I', d, 28)[0]
            self.shoff = struct.unpack_from(e+'I', d, 32)[0]
            self.phentsize, self.phnum = struct.unpack_from(e+'HH', d, 42)
            self.shentsize, self.shnum = struct.unpack_from(e+'HH', d, 46)
        self.segments = []   # (vaddr, filesz, offset, flags)
        self.sections = []   # (name, addr, size, offset, type)
        self._read_ph()
        self._read_sh()

    def _read_ph(self):
        d, e = self.d, self.e
        for i in range(self.phnum):
            o = self.phoff + i*self.phentsize
            if self.is64:
                p_type, p_flags = struct.unpack_from(e+'II', d, o)
                p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack_from(e+'QQQQQQ', d, o+8)
            else:
                p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = struct.unpack_from(e+'IIIIIIII', d, o)
            if p_type == 1:
                self.segments.append((p_vaddr, p_filesz, p_offset, p_flags))

    def _read_sh(self):
        d, e = self.d, self.e
        raw = []
        for i in range(self.shnum):
            o = self.shoff + i*self.shentsize
            if self.is64:
                sh_name, sh_type = struct.unpack_from(e+'II', d, o)
                sh_flags, sh_addr, sh_offset, sh_size = struct.unpack_from(e+'QQQQ', d, o+8)
            else:
                sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size = struct.unpack_from(e+'IIIIII', d, o)
            raw.append((sh_name, sh_type, sh_addr, sh_offset, sh_size))
        if not raw:
            return
        strtab_off = raw[-1][3] if raw[-1][1] == 3 else 0
        for (nm, typ, addr, off, size) in raw:
            name = b''
            if strtab_off:
                p = strtab_off + nm
                q = d.find(b'\0', p)
                name = d[p:q]
            self.sections.append((name.decode('utf-8', 'replace'), addr, size, off, typ))

    def v2o(self, va):
        """виртуальный адрес -> смещение в файле"""
        for (vaddr, filesz, off, _fl) in self.segments:
            if vaddr <= va < vaddr + filesz:
                return off + (va - vaddr)
        return None

    def sec_of(self, va):
        for s in self.sections:
            if s[1] and s[1] <= va < s[1] + s[2]:
                return s[0]
        return '?'

    def u32(self, va):
        o = self.v2o(va)
        return struct.unpack_from(self.e+'I', self.d, o)[0] if o is not None else None

    def string(self, va, maxlen=128):
        o = self.v2o(va)
        if o is None:
            return None
        q = self.d.find(b'\0', o, o+maxlen)
        return self.d[o:q].decode('utf-8', 'replace')
