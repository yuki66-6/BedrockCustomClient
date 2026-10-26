#include "core/EventBus.h"

#include <algorithm>

namespace bcc {

void EventBus::unsubscribe(Token token) {
    for (auto& [type, list] : handlers_)
        std::erase_if(list, [token](const Handler& h) { return h.token == token; });
}

bool EventBus::isSubscribed(Token token) const {
    for (auto& [type, list] : handlers_)
        for (auto& h : list)
            if (h.token == token) return true;
    return false;
}

size_t EventBus::subscriberCount() const {
    size_t n = 0;
    for (auto& [type, list] : handlers_) n += list.size();
    return n;
}

} // namespace bcc
