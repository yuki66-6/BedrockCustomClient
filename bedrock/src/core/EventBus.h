#pragma once

#include <cstdint>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace bcc {

// Bus d'événements synchrone, utilisé uniquement depuis le thread de l'interface.
// Les abonnés peuvent s'abonner/se désabonner pendant une publication.
class EventBus {
public:
    using Token = uint64_t;

    template <class E>
    Token subscribe(std::function<void(const E&)> handler) {
        Token token = next_++;
        handlers_[std::type_index(typeid(E))].push_back(
            {token, [h = std::move(handler)](const void* e) { h(*static_cast<const E*>(e)); }});
        return token;
    }

    void unsubscribe(Token token);

    template <class E>
    void publish(const E& event) {
        auto it = handlers_.find(std::type_index(typeid(E)));
        if (it == handlers_.end()) return;
        auto snapshot = it->second; // copie : tolère (dés)abonnements pendant l'envoi
        for (auto& h : snapshot)
            if (isSubscribed(h.token)) h.fn(&event);
    }

    size_t subscriberCount() const;

private:
    struct Handler {
        Token token;
        std::function<void(const void*)> fn;
    };
    bool isSubscribed(Token token) const;

    std::unordered_map<std::type_index, std::vector<Handler>> handlers_;
    Token next_ = 1;
};

// RAII : désabonne automatiquement à la destruction.
class Subscription {
public:
    Subscription() = default;
    Subscription(EventBus& bus, EventBus::Token token) : bus_(&bus), token_(token) {}
    Subscription(Subscription&& o) noexcept : bus_(o.bus_), token_(o.token_) { o.bus_ = nullptr; }
    Subscription& operator=(Subscription&& o) noexcept {
        if (this != &o) {
            reset();
            bus_ = o.bus_;
            token_ = o.token_;
            o.bus_ = nullptr;
        }
        return *this;
    }
    ~Subscription() { reset(); }
    void reset() {
        if (bus_) bus_->unsubscribe(token_);
        bus_ = nullptr;
    }

private:
    EventBus* bus_ = nullptr;
    EventBus::Token token_ = 0;
};

} // namespace bcc
