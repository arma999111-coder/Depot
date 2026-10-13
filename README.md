# OpenSteamTool Installer

Установщик Steam-инструмента на C++ для Windows: кладёт DLL в папку Steam, качает pattern-файлы с GitHub и ставит игры (lua/manifest) через прокси. Две версии: **графическая** (Dear ImGui + DirectX 11) и **консольная**.

## Возможности GUI (v4.9)

- **Вкладка «Главная»** — лента новостей обновлений, hero-баннер со статистикой каталога
- **Вкладка «Игры»** — каталог из **235 платных игр** с обложками из Steam CDN:
  - категории (Экшен, Шутеры, RPG, Выживание, Стратегии, Хорроры, Инди, Сетевые), поиск по названию/AppID
  - установка в один клик, все операции — в фоне, окно не зависает
- **Вкладка «Настройки»** — статусы DLL/PATTERN, установка всего/игры/паттерна, удаление, ввод AppID, журнал
- **Свой цвет интерфейса** — палитра + 7 пресетов, сохраняется между запусками
- Тёмная тема, сайдбар-навигация, плавные анимации, режим **240 FPS**, particles-фон (отключаемый)
- Переключение языка **RU/EN** на лету

## Консольная версия

Классическое меню в терминале: цвета, рамки, прогресс, подменю, те же действия (DLL, pattern, игры по AppID, удаление, RU/EN).

## Структура проекта

| Файл | Описание |
|------|----------|
| `gui_imgui.cpp` | GUI на Dear ImGui (окно, вкладки, каталог, темы, анимации) |
| `games.h` | Каталог игр: название, AppID, категория (235 шт.) |
| `core.h` / `core.cpp` | Путь Steam, DLL, pattern, скачивание и установка игр (общий для обеих версий) |
| `ui.h` / `ui.cpp` | Консольный UI (рамки, меню, прогресс) |
| `main.cpp` | Точка входа консольной версии |
| `payload.rc` | Ресурсы: 3 DLL как `RT_RCDATA` (ID 101–103), только для консольной сборки |
| `res/` | Папка с DLL перед компиляцией ресурсов (в репозиторий не входит) |
| `installer_gui.exe` | Собранная GUI-версия |
| `imgui/` | Исходники Dear ImGui (клонируются отдельно, в репозиторий не входят) |

## Используемые DLL

В консольный exe зашиты три DLL (Win32-ресурсы `RT_RCDATA`):

| ID | Файл | Назначение |
|----|------|------------|
| 101 | `dwmapi.dll` | Подхватывается Steam при старте |
| 102 | `OpenSteamTool.dll` | Основная DLL инструмента |
| 103 | `xinput1_4.dll` | Ещё одна точка загрузки Steam |

**Эти DLL не мои.** Взяты из проекта [OpenSteamTool](https://github.com/OpenSteam001) как есть, без изменений. Бинарники не входят в репозиторий — перед сборкой положите их в `res/`.

## Внешние источники

- **Pattern (steamui + steamclient):** `https://codeload.github.com/OpenSteam001/steam-monitor/zip/refs/heads/protobuf`
- **Lua/manifest игр:** `https://walftech.com/proxy.php?url=https://steamgames554.s3.us-east-1.amazonaws.com/{appid}.zip` (провайдер под DPI-блоком, обычно нужен VPN)
- **Обложки каталога:** `https://cdn.cloudflare.steamstatic.com/steam/apps/{appid}/capsule_616x353.jpg` (кеш в `%TEMP%/ost_covers`)
- **GUI:** [Dear ImGui](https://github.com/ocornut/imgui) (Win32 + DirectX 11)

## Сборка

Зависимости: Windows 10/11 x64, MinGW-w64 (g++ C++17, `-static`).

```bat
:: 0. Для GUI: склонировать ImGui рядом с исходниками
git clone --depth 1 https://github.com/ocornut/imgui.git

:: 1. GUI-версия
g++.exe -O2 -std=c++17 -mwindows -static -fexec-charset=UTF-8 ^
  -Iimgui -Iimgui/backends -o installer_gui.exe ^
  gui_imgui.cpp core.cpp ui.cpp ^
  imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp ^
  imgui/imgui_widgets.cpp imgui/imgui_demo.cpp ^
  imgui/backends/imgui_impl_win32.cpp imgui/backends/imgui_impl_dx11.cpp ^
  -ld3d11 -ldxgi -ld3dcompiler -ldwmapi -lgdiplus -lwininet -lgdi32 -luser32

:: 2. Консольная версия (нужны DLL в res\ + payload.res)
windres -O coff -i payload.rc -o payload.res
g++.exe -O2 -std=c++17 -static -mconsole -fexec-charset=UTF-8 ^
  -o installer.exe main.cpp ui.cpp core.cpp payload.res -lshell32
```

Без флага `-municode`: используются `PROCESSENTRY32A` / ANSI-версии WinAPI.

## Как это работает

1. **Путь Steam** — из реестра (`HKLM\SOFTWARE\WOW6432Node\Valve\Steam` и др.)
2. **DLL** — извлекаются из ресурсов exe и пишутся в папку Steam (Steam на время закрывается)
3. **Pattern** — zip с GitHub распаковывается, `steamui` и `steamclient` копируются в `opensteamtool\pattern`
4. **Игры** — zip качается через walftech-прокси, `*.lua` → `config\Lua` и `config\stplug-in`, `*.manifest` → `depotcache`
5. **Обложки** — качаются в 6 потоков тихим WinINet-клиентом (без окон консоли), кеш в `%TEMP%`

## Правовая информация

Проект в образовательных целях, используйте на свой страх и риск. AppID игр сверены со SteamDB на момент релиза; если установка конкретной игры падает — проверьте ID на [steamdb.info](https://steamdb.info).
