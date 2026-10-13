// gui_imgui.cpp — Dear ImGui + Win32 + DirectX 11
#include "common.h"
#include "core.h"
#include <windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "games.h"
#include <gdiplus.h>
#include <wininet.h>
#include <cmath>
#include <map>
#include <set>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "wininet.lib")

static ID3D11Device*           g_pd3dDevice = nullptr;
static ID3D11DeviceContext*    g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*         g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static bool                    g_SwapChainOccluded = false;
static UINT                    g_ResizeWidth = 0, g_ResizeHeight = 0;

// ── app state ──
static std::atomic<bool> g_busy{false};
static int g_lang_gui = 0;
static const char* tg(const char* ru, const char* en) { return g_lang_gui ? en : ru; }

static Paths g_paths;
static bool g_has = false, g_dok = false, g_pok = false;

static std::vector<std::string> g_log;
static std::mutex g_logMutex;
static std::string g_status = "Ready";
static std::mutex g_statusMutex;
static int g_progress = 0;
static std::atomic<int> g_progressAtomic{0};
static std::atomic<bool> g_refreshFlag{false};

static char g_appidBuf[32] = "";
static bool g_autoScroll = true;

// ── tabs + catalog state ──
static int g_tab = 0; // 0 = главная (новости), 1 = игры, 2 = настройки
static float g_selX = 0, g_selY0 = 0, g_selY1 = 0, g_indY0 = 0, g_indH0 = 0;
static bool g_indInit = false;
static char g_searchBuf[64] = "";
static int g_catSel = 0; // индекс в kCats

// ── новости обновлений ──
struct NewsItem {
    const char* ver;
    const char* date;
    const char* ru[6];
    const char* en[6];
    int n;
};

static const NewsItem kNews[] = {
    { "v4.9", "13.10.2026",
      { "Каталог вырос до 235 игр — плюс 25 по запросу",
        "Новинки: MiSide, Detroit, Batman Arkham Knight, Borderlands, Dying Light",
        "Mafia II обе части, GTA IV, Tomb Raider, Metro 2033, Portal",
        "AppID сверены со Steam — установка и обложки работают" },
      { "Catalog grew to 235 games — 25 more on request",
        "New: MiSide, Detroit, Batman Arkham Knight, Borderlands, Dying Light",
        "Both Mafia II, GTA IV, Tomb Raider, Metro 2033, Portal",
        "AppIDs verified against Steam — installs and covers work" },
      4 },
    { "v4.8", "13.10.2026",
      { "Закруглённый hero-баннер и ровная геометрия везде",
        "Режим 240 FPS: частицы и анимации без VSync",
        "Акцентный логотип, ничего не обрезано",
        "Плавные анимации на smoothstep" },
      { "Rounded hero banner and even geometry everywhere",
        "240 FPS mode: particles and animations without VSync",
        "Accent logo, nothing clipped",
        "Smoothstep-eased animations" },
      4 },
    { "v4.7", "13.10.2026",
      { "Полный реворк вкладок: ровные цвета и геометрия",
        "Зум и градиенты на обложках, шиммер загрузки",
        "Герой с бликом, пульс свежей новости, глянец кнопок",
        "Панель ID, переключатель частиц, всё в цвете темы" },
      { "Full tab rework: even colors and geometry",
        "Cover zoom and gradients, loading shimmer",
        "Hero with shine, fresh-news pulse, button gloss",
        "ID panel, particles toggle, all in theme color" },
      4 },
    { "v4.6", "13.10.2026",
      { "Обложки качаются в 6 потоков — каталог готов за секунды",
        "Ровная геометрия: отступы и ничего не обрезано",
        "Герой-баннер и сетка без визуальных косяков" },
      { "Covers download in 6 threads — catalog ready in seconds",
        "Neat geometry: even padding, nothing clipped",
        "Hero banner and grid without visual flaws" },
      3 },
    { "v4.5", "13.10.2026",
      { "Каталог вырос до 210 игр — плюс 100 хитов",
        "Новинки: Space Marine 2, NieR: Automata, Disco Elysium, Stray, Cuphead",
        "Герой-баннер и каскадные анимации новостей",
        "Зум обложек, скользящий индикатор, живые точки статусов",
        "Прогресс переехал в сайдбар, нижняя полоса убрана" },
      { "Catalog grew to 210 games — 100 more hits",
        "New: Space Marine 2, NieR: Automata, Disco Elysium, Stray, Cuphead",
        "Hero banner and cascading news animations",
        "Cover zoom, sliding indicator, living status dots",
        "Progress moved to sidebar, bottom strip removed" },
      5 },
    { "v4.4", "13.10.2026",
      { "Свой цвет интерфейса: палитра + любые оттенки, сохраняется",
        "Плавная прокрутка каталога и новостей",
        "Прогресс тонкой линией, статус переехал в сайдбар",
        "Живой фон с частицами, скрытые скроллбары" },
      { "Custom UI color: palette + any shade, saved",
        "Smooth scrolling for catalog and news",
        "Hairline progress, status moved to sidebar",
        "Living particle background, hidden scrollbars" },
      4 },
    { "v4.3", "13.10.2026",
      { "Боковая панель навигации вместо огромных кнопок",
        "Настройки с нуля: карточки действий с описаниями",
        "Кнопка обновления статуса и широкий layout",
        "Плавные анимации и неоновая подсветка везде" },
      { "Sidebar navigation instead of huge buttons",
        "Settings from scratch: action cards with descriptions",
        "Status refresh button and wide layout",
        "Smooth animations and neon highlights everywhere" },
      4 },
    { "v4.2", "13.10.2026",
      { "Вкладка «Настройки» — установка, журнал и AppID переехали туда",
        "Главная стала лентой новостей обновлений",
        "Каталог: 110 платных игр, сетка 4 в ряд",
        "Анимации: бегущий неон, спиннер, плавные вкладки",
        "Новинки: God of War Ragnarök, Ghost of Tsushima, KCD II, Silksong" },
      { "New «Settings» tab — installer, log and AppID moved there",
        "Home is now an update news feed",
        "Catalog: 110 paid games, 4-column grid",
        "Animations: running neon, spinner, smooth tabs",
        "New: God of War Ragnarök, Ghost of Tsushima, KCD II, Silksong" },
      5 },
    { "v4.1", "12.10.2026",
      { "Каталог игр с обложками прямо из Steam",
        "Категории, поиск и счётчик игр",
        "Тихая загрузка обложек — без всплывающих консолей",
        "Чёрная тема, цветные кнопки, моноширинный журнал" },
      { "Game catalog with covers straight from Steam",
        "Categories, search and game counter",
        "Silent cover downloads — no popup consoles",
        "Black theme, colored buttons, monospace log" },
      4 },
    { "v4.0", "11.10.2026",
      { "Новый интерфейс на Dear ImGui + DirectX 11",
        "Русский язык и кириллический шрифт",
        "Установка в фоне — окно не зависает" },
      { "New UI on Dear ImGui + DirectX 11",
        "Russian language and Cyrillic font",
        "Background installs — window never freezes" },
      3 },
    { "v3.0", "05.10.2026",
      { "Первая версия: DLL, паттерн и установка игр",
        "Автоопределение пути Steam из реестра" },
      { "First release: DLLs, pattern and game installs",
        "Steam path auto-detection from registry" },
      2 },
};
static constexpr int kNewsCount = (int)(sizeof(kNews) / sizeof(kNews[0]));

// ── covers (Steam CDN header.jpg) ──
static std::map<std::string, ID3D11ShaderResourceView*> g_covers;
static std::set<std::string> g_coverFail;
static std::string g_hoverCard; // карточка под курсором (для зума обложки)

static std::string cover_file(const std::string& appid) {
    return tmp_path() + "/ost_covers/" + appid + "_cap.jpg";
}

