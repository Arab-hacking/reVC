#!/usr/bin/env python3
"""
Проверка полноты исходников reVC: все локальные #include "..." должны разрешаться
(заголовки в своей папке или в одной из стандартных include-папок проекта).

Запуск из корня репозитория:
    python3 tools/sa-vehicle-verification/check_includes.py            # только src/**
    python3 tools/sa-vehicle-verification/check_includes.py --dirs     # + список папок
    python3 tools/sa-vehicle-verification/check_includes.py --vcxproj build/reVC.vcxproj --config d3d9
        # взять include-папки из сгенерированного Visual Studio проекта (самый точный вариант
        # для Windows-сборки: эмулирует то, что делает msbuild)

Известные платформенные исключения (не собираются в десктопных конфигурациях):
Android (JavaWrapper.h, AndroidMain.h), PS2 (eetypes.h, libpad.h),
Miles Sound System (mss.h), системные заголовки в кавычках (assert.h, ctype.h).
"""
import os
import re
import sys
import argparse

PLATFORM_ONLY = {
    'JavaWrapper.h', 'AndroidMain.h', 'eetypes.h', 'libpad.h', 'mss.h',
    'assert.h', 'ctype.h', 'AL/efx.h', 'eax.h', 'eax-util.h',
}

ROOT = None


def default_dirs(root):
    dirs = ['src']
    src = os.path.join(root, 'src')
    for name in sorted(os.listdir(src)):
        p = os.path.join(src, name)
        if os.path.isdir(p):
            dirs.append(os.path.join('src', name))
    return dirs


def dirs_from_vcxproj(root, path, config_key):
    txt = open(path, encoding='utf-8', errors='replace').read()
    blocks = re.findall(
        r"<ItemDefinitionGroup Condition=\"'\$\(Configuration\)\|\$\(Platform\)'=='([^']+)'\">(.*?)</ItemDefinitionGroup>",
        txt, re.S)
    projdir = os.path.dirname(os.path.abspath(path))
    for name, body in blocks:
        if config_key in name:
            m = re.search(r'<AdditionalIncludeDirectories>([^<]*)</AdditionalIncludeDirectories>', body)
            if m:
                dirs = []
                for d in m.group(1).split(';'):
                    d = d.strip().replace('\\', '/')
                    if not d:
                        continue
                    # пути в vcxproj относительны каталогу проекта
                    dirs.append(os.path.normpath(os.path.join(projdir, d)))
                return dirs
    sys.exit('в %s нет конфигурации с "%s"' % (path, config_key))


def resolve(inc, srcdir, dirs):
    if os.path.exists(os.path.join(srcdir, inc)):
        return True
    for d in dirs:
        if os.path.exists(os.path.join(d, inc)):
            return True
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..')))
    ap.add_argument('--vcxproj')
    ap.add_argument('--config', default='d3d9')
    ap.add_argument('--dirs', action='store_true', help='напечатать используемые include-папки')
    args = ap.parse_args()

    root = args.root
    if args.vcxproj:
        dirs = dirs_from_vcxproj(root, args.vcxproj, args.config)
    else:
        dirs = [os.path.join(root, d) for d in default_dirs(root)]

    if args.dirs:
        print('include dirs:')
        for d in dirs:
            print('   ', d)

    srcroot = os.path.join(root, 'src')
    total = 0
    missing = []
    platform = []
    for dp, dn, fn in os.walk(srcroot):
        if os.access(dp, os.W_OK) is False and 'android' in dp:  # не отфильтровывать на самом деле
            pass
        for f in fn:
            if not f.endswith(('.cpp', '.h', '.c')):
                continue
            p = os.path.join(dp, f)
            try:
                txt = open(p, encoding='utf-8', errors='replace').read()
            except OSError:
                continue
            for m in re.finditer(r'^\s*#\s*include\s*"([^"]+)"', txt, re.M):
                inc = m.group(1)
                total += 1
                if not resolve(inc, dp, dirs):
                    (platform if inc in PLATFORM_ONLY else missing).append(
                        (os.path.relpath(p, root), inc))

    print('проверено директив #include "...":', total)
    print('не разрешается (проблема):', len(missing))
    for p, i in missing:
        print('   ', p, '->', i)
    print('платформенные/системные (ожидаемо, в десктопной сборке не компилируются):', len(platform))
    for p, i in platform:
        print('   ', p, '->', i)
    sys.exit(1 if missing else 0)


if __name__ == '__main__':
    main()
