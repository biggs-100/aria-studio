#include "plugin_scanner.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

namespace aria::plugin {

namespace fs = std::filesystem;
using json = nlohmann::json;

// ══════════════════════════════════════════════════════════════════════
//  Scanning
// ══════════════════════════════════════════════════════════════════════

ScanResult PluginScanner::scan(
    const std::vector<std::string>& paths,
    const std::vector<std::unique_ptr<FormatScanner>>& scanners)
{
    auto start_time = std::chrono::steady_clock::now();
    scanning_.store(true, std::memory_order_release);
    progress_.store(0.0f, std::memory_order_release);
    cancel_.store(false, std::memory_order_release);

    ScanResult result;

    // Collect all files to scan
    std::vector<std::string> all_files;
    size_t total = 0;
    size_t processed = 0;

    for (const auto& dir_path : paths) {
        std::error_code ec;
        if (!fs::exists(dir_path, ec) || !fs::is_directory(dir_path, ec)) {
            continue;
        }

        for (const auto& entry : fs::recursive_directory_iterator(dir_path, ec)) {
            if (cancel_.load(std::memory_order_acquire)) {
                goto done;
            }

            if (!entry.is_regular_file(ec)) continue;

            auto ext = entry.path().extension().string();
            // Check if any scanner handles this extension
            for (const auto& scanner : scanners) {
                if (scanner->extension() == ext) {
                    all_files.push_back(entry.path().string());
                    break;
                }
            }
        }
    }

    if (all_files.empty()) {
        goto done;
    }

    // Sort for deterministic ordering
    std::sort(all_files.begin(), all_files.end());

    // Scan each file incrementally
    total = all_files.size();
    processed = 0;

    for (const auto& file_path : all_files) {
        if (cancel_.load(std::memory_order_acquire)) {
            break;
        }

        // Update progress
        processed++;
        progress_.store(static_cast<float>(processed) / static_cast<float>(total),
                        std::memory_order_release);

        // Check mtime against cache
        std::error_code ec;
        auto current_mtime = fs::last_write_time(file_path, ec);
        if (ec) {
            // Can't read file — skip
            continue;
        }

        auto cache_it = mtime_cache_.find(file_path);
        if (cache_it != mtime_cache_.end() && cache_it->second == current_mtime) {
            // File hasn't changed — keep cached descriptor
            auto desc_it = std::find_if(descriptors_.begin(), descriptors_.end(),
                [&](const PluginDescriptor& d) { return d.path == file_path; });
            if (desc_it != descriptors_.end()) {
                result.all.push_back(*desc_it);
            }
            continue;
        }

        // File changed or new — scan it
        auto descriptors = scan_file(file_path, scanners);

        if (!descriptors.empty()) {
            // Update mtime cache
            mtime_cache_[file_path] = current_mtime;

            // Remove old entries for this path
            auto& all = result.all;
            all.erase(std::remove_if(all.begin(), all.end(),
                [&](const PluginDescriptor& d) { return d.path == file_path; }),
                all.end());

            for (auto& desc : descriptors) {
                desc.last_modified = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::seconds>(
                        current_mtime.time_since_epoch()).count());
                result.all.push_back(std::move(desc));
            }
        }

        result.files_scanned++;
    }

    // Detect deleted files
    for (auto it = mtime_cache_.begin(); it != mtime_cache_.end(); ) {
        std::error_code ec;
        if (!fs::exists(it->first, ec)) {
            // File was deleted
            auto& all = result.all;
            all.erase(std::remove_if(all.begin(), all.end(),
                [&](const PluginDescriptor& d) { return d.path == it->first; }),
                all.end());

            it = mtime_cache_.erase(it);
        } else {
            ++it;
        }
    }

done:
    // Store the complete descriptor list
    descriptors_ = result.all;

    auto end_time = std::chrono::steady_clock::now();
    result.scan_duration_ms = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count());

    scanning_.store(false, std::memory_order_release);

    // Auto-save cache if path is configured
    if (!cache_path_.empty() && !result.all.empty()) {
        save_cache();
    }

    return result;
}

ScanResult PluginScanner::rescan(
    const std::vector<std::string>& paths,
    const std::vector<std::unique_ptr<FormatScanner>>& scanners)
{
    mtime_cache_.clear();
    descriptors_.clear();
    return scan(paths, scanners);
}

// ══════════════════════════════════════════════════════════════════════
//  Cache  (JSON)
// ══════════════════════════════════════════════════════════════════════

void PluginScanner::set_cache_path(const std::string& path) {
    cache_path_ = path;
}

