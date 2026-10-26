#include "integration/InputState.h"

#include <algorithm>

namespace bcc {

void CpsCounter::add(double t) {
    clicks_.push_back(t);
    ++total_;
    count(t);
}

int CpsCounter::count(double now) {
    while (!clicks_.empty() && now - clicks_.front() >= 1.0) clicks_.pop_front();
    // Horloge non monotone (tests) : on ignore les clics « du futur ».
    int n = static_cast<int>(std::count_if(clicks_.begin(), clicks_.end(), [now](double t) { return t <= now; }));
    peak_ = std::max(peak_, n);
    return n;
}

void CpsCounter::reset() {
    clicks_.clear();
    peak_ = 0;
    total_ = 0;
}

bool InputState::onKey(int vk, bool down, double, bool) {
    if (vk <= 0 || vk >= 256) return false;
    bool& state = keys_[static_cast<size_t>(vk)];
    bool repeat = down && state;
    if (down && !state) ++keyPresses_;
    state = down;
    return repeat;
}

void InputState::onMouseButton(MouseButton button, bool down, double t, bool gameFocused) {
    size_t i = static_cast<size_t>(button);
    if (i >= mouse_.size()) return;
    bool wasDown = mouse_[i];
    mouse_[i] = down;
    if (down && !wasDown && (gameFocused || !countOnlyInGame_)) {
        if (button == MouseButton::Left) left_.add(t);
        else if (button == MouseButton::Right) right_.add(t);
    }
}

void InputState::releaseAll() {
    keys_.fill(false);
    mouse_.fill(false);
}

void InputState::resetStats() {
    left_.reset();
    right_.reset();
    keyPresses_ = 0;
}

} // namespace bcc
