#pragma once

#include "modules/Module.h"

#include <windows.h>

#include <filesystem>
#include <memory>
#include <string>

namespace bcc {

class OverlayRenderer;
class ThemeManager;
class Notifications;
class PerfStats;
class ClickGUI;
class TrayIcon;
class EventBus;
class TelemetryProvider;
class ServerPinger;
class GameIntegration;
class InputMonitor;
class ModuleManager;
class KeybindManager;
class ClientSettings;
class HudManager;
class ConfigManager;
class TextureManager;
class MapPage;

struct AppOptions {
    bool selftest = false;            // exécution automatique puis rapport
    double selftestSeconds = 5.0;
    bool openMenu = false;            // ouvrir la ClickGUI au démarrage
    bool launchMinecraft = false;     // lancer le jeu (protocole minecraft:) au démarrage
    std::filesystem::path screenshot; // BMP du back buffer en fin d'autotest
    std::filesystem::path report;     // rapport JSON de l'autotest
    std::filesystem::path dataDir;    // dossier de données alternatif
};

class Application {
public:
    explicit Application(AppOptions options);
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run();

private:
    bool init();
    void shutdown();
    bool createWindow();
    bool initImGui();
    void setupConfigs();
    void tick(double now);
    void renderFrame(double now);
    void setMenuOpen(bool open);
    void updatePlacement();
    void applyWindowStyle();
    void launchMinecraft();
    void openConfigsFolder();
    bool registerMenuHotkey(int vk);
    bool overlayShouldBeVisible() const;
    bool hudShouldBeVisible() const;
    bool waitForNextFrame();
    void selftestFrame(double now);
    void writeSelftestReport();

    static LRESULT CALLBACK wndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    AppOptions options_;
    HWND hwnd_ = nullptr;
    HANDLE instanceMutex_ = nullptr;
    HANDLE frameTimer_ = nullptr;
    bool running_ = true;
    bool shuttingDown_ = false;
    bool menuOpen_ = false;
    bool windowShown_ = false;
    bool clickThrough_ = true;
    int registeredMenuVk_ = 0;
    RECT overlayRect_{};
    float dpiScale_ = 1.f;
    double startTime_ = 0;
    double lastTick_ = 0;
    double lastGamePoll_ = -1;
    double lastPlacement_ = -1;
    double lastTopmost_ = -1;
    double lastFrameTime_ = 0;
    double nextFrameTime_ = 0;

    std::unique_ptr<EventBus> bus_;
    std::unique_ptr<TelemetryProvider> telemetry_;
    std::unique_ptr<ServerPinger> pinger_;
    std::unique_ptr<GameIntegration> game_;
    std::unique_ptr<InputMonitor> input_;
    std::unique_ptr<ClientSettings> client_;
    std::unique_ptr<ModuleManager> modules_;
    std::unique_ptr<KeybindManager> keybinds_;
    std::unique_ptr<HudManager> hud_;
    std::unique_ptr<ConfigManager> configs_;
    std::unique_ptr<TextureManager> textures_;
    std::unique_ptr<OverlayRenderer> renderer_;
    std::unique_ptr<ThemeManager> theme_;
    std::unique_ptr<Notifications> notifications_;
    std::unique_ptr<PerfStats> perf_;
    std::unique_ptr<MapPage> map_;
    std::unique_ptr<ClickGUI> gui_;
    std::unique_ptr<TrayIcon> tray_;
    ModuleContext moduleCtx_;

    // Autotest
    struct SelftestState {
        bool done = false;
        bool drawMarker = false;
        bool clickThroughOk = false;
        bool hitTestTargetsOverlay = false;
        bool compositionChecked = false;
        bool compositionOk = false;
        unsigned screenPixel = 0;
        unsigned opaquePixels = 0;
        bool screenshotOk = false;
        bool rawInputOk = false;
        std::string profileStatus;
        std::string error;
    } selftest_;
};

} // namespace bcc
