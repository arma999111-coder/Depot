// core.h — steam + install logic
#pragma once
#include "common.h"

std::wstring widen(const std::string& s);
std::string lower(std::string s);
bool contains_ci(const char* h, const char* n);

std::string steam_path();
bool steam_running();
void kill_steam();
std::string tmp_path();

bool init_paths(Paths& p);
void ensure_dirs(const Paths& p);
bool dlls_ok(const std::string& steam);
bool pattern_ok(const std::string& pat);

bool drop_res(int id, const std::string& out);
bool dl(const std::string& url, const std::string& out);
int  dl_pattern(const std::string& pat);
int  dl_ipc(const std::string& ipcDir);
int  install_bst(const std::string& steam);
int  dll_variant(const std::string& steam); // 0 нет, 1 частично, 2 BetterSteamTools
void scan_installed(const std::string& steam, std::set<std::string>& out);
bool install_game(const std::string& appid, const Paths& p);