// тихо качает файл без всплывающих консолей (WinINet); hI — сессия worker-потока
static bool dl_quiet_h(HINTERNET hI, const std::string& url, const std::string& out) {
    bool ok = false;
    HINTERNET hU = InternetOpenUrlA(hI, url.c_str(), nullptr, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (hU) {
        FILE* f = nullptr;
        if (_wfopen_s(&f, widen(out).c_str(), L"wb") == 0 && f) {
            char buf[16384];
            DWORD rd = 0;
            size_t total = 0;
            BOOL r = TRUE;
            ok = true;
            while ((r = InternetReadFile(hU, buf, sizeof(buf), &rd)) && rd) {
                if (fwrite(buf, 1, rd, f) != rd) { ok = false; break; }
                total += rd;
            }
            if (!r) ok = false;
            fclose(f);
            if (!ok || total < 2048) {
                DeleteFileW(widen(out).c_str());
                ok = false;
            }
        }
        InternetCloseHandle(hU);
    }
    return ok;
}

static void ensure_covers_thread() {
    static bool started = false;
    if (started) return;
    started = true;
    std::thread([] {
        std::string dir = tmp_path() + "/ost_covers";
        std::error_code ec;
        fs::create_directories(widen(dir), ec);
        // чистим кэш удалённых из каталога фритуплей-игр
        for (const char* dead : { "730", "570", "440", "578080", "1172470", "230410", "1085660", "2357570" }) {
            DeleteFileW(widen(dir + "/" + dead + "_cap.jpg").c_str());
            DeleteFileW(widen(dir + "/" + dead + ".jpg").c_str());
        }
        // пул из 6 worker'ов — качаем параллельно, в разы быстрее
        std::atomic<int> next{0};
        auto worker = [&] {
            HINTERNET hI = InternetOpenA("OpenSteamTool/4.6", INTERNET_OPEN_TYPE_PRECONFIG,
                nullptr, nullptr, 0);
            if (!hI) return;
            DWORD to = 15000;
            InternetSetOptionA(hI, INTERNET_OPTION_CONNECT_TIMEOUT, &to, sizeof(to));
            InternetSetOptionA(hI, INTERNET_OPTION_RECEIVE_TIMEOUT, &to, sizeof(to));
            for (;;) {
                int i = next.fetch_add(1);
                if (i >= kGamesCount) break;
                std::string appid = kGames[i].appid;
                std::string out = cover_file(appid);
                // чистим старый мелкий кэш header.jpg
                DeleteFileW(widen(dir + "/" + appid + ".jpg").c_str());
                std::error_code ec2;
                auto sz = fs::file_size(widen(out), ec2);
                if (!ec2 && sz > 2048) continue; // уже скачана
                std::string url = std::string("https://cdn.cloudflare.steamstatic.com/steam/apps/")
                    + appid + "/capsule_616x353.jpg";
                std::string tmp = out + ".part";
                if (dl_quiet_h(hI, url, tmp))
                    MoveFileW(widen(tmp).c_str(), widen(out).c_str());
            }
            InternetCloseHandle(hI);
        };
        const int N = 6;
        std::vector<std::thread> pool;
        for (int k = 0; k < N; k++) pool.emplace_back(worker);
        for (auto& t : pool) t.join();
    }).detach();
}

static ID3D11ShaderResourceView* load_texture_gdi(const wchar_t* wpath) {
    Gdiplus::Bitmap bmp(wpath);
    if (bmp.GetLastStatus() != Gdiplus::Ok) return nullptr;
    UINT w = bmp.GetWidth(), h = bmp.GetHeight();
    if (!w || !h || w > 2048 || h > 2048) return nullptr;
    Gdiplus::Rect rc(0, 0, w, h);
    Gdiplus::BitmapData d;
    if (bmp.LockBits(&rc, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &d) != Gdiplus::Ok)
        return nullptr;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = w; desc.Height = h;
    desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{ d.Scan0, (UINT)d.Stride, 0 };
    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = g_pd3dDevice->CreateTexture2D(&desc, &init, &tex);
    bmp.UnlockBits(&d);
    if (FAILED(hr) || !tex) return nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    g_pd3dDevice->CreateShaderResourceView(tex, nullptr, &srv);
    tex->Release();
    return srv;
}

// подгружаем максимум N текстур за кадр, чтобы не фризить
// файлы появляются только целиком (поток качает в .part + rename), так что частичного чтения нет
static void pump_covers(int budget = 3) {
    for (int i = 0; i < kGamesCount && budget > 0; i++) {
        std::string appid = kGames[i].appid;
        if (g_covers.count(appid) || g_coverFail.count(appid)) continue;
        std::string p = cover_file(appid);
        std::error_code ec;
        auto sz = fs::file_size(widen(p), ec);
        if (ec || sz < 2048) continue; // ещё не скачался
        ID3D11ShaderResourceView* srv = load_texture_gdi(widen(p).c_str());
        if (srv) g_covers[appid] = srv;
        else g_coverFail.insert(appid);
        budget--;
    }
}

// обрезать строку с "..." чтобы влезла в одну строку шириной maxW
static std::string trunc1(const char* s, float maxW) {
    if (ImGui::CalcTextSize(s).x <= maxW) return s;
    std::string r = s;
    while (r.size() > 4) {
        r.pop_back();
        if (ImGui::CalcTextSize((r + "...").c_str()).x <= maxW) break;
    }
    return r + "...";
}

// название категории по ключу (для бейджа на обложке)
static const char* cat_name(const char* key) {
    for (int i = 0; i < kCatsCount; i++)
        if (!strcmp(kCats[i].key, key)) return tg(kCats[i].ru, kCats[i].en);
    return key;
}

// ── тема: свой акцентный цвет (сохраняется) ──
static ImVec4 g_accent = ImVec4(0.12f, 0.42f, 0.95f, 1.0f);
static const ImVec4 kAccentDef = ImVec4(0.12f, 0.42f, 0.95f, 1.0f);
static bool g_particles = true;
static bool g_fps240 = true;

static void load_theme() {
    std::string p = tmp_path() + "/ost_theme.ini";
    FILE* f = nullptr;
    if (_wfopen_s(&f, widen(p).c_str(), L"r") == 0 && f) {
        float r, g, b;
        if (fscanf(f, "%f %f %f", &r, &g, &b) == 3)
            g_accent = ImVec4(r, g, b, 1.0f);
        fclose(f);
    }
}
static void save_theme() {
    std::string p = tmp_path() + "/ost_theme.ini";
    FILE* f = nullptr;
    if (_wfopen_s(&f, widen(p).c_str(), L"w") == 0 && f) {
        fprintf(f, "%.3f %.3f %.3f\n", g_accent.x, g_accent.y, g_accent.z);
        fclose(f);
    }
}

// красивая кнопка с акцентным цветом
static bool accent_btn(const char* label, const ImVec2& size, const ImVec4& base) {
    ImVec4 hov(base.x + 0.07f, base.y + 0.10f, base.z + 0.10f, 1.0f);
    ImVec4 act(base.x - 0.03f, base.y - 0.05f, base.z - 0.05f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, base);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, act);
    bool r = ImGui::Button(label, size);
    if (ImGui::IsItemHovered()) {
        ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddLine(ImVec2(mn.x + 8, mn.y + 1), ImVec2(mx.x - 8, mn.y + 1),
            IM_COL32(255, 255, 255, 80), 2.0f);
    }
    ImGui::PopStyleColor(3);
    return r;
}

// маленький анимированный спиннер (дуга, крутится по времени)
static void spinner(float r, float th, ImU32 col) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(r * 2, r * 2));
    float t = (float)ImGui::GetTime();
    float a0 = t * 5.0f, a1 = a0 + 4.4f;
    ImVec2 c(p.x + r, p.y + r);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PathArcTo(c, r - th * 0.5f, a0, a1, 20);
    dl->PathStroke(col, 0, th);
}

// плавная прокрутка: колёсико задаёт цель, ScrollY плавно догоняет её
static std::map<std::string, float> g_scrollTgt;
static void smooth_child(const char* id) {
    float maxY = ImGui::GetScrollMaxY();
    float& tgt = g_scrollTgt[id];
    if (tgt > maxY) tgt = maxY;
    if (tgt < 0) tgt = 0;
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
        tgt -= ImGui::GetIO().MouseWheel * 70.0f;
    if (tgt > maxY) tgt = maxY;
    if (tgt < 0) tgt = 0;
    float cur = ImGui::GetScrollY();
    float k = 1.0f - expf(-ImGui::GetIO().DeltaTime * 12.0f);
    float ny = cur + (tgt - cur) * k;
    if (fabsf(ny - tgt) < 0.5f) ny = tgt;
    ImGui::SetScrollY(ny);
}

static void log_add(const std::string& s) {
    std::lock_guard<std::mutex> lk(g_logMutex);
    g_log.push_back(s);
    if (g_log.size() > 500) g_log.erase(g_log.begin());
}
static void set_status_s(const std::string& s) {
    std::lock_guard<std::mutex> lk(g_statusMutex);
    g_status = s;
}
static void set_progress_s(int p) {
    g_progressAtomic = p;
}

