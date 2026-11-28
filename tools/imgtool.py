#!/usr/bin/env python3
"""
imgtool.py - сборка/просмотр/распаковка IMG-архивов GTA III / Vice City / San Andreas
(формат "IMG v1": отдельный .dir с таблицей и .img с данными).

Формат (ровно как читает reVC, см. CDirectory::DirectoryInfo и CStreaming::LoadCdDirectory):

    .dir : последовательность записей по 32 байта, без заголовка
           struct Entry {
               uint32 offset;   // смещение в СЕКТОРАХ по 2048 байт от начала .img
               uint32 size;     // размер в СЕКТОРАХ по 2048 байт
               char   name[24]; // имя с расширением, обычно в верхнем регистре
           };
    .img : данные файлов подряд, каждый выровнен на границу 2048 байт
           (хвост добивается нулями; игра читает size*2048 и парсит до конца чанка)

    Движок ищет файл по имени, точка должна стоять не дальше 20-го символа,
    регистр не важен (strncasecmp). Если один и тот же файл есть в нескольких
    архивах, побеждает тот архив, который загружен ПОЗЖЕ: CStreaming::LoadCdDirectory
    обходит образы от последнего к первому, а повторная запись игнорируется.

Использование:
    python3 imgtool.py pack   OUT.img FILE [FILE ...]
    python3 imgtool.py list   IN.img
    python3 imgtool.py unpack IN.img [OUTDIR]
    python3 imgtool.py verify IN.img ORIG [ORIG ...]     # сверить содержимое с оригиналами
"""
import hashlib
import os
import struct
import sys

SECTOR = 2048           # CDSTREAM_SECTOR_SIZE
ENTRY = struct.Struct('<II24s')
MAX_NAME = 23           # name[23] движок затирает нулём, значит имя максимум 23 символа


def dir_path_for(img_path):
    return os.path.splitext(img_path)[0] + '.dir'


def check_name(name):
    raw = name.encode('ascii', 'replace')
    if len(raw) > MAX_NAME:
        sys.exit('имя "%s" длиннее %d символов' % (name, MAX_NAME))
    dot = name.find('.')
    if dot < 0:
        sys.exit('в имени "%s" нет расширения' % name)
    if dot > 20:
        sys.exit('в имени "%s" точка дальше 20-го символа (движок такое пропускает)' % name)
    return raw


def pack(img_path, files, verbose=True):
    assert files, 'нечего упаковывать'
    entries = []
    offset = 0
    with open(img_path, 'wb') as img:
        for path in files:
            data = open(path, 'rb').read()
            name = os.path.basename(path).upper()
            raw = check_name(name)
            size = (len(data) + SECTOR - 1) // SECTOR
            entries.append((offset, size, raw, name, len(data), path))
            img.write(data)
            img.write(b'\0' * (size * SECTOR - len(data)))
            offset += size

    dpath = dir_path_for(img_path)
    with open(dpath, 'wb') as d:
        for off, size, raw, _name, _real, _path in entries:
            d.write(ENTRY.pack(off, size, raw))

    if verbose:
        print('%-28s %10s %8s %8s  %s' % ('файл', 'байт', 'секторов', 'смещение', 'хэш sha1'))
        for off, size, raw, name, real, path in entries:
            h = hashlib.sha1(open(path, 'rb').read()).hexdigest()[:12]
            print('%-28s %10d %8d %8d  %s' % (name, real, size, off, h))
        print('\n%s: %d файлов, %d байт' % (img_path, len(entries), offset * SECTOR))
        print('%s: %d записей по 32 байта' % (dpath, len(entries)))
    return entries


def read_entries(img_path):
    dpath = dir_path_for(img_path)
    if not os.path.isfile(dpath):
        sys.exit('нет файла таблицы: %s' % dpath)
    raw = open(dpath, 'rb').read()
    if len(raw) % ENTRY.size:
        sys.exit('размер %s не кратен 32 - это не таблица IMG' % dpath)
    out = []
    for i in range(len(raw) // ENTRY.size):
        off, size, name = ENTRY.unpack_from(raw, i * ENTRY.size)
        out.append((off, size, name.split(b'\0')[0].decode('ascii', 'replace')))
    return out


def list_img(img_path):
    entries = read_entries(img_path)
    total = os.path.getsize(img_path)
    total_sectors = (total + SECTOR - 1) // SECTOR
    print('%-24s %10s %8s %8s %8s %9s' % ('имя', 'байт', 'секторов', 'смещение', 'конец', 'статус'))
    bad = 0
    for off, size, name in entries:
        end = off + size
        ok = end <= total_sectors
        if not ok:
            bad += 1
        print('%-24s %10d %8d %8d %8d %9s' % (name, size * SECTOR, size, off, end,
                                              'ok' if ok else 'ЗА ПРЕДЕЛАМИ ФАЙЛА'))
    print('итого: %d записей, %d секторов (%d байт), %s' %
          (len(entries), total_sectors, total, 'ошибок нет' if not bad else 'ошибок: %d' % bad))
    return bad


def unpack(img_path, outdir):
    entries = read_entries(img_path)
    os.makedirs(outdir, exist_ok=True)
    with open(img_path, 'rb') as img:
        for off, size, name in entries:
            img.seek(off * SECTOR)
            data = img.read(size * SECTOR)
            out = os.path.join(outdir, name.lower())
            open(out, 'wb').write(data)
            print('%-24s -> %-40s %d байт (без добивки: %d)' %
                  (name, out, len(data), len(data.rstrip(b'\0'))))
    return entries


def verify(img_path, origs):
    entries = read_entries(img_path)
    by_name = {n.upper(): (o, s) for o, s, n in entries}
    ok = True
    with open(img_path, 'rb') as img:
        for path in origs:
            name = os.path.basename(path).upper()
            want = open(path, 'rb').read()
            if name not in by_name:
                print('НЕТ В АРХИВЕ: %s' % name)
                ok = False
                continue
            off, size = by_name[name]
            img.seek(off * SECTOR)
            got = img.read(size * SECTOR)[:len(want)]
            same = got == want
            print('%-24s %s  %s' % (name, 'совпадает' if same else 'ОТЛИЧАЕТСЯ',
                                    hashlib.sha1(want).hexdigest()))
            ok = ok and same
    print('ИТОГ:', 'всё совпадает' if ok else 'есть расхождения')
    return ok


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    cmd, img = argv[1], argv[2]
    if cmd == 'pack':
        pack(img, argv[3:])
        return 0
    if cmd == 'list':
        return 1 if list_img(img) else 0
    if cmd == 'unpack':
        unpack(img, argv[3] if len(argv) > 3 else 'unpacked')
        return 0
    if cmd == 'verify':
        return 0 if verify(img, argv[3:]) else 1
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
