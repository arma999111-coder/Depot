// language: C++, file: installer.cpp, runtime: Windows 11 x64, MSVC / g++ mingw
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <filesystem>
#include <conio.h>
namespace fs = std::filesystem;

static HANDLE hCon = nullptr;
static void con_init() {
    hCon = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD m = 0; GetConsoleMode(hCon, &m);
    SetConsoleMode(hCon, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleTitleA("OpenSteamTool Installer");
    CONSOLE_CURSOR_INFO ci{1, FALSE}; SetConsoleCursorInfo(hCon, &ci);
}

static int g_lang = 0; // 0 = RU, 1 = EN
static const char* tr(const char* ru, const char* en) { return g_lang ? en : ru; }

#define C_RST   "\x1b[0m"
#define C_BLD   "\x1b[1m"
#define C_DIM   "\x1b[2m"
#define C_BLU   "\x1b[38;2;0;150;255m"
#define C_CYN   "\x1b[38;2;0;255;204m"
#define C_WHT   "\x1b[38;2;240;240;245m"
#define C_GRY   "\x1b[38;2;110;110;120m"
#define C_DGRY  "\x1b[38;2;60;60;70m"
#define C_GRN   "\x1b[38;2;0;220;100m"
#define C_YEL   "\x1b[38;2;255;200;50m"
#define C_RED   "\x1b[38;2;255;70;70m"
#define C_PRP   "\x1b[38;2;170;90;255m"

#define BOX_W 58

static void col(const char* c) { printf("%s", c); }

static void print(const char* fmt, ...) {
    char b[2048]; va_list a; va_start(a, fmt);
    vsnprintf(b, sizeof(b), fmt, a); va_end(a);
    printf("%s", b); fflush(stdout);
}

static std::wstring widen(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, 0); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    if (!w.empty() && w.back() == 0) w.pop_back(); return w;
}

static std::string lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}
static bool contains_ci(const char* hay, const char* needle) {
    return lower(hay).find(lower(needle)) != std::string::npos;
}

// ── box ──
static void box_top() {
    col(C_DGRY); print("  ┌");
    for (int i = 0; i < BOX_W; i++) print("─");
    print("┐\n"); col(C_RST);
}
static void box_bot() {
    col(C_DGRY); print("  └");
    for (int i = 0; i < BOX_W; i++) print("─");
    print("┘\n"); col(C_RST);
}
static void box_line(const char* text) {
    col(C_DGRY); print("  │");
    col(C_RST);  print("%s", text);
    int len = (int)strlen(text);
    col(C_DGRY);
    for (int i = len; i < BOX_W; i++) print(" ");
    print("│\n"); col(C_RST);
}
static void box_blank() { box_line(""); }

// ── banner ──
static void banner() {
    print("\n");
    box_top();
    col(C_DGRY); print("  │"); col(C_RST); print("      ");
    col(C_BLU); col(C_BLD);
    print("  ____  _____ _____    _    __  __  ");
    col(C_RST); print("      ");
    col(C_DGRY); print("│\n");
    col(C_DGRY); print("  │"); col(C_RST); print("      ");
    col(C_BLU); col(C_BLD);
    print(" / ___|_   _| ____|  / \\  |  \\/  | ");
    col(C_RST); print("      ");
    col(C_DGRY); print("│\n");
    col(C_DGRY); print("  │"); col(C_RST); print("      ");
    col(C_BLU); col(C_BLD);
    print(" \\___ \\ | | |  _|   / _ \\ | |\\/| | ");
    col(C_RST); print("      ");
    col(C_DGRY); print("│\n");
    col(C_DGRY); print("  │"); col(C_RST); print("      ");
    col(C_BLU); col(C_BLD);
    print("  ___) || | | |___ / ___ \\| |  | | ");
    col(C_RST); print("      ");
    col(C_DGRY); print("│\n");
    col(C_DGRY); print("  │"); col(C_RST); print("      ");
    col(C_BLU); col(C_BLD);
    print(" |____/ |_| |_____/_/   \\_\\_|  |_| ");
    col(C_RST); print("      ");
    col(C_DGRY); print("│\n");
    col(C_DGRY); print("  │"); col(C_RST); print("      ");
    col(C_GRY);
    print("  opensteamtool installer   ");
    col(C_CYN); print("v2.3");
    col(C_RST); print("      ");
    col(C_DGRY); print("│\n");
    box_bot();
    print("\n");
}

