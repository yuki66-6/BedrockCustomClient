#include "core/Application.h"

#include "core/ClientSettings.h"
#include "core/ConfigManager.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "core/Json.h"
#include "core/KeybindManager.h"
#include "core/ModuleDefinitions.h"
#include "core/ModuleManager.h"
#include "core/TrayIcon.h"
#include "gui/ClickGUI.h"
#include "gui/HudManager.h"
#include "gui/MapPage.h"
#include "gui/Notifications.h"
#include "gui/OverlayRenderer.h"
#include "gui/TextureManager.h"
#include "gui/ThemeManager.h"
#include "gui/Widgets.h"
#include "integration/BedrockOverlayAdapter.h"
#include "integration/GameIntegration.h"
#include "integration/InputMonitor.h"
#include "integration/ServerPinger.h"
#include "integration/TelemetryProvider.h"
#include "modules/ModuleRegistry.h"
#include "modules/render/RenderModules.h"
#include "utils/CrashHandler.h"
#include "utils/Log.h"
#include "utils/Paths.h"
#include "utils/PerfStats.h"
#include "utils/Strings.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <fstream>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace bcc {
namespace {

constexpr wchar_t kWindowClass[] = L"BedrockCustomClientOverlay";
constexpr int kMenuHotkeyId = 1;

double nowSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

RECT primaryMonitorRect() {
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY), &mi);
    return mi.rcMonitor;
}

} // namespace

Application::Application(AppOptions options) : options_(std::move(options)) {}

Application::~Application() { shutdown(); }

