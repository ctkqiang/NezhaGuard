#include "detail_panel.h"
#include "theme.h"
#include "../core/ipaddr.h"

#include <QScrollBar>

DetailPanel::DetailPanel(QWidget *parent) : QTextBrowser(parent) {
    setOpenExternalLinks(false);
    setOpenLinks(false);
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void DetailPanel::set_dark(bool dark) {
    dark_ = dark;
}

void DetailPanel::clear() {
    QTextBrowser::clear();
    pending_ip_.clear();
    pending_msg_.clear();
}

QString DetailPanel::css() const {
    return QStringLiteral(R"(
        body { margin:0; padding:12px 16px; background:%1; color:%2;
               font-family:"SF Mono","Menlo","Cascadia Code",monospace;
               font-size:11px; line-height:1.8; }
        .card { background:%3; border:1px solid %4; border-radius:18px;
                margin-bottom:10px; overflow:hidden; }
        .card-header { padding:12px 18px; font-weight:700; font-size:11px;
                       border-bottom:1px solid %4; letter-spacing:0.4px;
                       display:flex; align-items:center; }
        .card-header::before { content:''; display:inline-block; width:6px; height:6px;
                               border-radius:3px; margin-right:8px; }
        .card-body { padding:12px 18px; }
        table { width:100%%; border-collapse:collapse; }
        tr { border-bottom:1px solid %4; }
        tr:last-child { border-bottom:none; }
        td { padding:6px 8px 6px 0; vertical-align:top; }
        td.k { color:%5; font-weight:600; white-space:nowrap; width:1%%;
               font-size:10px; text-transform:uppercase; letter-spacing:0.5px; }
        td.v { color:%2; word-break:break-all; }
        .ip-badge { display:inline-block; padding:2px 8px; border-radius:10px;
                    font-weight:600; font-size:10px; margin:2px 4px 2px 0; }
        .tag { display:inline-block; padding:2px 10px; border-radius:8px;
               font-size:10px; font-weight:600; letter-spacing:0.3px; }
    )").arg(dark_ ? Theme::DkBg : Theme::LtBg,
            dark_ ? Theme::DkText : Theme::LtText,
            dark_ ? Theme::DkCard : Theme::LtCard,
            dark_ ? Theme::DkBorder : Theme::LtBorder,
            dark_ ? Theme::DkMuted : Theme::LtMuted);
}

QString DetailPanel::card_html(const QString &title, const QString &accentColor,
                               const QString &body) const {
    return QStringLiteral(
        "<div class='card'>"
        "<div class='card-header' style='color:%1; border-left:3px solid %1; padding-left:15px;'>"
        "%2</div>"
        "<div class='card-body'>%3</div>"
        "</div>"
    ).arg(accentColor, title, body);
}

QString DetailPanel::row(const QString &label, const QString &value,
                         const QString &color) const {
    return QStringLiteral(
        "<tr><td class='k'>%1</td><td class='v' style='color:%2;'>%3</td></tr>"
    ).arg(label.toHtmlEscaped(), color, value.toHtmlEscaped());
}

QString DetailPanel::badge(const QString &text, const QString &bg, const QString &fg) const {
    return QStringLiteral(
        "<span class='tag' style='background:%1; color:%2;'>%3</span>"
    ).arg(bg, fg, text.toHtmlEscaped());
}

void DetailPanel::show_log(const QString &timestamp, const QString &level,
                           const QString &message, const QStringList &ips) {
    QString lvlColor = level.startsWith(QStringLiteral("ERR")) || level.startsWith(QStringLiteral("CRIT"))
        ? Theme::Cherry : level.startsWith(QStringLiteral("WARN"))
        ? Theme::Tangerine : Theme::Seafoam;

    QString body;
    body += QStringLiteral("<table>");
    body += row(QStringLiteral("时间"), timestamp,
                dark_ ? Theme::DkMuted : Theme::LtMuted);
    body += row(QStringLiteral("级别"),
                badge(level,
                      dark_ ? QStringLiteral("rgba(232,80,104,0.18)") : QStringLiteral("rgba(212,120,144,0.15)"),
                      lvlColor),
                QString());
    body += row(QStringLiteral("内容"), message,
                dark_ ? Theme::DkText : Theme::LtText);
    body += QStringLiteral("</table>");

    if (!ips.isEmpty()) {
        body += QStringLiteral("<div style='margin-top:8px;'>");
        for (const auto &ip : ips) {
            std::string ip_std = ip.toStdString();
            Nezha::IPAddress::ipaddr addr;
            Nezha::IPAddress::ipaddr::parse(ip_std, addr);
            QString scope = addr.is_private() ? QStringLiteral("内网")
                          : addr.is_loopback() ? QStringLiteral("回环")
                          : QStringLiteral("公网");
            QString scopeColor = addr.is_private()
                ? (dark_ ? Theme::Sakura : Theme::SakuraDeep)
                : (dark_ ? Theme::Seafoam : Theme::SeafoamDeep);
            body += badge(QStringLiteral("%1  %2").arg(ip, scope),
                          dark_ ? QStringLiteral("rgba(184,160,232,0.12)") : QStringLiteral("rgba(144,120,208,0.10)"),
                          scopeColor);
            body += QStringLiteral(" ");
        }
        body += QStringLiteral("</div>");
    }

    QString accent = dark_ ? Theme::Sakura : Theme::SakuraDeep;
    setHtml(QStringLiteral("<html><head><style>%1</style></head><body>%2</body></html>")
        .arg(css(), card_html(QStringLiteral("日志详情"), accent, body)));

    if (!ips.isEmpty()) {
        pending_ip_ = ips.first();
        pending_msg_ = message;
    }
}