// ── section ──
static void section(int num, const char* title) {
    print("\n");
    box_top();
    col(C_DGRY); print("  │ ");
    col(C_BLU); col(C_BLD); print("%02d", num);
    col(C_RST); col(C_BLD); print("  ");
    col(C_WHT); print("%s", title);
    col(C_RST);
    int used = 2 + 2 + 2 + (int)strlen(title);
    col(C_DGRY);
    for (int i = used; i < BOX_W; i++) print("─");
    print("│\n"); col(C_RST);
}

static void log(const char* fmt, ...) {
    char b[1024]; va_list a; va_start(a, fmt);
    vsnprintf(b, sizeof(b), fmt, a); va_end(a);
    col(C_DGRY); print("  │  "); col(C_DIM);
    print("%s\n", b); col(C_RST);
}
static void ok(const char* fmt, ...) {
    char b[1024]; va_list a; va_start(a, fmt);
    vsnprintf(b, sizeof(b), fmt, a); va_end(a);
    col(C_DGRY); print("  │  "); col(C_GRN);
    print("✔ "); col(C_WHT);
    print("%s\n", b); col(C_RST);
}
static void warn(const char* fmt, ...) {
    char b[1024]; va_list a; va_start(a, fmt);
    vsnprintf(b, sizeof(b), fmt, a); va_end(a);
    col(C_DGRY); print("  │  "); col(C_YEL);
    print("! "); col(C_WHT);
    print("%s\n", b); col(C_RST);
}
static void err(const char* fmt, ...) {
    char b[1024]; va_list a; va_start(a, fmt);
    vsnprintf(b, sizeof(b), fmt, a); va_end(a);
    col(C_DGRY); print("  │  "); col(C_RED);
    print("✘ "); col(C_WHT);
    print("%s\n", b); col(C_RST);
}

// ── progress bar ──
static void bar(const char* label, int pct) {
    const int W = 28;
    if (pct > 100) pct = 100;
    if (pct < 0) pct = 0;
    int fill = pct * W / 100;
    col(C_DGRY); print("  │  ");
    col(C_CYN); print("%-9s", label);
    col(C_DGRY); print(" ┃");
    col(C_BLU);
    for (int i = 0; i < fill; i++) print("█");
    col(C_DGRY);
    for (int i = fill; i < W; i++) print("░");
    col(C_DGRY); print("┃ ");
    col(C_WHT); col(C_BLD);
    print("%3d%%", pct);
    col(C_RST); print("\n");
    fflush(stdout);
}

// ── menu items ──
static void menu_entry(int num, const char* label, const char* desc) {
    col(C_DGRY); print("  │     ");
    col(C_BLU); col(C_BLD); print("[%d]", num);
    col(C_RST); col(C_BLD); print("  %-24s", label);
    col(C_GRY); col(C_DIM); print("%s\n", desc);
    col(C_RST);
}
static void menu_entry_hot(int num, const char* label, const char* desc) {
    col(C_DGRY); print("  │     ");
    col(C_BLU); col(C_BLD); print("[%d]", num);
    col(C_RST); col(C_BLD); print("  ");
    col(C_CYN); print("%-24s", label);
    col(C_GRY); print("%s\n", desc);
    col(C_RST);
}
static void menu_input(const char* prompt) {
    col(C_DGRY); print("  │   \n  │   ");
    col(C_CYN); col(C_BLD); print(" %s ", prompt);
    col(C_WHT); print("▸ "); col(C_RST);
    fflush(stdout);
}
static int read_choice() {
    int c = _getch();
    if (c >= '0' && c <= '9') { print("%c\n", c); return c - '0'; }
    print("?\n");
    return 0;
}

// ── summary ──
static void summary(int ok_count, int fail_count) {
    print("\n");
    box_top();
    col(C_DGRY); print("  │    "); col(C_GRN); col(C_BLD);
    print("✔  %s: %d", tr("Установлено", "Installed"), ok_count);
    col(C_DGRY); print("  │\n");
    if (fail_count > 0) {
        col(C_DGRY); print("  │    "); col(C_RED); col(C_BLD);
        print("✘  %s: %d", tr("Ошибок", "Errors"), fail_count);
        col(C_DGRY); print("  │\n");
    }
    col(C_DGRY); print("  │    "); col(C_YEL);
    print("!  %s", tr("Перезапустите Steam для применения изменений!",
                     "Restart Steam to apply changes!"));
    col(C_DGRY); print("  │\n");
    box_bot();
}