static void refresh_status() {
    g_has = init_paths(g_paths);
    g_dok = g_has && dlls_ok(g_paths.steam);
    g_pok = g_has && pattern_ok(g_paths.pat);
}

static void run_async(std::function<void()> fn) {
    if (g_busy.exchange(true)) return;
    std::thread([fn] {
        fn();
        g_busy = false;
        refresh_status();
    }).detach();
}

// ── actions ──
static void do_full_install() {
    run_async([] {
        set_status_s(tg("Установка...", "Installing..."));
        log_add(tg("=== Полная установка ===", "=== Full install ==="));
        if (steam_running()) { log_add(tg("закрываю steam...", "closing steam...")); kill_steam(); }
        if (!g_has) { log_add(tg("steam не найден!", "steam not found!")); return; }
        if (!g_dok) {
            log_add(tg("установка dll...", "installing dll..."));
            int n = install_dlls(g_paths.steam);
            char b[64]; snprintf(b, sizeof(b), "dll: %d/3", n); log_add(b);
        } else log_add(tg("dll уже установлены", "dll already installed"));
        set_progress_s(30);
        ensure_dirs(g_paths);
        if (!g_pok) {
            log_add(tg("загрузка pattern (github)...", "downloading pattern (github)..."));
            int n = dl_pattern(g_paths.pat);
            char b[64]; snprintf(b, sizeof(b), "%s: %d", tg("файлов", "files"), n); log_add(b);
        } else log_add(tg("pattern уже установлен", "pattern already installed"));
        set_progress_s(60);
        log_add(tg("Готово! Введите AppID и нажмите «Добавить».", "Done! Enter AppID and click Add."));
        set_progress_s(100);
        set_status_s(tg("Готово — введите AppID", "Done — enter AppID"));
    });
}

static void do_games_only() {
    run_async([] {
        set_status_s(tg("Подготовка...", "Preparing..."));
        if (steam_running()) kill_steam();
        if (!g_has) { log_add(tg("steam не найден!", "steam not found!")); return; }
        ensure_dirs(g_paths);
        if (!g_pok) {
            log_add(tg("загрузка pattern...", "downloading pattern..."));
            int n = dl_pattern(g_paths.pat);
            char b[64]; snprintf(b, sizeof(b), "%s: %d", tg("файлов", "files"), n); log_add(b);
        }
        log_add(tg("Готово! Введите AppID.", "Done! Enter AppID."));
        set_status_s(tg("Введите AppID", "Enter AppID"));
    });
}

static void do_update_pattern() {
    run_async([] {
        set_status_s(tg("Обновление pattern...", "Updating pattern..."));
        if (!g_has) { log_add(tg("steam не найден!", "steam not found!")); return; }
        ensure_dirs(g_paths);
        log_add(tg("загрузка pattern (github)...", "downloading pattern (github)..."));
        int n = dl_pattern(g_paths.pat);
        char b[64]; snprintf(b, sizeof(b), "%s: %d", tg("файлов", "files"), n); log_add(b);
        set_progress_s(100);
        set_status_s(tg("Pattern обновлён", "Pattern updated"));
    });
}

static void do_remove() {
    run_async([] {
        set_status_s(tg("Удаление...", "Removing..."));
        if (!g_has) { log_add(tg("steam не найден!", "steam not found!")); return; }
        if (steam_running()) kill_steam();
        int n = 0;
        for (auto& f : { "\\dwmapi.dll", "\\OpenSteamTool.dll", "\\xinput1_4.dll" }) {
            std::string fp = g_paths.steam + f;
            if (fs::exists(widen(fp))) { DeleteFileW(widen(fp).c_str()); n++; }
        }
        char b[64]; snprintf(b, sizeof(b), "dll удалено: %d/3", n); log_add(b);
        std::error_code ec;
        if (fs::exists(widen(g_paths.ost))) {
            int nf = 0;
            for (auto& e : fs::recursive_directory_iterator(widen(g_paths.ost), ec))
                if (!ec && e.is_regular_file()) nf++;
            fs::remove_all(widen(g_paths.ost), ec);
            snprintf(b, sizeof(b), "файлов удалено: %d", nf); log_add(b);
        }
        set_progress_s(100);
        set_status_s(tg("Удалено", "Removed"));
    });
}

static void do_add_game(const std::string& appid) {
    if (appid.empty()) return;
    run_async([appid] {
        set_status_s("...");
        char b[128];
        snprintf(b, sizeof(b), tg("загрузка appid=%s ...", "downloading appid=%s ..."), appid.c_str());
        log_add(b);
        set_progress_s(20);
        ensure_dirs(g_paths);
        if (install_game(appid, g_paths)) {
            log_add(tg("установлено!", "installed!"));
            set_status_s(tg("Игра установлена", "Game installed"));
        } else {
            log_add(tg("ошибка скачивания", "download error"));
            set_status_s(tg("Ошибка скачивания", "Download error"));
        }
        set_progress_s(100);
    });
}

// ── DX11 ──
static void CreateRenderTarget();
static void CleanupRenderTarget();

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        featureLevels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
            featureLevels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK) return false;
    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

