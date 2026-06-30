#include "graphics/theme_engine.h"
#include "graphics/widget_themed.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

using json = nlohmann::json;

namespace aria {

// ── Static instance ────────────────────────────────────────────────

ThemeEngine* ThemeEngine::instance_ = nullptr;

ThemeEngine* ThemeEngine::get() {
    return instance_;
}

void ThemeEngine::set_instance(ThemeEngine* instance) {
    instance_ = instance;
}

// ── Construction / Destruction ─────────────────────────────────────

ThemeEngine::ThemeEngine() = default;
ThemeEngine::~ThemeEngine() = default;

// ── Loading ────────────────────────────────────────────────────────

bool ThemeEngine::load_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    current_path_ = path;
    parse_json(buffer.str());
    return true;
}

bool ThemeEngine::load_from_string(std::string_view json_str) {
    parse_json(json_str);
    return true;
}

// ── Token resolution ───────────────────────────────────────────────

Color ThemeEngine::resolveColor(std::string_view path) {
    std::string key(path);

    // Check color cache first
    auto it = color_cache_.find(key);
    if (it != color_cache_.end()) {
        return it->second;
    }

    // Resolve the raw token value
    std::string raw = resolve_token(path);

    // Try to parse as color
    if (raw.empty()) {
        Color fallback{};
        color_cache_[key] = fallback;
        return fallback;
    }

    Color result{};

    // Check if it's an rgba() value
    if (raw.starts_with("rgba(") || raw.starts_with("rgba(")) {
        result = parse_rgba_color(raw);
    } else {
        result = parse_hex_color(raw);
    }

    color_cache_[key] = result;
    return result;
}

float ThemeEngine::resolveFloat(std::string_view path) {
    std::string key(path);

    // Check float cache first
    auto it = float_cache_.find(key);
    if (it != float_cache_.end()) {
        return it->second;
    }

    // Resolve the raw token value
    std::string raw = resolve_token(path);

    float result = parse_float(raw);
    float_cache_[key] = result;
    return result;
}

// ── Theme enumeration ──────────────────────────────────────────────

std::vector<ThemeInfo> ThemeEngine::availableThemes() const {
    // Built-in themes shipped with the application.
    // In a future version, this will scan the theme directory.
    std::vector<ThemeInfo> themes;
    themes.push_back({"ARIA Dark",        "ARIA Team", "1.0", "Default dark theme — optimized for long sessions",           "themes/aria-dark.json"});
    themes.push_back({"ARIA Light",       "ARIA Team", "1.0", "Light theme for bright environments",                        "themes/aria-light.json"});
    themes.push_back({"ARIA High Contrast", "ARIA Team", "1.0", "High contrast theme for accessibility",                   "themes/aria-high-contrast.json"});
    return themes;
}

// ── Live reload ────────────────────────────────────────────────────

void ThemeEngine::watch(const std::string& path) {
    if (!watcher_) {
        watcher_ = std::make_unique<FileWatcher>();
    }

    watcher_->watch(path, [this](const std::string& changed_path) {
        if (load_from_file(changed_path)) {
            // Notify all ThemedWidgets via the static registry
            ThemedWidget::notify_all();

            // EventBus integration is handled by the application layer
            // via the ThemedWidget::notify_all() static method above.
            // TODO: Wire EventBus publish when Application::instance() is
            // accessible from the graphics module without circular deps.
        }
    });
}

// ── Private helpers ────────────────────────────────────────────────

void ThemeEngine::parse_json(std::string_view json_str) {
    raw_json_ = json_str;

    // Clear caches
    token_cache_.clear();
    color_cache_.clear();
    float_cache_.clear();

    current_ = Theme{};

    try {
        auto root = json::parse(json_str);

        // ── Metadata ────────────────────────────────────────────
        if (root.contains("name"))        current_.name        = root["name"].get<std::string>();
        if (root.contains("author"))      current_.author      = root["author"].get<std::string>();
        if (root.contains("version"))     current_.version     = root["version"].get<std::string>();
        if (root.contains("description")) current_.description = root["description"].get<std::string>();

        // ── Colors ──────────────────────────────────────────────
        if (root.contains("colors")) {
            const auto& colors = root["colors"];
            auto parse_color = [&](const std::string& key, const std::string& section) -> std::string {
                if (colors.contains(section) && colors[section].contains(key)) {
                    return colors[section][key].get<std::string>();
                }
                return {};
            };

            // The flat cache is rebuilt from the full JSON tree below.
        }

        // ── Typography ──────────────────────────────────────────
        if (root.contains("typography")) {
            const auto& typo = root["typography"];
            if (typo.contains("fontSize"))   current_.typography.font_size   = typo["fontSize"].get<float>();
            if (typo.contains("fontFamily")) current_.typography.font_family = typo["fontFamily"].get<std::string>();
        }

        // ── Spacing ─────────────────────────────────────────────
        if (root.contains("spacing")) {
            const auto& sp = root["spacing"];
            if (sp.contains("padding")) current_.spacing.padding = sp["padding"].get<float>();
            if (sp.contains("unit"))    current_.spacing.unit    = sp["unit"].get<float>();
        }

        // ── Build flat token cache ──────────────────────────────
        rebuild_cache();

        loaded_ = true;

    } catch (const json::exception&) {
        loaded_ = false;
    }
}