// ── paths ──
static std::string tmp_path() {
    char buf[MAX_PATH]; GetTempPathA(sizeof(buf), buf);
    return std::string(buf);
}

static std::string steam_path() {
    char buf[MAX_PATH] = {0}; DWORD sz = sizeof(buf);
    if (RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Valve\\Steam",
                     "InstallPath", RRF_RT_REG_SZ, nullptr, buf, &sz) == ERROR_SUCCESS && buf[0])
        return buf;
    sz = sizeof(buf);
    if (RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Valve\\Steam",
                     "InstallPath", RRF_RT_REG_SZ, nullptr, buf, &sz) == ERROR_SUCCESS && buf[0])
        return buf;
    sz = sizeof(buf);
    if (RegGetValueA(HKEY_CURRENT_USER, "SOFTWARE\\Valve\\Steam",
                     "SteamPath", RRF_RT_REG_SZ, nullptr, buf, &sz) == ERROR_SUCCESS && buf[0]) {
        std::string p = buf; for (auto& c : p) if (c == '/') c = '\\'; return p;
    }
    return "C:\\Program Files (x86)\\Steam";
}

static bool steam_running() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32 pe{ sizeof(pe) };
    bool found = false;
    for (BOOL r = Process32First(snap, &pe); r; r = Process32Next(snap, &pe))
        if (_stricmp(pe.szExeFile, "steam.exe") == 0) { found = true; break; }
    CloseHandle(snap);
    return found;
}

static void kill_steam() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32 pe{ sizeof(pe) };
    for (BOOL r = Process32First(snap, &pe); r; r = Process32Next(snap, &pe)) {
        if (_stricmp(pe.szExeFile, "steam.exe") == 0) {
            HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID);
            if (h) {
                log("%s (pid=%lu)...", tr("закрываю steam.exe", "closing steam.exe"), pe.th32ProcessID);
                TerminateProcess(h, 0);
                WaitForSingleObject(h, 8000);
                CloseHandle(h);
            }
        }
    }
    CloseHandle(snap);
    Sleep(2000);
    ok(tr("steam закрыт", "steam closed"));
}

static bool drop_res(int id, const std::string& out) {
    HRSRC h = FindResourceA(nullptr, MAKEINTRESOURCEA(id), RT_RCDATA);
    if (!h) { err(tr("ресурс %d не найден", "res %d not found"), id); return false; }
    DWORD sz = SizeofResource(nullptr, h);
    HGLOBAL g = LoadResource(nullptr, h);
    const void* p = LockResource(g);
    if (!p || !sz) { err(tr("ресурс %d пуст", "res %d empty"), id); return false; }
    DeleteFileW(widen(out).c_str());
    HANDLE f = CreateFileW(widen(out).c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) { err(tr("не удалось записать %s", "cannot write %s"), out.c_str()); return false; }
    DWORD wr = 0; WriteFile(f, p, sz, &wr, nullptr); CloseHandle(f);
    if (wr != sz) { err(tr("короткая запись %s", "short write %s"), out.c_str()); return false; }
    return true;
}

static bool dl(const std::string& url, const std::string& out) {
    std::string ps =
        "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
        "try { "
        "$headers = @{ 'Referer'='https://walftech.com/generator.html'; 'User-Agent'='Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/131.0.0.0' }; "
        "Invoke-WebRequest -Uri '" + url + "' -OutFile '" + out + "' -Headers $headers -UseBasicParsing -TimeoutSec 120; "
        "$f=Get-Item '" + out + "'; Write-Output ('SIZE:' + $f.Length) "
        "} catch { Write-Output 'ERR:download failed'; exit 1 }\"";
    FILE* pipe = _popen(ps.c_str(), "r");
    if (!pipe) { err("popen failed"); return false; }
    char result[256] = {0};
    fgets(result, sizeof(result), pipe);
    _pclose(pipe);
    result[strcspn(result, "\n")] = 0;
    if (strstr(result, "ERR:")) { DeleteFileA(out.c_str()); return false; }
    log("%s", result);
    return true;
}