static void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ── main ──
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    // GDI+ для загрузки обложек игр (jpg -> DX11 текстура)
    Gdiplus::GdiplusStartupInput gsi;
    ULONG_PTR gdiTok = 0;
    Gdiplus::GdiplusStartup(&gdiTok, &gsi, nullptr);

    WNDCLASSEXA wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInst, nullptr, nullptr, nullptr, nullptr, "OSTImGui", nullptr };
    ::RegisterClassExA(&wc);
    HWND hwnd = ::CreateWindowA(wc.lpszClassName, "OpenSteamTool Installer",
        WS_OVERLAPPEDWINDOW, 100, 100, 980, 720, nullptr, nullptr, wc.hInstance, nullptr);

    // dark title bar
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, nShow);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12.0f;
    style.FrameRounding = 8.0f;
    style.GrabRounding = 6.0f;
    style.ScrollbarRounding = 8.0f;
    style.ChildRounding = 10.0f;
    style.WindowPadding = ImVec2(20, 16);
    style.FramePadding = ImVec2(12, 9);
    style.ItemSpacing = ImVec2(10, 8);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.ScrollbarSize = 12.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.SeparatorTextBorderSize = 1.0f;

    ImVec4* c = style.Colors;
    // pure black premium theme
    c[ImGuiCol_WindowBg]        = ImVec4(0.016f, 0.016f, 0.022f, 1.00f); // #040405
    c[ImGuiCol_ChildBg]         = ImVec4(0.035f, 0.035f, 0.050f, 1.00f); // #09090d
    c[ImGuiCol_PopupBg]         = ImVec4(0.030f, 0.030f, 0.045f, 0.98f);
    c[ImGuiCol_Border]          = ImVec4(0.10f, 0.10f, 0.16f, 0.90f);
    c[ImGuiCol_BorderShadow]    = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
    c[ImGuiCol_FrameBg]         = ImVec4(0.060f, 0.060f, 0.085f, 1.00f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.085f, 0.085f, 0.120f, 1.00f);
    c[ImGuiCol_FrameBgActive]   = ImVec4(0.105f, 0.105f, 0.150f, 1.00f);
    c[ImGuiCol_TitleBg]         = ImVec4(0.016f, 0.016f, 0.022f, 1.00f);
    c[ImGuiCol_TitleBgActive]   = ImVec4(0.016f, 0.016f, 0.022f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]= ImVec4(0.016f, 0.016f, 0.022f, 1.00f);
    c[ImGuiCol_MenuBarBg]       = ImVec4(0.020f, 0.020f, 0.030f, 1.00f);
    c[ImGuiCol_ScrollbarBg]     = ImVec4(0.016f, 0.016f, 0.022f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]   = ImVec4(0.14f, 0.14f, 0.20f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.20f, 0.20f, 0.28f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.26f, 0.26f, 0.36f, 1.00f);
    c[ImGuiCol_CheckMark]       = ImVec4(0.20f, 0.90f, 0.50f, 1.00f);
    c[ImGuiCol_SliderGrab]      = ImVec4(0.20f, 0.55f, 1.00f, 1.00f);
    c[ImGuiCol_SliderGrabActive]= ImVec4(0.30f, 0.70f, 1.00f, 1.00f);
    c[ImGuiCol_Button]          = ImVec4(0.070f, 0.070f, 0.105f, 1.00f);
    c[ImGuiCol_ButtonHovered]   = ImVec4(0.110f, 0.110f, 0.165f, 1.00f);
    c[ImGuiCol_ButtonActive]    = ImVec4(0.090f, 0.090f, 0.140f, 1.00f);
    c[ImGuiCol_Header]          = ImVec4(0.070f, 0.070f, 0.105f, 1.00f);
    c[ImGuiCol_HeaderHovered]   = ImVec4(0.110f, 0.110f, 0.165f, 1.00f);
    c[ImGuiCol_HeaderActive]    = ImVec4(0.090f, 0.090f, 0.140f, 1.00f);
    c[ImGuiCol_Separator]       = ImVec4(0.10f, 0.10f, 0.16f, 0.70f);
    c[ImGuiCol_SeparatorHovered]= ImVec4(0.20f, 0.50f, 1.00f, 0.60f);
    c[ImGuiCol_SeparatorActive] = ImVec4(0.20f, 0.50f, 1.00f, 1.00f);
    c[ImGuiCol_ResizeGrip]      = ImVec4(0.15f, 0.15f, 0.22f, 1.00f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.25f, 0.55f, 1.00f, 1.00f);
    c[ImGuiCol_ResizeGripActive]  = ImVec4(0.30f, 0.65f, 1.00f, 1.00f);
    c[ImGuiCol_Tab]             = ImVec4(0.05f, 0.05f, 0.08f, 1.00f);
    c[ImGuiCol_TabHovered]      = ImVec4(0.20f, 0.50f, 1.00f, 0.70f);
    c[ImGuiCol_TabActive]       = ImVec4(0.20f, 0.50f, 1.00f, 0.90f);
    c[ImGuiCol_PlotHistogram]   = ImVec4(0.20f, 0.55f, 1.00f, 1.00f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.30f, 0.70f, 1.00f, 1.00f);
    c[ImGuiCol_Text]            = ImVec4(0.93f, 0.93f, 0.96f, 1.00f);
    c[ImGuiCol_TextDisabled]    = ImVec4(0.38f, 0.38f, 0.46f, 1.00f);

    // кириллица + символы для кнопок (геометрия ▼▶, стрелки ↻)
    static const ImWchar kFontRanges[] = {
        0x0020, 0x00FF, // latin + ×
        0x0400, 0x052F, // cyrillic
        0x2DE0, 0x2DFF, 0xA640, 0xA69F, // cyrillic ext
        0x25A0, 0x25FF, // ▼ ▶ ● ◆
        0x2190, 0x21FF, // ↻ →
        0,
    };

    // load font with Cyrillic support (UI)
    ImFont* fontMain = nullptr;
    for (const char* fp : { "C:\\Windows\\Fonts\\segoeui.ttf", "C:\\Windows\\Fonts\\msyh.ttc",
                             "C:\\Windows\\Fonts\\tahoma.ttf", "C:\\Windows\\Fonts\\arial.ttf" }) {
        fontMain = io.Fonts->AddFontFromFileTTF(fp, 19.0f, nullptr, kFontRanges);
        if (fontMain) break;
    }
    if (!fontMain)
        fontMain = io.Fonts->AddFontDefault();
    io.FontDefault = fontMain;

    // big title font
    ImFont* fontTitle = nullptr;
    for (const char* fp : { "C:\\Windows\\Fonts\\segoeuib.ttf", "C:\\Windows\\Fonts\\segoeui.ttf",
                             "C:\\Windows\\Fonts\\tahoma.ttf" }) {
        fontTitle = io.Fonts->AddFontFromFileTTF(fp, 30.0f, nullptr, kFontRanges);
        if (fontTitle) break;
    }
    if (!fontTitle) fontTitle = fontMain;

    // small font for hints
    ImFont* fontSmall = nullptr;
    for (const char* fp : { "C:\\Windows\\Fonts\\segoeui.ttf", "C:\\Windows\\Fonts\\tahoma.ttf" }) {
        fontSmall = io.Fonts->AddFontFromFileTTF(fp, 14.0f, nullptr, kFontRanges);
        if (fontSmall) break;
    }
    if (!fontSmall) fontSmall = fontMain;

    // monospace font for log
    ImFont* fontMono = nullptr;
    for (const char* fp : { "C:\\Windows\\Fonts\\consola.ttf", "C:\\Windows\\Fonts\\cour.ttf",
                             "C:\\Windows\\Fonts\\segoeui.ttf" }) {
        fontMono = io.Fonts->AddFontFromFileTTF(fp, 15.0f, nullptr, kFontRanges);
        if (fontMono) break;
    }
    if (!fontMono) fontMono = fontMain;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    refresh_status();
    log_add("OpenSteamTool Installer v4.9");
    log_add(tg("Готово. Установка — во вкладке «Настройки», игры — во вкладке «Игры».",
        "Ready. Installer is in «Settings», games are in «Games»."));
    load_theme();

    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // ── window ──
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("##main", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // плавное появление окна при запуске
        static double g_startT = -1.0;
        double nowS = ImGui::GetTime();
        if (g_startT < 0) g_startT = nowS;
        float startA = (float)((nowS - g_startT) / 0.6);
        if (startA > 1.0f) startA = 1.0f;
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.15f + 0.85f * startA);

        float cw = ImGui::GetContentRegionAvail().x;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 winPos = ImGui::GetWindowPos();
        ImU32 accentU32 = ImGui::ColorConvertFloat4ToU32(
            ImVec4(g_accent.x, g_accent.y, g_accent.z, 1.0f));
        ImGui::GetStyle().Colors[ImGuiCol_CheckMark] = g_accent;

        // ── top neon accent line, бегущий градиент в цвете темы ──
        {
            float x0 = winPos.x, x1 = winPos.x + ImGui::GetWindowSize().x;
            float y = winPos.y + 2.0f;
            float tfrac = fmodf((float)ImGui::GetTime() * 0.15f, 1.0f);
            ImVec4 ice(0.78f, 0.87f, 1.0f, 1.0f);
            int segs = 64;
            for (int i = 0; i < segs; i++) {
                float t0 = (float)i / segs, t1 = (float)(i + 1) / segs;
                float ph = t0 + tfrac; // бегущая фаза 0..1
                if (ph >= 1.0f) ph -= 1.0f;
                ImVec4 col;
                if (ph < 0.5f) {
                    float k = ph * 2.0f;
                    float m = 0.55f + 0.45f * k;
                    col = ImVec4(g_accent.x * m, g_accent.y * m, g_accent.z * m, 1.0f);
                } else {
                    float k = (ph - 0.5f) * 2.0f;
                    col = ImVec4(g_accent.x + (ice.x - g_accent.x) * k,
                                 g_accent.y + (ice.y - g_accent.y) * k,
                                 g_accent.z + (ice.z - g_accent.z) * k, 1.0f);
                }
                dl->AddLine(ImVec2(x0 + (x1 - x0) * t0, y), ImVec2(x0 + (x1 - x0) * t1, y),
                    ImGui::ColorConvertFloat4ToU32(col), 3.0f);
            }
        }
        // ── дрейфующие частицы на фоне (в цвете темы) ──
        if (g_particles)
        {
            ImVec2 ws = ImGui::GetWindowSize();
            float t = (float)ImGui::GetTime();
            int pr = (int)(g_accent.x * 255), pg = (int)(g_accent.y * 255), pb = (int)(g_accent.z * 255);
            for (int i = 0; i < 36; i++) {
                float hx = fmodf(i * 197.3f, 1.0f);
                float hy = fmodf(i * 331.7f, 1.0f);
                float sp = 6.0f + fmodf(i * 53.0f, 14.0f);
                float x = winPos.x + fmodf(hx * ws.x + t * sp, ws.x);
                float y = winPos.y + fmodf(hy * ws.y + t * sp * 0.4f, ws.y);
                int al = 8 + (int)(10 * (0.5f + 0.5f * sinf(t * 1.3f + (float)i)));
                dl->AddCircleFilled(ImVec2(x, y), 1.5f + fmodf((float)i * 7.0f, 2.0f),
                    IM_COL32(pr, pg, pb, al));
            }
        }
        ImGui::Spacing();

        // ══ layout: sidebar + page ══
        float footH = 36.0f;
        float midH = ImGui::GetContentRegionAvail().y - footH;
        if (midH < 200) midH = 200;
        float navW = 216.0f;

        // ── sidebar ──
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));
        ImGui::BeginChild("##nav", ImVec2(navW, midH), true);
        {
            float tm = (float)ImGui::GetTime();
            float pulse = 0.5f + 0.5f * sinf(tm * 2.5f);
            ImVec2 cur = ImGui::GetCursorScreenPos();
            ImVec2 dot(cur.x + 9, cur.y + 13);
            ImDrawList* ndl = ImGui::GetWindowDrawList();
            ndl->AddCircleFilled(dot, 12.0f, ImGui::ColorConvertFloat4ToU32(
                ImVec4(g_accent.x, g_accent.y, g_accent.z, 0.10f + 0.12f * pulse)));
            ndl->AddCircleFilled(dot, 9.0f + pulse * 1.5f, accentU32);
            ndl->AddCircleFilled(dot, 4.0f,
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.02f, 0.02f, 0.03f, 1.0f)));
            ImGui::Dummy(ImVec2(26, 2));
            ImGui::SameLine();
            ImGui::PushFont(fontMain);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "OpenSteamTool");
            ImGui::PopFont();
            ImGui::PushFont(fontSmall);
            ImGui::TextColored(ImVec4(0.45f, 0.47f, 0.58f, 1.0f),
                tg("Steam-игры в один клик", "One-click Steam games"));
            ImGui::PopFont();
            // пилюля версии
            {
                const char* ver = "v4.9";
                ImVec2 tp = ImGui::GetCursorScreenPos();
                ImVec2 tsz = ImGui::CalcTextSize(ver);
                ImVec2 pad(10, 3);
                ImVec2 b0(tp.x, tp.y + 2), b1(tp.x + tsz.x + pad.x * 2, tp.y + 2 + tsz.y + pad.y * 2);
                dl->AddRectFilled(b0, b1, accentU32, 9.0f);
                ImGui::SetCursorScreenPos(ImVec2(b0.x + pad.x, b0.y + pad.y));
                ImGui::PushFont(fontSmall);
                ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", ver);
                ImGui::PopFont();
            }
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::PushFont(fontSmall);
            ImGui::TextColored(ImVec4(0.45f, 0.47f, 0.58f, 1.0f), tg("МЕНЮ", "MENU"));
            ImGui::PopFont();
            ImGui::Spacing();
            // компактные кнопки навигации
            auto nav_btn = [&](int idx, const char* label) {
                ImVec2 sz(ImGui::GetContentRegionAvail().x, 40);
                if (g_tab == idx) {
                    accent_btn(label, sz, g_accent);
                    ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
                    g_selX = mn.x - 8; g_selY0 = mn.y; g_selY1 = mx.y;
                    if (!g_indInit) { g_indY0 = mn.y; g_indH0 = mx.y - mn.y; g_indInit = true; }
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.06f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.10f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.63f, 0.70f, 1.0f));
                    if (ImGui::Button(label, sz)) {
                        g_tab = idx;
                        if (idx == 1) ensure_covers_thread();
                    }
                    ImGui::PopStyleColor(4);
                }
            };
            nav_btn(0, tg("Главная", "Home"));
            char tabGames[64];
            snprintf(tabGames, sizeof(tabGames), "%s (%d)", tg("Игры", "Games"), kGamesCount);
            nav_btn(1, tabGames);
            nav_btn(2, tg("Настройки", "Settings"));
            // скользящий неоновый индикатор
            {
                float k = 1.0f - expf(-ImGui::GetIO().DeltaTime * 14.0f);
                g_indY0 += (g_selY0 - g_indY0) * k;
                g_indH0 += ((g_selY1 - g_selY0) - g_indH0) * k;
                if (fabsf(g_indY0 - g_selY0) < 0.5f) {
                    g_indY0 = g_selY0;
                    g_indH0 = g_selY1 - g_selY0;
                }
                dl->AddLine(ImVec2(g_selX, g_indY0 + 9), ImVec2(g_selX, g_indY0 + g_indH0 - 9),
                    accentU32, 3.0f);
            }
            // статус + прогресс + язык внизу сайдбара
            ImGui::SetCursorPosY(midH - 102);
            if (g_busy.load()) {
                spinner(7.0f, 2.5f, accentU32);
                ImGui::SameLine();
            }
            ImGui::PushFont(fontSmall);
            {
                std::lock_guard<std::mutex> lk(g_statusMutex);
                float sw = navW - 28 - (g_busy.load() ? 26 : 0);
                std::string st = trunc1(g_status.c_str(), sw);
                ImGui::TextDisabled("%s", st.c_str());
            }
            ImGui::PopFont();
            // мини-бар прогресса
            {
                int pgSide = g_progressAtomic.load();
                if (g_busy.load() || pgSide > 0) {
                    ImVec2 bp = ImGui::GetCursorScreenPos();
                    float bw = ImGui::GetContentRegionAvail().x;
                    ImDrawList* sdl = ImGui::GetWindowDrawList();
                    sdl->AddRectFilled(bp, ImVec2(bp.x + bw, bp.y + 6),
                        IM_COL32(25, 25, 38, 255), 3.0f);
                    if (pgSide > 0) {
                        float fx = bp.x + bw * (pgSide / 100.0f);
                        sdl->AddRectFilled(bp, ImVec2(fx, bp.y + 6), accentU32, 3.0f);
                        if (g_busy.load()) {
                            float ph = fmodf((float)ImGui::GetTime() * 0.9f, 1.2f) - 0.1f;
                            float sx = bp.x + bw * ph;
                            sdl->AddLine(ImVec2(sx, bp.y), ImVec2(sx, bp.y + 6),
                                IM_COL32(255, 255, 255, 110), 2.0f);
                        }
                    }
                    ImGui::Dummy(ImVec2(bw, 6));
                }
            }
            ImGui::SetCursorPosY(midH - 52);
            if (ImGui::Button(g_lang_gui ? "EN" : "RU",
                    ImVec2(ImGui::GetContentRegionAvail().x, 30)))
                g_lang_gui = !g_lang_gui;
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::SameLine(0, 10);

        // ── page ──
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::BeginChild("##page", ImVec2(0, midH), false, ImGuiWindowFlags_NoBackground);
        ImGui::PopStyleVar();
        float pw = ImGui::GetContentRegionAvail().x;
        // заголовок страницы
        {
            ImGui::PushFont(fontTitle);
            const char* ttl = g_tab == 0 ? tg("Новости обновлений", "Update news")
                : g_tab == 1 ? tg("Каталог игр", "Game catalog")
                : tg("Настройки", "Settings");
            ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", ttl);
            ImGui::PopFont();
        }
        if (g_tab == 2) {
            ImGui::SameLine(pw - 30);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
            if (ImGui::SmallButton("\xE2\x86\xBB")) refresh_status();
        }
        ImGui::Spacing();

        // плавное появление контента при переключении вкладок
        static int g_lastTab = -1;
        static double g_tabTime = 0;
        double nowT = ImGui::GetTime();
        if (g_lastTab != g_tab) { g_lastTab = g_tab; g_tabTime = nowT; }
        float fade = (float)((nowT - g_tabTime) / 0.22);
        if (fade > 1.0f) fade = 1.0f;
        if (fade < 0.0f) fade = 0.0f;
        float ease = fade * fade * (3.0f - 2.0f * fade); // smoothstep
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.30f + 0.70f * ease);

        if (g_tab == 2) {

        // ── НАСТРОЙКИ: статусы установки ──
        ImGui::PushFont(fontSmall);
        ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.62f, 1.0f), tg("СТАТУС", "STATUS"));
        ImGui::PopFont();

        // ── status cards: DLL / PATTERN ──
        {
            float gap = 10.0f;
            float cardW = (pw - gap) / 2.0f;
            float cardH = 72.0f;

            auto card = [&](const char* id, const char* name, bool ok, const char* okT, const char* badT) {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
                ImGui::BeginChild(id, ImVec2(cardW, cardH), true);
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                ImVec2 dot = ImVec2(p0.x + 16, p0.y + 15);
                ImU32 dotCol = ok ? IM_COL32(20, 230, 110, 255) : IM_COL32(255, 70, 70, 255);
                dl->AddCircleFilled(dot, 6.0f, dotCol);
                int glow = 45 + (int)(35 * (0.5f + 0.5f * sinf((float)ImGui::GetTime() * 3.0f)));
                if (ok) dl->AddCircle(dot, 10.0f, IM_COL32(20, 230, 110, glow), 24, 2.0f);
                else    dl->AddCircle(dot, 10.0f, IM_COL32(255, 70, 70, glow), 24, 2.0f);
                // цветная полоска слева по состоянию
                dl->AddLine(ImVec2(p0.x + 2, p0.y + 10), ImVec2(p0.x + 2, p0.y + cardH - 20), dotCol, 3.0f);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 34);
                ImGui::PushFont(fontSmall);
                ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.62f, 1.0f), "%s", name);
                ImGui::PopFont();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 34);
                if (ok) ImGui::TextColored(ImVec4(0.15f, 0.90f, 0.45f, 1.0f), "%s", okT);
                else    ImGui::TextColored(ImVec4(1.0f, 0.32f, 0.32f, 1.0f), "%s", badT);
                ImGui::EndChild();
                ImGui::PopStyleVar();
                if (ImGui::IsItemHovered()) {
                    ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
                    dl->AddRect(mn, mx, accentU32, 10.0f, 0, 2.0f);
                }
            };
            card("##cardDll", "DLL", g_dok,
                tg("Установлены", "Installed"), tg("Не установлены", "Missing"));
            ImGui::SameLine(0, gap);
            card("##cardPat", "PATTERN", g_pok,
                tg("Загружен", "Loaded"), tg("Не загружен", "Missing"));
        }

        ImGui::Spacing();

        // ── AppID row ──
        ImGui::PushFont(fontSmall);
        ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.62f, 1.0f), "AppID");
        ImGui::PopFont();
        float addW = 130;
        {
            float idBox = 34.0f;
            float fh = ImGui::GetFrameHeight();
            ImVec2 ip = ImGui::GetCursorScreenPos();
            dl->AddRectFilled(ip, ImVec2(ip.x + idBox, ip.y + fh), accentU32, 8.0f);
            ImVec2 itw = ImGui::CalcTextSize("ID");
            ImGui::SetCursorScreenPos(ImVec2(ip.x + (idBox - itw.x) * 0.5f, ip.y + (fh - itw.y) * 0.5f));
            ImGui::TextColored(ImVec4(1, 1, 1, 1), "ID");
            ImGui::SetCursorScreenPos(ImVec2(ip.x + idBox + 8, ip.y));
            ImGui::SetNextItemWidth(pw - addW - 10 - idBox - 8);
            ImGui::InputTextWithHint("##appid", tg("Например: 271590", "e.g. 271590"),
                g_appidBuf, sizeof(g_appidBuf));
        }
        ImGui::SameLine();
        bool addClicked = accent_btn(tg("+  Добавить игру", "+  Add game"), ImVec2(addW, 0),
            ImVec4(0.05f, 0.55f, 0.30f, 1.0f));
        if (addClicked || (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter))) {
            bool valid = g_appidBuf[0] != 0;
            for (char* cptr = g_appidBuf; *cptr; cptr++)
                if (!isdigit((unsigned char)*cptr)) { valid = false; break; }
            if (valid) {
                do_add_game(g_appidBuf);
                g_appidBuf[0] = 0;
            } else if (g_appidBuf[0]) {
                log_add(tg("не AppID — только цифры", "not AppID — digits only"));
            }
        }

        ImGui::Spacing();

        // ── ДЕЙСТВИЯ: карточки 2x2 ──
        ImGui::PushFont(fontSmall);
        ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.62f, 1.0f), tg("ДЕЙСТВИЯ", "ACTIONS"));
        ImGui::PopFont();
        {
            float agap = 10.0f;
            float acw = (pw - agap) / 2.0f;
            float ah = 68.0f;
            auto act = [&](const char* id, const char* glyph, const char* title, const char* desc,
                           ImVec4 accent, std::function<void()> fn) {
                ImGui::BeginChild(id, ImVec2(acw, ah), true);
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                ImVec2 inner = ImGui::GetContentRegionAvail();
                float box = 40.0f;
                ImU32 acol = ImGui::ColorConvertFloat4ToU32(accent);
                dl->AddRectFilled(p0, ImVec2(p0.x + box, p0.y + box), acol, 9.0f);
                // глянец сверху иконки
                dl->AddRectFilled(p0, ImVec2(p0.x + box, p0.y + box * 0.45f),
                    IM_COL32(255, 255, 255, 26), 9.0f, ImDrawFlags_RoundCornersTop);
                ImVec2 gz = ImGui::CalcTextSize(glyph);
                ImGui::SetCursorScreenPos(ImVec2(p0.x + (box - gz.x) / 2, p0.y + (box - gz.y) / 2));
                ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", glyph);
                ImGui::SetCursorScreenPos(ImVec2(p0.x + box + 10, p0.y + 1));
                ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", title);
                ImGui::SetCursorScreenPos(ImVec2(p0.x + box + 10, p0.y + 23));
                ImGui::PushFont(fontSmall);
                ImGui::TextColored(ImVec4(0.55f, 0.57f, 0.65f, 1.0f), "%s", desc);
                ImGui::PopFont();
                ImGui::SetCursorScreenPos(p0);
                bool clicked = ImGui::InvisibleButton("##hit", inner);
                if (ImGui::IsItemHovered()) {
                    dl->AddRect(ImVec2(p0.x - 5, p0.y - 5),
                        ImVec2(p0.x + inner.x + 5, p0.y + inner.y + 5),
                        ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.35f)),
                        14.0f, 0, 3.0f);
                    dl->AddRect(p0, ImVec2(p0.x + inner.x, p0.y + inner.y),
                        ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.9f)),
                        10.0f, 0, 2.0f);
                }
                ImGui::EndChild();
                if (clicked) { refresh_status(); fn(); }
            };
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
            act("##aAll", "\xE2\x96\xBC", tg("Установить всё", "Install all"),
                tg("DLL + паттерн + подготовка", "DLLs + pattern + setup"),
                ImVec4(0.12f, 0.42f, 0.95f, 1.0f), do_full_install);
            ImGui::SameLine(0, agap);
            act("##aGame", "\xE2\x96\xB6", tg("Только игру", "Game only"),
                tg("Подготовка под AppID", "Prepare for AppID"),
                ImVec4(0.06f, 0.45f, 0.48f, 1.0f), do_games_only);
            act("##aPat", "\xE2\x86\xBB", tg("Обновить паттерн", "Update pattern"),
                tg("Свежие файлы с GitHub", "Fresh files from GitHub"),
                ImVec4(0.35f, 0.22f, 0.62f, 1.0f), do_update_pattern);
            ImGui::SameLine(0, agap);
            act("##aDel", "\xC3\x97", tg("Удалить", "Remove"),
                tg("Убрать DLL и файлы", "Remove DLLs and files"),
                ImVec4(0.55f, 0.16f, 0.16f, 1.0f), do_remove);
            ImGui::PopStyleVar();
        }

        ImGui::Spacing();

        // ── ОФОРМЛЕНИЕ: свой цвет интерфейса ──
        ImGui::PushFont(fontSmall);
        ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.62f, 1.0f), tg("ОФОРМЛЕНИЕ", "APPEARANCE"));
        ImGui::PopFont();
        {
            bool ch = ImGui::ColorEdit3(tg("Свой цвет", "Custom color"), (float*)&g_accent,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            ImGui::SameLine();
            const ImVec4 presets[] = {
                ImVec4(0.12f, 0.42f, 0.95f, 1), ImVec4(0.05f, 0.75f, 0.85f, 1),
                ImVec4(0.45f, 0.30f, 0.85f, 1), ImVec4(0.10f, 0.65f, 0.35f, 1),
                ImVec4(0.95f, 0.50f, 0.10f, 1), ImVec4(0.90f, 0.20f, 0.25f, 1),
                ImVec4(0.90f, 0.30f, 0.60f, 1),
            };
            for (int pi = 0; pi < 7; pi++) {
                if (pi) ImGui::SameLine(0, 6);
                ImGui::PushID(pi);
                if (ImGui::ColorButton("##sw", presets[pi], ImGuiColorEditFlags_NoTooltip, ImVec2(24, 24))) {
                    g_accent = presets[pi];
                    ch = true;
                }
                float dd = fabsf(g_accent.x - presets[pi].x) + fabsf(g_accent.y - presets[pi].y)
                    + fabsf(g_accent.z - presets[pi].z);
                if (dd < 0.03f) {
                    ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddRect(mn, mx, IM_COL32(255, 255, 255, 220), 4.0f, 0, 2.0f);
                }
                ImGui::PopID();
            }
            ImGui::SameLine(0, 10);
            if (ImGui::SmallButton(tg("Сброс", "Reset"))) { g_accent = kAccentDef; ch = true; }
            if (ch) save_theme();
            ImGui::SameLine(0, 12);
            ImGui::Checkbox(tg("Частицы", "Particles"), &g_particles);
            ImGui::SameLine(0, 12);
            ImGui::Checkbox("240 FPS", &g_fps240);
        }

        ImGui::Spacing();

        // ── ЖУРНАЛ ──
        ImGui::PushFont(fontSmall);
        ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.62f, 1.0f), tg("ЖУРНАЛ", "LOG"));
        ImGui::PopFont();
        ImGui::SameLine(pw - 190);
        ImGui::PushFont(fontSmall);
        ImGui::Checkbox(tg("Автопрокрутка", "Autoscroll"), &g_autoScroll);
        ImGui::PopFont();
        ImGui::SameLine();
        if (ImGui::SmallButton(tg("Очистить", "Clear"))) {
            std::lock_guard<std::mutex> lk(g_logMutex);
            g_log.clear();
        }

        float logH = ImGui::GetContentRegionAvail().y - 8;
        if (logH < 60) logH = 60;

        ImGui::BeginChild("Log", ImVec2(0, logH), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::PushFont(fontMono);
        {
            std::lock_guard<std::mutex> lk(g_logMutex);
            if (g_log.empty())
                ImGui::TextDisabled("%s", tg("Здесь появятся сообщения...", "Messages will appear here..."));
            for (auto& line : g_log) {
                ImVec4 col(0.82f, 0.82f, 0.86f, 1.0f);
                if (line.find("ошибка") != std::string::npos ||
                    line.find("error") != std::string::npos ||
                    line.find("ERR") != std::string::npos ||
                    line.find("не найден") != std::string::npos)
                    col = ImVec4(1.0f, 0.38f, 0.38f, 1.0f);
                else if (line.find("установлено!") != std::string::npos ||
                         line.find("installed!") != std::string::npos ||
                         line.find("Удалено") != std::string::npos ||
                         line.find("Removed") != std::string::npos)
                    col = ImVec4(0.15f, 0.90f, 0.45f, 1.0f);
                else if (line.find("загрузка") != std::string::npos ||
                         line.find("downloading") != std::string::npos ||
                         line.find("установка") != std::string::npos)
                    col = ImVec4(0.35f, 0.75f, 1.0f, 1.0f);
                else if (line.find("===") != std::string::npos ||
                         line.find("Готово") != std::string::npos ||
                         line.find("Done") != std::string::npos)
                    col = ImVec4(0.60f, 0.62f, 0.72f, 1.0f);
                ImGui::TextColored(col, "%s", line.c_str());
            }
            if (g_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4)
                ImGui::SetScrollHereY(1.0f);
        }
        ImGui::PopFont();
        ImGui::EndChild();

        } else if (g_tab == 1) {
            // ══ вкладка ИГРЫ: каталог с обложками ══
            ensure_covers_thread();
            pump_covers(6);

            ImGui::SetNextItemWidth(pw);
            ImGui::InputTextWithHint("##gsearch",
                tg("Поиск игры или AppID...", "Search game or AppID..."),
                g_searchBuf, sizeof(g_searchBuf));

            // категории-чипсы (два ряда, без скроллбара)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
            ImGui::BeginChild("##cats", ImVec2(0, 68), false, ImGuiWindowFlags_NoScrollbar);
            for (int ci = 0; ci < kCatsCount; ci++) {
                const char* lbl = tg(kCats[ci].ru, kCats[ci].en);
                if (ci) {
                    float wneed = ImGui::CalcTextSize(lbl).x + 26.0f;
                    if (ImGui::GetCursorPosX() + wneed < pw - 8) ImGui::SameLine();
                }
                if (ci == g_catSel) {
                    accent_btn(lbl, ImVec2(0, 28), g_accent);
                } else if (ImGui::Button(lbl, ImVec2(0, 28))) {
                    g_catSel = ci;
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();

            // фильтр
            std::vector<const GameEntry*> list;
            for (int i = 0; i < kGamesCount; i++) {
                const GameEntry* ge = &kGames[i];
                if (g_catSel != 0 && strcmp(ge->cat, kCats[g_catSel].key) != 0) continue;
                if (g_searchBuf[0]) {
                    if (!contains_ci(ge->name, g_searchBuf) && !strstr(ge->appid, g_searchBuf))
                        continue;
                }
                list.push_back(ge);
            }

            ImGui::PushFont(fontSmall);
            char foundBuf[128];
            snprintf(foundBuf, sizeof(foundBuf), tg("Найдено игр: %d  •  обложки: %d/%d",
                "Games found: %d  •  covers: %d/%d"),
                (int)list.size(), (int)g_covers.size(), kGamesCount);
            ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.62f, 1.0f), "%s", foundBuf);
            ImGui::PopFont();

            float gridH = ImGui::GetContentRegionAvail().y - 40;
            if (gridH < 100) gridH = 100;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::BeginChild("##grid", ImVec2(0, gridH), false,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
            smooth_child("##grid");

            int cols = pw > 560 ? 4 : (pw > 380 ? 3 : 2);
            float gap = 10.0f;
            float cardW = (ImGui::GetContentRegionAvail().x - gap * (float)(cols - 1)) / (float)cols;
            float imgW = cardW - 16.0f;
            float imgH = imgW * 353.0f / 616.0f; // capsule_616x353

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
            ImGui::PushFont(fontSmall);
            for (size_t i = 0; i < list.size(); i++) {
                const GameEntry* ge = list[i];
                if (i % (size_t)cols) ImGui::SameLine(0, gap);
                ImGui::PushID((int)i);
                ImGui::BeginChild("##card", ImVec2(cardW, imgH + 104), true);
                auto it = g_covers.find(ge->appid);
                if (it != g_covers.end() && it->second) {
                    if (g_hoverCard == ge->appid) {
                        ImVec2 ic = ImGui::GetCursorScreenPos();
                        ImGui::SetCursorScreenPos(ImVec2(ic.x - 6, ic.y - 3));
                        ImGui::Image((ImTextureID)it->second, ImVec2(imgW + 12, imgH + 7));
                    } else {
                        ImGui::Image((ImTextureID)it->second, ImVec2(imgW, imgH));
                    }
                    // затемнение низа + бейдж категории поверх обложки
                    ImVec2 imn = ImGui::GetItemRectMin(), imx = ImGui::GetItemRectMax();
                    ImDrawList* cdl = ImGui::GetWindowDrawList();
                    float fh2 = 26.0f;
                    for (int sgi = 0; sgi < 8; sgi++) {
                        float s0 = (float)sgi / 8.0f, s1 = (float)(sgi + 1) / 8.0f;
                        cdl->AddRectFilled(ImVec2(imn.x, imx.y - fh2 + fh2 * s0),
                            ImVec2(imx.x, imx.y - fh2 + fh2 * s1),
                            IM_COL32(4, 4, 8, (int)(s0 * 120)), 0.0f);
                    }
                    ImVec2 saved = ImGui::GetCursorPos();
                    const char* cname = cat_name(ge->cat);
                    ImVec2 pp(imn.x + 6, imn.y + 6);
                    ImVec2 psz = ImGui::CalcTextSize(cname);
                    cdl->AddRectFilled(pp, ImVec2(pp.x + psz.x + 12, pp.y + psz.y + 6),
                        IM_COL32(5, 5, 10, 170), 5.0f);
                    ImGui::SetCursorScreenPos(ImVec2(pp.x + 6, pp.y + 3));
                    ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", cname);
                    ImGui::SetCursorPos(saved);
                } else {
                    // скелетон с пробегающим бликом
                    ImVec2 p0 = ImGui::GetCursorScreenPos();
                    ImDrawList* pdl = ImGui::GetWindowDrawList();
                    pdl->AddRectFilled(p0,
                        ImVec2(p0.x + imgW, p0.y + imgH), IM_COL32(10, 10, 18, 255), 6.0f);
                    pdl->PushClipRect(p0, ImVec2(p0.x + imgW, p0.y + imgH), true);
                    float psx = p0.x + imgW * (fmodf((float)ImGui::GetTime() * 0.5f, 1.4f) - 0.2f);
                    pdl->AddLine(ImVec2(psx, p0.y), ImVec2(psx, p0.y + imgH),
                        IM_COL32(255, 255, 255, 26), 18.0f);
                    pdl->PopClipRect();
                    ImGui::Dummy(ImVec2(imgW, imgH));
                    ImGui::SetCursorScreenPos(ImVec2(p0.x + 8, p0.y + imgH / 2 - 9));
                    ImGui::TextDisabled("%s", tg("Загрузка...", "Loading..."));
                }
                ImGui::Spacing();
                std::string nm = trunc1(ge->name, imgW);
                ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", nm.c_str());
                ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.62f, 1.0f), "AppID %s", ge->appid);
                ImGui::BeginDisabled(g_busy.load());
                if (accent_btn(tg("Установить", "Install"), ImVec2(imgW, 28),
                        ImVec4(0.05f, 0.55f, 0.30f, 1.0f))) {
                    refresh_status();
                    do_add_game(std::string(ge->appid));
                }
                ImGui::EndDisabled();
                ImGui::EndChild();
                // неоновая рамка + зум при наведении на карточку
                if (ImGui::IsItemHovered()) {
                    g_hoverCard = ge->appid;
                    ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddRect(mn, mx,
                        accentU32, 10.0f, 0, 2.0f);
                } else if (g_hoverCard == ge->appid) {
                    g_hoverCard.clear();
                }
                ImGui::PopID();
            }
            ImGui::PopFont();
            ImGui::PopStyleVar();
            ImGui::PopStyleVar();
            ImGui::EndChild(); // ##grid
        } else {
            // ══ вкладка ГЛАВНАЯ: новости обновлений ══
            // ── hero-баннер ──
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 10));
            ImGui::BeginChild("##hero", ImVec2(0, 128), true);
            {
                ImVec2 hp = ImGui::GetCursorScreenPos();
                float hw = ImGui::GetContentRegionAvail().x;
                int segs = 28;
                for (int i = 0; i < segs; i++) {
                    float k0 = (float)i / segs, k1 = (float)(i + 1) / segs;
                    int al = (int)((1.0f - k0) * 70);
                    ImU32 hc = IM_COL32((int)(g_accent.x * 255), (int)(g_accent.y * 255),
                        (int)(g_accent.z * 255), al);
                    ImDrawFlags rf = 0;
                    if (i == 0) rf = ImDrawFlags_RoundCornersLeft;
                    else if (i == segs - 1) rf = ImDrawFlags_RoundCornersRight;
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        ImVec2(hp.x + hw * k0, hp.y), ImVec2(hp.x + hw * k1, hp.y + 108), hc, 10.0f, rf);
                }
                // пробегающий блик по баннеру
                {
                    ImDrawList* hdl = ImGui::GetWindowDrawList();
                    ImVec2 h0(hp.x, hp.y), h1(hp.x + hw, hp.y + 108);
                    hdl->PushClipRect(h0, h1, true);
                    float hsx = hp.x + hw * (fmodf((float)ImGui::GetTime() * 0.22f, 1.4f) - 0.2f);
                    hdl->AddLine(ImVec2(hsx, h0.y), ImVec2(hsx, h1.y),
                        IM_COL32(255, 255, 255, 34), 30.0f);
                    hdl->PopClipRect();
                }
                ImGui::PushFont(fontTitle);
                ImGui::TextColored(ImVec4(1, 1, 1, 1), "OpenSteamTool");
                ImGui::PopFont();                ImGui::PushFont(fontSmall);
                ImGui::TextColored(ImVec4(0.65f, 0.67f, 0.75f, 1.0f),
                    tg("Всё для Steam-игр в одном окне", "Everything for Steam games in one window"));
                char stats[128];
                snprintf(stats, sizeof(stats), tg("%d игр  •  %d категорий  •  RU / EN",
                    "%d games  •  %d categories  •  RU / EN"), kGamesCount, kCatsCount - 1);
                ImGui::TextColored(g_accent, "%s", stats);
                ImGui::PopFont();
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::Spacing();

            ImGui::PushFont(fontSmall);
            ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.62f, 1.0f), tg("ЧТО НОВОГО", "WHAT'S NEW"));
            ImGui::PopFont();
            float newsH = ImGui::GetContentRegionAvail().y - 8;
            if (newsH < 100) newsH = 100;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::BeginChild("##news", ImVec2(0, newsH), false,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
            smooth_child("##news");
            for (int ni = 0; ni < kNewsCount; ni++) {
                const NewsItem* nw = &kNews[ni];
                float lh = ImGui::GetTextLineHeightWithSpacing();
                float cardH = 66 + nw->n * lh;
                char nid[32];
                snprintf(nid, sizeof(nid), "##news%d", ni);
                // каскадное появление: каждая следующая выезжает дальше
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (1.0f - ease) * (10.0f + ni * 8.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 10));
                ImGui::BeginChild(nid, ImVec2(0, cardH), true);
                // пилюля версии (у свежей — пульсирует)
                {
                    ImVec2 tp = ImGui::GetCursorScreenPos();
                    ImVec2 tsz = ImGui::CalcTextSize(nw->ver);
                    ImVec2 pad(10, 3);
                    ImVec2 b0(tp.x, tp.y), b1(tp.x + tsz.x + pad.x * 2, tp.y + tsz.y + pad.y * 2);
                    ImU32 pvc = (ni == 0) ? accentU32 : IM_COL32(40, 40, 58, 255);
                    if (ni == 0) {
                        int pa = 200 + (int)(55 * (0.5f + 0.5f * sinf((float)ImGui::GetTime() * 3.0f)));
                        pvc = IM_COL32((int)(g_accent.x * 255), (int)(g_accent.y * 255),
                            (int)(g_accent.z * 255), pa);
                    }
                    ImGui::GetWindowDrawList()->AddRectFilled(b0, b1, pvc, 8.0f);
                    ImGui::SetCursorScreenPos(ImVec2(b0.x + pad.x, b0.y + pad.y));
                    ImGui::PushFont(fontSmall);
                    ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", nw->ver);
                    ImGui::PopFont();
                }
                ImGui::SameLine(0, 10);
                ImGui::PushFont(fontSmall);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3);
                ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.62f, 1.0f), "%s", nw->date);
                ImGui::PopFont();
                ImGui::Spacing();
                for (int li = 0; li < nw->n; li++) {
                    ImGui::TextColored(g_accent, "%s", "•");
                    ImGui::SameLine();
                    std::string bl = trunc1(tg(nw->ru[li], nw->en[li]), pw - 70);
                    ImGui::Text("%s", bl.c_str());
                }
                ImGui::EndChild();
                ImGui::PopStyleVar();
                // акцентная полоска слева (у свежей новости — яркая) + рамка при наведении
                {
                    ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
                    ImU32 ac = (ni == 0) ? accentU32 : IM_COL32(60, 60, 80, 160);
                    ImGui::GetWindowDrawList()->AddLine(
                        ImVec2(mn.x + 2, mn.y + 10), ImVec2(mn.x + 2, mx.y - 10), ac, 3.0f);
                    if (ImGui::IsItemHovered())
                        ImGui::GetWindowDrawList()->AddRect(mn, mx, accentU32, 10.0f, 0, 2.0f);
                }
                ImGui::Spacing();
            }
            ImGui::EndChild(); // ##news
            ImGui::PopStyleVar();
        }
        ImGui::PopStyleVar(); // fade вкладок
        ImGui::EndChild(); // ##page

        ImGui::PopStyleVar(); // startup fade

        ImGui::End();

        ImGui::Render();
        const float clear_color[4] = { 0.016f, 0.016f, 0.022f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        HRESULT hr = g_pSwapChain->Present(0, 0); // без VSync — до 240 FPS
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
        if (g_fps240) {
            static double lastFrame = -1.0;
            double nowF = ImGui::GetTime();
            if (lastFrame < 0) lastFrame = nowF;
            double target = 1.0 / 240.0;
            double elapsed = nowF - lastFrame;
            if (elapsed < target) {
                DWORD ms = (DWORD)((target - elapsed) * 1000.0);
                if (ms > 0) ::Sleep(ms);
            }
            lastFrame = ImGui::GetTime();
        }
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    for (auto& kv : g_covers) if (kv.second) kv.second->Release();
    g_covers.clear();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassA(wc.lpszClassName, wc.hInstance);
    Gdiplus::GdiplusShutdown(gdiTok);
    return 0;
}
