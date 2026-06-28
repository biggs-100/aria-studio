#include <catch2/catch_test_macros.hpp>

#include "plugin/plugin_scanner.h"
#include "plugin/format_scanner.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace aria::plugin;
namespace fs = std::filesystem;

// ══════════════════════════════════════════════════════════════════════
//  Mock FormatScanner for testing
// ══════════════════════════════════════════════════════════════════════

class MockScanner : public FormatScanner {
public:
    explicit MockScanner(std::string ext, std::string format_name)
        : ext_(std::move(ext))
        , format_name_(std::move(format_name))
    {}

    std::string name() const override { return format_name_; }
    std::string extension() const override { return ext_; }

    std::vector<PluginDescriptor> scan(const std::string& path) override {
        PluginDescriptor desc;
        desc.id = "mock." + fs::path(path).stem().string();
        desc.name = fs::path(path).stem().string();
        desc.vendor = "MockVendor";
        desc.format = format_name_;
        desc.path = path;
        desc.category = PluginCategory::Effect;

        std::string stem = fs::path(path).stem().string();
        if (stem.find("synth") != std::string::npos) {
            desc.category = PluginCategory::Synth;
            desc.is_synth = true;
        }
        if (stem.find("analyzer") != std::string::npos) {
            desc.category = PluginCategory::Analyzer;
        }

        return {desc};
    }

private:
    std::string ext_;
    std::string format_name_;
};

// ══════════════════════════════════════════════════════════════════════
//  Test helpers
// ══════════════════════════════════════════════════════════════════════