static int dl_pattern_fast(const std::string& pat) {
    std::string zip = pat + "\\__repo.zip";
    std::string ext = pat + "\\__ext";
    fs::create_directories(widen(pat));
    if (!dl("https://codeload.github.com/OpenSteam001/steam-monitor/zip/refs/heads/protobuf", zip))
        return 0;
    std::string ps =
        "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
        "Expand-Archive -LiteralPath '" + zip + "' -DestinationPath '" + ext + "' -Force; "
        "$root=(Get-ChildItem -LiteralPath '" + ext + "')[0].FullName; "
        "foreach($d in @('steamui','steamclient')){"
        "$s=Join-Path $root $d; $t=Join-Path '" + pat + "' $d; "
        "if(Test-Path $s){ New-Item -ItemType Directory -Path $t -Force|Out-Null; "
        "Copy-Item -Path (Join-Path $s '*') -Destination $t -Recurse -Force } }; "
        "Remove-Item -LiteralPath '" + zip + "' -Force -ErrorAction SilentlyContinue; "
        "Remove-Item -LiteralPath '" + ext + "' -Recurse -Force -ErrorAction SilentlyContinue\"";
    if (system(ps.c_str()) != 0) return 0;
    int n = 0; std::error_code ec;
    for (auto& sub : { pat + "\\steamui", pat + "\\steamclient" })
        for (auto& e : fs::recursive_directory_iterator(widen(sub), ec))
            if (!ec && e.is_regular_file()) n++;
    return n;
}

// ── install helpers ──
struct Paths {
    std::string steam, ost, ipc, pat, cfg, lua, stp, depot;
};

static bool make_dirs(Paths& p) {
    std::error_code ec;
    fs::create_directories(widen(p.ipc), ec);
    fs::create_directories(widen(p.pat), ec);
    fs::create_directories(widen(p.lua), ec);
    fs::create_directories(widen(p.stp), ec);
    fs::create_directories(widen(p.depot), ec);
    return !ec;
}

static int install_dlls(const std::string& steam) {
    int n = 0;
    n += drop_res(101, steam + "\\dwmapi.dll")        ? 1 : 0;
    n += drop_res(102, steam + "\\OpenSteamTool.dll")  ? 1 : 0;
    n += drop_res(103, steam + "\\xinput1_4.dll")      ? 1 : 0;
    return n;
}

static bool install_game(const std::string& appid, const Paths& p) {
    std::string tmpDir = tmp_path() + "\\ost_" + appid;
    std::string zipPath = tmpDir + "\\" + appid + ".zip";
    std::error_code ec;
    fs::create_directories(widen(tmpDir), ec);

    std::string url = "https://walftech.com/proxy.php?url=https://steamgames554.s3.us-east-1.amazonaws.com/"
                      + appid + ".zip";

    log("%s appid=%s ...", tr("загрузка", "download"), appid.c_str());
    if (!dl(url, zipPath)) {
        err("%s appid=%s", tr("не удалось скачать", "cannot download"), appid.c_str());
        fs::remove_all(widen(tmpDir), ec);
        return false;
    }

    std::string psScript = tmpDir + "\\inst.ps1";
    FILE* psf = fopen(psScript.c_str(), "w");
    if (!psf) { err(tr("не удалось создать скрипт", "cannot create script")); fs::remove_all(widen(tmpDir), ec); return false; }
    fprintf(psf,
        "$ErrorActionPreference='SilentlyContinue'\n"
        "Expand-Archive -LiteralPath '%s' -DestinationPath '%s' -Force\n"
        "Get-ChildItem -LiteralPath '%s' -Recurse -Filter '*.lua' | ForEach-Object {\n"
        "  Copy-Item $_.FullName -Destination (Join-Path '%s' $_.Name) -Force\n"
        "  Copy-Item $_.FullName -Destination (Join-Path '%s' $_.Name) -Force\n"
        "}\n"
        "Get-ChildItem -LiteralPath '%s' -Recurse -Filter '*.manifest' | ForEach-Object {\n"
        "  Copy-Item $_.FullName -Destination (Join-Path '%s' $_.Name) -Force\n"
        "}\n",
        zipPath.c_str(), tmpDir.c_str(),
        tmpDir.c_str(),
        p.lua.c_str(), p.stp.c_str(),
        tmpDir.c_str(),
        p.depot.c_str()
    );
    fclose(psf);
    std::string cmd = "powershell -NoProfile -ExecutionPolicy Bypass -File \"" + psScript + "\" 2>nul";
    system(cmd.c_str());

    int nlua = 0, nman = 0;
    for (auto& e : fs::directory_iterator(widen(tmpDir), ec)) {
        if (ec) break;
        if (e.path().extension() == ".lua") nlua++;
        if (e.path().extension() == ".manifest") nman++;
    }
    { char t[128]; snprintf(t, sizeof(t), "lua: %d, manifests: %d", nlua, nman); ok("%s", t); }
    fs::remove_all(widen(tmpDir), ec);
    return (nlua + nman) > 0;
}

