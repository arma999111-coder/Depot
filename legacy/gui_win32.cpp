// gui.cpp — Win32 GUI, dark theme
#include "common.h"
#include "core.h"
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <functional>
#include <string>
#include <vector>
#include <thread>
#include <atomic>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")

// ── colors ──
static const COLORREF CLR_BG      = RGB(15, 15, 26);
static const COLORREF CLR_PANEL   = RGB(22, 22, 38);
static const COLORREF CLR_PANEL2  = RGB(30, 30, 48);
static const COLORREF CLR_BORDER  = RGB(55, 55, 75);
static const COLORREF CLR_ACCENT  = RGB(0, 170, 255);
static const COLORREF CLR_ACCENT2 = RGB(0, 220, 200);
static const COLORREF CLR_OK      = RGB(0, 210, 90);
static const COLORREF CLR_WARN    = RGB(255, 195, 50);
static const COLORREF CLR_ERR     = RGB(255, 70, 70);
static const COLORREF CLR_TEXT    = RGB(235, 235, 240);
static const COLORREF CLR_MUTED   = RGB(110, 110, 130);
static const COLORREF CLR_HOVER   = RGB(40, 40, 65);

// ── controls ──
#define ID_BTN_INSTALL   101
#define ID_BTN_GAMES     102
#define ID_BTN_PATTERN   103
#define ID_BTN_REMOVE    104
#define ID_BTN_LANG      105
#define ID_EDIT_APPID    201
#define ID_EDIT_LOG      202
#define ID_BTN_ADD       203
#define ID_PROGRESS      301
#define ID_STATUS        401

static HWND hMain, hLog, hAppId, hProg, hStatus;
static HWND hBtn[7];
static std::atomic<bool> g_busy{false};
static int g_lang_gui = 0;
static const char* tg(const char* ru, const char* en) { return g_lang_gui ? en : ru; }

// ── dark brushes ──
static HBRUSH hbrBg, hbrPanel, hbrPanel2, hbrHover;

static void log_line(const char* line) {
    // strip ANSI
    std::string s;
    for (const char* p = line; *p; ) {
        if (*p == '\x1b') { while (*p && *p != 'm') p++; if (*p) p++; continue; }
        s += *p++;
    }
    s += "\r\n";
    int len = GetWindowTextLengthA(hLog);
    SendMessageA(hLog, EM_SETSEL, len, len);
    SendMessageA(hLog, EM_REPLACESEL, FALSE, (LPARAM)s.c_str());
}

// ── custom button ──
struct BtnState { RECT rc; const char* label; bool hot; bool enabled; COLORREF fg; };
static BtnState g_btns[7];

