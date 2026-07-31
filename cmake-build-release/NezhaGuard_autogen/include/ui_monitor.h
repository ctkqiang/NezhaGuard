/********************************************************************************
** Form generated from reading UI file 'monitor.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MONITOR_H
#define UI_MONITOR_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_monitor
{
public:
    QWidget *centralwidget;
    QVBoxLayout *root_layout;
    QWidget *header;
    QHBoxLayout *header_layout;
    QLabel *app_title;
    QSpacerItem *header_spacer;
    QLabel *clock_label;
    QLabel *status_dot;
    QLabel *status_text;
    QWidget *body;
    QHBoxLayout *body_layout;
    QListWidget *sidebar;
    QStackedWidget *pages;
    QWidget *page_dashboard;
    QVBoxLayout *dash_layout;
    QLabel *dash_title;
    QHBoxLayout *cards_layout;
    QFrame *card_logs;
    QVBoxLayout *card_logs_layout;
    QLabel *card_logs_value;
    QLabel *card_logs_label;
    QFrame *card_alerts;
    QVBoxLayout *card_alerts_layout;
    QLabel *card_alerts_value;
    QLabel *card_alerts_label;
    QFrame *card_threats;
    QVBoxLayout *card_threats_layout;
    QLabel *card_threats_value;
    QLabel *card_threats_label;
    QFrame *card_uptime;
    QVBoxLayout *card_uptime_layout;
    QLabel *card_uptime_value;
    QLabel *card_uptime_label;
    QLabel *recent_alerts_label;
    QTableView *recent_alerts_view;
    QWidget *page_logs;
    QVBoxLayout *logs_layout;
    QLabel *logs_title;
    QHBoxLayout *logs_filter_layout;
    QLabel *logs_filter_label;
    QComboBox *level_filter;
    QSpacerItem *logs_spacer;
    QTableView *log_view;
    QWidget *page_alerts;
    QVBoxLayout *alerts_layout;
    QLabel *alerts_title;
    QTableView *alert_view;
    QWidget *page_honeypot;
    QVBoxLayout *honey_layout;
    QLabel *honey_title;
    QTableView *honey_view;
    QWidget *page_network;
    QVBoxLayout *network_layout;
    QHBoxLayout *network_header_layout;
    QLabel *network_title;
    QSpacerItem *network_spacer;
    QPushButton *refresh_network;
    QLabel *local_ip_label;
    QTableWidget *local_ip_table;
    QLabel *arp_label;
    QTableWidget *arp_table;
    QStatusBar *statusbar;
    QLabel *status_label;

    void setupUi(QMainWindow *monitor)
    {
        if (monitor->objectName().isEmpty())
            monitor->setObjectName("monitor");
        monitor->resize(1360, 860);
        centralwidget = new QWidget(monitor);
        centralwidget->setObjectName("centralwidget");
        root_layout = new QVBoxLayout(centralwidget);
        root_layout->setSpacing(0);
        root_layout->setObjectName("root_layout");
        root_layout->setContentsMargins(0, 0, 0, 0);
        header = new QWidget(centralwidget);
        header->setObjectName("header");
        header->setMinimumSize(QSize(0, 48));
        header->setMaximumSize(QSize(16777215, 48));
        header_layout = new QHBoxLayout(header);
        header_layout->setSpacing(8);
        header_layout->setObjectName("header_layout");
        header_layout->setContentsMargins(16, 0, 16, 0);
        app_title = new QLabel(header);
        app_title->setObjectName("app_title");

        header_layout->addWidget(app_title);

        header_spacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        header_layout->addItem(header_spacer);

        clock_label = new QLabel(header);
        clock_label->setObjectName("clock_label");

        header_layout->addWidget(clock_label);

        status_dot = new QLabel(header);
        status_dot->setObjectName("status_dot");
        status_dot->setMinimumSize(QSize(8, 8));
        status_dot->setMaximumSize(QSize(8, 8));

        header_layout->addWidget(status_dot);

        status_text = new QLabel(header);
        status_text->setObjectName("status_text");

        header_layout->addWidget(status_text);


        root_layout->addWidget(header);

        body = new QWidget(centralwidget);
        body->setObjectName("body");
        body_layout = new QHBoxLayout(body);
        body_layout->setSpacing(0);
        body_layout->setObjectName("body_layout");
        body_layout->setContentsMargins(0, 0, 0, 0);
        sidebar = new QListWidget(body);
        sidebar->setObjectName("sidebar");
        sidebar->setMinimumSize(QSize(200, 0));
        sidebar->setMaximumSize(QSize(200, 16777215));
        sidebar->setFrameShape(QFrame::NoFrame);

        body_layout->addWidget(sidebar);

        pages = new QStackedWidget(body);
        pages->setObjectName("pages");
        page_dashboard = new QWidget();
        page_dashboard->setObjectName("page_dashboard");
        dash_layout = new QVBoxLayout(page_dashboard);
        dash_layout->setSpacing(16);
        dash_layout->setObjectName("dash_layout");
        dash_layout->setContentsMargins(24, 24, 24, 24);
        dash_title = new QLabel(page_dashboard);
        dash_title->setObjectName("dash_title");

        dash_layout->addWidget(dash_title);

        cards_layout = new QHBoxLayout();
        cards_layout->setObjectName("cards_layout");
        card_logs = new QFrame(page_dashboard);
        card_logs->setObjectName("card_logs");
        card_logs->setFrameShape(QFrame::StyledPanel);
        card_logs_layout = new QVBoxLayout(card_logs);
        card_logs_layout->setObjectName("card_logs_layout");
        card_logs_value = new QLabel(card_logs);
        card_logs_value->setObjectName("card_logs_value");

        card_logs_layout->addWidget(card_logs_value);

        card_logs_label = new QLabel(card_logs);
        card_logs_label->setObjectName("card_logs_label");

        card_logs_layout->addWidget(card_logs_label);


        cards_layout->addWidget(card_logs);

        card_alerts = new QFrame(page_dashboard);
        card_alerts->setObjectName("card_alerts");
        card_alerts->setFrameShape(QFrame::StyledPanel);
        card_alerts_layout = new QVBoxLayout(card_alerts);
        card_alerts_layout->setObjectName("card_alerts_layout");
        card_alerts_value = new QLabel(card_alerts);
        card_alerts_value->setObjectName("card_alerts_value");

        card_alerts_layout->addWidget(card_alerts_value);

        card_alerts_label = new QLabel(card_alerts);
        card_alerts_label->setObjectName("card_alerts_label");

        card_alerts_layout->addWidget(card_alerts_label);


        cards_layout->addWidget(card_alerts);

        card_threats = new QFrame(page_dashboard);
        card_threats->setObjectName("card_threats");
        card_threats->setFrameShape(QFrame::StyledPanel);
        card_threats_layout = new QVBoxLayout(card_threats);
        card_threats_layout->setObjectName("card_threats_layout");
        card_threats_value = new QLabel(card_threats);
        card_threats_value->setObjectName("card_threats_value");

        card_threats_layout->addWidget(card_threats_value);

        card_threats_label = new QLabel(card_threats);
        card_threats_label->setObjectName("card_threats_label");

        card_threats_layout->addWidget(card_threats_label);


        cards_layout->addWidget(card_threats);

        card_uptime = new QFrame(page_dashboard);
        card_uptime->setObjectName("card_uptime");
        card_uptime->setFrameShape(QFrame::StyledPanel);
        card_uptime_layout = new QVBoxLayout(card_uptime);
        card_uptime_layout->setObjectName("card_uptime_layout");
        card_uptime_value = new QLabel(card_uptime);
        card_uptime_value->setObjectName("card_uptime_value");

        card_uptime_layout->addWidget(card_uptime_value);

        card_uptime_label = new QLabel(card_uptime);
        card_uptime_label->setObjectName("card_uptime_label");

        card_uptime_layout->addWidget(card_uptime_label);


        cards_layout->addWidget(card_uptime);


        dash_layout->addLayout(cards_layout);

        recent_alerts_label = new QLabel(page_dashboard);
        recent_alerts_label->setObjectName("recent_alerts_label");

        dash_layout->addWidget(recent_alerts_label);

        recent_alerts_view = new QTableView(page_dashboard);
        recent_alerts_view->setObjectName("recent_alerts_view");

        dash_layout->addWidget(recent_alerts_view);

        pages->addWidget(page_dashboard);
        page_logs = new QWidget();
        page_logs->setObjectName("page_logs");
        logs_layout = new QVBoxLayout(page_logs);
        logs_layout->setSpacing(12);
        logs_layout->setObjectName("logs_layout");
        logs_layout->setContentsMargins(24, 24, 24, 24);
        logs_title = new QLabel(page_logs);
        logs_title->setObjectName("logs_title");

        logs_layout->addWidget(logs_title);

        logs_filter_layout = new QHBoxLayout();
        logs_filter_layout->setObjectName("logs_filter_layout");
        logs_filter_label = new QLabel(page_logs);
        logs_filter_label->setObjectName("logs_filter_label");

        logs_filter_layout->addWidget(logs_filter_label);

        level_filter = new QComboBox(page_logs);
        level_filter->addItem(QString());
        level_filter->addItem(QString());
        level_filter->addItem(QString());
        level_filter->addItem(QString());
        level_filter->addItem(QString());
        level_filter->addItem(QString());
        level_filter->addItem(QString());
        level_filter->setObjectName("level_filter");

        logs_filter_layout->addWidget(level_filter);

        logs_spacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        logs_filter_layout->addItem(logs_spacer);


        logs_layout->addLayout(logs_filter_layout);

        log_view = new QTableView(page_logs);
        log_view->setObjectName("log_view");

        logs_layout->addWidget(log_view);

        pages->addWidget(page_logs);
        page_alerts = new QWidget();
        page_alerts->setObjectName("page_alerts");
        alerts_layout = new QVBoxLayout(page_alerts);
        alerts_layout->setSpacing(12);
        alerts_layout->setObjectName("alerts_layout");
        alerts_layout->setContentsMargins(24, 24, 24, 24);
        alerts_title = new QLabel(page_alerts);
        alerts_title->setObjectName("alerts_title");

        alerts_layout->addWidget(alerts_title);

        alert_view = new QTableView(page_alerts);
        alert_view->setObjectName("alert_view");

        alerts_layout->addWidget(alert_view);

        pages->addWidget(page_alerts);
        page_honeypot = new QWidget();
        page_honeypot->setObjectName("page_honeypot");
        honey_layout = new QVBoxLayout(page_honeypot);
        honey_layout->setSpacing(12);
        honey_layout->setObjectName("honey_layout");
        honey_layout->setContentsMargins(24, 24, 24, 24);
        honey_title = new QLabel(page_honeypot);
        honey_title->setObjectName("honey_title");

        honey_layout->addWidget(honey_title);

        honey_view = new QTableView(page_honeypot);
        honey_view->setObjectName("honey_view");

        honey_layout->addWidget(honey_view);

        pages->addWidget(page_honeypot);
        page_network = new QWidget();
        page_network->setObjectName("page_network");
        network_layout = new QVBoxLayout(page_network);
        network_layout->setSpacing(12);
        network_layout->setObjectName("network_layout");
        network_layout->setContentsMargins(24, 24, 24, 24);
        network_header_layout = new QHBoxLayout();
        network_header_layout->setObjectName("network_header_layout");
        network_title = new QLabel(page_network);
        network_title->setObjectName("network_title");

        network_header_layout->addWidget(network_title);

        network_spacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        network_header_layout->addItem(network_spacer);

        refresh_network = new QPushButton(page_network);
        refresh_network->setObjectName("refresh_network");

        network_header_layout->addWidget(refresh_network);


        network_layout->addLayout(network_header_layout);

        local_ip_label = new QLabel(page_network);
        local_ip_label->setObjectName("local_ip_label");

        network_layout->addWidget(local_ip_label);

        local_ip_table = new QTableWidget(page_network);
        if (local_ip_table->columnCount() < 2)
            local_ip_table->setColumnCount(2);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        local_ip_table->setHorizontalHeaderItem(0, __qtablewidgetitem);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        local_ip_table->setHorizontalHeaderItem(1, __qtablewidgetitem1);
        local_ip_table->setObjectName("local_ip_table");
        local_ip_table->setColumnCount(2);
        local_ip_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        local_ip_table->setSelectionMode(QAbstractItemView::SingleSelection);
        local_ip_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        local_ip_table->horizontalHeader()->setVisible(true);

        network_layout->addWidget(local_ip_table);

        arp_label = new QLabel(page_network);
        arp_label->setObjectName("arp_label");

        network_layout->addWidget(arp_label);

        arp_table = new QTableWidget(page_network);
        if (arp_table->columnCount() < 2)
            arp_table->setColumnCount(2);
        QTableWidgetItem *__qtablewidgetitem2 = new QTableWidgetItem();
        arp_table->setHorizontalHeaderItem(0, __qtablewidgetitem2);
        QTableWidgetItem *__qtablewidgetitem3 = new QTableWidgetItem();
        arp_table->setHorizontalHeaderItem(1, __qtablewidgetitem3);
        arp_table->setObjectName("arp_table");
        arp_table->setColumnCount(2);
        arp_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        arp_table->setSelectionMode(QAbstractItemView::SingleSelection);
        arp_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        arp_table->horizontalHeader()->setVisible(true);

        network_layout->addWidget(arp_table);

        pages->addWidget(page_network);

        body_layout->addWidget(pages);


        root_layout->addWidget(body);

        monitor->setCentralWidget(centralwidget);
        statusbar = new QStatusBar(monitor);
        statusbar->setObjectName("statusbar");
        status_label = new QLabel(statusbar);
        status_label->setObjectName("status_label");
        monitor->setStatusBar(statusbar);

        retranslateUi(monitor);

        QMetaObject::connectSlotsByName(monitor);
    } // setupUi

    void retranslateUi(QMainWindow *monitor)
    {
        monitor->setWindowTitle(QCoreApplication::translate("monitor", "NezhaGuard SIEM", nullptr));
        app_title->setText(QCoreApplication::translate("monitor", "NezhaGuard SIEM", nullptr));
        app_title->setStyleSheet(QCoreApplication::translate("monitor", "font-size:15px;font-weight:bold;", nullptr));
        clock_label->setText(QString());
        status_dot->setText(QString());
        status_text->setText(QCoreApplication::translate("monitor", "Running", nullptr));
        dash_title->setText(QCoreApplication::translate("monitor", "Dashboard", nullptr));
        card_logs_value->setText(QCoreApplication::translate("monitor", "0", nullptr));
        card_logs_label->setText(QCoreApplication::translate("monitor", "Total Logs", nullptr));
        card_alerts_value->setText(QCoreApplication::translate("monitor", "0", nullptr));
        card_alerts_label->setText(QCoreApplication::translate("monitor", "Alerts", nullptr));
        card_threats_value->setText(QCoreApplication::translate("monitor", "0", nullptr));
        card_threats_label->setText(QCoreApplication::translate("monitor", "Threats", nullptr));
        card_uptime_value->setText(QCoreApplication::translate("monitor", "00:00", nullptr));
        card_uptime_label->setText(QCoreApplication::translate("monitor", "Uptime", nullptr));
        recent_alerts_label->setText(QCoreApplication::translate("monitor", "Recent Alerts", nullptr));
        logs_title->setText(QCoreApplication::translate("monitor", "Log Monitor", nullptr));
        logs_filter_label->setText(QCoreApplication::translate("monitor", "Level", nullptr));
        level_filter->setItemText(0, QCoreApplication::translate("monitor", "ALL", nullptr));
        level_filter->setItemText(1, QCoreApplication::translate("monitor", "Trace", nullptr));
        level_filter->setItemText(2, QCoreApplication::translate("monitor", "Debug", nullptr));
        level_filter->setItemText(3, QCoreApplication::translate("monitor", "Info", nullptr));
        level_filter->setItemText(4, QCoreApplication::translate("monitor", "Warn", nullptr));
        level_filter->setItemText(5, QCoreApplication::translate("monitor", "Error", nullptr));
        level_filter->setItemText(6, QCoreApplication::translate("monitor", "Critical", nullptr));

        alerts_title->setText(QCoreApplication::translate("monitor", "Security Alerts", nullptr));
        honey_title->setText(QCoreApplication::translate("monitor", "Honeypot", nullptr));
        network_title->setText(QCoreApplication::translate("monitor", "Network", nullptr));
        refresh_network->setText(QCoreApplication::translate("monitor", "Refresh", nullptr));
        local_ip_label->setText(QCoreApplication::translate("monitor", "Local Interfaces", nullptr));
        QTableWidgetItem *___qtablewidgetitem = local_ip_table->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("monitor", "Interface", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = local_ip_table->horizontalHeaderItem(1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("monitor", "IP Address", nullptr));
        arp_label->setText(QCoreApplication::translate("monitor", "ARP Cache", nullptr));
        QTableWidgetItem *___qtablewidgetitem2 = arp_table->horizontalHeaderItem(0);
        ___qtablewidgetitem2->setText(QCoreApplication::translate("monitor", "IP Address", nullptr));
        QTableWidgetItem *___qtablewidgetitem3 = arp_table->horizontalHeaderItem(1);
        ___qtablewidgetitem3->setText(QCoreApplication::translate("monitor", "MAC Address", nullptr));
        status_label->setText(QCoreApplication::translate("monitor", "Ready", nullptr));
    } // retranslateUi

};

namespace Ui {
    class monitor: public Ui_monitor {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MONITOR_H
