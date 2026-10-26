#include <windows.h>
#include <shellapi.h>
#include <string>

namespace {
constexpr wchar_t kClass[] = L"BccNativeLauncherWindow";
HWND button = nullptr;
void launchBedrock(HWND hwnd) {
  // The Minecraft URI asks Windows to launch the installed edition.
  HINSTANCE rc = ShellExecuteW(hwnd, L"open", L"minecraft://", nullptr, nullptr, SW_SHOWNORMAL);
  if (reinterpret_cast<INT_PTR>(rc) <= 32) {
    MessageBoxW(hwnd, L"Impossible d'ouvrir minecraft://. Verifiez l'installation Bedrock.", L"Bedrock Launcher", MB_ICONERROR);
  }
}
LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
  switch (message) {
    case WM_CREATE:
      CreateWindowW(L"STATIC", L"Bedrock Native Client - developpement", WS_VISIBLE | WS_CHILD,
          22, 20, 350, 25, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
      button = CreateWindowW(L"BUTTON", L"Lancer Minecraft Bedrock", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
          22, 66, 350, 42, hwnd, reinterpret_cast<HMENU>(1001), GetModuleHandleW(nullptr), nullptr);
      CreateWindowW(L"STATIC", L"La DLL native n'est pas injectee automatiquement.\nLe ClickGUI et les modules ne sont pas connectes au jeu.", WS_VISIBLE | WS_CHILD,
          22, 126, 370, 60, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
      return 0;
    case WM_COMMAND:
      if (LOWORD(wp) == 1001) launchBedrock(hwnd);
      return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
  }
  return DefWindowProcW(hwnd, message, wp, lp);
}
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = windowProc;
  wc.hInstance = instance;
  wc.lpszClassName = kClass;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  if (!RegisterClassExW(&wc)) return 1;
  HWND hwnd = CreateWindowExW(0, kClass, L"Bedrock Native Launcher", WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
      CW_USEDEFAULT, CW_USEDEFAULT, 420, 270, nullptr, nullptr, instance, nullptr);
  if (!hwnd) return 2;
  ShowWindow(hwnd, show);
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
  return static_cast<int>(msg.wParam);
}
