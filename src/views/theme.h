#ifndef NEZHAGUARD_THEME_H
#define NEZHAGUARD_THEME_H

namespace Theme {

// ── Sakura (cherry blossom pink) ── primary accent ──
inline constexpr auto Sakura          = "#F2A0B6";
inline constexpr auto SakuraLight     = "#F9C8D6";
inline constexpr auto SakuraPale      = "#FDE4EB";
inline constexpr auto SakuraDeep      = "#D47890";
inline constexpr auto SakuraHot       = "#E84878";

// ── Wisteria (soft lavender) ── secondary accent ──
inline constexpr auto Wisteria        = "#B8A0E8";
inline constexpr auto WisteriaLight   = "#D0C0F8";
inline constexpr auto WisteriaPale    = "#EBE4FA";
inline constexpr auto WisteriaDeep    = "#9078D0";

// ── Seafoam (mint cyan) ── tertiary accent ──
inline constexpr auto Seafoam         = "#60D0B0";
inline constexpr auto SeafoamLight    = "#90E8D0";
inline constexpr auto SeafoamPale     = "#D0F8EE";
inline constexpr auto SeafoamDeep     = "#40A888";

// ── Sky (baby blue) ──
inline constexpr auto Sky             = "#78C0F0";
inline constexpr auto SkyLight        = "#A8DCF8";
inline constexpr auto SkyPale         = "#DCF0FC";

// ── Peach ──
inline constexpr auto Peach           = "#F8B088";
inline constexpr auto PeachLight      = "#FCD0B8";
inline constexpr auto PeachPale       = "#FEE8D8";

// ── Semantic ──
inline constexpr auto Cherry          = "#E85068";
inline constexpr auto Tangerine       = "#E88858";
inline constexpr auto Matcha          = "#50C888";
inline constexpr auto Iris            = "#A880E0";
inline constexpr auto Cream           = "#FEFAFC";
inline constexpr auto Mauve           = "#A890A0";

// ── Legacy aliases (keep compat) ──
inline constexpr auto Pink            = Sakura;
inline constexpr auto PinkLight       = SakuraLight;
inline constexpr auto PinkDeep        = SakuraDeep;
inline constexpr auto PinkBlush       = SakuraPale;
inline constexpr auto PinkHot         = SakuraHot;
inline constexpr auto Cyan            = Seafoam;
inline constexpr auto CyanLight       = SeafoamLight;
inline constexpr auto CyanDeep        = SeafoamDeep;
inline constexpr auto CyanNeon        = Seafoam;
inline constexpr auto CyanIce         = SeafoamPale;
inline constexpr auto Lavender        = Wisteria;
inline constexpr auto LavenderLight   = WisteriaLight;
inline constexpr auto LavenderDeep    = WisteriaDeep;
inline constexpr auto Lilac           = WisteriaLight;
inline constexpr auto BabyBlue        = Sky;
inline constexpr auto BabyBlueLight   = SkyLight;
inline constexpr auto Mint            = Seafoam;
inline constexpr auto MintLight       = SeafoamLight;
inline constexpr auto MintDeep        = SeafoamDeep;
inline constexpr auto Red             = Cherry;
inline constexpr auto Orange          = Tangerine;
inline constexpr auto Green           = Matcha;
inline constexpr auto Purple          = Iris;
inline constexpr auto White           = Cream;
inline constexpr auto Grey            = Mauve;
inline constexpr auto PeachDeep       = Peach;

// ── Dark theme ──
inline constexpr auto DkBg            = "#0D0710";
inline constexpr auto DkSheet         = "#160E18";
inline constexpr auto DkCard          = "#1C121E";
inline constexpr auto DkCardHover     = "#241824";
inline constexpr auto DkBorder        = "#382838";
inline constexpr auto DkBorderHover   = "#584858";
inline constexpr auto DkSelected      = "#302030";
inline constexpr auto DkText          = "#F4E8F2";
inline constexpr auto DkMuted         = "#A08898";

// ── Light theme ──
inline constexpr auto LtBg            = "#FFF4F8";
inline constexpr auto LtSheet         = "#FFFAFC";
inline constexpr auto LtCard          = "#FFFFFF";
inline constexpr auto LtCardHover     = "#FFF0F5";
inline constexpr auto LtBorder        = "#F0D8E2";
inline constexpr auto LtBorderHover   = "#E0C0D0";
inline constexpr auto LtSelected      = "#FFE8F0";
inline constexpr auto LtText          = "#28101C";
inline constexpr auto LtMuted         = "#A88095";

// legacy hover names (reference DkCardHover/LtCardHover above)
inline constexpr auto DkHover         = DkCardHover;
inline constexpr auto LtHover         = LtCardHover;

// ── Glass alpha variants (dark theme) ──
inline constexpr auto GlassDk         = "rgba(28,18,30,0.85)";
inline constexpr auto GlassDkCard     = "rgba(28,18,30,0.65)";
inline constexpr auto GlassDkHover    = "rgba(36,24,36,0.80)";
inline constexpr auto GlassBorder     = "rgba(248,160,182,0.18)";

// ── Glass alpha variants (light theme) ──
inline constexpr auto GlassLt         = "rgba(255,255,255,0.85)";
inline constexpr auto GlassLtCard     = "rgba(255,255,255,0.72)";
inline constexpr auto GlassLtHover    = "rgba(255,240,245,0.88)";
inline constexpr auto GlassBorderLt   = "rgba(210,120,150,0.15)";

} // namespace Theme

#endif // NEZHAGUARD_THEME_H