// ── popular games ──
static const struct { const char* id; const char* name; } popular[] = {
    {"271590",  "GTA V"},            {"1174180", "Red Dead Redemption 2"},
    {"2344520", "Diablo IV"},         {"3595230", "Call of Duty MW2"},
    {"1285190", "Borderlands 4"},     {"2483190", "Forza Horizon 6"},
    {"2369390", "Far Cry 6"},         {"552520",  "Far Cry 5"},
    {"298110",  "Far Cry 4"},         {"220240",  "Far Cry 3"},
    {"19900",   "Far Cry 2"},         {"3764200", "Resident Evil Requiem"},
    {"2584270", "Mortal Shell II"},   {"1172620", "Sea of Thieves"},
    {"3219630", "Halloween Game"},    {"292030",  "The Witcher 3"},
    {"1091500", "Cyberpunk 2077"},    {"1245620", "Elden Ring"},
    {"814380",  "Sekiro"},            {"275850",  "No Man's Sky"},
    {"730",     "Counter-Strike 2"},  {"570",     "Dota 2"},
    {"440",     "Team Fortress 2"},   {"620",     "Portal 2"},
    {"4000",    "Garry's Mod"},
    {nullptr, nullptr}
};

static void print_game_list(const int* idxs, int n) {
    for (int i = 0; i < n; i++) {
        int k = idxs[i];
        col(C_DGRY); print("  │    ");
        col(C_BLU); col(C_BLD); print("%2d  ", i + 1);
        col(C_WHT); print("%-20s", popular[k].name);
        col(C_GRY); col(C_DIM); print("%s\n", popular[k].id);
        col(C_RST);
    }
}

