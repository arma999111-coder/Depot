// main.cpp — entry point + menus
#include "common.h"
#include "ui.h"
#include "core.h"
#include <conio.h>

// ── setup helpers ──
static void ensure_base(Paths& p, bool with_dll) {
    if (with_dll) {
        bx_top(tr("DLL", "DLL"));
        if (dlls_ok(p.steam)) ok(tr("уже установлены", "already installed"));
        else {
            int n = install_dlls(p.steam);
            ok("dll: %d/3", n);
        }
        bar("dll", 30);
        bx_bot();
    }
    bx_top(tr("PATTERN", "PATTERN"));
    ensure_dirs(p);
    if (pattern_ok(p.pat)) ok(tr("уже установлен", "already installed"));
    else {
        info(tr("загрузка (github)...", "downloading (github)..."));
        int n = dl_pattern(p.pat);
        ok("%s: %d", tr("файлов", "files"), n);
    }
    bar("pattern", 50);
    bx_bot();
}

static void close_steam() {
    if (steam_running()) {
        bx_top(tr("STEAM", "STEAM"));
        warn(tr("steam запущен, закрываю...", "steam running, closing..."));
        kill_steam();
        ok(tr("закрыт", "closed"));
        bx_bot();
    }
}

// ── game installer (no setup — caller does it) ──
static void game_input_loop(const Paths& p) {
    bx_top(tr("УСТАНОВКА ИГРЫ", "INSTALL GAME"));
    bx_empty();
    info(tr("Введите AppID и Enter — игра скачается", "Enter AppID + Enter — game downloads"));
    info(tr("Можно вводить ещё — по одной", "Keep entering — one at a time"));
    info(tr("0 — назад", "0 — back"));
    bx_empty();

    int nOk = 0, nFail = 0;
    while (true) {
        ask(tr("AppID", "AppID"));
        char buf[64] = {0};
        if (!read_line(buf, sizeof(buf))) break;
        if (!buf[0]) continue;
        if (strcmp(buf, "0") == 0) break;

        bool valid = true;
        for (char* c = buf; *c; c++)
            if (!isdigit((unsigned char)*c)) { valid = false; break; }
        if (!valid) { err("%s: %s", tr("не AppID", "not AppID"), buf); continue; }

        std::string appid(buf);
        print(C_DGRY "  \xe2\x94\x82" C_RST "  " C_PRP C_BLD "\xe2\x96\xb6 appid=%s" C_RST "\n", appid.c_str());
        if (install_game(appid, p)) { nOk++; ok(tr("установлено", "installed")); }
        else { nFail++; err(tr("не удалось скачать", "download failed")); }
        print("\n");
    }
    bx_bot();

    if (nOk + nFail > 0) {
        bx_top(tr("ИТОГ", "RESULT"));
        bx_empty();
        print(C_DGRY "  \xe2\x94\x82" C_RST "  " C_GRN C_BLD "\xe2\x9c\x93 %s: %d" C_RST "\n", tr("установлено", "installed"), nOk);
        if (nFail > 0)
            print(C_DGRY "  \xe2\x94\x82" C_RST "  " C_RED C_BLD "\xe2\x9c\x97 %s: %d" C_RST "\n", tr("ошибок", "errors"), nFail);
        bx_empty();
        print(C_DGRY "  \xe2\x94\x82" C_RST "  " C_YEL "! %s" C_RST "\n", tr("Перезапустите Steam!", "Restart Steam!"));
        bx_empty();
        bx_bot();
    }
}

// ── full install ──
static void full_install() {
    Paths p;
    bx_top(tr("ПОИСК STEAM", "STEAM"));
    if (!init_paths(p)) { err(tr("steam не найден", "steam not found")); bx_bot(); return; }
    ok(tr("найден", "found"));
    info(p.steam.c_str());
    bar("steam", 10);
    bx_bot();

    close_steam();
    ensure_base(p, true);
    game_input_loop(p);
}

// ── games only ──
static void games_only() {
    Paths p;
    bx_top(tr("ПОИСК STEAM", "STEAM"));
    if (!init_paths(p)) { err(tr("steam не найден", "steam not found")); bx_bot(); return; }
    ok(tr("найден", "found"));
    bx_bot();

    close_steam();
    ensure_base(p, false);
    game_input_loop(p);
}

// ── update pattern ──
static void update_pattern() {
    Paths p;
    bx_top(tr("ОБНОВЛЕНИЕ PATTERN", "UPDATE PATTERN"));
    if (!init_paths(p)) { err(tr("steam не найден", "steam not found")); bx_bot(); return; }
    ensure_dirs(p);
    info(tr("загрузка (github)...", "downloading (github)..."));
    int n = dl_pattern(p.pat);
    ok("%s: %d", tr("файлов", "files"), n);
    bar("pattern", 100);
    bx_bot();
}