bool Application::init() {
    if (options_.selftest && options_.dataDir.empty())
        options_.dataDir = std::filesystem::temp_directory_path() / L"bcc-selftest-data";
    if (!options_.dataDir.empty()) paths::setDataDir(options_.dataDir);
    log::init(paths::logsDir() / L"bcc.log");
    crash::install(paths::logsDir());
    log::info("Démarrage de Bedrock Custom Client v{} (données : {})", BCC_VERSION, toUtf8(paths::dataDir().wstring()));

    // Une seule instance (l'autotest utilise son propre verrou pour ne pas gêner).
    instanceMutex_ = CreateMutexW(nullptr, TRUE,
                                  options_.selftest ? L"Local\\BedrockCustomClient.Selftest" : L"Local\\BedrockCustomClient.Instance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        log::warn("Une autre instance est déjà en cours d'exécution.");
        if (!options_.selftest)
            MessageBoxW(nullptr, L"Bedrock Custom Client est déjà lancé (voir la zone de notification).",
                        L"Bedrock Custom Client", MB_ICONINFORMATION);
        return false;
    }

    ImGui_ImplWin32_EnableDpiAwareness();
    {
        std::string err;
        if (!ModuleDefinitions::global().load(paths::sharedDir() / L"module-definitions" / L"modules.json", &err))
            log::warn("Définitions de modules partagées non chargées : {}", err);
    }
    theme_ = std::make_unique<ThemeManager>();
    notifications_ = std::make_unique<Notifications>();
    perf_ = std::make_unique<PerfStats>();
    bus_ = std::make_unique<EventBus>();
    telemetry_ = std::make_unique<TelemetryProvider>();
    adapter_ = std::make_unique<BedrockOverlayAdapter>(*telemetry_);
    client_ = std::make_unique<ClientSettings>();
    modules_ = std::make_unique<ModuleManager>(bus_.get());
    registerAllModules(*modules_);
    keybinds_ = std::make_unique<KeybindManager>(*bus_, *modules_);
    hud_ = std::make_unique<HudManager>(*modules_);
    textures_ = std::make_unique<TextureManager>();
    ui::setTheme(theme_.get());
    ui::setKeybindManager(keybinds_.get());

    if (!createWindow()) return false;
    renderer_ = std::make_unique<OverlayRenderer>();
    if (!renderer_->init(hwnd_, static_cast<UINT>(overlayRect_.right - overlayRect_.left),
                         static_cast<UINT>(overlayRect_.bottom - overlayRect_.top))) {
        selftest_.error = renderer_->lastError();
        if (!options_.selftest)
            MessageBoxW(nullptr, toWide("Initialisation du rendu impossible :\n" + renderer_->lastError()).c_str(),
                        L"Bedrock Custom Client", MB_ICONERROR);
        return false;
    }
    if (!initImGui()) return false;
    textures_->setDevice(renderer_->device());

    pinger_ = std::make_unique<ServerPinger>();
    telemetry_->setPinger(pinger_.get());
    telemetry_->setTextures(textures_.get());
    game_ = std::make_unique<GameIntegration>(hwnd_);
    input_ = std::make_unique<InputMonitor>(*bus_, telemetry_->input());
    input_->setFocusQuery([this] { return game_->isGameForeground(); }, [this] { return GetForegroundWindow() == hwnd_; });
    selftest_.rawInputOk = input_->registerDevices(hwnd_);

    GuiServices services;
    services.theme = theme_.get();
    services.notify = notifications_.get();
    services.perf = perf_.get();
    services.modules = modules_.get();
    services.keybinds = keybinds_.get();
    services.hud = hud_.get();
    services.client = client_.get();
    services.telemetry = telemetry_.get();
    services.requestQuit = [this] { PostMessageW(hwnd_, WM_CLOSE, 0, 0); };
    services.requestClose = [this] { setMenuOpen(false); };
    services.openConfigsFolder = [this] { openConfigsFolder(); };
    services.launchMinecraft = [this] { launchMinecraft(); };
    services.gameDetected = [this] { return game_ && game_->state().found; };
    services.ownerHwnd = hwnd_;
    map_ = std::make_unique<MapPage>(textures_.get(), notifications_.get());
    services.map = map_.get();
    services.adapter = adapter_.get();
    configs_ = std::make_unique<ConfigManager>(paths::configsDir());
    services.configs = configs_.get();
    gui_ = std::make_unique<ClickGUI>(services);

    setupConfigs();
    OpResult profile = configs_->loadStartupProfile();
    selftest_.profileStatus = profile.message;
    if (!profile.ok) notifications_->error(profile.message);
    else if (!configs_->lastWarnings().empty()) notifications_->warning(profile.message);

    moduleCtx_ = {bus_.get(), telemetry_.get(), notifications_.get()};
    modules_->initAll(moduleCtx_);

    // La touche du menu est configurable : réenregistrement à chaque changement.
    client_->menuKey->setOnChange([this] {
        registerMenuHotkey(client_->menuKey->get());
        keybinds_->setReservedKey(client_->menuKey->get());
    });
    keybinds_->setReservedKey(client_->menuKey->get());

    tray_ = std::make_unique<TrayIcon>();
    tray_->create(hwnd_, L"Bedrock Custom Client");
    registerMenuHotkey(client_->menuKey->get());

    // Dossier des réticules personnalisés créé d'avance pour l'utilisateur.
    std::error_code ec;
    std::filesystem::create_directories(TextureManager::folder(CrosshairModule::kFolder), ec);

    frameTimer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    startTime_ = lastTick_ = nowSeconds();
    telemetry_->setSessionStart(startTime_);
    if (options_.launchMinecraft) launchMinecraft();
    if (options_.openMenu) setMenuOpen(true);
    if (client_->startupNotification->get())
        notifications_->info("Bedrock Custom Client prêt — " + KeybindManager::keyName(client_->menuKey->get()) +
                             " pour ouvrir le menu.");
    return true;
}

