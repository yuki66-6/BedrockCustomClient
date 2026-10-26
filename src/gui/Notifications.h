#pragma once

#include <deque>
#include <string>

namespace bcc {

// Petites notifications (toasts) en bas à droite de l'overlay.
class Notifications {
public:
    enum class Kind { Info, Success, Warning, Error };

    void push(Kind kind, std::string text, double durationSec = 3.5);
    void info(std::string text) { push(Kind::Info, std::move(text)); }
    void success(std::string text) { push(Kind::Success, std::move(text)); }
    void warning(std::string text) { push(Kind::Warning, std::move(text), 5.0); }
    void error(std::string text) { push(Kind::Error, std::move(text), 7.0); }

    void draw(double now);
    bool empty() const { return items_.empty(); }
    size_t size() const { return items_.size(); }

private:
    struct Item {
        Kind kind;
        std::string text;
        double duration;
        double start = -1; // fixé à la première image affichée
    };
    std::deque<Item> items_;
};

} // namespace bcc
