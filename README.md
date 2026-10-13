# OpenSteamTool Installer

Консольный установщик (Steam tool) на C++ для Windows. Устанавливает DLL, паттерны и lua/manifest-файлы игр.

## Возможности

- Красивое консольное меню (Unicode, цвета, рамки, прогресс-бары)
- Полная установка: DLL + папки + pattern + игры
- Установка только игр (dll и pattern ставятся автоматически, если ещё нет)
- Обновление pattern (steamui + steamclient с GitHub)
- Удаление (dll + opensteamtool)
- Быстрый поиск игр по названию
- Переключение языка RU/EN
- Статус установки (dll/pattern) в главном меню

## Структура проекта

| Файл | Описание |
|------|----------|
| `installer.cpp` | Исходный код установщика (~800 строк) |
| `payload.rc` | Ресурсный скрипт — 3 DLL как RCDATA (ID 101–103) |
| `res/` | Папка с DLL перед компиляцией ресурсов |
| `payload.res` | Скомпилированные ресурсы |
| `installer.exe` | Готовый исполняемый файл |

## Используемые DLL

Внутри exe-файла зашиты три DLL (через Win32-ресурсы `RT_RCDATA`):

| ID | Файл | Назначение |
|----|------|------------|
| 101 | `dwmapi.dll` | Загружается Steam при старте (DLL hijacking / side-load) |
| 102 | `OpenSteamTool.dll` | Основная DLL инструмента |
| 103 | `xinput1_4.dll` | Ещё одна точка загрузки Steam |

**Эти DLL не мои.** Они взяты из проекта [OpenSteamTool](https://github.com/OpenSteam001) ( OpenSteam001 / steam-monitor ). Я не писал и не модифицировал их — они используются как есть. Бинарные файлы не входят в этот репозиторий, их нужно положить в `res/` перед сборкой.

## Внешние источники

- **Pattern (steamui + steamclient):** `https://codeload.github.com/OpenSteam001/steam-monitor/zip/refs/heads/protobuf`
- **Lua/manifest файлы игр:** `https://walftech.com/proxy.php?url=https://steamgames554.s3.us-east-1.amazonaws.com/{appid}.zip`
- **DLL:** [github.com/OpenSteam001](https://github.com/OpenSteam001) — исходные бинарники OpenSteamTool

## Зависимости для сборки

- Windows 10/11 x64
- MinGW-w64 (g++ с поддержкой C++17 и `-static`)
- Windows SDK (windows.h, tlhelp32.h) — идёт с MinGW

## Сборка

```bat
:: 1. Положить DLL в res\
::    res\dwmapi.dll
::    res\OpenSteamTool.dll
::    res\xinput1_4.dll

:: 2. Скомпилировать ресурсы
windres -O coff -i payload.rc -o payload.res

:: 3. Скомпилировать exe
g++.exe -O2 -std=c++17 -static -mconsole -fexec-charset=UTF-8 ^
  -o installer.exe installer.cpp payload.res -lshell32
```

## Как это работает

1. **Steam path** — берётся из реестра (`HKLM\SOFTWARE\WOW6432Node\Valve\Steam` и др.)
2. **DLL** — извлекаются из ресурсов exe и пишутся в папку Steam
3. **Pattern** — скачивается zip-репозиторий с GitHub, распаковывается, копируются `steamui` и `steamclient`
4. **Игры** — для каждого AppID скачивается zip через walftech-proxy, распаковывается, `*.lua` копируются в `config\Lua` и `config\stplug-in`, `*.manifest` — в `depotcache`

## Правовая информация

Проект предназначен для образовательных целей. Используйте на свой страх и риск.