void Application::setupConfigs() {
    configs_->addSection({"modules", [this] { return modules_->toJson(); },
                          [this](const Json& j) { return modules_->fromJson(j); }, [this] { modules_->resetAll(); }});
    configs_->addSection({"theme", [this] { return theme_->settingsToJson(); },
                          [this](const Json& j) {
                              auto w = theme_->settingsFromJson(j, "thème");
                              theme_->reconcilePreset();
                              return w;
                          },
                          [this] { theme_->resetSettings(); }});
    configs_->addSection({"hud", [this] { return hud_->toJson(); }, [this](const Json& j) { return hud_->fromJson(j); },
                          [this] { hud_->resetAll(); }});
    configs_->addSection({"client", [this] { return client_->settingsToJson(); },
                          [this](const Json& j) { return client_->settingsFromJson(j, "client"); },
                          [this] { client_->resetSettings(); }, "bedrock/client"});
    configs_->addSection({"gui", [this] { return gui_->toJson(); },
                          [this](const Json& j) {
                              gui_->fromJson(j);
                              return std::vector<std::string>{};
                          },
                          [this] { gui_->resetLayout(); }, "bedrock/gui"});
}

bool Application::createWindow() {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &Application::wndProcThunk;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(1));
    wc.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&wc)) {
        log::error("RegisterClassEx : {}", win32ErrorMessage(GetLastError()));
        return false;
    }
    overlayRect_ = primaryMonitorRect();

    // WS_EX_LAYERED + WS_EX_TRANSPARENT : la fenêtre laisse passer tous les
    // clics vers le jeu. WS_EX_NOACTIVATE : elle ne prend jamais le focus
    // tant que le menu est fermé. WS_EX_NOREDIRECTIONBITMAP : contenu fourni
    // uniquement par DirectComposition.
    DWORD ex = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE |
               WS_EX_NOREDIRECTIONBITMAP;
    hwnd_ = CreateWindowExW(ex, kWindowClass, L"Bedrock Custom Client", WS_POPUP, overlayRect_.left,
                            overlayRect_.top, overlayRect_.right - overlayRect_.left,
                            overlayRect_.bottom - overlayRect_.top, nullptr, nullptr, wc.hInstance, this);
    if (!hwnd_) {
        log::error("CreateWindowEx : {}", win32ErrorMessage(GetLastError()));
        return false;
    }
    SetLayeredWindowAttributes(hwnd_, 0, 255, LWA_ALPHA);
    dpiScale_ = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd_);
    return true;
}

bool Application::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // positions gérées par nos profils JSON
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ThemeManager::loadFonts();
    theme_->apply(dpiScale_);
    if (!ImGui_ImplWin32_Init(hwnd_) || !ImGui_ImplDX11_Init(renderer_->device(), renderer_->context())) {
        log::error("Initialisation des backends ImGui impossible");
        return false;
    }
    return true;
}

void Application::shutdown() {
    // DestroyWindow envoie encore des messages (WM_ACTIVATE...) : on les ignore.
    shuttingDown_ = true;
    if (configs_ && configs_->dirty()) configs_->save();
    if (modules_) modules_->shutdownAll();
    if (input_) input_->unregisterDevices();
    if (telemetry_) telemetry_->setPinger(nullptr);
    pinger_.reset();
    gui_.reset();
    map_.reset(); // libère ses textures avant le périphérique graphique
    if (textures_) textures_->clear();
    if (renderer_ && ImGui::GetCurrentContext()) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    if (tray_) tray_->destroy();
    if (hwnd_ && registeredMenuVk_) UnregisterHotKey(hwnd_, kMenuHotkeyId);
    registeredMenuVk_ = 0;
    gui_.reset();
    textures_.reset();
    renderer_.reset();
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (frameTimer_) {
        CloseHandle(frameTimer_);
        frameTimer_ = nullptr;
    }
    if (instanceMutex_) {
        ReleaseMutex(instanceMutex_);
        CloseHandle(instanceMutex_);
        instanceMutex_ = nullptr;
    }
    ui::setKeybindManager(nullptr);
    ui::setTheme(nullptr);
    log::info("Arrêt terminé");
    log::shutdown();
}

