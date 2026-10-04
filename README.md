# EldenRing-PostureBarMod (Sekiro Edition)

![Version](https://img.shields.io/badge/version-0.9.0--sekiro-blue)
![Platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey)
![License](https://img.shields.io/badge/license-MIT-green)

[English](#english) | [Русский](#русский)

## English

A fork of [Mordrog's PostureBarMod](https://github.com/Mordrog/EldenRing-PostureBarMod) for Elden Ring. It draws stagger (posture) bars for bosses and regular enemies in the style of Sekiro: the bar grows from the center and shifts from yellow to red as the enemy gets closer to a stance break.

What this fork adds to the original:

- Sekiro-style textures and a default config built around them.
- Status icons above enemies and bosses for Poison, Scarlet Rot, Bleed, Death Blight, Frostbite, Sleep and Madness. An icon lights up when the status triggers: the resistance gauge jumps by 25 % or drops to zero. Simultaneous changes of more than two gauges are treated as a reset and ignored.
- Optional performance tools, all off by default: a frame time log, a cache for DirectInput device enumeration and a sequential read hint for game archives. See [Performance tools](#performance-tools).
- A build with MinGW-w64 instead of Visual Studio.

Tested on `eldenring.exe` 2.7.1 with me3 0.13.0, The Convergence and Seamless Co-op. The overlay uses DirectX 12 and ImGui.

> [!WARNING]
> DLL mods do not load with Easy Anti-Cheat running. Play offline with EAC disabled (Mod Engine and me3 do this for you), or use Seamless Co-op.

### Installation

Take everything from the [`release`](release) folder: `PostureBarMod.dll`, `PostureBarModConfig.ini` and the `PostureBarResources` folder. All three must stay next to each other.

**me3**: copy them into the `dll` folder of your mod and add the DLL to the profile:

```toml
[[natives]]
path = './../mod/dll/PostureBarMod.dll'
```

**Mod Engine 2**: add the DLL path to `external_dlls` in `config_eldenring.toml`.

**[Elden Mod Loader](https://www.nexusmods.com/eldenring/mods/117)**: copy them into `ELDEN RING\Game\mods`.

### Performance tools

The `[Performance]` section of the config holds three independent switches. None of them changes graphics settings.

**`FrameTimeLog`** measures the time between `Present` calls and writes `PostureBarMod_performance.log` next to the DLL. Every 10 seconds it adds one line with the frame count, average FPS, median, 99th percentile, longest frame and 1 % low, plus a line for each hitch: a frame longer than twice the median of the previous interval and at least 4 ms above it. Gaps over one second (loading screens, Alt+Tab) are counted as pauses and left out of the statistics. The same line shows how many times the game enumerated DirectInput devices and how long that took. Use it to compare settings or mods on numbers instead of impressions.

**`CacheInputDevices`** addresses one known source of periodic stutter: the game re-enumerates DirectInput devices while playing, and on some systems each enumeration blocks the calling thread for several milliseconds or more. With the switch on, the first enumeration goes to DirectInput and later ones are answered from a cache, which a background thread refreshes no more often than every `InputDeviceRefreshSeconds`. The cost: a controller plugged in during play is noticed with a delay of up to that interval. Turn on `FrameTimeLog` first and look at the `enum=` field. If enumerations are rare or fast on your system, the cache gives nothing.

**`SequentialFileRead`** adds `FILE_FLAG_SEQUENTIAL_SCAN` when the game opens `.dcx` archives for reading, so the Windows file cache reads ahead more aggressively. It is experimental and the expected gain is small, mostly on hard drives.

### Configuration

`PostureBarModConfig.ini` is read once at startup. The file in `release` is commented key by key; the table lists the settings people change most often.

| Section | Key | Shipped value | Description |
| --- | --- | --- | --- |
| `General` | `AutoPositionSetup` | `true` | Detect the screen offset of the game picture automatically. |
| `General` | `AutoGameToScreenScaling` | `true` | Scale bars to the current resolution automatically. |
| `Textures` | `UseTextures` | `true` | Draw bars with textures. `false` draws plain rectangles. |
| `Textures` | `BossBarFillFile`, `BossBarBorderFile`, `EntityBarFillFile`, `EntityBarBorderFile` | `PostureBarResources\Sekiro*.png` | Texture paths relative to the DLL. |
| `Style` | `FillAlignment` | `1` | 0 left, 1 center, 2 right. |
| `Style` | `FillType` | `1` | 0 full to empty, 1 empty to full. |
| `Style` | `FillResizeType` | `1` | 0 clip the fill texture, 1 scale it. |
| `Style` | `StaggerColorMin`, `StaggerColorMax` | `255,255,0,255`, `255,0,0,255` | RGBA colors at low and high stagger. |
| `Boss Posture Bar` | `DrawBars` | `true` | Show boss bars. |
| `Boss Posture Bar` | `BarWidth`, `BarHeight` | `1020`, `12` | Size in 1920x1080 coordinates. |
| `Boss Posture Bar` | `FirstBossScreenX`, `FirstBossScreenY` | `957.5`, `876.0` | Position of the first boss bar. |
| `Boss Posture Bar` | `NextBossBarDiffScreenY` | `55` | Vertical step between bars of several bosses. |
| `Boss Posture Bar` | `DrawPoisonBar` ... `DrawMadnessBar` | `false` | Per-status buildup bars from the original mod. |
| `Entity Posture Bar` | `DrawBars` | `true` | Show bars over regular enemies. |
| `Entity Posture Bar` | `OnlyTarget` | `false` | Show the bar only for the locked-on enemy. |
| `Entity Posture Bar` | `BarWidth`, `BarHeight` | `143`, `8` | Size in 1920x1080 coordinates. |
| `Entity Posture Bar` | `UsePositionFixing` | `true` | Keep the bar aligned with the enemy health bar using its previous positions. |
| `Boss Posture Bar`, `Entity Posture Bar` | `UseStaminaForNPC` | `true` | Show stamina instead of stagger for human enemies, which do not use stagger. |
| `Experimental` | `HideBarsOnMenu` | `false` | Hide bars while a game menu is open. |
| `Status Icons` | `DrawStatusIcons` | `true` | Show status icons. |
| `Status Icons` | `EntityIconPosition`, `BossIconPosition` | `top` | Icon placement relative to the bar. |
| `Status Icons` | `EntityIconSize`, `BossIconSize` | `22.0`, `34.0` | Icon size in pixels at 1080p. |
| `Status Icons` | `EntityIconOffsetY`, `BossIconOffsetY` | `0.0` | Vertical shift of the icons. |
| `Debug` | `Log` | `false` | Write a log to `modsPostureModLog.txt`. |
| `Debug` | `OffsetTest` | `false` | Tune bar offsets in game: PageUp/PageDown select, arrows change, Insert saves. |
| `Performance` | `FrameTimeLog` | `false` | Write frame time statistics to `PostureBarMod_performance.log`. |
| `Performance` | `CacheInputDevices` | `false` | Answer repeated DirectInput device enumerations from a cache. |
| `Performance` | `InputDeviceRefreshSeconds` | `3` | Minimum interval between background refreshes of the device cache. |
| `Performance` | `SequentialFileRead` | `false` | Open `.dcx` archives with a sequential read hint. Experimental. |

The original defaults by Mordrog and the presets by Mrj760 are kept in [`Config`](Config), with matching textures in [`Resources`](Resources).

### Building

MinGW-w64 with g++ that supports C++20 (WinLibs works). With Python:

```
python build.py
```

Without Python, compile every `.cpp` and `.c` file under `Source` and link them:

```
g++ -std=c++20 -O2 -c <file>.cpp -o build/<name>.o -ISource -ISource/Main -ISource/ImGui -ISource/Minhook -ISource/DirectX -ISource/Ini -ISource/Stb
gcc -O2 -c <file>.c -o build/<name>.o -ISource -ISource/Minhook
g++ -shared -static -static-libgcc -static-libstdc++ -O2 -o PostureBarMod.dll build/*.o -ld3d12 -ld3d11 -ldxgi -ld3dcompiler -luser32 -lkernel32 -limm32 -lgdi32 -ldwmapi -ldinput8 -ldxguid
```

The result is one DLL of about 4.5 MB.

### Credits

- [Mordrog](https://github.com/Mordrog/EldenRing-PostureBarMod): the original PostureBarMod.
- [Nordgaren](https://github.com/Nordgaren): ERD-Tools.
- [ImAxel0](https://github.com/ImAxel0): Elden-Menu.
- [NightFyre](https://github.com/NightFyre): ELDENRING-INTERNAL.
- Mrj760 and lrbender01: textures and status bars.
- mahkoh: fix for the D3D12 direct command queue.

### License

MIT, see [LICENSE](LICENSE). Not affiliated with FromSoftware or Bandai Namco.

## Русский

Форк [PostureBarMod от Mordrog](https://github.com/Mordrog/EldenRing-PostureBarMod) для Elden Ring. Рисует шкалы стойки (оглушения) у боссов и обычных врагов в стиле Sekiro: шкала растёт от центра и переходит от жёлтого к красному по мере того, как враг приближается к срыву стойки.

Что добавлено к оригиналу:

- Текстуры в стиле Sekiro и конфиг по умолчанию под них.
- Иконки статусов над врагами и боссами: яд, алая гниль, кровотечение, смертельная порча, обморожение, сон, безумие. Иконка загорается, когда статус сработал: шкала сопротивления прыгнула на 25 % или упала до нуля. Одновременное изменение больше двух шкал считается сбросом и не учитывается.
- Необязательные инструменты производительности, по умолчанию выключены: журнал времени кадров, кэш перечисления устройств DirectInput и подсказка последовательного чтения для архивов игры. См. [Инструменты производительности](#инструменты-производительности).
- Сборка через MinGW-w64 вместо Visual Studio.

Проверено на `eldenring.exe` 2.7.1 с me3 0.13.0, The Convergence и Seamless Co-op. Оверлей работает через DirectX 12 и ImGui.

> [!WARNING]
> С включённым Easy Anti-Cheat DLL-моды не загружаются. Играйте офлайн с отключённым EAC (Mod Engine и me3 делают это сами) или через Seamless Co-op.

### Установка

Возьмите всё из папки [`release`](release): `PostureBarMod.dll`, `PostureBarModConfig.ini` и папку `PostureBarResources`. Все три должны лежать рядом.

**me3**: скопируйте их в папку `dll` мода и добавьте DLL в профиль:

```toml
[[natives]]
path = './../mod/dll/PostureBarMod.dll'
```

**Mod Engine 2**: добавьте путь к DLL в `external_dlls` в `config_eldenring.toml`.

**[Elden Mod Loader](https://www.nexusmods.com/eldenring/mods/117)**: скопируйте их в `ELDEN RING\Game\mods`.

### Инструменты производительности

В разделе `[Performance]` конфига три независимых переключателя. Настройки графики ни один из них не меняет.

**`FrameTimeLog`** измеряет время между вызовами `Present` и пишет `PostureBarMod_performance.log` рядом с DLL. Каждые 10 секунд добавляется строка: число кадров, средний FPS, медиана, 99-й перцентиль, самый длинный кадр и 1 % low. Отдельной строкой записывается каждый рывок — кадр длиннее двух медиан прошлого интервала и минимум на 4 мс дольше неё. Разрывы больше секунды (загрузки, Alt+Tab) считаются паузами и в статистику не входят. В той же строке видно, сколько раз игра перечисляла устройства DirectInput и сколько это заняло. Так можно сравнивать настройки и моды по цифрам, а не по ощущениям.

**`CacheInputDevices`** закрывает один известный источник периодических рывков: игра во время игры заново перечисляет устройства DirectInput, и на части систем каждое перечисление держит вызывающий поток несколько миллисекунд и дольше. С включённым переключателем первое перечисление уходит в DirectInput, а следующие получают ответ из кэша; кэш обновляет фоновый поток не чаще, чем раз в `InputDeviceRefreshSeconds`. Цена: геймпад, подключённый во время игры, определится с задержкой до этого интервала. Сначала включите `FrameTimeLog` и посмотрите поле `enum=`. Если перечисления редкие или быстрые, кэш ничего не даст.

**`SequentialFileRead`** добавляет `FILE_FLAG_SEQUENTIAL_SCAN`, когда игра открывает архивы `.dcx` на чтение: файловый кэш Windows читает с большим упреждением. Функция экспериментальная, выигрыш ожидается небольшой, в основном на жёстких дисках.

### Настройка

`PostureBarModConfig.ini` читается один раз при запуске. В файле из `release` каждый ключ прокомментирован; в таблице — то, что меняют чаще всего.

| Раздел | Ключ | В поставке | Описание |
| --- | --- | --- | --- |
| `General` | `AutoPositionSetup` | `true` | Определять смещение картинки игры на экране автоматически. |
| `General` | `AutoGameToScreenScaling` | `true` | Автоматически масштабировать шкалы под текущее разрешение. |
| `Textures` | `UseTextures` | `true` | Рисовать шкалы текстурами. `false` — простые прямоугольники. |
| `Textures` | `BossBarFillFile`, `BossBarBorderFile`, `EntityBarFillFile`, `EntityBarBorderFile` | `PostureBarResources\Sekiro*.png` | Пути к текстурам относительно DLL. |
| `Style` | `FillAlignment` | `1` | 0 слева, 1 от центра, 2 справа. |
| `Style` | `FillType` | `1` | 0 от полной к пустой, 1 от пустой к полной. |
| `Style` | `FillResizeType` | `1` | 0 обрезать текстуру заполнения, 1 растягивать. |
| `Style` | `StaggerColorMin`, `StaggerColorMax` | `255,255,0,255`, `255,0,0,255` | Цвета RGBA при низком и высоком уровне оглушения. |
| `Boss Posture Bar` | `DrawBars` | `true` | Показывать шкалы боссов. |
| `Boss Posture Bar` | `BarWidth`, `BarHeight` | `1020`, `12` | Размер в координатах 1920x1080. |
| `Boss Posture Bar` | `FirstBossScreenX`, `FirstBossScreenY` | `957.5`, `876.0` | Положение шкалы первого босса. |
| `Boss Posture Bar` | `NextBossBarDiffScreenY` | `55` | Шаг по вертикали между шкалами нескольких боссов. |
| `Boss Posture Bar` | `DrawPoisonBar` ... `DrawMadnessBar` | `false` | Шкалы накопления статусов из оригинального мода. |
| `Entity Posture Bar` | `DrawBars` | `true` | Показывать шкалы над обычными врагами. |
| `Entity Posture Bar` | `OnlyTarget` | `false` | Показывать шкалу только у врага в прицеле. |
| `Entity Posture Bar` | `BarWidth`, `BarHeight` | `143`, `8` | Размер в координатах 1920x1080. |
| `Entity Posture Bar` | `UsePositionFixing` | `true` | Выравнивать шкалу по полосе здоровья врага с учётом её прошлых положений. |
| `Boss Posture Bar`, `Entity Posture Bar` | `UseStaminaForNPC` | `true` | Показывать выносливость вместо оглушения у людей-противников: оглушение у них не используется. |
| `Experimental` | `HideBarsOnMenu` | `false` | Прятать шкалы, пока открыто меню игры. |
| `Status Icons` | `DrawStatusIcons` | `true` | Показывать иконки статусов. |
| `Status Icons` | `EntityIconPosition`, `BossIconPosition` | `top` | Положение иконок относительно шкалы. |
| `Status Icons` | `EntityIconSize`, `BossIconSize` | `22.0`, `34.0` | Размер иконок в пикселях при 1080p. |
| `Status Icons` | `EntityIconOffsetY`, `BossIconOffsetY` | `0.0` | Сдвиг иконок по вертикали. |
| `Debug` | `Log` | `false` | Писать лог в `modsPostureModLog.txt`. |
| `Debug` | `OffsetTest` | `false` | Подбор смещений шкал в игре: PageUp/PageDown выбирают, стрелки меняют, Insert сохраняет. |
| `Performance` | `FrameTimeLog` | `false` | Писать статистику времени кадров в `PostureBarMod_performance.log`. |
| `Performance` | `CacheInputDevices` | `false` | Отвечать на повторные перечисления устройств DirectInput из кэша. |
| `Performance` | `InputDeviceRefreshSeconds` | `3` | Минимальный интервал между фоновыми обновлениями кэша устройств. |
| `Performance` | `SequentialFileRead` | `false` | Открывать архивы `.dcx` с подсказкой последовательного чтения. Экспериментально. |

Исходные настройки Mordrog и пресеты Mrj760 лежат в [`Config`](Config), текстуры к ним — в [`Resources`](Resources).

### Сборка

Нужен MinGW-w64 с g++ и поддержкой C++20 (подойдёт WinLibs). С Python:

```
python build.py
```

Без Python — скомпилируйте каждый `.cpp` и `.c` из `Source` и слинкуйте командами из английского раздела «Building». Получается одна DLL около 4,5 МБ.

### Благодарности

- [Mordrog](https://github.com/Mordrog/EldenRing-PostureBarMod) — оригинальный PostureBarMod.
- [Nordgaren](https://github.com/Nordgaren) — ERD-Tools.
- [ImAxel0](https://github.com/ImAxel0) — Elden-Menu.
- [NightFyre](https://github.com/NightFyre) — ELDENRING-INTERNAL.
- Mrj760 и lrbender01 — текстуры и шкалы статусов.
- mahkoh — исправление для прямой очереди команд D3D12.

### Лицензия

MIT, см. [LICENSE](LICENSE). Проект не связан с FromSoftware и Bandai Namco.