static HWND make_btn(HWND parent, int id, const char* label, COLORREF fg) {
    int idx = id - ID_BTN_INSTALL;
    g_btns[idx] = { {}, label, false, true, fg };
    return CreateWindowExA(0, "BUTTON", label,
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
}

static void draw_btn(LPDRAWITEMSTRUCT ds) {
    int idx = (int)ds->CtlID - ID_BTN_INSTALL;
    if (idx < 0 || idx >= 7) return;
    BtnState& b = g_btns[idx];
    HDC hdc = ds->hDC;
    RECT rc = ds->rcItem;
    bool pressed = (ds->itemState & ODS_SELECTED) != 0;
    bool hot = b.hot || pressed;

    // bg
    HBRUSH br = CreateSolidBrush(hot ? CLR_HOVER : CLR_PANEL2);
    FillRect(hdc, &rc, br);
    DeleteObject(br);

    // border
    HPEN pen = CreatePen(PS_SOLID, 1, hot ? b.fg : CLR_BORDER);
    HGDIOBJ old = SelectObject(hdc, pen);
    HGDIOBJ oldb = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
    SelectObject(hdc, old);
    SelectObject(hdc, oldb);
    DeleteObject(pen);

    // text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, b.enabled ? (hot ? CLR_TEXT : b.fg) : CLR_MUTED);
    HFONT font = CreateFontA(16, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    HGDIOBJ oldf = SelectObject(hdc, font);
    DrawTextA(hdc, b.label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldf);
    DeleteObject(font);
}

// ── layout ──
static void layout() {
    RECT rc; GetClientRect(hMain, &rc);
    int cw = rc.right, ch = rc.bottom;
    int mx = 20;
    int y = 16;

    // status line area: y..y+40
    y += 44;

    // log
    int logTop = y;
    int logH = ch - logTop - 140;
    if (logH < 100) logH = 100;
    SetWindowPos(hLog, nullptr, mx, logTop, cw - 2 * mx, logH, SWP_NOZORDER);
    y = logTop + logH + 12;

    // AppID row
    int btnW = 90, btnH = 34, gap = 10;
    SetWindowPos(hAppId, nullptr, mx, y, cw - 2 * mx - btnW - gap, btnH, SWP_NOZORDER);
    SetWindowPos(hBtn[6], nullptr, cw - mx - btnW, y, btnW, btnH, SWP_NOZORDER);
    y += btnH + 14;

    // action buttons row
    int bw = (cw - 2 * mx - 3 * gap) / 4;
    SetWindowPos(hBtn[0], nullptr, mx, y, bw, btnH, SWP_NOZORDER);
    SetWindowPos(hBtn[1], nullptr, mx + bw + gap, y, bw, btnH, SWP_NOZORDER);
    SetWindowPos(hBtn[2], nullptr, mx + 2 * (bw + gap), y, bw, btnH, SWP_NOZORDER);
    SetWindowPos(hBtn[3], nullptr, mx + 3 * (bw + gap), y, bw, btnH, SWP_NOZORDER);
    y += btnH + 14;

    // lang + progress
    int lw = 120;
    SetWindowPos(hBtn[4], nullptr, mx, y, lw, btnH, SWP_NOZORDER);
    SetWindowPos(hProg, nullptr, mx + lw + gap, y + 6, cw - 2 * mx - lw - gap, 22, SWP_NOZORDER);
    y += btnH + 8;

    // status bar
    SetWindowPos(hStatus, nullptr, mx, y, cw - 2 * mx, 20, SWP_NOZORDER);
}

// ── update status badges (drawn on parent) ──
static Paths g_paths;
static bool g_has = false, g_dok = false, g_pok = false;

static void refresh_status() {
    g_has = init_paths(g_paths);
    g_dok = g_has && dlls_ok(g_paths.steam);
    g_pok = g_has && pattern_ok(g_paths.pat);
    InvalidateRect(hMain, nullptr, TRUE);
}

static void set_status(const char* s) {
    SetWindowTextA(hStatus, s);
}

static void set_progress(int pct) {
    SendMessageA(hProg, PBM_SETPOS, pct, 0);
}

// ── actions (background) ──
static void run_async(std::function<void()> fn) {
    if (g_busy.exchange(true)) return;
    std::thread([fn] {
        fn();
        g_busy = false;
        PostMessageA(hMain, WM_APP + 10, 0, 0); // refresh
    }).detach();
}

static void do_full_install() {
    run_async([] {
        set_status(tg("Установка...", "Installing..."));
        if (steam_running()) { log_line(tg("steam запущен, закрываю...", "closing steam...")); kill_steam(); }
        if (g_has) {
            if (!g_dok) {
                log_line(tg("установка dll...", "installing dll..."));
                int n = install_dlls(g_paths.steam);
                char b[64]; snprintf(b, sizeof(b), "dll: %d/3", n); log_line(b);
            } else log_line(tg("dll уже установлены", "dll already installed"));
            set_progress(30);
            ensure_dirs(g_paths);
            if (!g_pok) {
                log_line(tg("загрузка pattern...", "downloading pattern..."));
                int n = dl_pattern(g_paths.pat);
                char b[64]; snprintf(b, sizeof(b), "%s: %d", tg("паттернов", "patterns"), n); log_line(b);
            } else log_line(tg("pattern уже установлен", "pattern already installed"));
            set_progress(50);
            log_line(tg("готово. Введите AppID ниже.", "done. Enter AppID below."));
            set_progress(100);
            set_status(tg("Готово — введите AppID", "Done — enter AppID"));
        }
    });
}

static void do_update_pattern() {
    run_async([] {
        set_status(tg("Обновление pattern...", "Updating pattern..."));
        ensure_dirs(g_paths);
        log_line(tg("загрузка pattern (github)...", "downloading pattern (github)..."));
        int n = dl_pattern(g_paths.pat);
        char b[64]; snprintf(b, sizeof(b), "%s: %d", tg("файлов", "files"), n); log_line(b);
        set_progress(100);
        set_status(tg("Pattern обновлён", "Pattern updated"));
    });
}

static void do_remove() {
    run_async([] {
        set_status(tg("Удаление...", "Removing..."));
        if (steam_running()) kill_steam();
        int n = 0;
        for (auto& f : { "\\dwmapi.dll", "\\OpenSteamTool.dll", "\\xinput1_4.dll" }) {
            std::string fp = g_paths.steam + f;
            if (fs::exists(widen(fp))) { DeleteFileW(widen(fp).c_str()); n++; }
        }
        char b[64]; snprintf(b, sizeof(b), "dll удалено: %d/3", n); log_line(b);
        std::error_code ec;
        if (fs::exists(widen(g_paths.ost))) {
            int nf = 0;
            for (auto& e : fs::recursive_directory_iterator(widen(g_paths.ost), ec))
                if (!ec && e.is_regular_file()) nf++;
            fs::remove_all(widen(g_paths.ost), ec);
            snprintf(b, sizeof(b), "файлов удалено: %d", nf); log_line(b);
        }
        set_progress(100);
        set_status(tg("Удалено", "Removed"));
    });
}

static void do_add_game(const std::string& appid) {
    if (appid.empty()) return;
    run_async([appid] {
        set_status("...");
        char b[128];
        snprintf(b, sizeof(b), tg("загрузка appid=%s ...", "downloading appid=%s ..."), appid.c_str());
        log_line(b);
        set_progress(20);
        ensure_dirs(g_paths);
        if (install_game(appid, g_paths)) {
            log_line(tg("установлено!", "installed!"));
            set_status(tg("Игра установлена", "Game installed"));
        } else {
            log_line(tg("ошибка скачивания", "download error"));
            set_status(tg("Ошибка скачивания", "Download error"));
        }
        set_progress(100);
    });
}

// ── dark edit/label colors ──
static HFONT hFontDef, hFontBold, hFontSmall;

// ── wndproc ──
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        // log
        hLog = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            0, 0, 10, 10, hWnd, (HMENU)ID_EDIT_LOG, GetModuleHandle(nullptr), nullptr);
        SendMessageA(hLog, WM_SETFONT, (WPARAM)hFontSmall, TRUE);
        SendMessageA(hLog, EM_SETLIMITTEXT, 65536, 0);

        // appid
        hAppId = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 10, 10, hWnd, (HMENU)ID_EDIT_APPID, GetModuleHandle(nullptr), nullptr);
        SendMessageA(hAppId, WM_SETFONT, (WPARAM)hFontDef, TRUE);
        SendMessageA(hAppId, EM_SETLIMITTEXT, 16, 0);

        // buttons
        hBtn[0] = make_btn(hWnd, ID_BTN_INSTALL, tg("Установить всё", "Install all"), CLR_ACCENT);
        hBtn[1] = make_btn(hWnd, ID_BTN_GAMES,   tg("Только игру", "Game only"),      CLR_ACCENT2);
        hBtn[2] = make_btn(hWnd, ID_BTN_PATTERN, tg("Обновить паттерн", "Update"),    CLR_TEXT);
        hBtn[3] = make_btn(hWnd, ID_BTN_REMOVE,  tg("Удалить", "Remove"),             CLR_ERR);
        hBtn[4] = make_btn(hWnd, ID_BTN_LANG,    "RU / EN",                           CLR_MUTED);
        hBtn[6] = make_btn(hWnd, ID_BTN_ADD,     tg("Добавить", "Add"),               CLR_OK);

        // progress
        hProg = CreateWindowExA(0, PROGRESS_CLASSA, "",
            WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, (HMENU)ID_PROGRESS, GetModuleHandle(nullptr), nullptr);
        SendMessageA(hProg, PBM_SETBARCOLOR, 0, (LPARAM)CLR_ACCENT);
        SendMessageA(hProg, PBM_SETBKCOLOR, 0, (LPARAM)CLR_PANEL2);
        SendMessageA(hProg, PBM_SETRANGE32, 0, 100);

        // status
        hStatus = CreateWindowExA(0, "STATIC", tg("Готово", "Ready"),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 10, 10, hWnd, (HMENU)ID_STATUS, GetModuleHandle(nullptr), nullptr);
        SendMessageA(hStatus, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

        layout();
        refresh_status();
        log_line("OpenSteamTool Installer v3.0 GUI");
        log_line(tg("Готово. Нажмите «Установить всё» или введите AppID.", "Ready. Click Install or enter AppID."));
        return 0;
    }

    case WM_SIZE:
        layout();
        return 0;

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        HWND ctrl = (HWND)lp;
        SetTextColor(hdc, CLR_TEXT);
        SetBkColor(hdc, CLR_BG);
        if (ctrl == hStatus) SetTextColor(hdc, CLR_MUTED);
        return (LRESULT)hbrBg;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, CLR_TEXT);
        SetBkColor(hdc, CLR_PANEL);
        return (LRESULT)hbrPanel;
    }

    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, CLR_TEXT);
        SetBkColor(hdc, CLR_PANEL);
        return (LRESULT)hbrPanel;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT ds = (LPDRAWITEMSTRUCT)lp;
        if (ds->CtlType == ODT_BUTTON) { draw_btn(ds); return TRUE; }
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == ID_BTN_INSTALL) { refresh_status(); do_full_install(); }
        else if (id == ID_BTN_GAMES) { refresh_status(); run_async([] {
            if (steam_running()) kill_steam();
            ensure_dirs(g_paths);
            log_line(tg("готово. Введите AppID ниже.", "done. Enter AppID below."));
            set_status(tg("Введите AppID", "Enter AppID"));
        }); }
        else if (id == ID_BTN_PATTERN) { refresh_status(); do_update_pattern(); }
        else if (id == ID_BTN_REMOVE) { refresh_status(); do_remove(); }
        else if (id == ID_BTN_LANG) {
            g_lang_gui = !g_lang_gui;
            SetWindowTextA(hBtn[0], tg("Установить всё", "Install all"));
            SetWindowTextA(hBtn[1], tg("Только игру", "Game only"));
            SetWindowTextA(hBtn[2], tg("Обновить паттерн", "Update"));
            SetWindowTextA(hBtn[3], tg("Удалить", "Remove"));
            SetWindowTextA(hBtn[6], tg("Добавить", "Add"));
            for (int i = 0; i < 7; i++) if (hBtn[i]) {
                int idx = i;
                const char* t = nullptr;
                if (i == 0) t = tg("Установить всё", "Install all");
                else if (i == 1) t = tg("Только игру", "Game only");
                else if (i == 2) t = tg("Обновить паттерн", "Update");
                else if (i == 3) t = tg("Удалить", "Remove");
                else if (i == 6) t = tg("Добавить", "Add");
                if (t) g_btns[idx].label = t;
            }
            for (int i = 0; i < 7; i++) if (hBtn[i]) InvalidateRect(hBtn[i], nullptr, TRUE);
        }
        else if (id == ID_BTN_ADD) {
            char buf[32] = {0};
            GetWindowTextA(hAppId, buf, sizeof(buf));
            bool valid = buf[0] != 0;
            for (char* c = buf; *c; c++) if (!isdigit((unsigned char)*c)) { valid = false; break; }
            if (valid) { do_add_game(buf); SetWindowTextA(hAppId, ""); }
            else if (buf[0]) log_line(tg("не AppID — только цифры", "not AppID — digits only"));
        }
        return 0;
    }

    case WM_APP + 10:
        refresh_status();
        return 0;

    case WM_MOUSEMOVE: {
        // hover tracking for buttons
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        for (int i = 0; i < 7; i++) {
            if (!hBtn[i]) continue;
            RECT rc; GetWindowRect(hBtn[i], &rc);
            MapWindowPoints(nullptr, hWnd, (POINT*)&rc, 2);
            bool in = PtInRect(&rc, pt);
            if (in != g_btns[i].hot) {
                g_btns[i].hot = in;
                InvalidateRect(hBtn[i], nullptr, TRUE);
            }
        }
        return 0;
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc; GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, hbrBg);

        // header line
        HPEN linePen = CreatePen(PS_SOLID, 1, CLR_BORDER);
        HGDIOBJ oldp = SelectObject(hdc, linePen);
        MoveToEx(hdc, 16, 58, nullptr);
        LineTo(hdc, rc.right - 16, 58);
        SelectObject(hdc, oldp);
        DeleteObject(linePen);

        // status badges
        SetBkMode(hdc, TRANSPARENT);
        HGDIOBJ oldf = SelectObject(hdc, hFontBold);
        int bx = 20;
        // DLL badge
        SetTextColor(hdc, g_dok ? CLR_OK : CLR_ERR);
        const char* d1 = g_dok ? "DLL  OK" : "DLL  --";
        TextOutA(hdc, bx, 22, d1, (int)strlen(d1));
        bx += 100;
        // PATTERN badge
        SetTextColor(hdc, g_pok ? CLR_OK : CLR_ERR);
        const char* d2 = g_pok ? "PATTERN  OK" : "PATTERN  --";
        TextOutA(hdc, bx, 22, d2, (int)strlen(d2));
        SelectObject(hdc, oldf);

        // version
        SetTextColor(hdc, CLR_MUTED);
        HGDIOBJ oldf2 = SelectObject(hdc, hFontSmall);
        const char* ver = "v3.0";
        int vw = GetTextExtentPoint32A(hdc, ver, (int)strlen(ver), nullptr) ? 30 : 30;
        TextOutA(hdc, rc.right - 50, 24, ver, (int)strlen(ver));
        SelectObject(hdc, oldf2);

        return 1;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hWnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    try {
        // create brushes BEFORE window class
        hbrBg     = CreateSolidBrush(CLR_BG);
        hbrPanel  = CreateSolidBrush(CLR_PANEL);
        hbrPanel2 = CreateSolidBrush(CLR_PANEL2);
        hbrHover  = CreateSolidBrush(CLR_HOVER);

        hFontDef   = CreateFontA(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
        hFontBold  = CreateFontA(16, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
        hFontSmall = CreateFontA(13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");

        INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_PROGRESS_CLASS };
        InitCommonControlsEx(&icc);

        WNDCLASSEXA wc{ sizeof(wc) };
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = hbrBg;
        wc.lpszClassName = "OSTGUI";
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        if (!RegisterClassExA(&wc)) {
            MessageBoxA(nullptr, "RegisterClassEx failed", "Error", MB_ICONERROR);
            return 1;
        }

        int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
        int ww = 660, wh = 580;
        int x = (sw - ww) / 2, y = (sh - wh) / 2;

        hMain = CreateWindowExA(0, "OSTGUI", "OpenSteamTool Installer",
            WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
            x, y, ww, wh, nullptr, nullptr, hInst, nullptr);

        if (!hMain) {
            MessageBoxA(nullptr, "CreateWindowEx failed", "Error", MB_ICONERROR);
            return 1;
        }

        // dark title bar
        BOOL dark = TRUE;
        DwmSetWindowAttribute(hMain, 20, &dark, sizeof(dark));

        ShowWindow(hMain, nShow);
        UpdateWindow(hMain);

        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return (int)msg.wParam;
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Exception", MB_ICONERROR);
        return 1;
    } catch (...) {
        MessageBoxA(nullptr, "Unknown error", "Error", MB_ICONERROR);
        return 1;
    }
}
