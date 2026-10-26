#include "integration/GameIntegration.h"

#include "utils/Log.h"
#include "utils/Strings.h"

#include <algorithm>
#include <cwctype>

namespace bcc {
namespace {

std::wstring processPath(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return {};
    wchar_t buf[MAX_PATH * 2];
    DWORD size = static_cast<DWORD>(std::size(buf));
    std::wstring out;
    if (QueryFullProcessImageNameW(h, 0, buf, &size)) out.assign(buf, size);
    CloseHandle(h);
    return out;
}

struct SearchCtx {
    HWND overlay;
    HWND byExe = nullptr;
    HWND byTitle = nullptr;
};

BOOL CALLBACK enumProc(HWND hwnd, LPARAM lp) {
    auto* ctx = reinterpret_cast<SearchCtx*>(lp);
    if (hwnd == ctx->overlay || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER)) return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId()) return TRUE;
    if (GameIntegration::isMinecraftExe(processPath(pid))) {
        ctx->byExe = hwnd;
        return FALSE;
    }
    wchar_t title[64];
    if (!ctx->byTitle && GetWindowTextW(hwnd, title, 64) && wcscmp(title, L"Minecraft") == 0) ctx->byTitle = hwnd;
    return TRUE;
}

} // namespace

bool GameIntegration::isMinecraftExe(const std::wstring& path) {
    if (path.empty()) return false;
    auto slash = path.find_last_of(L"\\/");
    std::wstring name = path.substr(slash == std::wstring::npos ? 0 : slash + 1);
    std::transform(name.begin(), name.end(), name.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return name == L"minecraft.windows.exe";
}

bool GameIntegration::validate(HWND hwnd) const {
    if (!hwnd || !IsWindow(hwnd)) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return pid == state_.pid;
}

HWND GameIntegration::find() {
    SearchCtx ctx{overlay_};
    EnumWindows(&enumProc, reinterpret_cast<LPARAM>(&ctx));
    HWND hwnd = ctx.byExe ? ctx.byExe : ctx.byTitle;
    if (hwnd) {
        GetWindowThreadProcessId(hwnd, &state_.pid);
        state_.detection = ctx.byExe ? "exe" : "titre";
    }
    return hwnd;
}

bool GameIntegration::isGameForeground() const {
    return state_.found && GetForegroundWindow() == state_.hwnd;
}

bool GameIntegration::poll(double now) {
    GameState before = state_;
    if (!validate(state_.hwnd)) {
        state_.hwnd = nullptr;
        state_.found = false;
        if (now - lastSearch_ >= 1.5) {
            lastSearch_ = now;
            state_.hwnd = find();
            state_.found = state_.hwnd != nullptr;
            if (state_.found) log::info("Fenêtre Minecraft détectée (pid {}, par {})", state_.pid, state_.detection);
        }
        if (before.found && !state_.found) log::info("Fenêtre Minecraft perdue");
    }
    if (state_.found) {
        state_.minimized = IsIconic(state_.hwnd) != FALSE;
        state_.foreground = GetForegroundWindow() == state_.hwnd;
        RECT rc{};
        GetClientRect(state_.hwnd, &rc);
        POINT tl{rc.left, rc.top}, br{rc.right, rc.bottom};
        ClientToScreen(state_.hwnd, &tl);
        ClientToScreen(state_.hwnd, &br);
        state_.clientRect = {tl.x, tl.y, br.x, br.y};
    } else {
        state_.foreground = state_.minimized = false;
        state_.clientRect = {};
        state_.pid = 0;
    }
    return before.found != state_.found || !EqualRect(&before.clientRect, &state_.clientRect) ||
           before.minimized != state_.minimized;
}

} // namespace bcc
