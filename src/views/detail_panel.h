#ifndef NEZHAGUARD_DETAIL_PANEL_H
#define NEZHAGUARD_DETAIL_PANEL_H

#include <QTextBrowser>
#include <QString>
#include <QStringList>

#include "../core/geo_ip.h"

class DetailPanel : public QTextBrowser {
    Q_OBJECT
public:
    explicit DetailPanel(QWidget *parent = nullptr);

    void show_log(const QString &timestamp, const QString &level, const QString &message,
                  const QStringList &ips);
    void show_alert(const QString &timestamp, const QString &level, const QString &message);
    void show_honeypot(const QString &timestamp, const QString &message);
    void add_geo_info(const QString &ip, const QString &hostname,
                      const Nezha::Core::GeoIPResult &geo);
    void clear();
    void set_dark(bool dark);

private:
    QString card_html(const QString &title, const QString &accentColor, const QString &body) const;
    QString row(const QString &label, const QString &value, const QString &color) const;
    QString badge(const QString &text, const QString &bg, const QString &fg) const;
    QString css() const;

    bool dark_ = true;
    QString pending_ip_;
    QString pending_msg_;
};

#endif
