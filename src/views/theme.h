#ifndef NEZHAGUARD_THEME_H
#define NEZHAGUARD_THEME_H

namespace Theme {


// ── 草莓奶霜 · 标题/主要强调 · 嫩嫩的亮粉 ──
inline constexpr auto Strawberry      = "#F298B0";
inline constexpr auto StrawberryLight = "#FAC0D0";
inline constexpr auto StrawberryPale  = "#FDE4EC";
inline constexpr auto StrawberryDeep  = "#E87894";

// ── 蜜桃粉 · 次要强调 · 暖暖的橘粉 ──
inline constexpr auto Peachy          = "#F8B0A0";
inline constexpr auto PeachyLight     = "#FCD0C4";
inline constexpr auto PeachyPale      = "#FEEAE4";
inline constexpr auto PeachyDeep      = "#E89080";

// ── 蔷薇粉 · hover/交互 · 冷调的深粉 ──
inline constexpr auto Rosy            = "#E888A0";
inline constexpr auto RosyLight       = "#F4B8C8";
inline constexpr auto RosyPale        = "#FCE4EA";
inline constexpr auto RosyDeep        = "#D06880";

// ── 珊瑚粉 · 告警/紧急 · 热烈的亮粉红 ──
inline constexpr auto Coral           = "#F06880";

// ── 薰衣草奶霜 · 辅助强调 · 软软的淡紫 ──
inline constexpr auto Lilac           = "#C8A8F0";
inline constexpr auto LilacLight      = "#E0C8F8";
inline constexpr auto LilacPale       = "#F2E8FC";
inline constexpr auto LilacDeep       = "#B080D8";

// ── 薄荷奶霜 · 辅助强调 · 清新的嫩绿 ──
inline constexpr auto Mint            = "#88D8BC";
inline constexpr auto MintLight       = "#B0E8D6";
inline constexpr auto MintPale        = "#E0F6EE";
inline constexpr auto MintDeep        = "#60B898";

// ── 天蓝奶霜 · 点缀 · 软软的淡蓝 ──
inline constexpr auto Sky             = "#98D0F4";
inline constexpr auto SkyLight        = "#C0E4F8";
inline constexpr auto SkyPale         = "#E4F4FC";

// ── Semantic ──
inline constexpr auto Cherry          = "#F05068";
inline constexpr auto Tangerine       = "#F09060";
inline constexpr auto Matcha          = "#68C890";
inline constexpr auto Iris            = "#B090E4";
inline constexpr auto Cream           = "#FFFAFC";
inline constexpr auto Mauve           = "#B498A8";

// ── Legacy aliases ──
inline constexpr auto Pink            = Strawberry;
inline constexpr auto PinkLight       = StrawberryLight;
inline constexpr auto PinkDeep        = StrawberryDeep;
inline constexpr auto PinkBlush       = StrawberryPale;
inline constexpr auto PinkHot         = Coral;
inline constexpr auto Sakura          = Strawberry;
inline constexpr auto SakuraLight     = StrawberryLight;
inline constexpr auto SakuraPale      = StrawberryPale;
inline constexpr auto SakuraDeep      = StrawberryDeep;
inline constexpr auto SakuraHot       = Coral;
inline constexpr auto Lavender        = Lilac;
inline constexpr auto LavenderLight   = LilacLight;
inline constexpr auto LavenderDeep    = LilacDeep;
inline constexpr auto Wisteria        = Lilac;
inline constexpr auto WisteriaLight   = LilacLight;
inline constexpr auto WisteriaPale    = LilacPale;
inline constexpr auto WisteriaDeep    = LilacDeep;
inline constexpr auto Cyan            = Mint;
inline constexpr auto CyanLight       = MintLight;
inline constexpr auto CyanDeep        = MintDeep;
inline constexpr auto CyanNeon        = Mint;
inline constexpr auto CyanIce         = MintPale;
inline constexpr auto Seafoam         = Mint;
inline constexpr auto SeafoamLight    = MintLight;
inline constexpr auto SeafoamPale     = MintPale;
inline constexpr auto SeafoamDeep     = MintDeep;
inline constexpr auto BabyBlue        = Sky;
inline constexpr auto BabyBlueLight   = SkyLight;
inline constexpr auto Red             = Cherry;
inline constexpr auto Orange          = Tangerine;
inline constexpr auto Green           = Matcha;
inline constexpr auto MintLight_      = MintLight;
inline constexpr auto MintDeep_       = MintDeep;
inline constexpr auto Purple          = Iris;
inline constexpr auto White           = Cream;
inline constexpr auto Grey            = Mauve;
inline constexpr auto PeachDeep       = PeachyDeep;

// ╔══════════════════════════════════════════════╗
// ║  暗色模式 · 深莓夜色 · 粉光氛围              ║
// ╚══════════════════════════════════════════════╝
inline constexpr auto DkBg            = "#0C0A10";
inline constexpr auto DkSheet         = "#14101C";
inline constexpr auto DkCard          = "#1C1628";
inline constexpr auto DkCardHover     = "#241E34";
inline constexpr auto DkBorder        = "#322840";
inline constexpr auto DkBorderHover   = "#524060";
inline constexpr auto DkSelected      = "#2A2038";
inline constexpr auto DkText          = "#F0EAF4";
inline constexpr auto DkMuted         = "#9488A0";

// ╔══════════════════════════════════════════════╗
// ║  浅色模式 · 奶白画布 · 粉彩点缀              ║
// ╚══════════════════════════════════════════════╝
inline constexpr auto LtBg            = "#FFFAFB";
inline constexpr auto LtSheet         = "#FFFDFD";
inline constexpr auto LtCard          = "#FFFFFF";
inline constexpr auto LtCardHover     = "#FFF5F7";
inline constexpr auto LtBorder        = "#F0E0E6";
inline constexpr auto LtBorderHover   = "#E4C8D4";
inline constexpr auto LtSelected      = "#FFEAF0";
inline constexpr auto LtText          = "#241820";
inline constexpr auto LtMuted         = "#B098A0";

inline constexpr auto DkHover         = DkCardHover;
inline constexpr auto LtHover         = LtCardHover;

} // namespace Theme

#endif // NEZHAGUARD_THEME_H