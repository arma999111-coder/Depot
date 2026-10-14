// core.cpp — steam + install logic implementation
#include "core.h"
#include "ui.h"

std::wstring widen(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, 0); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    if (!w.empty() && w.back() == 0) w.pop_back(); return w;
}

std::string lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}
bool contains_ci(const char* h, const char* n) {
    return lower(h).find(lower(n)) != std::string::npos;
}

// ── steam ──
std::string steam_path() {
    char buf[MAX_PATH] = {0}; DWORD sz = sizeof(buf);
    if (RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Valve\\Steam", "InstallPath", RRF_RT_REG_SZ, nullptr, buf, &sz) == ERROR_SUCCESS && buf[0]) return buf;
    sz = sizeof(buf);
    if (RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Valve\\Steam", "InstallPath", RRF_RT_REG_SZ, nullptr, buf, &sz) == ERROR_SUCCESS && buf[0]) return buf;
    sz = sizeof(buf);
    if (RegGetValueA(HKEY_CURRENT_USER, "SOFTWARE\\Valve\\Steam", "SteamPath", RRF_RT_REG_SZ, nullptr, buf, &sz) == ERROR_SUCCESS && buf[0]) {
        std::string p = buf; for (auto& c : p) if (c == '/') c = '\\'; return p;
    }
    return "C:\\Program Files (x86)\\Steam";
}

bool steam_running() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{ sizeof(pe) }; bool f = false;
    for (BOOL r = Process32FirstW(snap, &pe); r; r = Process32NextW(snap, &pe))
        if (_wcsicmp(pe.szExeFile, L"steam.exe") == 0) { f = true; break; }
    CloseHandle(snap); return f;
}

void kill_steam() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe{ sizeof(pe) };
    for (BOOL r = Process32FirstW(snap, &pe); r; r = Process32NextW(snap, &pe)) {
        if (_wcsicmp(pe.szExeFile, L"steam.exe") == 0) {
            HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID);
            if (h) { TerminateProcess(h, 0); WaitForSingleObject(h, 8000); CloseHandle(h); }
        }
    }
    CloseHandle(snap); Sleep(2000);
}

std::string tmp_path() {
    char buf[MAX_PATH]; GetTempPathA(sizeof(buf), buf); return buf;
}

// ── paths ──
bool init_paths(Paths& p) {
    p.steam = steam_path();
    if (!fs::exists(widen(p.steam))) return false;
    p.ost   = p.steam + "\\opensteamtool";
    p.ipc   = p.ost + "\\ipc";
    p.pat   = p.ost + "\\pattern";
    p.lua   = p.steam + "\\config\\Lua";
    p.stp   = p.steam + "\\config\\stplug-in";
    p.depot = p.steam + "\\depotcache";
    return true;
}

void ensure_dirs(const Paths& p) {
    std::error_code ec;
    fs::create_directories(widen(p.ipc), ec);
    fs::create_directories(widen(p.pat), ec);
    fs::create_directories(widen(p.lua), ec);
    fs::create_directories(widen(p.stp), ec);
    fs::create_directories(widen(p.depot), ec);
}

bool dlls_ok(const std::string& s) {
    return fs::exists(widen(s + "\\dwmapi.dll")) &&
           fs::exists(widen(s + "\\OpenSteamTool.dll")) &&
           fs::exists(widen(s + "\\xinput1_4.dll"));
}

bool pattern_ok(const std::string& p) {
    std::error_code ec;
    if (!fs::exists(widen(p))) return false;
    int n = 0;
    for (auto& e : fs::recursive_directory_iterator(widen(p), ec)) {
        if (ec) return false;
        // честные сигнатуры — только .toml (.proto из ветки protobuf не в счёт)
        if (e.is_regular_file() && e.path().extension() == ".toml" && ++n >= 5) return true;
    }
    return false;
}

