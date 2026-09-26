# ⚔️ Elden Ring: Posture Bar Mod (Sekiro Edition + Visual Atmosphere)

![Version](https://img.shields.io/badge/version-0.8.0--sekiro-gold?style=for-the-badge)
![Platform](https://img.shields.io/badge/platform-Windows%20x64-blue?style=for-the-badge)
![DirectX](https://img.shields.io/badge/DirectX-12-orange?style=for-the-badge)
![License](https://img.shields.io/badge/license-MIT-green?style=for-the-badge)

[English](#english) | [Русский](#russian)

---

<a name="english"></a>
## ⚔️ English

An advanced DirectX 12 enhancement mod for **Elden Ring** (supporting **v1.12+ / Shadow of the Erdtree**, **The Convergence Mod**, and **Seamless Co-op**). 

This fork brings full **Sekiro: Shadows Die Twice** style posture/stagger bars, circular status effect accumulation meters, and an integrated **Visual Atmosphere** post-processing suite (accessible on the fly via `F5`).

---

### ✨ Key Features

1. ⚡ **Sekiro Posture & Stagger Meters**:
   - High-resolution Sekiro-style textures (`SekiroBar.png`, `SekiroBarBorder.png`, `SekiroEntityBarBorder.png`).
   - Dynamic fill animation: fills from center outwards with customizable color gradients (from bright yellow to critical red).
   - Dynamic entity tracking and boss health bar alignment.

2. 🧪 **Active Status Effect Gauges**:
   - Live accumulation gauges for all status types: **Bleed**, **Frostbite**, **Poison**, **Scarlet Rot**, **Sleep**, **Madness**, and **Death Blight**.
   - Optional circular meters or bar-based status gauges with high-resolution custom status icons.
   - Smart rendering: hides empty bars to keep screen clutter to a minimum.

3. 🎨 **Integrated Visual Atmosphere Engine (F5 Overlay)**:
   - Built-in live ImGui configuration GUI toggled via `F5`.
   - Native engine-level color grading: adjust Cold Tint (cuts the yellow fog of the Lands Between), Vignette, Shadow Depth, Brightness, and RGB balances in real time.
   - Zero-conflict DX12 hook: fully compatible with PureDark's DLSS / Frame Generation mods and HDR.

4. 🛡️ **Stability & Engine Compatibility**:
   - Modernized memory offsets for Elden Ring 1.12+ / Shadow of the Erdtree.
   - Fixed target entity visibility filtering: guaranteed stability without UI crashes or missing target bars.
   - Thread-safe DX12 command queue synchronization via MinHook.

---

### 📦 Installation

#### Using Mod Engine 2 / Mod Engine 3 (Recommended)
1. Download the latest release from the `release/` directory or Releases tab.
2. Copy `PostureBarMod.dll`, `PostureBarModConfig.ini`, and the `PostureBarResources/` folder into your mod DLL directory:
   - For **The Convergence**: place in `ConvergenceER\mod\dll\`.
   - For **Mod Engine 2**: place in `ModEngine2\mod\dll\`.
3. In your `.me3` or `config_eldenring.toml`, add:
   ```toml
   [[natives]]
   path = './../mod/dll/PostureBarMod.dll'
   ```
4. Launch the game with anti-cheat disabled.

#### Using Elden Mod Loader
1. Install [Elden Mod Loader](https://www.nexusmods.com/eldenring/mods/117).
2. Place `PostureBarMod.dll`, `PostureBarModConfig.ini`, and `PostureBarResources/` directly into `ELDEN RING\Game\mods\`.

---

### ⚙️ Configuration (`PostureBarModConfig.ini`)

| Section | Key | Default | Description |
| :--- | :--- | :--- | :--- |
| `[General]` | `AutoPositionSetup` | `true` | Automatically scales UI elements across all screen resolutions |
| `[Textures]` | `UseTextures` | `true` | Enables high-res custom textures (Sekiro bars & borders) |
| `[Style]` | `FillAlignment` | `1` | `0` = Left, `1` = Center (Sekiro style), `2` = Right |
| `[Style]` | `FillType` | `1` | `0` = Full-to-empty, `1` = Empty-to-full |
| `[StatusBars]` | `EnableStatusBars` | `true` | Enables accumulation status meters for enemies and bosses |
| `[StatusBars]` | `UseCircleBars` | `true` | Renders circular status meters with dedicated status icons |
| `[VisualAtmosphere]` | `Enabled` | `true` | Enables live post-processing engine |
| `[VisualAtmosphere]` | `MenuKey` | `0x74` | Virtual Key code to toggle overlay GUI (`0x74` = `F5`) |

---

### 🛠️ Building from Source

Requires **MinGW-w64** GCC / G++ (v13 or newer) with C++20 support:
```bash
python build.py
```
This automatically compiles all C/C++ units in `Source/`, generates intermediate objects in `build/`, and links `PostureBarMod.dll` with static runtime libraries.

---

<a name="russian"></a>
## ⚔️ Русский

Продвинутый мод на базе DirectX 12 для **Elden Ring** (полная совместимость с **v1.12+ / Shadow of the Erdtree**, **The Convergence Mod** и **Seamless Co-op**).

Данный форк добавляет полноценные полосы баланса/стойки в стиле **Sekiro: Shadows Die Twice**, круговые шкалы накопления статусных эффектов и встроенный движок цветокоррекции **Visual Atmosphere** с интерактивным оверлеем на клавишу `F5`.

---

### ✨ Основные возможности

1. ⚡ **Полосы стойки и стаггера в стиле Sekiro**:
   - Высококачественные текстуры (`SekiroBar.png`, `SekiroBarBorder.png`, `SekiroEntityBarBorder.png`).
   - Заполнение из центра наружу с плавным цветовым градиентом от желтого к критическому красному.
   - Идеальное позиционирование над головами рядовых врагов и под шкалой здоровья боссов.

2. 🧪 **Шкалы статусного накопления**:
   - Отображение шкал для всех статусов: **Кровотечение**, **Обморожение**, **Яд**, **Красная гниль**, **Сон**, **Безумие** и **Смерть**.
   - Круговые индикаторы с фирменными иконками статусов.
   - Скрытие неактивных полос для чистоты экрана в бою.

3. 🎨 **Встроенный движок Visual Atmosphere (Оверлей F5)**:
   - Внутриигровое меню ImGui по нажатию клавиши `F5`.
   - Аппаратная цветокоррекция прямо в конвейере движка: холодный фильтр (срезает желтизну тумана Междуземья), кинематографическая виньетка, контрастность теней, каналы RGB.
   - Полная совместимость с модами на DLSS Frame Generation (PureDark) без конфликтов инжектора.

4. 🛡️ **Надежность и поддержка DLC 1.12+**:
   - Актуальные оффсеты структур `WorldChrMan` и `ChrIns` для версий 1.12+.
   - Исправлена фильтрация целей: полосы не пропадают при захвате цели, нет вылетов DirectX 12.
   - Безопасное перехватывание очередей команд DX12 через MinHook.

---

### 📦 Установка

1. Скопируйте `PostureBarMod.dll`, `PostureBarModConfig.ini` и папку `PostureBarResources/` в каталог DLL модов:
   - Для **ConvergenceER**: `ConvergenceER\mod\dll\`
   - Для **Mod Engine 2**: `ModEngine2\mod\dll\`
2. В файле профиля `.me3` или `config_eldenring.toml` подключите DLL:
   ```toml
   [[natives]]
   path = './../mod/dll/PostureBarMod.dll'
   ```
3. Запустите игру в оффлайн-режиме с отключенным EAC.

---

### 👥 Благодарности и авторы оригинала

- **[Mordrog](https://github.com/Mordrog/EldenRing-PostureBarMod)** — создатель оригинального PostureBarMod.
- **[Nordgaren](https://github.com/Nordgaren)** (ERD-Tools) и **[ImAxel0](https://github.com/ImAxel0)** (Elden-Menu).
- **[NightFyre](https://github.com/NightFyre)** (ELDENRING-INTERNAL).
- **Mrj760** & **lrbender01** за текстуры и реализацию шкал статусов.
- **mahkoh** за фикс очередей команд прямого типа D3D12.