bool Application::registerMenuHotkey(int vk) {
    if (!hwnd_) return false;
    if (registeredMenuVk_ == vk) return true;
    if (registeredMenuVk_) UnregisterHotKey(hwnd_, kMenuHotkeyId);
    registeredMenuVk_ = 0;
    if (!vk) {
        notifications_->warning("Aucune touche de menu : utilise l'icône de la zone de notification pour l'ouvrir.");
        return false;
    }
    // RegisterHotKey est l'API documentée pour un raccourci global ; elle
    // autorise aussi l'overlay à passer au premier plan quand on l'ouvre.
    if (!RegisterHotKey(hwnd_, kMenuHotkeyId, MOD_NOREPEAT, static_cast<UINT>(vk))) {
        notifications_->error("Impossible d'utiliser " + KeybindManager::keyName(vk) +
                              " comme touche du menu (déjà prise par une autre application ?)");
        return false;
    }
    registeredMenuVk_ = vk;
    return true;
}

void Application::applyWindowStyle() {
    LONG_PTR ex = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    LONG_PTR want = ex;
    if (clickThrough_) want |= (WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    else want &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    if (want != ex) {
        SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, want);
        SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
}

// Lance le jeu via son protocole officiel « minecraft: » (enregistré par
// l'installation Microsoft Store / Xbox). Aucun fichier du jeu n'est touché.
void Application::launchMinecraft() {
    auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", L"minecraft:", nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32) {
        notifications_->error("Impossible de lancer Minecraft (protocole minecraft: non enregistré ?)");
    } else {
        notifications_->info("Lancement de Minecraft…");
        log::info("Minecraft lancé via le protocole minecraft:");
    }
}