// ── resources ──
bool drop_res(int id, const std::string& out) {
    HRSRC h = FindResourceA(nullptr, MAKEINTRESOURCEA(id), (LPCSTR)RT_RCDATA);
    if (!h) return false;
    DWORD sz = SizeofResource(nullptr, h);
    HGLOBAL g = LoadResource(nullptr, h);
    const void* p = LockResource(g);
    if (!p || !sz) return false;
    DeleteFileW(widen(out).c_str());
    HANDLE f = CreateFileW(widen(out).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD wr = 0; WriteFile(f, p, sz, &wr, nullptr); CloseHandle(f);
    return wr == sz;
}

int install_dlls(const std::string& steam) {
    int n = 0;
    n += drop_res(101, steam + "\\dwmapi.dll")        ? 1 : 0;
    n += drop_res(102, steam + "\\OpenSteamTool.dll")  ? 1 : 0;
    n += drop_res(103, steam + "\\xinput1_4.dll")      ? 1 : 0;
    return n;
}

// ── download ──
// тихий запуск без консольного окна (вместо system/_popen)
static DWORD run_hidden(const std::string& cmd, std::string* cap = nullptr) {
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE hR = nullptr, hW = nullptr;
    if (cap && !CreatePipe(&hR, &hW, &sa, 0)) return 1;
    if (hW) SetHandleInformation(hR, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | (cap ? STARTF_USESTDHANDLES : 0);
    si.wShowWindow = SW_HIDE;
    if (cap) { si.hStdOutput = hW; si.hStdError = hW; si.hStdInput = GetStdHandle(STD_INPUT_HANDLE); }
    PROCESS_INFORMATION pi{};
    std::vector<char> buf(cmd.begin(), cmd.end());
    buf.push_back(0);
    DWORD code = 1;
    if (CreateProcessA(nullptr, buf.data(), nullptr, nullptr, cap ? TRUE : FALSE,
            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        if (hW) { CloseHandle(hW); hW = nullptr; }
        if (cap) {
            char tmp[256];
            DWORD rd = 0;
            std::string all;
            for (;;) {
                BOOL ok = ReadFile(hR, tmp, sizeof(tmp) - 1, &rd, nullptr);
                if (!ok || rd == 0) break;
                tmp[rd] = 0;
                all += tmp;
                if (all.size() > 4096) break;
            }
            auto p = all.find('\n');
            if (p != std::string::npos) all.resize(p);
            *cap = all;
        }
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    if (hW) CloseHandle(hW);
    if (hR) CloseHandle(hR);
    return code;
}

bool dl(const std::string& url, const std::string& out) {
    std::string ps =
        "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
        "try { $h=@{Referer='https://walftech.com/generator.html';'User-Agent'='Mozilla/5.0'}; "
        "Invoke-WebRequest -Uri '" + url + "' -OutFile '" + out + "' -Headers $h -UseBasicParsing -TimeoutSec 120; "
        "Write-Output ('SIZE:' + (Get-Item '" + out + "').Length) } "
        "catch { Write-Output 'ERR'; exit 1 }\"";
    FILE* pipe = nullptr;
    (void)pipe;
    std::string r;
    run_hidden(ps, &r);
    if (r.find("ERR") != std::string::npos) { DeleteFileA(out.c_str()); return false; }
    return true;
}

int dl_pattern(const std::string& pat) {
    std::string zip = pat + "\\__r.zip", ext = pat + "\\__e";
    fs::create_directories(widen(pat));
    // ВАЖНО: сигнатуры .toml лежат в ветке pattern (в protobuf — только .proto-исходники)
    if (!dl("https://codeload.github.com/OpenSteam001/steam-monitor/zip/refs/heads/pattern", zip)) return 0;
    std::string ps =
        "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
        "Expand-Archive -LiteralPath '" + zip + "' -DestinationPath '" + ext + "' -Force; "
        "$r=(Get-ChildItem -LiteralPath '" + ext + "')[0].FullName; "
        "foreach($d in @('steamui','steamclient')){ $s=Join-Path $r $d; $t=Join-Path '" + pat + "' $d; "
        "if(Test-Path $s){ New-Item -ItemType Directory -Path $t -Force|Out-Null; "
        "Copy-Item (Join-Path $s '*') $t -Recurse -Force } }; "
        "Remove-Item '" + zip + "' -Force -EA 0; Remove-Item '" + ext + "' -Recurse -Force -EA 0\"";
    if (run_hidden(ps) != 0) return 0;
    // чистим мусор от старой ветки protobuf (.proto вместо сигнатур)
    std::error_code ec;
    for (auto& e : fs::recursive_directory_iterator(widen(pat), ec)) {
        if (ec) break;
        if (e.is_regular_file() && e.path().extension() == ".proto")
            fs::remove(e.path(), ec);
    }
    int n = 0;
    for (auto& sub : { pat + "\\steamui", pat + "\\steamclient" })
        for (auto& e : fs::recursive_directory_iterator(widen(sub), ec)) {
            if (ec) break;
            if (e.is_regular_file() && e.path().extension() == ".toml") n++;
        }
    return n;
}

// IPC-спеки лежат в отдельной ветке ipc (раньше не качались вообще)
int dl_ipc(const std::string& ipcDir) {
    std::string zip = ipcDir + "\\__r.zip", ext = ipcDir + "\\__e";
    fs::create_directories(widen(ipcDir));
    if (!dl("https://codeload.github.com/OpenSteam001/steam-monitor/zip/refs/heads/ipc", zip)) return 0;
    std::string ps =
        "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
        "Expand-Archive -LiteralPath '" + zip + "' -DestinationPath '" + ext + "' -Force; "
        "$r=(Get-ChildItem -LiteralPath '" + ext + "')[0].FullName; "
        "$s=Join-Path $r 'steamclient'; $t=Join-Path '" + ipcDir + "' 'steamclient'; "
        "if(Test-Path $s){ New-Item -ItemType Directory -Path $t -Force|Out-Null; "
        "Copy-Item (Join-Path $s '*') $t -Recurse -Force }; "
        "Remove-Item '" + zip + "' -Force -EA 0; Remove-Item '" + ext + "' -Recurse -Force -EA 0\"";
    if (run_hidden(ps) != 0) return 0;
    int n = 0; std::error_code ec;
    for (auto& e : fs::recursive_directory_iterator(widen(ipcDir), ec)) {
        if (ec) break;
        if (e.is_regular_file() && e.path().extension() == ".toml") n++;
    }
    return n;
}

// ── install game ──
bool install_game(const std::string& appid, const Paths& p) {
    std::string dir = tmp_path() + "\\ost_" + appid;
    std::string zip = dir + "\\" + appid + ".zip";
    std::error_code ec;
    fs::create_directories(widen(dir), ec);
    std::string url = "https://walftech.com/proxy.php?url=https://steamgames554.s3.us-east-1.amazonaws.com/" + appid + ".zip";
    if (!dl(url, zip)) { fs::remove_all(widen(dir), ec); return false; }
    std::string sc = dir + "\\i.ps1";
    FILE* f = fopen(sc.c_str(), "w");
    if (!f) { fs::remove_all(widen(dir), ec); return false; }
    fprintf(f,
        "$ErrorActionPreference='SilentlyContinue'\n"
        "Expand-Archive -LiteralPath '%s' -DestinationPath '%s' -Force\n"
        "Get-ChildItem -LiteralPath '%s' -Recurse -Filter '*.lua' | %%{ "
        "  Copy-Item $_.FullName (Join-Path '%s' $_.Name) -Force; "
        "  Copy-Item $_.FullName (Join-Path '%s' $_.Name) -Force }\n"
        "Get-ChildItem -LiteralPath '%s' -Recurse -Filter '*.manifest' | %%{ "
        "  Copy-Item $_.FullName (Join-Path '%s' $_.Name) -Force }\n",
        zip.c_str(), dir.c_str(), dir.c_str(),
        p.lua.c_str(), p.stp.c_str(),
        dir.c_str(), p.depot.c_str());
    fclose(f);
    run_hidden("powershell -NoProfile -ExecutionPolicy Bypass -File \"" + sc + "\"");
    int nl = 0, nm = 0;
    for (auto& e : fs::directory_iterator(widen(dir), ec)) {
        if (ec) break;
        if (e.path().extension() == ".lua") nl++;
        if (e.path().extension() == ".manifest") nm++;
    }
    ok("lua: %d, manifest: %d", nl, nm);
    fs::remove_all(widen(dir), ec);
    return (nl + nm) > 0;
}
