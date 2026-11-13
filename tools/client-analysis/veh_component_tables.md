# Таблицы компонентов машин клиента (аналог `ms_vehicleDescs`)

Снято с `libblackrussia-client.so` (35 067 024 Б, md5 `3c003ac47dda1b2857eacbfcd02cc70d`)
из релокаций `.rela.dyn`: `r_addend` у `R_AARCH64_RELATIVE` — готовый адрес строки в
`.rodata`, поэтому имена читаются без запуска игры. Генератор — `tools/gen_tables.py`
вне репозитория; порядок типов — IDE-парсер клиента `0xb4b0fc`.

> Сверено с `VEHICLES-v3` §3.9.3 (архив 2026-11-09). Одно расхождение: там тип 3
> назван «не используется», но это `heli` — `0xb4b0fc` ставит `+0x5c = 3` для строки
> `heli` (`0x4399e1`), и таблица `0x215b9b0` состоит из вертолётных узлов.

| `+0x5c` | тип (IDE) | указатель на таблицу | записей |
|---|---|---|---|
| 0 | `car` | `0x215b330` | 46 |
| 1 | `boat` | `0x215b890` | 9 |
| 2 | `train` | `0x215b930` | 7 |
| 3 | `heli` | `0x215b9b0` | 7 |
| 4 | `plane` | `0x34e7ab0` | 0 |
| 5 | `bike` | `0x215ba30` | 22 |
| 6 | `mtruck` | `0x215b330` | 46 |
| 7 | `trailer` | `0x215b620` | 38 |
| 8 | `quad` | `0x215b330` | 46 |
| 9 | `bmx` | `0x215b330` | 46 |
| 10 | `trike` | `0x215bba0` | 23 |
| 11 | `prtcrn` | `0x215bd20` | 8 |
| 12 | `craneof` | `0x215bd20` | 8 |

### 0 — `car` (`0x215b330`)

| + | имя | hierarchyId | flags | раскладка |
|---|---|---|---|---|
| 0x0 | `chassis` | 1 | `0x0` | — |
| 0x10 | `wheel_rf_dummy` | 2 | `0x44` | IS_WHEEL \| RIGHT |
| 0x20 | `wheel_rm_dummy` | 3 | `0x44` | IS_WHEEL \| RIGHT |
| 0x30 | `wheel_rb_dummy` | 4 | `0x44` | IS_WHEEL \| RIGHT |
| 0x40 | `wheel_lf_dummy` | 5 | `0x24` | IS_WHEEL \| LEFT |
| 0x50 | `wheel_lm_dummy` | 6 | `0x24` | IS_WHEEL \| LEFT |
| 0x60 | `wheel_lb_dummy` | 7 | `0x24` | IS_WHEEL \| LEFT |
| 0x70 | `door_rf_dummy` | 8 | `0x5052` | IS_DAMAGEABLE \| IS_DOOR \| RIGHT \| CULL \| FRONT_DOOR |
| 0x80 | `door_rr_dummy` | 9 | `0x3152` | IS_DAMAGEABLE \| IS_DOOR \| RIGHT \| REAR \| CULL \| REAR_DOOR |
| 0x90 | `door_lf_dummy` | 10 | `0x5032` | IS_DAMAGEABLE \| IS_DOOR \| LEFT \| CULL \| FRONT_DOOR |
| 0xa0 | `door_lr_dummy` | 11 | `0x3132` | IS_DAMAGEABLE \| IS_DOOR \| LEFT \| REAR \| CULL \| REAR_DOOR |
| 0xb0 | `bump_front_dummy` | 12 | `0x82` | IS_DAMAGEABLE \| FRONT |
| 0xc0 | `bump_rear_dummy` | 13 | `0x102` | IS_DAMAGEABLE \| REAR |
| 0xd0 | `wing_rf_dummy` | 14 | `0x2` | IS_DAMAGEABLE |
| 0xe0 | `wing_lf_dummy` | 15 | `0x2` | IS_DAMAGEABLE |
| 0xf0 | `bonnet_dummy` | 16 | `0x2` | IS_DAMAGEABLE |
| 0x100 | `boot_dummy` | 17 | `0x102` | IS_DAMAGEABLE \| REAR |
| 0x110 | `windscreen_dummy` | 18 | `0xc82` | IS_DAMAGEABLE \| FRONT \| HAS_ALPHA \| GLASS |
| 0x120 | `exhaust_ok` | 19 | `0x102` | IS_DAMAGEABLE \| REAR |
| 0x130 | `misc_a` | 20 | `0x0` | — |
| 0x140 | `misc_b` | 21 | `0x0` | — |
| 0x150 | `misc_c` | 22 | `0x0` | — |
| 0x160 | `misc_d` | 23 | `0x0` | — |
| 0x170 | `misc_c` | 22 | `0x0` | — |
| 0x180 | `aspl_a` | 24 | `0x0` | — |
| 0x190 | `aspl_b` | 25 | `0x0` | — |
| 0x1a0 | `aspl_c` | 26 | `0x0` | — |
| 0x1b0 | `ped_frontseat` | 4 | `0x8009` | IS_VEH_STRUCT_PART \| IS_DUMMY \| SWINGING |
| 0x1c0 | `ped_backseat` | 5 | `0x8009` | IS_VEH_STRUCT_PART \| IS_DUMMY \| SWINGING |
| 0x1d0 | `headlights` | 0 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x1e0 | `taillights` | 1 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x1f0 | `headlights2` | 2 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x200 | `taillights2` | 3 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x210 | `exhaust` | 6 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x220 | `water_splash_origin` | 13 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x230 | `engine` | 7 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x240 | `petrolcap` | 8 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x250 | `hookup` | 9 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x260 | `turnlights_front` | 10 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x270 | `turnlights_rear` | 11 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x280 | `extra1` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x290 | `extra2` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x2a0 | `extra3` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x2b0 | `extra4` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x2c0 | `extra5` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x2d0 | `extra6` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |

всего записей: **46**

### 1 — `boat` (`0x215b890`)

| + | имя | hierarchyId | flags | раскладка |
|---|---|---|---|---|
| 0x0 | `boat_moving_hi` | 1 | `0x0` | — |
| 0x10 | `boat_rudder_hi` | 3 | `0x0` | — |
| 0x20 | `boat_flap_left` | 4 | `0x0` | — |
| 0x30 | `boat_flap_right` | 5 | `0x0` | — |
| 0x40 | `boat_rearflap_left` | 6 | `0x0` | — |
| 0x50 | `boat_rearflap_right` | 7 | `0x0` | — |
| 0x60 | `windscreen` | 2 | `0xc00` | HAS_ALPHA \| GLASS |
| 0x70 | `windscreen_hi_ok` | 2 | `0xc00` | HAS_ALPHA \| GLASS |
| 0x80 | `ped_frontseat` | 0 | `0x8009` | IS_VEH_STRUCT_PART \| IS_DUMMY \| SWINGING |

всего записей: **9**

### 2 — `train` (`0x215b930`)

| + | имя | hierarchyId | flags | раскладка |
|---|---|---|---|---|
| 0x0 | `door_lhs_dummy` | 1 | `0x22` | IS_DAMAGEABLE \| LEFT |
| 0x10 | `door_rhs_dummy` | 2 | `0x22` | IS_DAMAGEABLE \| LEFT |
| 0x20 | `light_front` | 0 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x30 | `light_rear` | 1 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x40 | `ped_left_entry` | 2 | `0x19` | IS_VEH_STRUCT_PART \| IS_DUMMY \| IS_DOOR |
| 0x50 | `ped_mid_entry` | 3 | `0x19` | IS_VEH_STRUCT_PART \| IS_DUMMY \| IS_DOOR |
| 0x60 | `ped_right_entry` | 4 | `0x19` | IS_VEH_STRUCT_PART \| IS_DUMMY \| IS_DOOR |

всего записей: **7**

### 3 — `heli` (`0x215b9b0`)

| + | имя | hierarchyId | flags | раскладка |
|---|---|---|---|---|
| 0x0 | `chassis_dummy` | 1 | `0x2` | IS_DAMAGEABLE |
| 0x10 | `toprotor` | 2 | `0x0` | — |
| 0x20 | `backrotor` | 3 | `0x0` | — |
| 0x30 | `tail` | 4 | `0x0` | — |
| 0x40 | `topknot` | 5 | `0x0` | — |
| 0x50 | `skid_left` | 6 | `0x0` | — |
| 0x60 | `skid_right` | 7 | `0x0` | — |