bool PluginScanner::load_cache() {
    if (cache_path_.empty()) return false;

    std::ifstream ifs(cache_path_);
    if (!ifs.is_open()) return false;

    json j;
    try {
        ifs >> j;
    } catch (...) {
        return false;
    }

    if (!j.is_object()) return false;

    mtime_cache_.clear();
    descriptors_.clear();

    // Load mtime cache
    if (j.contains("mtimes") && j["mtimes"].is_object()) {
        for (auto it = j["mtimes"].begin(); it != j["mtimes"].end(); ++it) {
            std::string path = it.key();
            int64_t epoch_seconds = it->get<int64_t>();
            mtime_cache_[path] = fs::file_time_type(
                fs::file_time_type::duration(epoch_seconds));
        }
    }

    // Load descriptors
    if (j.contains("descriptors") && j["descriptors"].is_array()) {
        for (const auto& item : j["descriptors"]) {
            try {
                PluginDescriptor desc;
                desc.id = item.value("id", "");
                desc.name = item.value("name", "");
                desc.vendor = item.value("vendor", "");
                desc.version = item.value("version", "");
                desc.format = item.value("format", "");
                desc.path = item.value("path", "");
                desc.category = static_cast<PluginCategory>(
                    item.value("category", static_cast<int>(PluginCategory::Effect)));
                desc.num_inputs = item.value("num_inputs", 0u);
                desc.num_outputs = item.value("num_outputs", 0u);
                desc.has_editor = item.value("has_editor", false);
                desc.is_synth = item.value("is_synth", false);
                desc.last_modified = item.value("last_modified", 0ull);
                descriptors_.push_back(std::move(desc));
            } catch (...) {
                // Skip malformed entries
                continue;
            }
        }
    }

    return true;
}

bool PluginScanner::save_cache() {
    if (cache_path_.empty()) return false;

    json j;
    j["version"] = 1;
    j["generator"] = "aria-plugin-scanner";

    // Save mtimes as epoch seconds
    json mtime_obj = json::object();
    for (const auto& [path, mtime] : mtime_cache_) {
        auto epoch = std::chrono::duration_cast<std::chrono::seconds>(
            mtime.time_since_epoch()).count();
        mtime_obj[path] = static_cast<int64_t>(epoch);
    }
    j["mtimes"] = mtime_obj;

    // Save descriptors
    json desc_array = json::array();
    for (const auto& desc : descriptors_) {
        json item;
        item["id"] = desc.id;
        item["name"] = desc.name;
        item["vendor"] = desc.vendor;
        item["version"] = desc.version;
        item["format"] = desc.format;
        item["path"] = desc.path;
        item["category"] = static_cast<int>(desc.category);
        item["num_inputs"] = desc.num_inputs;
        item["num_outputs"] = desc.num_outputs;
        item["has_editor"] = desc.has_editor;
        item["is_synth"] = desc.is_synth;
        item["last_modified"] = desc.last_modified;
        desc_array.push_back(item);
    }
    j["descriptors"] = desc_array;

    // Atomic write: temp → rename
    std::string tmp_path = cache_path_ + ".tmp";
    {
        std::ofstream ofs(tmp_path);
        if (!ofs.is_open()) return false;
        ofs << j.dump(2);
        ofs.close();
    }

    if (std::rename(tmp_path.c_str(), cache_path_.c_str()) != 0) {
        std::remove(tmp_path.c_str());
        return false;
    }

    return true;
}

bool PluginScanner::schedule_save() {
    return save_cache();
}

// ══════════════════════════════════════════════════════════════════════
//  Control
// ══════════════════════════════════════════════════════════════════════

void PluginScanner::cancel() {
    cancel_.store(true, std::memory_order_release);
}

bool PluginScanner::is_scanning() const {
    return scanning_.load(std::memory_order_acquire);
}

float PluginScanner::progress() const {
    return progress_.load(std::memory_order_acquire);
}

// ══════════════════════════════════════════════════════════════════════
//  Private Helpers
// ══════════════════════════════════════════════════════════════════════

void PluginScanner::rebuild_mtime_cache(
    const std::vector<PluginDescriptor>& descriptors)
{
    mtime_cache_.clear();
    for (const auto& desc : descriptors) {
        std::error_code ec;
        auto mtime = fs::last_write_time(desc.path, ec);
        if (!ec) {
            mtime_cache_[desc.path] = mtime;
        }
    }
}

std::vector<PluginDescriptor> PluginScanner::scan_file(
    const std::string& path,
    const std::vector<std::unique_ptr<FormatScanner>>& scanners)
{
    auto ext = fs::path(path).extension().string();

    for (const auto& scanner : scanners) {
        if (scanner->extension() == ext) {
            return scanner->scan(path);
        }
    }

    return {};
}

} // namespace aria::plugin