void Application::openConfigsFolder() {
    auto dir = paths::configsDir();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void Application::setMenuOpen(bool open) {
    if (open == menuOpen_ || !gui_) return;
    menuOpen_ = open;
    gui_->setOpen(open);
    keybinds_->setSuspended(open);
    if (!open) keybinds_->cancelCapture();
    clickThrough_ = !open;
    applyWindowStyle();
    bus_->publish(MenuToggledEvent{open});
    if (open) {
        updatePlacement();
        if (!windowShown_) {
            ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
            windowShown_ = true;
        }
        if (!options_.selftest) { // l'autotest ne vole jamais le focus
            ClipCursor(nullptr); // libère la souris si le jeu la retenait
            SetForegroundWindow(hwnd_);
            SetFocus(hwnd_);
        }
    } else if (game_ && game_->state().found && !options_.selftest && GetForegroundWindow() == hwnd_) {
        SetForegroundWindow(game_->state().hwnd); // rend la main au jeu
    }
    log::info("Menu {}", open ? "ouvert" : "fermé");
}

bool Application::hudShouldBeVisible() const {
    if (options_.selftest || menuOpen_) return true;
    const GameState& g = game_->state();
    if (g.found && g.minimized) return false;
    if (client_->hudOnlyWhenGameFocused->get()) return g.found && g.foreground;
    return true;
}

bool Application::overlayShouldBeVisible() const {
    if (options_.selftest && !selftest_.done) return true;
    if (menuOpen_ || (gui_ && gui_->visible())) return true;
    if (notifications_ && !notifications_->empty()) return true;
    return modules_->enabledCount() > 0 && hudShouldBeVisible();
}

void Application::updatePlacement() {
    const GameState& g = game_->state();
    RECT target = (g.found && !g.minimized && client_->followGameWindow->get() && g.width() > 0 && g.height() > 0)
                      ? g.clientRect
                      : primaryMonitorRect();
    if (!EqualRect(&target, &overlayRect_)) {
        overlayRect_ = target;
        SetWindowPos(hwnd_, HWND_TOPMOST, target.left, target.top, target.right - target.left, target.bottom - target.top,
                     SWP_NOACTIVATE);
        log::info("Overlay positionné : {}x{} en ({}, {})", target.right - target.left, target.bottom - target.top,
                  target.left, target.top);
    }
    bool visible = overlayShouldBeVisible();
    if (visible && !windowShown_) {
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        windowShown_ = true;
    } else if (!visible && windowShown_) {
        ShowWindow(hwnd_, SW_HIDE);
        windowShown_ = false;
    }
}

void Application::tick(double now) {
    double dt = std::clamp(now - lastTick_, 0.0, 0.25);
    lastTick_ = now;
    bool gameChanged = false;
    if (now - lastGamePoll_ >= 0.1) {
        lastGamePoll_ = now;
        gameChanged = game_->poll(now);
    }
    telemetry_->setNow(now);
    telemetry_->setGameState(game_->state());
    telemetry_->setOverlayFps(static_cast<float>(perf_->fps()));
    keybinds_->setClock(now);
    keybinds_->setRequireGameFocus(client_->keybindsRequireGameFocus->get());
    modules_->updateAll(now, dt);
    configs_->setAutosave(client_->autosave->get(), client_->autosaveDelay->get());
    configs_->update(now);
    perf_->sampleProcess(now);
    if (gameChanged || now - lastPlacement_ >= 0.25) {
        lastPlacement_ = now;
        updatePlacement();
    }
    // Les jeux en plein écran peuvent repasser au-dessus : on réaffirme TOPMOST.
    if (windowShown_ && game_->state().foreground && now - lastTopmost_ >= 1.0) {
        lastTopmost_ = now;
        SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void Application::renderFrame(double now) {
    perf_->beginFrame(now);
    float dt = lastFrameTime_ > 0 ? static_cast<float>(now - lastFrameTime_) : 0.f;
    float dpi = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd_);
    theme_->apply(dpi);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    if (menuOpen_) {
        // Le menu est incliné en perspective : on convertit la souris en coordonnées « à plat ».
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hwnd_, &pt);
        ImVec2 flat;
        if (gui_->remapMouse({static_cast<float>(pt.x), static_cast<float>(pt.y)}, &flat)) io.AddMousePosEvent(flat.x, flat.y);
    }
    ImGui::NewFrame();

    ImVec2 area = io.DisplaySize;
    hud_->setEditMode(hud_->editMode() && menuOpen_);
    if (hudShouldBeVisible()) {
        hud_->drawOverlays(telemetry_.get(), {0, 0}, area, now, dt);
        hud_->draw(telemetry_.get(), {0, 0}, area, now, dt);
    }
    gui_->draw(now);
    if (menuOpen_ && ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !keybinds_->capturing() &&
        !keybinds_->recentlyCaptured(now) && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup)) {
        if (hud_->editMode()) hud_->setEditMode(false);
        else setMenuOpen(false);
    }
    notifications_->draw(now);
    if (options_.selftest && selftest_.drawMarker) {
        // Marqueur opaque magenta : sert à vérifier que DWM affiche bien l'overlay.
        ImGui::GetForegroundDrawList()->AddRectFilled({0, 0}, {24, 24}, IM_COL32(255, 0, 255, 255));
    }

    ImGui::Render();
    gui_->applyPerspective();
    if (renderer_->beginFrame()) ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    if (options_.selftest) selftestFrame(now);
    perf_->endFrameWork(nowSeconds());
    if (!renderer_->present(true)) {
        notifications_->error("Périphérique graphique perdu — redémarrez l'overlay.");
        running_ = false;
    }
    lastFrameTime_ = now;
    int limit = client_->fpsLimit->get();
    nextFrameTime_ = limit > 0 ? std::max(nextFrameTime_ + 1.0 / limit, now + 0.5 / limit) : 0;
}

bool Application::waitForNextFrame() {
    // Limite de FPS facultative : minuterie haute résolution, pas d'attente active.
    double now = nowSeconds();
    if (nextFrameTime_ > now && frameTimer_) {
        LARGE_INTEGER due;
        due.QuadPart = -static_cast<LONGLONG>((nextFrameTime_ - now) * 1e7);
        SetWaitableTimer(frameTimer_, &due, 0, nullptr, nullptr, FALSE);
        DWORD r = MsgWaitForMultipleObjects(1, &frameTimer_, FALSE, 100, QS_ALLINPUT);
        if (r != WAIT_OBJECT_0) return false;
    }
    // Attente de la swapchain (cadencée sur l'écran), réveillée par les messages.
    HANDLE waitable = renderer_->frameWaitable();
    DWORD r = MsgWaitForMultipleObjects(waitable ? 1 : 0, waitable ? &waitable : nullptr, FALSE, 100, QS_ALLINPUT);
    return !waitable || r == WAIT_OBJECT_0;
}

