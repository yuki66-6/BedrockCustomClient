#include <windows.h>
#include "BedrockBridge.h"
#include "ClickGuiHotkey.h"
namespace {
  bcc::native::ClickGuiHotkey menu;
}
extern "C" __declspec(dllexport) unsigned BccNativeApiVersion() noexcept { return 1; }
extern "C" __declspec(dllexport) bool BccMenuKeyUpdate(bool rightShiftDown, bool gameFocused) noexcept {
  return menu.update(rightShiftDown, gameFocused);
}
extern "C" __declspec(dllexport) bool BccMenuOpen() noexcept { return menu.isOpen(); }
// Never initialize rendering, input hooks or threads inside DllMain.
BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
