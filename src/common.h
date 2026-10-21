// common.h — shared types, colors, paths
#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <string>
#include <vector>
#include <set>
#include <filesystem>

namespace fs = std::filesystem;

// language
inline int g_lang = 0;
inline const char* tr(const char* ru, const char* en) { return g_lang ? en : ru; }

// colors
#define C_RST  "\x1b[0m"
#define C_BLD  "\x1b[1m"
#define C_DIM  "\x1b[2m"
#define C_BLU  "\x1b[38;2;0;140;255m"
#define C_CYN  "\x1b[38;2;0;230;200m"
#define C_WHT  "\x1b[38;2;235;235;240m"
#define C_GRY  "\x1b[38;2;105;105;115m"
#define C_DGRY "\x1b[38;2;55;55;65m"
#define C_GRN  "\x1b[38;2;0;210;90m"
#define C_YEL  "\x1b[38;2;255;195;50m"
#define C_RED  "\x1b[38;2;255;70;70m"
#define C_PRP  "\x1b[38;2;165;95;255m"

#define W 52

// версия приложения (для проверки обновлений на GitHub)
#define DEPOT_VER "6.9"
#define DEPOT_VER_TAG "v6.9"

// steam paths
struct Paths {
    std::string steam, ost, ipc, pat, lua, stp, depot;
};
