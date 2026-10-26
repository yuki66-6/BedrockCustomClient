#include "core/Application.h"
#include "utils/Strings.h"

#include <windows.h>
#include <shellapi.h>

#include <string>

// Arguments :
//   --selftest [secondes]   lance, mesure, vérifie la composition puis quitte
//   --launch-minecraft      lance Minecraft (protocole minecraft:) au démarrage
//   --open-menu             ouvre la ClickGUI au démarrage
//   --screenshot <fichier>  BMP du rendu de l'overlay (autotest)
//   --report <fichier>      rapport JSON (autotest)
//   --data-dir <dossier>    dossier de configs/logs alternatif
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    bcc::AppOptions options;
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        auto next = [&]() -> std::wstring { return i + 1 < argc ? argv[++i] : L""; };
        if (a == L"--selftest") {
            options.selftest = true;
            if (i + 1 < argc && iswdigit(argv[i + 1][0])) options.selftestSeconds = _wtof(argv[++i]);
        } else if (a == L"--launch-minecraft") {
            options.launchMinecraft = true;
        } else if (a == L"--open-menu") {
            options.openMenu = true;
        } else if (a == L"--screenshot") {
            options.screenshot = next();
        } else if (a == L"--report") {
            options.report = next();
        } else if (a == L"--data-dir") {
            options.dataDir = next();
        }
    }
    LocalFree(argv);

    bcc::Application app(std::move(options));
    return app.run();
}