// ── game selection ──
static std::vector<std::string> select_games() {
    std::vector<std::string> ids;
    section(1, tr("ВЫБОР ИГР", "GAME SELECT"));
    box_blank();
    menu_entry(1, tr("Ввести AppID", "Enter AppID"),   tr("вручную", "manually"));
    menu_entry(2, tr("Популярные игры", "Popular"),    tr("список 25", "list of 25"));
    menu_entry(3, tr("Быстрый поиск", "Quick search"), tr("по названию", "by name"));
    menu_entry(4, tr("Назад", "Back"),                 tr("в главное меню", "to main menu"));
    menu_input(tr("Выбор", "Select"));
    int choice = read_choice();

    if (choice == 1) {
        box_blank();
        col(C_DGRY); print("  │    "); col(C_WHT);
        print("%s", tr("AppID (напр. 271590)", "AppID (e.g. 271590)"));
        col(C_RST); print("\n");
        menu_input("AppID");
        char buf[32] = {0};
        if (fgets(buf, sizeof(buf), stdin)) {
            buf[strcspn(buf, "\r\n")] = 0;
            if (buf[0]) ids.push_back(buf);
        }
    } else if (choice == 2) {
        box_blank();
        int n = 0;
        for (int i = 0; popular[i].id; i++) n++;
        static int all[32];
        for (int i = 0; i < n; i++) all[i] = i;
        print_game_list(all, n);
        box_blank();
        menu_input(tr("Номера", "Numbers"));
        char buf[256] = {0};
        if (fgets(buf, sizeof(buf), stdin)) {
            buf[strcspn(buf, "\r\n")] = 0;
            char* tok = strtok(buf, " ,");
            while (tok) {
                int idx = atoi(tok) - 1;
                if (idx >= 0 && idx < n && popular[idx].id) ids.push_back(popular[idx].id);
                tok = strtok(nullptr, " ,");
            }
        }
    } else if (choice == 3) {
        box_blank();
        col(C_DGRY); print("  │    "); col(C_WHT);
        print("%s", tr("Введите часть названия", "Type a part of the name"));
        col(C_RST); print("\n");
        menu_input(tr("Поиск", "Search"));
        char q[64] = {0};
        if (!fgets(q, sizeof(q), stdin)) q[0] = 0;
        q[strcspn(q, "\r\n")] = 0;

        int matches[32], n = 0;
        for (int i = 0; popular[i].id && n < 32; i++)
            if (q[0] && contains_ci(popular[i].name, q)) matches[n++] = i;

        box_blank();
        if (n == 0) {
            err(tr("ничего не найдено", "nothing found"));
        } else {
            ok("%s: %d", tr("найдено", "found"), n);
            box_blank();
            print_game_list(matches, n);
            box_blank();
            menu_input(tr("Номера", "Numbers"));
            char buf[256] = {0};
            if (fgets(buf, sizeof(buf), stdin)) {
                buf[strcspn(buf, "\r\n")] = 0;
                char* tok = strtok(buf, " ,");
                while (tok) {
                    int idx = atoi(tok) - 1;
                    if (idx >= 0 && idx < n) ids.push_back(popular[matches[idx]].id);
                    tok = strtok(nullptr, " ,");
                }
            }
        }
    } else if (choice == 4) {
        box_bot();
        return ids;
    }

    if (ids.empty()) {
        box_blank();
        err(choice ? tr("не выбрано ни одной игры", "no games selected")
                   : tr("неверный выбор", "invalid choice"));
        box_bot();
        return ids;
    }
    box_blank();
    { char t[64]; snprintf(t, sizeof(t), "%s: %zu", tr("выбрано", "selected"), ids.size()); ok("%s", t); }
    box_bot();
    return ids;
}

// ── main actions ──
static bool need_steam(Paths& p) {
    p.steam = steam_path();
    if (!fs::exists(widen(p.steam))) {
        err(tr("steam не найден — установите Steam", "steam not found"));
        box_bot();
        return false;
    }
    p.ost    = p.steam + "\\opensteamtool";
    p.ipc    = p.ost + "\\ipc";
    p.pat    = p.ost + "\\pattern";
    p.cfg    = p.steam + "\\config";
    p.lua    = p.cfg + "\\Lua";
    p.stp    = p.cfg + "\\stplug-in";
    p.depot  = p.steam + "\\depotcache";
    return true;
}

static void close_steam_if_running() {
    if (steam_running()) {
        warn(tr("steam запущен — закрываю...", "steam running - closing..."));
        kill_steam();
    } else {
        ok(tr("steam не запущен", "steam not running"));
    }
}

static void do_base_setup(Paths& p);

static void do_full_install() {
    Paths p;
    section(1, tr("ПОИСК STEAM", "STEAM SEARCH"));
    if (!need_steam(p)) return;
    log("path: %s", p.steam.c_str());
    ok(tr("steam найден", "steam found"));
    bar("scan", 10);
    box_bot();

    do_base_setup(p);

    std::vector<std::string> ids = select_games();
    if (ids.empty()) return;

    section(6, tr("УСТАНОВКА ИГР", "INSTALL GAMES"));
    int nOk = 0, nFail = 0;
    for (size_t i = 0; i < ids.size(); i++) {
        col(C_DGRY); print("  │    ");
        col(C_PRP); col(C_BLD);
        print("[%zu/%zu]", i + 1, ids.size());
        col(C_RST); col(C_BLD);
        print(" appid=%s\n", ids[i].c_str());
        col(C_RST);
        if (install_game(ids[i], p)) nOk++;
        else nFail++;
    }
    bar("done", 100);
    box_bot();
    summary(nOk, nFail);
}

static bool dlls_installed(const std::string& steam) {
    return fs::exists(widen(steam + "\\dwmapi.dll")) &&
           fs::exists(widen(steam + "\\OpenSteamTool.dll")) &&
           fs::exists(widen(steam + "\\xinput1_4.dll"));
}