// ── remove ──
static void do_remove() {
    Paths p;
    bx_top(tr("УДАЛЕНИЕ", "REMOVE"));
    if (!init_paths(p)) { err(tr("steam не найден", "steam not found")); bx_bot(); return; }
    close_steam();

    int n = 0;
    for (auto& f : { "\\dwmapi.dll", "\\OpenSteamTool.dll", "\\xinput1_4.dll" }) {
        std::string fp = p.steam + f;
        if (fs::exists(widen(fp))) { DeleteFileW(widen(fp).c_str()); n++; }
    }
    ok("dll: %d/3", n);

    std::error_code ec;
    if (fs::exists(widen(p.ost))) {
        int nf = 0;
        for (auto& e : fs::recursive_directory_iterator(widen(p.ost), ec))
            if (!ec && e.is_regular_file()) nf++;
        fs::remove_all(widen(p.ost), ec);
        ok("%s opensteamtool: %d", tr("удалено", "removed"), nf);
    }
    bar("remove", 100);
    bx_bot();
}

// ── install submenu ──
static void install_menu() {
    while (true) {
        bx_top(tr("УСТАНОВКА", "INSTALL"));
        bx_empty();
        menu_item(1, true,  tr("Полная установка", "Full install"),    tr("dll+pattern+игра", "dll+pattern+game"));
        menu_gap();
        menu_item(2, false, tr("Только игру", "Game only"),            tr("авто добавит base", "auto-adds base"));
        menu_gap();
        menu_item(0, false, tr("Назад", "Back"),                       tr("", ""));
        bx_empty();
        ask(tr("Выбор", "Select"));
        int c = read_key();
        print("\n\n");
        switch (c) {
            case 1: full_install(); break;
            case 2: games_only();   break;
            case 0: return;
            default: err(tr("неверный выбор", "invalid choice")); break;
        }
        pause_back();
    }
}

// ── main menu ──
static void main_menu() {
    Paths p; bool has = init_paths(p);
    bool dok = has && dlls_ok(p.steam);
    bool pok = has && pattern_ok(p.pat);

    bx_top(tr("ГЛАВНОЕ МЕНЮ", "MAIN MENU"));
    bx_empty();

    // status
    print(C_DGRY "  \xe2\x94\x82" C_RST "  " C_GRY "%s:" C_RST " %s%s" C_RST
          "   " C_GRY "%s:" C_RST " %s%s" C_RST,
          tr("DLL", "DLL"), dok ? C_GRN : C_RED, dok ? "OK" : "--",
          tr("PATTERN", "PATTERN"), pok ? C_GRN : C_RED, pok ? "OK" : "--");
    int vis = 4 + (int)strlen(tr("DLL", "DLL")) + 2 + 2 + 3 + (int)strlen(tr("PATTERN", "PATTERN")) + 2 + 2;
    for (int i = vis; i < W; i++) print(" ");
    print(C_DGRY "\xe2\x94\x82" C_RST "\n");
    bx_empty();

    menu_item(1, true,  tr("Установить", "Install"),       tr("dll + pattern + игра", "dll+pattern+game"));
    menu_gap();
    menu_item(2, false, tr("Обновить pattern", "Pattern"), tr("steamui + steamclient", "steamui+steamclient"));
    menu_gap();
    menu_item(3, false, tr("Удалить", "Remove"),           tr("dll + opensteamtool", "dll+opensteamtool"));
    menu_gap();
    char lang[32]; snprintf(lang, sizeof(lang), "%s [%s]", tr("Язык", "Lang"), g_lang ? "EN" : "RU");
    menu_item(4, false, lang,                               tr("переключить", "switch"));
    menu_gap();
    menu_item(5, false, tr("Выход", "Exit"),               tr("", ""));

    bx_empty();
    ask(tr("Выбор", "Select"));
}

// ── main ──
int main() {
    AttachConsole(ATTACH_PARENT_PROCESS);
    SetConsoleOutputCP(CP_UTF8);
    ui_init();

    bool run = true;
    while (run) {
        banner();
        main_menu();
        int c = read_key();
        print("\n\n");
        switch (c) {
            case 1: install_menu();      break;
            case 2: update_pattern();    break;
            case 3: do_remove();         break;
            case 4: g_lang = !g_lang; ok("%s -> %s", tr("язык", "lang"), g_lang ? "EN" : "RU"); break;
            case 5: run = false;         break;
            default: err(tr("неверный выбор", "invalid choice")); break;
        }
        if (!run) break;
        pause_back();
    }
    print("\n  " C_GRY "%s!" C_RST "\n\n", tr("до встречи", "goodbye"));
    return 0;
}
