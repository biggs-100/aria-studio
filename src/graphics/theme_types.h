#ifndef ARIA_THEME_TYPES_H
#define ARIA_THEME_TYPES_H

#include "graphics/graphics_types.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace aria {

// ── ColorPalette ────────────────────────────────────────────────────

/// Semantic color palette matching the theme JSON schema.
struct ColorPalette {
    // Background hierarchy
    Color bg_primary   = Color::Black;
    Color bg_secondary = Color::Black;
    Color bg_tertiary  = Color::Black;
    Color bg_surface   = Color::Black;
    Color bg_elevated  = Color::Black;
    Color bg_tooltip   = Color::Black;

    // Text hierarchy
    Color text_primary   = Color::White;
    Color text_secondary = Color::White;
    Color text_tertiary  = Color::White;
    Color text_disabled  = Color::White;
    Color text_inverse   = Color::White;
    Color text_link      = Color::White;
    Color text_error     = Color::White;
    Color text_warning   = Color::White;
    Color text_success   = Color::White;

    // Accent
    Color accent_primary   = Color::White;
    Color accent_secondary = Color::White;
    Color accent_tertiary  = Color::White;
    Color accent_hover     = Color::White;
    Color accent_pressed   = Color::White;
    Color accent_muted     = Color::White;
    Color accent_glow      = Color::White;

    // Border
    Color border_primary   = Color::White;
    Color border_secondary = Color::White;
    Color border_focus     = Color::White;
    Color border_error     = Color::White;
    Color border_selected  = Color::White;

    // Track
    Color track_default    = Color::White;
    Color track_audio      = Color::White;
    Color track_midi       = Color::White;
    Color track_group      = Color::White;
    Color track_vca        = Color::White;
    Color track_return     = Color::White;
    Color track_master     = Color::White;
    Color track_automation = Color::White;

    // Meter
    Color meter_cold     = Color::White;
    Color meter_warm     = Color::White;
    Color meter_hot      = Color::White;
    Color meter_critical = Color::White;
    Color meter_clip     = Color::White;
    Color meter_background = Color::White;

    // Waveform
    Color waveform_fill       = Color::White;
    Color waveform_outline    = Color::White;
    Color waveform_background = Color::White;
    Color waveform_grid       = Color::White;

    // Selection
    Color selection_fill    = Color::White;
    Color selection_outline = Color::White;
    Color selection_text    = Color::White;
};

// ── TypographySettings ──────────────────────────────────────────────

/// Typography configuration matching the theme JSON schema.
struct TypographySettings {
    // Font families (CSS-style fallback strings)
    std::string font_family_ui      = "Inter, -apple-system, sans-serif";
    std::string font_family_mono    = "JetBrains Mono, SF Mono, monospace";
    std::string font_family_display = "Inter, -apple-system, sans-serif";

    // Font weights
    int font_weight_light    = 300;
    int font_weight_regular  = 400;
    int font_weight_medium   = 500;
    int font_weight_semibold = 600;
    int font_weight_bold     = 700;

    // Font sizes (in px)
    float font_size_xs  = 10.0f;
    float font_size_sm  = 11.0f;
    float font_size_base = 12.0f;
    float font_size_md  = 13.0f;
    float font_size_lg  = 14.0f;
    float font_size_xl  = 16.0f;
    float font_size_2xl = 18.0f;
    float font_size_3xl = 24.0f;
    float font_size_4xl = 32.0f;

    // Convenience: the default font size for UI elements
    float font_size = 12.0f;
    std::string font_family = "Inter, -apple-system, sans-serif";
};

// ── SpacingSystem ───────────────────────────────────────────────────

/// Spacing and layout configuration matching the theme JSON schema.
struct SpacingSystem {
    // Spacing unit (base increment in px)
    float unit = 4.0f;

    // Scale
    float scale_0  = 0.0f;
    float scale_1  = 4.0f;
    float scale_2  = 8.0f;
    float scale_3  = 12.0f;
    float scale_4  = 16.0f;
    float scale_5  = 20.0f;
    float scale_6  = 24.0f;
    float scale_8  = 32.0f;
    float scale_10 = 40.0f;
    float scale_12 = 48.0f;
    float scale_16 = 64.0f;
    float scale_20 = 80.0f;
    float scale_24 = 96.0f;

    // Panel defaults
    float panel_padding      = 16.0f;
    float panel_gap          = 8.0f;
    float panel_header_height = 32.0f;
    float panel_footer_height = 24.0f;

    // Channel strip
    float channel_strip_width_narrow = 48.0f;
    float channel_strip_width_normal = 72.0f;
    float channel_strip_width_wide   = 120.0f;
    float channel_strip_width_full   = 200.0f;

    // Border radius
    float border_radius_none = 0.0f;
    float border_radius_sm   = 2.0f;
    float border_radius_md   = 4.0f;
    float border_radius_lg   = 6.0f;
    float border_radius_xl   = 8.0f;
    float border_radius_full = 9999.0f;

    // Convenience: base padding
    float padding = 8.0f;
};

// ── ComponentStyles ─────────────────────────────────────────────────

/// Component style overrides matching the theme JSON schema.
struct ComponentStyles {
    // Placeholder — full component style resolution is a future extension.
    // For now, we store a simple key-value map for component style tokens.
    std::unordered_map<std::string, std::string> overrides;
};

// ── Theme ───────────────────────────────────────────────────────────

/// Top-level theme structure matching the theme JSON schema.
struct Theme {
    std::string name;
    std::string author;
    std::string version;
    std::string description;

    ColorPalette colors;
    TypographySettings typography;
    SpacingSystem spacing;
    ComponentStyles components;

    /// Raw JSON key-value pairs for token resolution.
    /// Populated during load for arbitrary dot-path access.
    std::unordered_map<std::string, std::string> tokens;
};

// ── ThemeInfo ───────────────────────────────────────────────────────

/// Summary info for listing available themes.
struct ThemeInfo {
    std::string name;
    std::string author;
    std::string version;
    std::string description;
    std::string file_path;
};

} // namespace aria

#endif // ARIA_THEME_TYPES_H
