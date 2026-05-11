#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace runtime::entity {

template <typename Key, typename State>
class EntityStateStore {
public:
    std::shared_ptr<State> find_or_create(const Key& key) {
        return find_or_create(key, [](State&) {});
    }

    template <typename Initializer>
    std::shared_ptr<State> find_or_create(
        const Key& key,
        Initializer initializer) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& state = states_[key];
        if (state == nullptr) {
            state = std::make_shared<State>();
            initializer(*state);
        }
        return state;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return states_.size();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<Key, std::shared_ptr<State>> states_;
};

}  // namespace runtime::entity