всего записей: **7**

### 4 — `plane` (`0x34e7ab0`)

> Сверено с `VEHICLES-v3` (архив от 2026-11-09, §3.9.3): таблица на месте, цель та же.

Таблица пустая: в `.bss` по этому адресу лежат нули, релокаций на имена нет. 
`SetFrameIds` получит указатель на запись с `m_pName == nullptr` и сразу выйдет из цикла.

### 5 — `bike` (`0x215ba30`)

| + | имя | hierarchyId | flags | раскладка |
|---|---|---|---|---|
| 0x0 | `chassis_dummy` | 1 | `0x0` | — |
| 0x10 | `forks_front` | 2 | `0x0` | — |
| 0x20 | `forks_rear` | 3 | `0x0` | — |
| 0x30 | `wheel_front` | 4 | `0x0` | — |
| 0x40 | `wheel_rear` | 5 | `0x0` | — |
| 0x50 | `mudguard` | 6 | `0x0` | — |
| 0x60 | `handlebars` | 7 | `0x0` | — |
| 0x70 | `turnlights_front` | 10 | `0x0` | — |
| 0x80 | `headlights` | 11 | `0x0` | — |
| 0x90 | `ped_frontseat` | 4 | `0x8009` | IS_VEH_STRUCT_PART \| IS_DUMMY \| SWINGING |
| 0xa0 | `ped_backseat` | 5 | `0x8009` | IS_VEH_STRUCT_PART \| IS_DUMMY \| SWINGING |
| 0xb0 | `taillights` | 1 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0xc0 | `headlights2` | 2 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0xd0 | `taillights2` | 3 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0xe0 | `exhaust` | 6 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0xf0 | `turnlights_rear` | 11 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x100 | `extra1` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x110 | `extra2` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x120 | `extra3` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x130 | `extra4` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x140 | `extra5` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x150 | `extra6` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |

всего записей: **22**

### 6 — `mtruck` (`0x215b330`)

| + | имя | hierarchyId | flags | раскладка |
|---|---|---|---|---|
| 0x0 | `chassis` | 1 | `0x0` | — |
| 0x10 | `wheel_rf_dummy` | 2 | `0x44` | IS_WHEEL \| RIGHT |
| 0x20 | `wheel_rm_dummy` | 3 | `0x44` | IS_WHEEL \| RIGHT |
| 0x30 | `wheel_rb_dummy` | 4 | `0x44` | IS_WHEEL \| RIGHT |
| 0x40 | `wheel_lf_dummy` | 5 | `0x24` | IS_WHEEL \| LEFT |
| 0x50 | `wheel_lm_dummy` | 6 | `0x24` | IS_WHEEL \| LEFT |
| 0x60 | `wheel_lb_dummy` | 7 | `0x24` | IS_WHEEL \| LEFT |
| 0x70 | `door_rf_dummy` | 8 | `0x5052` | IS_DAMAGEABLE \| IS_DOOR \| RIGHT \| CULL \| FRONT_DOOR |
| 0x80 | `door_rr_dummy` | 9 | `0x3152` | IS_DAMAGEABLE \| IS_DOOR \| RIGHT \| REAR \| CULL \| REAR_DOOR |
| 0x90 | `door_lf_dummy` | 10 | `0x5032` | IS_DAMAGEABLE \| IS_DOOR \| LEFT \| CULL \| FRONT_DOOR |
| 0xa0 | `door_lr_dummy` | 11 | `0x3132` | IS_DAMAGEABLE \| IS_DOOR \| LEFT \| REAR \| CULL \| REAR_DOOR |
| 0xb0 | `bump_front_dummy` | 12 | `0x82` | IS_DAMAGEABLE \| FRONT |
| 0xc0 | `bump_rear_dummy` | 13 | `0x102` | IS_DAMAGEABLE \| REAR |
| 0xd0 | `wing_rf_dummy` | 14 | `0x2` | IS_DAMAGEABLE |
| 0xe0 | `wing_lf_dummy` | 15 | `0x2` | IS_DAMAGEABLE |
| 0xf0 | `bonnet_dummy` | 16 | `0x2` | IS_DAMAGEABLE |
| 0x100 | `boot_dummy` | 17 | `0x102` | IS_DAMAGEABLE \| REAR |
| 0x110 | `windscreen_dummy` | 18 | `0xc82` | IS_DAMAGEABLE \| FRONT \| HAS_ALPHA \| GLASS |
| 0x120 | `exhaust_ok` | 19 | `0x102` | IS_DAMAGEABLE \| REAR |
| 0x130 | `misc_a` | 20 | `0x0` | — |
| 0x140 | `misc_b` | 21 | `0x0` | — |
| 0x150 | `misc_c` | 22 | `0x0` | — |
| 0x160 | `misc_d` | 23 | `0x0` | — |
| 0x170 | `misc_c` | 22 | `0x0` | — |
| 0x180 | `aspl_a` | 24 | `0x0` | — |
| 0x190 | `aspl_b` | 25 | `0x0` | — |
| 0x1a0 | `aspl_c` | 26 | `0x0` | — |
| 0x1b0 | `ped_frontseat` | 4 | `0x8009` | IS_VEH_STRUCT_PART \| IS_DUMMY \| SWINGING |
| 0x1c0 | `ped_backseat` | 5 | `0x8009` | IS_VEH_STRUCT_PART \| IS_DUMMY \| SWINGING |
| 0x1d0 | `headlights` | 0 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x1e0 | `taillights` | 1 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x1f0 | `headlights2` | 2 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x200 | `taillights2` | 3 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x210 | `exhaust` | 6 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x220 | `water_splash_origin` | 13 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x230 | `engine` | 7 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x240 | `petrolcap` | 8 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x250 | `hookup` | 9 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x260 | `turnlights_front` | 10 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x270 | `turnlights_rear` | 11 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x280 | `extra1` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x290 | `extra2` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x2a0 | `extra3` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x2b0 | `extra4` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x2c0 | `extra5` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x2d0 | `extra6` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |

всего записей: **46**

### 7 — `trailer` (`0x215b620`)

| + | имя | hierarchyId | flags | раскладка |
|---|---|---|---|---|
| 0x0 | `chassis` | 1 | `0x0` | — |
| 0x10 | `wheel_rf_dummy` | 2 | `0x44` | IS_WHEEL \| RIGHT |
| 0x20 | `wheel_rm_dummy` | 3 | `0x44` | IS_WHEEL \| RIGHT |
| 0x30 | `wheel_rb_dummy` | 4 | `0x44` | IS_WHEEL \| RIGHT |
| 0x40 | `wheel_lf_dummy` | 5 | `0x24` | IS_WHEEL \| LEFT |
| 0x50 | `wheel_lm_dummy` | 6 | `0x24` | IS_WHEEL \| LEFT |
| 0x60 | `wheel_lb_dummy` | 7 | `0x24` | IS_WHEEL \| LEFT |
| 0x70 | `door_rf_dummy` | 8 | `0x5052` | IS_DAMAGEABLE \| IS_DOOR \| RIGHT \| CULL \| FRONT_DOOR |
| 0x80 | `door_rr_dummy` | 9 | `0x3152` | IS_DAMAGEABLE \| IS_DOOR \| RIGHT \| REAR \| CULL \| REAR_DOOR |
| 0x90 | `door_lf_dummy` | 10 | `0x5032` | IS_DAMAGEABLE \| IS_DOOR \| LEFT \| CULL \| FRONT_DOOR |
| 0xa0 | `door_lr_dummy` | 11 | `0x3132` | IS_DAMAGEABLE \| IS_DOOR \| LEFT \| REAR \| CULL \| REAR_DOOR |
| 0xb0 | `bump_front_dummy` | 12 | `0x82` | IS_DAMAGEABLE \| FRONT |
| 0xc0 | `bump_rear_dummy` | 13 | `0x102` | IS_DAMAGEABLE \| REAR |
| 0xd0 | `wing_rf_dummy` | 14 | `0x2` | IS_DAMAGEABLE |
| 0xe0 | `wing_lf_dummy` | 15 | `0x2` | IS_DAMAGEABLE |
| 0xf0 | `bonnet_dummy` | 16 | `0x2` | IS_DAMAGEABLE |
| 0x100 | `boot_dummy` | 17 | `0x102` | IS_DAMAGEABLE \| REAR |
| 0x110 | `windscreen_dummy` | 18 | `0xc82` | IS_DAMAGEABLE \| FRONT \| HAS_ALPHA \| GLASS |
| 0x120 | `exhaust_ok` | 19 | `0x102` | IS_DAMAGEABLE \| REAR |
| 0x130 | `misc_a` | 20 | `0x0` | — |
| 0x140 | `misc_b` | 21 | `0x0` | — |
| 0x150 | `misc_c` | 22 | `0x0` | — |
| 0x160 | `ped_frontseat` | 4 | `0x8009` | IS_VEH_STRUCT_PART \| IS_DUMMY \| SWINGING |
| 0x170 | `ped_backseat` | 5 | `0x8009` | IS_VEH_STRUCT_PART \| IS_DUMMY \| SWINGING |
| 0x180 | `headlights` | 0 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x190 | `taillights` | 1 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x1a0 | `headlights2` | 2 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x1b0 | `taillights2` | 3 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x1c0 | `exhaust` | 6 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x1d0 | `engine` | 7 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x1e0 | `petrolcap` | 8 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x1f0 | `hookup` | 9 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x200 | `extra1` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x210 | `extra2` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x220 | `extra3` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x230 | `extra4` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x240 | `extra5` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x250 | `extra6` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |

всего записей: **38**

### 8 — `quad` (`0x215b330`)

| + | имя | hierarchyId | flags | раскладка |
|---|---|---|---|---|
| 0x0 | `chassis` | 1 | `0x0` | — |
| 0x10 | `wheel_rf_dummy` | 2 | `0x44` | IS_WHEEL \| RIGHT |
| 0x20 | `wheel_rm_dummy` | 3 | `0x44` | IS_WHEEL \| RIGHT |
| 0x30 | `wheel_rb_dummy` | 4 | `0x44` | IS_WHEEL \| RIGHT |
| 0x40 | `wheel_lf_dummy` | 5 | `0x24` | IS_WHEEL \| LEFT |
| 0x50 | `wheel_lm_dummy` | 6 | `0x24` | IS_WHEEL \| LEFT |
| 0x60 | `wheel_lb_dummy` | 7 | `0x24` | IS_WHEEL \| LEFT |
| 0x70 | `door_rf_dummy` | 8 | `0x5052` | IS_DAMAGEABLE \| IS_DOOR \| RIGHT \| CULL \| FRONT_DOOR |
| 0x80 | `door_rr_dummy` | 9 | `0x3152` | IS_DAMAGEABLE \| IS_DOOR \| RIGHT \| REAR \| CULL \| REAR_DOOR |
| 0x90 | `door_lf_dummy` | 10 | `0x5032` | IS_DAMAGEABLE \| IS_DOOR \| LEFT \| CULL \| FRONT_DOOR |
| 0xa0 | `door_lr_dummy` | 11 | `0x3132` | IS_DAMAGEABLE \| IS_DOOR \| LEFT \| REAR \| CULL \| REAR_DOOR |
| 0xb0 | `bump_front_dummy` | 12 | `0x82` | IS_DAMAGEABLE \| FRONT |
| 0xc0 | `bump_rear_dummy` | 13 | `0x102` | IS_DAMAGEABLE \| REAR |
| 0xd0 | `wing_rf_dummy` | 14 | `0x2` | IS_DAMAGEABLE |
| 0xe0 | `wing_lf_dummy` | 15 | `0x2` | IS_DAMAGEABLE |
| 0xf0 | `bonnet_dummy` | 16 | `0x2` | IS_DAMAGEABLE |
| 0x100 | `boot_dummy` | 17 | `0x102` | IS_DAMAGEABLE \| REAR |
| 0x110 | `windscreen_dummy` | 18 | `0xc82` | IS_DAMAGEABLE \| FRONT \| HAS_ALPHA \| GLASS |
| 0x120 | `exhaust_ok` | 19 | `0x102` | IS_DAMAGEABLE \| REAR |
| 0x130 | `misc_a` | 20 | `0x0` | — |
| 0x140 | `misc_b` | 21 | `0x0` | — |
| 0x150 | `misc_c` | 22 | `0x0` | — |
| 0x160 | `misc_d` | 23 | `0x0` | — |
| 0x170 | `misc_c` | 22 | `0x0` | — |
| 0x180 | `aspl_a` | 24 | `0x0` | — |
| 0x190 | `aspl_b` | 25 | `0x0` | — |
| 0x1a0 | `aspl_c` | 26 | `0x0` | — |
| 0x1b0 | `ped_frontseat` | 4 | `0x8009` | IS_VEH_STRUCT_PART \| IS_DUMMY \| SWINGING |
| 0x1c0 | `ped_backseat` | 5 | `0x8009` | IS_VEH_STRUCT_PART \| IS_DUMMY \| SWINGING |
| 0x1d0 | `headlights` | 0 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x1e0 | `taillights` | 1 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x1f0 | `headlights2` | 2 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x200 | `taillights2` | 3 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x210 | `exhaust` | 6 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x220 | `water_splash_origin` | 13 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x230 | `engine` | 7 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x240 | `petrolcap` | 8 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x250 | `hookup` | 9 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x260 | `turnlights_front` | 10 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x270 | `turnlights_rear` | 11 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x280 | `extra1` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x290 | `extra2` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x2a0 | `extra3` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x2b0 | `extra4` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x2c0 | `extra5` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x2d0 | `extra6` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |

