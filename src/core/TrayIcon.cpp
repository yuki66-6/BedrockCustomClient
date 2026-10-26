#include "core/TrayIcon.h"

#include <shellapi.h>

#include <cwchar>

namespace bcc {

bool TrayIcon::create(HWND owner, const wchar_t* tooltip) {
    owner_ = owner;
    wcsncpy_s(tooltip_, tooltip, _TRUNCATE);
    taskbarCreatedMsg_ = RegisterWindowMessageW(L"TaskbarCreated");

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = owner_;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = kCallbackMessage;
    nid.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
    if (!nid.hIcon) nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcsncpy_s(nid.szTip, tooltip_, _TRUNCATE);
    created_ = Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
    return created_;
}

void TrayIcon::destroy() {
    if (!created_) return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = owner_;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    created_ = false;
}

void TrayIcon::handleTaskbarCreated(UINT msg) {
    if (taskbarCreatedMsg_ && msg == taskbarCreatedMsg_) {
        created_ = false;
        create(owner_, tooltip_);
    }
}

UINT TrayIcon::handleMessage(LPARAM lParam) {
    UINT event = LOWORD(lParam);
    if (event == WM_LBUTTONDBLCLK) return CmdOpenMenu;
    if (event != WM_RBUTTONUP && event != WM_CONTEXTMENU) return 0;

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, CmdOpenMenu, L"Ouvrir le menu");
    AppendMenuW(menu, MF_STRING, CmdLaunchMinecraft, L"Lancer Minecraft");
    AppendMenuW(menu, MF_STRING, CmdOpenConfigs, L"Ouvrir le dossier des profils");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CmdQuit, L"Quitter Bedrock Custom Client");
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(owner_); // requis pour que le menu se ferme correctement
    UINT cmd = static_cast<UINT>(
        TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, owner_, nullptr));
    PostMessageW(owner_, WM_NULL, 0, 0);
    DestroyMenu(menu);
    return cmd;
}

} // namespace bcc
