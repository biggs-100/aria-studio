#ifndef ARIA_SERVICE_LOCATOR_H
#define ARIA_SERVICE_LOCATOR_H

#include <functional>
#include <memory>
#include <string>
#include <typeinfo>
#include <unordered_map>

namespace aria {

/// Service locator — explicit service registration and lookup.
class ServiceLocator {
public:
    ServiceLocator() = default;
    ~ServiceLocator() = default;

    /// Register a service by type.
    template<typename T>
    void register_service(std::unique_ptr<T> service) {
        const std::string key = typeid(T).name();
        auto* raw = service.release();
        entries_[key] = {raw, [](void* ptr) { delete static_cast<T*>(ptr); }};
    }

    /// Retrieve a registered service.
    template<typename T>
    T* get_service() const {
        const std::string key = typeid(T).name();
        auto it = entries_.find(key);
        return (it != entries_.end()) ? static_cast<T*>(it->second.ptr) : nullptr;
    }

    /// Check if a service is registered.
    template<typename T>
    bool has_service() const {
        const std::string key = typeid(T).name();
        return entries_.find(key) != entries_.end();
    }

    /// Remove a service.
    template<typename T>
    void remove_service() {
        const std::string key = typeid(T).name();
        entries_.erase(key);
    }

private:
    struct Entry {
        void* ptr = nullptr;
        std::function<void(void*)> deleter;
        ~Entry() { if (ptr && deleter) deleter(ptr); }
        Entry() = default;
        Entry(void* p, std::function<void(void*)> d) : ptr(p), deleter(std::move(d)) {}
        Entry(Entry&& other) noexcept
            : ptr(other.ptr), deleter(std::move(other.deleter)) {
            other.ptr = nullptr;
        }
        Entry& operator=(Entry&& other) noexcept {
            if (this != &other) {
                if (ptr && deleter) deleter(ptr);
                ptr = other.ptr;
                deleter = std::move(other.deleter);
                other.ptr = nullptr;
            }
            return *this;
        }
        Entry(const Entry&) = delete;
        Entry& operator=(const Entry&) = delete;
    };

    std::unordered_map<std::string, Entry> entries_;
};

} // namespace aria

#endif // ARIA_SERVICE_LOCATOR_H