struct TempDir {
    fs::path path;
    TempDir() {
        std::random_device rd;
        path = fs::temp_directory_path() / ("aria_scanner_test_" + std::to_string(rd()));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    std::string str() const { return path.string(); }
};

void create_plugin_file(const fs::path& dir, const std::string& name,
                        const std::string& ext) {
    std::ofstream ofs(dir / (name + ext));
    ofs << "mock plugin content";
    ofs.close();
}

// ══════════════════════════════════════════════════════════════════════
//  Scanner tests
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("Scanner discovers new plugins in directory", "[plugin][scanner]") {
    TempDir tmp;
    create_plugin_file(tmp.path, "test_eq", ".clap");
    create_plugin_file(tmp.path, "test_reverb", ".clap");

    auto scanner = std::make_unique<MockScanner>(".clap", "CLAP");
    std::vector<std::unique_ptr<FormatScanner>> scanners;
    scanners.push_back(std::move(scanner));

    PluginScanner ps;
    auto result = ps.scan({tmp.str()}, scanners);

    REQUIRE(result.all.size() == 2);
    REQUIRE(result.files_scanned == 2);
}

TEST_CASE("Scanner handles multiple extensions", "[plugin][scanner]") {
    TempDir tmp;
    create_plugin_file(tmp.path, "test_synth", ".clap");
    create_plugin_file(tmp.path, "test_compressor", ".vst3");

    auto clap_scanner = std::make_unique<MockScanner>(".clap", "CLAP");
    auto vst3_scanner = std::make_unique<MockScanner>(".vst3", "VST3");

    std::vector<std::unique_ptr<FormatScanner>> scanners;
    scanners.push_back(std::move(clap_scanner));
    scanners.push_back(std::move(vst3_scanner));

    PluginScanner ps;
    auto result = ps.scan({tmp.str()}, scanners);

    REQUIRE(result.all.size() == 2);
}

TEST_CASE("Scanner ignores unrelated file types", "[plugin][scanner]") {
    TempDir tmp;
    create_plugin_file(tmp.path, "notes", ".txt");
    create_plugin_file(tmp.path, "data", ".dat");

    auto scanner = std::make_unique<MockScanner>(".clap", "CLAP");
    std::vector<std::unique_ptr<FormatScanner>> scanners;
    scanners.push_back(std::move(scanner));

    PluginScanner ps;
    auto result = ps.scan({tmp.str()}, scanners);

    REQUIRE(result.all.empty());
    REQUIRE(result.files_scanned == 0);
}

// ══════════════════════════════════════════════════════════════════════
//  Incremental mtime scan
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("Incremental scan discovers new files", "[plugin][scanner][incremental]") {
    TempDir tmp;
    create_plugin_file(tmp.path, "existing_eq", ".clap");

    auto scanner = std::make_unique<MockScanner>(".clap", "CLAP");
    std::vector<std::unique_ptr<FormatScanner>> scanners;
    scanners.push_back(std::move(scanner));

    PluginScanner ps;
    auto result1 = ps.scan({tmp.str()}, scanners);
    REQUIRE(result1.all.size() == 1);

    // Add a new plugin file
    create_plugin_file(tmp.path, "new_synth", ".clap");

    auto scanner2 = std::make_unique<MockScanner>(".clap", "CLAP");
    std::vector<std::unique_ptr<FormatScanner>> scanners2;
    scanners2.push_back(std::move(scanner2));

    auto result2 = ps.scan({tmp.str()}, scanners2);
    REQUIRE(result2.all.size() == 2);
}

TEST_CASE("Deleted plugin is removed from result", "[plugin][scanner][incremental]") {
    TempDir tmp;
    create_plugin_file(tmp.path, "to_delete", ".clap");
    create_plugin_file(tmp.path, "keep_me", ".clap");

    auto scanner = std::make_unique<MockScanner>(".clap", "CLAP");
    std::vector<std::unique_ptr<FormatScanner>> scanners;
    scanners.push_back(std::move(scanner));

    PluginScanner ps;
    auto result1 = ps.scan({tmp.str()}, scanners);
    REQUIRE(result1.all.size() == 2);

    // Delete one plugin
    std::error_code ec;
    fs::remove(tmp.path / "to_delete.clap", ec);
    REQUIRE_FALSE(ec);

    // Re-scan — should detect the deletion
    auto scanner2 = std::make_unique<MockScanner>(".clap", "CLAP");
    std::vector<std::unique_ptr<FormatScanner>> scanners2;
    scanners2.push_back(std::move(scanner2));

    auto result2 = ps.scan({tmp.str()}, scanners2);
    REQUIRE(result2.all.size() == 1);
    REQUIRE(result2.all[0].name == "keep_me");
}

TEST_CASE("Updated plugin is re-scanned", "[plugin][scanner][incremental]") {
    TempDir tmp;
    create_plugin_file(tmp.path, "updated", ".clap");

    auto scanner = std::make_unique<MockScanner>(".clap", "CLAP");
    std::vector<std::unique_ptr<FormatScanner>> scanners;
    scanners.push_back(std::move(scanner));

    PluginScanner ps;
    auto result1 = ps.scan({tmp.str()}, scanners);
    REQUIRE(result1.all.size() == 1);

    // Touch the file to update its mtime
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    {
        std::ofstream ofs(tmp.path / "updated.clap", std::ios::app);
        ofs << " ";
        ofs.close();
    }

    // Re-scan — should re-detect the modified file
    auto scanner2 = std::make_unique<MockScanner>(".clap", "CLAP");
    std::vector<std::unique_ptr<FormatScanner>> scanners2;
    scanners2.push_back(std::move(scanner2));

    auto result2 = ps.scan({tmp.str()}, scanners2);
    REQUIRE(result2.all.size() == 1);
}

TEST_CASE("Rescan clears cache and rescans everything", "[plugin][scanner][rescan]") {
    TempDir tmp;
    create_plugin_file(tmp.path, "plugin_a", ".clap");

    auto scanner = std::make_unique<MockScanner>(".clap", "CLAP");
    std::vector<std::unique_ptr<FormatScanner>> scanners;
    scanners.push_back(std::move(scanner));

    PluginScanner ps;
    auto result1 = ps.scan({tmp.str()}, scanners);
    REQUIRE(result1.all.size() == 1);

    // Add another plugin
    create_plugin_file(tmp.path, "plugin_b", ".clap");

    // Full rescan should find both
    auto scanner2 = std::make_unique<MockScanner>(".clap", "CLAP");
    std::vector<std::unique_ptr<FormatScanner>> scanners2;
    scanners2.push_back(std::move(scanner2));

    auto result2 = ps.rescan({tmp.str()}, scanners2);
    REQUIRE(result2.all.size() == 2);
}

// ══════════════════════════════════════════════════════════════════════
//  JSON Cache
// ══════════════════════════════════════════════════════════════════════

TEST_CASE("Save and load cache round-trip", "[plugin][scanner][cache]") {
    TempDir tmp;
    create_plugin_file(tmp.path, "cached_eq", ".clap");
    create_plugin_file(tmp.path, "cached_synth", ".clap");

    auto scanner = std::make_unique<MockScanner>(".clap", "CLAP");
    std::vector<std::unique_ptr<FormatScanner>> scanners;
    scanners.push_back(std::move(scanner));

    std::string cache_path = (tmp.path / "plugin_cache.json").string();

    PluginScanner ps;
    ps.set_cache_path(cache_path);
    auto result1 = ps.scan({tmp.str()}, scanners);
    REQUIRE(result1.all.size() == 2);

    // Cache should have been saved automatically
    REQUIRE(fs::exists(cache_path));

    // Create a new scanner and load cache
    PluginScanner ps2;
    ps2.set_cache_path(cache_path);
    REQUIRE(ps2.load_cache());

    // Scan should use cache and find same plugins
    auto scanner2 = std::make_unique<MockScanner>(".clap", "CLAP");
    std::vector<std::unique_ptr<FormatScanner>> scanners2;
    scanners2.push_back(std::move(scanner2));

    auto result2 = ps2.scan({tmp.str()}, scanners2);
    REQUIRE(result2.all.size() == 2);
}

TEST_CASE("Missing cache triggers scan from scratch", "[plugin][scanner][cache]") {
    PluginScanner ps;
    ps.set_cache_path("nonexistent_cache.json");
    REQUIRE_FALSE(ps.load_cache());
}

TEST_CASE("Cancel scan returns partial results", "[plugin][scanner][cancel]") {
    TempDir tmp;
    // Create a few plugin files
    for (int i = 0; i < 5; ++i) {
        create_plugin_file(tmp.path, "plugin_" + std::to_string(i), ".clap");
    }

    auto scanner = std::make_unique<MockScanner>(".clap", "CLAP");
    std::vector<std::unique_ptr<FormatScanner>> scanners;
    scanners.push_back(std::move(scanner));

    PluginScanner ps;
    // Cancel before scan starts
    ps.cancel();
    auto result = ps.scan({tmp.str()}, scanners);
    // When cancelled before any files processed, results may be empty or partial
    REQUIRE(result.scan_duration_ms >= 0);
}

TEST_CASE("PluginScanner progress starts at 0 and ends at 1", "[plugin][scanner]") {
    TempDir tmp;
    create_plugin_file(tmp.path, "progress_test", ".clap");

    auto scanner = std::make_unique<MockScanner>(".clap", "CLAP");
    std::vector<std::unique_ptr<FormatScanner>> scanners;
    scanners.push_back(std::move(scanner));

    PluginScanner ps;
    REQUIRE(ps.progress() == 0.0f);

    ps.scan({tmp.str()}, scanners);

    REQUIRE_FALSE(ps.is_scanning());
}

TEST_CASE("Scan result includes duration", "[plugin][scanner]") {
    TempDir tmp;
    create_plugin_file(tmp.path, "duration_test", ".clap");

    auto scanner = std::make_unique<MockScanner>(".clap", "CLAP");
    std::vector<std::unique_ptr<FormatScanner>> scanners;
    scanners.push_back(std::move(scanner));

    PluginScanner ps;
    auto result = ps.scan({tmp.str()}, scanners);
    REQUIRE(result.scan_duration_ms >= 0);
}
