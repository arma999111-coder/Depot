// ui.cpp — UI helpers implementation
#include "ui.h"
#include <conio.h>

static HANDLE hCon = nullptr;

void ui_init() {
    hCon = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD m = 0; GetConsoleMode(hCon, &m);
    SetConsoleMode(hCon, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleTitleA("OpenSteamTool Installer");
    CONSOLE_CURSOR_INFO ci{1, FALSE}; SetConsoleCursorInfo(hCon, &ci);
}

void print(const char* fmt, ...) {
    char b[2048]; va_list a; va_start(a, fmt);
    vsnprintf(b, sizeof(b), fmt, a); va_end(a);
    printf("%s", b); fflush(stdout);
}

// ── boxes ──
void bx_top(const char* title) {
    print("\n");
    print(C_DGRY "  \xe2\x94\x8c");
    int tlen = title ? (int)strlen(title) : 0;
    int pad = (W - tlen - 2) / 2;
    for (int i = 0; i < pad; i++) print("\xe2\x94\x80");
    if (title) {
        print(" " C_BLD C_WHT "%s" C_RST C_DGRY " ", title);
        int rest = W - tlen - 2 - pad - 2;
        for (int i = 0; i < rest; i++) print("\xe2\x94\x80");
    } else {
        for (int i = 0; i < W - 2; i++) print("\xe2\x94\x80");
    }
    print("\xe2\x94\x90" C_RST "\n");
}
void bx_bot() {
    print(C_DGRY "  \xe2\x94\x94");
    for (int i = 0; i < W; i++) print("\xe2\x94\x80");
    print("\xe2\x94\x98" C_RST "\n");
}
void bx_line(const char* text) {
    int len = text ? (int)strlen(text) : 0;
    print(C_DGRY "  \xe2\x94\x82" C_RST " %s", text ? text : "");
    for (int i = len + 1; i < W; i++) print(" ");
    print(C_DGRY "\xe2\x94\x82" C_RST "\n");
}
void bx_empty() { bx_line(""); }

// ── messages ──
static void msg(const char* icon, const char* color, const char* text) {
    print(C_DGRY "  \xe2\x94\x82" C_RST "  %s%s" C_RST " %s\n", color, icon, text);
}
void ok(const char* fmt, ...) {
    char b[1024]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof(b), fmt, a); va_end(a);
    msg("\xe2\x9c\x93", C_GRN, b);
}
void warn(const char* fmt, ...) {
    char b[1024]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof(b), fmt, a); va_end(a);
    msg("!", C_YEL, b);
}
void err(const char* fmt, ...) {
    char b[1024]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof(b), fmt, a); va_end(a);
    msg("\xe2\x9c\x97", C_RED, b);
}
void info(const char* fmt, ...) {
    char b[1024]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof(b), fmt, a); va_end(a);
    print(C_DGRY "  \xe2\x94\x82" C_RST "  " C_GRY "%s" C_RST "\n", b);
}

// ── banner ──
void banner() {
    print("\n");
    print(C_BLU "\n");
    print("         ____  _____ _____    _    __  __ \n");
    print("        / ___|_   _| ____|  / \\  |  \\/  |\n");
    print("        \\___ \\ | | |  _|   / _ \\ | |\\/| |\n");
    print("         ___) || | | |___ / ___ \\| |  | |\n");
    print("        |____/ |_| |_____/_/   \\_\\_|  |_|\n");
    print("\n");
    print("      " C_GRY "  opensteamtool installer  " C_CYN "v3.0" C_RST "\n");
    print("\n");
}

// ── progress ──
void bar(const char* label, int pct) {
    const int BW = 26;
    if (pct > 100) pct = 100; if (pct < 0) pct = 0;
    int fill = pct * BW / 100;
    print(C_DGRY "  \xe2\x94\x82" C_RST "  " C_CYN "%-9s" C_RST " " C_DGRY "\xe2\x94\x83" C_RST C_BLU, label);
    for (int i = 0; i < fill; i++) print("\xe2\x96\x88");
    print(C_DGRY);
    for (int i = fill; i < BW; i++) print("\xe2\x96\x91");
    print("\xe2\x94\x83" C_RST " " C_BLD "%3d%%" C_RST "\n", pct);
}

// ── input ──
void ask(const char* prompt) {
    print(C_DGRY "  \xe2\x94\x82" C_RST "  " C_CYN C_BLD "%s" C_RST " " C_WHT "\xe2\x96\xb6" C_RST " ", prompt);
    fflush(stdout);
}
int read_key() {
    int c = _getch();
    if (c >= '0' && c <= '9') { print("%c", c); return c - '0'; }
    return -1;
}
int read_line(char* buf, int sz) {
    if (!fgets(buf, sz, stdin)) { buf[0] = 0; return 0; }
    buf[strcspn(buf, "\r\n")] = 0;
    return 1;
}

// ── menu ──
void menu_item(int num, bool hot, const char* label, const char* desc) {
    print(C_DGRY "  \xe2\x94\x82" C_RST "  ");
    if (hot) print(C_CYN C_BLD "[%d]" C_RST "  " C_CYN "%-24s" C_RST " " C_GRY "%s" C_RST, num, label, desc);
    else     print(C_BLU C_BLD "[%d]" C_RST "  " C_BLD "%-24s" C_RST " " C_GRY "%s" C_RST, num, label, desc);
    int vis = 2 + 5 + 24 + 1 + (int)strlen(desc);
    for (int i = vis; i < W; i++) print(" ");
    print(C_DGRY "\xe2\x94\x82" C_RST "\n");
}
void menu_gap() {
    print(C_DGRY "  \xe2\x94\x82" C_RST "\n");
}
void pause_back() {
    print("\n");
    ask(tr("Enter — назад", "Enter — back"));
    while (true) { int k = _getch(); if (k == 13 || k == 10) break; }
    print("\n");
}
