#ifndef NEZHAGUARD_THEME_H
#define NEZHAGUARD_THEME_H

namespace Theme {
    // pink family (primary accent)
    inline constexpr auto Pink         = "#f060a0";   // readable kawaii pink
    inline constexpr auto PinkLight    = "#f8a0c8";   // soft petal pink
    inline constexpr auto PinkDeep     = "#d04880";   // deep rose (light theme accent)
    inline constexpr auto PinkBlush    = "#ffe0ef";   // blush tint
    inline constexpr auto PinkHot      = "#e83070";   // hot pink (crit/alert)

    // cyan family (secondary accent)
    inline constexpr auto Cyan         = "#30c8a0";   // readable kawaii cyan
    inline constexpr auto CyanLight    = "#70e0c8";   // soft mint cyan
    inline constexpr auto CyanDeep     = "#209878";   // deep teal (light theme accent)
    inline constexpr auto CyanNeon     = "#18d8a8";   // dark theme cyan glow — visible on dark
    inline constexpr auto CyanIce      = "#a0f0e0";

    // lavender / purple
    inline constexpr auto Lavender     = "#b888f0";
    inline constexpr auto LavenderLight= "#d4b8ff";
    inline constexpr auto LavenderDeep = "#9060d8";
    inline constexpr auto Lilac        = "#c8a0f8";

    // baby blue
    inline constexpr auto BabyBlue     = "#60b8f0";
    inline constexpr auto BabyBlueLight= "#90d0f8";

    // mint / green
    inline constexpr auto Mint         = "#50d898";
    inline constexpr auto MintLight    = "#88f0c0";
    inline constexpr auto MintDeep     = "#38b878";

    // peach / coral
    inline constexpr auto Peach        = "#f8a878";
    inline constexpr auto PeachLight   = "#fcc8a8";
    inline constexpr auto PeachDeep    = "#d88858";

    // semantic
    inline constexpr auto Red          = "#e84060";   // readable cherry
    inline constexpr auto Orange       = "#e87850";   // readable tangerine
    inline constexpr auto Green        = "#40c878";   // readable mint
    inline constexpr auto Purple       = "#a870e0";   // readable wisteria
    inline constexpr auto White        = "#fef8fb";   // cream white
    inline constexpr auto Grey         = "#988898";   // mauve grey

    // dark theme
    inline constexpr auto DkBg         = "#120810";   // deep rose black
    inline constexpr auto DkCard       = "#1e1018";   // pink-tinted card
    inline constexpr auto DkBorder     = "#422838";   // visible pink border
    inline constexpr auto DkHover      = "#2a1825";
    inline constexpr auto DkSelected   = "#351d30";
    inline constexpr auto DkText       = "#f8e8f0";   // warm pink-white
    inline constexpr auto DkMuted      = "#b890a0";

    // light theme
    inline constexpr auto LtBg         = "#fff2f8";   // blush pink
    inline constexpr auto LtCard       = "#ffffff";
    inline constexpr auto LtBorder     = "#ecc8d8";
    inline constexpr auto LtHover      = "#fff5fa";
    inline constexpr auto LtSelected   = "#ffe4f0";
    inline constexpr auto LtText       = "#2a101d";
    inline constexpr auto LtMuted      = "#a88095";
}

#endif
