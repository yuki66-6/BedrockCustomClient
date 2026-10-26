#pragma once
#include <cstdint>
namespace bcc::native {
// Windows VK_RSHIFT = 0xA1. Transition sur l'appui, pas sur la répétition.
inline constexpr std::uint32_t kRightShiftVk = 0xA1;
class ClickGuiHotkey {
public:
    bool update(bool rightShiftDown, bool gameFocused) noexcept {
        if (!gameFocused) { previousDown_ = false; return false; }
        const bool pressed = rightShiftDown && !previousDown_;
        previousDown_ = rightShiftDown;
        if (pressed) { open_ = !open_; return true; }
        return false;
    }
    bool isOpen() const noexcept { return open_; }
    void close() noexcept { open_ = false; }
private:
    bool previousDown_ = false;
    bool open_ = false;
};
} // namespace bcc::native
