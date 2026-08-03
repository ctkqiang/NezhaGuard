#ifndef NEZHAGUARD_THEME_H
#define NEZHAGUARD_THEME_H

namespace Theme {
    // ♡ ═══════════════════════════════════════════ ♡
    // ♡  femboy kawaii palette — pink & cyan first ♡
    // ♡ ═══════════════════════════════════════════ ♡

    // ── pink family (primary accent) ──
    inline constexpr auto Pink         = "#ff8fc7";   // bright kawaii pink
    inline constexpr auto PinkLight    = "#ffb8da";   // soft petal pink
    inline constexpr auto PinkDeep     = "#f472b6";   // deep rose pink
    inline constexpr auto PinkBlush    = "#ffe0ef";   // blush (light bg tint)
    inline constexpr auto PinkHot      = "#ff5ea0";   // hot pink (crit/alert)
    inline constexpr auto PinkNeon     = "#ff66b2";   // neon pop pink

    // ── cyan family (secondary accent) ──
    inline constexpr auto Cyan         = "#5ef0d1";   // bright kawaii cyan
    inline constexpr auto CyanLight    = "#8fffe4";   // soft mint cyan
    inline constexpr auto CyanDeep     = "#3dd6b4";   // deep teal cyan
    inline constexpr auto CyanNeon     = "#00ffcc";   // neon cyan glow
    inline constexpr auto CyanIce      = "#bffff3";   // ice cyan

    // ── lavender / purple ──
    inline constexpr auto Lavender     = "#c9a0ff";   // soft lavender
    inline constexpr auto LavenderLight= "#e0ccff";
    inline constexpr auto LavenderDeep = "#b080f0";
    inline constexpr auto Lilac        = "#d4b8ff";

    // ── baby blue ──
    inline constexpr auto BabyBlue     = "#7ecfff";
    inline constexpr auto BabyBlueLight= "#b0e4ff";

    // ── mint / green ──
    inline constexpr auto Mint         = "#7ef0b8";
    inline constexpr auto MintLight    = "#b0ffd8";
    inline constexpr auto MintDeep     = "#5ad498";

    // ── peach / coral ──
    inline constexpr auto Peach        = "#ffc8a8";
    inline constexpr auto PeachLight   = "#ffe0d0";
    inline constexpr auto PeachDeep    = "#f0a878";

    // ── semantic (soft) ──
    inline constexpr auto Red          = "#ff5e7a";   // cherry red
    inline constexpr auto Orange       = "#ffa070";   // soft tangerine
    inline constexpr auto Green        = "#6ae0a0";   // mint green
    inline constexpr auto Purple       = "#c090f0";   // wisteria
    inline constexpr auto White        = "#fff5fa";   // cream pink-white
    inline constexpr auto Grey         = "#a090a8";   // mauve grey

    // ═══════════════════════════════════════════
    // ♡ DARK THEME — pink-tinted, vibrant ♡
    // ═══════════════════════════════════════════
    inline constexpr auto DkBg         = "#1a0a14";   // deep rose black (NOT plum)
    inline constexpr auto DkCard       = "#261420";   // pink-tinted dark card
    inline constexpr auto DkBorder     = "#4d2d3d";   // visible pink border
    inline constexpr auto DkHover      = "#301d28";
    inline constexpr auto DkSelected   = "#3d1d33";   // pink selection glow
    inline constexpr auto DkText       = "#fce8f2";   // warm pink-white text
    inline constexpr auto DkMuted      = "#c090a8";   // pink-mauve muted

    // ═══════════════════════════════════════════
    // ♡ LIGHT THEME — soft pink dream ♡
    // ═══════════════════════════════════════════
    inline constexpr auto LtBg         = "#fff0f6";   // blush pink bg
    inline constexpr auto LtCard       = "#ffffff";
    inline constexpr auto LtBorder     = "#f0c8dd";   // pink border
    inline constexpr auto LtHover      = "#fff5fa";
    inline constexpr auto LtSelected   = "#ffe0ef";
    inline constexpr auto LtText       = "#2d1020";   // deep rose text
    inline constexpr auto LtMuted      = "#b088a0";
}

#endif