void Application::selftestFrame(double now) {
    double elapsed = now - startTime_;
    if (elapsed > options_.selftestSeconds - 0.6) selftest_.drawMarker = true;

    if (!selftest_.compositionChecked && elapsed > options_.selftestSeconds - 0.2) {
        selftest_.compositionChecked = true;
        // Lecture d'UN pixel de l'écran composé, au centre du marqueur.
        POINT pt{overlayRect_.left + 12, overlayRect_.top + 12};
        HDC screen = GetDC(nullptr);
        HDC mem = CreateCompatibleDC(screen);
        HBITMAP bmp = CreateCompatibleBitmap(screen, 1, 1);
        HGDIOBJ old = SelectObject(mem, bmp);
        BitBlt(mem, 0, 0, 1, 1, screen, pt.x, pt.y, SRCCOPY | CAPTUREBLT);
        COLORREF c = GetPixel(mem, 0, 0);
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        ReleaseDC(nullptr, screen);
        selftest_.screenPixel = c;
        selftest_.compositionOk = GetRValue(c) > 230 && GetGValue(c) < 25 && GetBValue(c) > 230;

        // Test de « click-through » : quelle fenêtre reçoit un clic au centre ?
        POINT center{(overlayRect_.left + overlayRect_.right) / 2, (overlayRect_.top + overlayRect_.bottom) / 2};
        HWND hit = WindowFromPoint(center);
        selftest_.hitTestTargetsOverlay = hit == hwnd_;
        selftest_.clickThroughOk = menuOpen_ ? (hit == hwnd_) : (hit != hwnd_);
    }

    if (!selftest_.done && elapsed > options_.selftestSeconds) {
        selftest_.done = true;
        if (!options_.screenshot.empty())
            selftest_.screenshotOk = renderer_->captureBackBuffer(options_.screenshot, &selftest_.opaquePixels);
        writeSelftestReport();
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    }
}

void Application::writeSelftestReport() {
    Json r;
    r["version"] = BCC_VERSION;
    r["imgui"] = IMGUI_VERSION;
    r["adapter"] = renderer_ ? renderer_->adapterName() : "";
    r["overlay_rect"] = {overlayRect_.left, overlayRect_.top, overlayRect_.right, overlayRect_.bottom};
    r["dpi_scale"] = dpiScale_;
    r["menu_open"] = menuOpen_;
    if (perf_) {
        r["frames"] = perf_->frames();
        r["fps"] = perf_->fps();
        r["frame_cpu_ms_avg"] = perf_->avgFrameMs();
        r["frame_cpu_ms_p99"] = perf_->p99FrameMs();
        r["frame_cpu_ms_max"] = perf_->maxFrameMs();
        r["process_cpu_percent_one_core"] = perf_->cpuPercent();
        r["process_cpu_percent_all_cores"] = perf_->cpuPercentAllCores();
        r["working_set_mb"] = perf_->workingSetMb();
        r["peak_working_set_mb"] = perf_->peakWorkingSetMb();
        r["private_mb"] = perf_->privateMb();
    }
    r["composition_checked"] = selftest_.compositionChecked;
    r["composition_ok"] = selftest_.compositionOk;
    r["screen_pixel_rgb"] = {GetRValue(selftest_.screenPixel), GetGValue(selftest_.screenPixel), GetBValue(selftest_.screenPixel)};
    r["hit_test_targets_overlay"] = selftest_.hitTestTargetsOverlay;
    r["click_through_ok"] = selftest_.clickThroughOk;
    r["screenshot_ok"] = selftest_.screenshotOk;
    r["screenshot_opaque_pixels"] = selftest_.opaquePixels;
    r["raw_input_registered"] = selftest_.rawInputOk;
    r["raw_input_events"] = input_ ? input_->eventsProcessed() : 0;
    if (game_) {
        const GameState& g = game_->state();
        r["minecraft_found"] = g.found;
        r["minecraft_detection"] = g.detection;
        r["minecraft_foreground"] = g.foreground;
        r["minecraft_client_rect"] = {g.clientRect.left, g.clientRect.top, g.clientRect.right, g.clientRect.bottom};
    }
    if (modules_) {
        r["modules_registered"] = modules_->all().size();
        r["modules_enabled"] = modules_->enabledCount();
    }
    r["profile"] = configs_ ? configs_->activeProfile() : "";
    r["profile_status"] = selftest_.profileStatus;
    r["error"] = selftest_.error;
    std::string text = r.dump(2);
    log::info("Rapport d'autotest :\n{}", text);
    if (!options_.report.empty()) {
        std::ofstream f(options_.report, std::ios::binary);
        f << text;
    }
}

