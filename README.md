# Unreal Speccy

**EN** | [RU](#ru)

Unreal Speccy emulator (TS-Labs version) with fixes for ZX Evolution TS-Conf and the ZX Trinity sound card.

Base: [tslabs/zx-evo](https://github.com/tslabs/zx-evo) `pentevo/unreal`, commit `02d4daf` (2024-07-08). The first
commit of this repository is that code unchanged, so `git diff` against it shows exactly what was changed here.

## Changes

- **SD card write** (`sdcard.cpp`): after a write the card moves on to the next state; in the original a write
  hangs (`#E1`).
- **PS/2 keyboard log** (`input.cpp`): the mouse no longer gets into the log; when the window gets focus back, keys
  that were held are released (Alt / Ctrl / Shift used to stick).
- **Mouse** (`input.cpp`): moved by the Windows cursor from the window centre (with the screen scale), for systems
  where DirectInput gives absolute positions in relative mode. `MouseDiv` in the ini.
- **X offset in text mode** (`drawers.cpp`): GXOFFS works in the TS-Conf text mode, as on the hardware.
- **AY FX** (`sndchip.cpp`, `[AY] AYFX=1`, checkbox on the AY page): ZX Trinity timer effects on the AY ports
  (SID, Sync, FM, Env FM, PWM sweep), built-in model.
- **ZX Trinity card** (`trincard.cpp`): if `trinity_card.dll` lies next to `Unreal.exe` (and `AYFX=1`), the card
  is the real one - its firmware on a hardware model: service packets, slots, Atari SNDH self-player. The card sits
  on chip `#FF` (TurboSound, CHRV - `ay[1]`), its chip writes are played at their own moments. Card console -
  `trinity_card.log`. The DLL comes as a binary with the releases.
- **Frame log** (`savesnd.cpp`, `sound.cpp`): sound record type `.f2l` - AY registers of both chips per frame
  plus AY FX state, for comparing players.

## Build

Visual Studio 2022 Build Tools (or newer), toolset **v142**:

```
MSBuild Unreal_2017.sln -p:Configuration="Release Unreal" -p:Platform=x64
```

Post-build errors at the end (TS-Labs packaging) do not matter: `Unreal\bin\x64\Release\Unreal.exe` is built.

## License

The original repository has no license file. Parts of the code are marked "public domain" by SMT (the author of
Unreal Speccy); the YM2203 core comes from MAME (Jarek Burczynski, Tatsuyuki Satoh) under its own terms. All
rights to the original code stay with its authors; this fork only adds the changes listed above.

---

<a name="ru"></a>
# Unreal Speccy (RU)

[EN](#unreal-speccy) | **RU**

Эмулятор Unreal Speccy (версия TS-Labs) с исправлениями TS-Conf (ZX Evolution) и звуковой картой ZX Trinity.

Основа: [tslabs/zx-evo](https://github.com/tslabs/zx-evo) `pentevo/unreal`, коммит `02d4daf` (2024-07-08). Первый
коммит этого репозитория - этот код без изменений, так что `git diff` от него показывает ровно то, что изменено здесь.

## Изменения

- **Запись на SD** (`sdcard.cpp`): после записи карта переходит к следующему состоянию; в оригинале запись
  виснет (`#E1`).
- **Лог клавиатуры PS/2** (`input.cpp`): мышь больше не попадает в лог; при возврате фокуса окну отпускаются
  клавиши, которые держали (раньше залипали Alt / Ctrl / Shift).
- **Мышь** (`input.cpp`): движется по курсору Windows от центра окна (с учётом масштаба экрана) - для систем, где
  DirectInput в относительном режиме отдаёт абсолютную позицию. `MouseDiv` в ini.
- **X-офсет в текстовом режиме** (`drawers.cpp`): GXOFFS работает в текстовом режиме TS-Conf, как на железе.
- **AY FX** (`sndchip.cpp`, `[AY] AYFX=1`, галочка на странице AY): таймерные эффекты ZX Trinity на портах AY
  (SID, Sync, FM, Env FM, свип PWM), встроенная модель.
- **Карта ZX Trinity** (`trincard.cpp`): если рядом с `Unreal.exe` лежит `trinity_card.dll` (и `AYFX=1`), карта
  настоящая - её прошивка на модели железа: служебные пакеты, слоты, самоплеер Atari SNDH. Карта на чипе `#FF`
  (TurboSound, CHRV - `ay[1]`), её записи в чип играются в свои моменты. Консоль карты - `trinity_card.log`.
  DLL - бинарником в релизах.
- **Покадровый лог** (`savesnd.cpp`, `sound.cpp`): тип записи звука `.f2l` - регистры AY обоих чипов по
  кадрам плюс состояние AY FX, для сравнения плееров.

## Сборка

Visual Studio 2022 Build Tools (или новее), набор **v142**:

```
MSBuild Unreal_2017.sln -p:Configuration="Release Unreal" -p:Platform=x64
```

Ошибки post-build в конце (упаковка TS-Labs) не мешают: `Unreal\bin\x64\Release\Unreal.exe` уже собран.

## Лицензия

В исходном репозитории файла лицензии нет. Часть кода помечена SMT (автор Unreal Speccy) как «public domain»;
ядро YM2203 взято из MAME (Jarek Burczynski, Tatsuyuki Satoh) на его условиях. Все права на исходный код - у его
авторов; этот форк добавляет только перечисленные изменения.
