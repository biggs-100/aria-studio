#ifndef ARIA_SERVICE_LOCATOR_H
#define ARIA_SERVICE_LOCATOR_H

#include <memory>
#include <string>
#include <unordered_map>

namespace aria {

/// Service locator — explicit service registration and lookup.
class ServiceLocator {
public:
    ServiceLocator() = default;
    ~ServiceLocator() = default;

    /// Register a service by type.
    template<typename T>
    void register_service(std::unique_ptr<T> service);

    /// Retrieve a registered service.
    template<typename T>
    T* get_service() const;

    /// Check if a service is registered.
    template<typename T>
    bool has_service() const;

    /// Remove a service.
    template<typename T>
    void remove_service();

private:
    std::unordered_map<std::string, std::unique_ptr<void, void(*)(void*)>> services_;
};

} // namespace aria

#endif // ARIA_SERVICE_LOCATOR_H