static bool pattern_installed(const std::string& pat) {
    std::error_code ec;
    if (!fs::exists(widen(pat))) return false;
    int n = 0;
    for (auto& e : fs::recursive_directory_iterator(widen(pat), ec)) {
        if (ec) return false;
        if (e.is_regular_file()) { n++; if (n >= 5) return true; }
    }
    return false;
}

static void do_base_setup(Paths& p) {
    section(2, tr("ПРОВЕРКА STEAM", "CHECK STEAM"));
    close_steam_if_running();
    bar("steam", 15);
    box_bot();

    if (!dlls_installed(p.steam)) {
        warn(tr("dll ещё не установлены — ставлю сначала их", "dll not installed - installing first"));
        section(3, tr("УСТАНОВКА DLL", "DLL INSTALL"));
        int ndll = install_dlls(p.steam);
        { char t[64]; snprintf(t, sizeof(t), "dll %s: %d/3", tr("установлено", "installed"), ndll); ok("%s", t); }
        bar("dll", 35);
        box_bot();
    } else {
        ok(tr("dll уже установлены", "dll already installed"));
    }

    section(4, tr("СОЗДАНИЕ ПАПОК", "FOLDERS"));
    make_dirs(p);
    ok("opensteamtool/ipc + pattern");
    ok("config/Lua + stplug-in + depotcache");
    bar("dirs", 45);
    box_bot();

    if (!pattern_installed(p.pat)) {
        warn(tr("pattern ещё не загружен — качаю сначала его", "pattern not downloaded - fetching first"));
        section(5, tr("ЗАГРУЗКА PATTERN", "PATTERN"));
        log("steamui + steamclient (github)...");
        int npat = dl_pattern_fast(p.pat);
        { char t[64]; snprintf(t, sizeof(t), "%s: %d", tr("файлов", "files"), npat); ok("%s", t); }
        bar("pattern", 60);
        box_bot();
    } else {
        section(5, tr("PATTERN", "PATTERN"));
        ok(tr("pattern уже установлен", "pattern already installed"));
        bar("pattern", 60);
        box_bot();
    }
}

static void do_games_only() {
    Paths p;
    section(1, tr("ПОИСК STEAM", "STEAM SEARCH"));
    if (!need_steam(p)) return;
    log("path: %s", p.steam.c_str());
    ok(tr("steam найден", "steam found"));
    bar("scan", 10);
    box_bot();

    do_base_setup(p);

    std::vector<std::string> ids = select_games();
    if (ids.empty()) return;

    section(6, tr("УСТАНОВКА ИГР", "INSTALL GAMES"));
    int nOk = 0, nFail = 0;
    for (size_t i = 0; i < ids.size(); i++) {
        col(C_DGRY); print("  │    ");
        col(C_PRP); col(C_BLD);
        print("[%zu/%zu]", i + 1, ids.size());
        col(C_RST); col(C_BLD);
        print(" appid=%s\n", ids[i].c_str());
        col(C_RST);
        if (install_game(ids[i], p)) nOk++;
        else nFail++;
    }
    bar("done", 100);
    box_bot();
    summary(nOk, nFail);
}

static void do_update_pattern() {
    Paths p;
    section(1, tr("ПОИСК STEAM", "STEAM SEARCH"));
    if (!need_steam(p)) return;
    ok(tr("steam найден", "steam found"));
    box_bot();

    section(2, tr("ОБНОВЛЕНИЕ PATTERN", "UPDATE PATTERN"));
    make_dirs(p);
    log("steamui + steamclient (github)...");
    int npat = dl_pattern_fast(p.pat);
    { char t[64]; snprintf(t, sizeof(t), "%s: %d", tr("файлов", "files"), npat); ok("%s", t); }
    bar("pattern", 100);
    box_bot();
}