void DetailPanel::show_alert(const QString &timestamp, const QString &level,
                             const QString &message) {
    QString lvlColor = level.startsWith(QStringLiteral("ERR")) || level.startsWith(QStringLiteral("CRIT"))
        ? Theme::Cherry : level.startsWith(QStringLiteral("WARN"))
        ? Theme::Tangerine : Theme::Sakura;

    QString body;
    body += QStringLiteral("<table>");
    body += row(QStringLiteral("时间"), timestamp,
                dark_ ? Theme::DkMuted : Theme::LtMuted);
    body += row(QStringLiteral("级别"),
                badge(level,
                      dark_ ? QStringLiteral("rgba(232,80,104,0.18)") : QStringLiteral("rgba(212,120,144,0.15)"),
                      lvlColor),
                QString());
    body += row(QStringLiteral("详情"), message,
                dark_ ? Theme::DkText : Theme::LtText);
    body += QStringLiteral("</table>");

    QString accent = dark_ ? Theme::SakuraHot : Theme::SakuraHot;
    setHtml(QStringLiteral("<html><head><style>%1</style></head><body>%2</body></html>")
        .arg(css(), card_html(QStringLiteral("告警详情"), accent, body)));
}

void DetailPanel::show_honeypot(const QString &timestamp, const QString &message) {
    QString body;
    body += QStringLiteral("<table>");
    body += row(QStringLiteral("时间"), timestamp,
                dark_ ? Theme::DkMuted : Theme::LtMuted);
    body += row(QStringLiteral("事件"), message,
                dark_ ? Theme::DkText : Theme::LtText);
    body += QStringLiteral("</table>");

    QString accent = dark_ ? Theme::Wisteria : Theme::WisteriaDeep;
    setHtml(QStringLiteral("<html><head><style>%1</style></head><body>%2</body></html>")
        .arg(css(), card_html(QStringLiteral("蜜罐事件"), accent, body)));
}

void DetailPanel::add_geo_info(const QString &ip, const QString &hostname,
                               const Nezha::Core::GeoIPResult &geo) {
    QString geoBody;
    geoBody += QStringLiteral("<table>");
    if (!hostname.isEmpty() && hostname.toStdString() != ip.toStdString())
        geoBody += row(QStringLiteral("主机名"), hostname,
                       dark_ ? Theme::DkText : Theme::LtText);
    if (geo.valid) {
        geoBody += row(QStringLiteral("国家"),
                       QStringLiteral("%1 (%2)")
                           .arg(QString::fromStdString(geo.country),
                                QString::fromStdString(geo.country_code)),
                       dark_ ? Theme::SakuraLight : Theme::SakuraDeep);
        if (!geo.city.empty())
            geoBody += row(QStringLiteral("城市"),
                           QString::fromStdString(geo.city),
                           dark_ ? Theme::DkText : Theme::LtText);
        if (geo.lat != 0.0 || geo.lon != 0.0)
            geoBody += row(QStringLiteral("坐标"),
                           QStringLiteral("%1, %2").arg(geo.lat, 0, 'f', 4).arg(geo.lon, 0, 'f', 4),
                           dark_ ? Theme::DkMuted : Theme::LtMuted);
        if (!geo.isp.empty())
            geoBody += row(QStringLiteral("ISP"),
                           QString::fromStdString(geo.isp),
                           dark_ ? Theme::DkText : Theme::LtText);
    } else {
        geoBody += row(QStringLiteral("状态"), QStringLiteral("无地理信息数据"),
                       dark_ ? Theme::DkMuted : Theme::LtMuted);
    }
    geoBody += QStringLiteral("</table>");

    QString accent = dark_ ? Theme::Seafoam : Theme::SeafoamDeep;
    QString geoCard = card_html(QStringLiteral("地理定位"), accent, geoBody);

    QString evtCard;
    if (!pending_msg_.isEmpty()) {
        evtCard = card_html(QStringLiteral("原始日志"),
                            dark_ ? Theme::DkMuted : Theme::LtMuted,
                            pending_msg_.toHtmlEscaped());
    }

    setHtml(QStringLiteral("<html><head><style>%1</style></head><body>%2%3</body></html>")
        .arg(css(), geoCard, evtCard));
}
