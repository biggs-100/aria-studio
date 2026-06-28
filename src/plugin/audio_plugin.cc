#include "audio_plugin.h"

#include <algorithm>
#include <shared_mutex>
#include <unordered_map>

namespace aria::plugin {

// ── ParameterManager ──────────────────────────────────────────────

ParamID ParameterManager::register_parameter(const ParameterInfo& info) {
    std::unique_lock lock(mutex_);
    ParamID id = next_id_++;
    auto& param = params_[id];
    param.info = info;
    param.info.id = id;
    param.value.store(info.default_value, std::memory_order_relaxed);
    param.editing = false;
    return id;
}

ParameterInfo ParameterManager::parameter_info(ParamID id) const {
    std::shared_lock lock(mutex_);
    auto it = params_.find(id);
    if (it != params_.end()) {
        return it->second.info;
    }
    return ParameterInfo{};
}

double ParameterManager::get_value(ParamID id) const {
    std::shared_lock lock(mutex_);
    auto it = params_.find(id);
    if (it != params_.end()) {
        return it->second.value.load(std::memory_order_relaxed);
    }
    return 0.0;
}

void ParameterManager::set_value(ParamID id, double value) {
    std::shared_lock lock(mutex_);
    auto it = params_.find(id);
    if (it != params_.end()) {
        // Clamp to valid range
        const auto& info = it->second.info;
        double clamped = std::clamp(value, info.min_value, info.max_value);
        it->second.value.store(clamped, std::memory_order_relaxed);
    }
}

void ParameterManager::begin_edit(ParamID id) {
    std::shared_lock lock(mutex_);
    auto it = params_.find(id);
    if (it != params_.end()) {
        it->second.editing = true;
    }
}

void ParameterManager::perform_edit(ParamID id, double value) {
    std::shared_lock lock(mutex_);
    auto it = params_.find(id);
    if (it != params_.end() && it->second.editing) {
        const auto& info = it->second.info;
        double clamped = std::clamp(value, info.min_value, info.max_value);
        it->second.value.store(clamped, std::memory_order_relaxed);
    }
}

void ParameterManager::end_edit(ParamID id) {
    std::shared_lock lock(mutex_);
    auto it = params_.find(id);
    if (it != params_.end()) {
        it->second.editing = false;
    }
}

uint32_t ParameterManager::count() const {
    std::shared_lock lock(mutex_);
    return static_cast<uint32_t>(params_.size());
}

} // namespace aria::plugin