static void do_remove() {
    Paths p;
    section(1, tr("УДАЛЕНИЕ", "REMOVE"));
    if (!need_steam(p)) return;
    close_steam_if_running();

    int n = 0;
    for (auto& f : { "\\dwmapi.dll", "\\OpenSteamTool.dll", "\\xinput1_4.dll" }) {
        std::string fp = p.steam + f;
        if (fs::exists(widen(fp))) { DeleteFileW(widen(fp).c_str()); n++; }
    }
    { char t[64]; snprintf(t, sizeof(t), "удалено dll: %d/3", n); ok("%s", t); }

    std::error_code ec;
    if (fs::exists(widen(p.ost))) {
        int nf = 0;
        for (auto& e : fs::recursive_directory_iterator(widen(p.ost), ec))
            if (!ec && e.is_regular_file()) nf++;
        fs::remove_all(widen(p.ost), ec);
        { char t[64]; snprintf(t, sizeof(t), "удалено файлов opensteamtool: %d", nf); ok("%s", t); }
    } else {
        log("opensteamtool не найден");
    }
    bar("remove", 100);
    box_bot();

    col(C_DGRY); print("  │\n");
    warn(tr("файлы игр в config/Lua и depotcache не тронуты",
            "game files untouched"));
    col(C_DGRY); print("  │\n");
    box_bot();
}

// ── main menu ──
static bool dlls_installed(const std::string& steam);
static bool pattern_installed(const std::string& pat);

static void main_menu() {
    Paths tmp;
    tmp.steam = steam_path();
    tmp.ost   = tmp.steam + "\\opensteamtool";
    tmp.pat   = tmp.ost + "\\pattern";

    bool dll_ok = dlls_installed(tmp.steam);
    bool pat_ok = pattern_installed(tmp.pat);

    box_top();
    const char* title = tr("ГЛАВНОЕ МЕНЮ", "MAIN MENU");
    col(C_DGRY); print("  │ ");
    col(C_WHT); col(C_BLD);
    print("%s", title);
    col(C_RST);
    int used = 2 + 1 + (int)strlen(title);
    col(C_DGRY);
    for (int i = used; i < BOX_W; i++) print("─");
    print("│\n");
    box_blank();

    col(C_DGRY); print("  │   "); col(C_DIM);
    print("  %s: ", tr("Статус", "Status"));
    if (dll_ok) { col(C_GRN); print("dll ✔"); }
    else        { col(C_RED); print("dll ✘"); }
    col(C_DIM); print("   ");
    if (pat_ok) { col(C_GRN); print("pattern ✔"); }
    else        { col(C_RED); print("pattern ✘"); }
    col(C_RST); print("\n");
    box_blank();
    col(C_DGRY); print("  │   ");
    for (int i = 0; i < BOX_W - 4; i++) print("┄");
    print("\n"); col(C_RST);
    box_blank();

    menu_entry_hot(1, tr("Установить всё", "Install all"),   tr("dll+pattern+игры", "dll+pattern+games"));
    box_blank();
    menu_entry(2, tr("Только игры", "Games only"),           tr("авто: dll+pattern если нет", "auto: dll+pattern first"));
    box_blank();
    menu_entry(3, tr("Обновить pattern", "Update pattern"),  tr("steamui+steamclient", "steamui+steamclient"));
    box_blank();
    menu_entry(4, tr("Удалить", "Remove"),                   tr("dll+opensteamtool", "dll+opensteamtool"));
    box_blank();
    { char lbl[32]; snprintf(lbl, sizeof(lbl), "%s [%s]", tr("Язык", "Language"), g_lang ? "EN" : "RU");
      menu_entry(5, lbl, tr("переключить", "switch")); }
    box_blank();
    menu_entry(6, tr("Выход", "Exit"),                       tr("закрыть", "close"));
    menu_input(tr("Выбор", "Select"));
}

int main() {
    AttachConsole(ATTACH_PARENT_PROCESS);
    SetConsoleOutputCP(CP_UTF8);
    con_init();

    bool running = true;
    while (running) {
        banner();
        main_menu();
        int c = read_choice();
        print("\n");
        switch (c) {
            case 1: do_full_install();   break;
            case 2: do_games_only();     break;
            case 3: do_update_pattern(); break;
            case 4: do_remove();         break;
            case 5:
                g_lang = !g_lang;
                ok("%s -> %s", tr("язык", "language"), g_lang ? "EN" : "RU");
                break;
            case 6: running = false;     break;
            default:
                err(tr("неверный выбор", "invalid choice"));
                break;
        }
        if (!running) break;
        col(C_GRY);
        print("\n  %s...", tr("нажмите любую клавишу", "press any key"));
        col(C_RST); fflush(stdout);
        _getch();
        print("\n");
    }

    print("\n");
    col(C_GRY); print("  %s!\n", tr("до встречи", "goodbye")); col(C_RST);
    _getch();
    print("\n");
    return 0;
}
