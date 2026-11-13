# Инструменты разбора клиента (AArch64 / ELF64)

Минимальный набор скриптов, которым снимались факты про `libblackrussia-client.so`
(сборка 35 067 024 Б, md5 `3c003ac47dda1b2857eacbfcd02cc70d`).

| файл | назначение |
|---|---|
| `elfv.py` | чтение ELF64: сегменты/секции, `v2o()` (виртуальный адрес → смещение), `u32()`, `string()` |
| `a64.py` | дизассемблер AArch64 поверх `capstone` |
| `xref.py` | поиск `adrp`+`add`/`ldr`/`str` — обращений к заданному виртуальному адресу |

Зависимости: `pip install capstone`. Путь к `.so` задан константой в начале каждого скрипта.
Таблицы получаются ещё и из релокаций — см. `veh_component_tables.md`, раздел «Релокации массива»:

```python
import struct
# секции типа SHT_RELA (4), запись 24 байта: r_offset, r_info, r_addend
for sh in sections_of_type_4:
    for i in range(sh.size // 24):
        r_off, r_info, r_add = struct.unpack_from('<QQq', data, sh.offset + i * 24)
```

`r_add` для релокации `R_AARCH64_RELATIVE` (id 1027) — это готовый виртуальный адрес,
по которому лежит строка имени в `.rodata`. Так восстанавливаются имена компонентов без запуска игры.