int Application::run() {
    if (!init()) {
        if (options_.selftest) writeSelftestReport();
        return 1;
    }
    MSG msg;
    while (running_) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running_ = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running_) break;
        tick(nowSeconds());
        if (!windowShown_) {
            // Rien à afficher : on dort jusqu'au prochain message (au plus 100 ms).
            MsgWaitForMultipleObjects(0, nullptr, FALSE, 100, QS_ALLINPUT);
            continue;
        }
        if (!waitForNextFrame()) continue; // message ou délai : on pompe d'abord
        renderFrame(nowSeconds());
    }
    return 0;
}

LRESULT CALLBACK Application::wndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Application* app = nullptr;
    if (msg == WM_NCCREATE) {
        app = static_cast<Application*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    } else {
        app = reinterpret_cast<Application*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    // Une exception C++ ne doit jamais traverser un callback Win32.
    if (app) {
        try {
            return app->wndProc(hwnd, msg, wParam, lParam);
        } catch (const std::exception& e) {
            log::error("Exception dans la procédure de fenêtre (message 0x{:04X}) : {}", msg, e.what());
        } catch (...) {
            log::error("Exception inconnue dans la procédure de fenêtre (message 0x{:04X})", msg);
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT Application::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (shuttingDown_) return DefWindowProcW(hwnd, msg, wParam, lParam);
    if (msg == WM_INPUT) {
        if (input_) input_->handleRawInput(lParam, nowSeconds());
        return DefWindowProcW(hwnd, msg, wParam, lParam); // requis pour libérer le message
    }
    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return 1;
    if (tray_) tray_->handleTaskbarCreated(msg);

    switch (msg) {
    case WM_HOTKEY:
        // Pendant la capture d'un raccourci, la touche est gérée par le KeybindManager.
        if (wParam == kMenuHotkeyId && !(keybinds_ && keybinds_->capturing())) setMenuOpen(!menuOpen_);
        return 0;
    case WM_SIZE:
        if (renderer_) renderer_->resize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_ACTIVATE:
        // Le menu se ferme si l'utilisateur active une fenêtre d'un autre
        // processus (sinon l'overlay bloquerait les clics sur le jeu).
        if (LOWORD(wParam) == WA_INACTIVE && menuOpen_ && !options_.selftest) {
            HWND other = reinterpret_cast<HWND>(lParam);
            DWORD pid = 0;
            if (other) GetWindowThreadProcessId(other, &pid);
            if (pid != GetCurrentProcessId()) setMenuOpen(false);
        }
        break;
    case WM_DPICHANGED:
        dpiScale_ = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
        return 0;
    case TrayIcon::kCallbackMessage:
        switch (tray_->handleMessage(lParam)) {
        case TrayIcon::CmdOpenMenu: setMenuOpen(true); break;
        case TrayIcon::CmdLaunchMinecraft: launchMinecraft(); break;
        case TrayIcon::CmdOpenConfigs: openConfigsFolder(); break;
        case TrayIcon::CmdQuit: PostMessageW(hwnd, WM_CLOSE, 0, 0); break;
        default: break;
        }
        return 0;
    case WM_CLOSE:
        running_ = false;
        return 0;
    case WM_DESTROY:
        return 0;
    default: break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace bcc
