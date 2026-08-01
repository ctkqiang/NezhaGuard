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
        body { margin:0; padding:8px 12px; background:%1; color:%2;
               font-family:"Menlo","SF Mono",monospace; font-size:11px; line-height:1.6; }
        .card { background:%3; border:1px solid %4; border-radius:10px; margin-bottom:6px; overflow:hidden; }
        .card-header { padding:7px 12px; font-weight:700; font-size:11px;
                       border-bottom:1px solid %4; }
        .card-body { padding:6px 12px; }
        table { width:100%%; border-collapse:collapse; }
        td { padding:3px 6px 3px 0; vertical-align:top; }
        td.k { color:%5; font-weight:600; white-space:nowrap; width:1%%; }
        td.v { color:%2; word-break:break-all; }
    )").arg(dark_ ? Theme::DkBg : Theme::LtBg,
            dark_ ? Theme::DkText : Theme::LtText,
            dark_ ? Theme::DkCard : Theme::LtCard,
            dark_ ? Theme::DkBorder : Theme::LtBorder,
            dark_ ? Theme::DkMuted : Theme::LtMuted);
}

QString DetailPanel::card_html(const QString &title, const QString &titleColor,
                               const QString &body) const {
    return QStringLiteral(
        "<div class='card'>"
        "<div class='card-header' style='color:%1;'>%2</div>"
        "<div class='card-body'>%3</div>"
        "</div>"
    ).arg(titleColor, title, body);
}

QString DetailPanel::row(const QString &label, const QString &value,
                          const QString &color) const {
    return QStringLiteral("<tr><td class='k'>%1</td><td class='v' style='color:%2;'>%3</td></tr>")
        .arg(label.toHtmlEscaped(), color, value.toHtmlEscaped());
}

void DetailPanel::show_log(const QString &timestamp, const QString &level,
                           const QString &message, const QStringList &ips) {
    QString body = QStringLiteral("<table>");
    body += row(QStringLiteral("时间"), timestamp, Theme::Pink);
    body += row(QStringLiteral("级别"), level, Theme::Pink);
    body += row(QStringLiteral("内容"), message,
                dark_ ? Theme::DkText : Theme::LtText);
    body += QStringLiteral("</table>");

    if (!ips.isEmpty()) {
        body += QStringLiteral("<br/>");
        body += QStringLiteral("<table>");
        for (const auto &ip : ips) {
            std::string ip_std = ip.toStdString();
            Nezha::IPAddress::ipaddr addr;
            Nezha::IPAddress::ipaddr::parse(ip_std, addr);
            QString scope = addr.is_private() ? QStringLiteral("内网")
                          : addr.is_loopback() ? QStringLiteral("回环")
                          : QStringLiteral("公网");
            body += row(QStringLiteral("地址"), ip,
                        dark_ ? Theme::DkText : Theme::LtText);
            body += row(QStringLiteral("类型"), scope,
                        addr.is_private() ? Theme::Pink : Theme::Green);
        }
        body += QStringLiteral("</table>");
    }

    QString html = QStringLiteral("<html><head><style>%1</style></head><body>%2</body></html>")
        .arg(css(), card_html(QStringLiteral("日志详情"), Theme::Pink, body));
    setHtml(html);

    if (!ips.isEmpty()) {
        pending_ip_ = ips.first();
        pending_msg_ = message;
    }
}

void DetailPanel::show_alert(const QString &timestamp, const QString &level,
                             const QString &message) {
    QString body = QStringLiteral("<table>");
    body += row(QStringLiteral("时间"), timestamp, Theme::Pink);
    body += row(QStringLiteral("级别"), level, Theme::Pink);
    body += row(QStringLiteral("详情"), message,
                dark_ ? Theme::DkText : Theme::LtText);
    body += QStringLiteral("</table>");

    QString html = QStringLiteral("<html><head><style>%1</style></head><body>%2</body></html>")
        .arg(css(), card_html(QStringLiteral("告警详情"), Theme::Pink, body));
    setHtml(html);
}

void DetailPanel::show_honeypot(const QString &timestamp, const QString &message) {
    QString body = QStringLiteral("<table>");
    body += row(QStringLiteral("时间"), timestamp, Theme::Pink);
    body += row(QStringLiteral("事件"), message,
                dark_ ? Theme::DkText : Theme::LtText);
    body += QStringLiteral("</table>");

    QString html = QStringLiteral("<html><head><style>%1</style></head><body>%2</body></html>")
        .arg(css(), card_html(QStringLiteral("蜜罐详情"), Theme::Pink, body));
    setHtml(html);
}

void DetailPanel::add_geo_info(const QString &ip, const QString &hostname,
                               const Nezha::Core::GeoIPResult &geo) {
    QString body = QStringLiteral("<table>");
    if (!hostname.isEmpty() && hostname.toStdString() != ip.toStdString())
        body += row(QStringLiteral("主机名"), hostname,
                    dark_ ? Theme::DkText : Theme::LtText);
    if (geo.valid) {
        body += row(QStringLiteral("国家"),
                    QStringLiteral("%1 (%2)")
                        .arg(QString::fromStdString(geo.country),
                             QString::fromStdString(geo.country_code)),
                    dark_ ? Theme::DkText : Theme::LtText);
        if (!geo.city.empty())
            body += row(QStringLiteral("城市"),
                        QString::fromStdString(geo.city),
                        dark_ ? Theme::DkText : Theme::LtText);
        if (geo.lat != 0.0 || geo.lon != 0.0)
            body += row(QStringLiteral("坐标"),
                        QStringLiteral("%1, %2").arg(geo.lat, 0, 'f', 4).arg(geo.lon, 0, 'f', 4),
                        dark_ ? Theme::DkText : Theme::LtText);
        if (!geo.isp.empty())
            body += row(QStringLiteral("ISP"),
                        QString::fromStdString(geo.isp),
                        dark_ ? Theme::DkText : Theme::LtText);
    }
    body += QStringLiteral("</table>");

    QString html = QStringLiteral("<html><head><style>%1</style></head><body>%2%3</body></html>")
        .arg(css(),
             card_html(QStringLiteral("GeoIP — %1").arg(ip), Theme::Pink, body),
             !pending_msg_.isEmpty()
                ? card_html(QStringLiteral("原始事件"), Theme::DkMuted,
                            pending_msg_.toHtmlEscaped())
                : QString());
    setHtml(html);
}