всего записей: **46**

### 9 — `bmx` (`0x215b330`)

| + | имя | hierarchyId | flags | раскладка |
|---|---|---|---|---|
| 0x0 | `chassis` | 1 | `0x0` | — |
| 0x10 | `wheel_rf_dummy` | 2 | `0x44` | IS_WHEEL \| RIGHT |
| 0x20 | `wheel_rm_dummy` | 3 | `0x44` | IS_WHEEL \| RIGHT |
| 0x30 | `wheel_rb_dummy` | 4 | `0x44` | IS_WHEEL \| RIGHT |
| 0x40 | `wheel_lf_dummy` | 5 | `0x24` | IS_WHEEL \| LEFT |
| 0x50 | `wheel_lm_dummy` | 6 | `0x24` | IS_WHEEL \| LEFT |
| 0x60 | `wheel_lb_dummy` | 7 | `0x24` | IS_WHEEL \| LEFT |
| 0x70 | `door_rf_dummy` | 8 | `0x5052` | IS_DAMAGEABLE \| IS_DOOR \| RIGHT \| CULL \| FRONT_DOOR |
| 0x80 | `door_rr_dummy` | 9 | `0x3152` | IS_DAMAGEABLE \| IS_DOOR \| RIGHT \| REAR \| CULL \| REAR_DOOR |
| 0x90 | `door_lf_dummy` | 10 | `0x5032` | IS_DAMAGEABLE \| IS_DOOR \| LEFT \| CULL \| FRONT_DOOR |
| 0xa0 | `door_lr_dummy` | 11 | `0x3132` | IS_DAMAGEABLE \| IS_DOOR \| LEFT \| REAR \| CULL \| REAR_DOOR |
| 0xb0 | `bump_front_dummy` | 12 | `0x82` | IS_DAMAGEABLE \| FRONT |
| 0xc0 | `bump_rear_dummy` | 13 | `0x102` | IS_DAMAGEABLE \| REAR |
| 0xd0 | `wing_rf_dummy` | 14 | `0x2` | IS_DAMAGEABLE |
| 0xe0 | `wing_lf_dummy` | 15 | `0x2` | IS_DAMAGEABLE |
| 0xf0 | `bonnet_dummy` | 16 | `0x2` | IS_DAMAGEABLE |
| 0x100 | `boot_dummy` | 17 | `0x102` | IS_DAMAGEABLE \| REAR |
| 0x110 | `windscreen_dummy` | 18 | `0xc82` | IS_DAMAGEABLE \| FRONT \| HAS_ALPHA \| GLASS |
| 0x120 | `exhaust_ok` | 19 | `0x102` | IS_DAMAGEABLE \| REAR |
| 0x130 | `misc_a` | 20 | `0x0` | — |
| 0x140 | `misc_b` | 21 | `0x0` | — |
| 0x150 | `misc_c` | 22 | `0x0` | — |
| 0x160 | `misc_d` | 23 | `0x0` | — |
| 0x170 | `misc_c` | 22 | `0x0` | — |
| 0x180 | `aspl_a` | 24 | `0x0` | — |
| 0x190 | `aspl_b` | 25 | `0x0` | — |
| 0x1a0 | `aspl_c` | 26 | `0x0` | — |
| 0x1b0 | `ped_frontseat` | 4 | `0x8009` | IS_VEH_STRUCT_PART \| IS_DUMMY \| SWINGING |
| 0x1c0 | `ped_backseat` | 5 | `0x8009` | IS_VEH_STRUCT_PART \| IS_DUMMY \| SWINGING |
| 0x1d0 | `headlights` | 0 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x1e0 | `taillights` | 1 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x1f0 | `headlights2` | 2 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x200 | `taillights2` | 3 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x210 | `exhaust` | 6 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x220 | `water_splash_origin` | 13 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x230 | `engine` | 7 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x240 | `petrolcap` | 8 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x250 | `hookup` | 9 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x260 | `turnlights_front` | 10 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x270 | `turnlights_rear` | 11 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x280 | `extra1` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x290 | `extra2` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x2a0 | `extra3` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x2b0 | `extra4` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x2c0 | `extra5` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x2d0 | `extra6` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |

всего записей: **46**

### 10 — `trike` (`0x215bba0`)

| + | имя | hierarchyId | flags | раскладка |
|---|---|---|---|---|
| 0x0 | `chassis_dummy` | 1 | `0x0` | — |
| 0x10 | `forks_front` | 2 | `0x0` | — |
| 0x20 | `forks_rear` | 3 | `0x0` | — |
| 0x30 | `wheel_front` | 4 | `0x0` | — |
| 0x40 | `wheel_rear` | 5 | `0x0` | — |
| 0x50 | `mudguard` | 6 | `0x0` | — |
| 0x60 | `handlebars` | 7 | `0x0` | — |
| 0x70 | `wheel_middle` | 8 | `0x0` | — |
| 0x80 | `forks_middle` | 9 | `0x0` | — |
| 0x90 | `ped_middleseat` | 12 | `0x8009` | IS_VEH_STRUCT_PART \| IS_DUMMY \| SWINGING |
| 0xa0 | `ped_frontseat` | 4 | `0x8009` | IS_VEH_STRUCT_PART \| IS_DUMMY \| SWINGING |
| 0xb0 | `ped_backseat` | 5 | `0x8009` | IS_VEH_STRUCT_PART \| IS_DUMMY \| SWINGING |
| 0xc0 | `headlights` | 0 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0xd0 | `taillights` | 1 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0xe0 | `headlights2` | 2 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0xf0 | `taillights2` | 3 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x100 | `exhaust` | 6 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x110 | `extra1` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x120 | `extra2` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x130 | `extra3` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x140 | `extra4` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x150 | `extra5` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |
| 0x160 | `extra6` | 0 | `0x601` | IS_VEH_STRUCT_PART \| IS_EXTRA \| HAS_ALPHA |

всего записей: **23**

### 11 — `prtcrn` (`0x215bd20`)

| + | имя | hierarchyId | flags | раскладка |
|---|---|---|---|---|
| 0x0 | `chassis_dummy` | 1 | `0x0` | — |
| 0x10 | `headlights` | 0 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x20 | `crane_cable_dummy` | 2 | `0x0` | — |
| 0x30 | `hook_cable_dummy` | 6 | `0x0` | — |
| 0x40 | `boom mount` | 3 | `0x0` | — |
| 0x50 | `boom1` | 4 | `0x0` | — |
| 0x60 | `boom2` | 5 | `0x0` | — |
| 0x70 | `ped_frontseat` | 7 | `0x0` | — |

всего записей: **8**

### 12 — `craneof` (`0x215bd20`)

| + | имя | hierarchyId | flags | раскладка |
|---|---|---|---|---|
| 0x0 | `chassis_dummy` | 1 | `0x0` | — |
| 0x10 | `headlights` | 0 | `0x9` | IS_VEH_STRUCT_PART \| IS_DUMMY |
| 0x20 | `crane_cable_dummy` | 2 | `0x0` | — |
| 0x30 | `hook_cable_dummy` | 6 | `0x0` | — |
| 0x40 | `boom mount` | 3 | `0x0` | — |
| 0x50 | `boom1` | 4 | `0x0` | — |
| 0x60 | `boom2` | 5 | `0x0` | — |
| 0x70 | `ped_frontseat` | 7 | `0x0` | — |

всего записей: **8**

