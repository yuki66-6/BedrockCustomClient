#include "integration/InputMonitor.h"

#include "core/EventBus.h"
#include "core/Events.h"
#include "integration/InputState.h"
#include "utils/Log.h"
#include "utils/Strings.h"

namespace bcc {

bool InputMonitor::registerDevices(HWND target) {
    RAWINPUTDEVICE devices[2] = {};
    devices[0].usUsagePage = 0x01; // Generic Desktop
    devices[0].usUsage = 0x06;     // Keyboard
    devices[0].dwFlags = RIDEV_INPUTSINK;
    devices[0].hwndTarget = target;
    devices[1].usUsagePage = 0x01;
    devices[1].usUsage = 0x02; // Mouse
    devices[1].dwFlags = RIDEV_INPUTSINK;
    devices[1].hwndTarget = target;
    registered_ = RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE)) != FALSE;
    if (!registered_) log::error("RegisterRawInputDevices : {}", win32ErrorMessage(GetLastError()));
    return registered_;
}

void InputMonitor::unregisterDevices() {
    if (!registered_) return;
    RAWINPUTDEVICE devices[2] = {};
    devices[0] = {0x01, 0x06, RIDEV_REMOVE, nullptr};
    devices[1] = {0x01, 0x02, RIDEV_REMOVE, nullptr};
    RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE));
    registered_ = false;
}

int InputMonitor::normalizeVk(int vk, unsigned short makeCode, bool e0) {
    switch (vk) {
    case VK_SHIFT: return makeCode == 0x36 ? VK_RSHIFT : VK_LSHIFT;
    case VK_CONTROL: return e0 ? VK_RCONTROL : VK_LCONTROL;
    case VK_MENU: return e0 ? VK_RMENU : VK_LMENU;
    default: return vk;
    }
}

void InputMonitor::handleRawInput(LPARAM lParam, double now) {
    RAWINPUT raw;
    UINT size = sizeof(raw);
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1))
        return;
    bool game = gameFocused_ && gameFocused_();
    bool overlay = overlayFocused_ && overlayFocused_();

    if (raw.header.dwType == RIM_TYPEKEYBOARD) {
        const RAWKEYBOARD& kb = raw.data.keyboard;
        if (kb.VKey == 0 || kb.VKey >= 255) return; // séquences factices
        ++events_;
        bool down = !(kb.Flags & RI_KEY_BREAK);
        int vk = normalizeVk(kb.VKey, kb.MakeCode, (kb.Flags & RI_KEY_E0) != 0);
        bool repeat = state_.onKey(vk, down, now, game);
        bus_.publish(KeyEvent{vk, down, repeat, game, overlay});
    } else if (raw.header.dwType == RIM_TYPEMOUSE) {
        USHORT f = raw.data.mouse.usButtonFlags;
        if (!f) return; // simple déplacement : ignoré (coût minimal)
        ++events_;
        struct Map {
            USHORT downFlag, upFlag;
            MouseButton button;
        };
        static constexpr Map maps[] = {
            {RI_MOUSE_LEFT_BUTTON_DOWN, RI_MOUSE_LEFT_BUTTON_UP, MouseButton::Left},
            {RI_MOUSE_RIGHT_BUTTON_DOWN, RI_MOUSE_RIGHT_BUTTON_UP, MouseButton::Right},
            {RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP, MouseButton::Middle},
            {RI_MOUSE_BUTTON_4_DOWN, RI_MOUSE_BUTTON_4_UP, MouseButton::X1},
            {RI_MOUSE_BUTTON_5_DOWN, RI_MOUSE_BUTTON_5_UP, MouseButton::X2},
        };
        for (auto& m : maps) {
            if (f & m.downFlag) {
                state_.onMouseButton(m.button, true, now, game);
                bus_.publish(MouseButtonEvent{m.button, true, game, now});
            }
            if (f & m.upFlag) {
                state_.onMouseButton(m.button, false, now, game);
                bus_.publish(MouseButtonEvent{m.button, false, game, now});
            }
        }
    }
}

} // namespace bcc
