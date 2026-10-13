// ui.h — UI helpers
#pragma once
#include "common.h"

void ui_init();
void print(const char* fmt, ...);

// boxes
void bx_top(const char* title);
void bx_bot();
void bx_line(const char* text);
void bx_empty();

// messages
void ok(const char* fmt, ...);
void warn(const char* fmt, ...);
void err(const char* fmt, ...);
void info(const char* fmt, ...);

// widgets
void banner();
void bar(const char* label, int pct);
void ask(const char* prompt);
int  read_key();
int  read_line(char* buf, int sz);

// menu
void menu_item(int num, bool hot, const char* label, const char* desc);
void menu_gap();
void pause_back();