void ThemeEngine::rebuild_cache() {
    if (raw_json_.empty()) return;

    try {
        auto root = json::parse(raw_json_);

        // Recursive walk: collect all leaf paths as dot-separated keys.
        std::function<void(const json&, const std::string&)> walk =
            [&](const json& node, const std::string& prefix) {
                if (node.is_object()) {
                    for (auto it = node.begin(); it != node.end(); ++it) {
                        std::string key = prefix.empty() ? it.key() : prefix + "." + it.key();
                        if (it.value().is_object()) {
                            walk(it.value(), key);
                        } else if (it.value().is_string()) {
                            token_cache_[key] = it.value().get<std::string>();
                        } else if (it.value().is_number()) {
                            token_cache_[key] = std::to_string(it.value().get<double>());
                        }
                    }
                }
            };

        walk(root, "");
    } catch (const json::exception&) {
        // If rebuild fails, keep existing cache
    }
}

std::string ThemeEngine::resolve_token(std::string_view path) {
    std::string key(path);

    // Check flat token cache
    auto it = token_cache_.find(key);
    if (it != token_cache_.end()) {
        std::string value = it->second;

        // Resolve any {token} references in the value (recursive, max depth 10)
        int depth = 0;
        while (depth < 10) {
            auto ref_start = value.find('{');
            if (ref_start == std::string::npos) break;

            auto ref_end = value.find('}', ref_start);
            if (ref_end == std::string::npos) break;

            std::string ref_path = value.substr(ref_start + 1, ref_end - ref_start - 1);
            auto ref_it = token_cache_.find(ref_path);
            if (ref_it != token_cache_.end()) {
                value.replace(ref_start, ref_end - ref_start + 1, ref_it->second);
            } else {
                // Reference not found — leave as-is
                break;
            }
            ++depth;
        }

        return value;
    }

    return {};
}

std::string ThemeEngine::resolve_references(const std::string& value) {
    std::string result = value;
    int depth = 0;
    while (depth < 10) {
        auto ref_start = result.find('{');
        if (ref_start == std::string::npos) break;

        auto ref_end = result.find('}', ref_start);
        if (ref_end == std::string::npos) break;

        std::string ref_path = result.substr(ref_start + 1, ref_end - ref_start - 1);
        auto ref_it = token_cache_.find(ref_path);
        if (ref_it != token_cache_.end()) {
            result.replace(ref_start, ref_end - ref_start + 1, ref_it->second);
        } else {
            break;
        }
        ++depth;
    }
    return result;
}

Color ThemeEngine::parse_hex_color(const std::string& hex) {
    Color c{};

    std::string h = hex;
    // Strip leading # if present
    if (!h.empty() && h[0] == '#') {
        h = h.substr(1);
    }

    // Handle short form (#RGB)
    if (h.size() == 3) {
        uint32_t r_val = 0, g_val = 0, b_val = 0;
        std::from_chars(h.data(), h.data() + 1, r_val, 16);
        std::from_chars(h.data() + 1, h.data() + 2, g_val, 16);
        std::from_chars(h.data() + 2, h.data() + 3, b_val, 16);
        c.r = static_cast<float>(r_val * 17) / 255.0f;
        c.g = static_cast<float>(g_val * 17) / 255.0f;
        c.b = static_cast<float>(b_val * 17) / 255.0f;
        return c;
    }

    // Handle full form (#RRGGBB or RRGGBB)
    if (h.size() >= 6) {
        uint32_t r_val = 0, g_val = 0, b_val = 0;
        std::from_chars(h.data(), h.data() + 2, r_val, 16);
        std::from_chars(h.data() + 2, h.data() + 4, g_val, 16);
        std::from_chars(h.data() + 4, h.data() + 6, b_val, 16);
        c.r = static_cast<float>(r_val) / 255.0f;
        c.g = static_cast<float>(g_val) / 255.0f;
        c.b = static_cast<float>(b_val) / 255.0f;
    }

    // Handle RGBA (#RRGGBBAA)
    if (h.size() == 8) {
        uint32_t a_val = 0;
        std::from_chars(h.data() + 6, h.data() + 8, a_val, 16);
        c.a = static_cast<float>(a_val) / 255.0f;
    }

    return c;
}

Color ThemeEngine::parse_rgba_color(const std::string& rgba) {
    Color c{};

    // Expected format: rgba(r, g, b, a) or rgba(r,g,b,a)
    auto open = rgba.find('(');
    auto close = rgba.find(')');
    if (open == std::string::npos || close == std::string::npos) {
        return c;
    }

    std::string inner = rgba.substr(open + 1, close - open - 1);
    // Remove spaces
    inner.erase(std::remove(inner.begin(), inner.end(), ' '), inner.end());

    // Split by commas
    std::vector<float> parts;
    std::stringstream ss(inner);
    std::string part;
    while (std::getline(ss, part, ',')) {
        float val = 0.0f;
        std::from_chars(part.data(), part.data() + part.size(), val);
        parts.push_back(val);
    }

    if (parts.size() >= 3) {
        c.r = parts[0] / 255.0f;
        c.g = parts[1] / 255.0f;
        c.b = parts[2] / 255.0f;
        c.a = (parts.size() >= 4) ? parts[3] : 1.0f;
    }

    return c;
}

float ThemeEngine::parse_float(const std::string& value) {
    if (value.empty()) return 0.0f;

    float result = 0.0f;
    auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (ec == std::errc()) {
        return result;
    }
    return 0.0f;
}

} // namespace aria
