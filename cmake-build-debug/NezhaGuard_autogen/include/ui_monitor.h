/********************************************************************************
** Form generated from reading UI file 'monitor.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MONITOR_H
#define UI_MONITOR_H

#include <QtCharts/QChartView>
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
#include <QtWidgets/QTextEdit>
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
    QLabel *brand_badge;
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
    QChartView *alert_chart;
    QLabel *recent_alerts_label;
    QTableView *recent_alerts_view;
    QWidget *page_logs;
    QVBoxLayout *logs_layout;
    QHBoxLayout *logs_filter_layout;
    QLabel *logs_title;
    QSpacerItem *logs_spacer;
    QLabel *logs_filter_label;
    QComboBox *level_filter;
    QTableView *log_view;
    QWidget *page_alerts;
    QVBoxLayout *alerts_layout;
    QHBoxLayout *alerts_header_layout;
    QLabel *alerts_title;
    QLabel *alert_stats_label;
    QSpacerItem *alts_hdr_spc;
    QLabel *alert_filter_label;
    QComboBox *alert_severity_filter;
    QTableView *alert_view;
    QTextEdit *alert_detail;
    QWidget *page_honeypot;
    QVBoxLayout *honey_layout;
    QHBoxLayout *honey_header_layout;
    QLabel *honey_title;
    QLabel *honey_stats_label;
    QSpacerItem *hny_hdr_spc;
    QTableView *honey_view;
    QTextEdit *honey_detail;
    QWidget *page_network;
    QVBoxLayout *network_layout;
    QHBoxLayout *network_header_layout;
    QLabel *network_title;
    QSpacerItem *net_hdr_spc;
    QPushButton *refresh_network;
    QLabel *local_ip_label;
    QTableWidget *local_ip_table;
    QLabel *arp_label;
    QTableWidget *arp_table;
    QLabel *quarantine_label;
    QTableWidget *quarantine_table;
    QStatusBar *statusbar;
    QLabel *status_label;

    void setupUi(QMainWindow *monitor)
    {
        if (monitor->objectName().isEmpty())
            monitor->setObjectName("monitor");
        monitor->resize(1100, 740);
        monitor->setMinimumSize(QSize(960, 640));
        centralwidget = new QWidget(monitor);
        centralwidget->setObjectName("centralwidget");
        root_layout = new QVBoxLayout(centralwidget);
        root_layout->setSpacing(0);
        root_layout->setObjectName("root_layout");
        root_layout->setContentsMargins(0, 0, 0, 0);
        header = new QWidget(centralwidget);
        header->setObjectName("header");
        header->setMinimumSize(QSize(0, 56));
        header->setMaximumSize(QSize(16777215, 56));
        header_layout = new QHBoxLayout(header);
        header_layout->setSpacing(10);
        header_layout->setObjectName("header_layout");
        header_layout->setContentsMargins(20, 0, 20, 0);
        brand_badge = new QLabel(header);
        brand_badge->setObjectName("brand_badge");
        brand_badge->setMinimumSize(QSize(26, 26));
        brand_badge->setMaximumSize(QSize(26, 26));
        brand_badge->setAlignment(Qt::AlignCenter);

        header_layout->addWidget(brand_badge);

        app_title = new QLabel(header);
        app_title->setObjectName("app_title");

        header_layout->addWidget(app_title);

        header_spacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

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
        QListWidgetItem *__qlistwidgetitem = new QListWidgetItem(sidebar);
        __qlistwidgetitem->setTextAlignment(Qt::AlignLeading|Qt::AlignVCenter);
        QListWidgetItem *__qlistwidgetitem1 = new QListWidgetItem(sidebar);
        __qlistwidgetitem1->setTextAlignment(Qt::AlignLeading|Qt::AlignVCenter);
        QListWidgetItem *__qlistwidgetitem2 = new QListWidgetItem(sidebar);
        __qlistwidgetitem2->setTextAlignment(Qt::AlignLeading|Qt::AlignVCenter);
        QListWidgetItem *__qlistwidgetitem3 = new QListWidgetItem(sidebar);
        __qlistwidgetitem3->setTextAlignment(Qt::AlignLeading|Qt::AlignVCenter);
        QListWidgetItem *__qlistwidgetitem4 = new QListWidgetItem(sidebar);
        __qlistwidgetitem4->setTextAlignment(Qt::AlignLeading|Qt::AlignVCenter);
        sidebar->setObjectName("sidebar");
        sidebar->setMinimumSize(QSize(200, 0));
        sidebar->setMaximumSize(QSize(200, 16777215));
        sidebar->setFrameShape(QFrame::NoFrame);
        sidebar->setSelectionMode(QAbstractItemView::SingleSelection);
        sidebar->setSelectionBehavior(QAbstractItemView::SelectRows);
        sidebar->setSpacing(0);

        body_layout->addWidget(sidebar);

        pages = new QStackedWidget(body);
        pages->setObjectName("pages");
        page_dashboard = new QWidget();
        page_dashboard->setObjectName("page_dashboard");
        dash_layout = new QVBoxLayout(page_dashboard);
        dash_layout->setSpacing(16);
        dash_layout->setObjectName("dash_layout");
        dash_layout->setContentsMargins(24, 20, 24, 20);
        dash_title = new QLabel(page_dashboard);
        dash_title->setObjectName("dash_title");

        dash_layout->addWidget(dash_title);

        cards_layout = new QHBoxLayout();
        cards_layout->setSpacing(16);
        cards_layout->setObjectName("cards_layout");
        card_logs = new QFrame(page_dashboard);
        card_logs->setObjectName("card_logs");
        card_logs->setMinimumSize(QSize(0, 92));
        card_logs->setFrameShape(QFrame::NoFrame);
        card_logs_layout = new QVBoxLayout(card_logs);
        card_logs_layout->setSpacing(4);
        card_logs_layout->setObjectName("card_logs_layout");
        card_logs_layout->setContentsMargins(18, 16, 16, 16);
        card_logs_value = new QLabel(card_logs);
        card_logs_value->setObjectName("card_logs_value");

        card_logs_layout->addWidget(card_logs_value);

        card_logs_label = new QLabel(card_logs);
        card_logs_label->setObjectName("card_logs_label");

        card_logs_layout->addWidget(card_logs_label);


        cards_layout->addWidget(card_logs);

        card_alerts = new QFrame(page_dashboard);
        card_alerts->setObjectName("card_alerts");
        card_alerts->setMinimumSize(QSize(0, 92));
        card_alerts->setFrameShape(QFrame::NoFrame);
        card_alerts_layout = new QVBoxLayout(card_alerts);
        card_alerts_layout->setSpacing(4);
        card_alerts_layout->setObjectName("card_alerts_layout");
        card_alerts_layout->setContentsMargins(18, 16, 16, 16);
        card_alerts_value = new QLabel(card_alerts);
        card_alerts_value->setObjectName("card_alerts_value");

        card_alerts_layout->addWidget(card_alerts_value);

        card_alerts_label = new QLabel(card_alerts);
        card_alerts_label->setObjectName("card_alerts_label");

        card_alerts_layout->addWidget(card_alerts_label);


        cards_layout->addWidget(card_alerts);

        card_threats = new QFrame(page_dashboard);
        card_threats->setObjectName("card_threats");
        card_threats->setMinimumSize(QSize(0, 92));
        card_threats->setFrameShape(QFrame::NoFrame);
        card_threats_layout = new QVBoxLayout(card_threats);
        card_threats_layout->setSpacing(4);
        card_threats_layout->setObjectName("card_threats_layout");
        card_threats_layout->setContentsMargins(18, 16, 16, 16);
        card_threats_value = new QLabel(card_threats);
        card_threats_value->setObjectName("card_threats_value");

        card_threats_layout->addWidget(card_threats_value);

        card_threats_label = new QLabel(card_threats);
        card_threats_label->setObjectName("card_threats_label");

        card_threats_layout->addWidget(card_threats_label);


        cards_layout->addWidget(card_threats);

        card_uptime = new QFrame(page_dashboard);
        card_uptime->setObjectName("card_uptime");
        card_uptime->setMinimumSize(QSize(0, 92));
        card_uptime->setFrameShape(QFrame::NoFrame);
        card_uptime_layout = new QVBoxLayout(card_uptime);
        card_uptime_layout->setSpacing(4);
        card_uptime_layout->setObjectName("card_uptime_layout");
        card_uptime_layout->setContentsMargins(18, 16, 16, 16);
        card_uptime_value = new QLabel(card_uptime);
        card_uptime_value->setObjectName("card_uptime_value");

        card_uptime_layout->addWidget(card_uptime_value);

        card_uptime_label = new QLabel(card_uptime);
        card_uptime_label->setObjectName("card_uptime_label");

        card_uptime_layout->addWidget(card_uptime_label);


        cards_layout->addWidget(card_uptime);


        dash_layout->addLayout(cards_layout);

        alert_chart = new QChartView(page_dashboard);
        alert_chart->setObjectName("alert_chart");
        alert_chart->setMinimumSize(QSize(0, 160));
        alert_chart->setMaximumSize(QSize(16777215, 160));

        dash_layout->addWidget(alert_chart);

        recent_alerts_label = new QLabel(page_dashboard);
        recent_alerts_label->setObjectName("recent_alerts_label");

        dash_layout->addWidget(recent_alerts_label);

        recent_alerts_view = new QTableView(page_dashboard);
        recent_alerts_view->setObjectName("recent_alerts_view");
        recent_alerts_view->setFrameShape(QFrame::NoFrame);
        recent_alerts_view->setAlternatingRowColors(true);
        recent_alerts_view->setShowGrid(false);
        recent_alerts_view->setSelectionBehavior(QAbstractItemView::SelectRows);
        recent_alerts_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        recent_alerts_view->horizontalHeader()->setHighlightSections(false);
        recent_alerts_view->horizontalHeader()->setStretchLastSection(true);
        recent_alerts_view->verticalHeader()->setVisible(false);

        dash_layout->addWidget(recent_alerts_view);

        pages->addWidget(page_dashboard);
        page_logs = new QWidget();
        page_logs->setObjectName("page_logs");
        logs_layout = new QVBoxLayout(page_logs);
        logs_layout->setSpacing(12);
        logs_layout->setObjectName("logs_layout");
        logs_layout->setContentsMargins(24, 20, 24, 20);
        logs_filter_layout = new QHBoxLayout();
        logs_filter_layout->setSpacing(10);
        logs_filter_layout->setObjectName("logs_filter_layout");
        logs_title = new QLabel(page_logs);
        logs_title->setObjectName("logs_title");

        logs_filter_layout->addWidget(logs_title);

        logs_spacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        logs_filter_layout->addItem(logs_spacer);

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


        logs_layout->addLayout(logs_filter_layout);

        log_view = new QTableView(page_logs);
        log_view->setObjectName("log_view");
        log_view->setFrameShape(QFrame::NoFrame);
        log_view->setAlternatingRowColors(true);
        log_view->setShowGrid(false);
        log_view->setSelectionBehavior(QAbstractItemView::SelectRows);
        log_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        log_view->horizontalHeader()->setHighlightSections(false);
        log_view->horizontalHeader()->setStretchLastSection(true);
        log_view->verticalHeader()->setVisible(false);

        logs_layout->addWidget(log_view);

        pages->addWidget(page_logs);
        page_alerts = new QWidget();
        page_alerts->setObjectName("page_alerts");
        alerts_layout = new QVBoxLayout(page_alerts);
        alerts_layout->setSpacing(10);
        alerts_layout->setObjectName("alerts_layout");
        alerts_layout->setContentsMargins(24, 20, 24, 20);
        alerts_header_layout = new QHBoxLayout();
        alerts_header_layout->setSpacing(10);
        alerts_header_layout->setObjectName("alerts_header_layout");
        alerts_title = new QLabel(page_alerts);
        alerts_title->setObjectName("alerts_title");

        alerts_header_layout->addWidget(alerts_title);

        alert_stats_label = new QLabel(page_alerts);
        alert_stats_label->setObjectName("alert_stats_label");

        alerts_header_layout->addWidget(alert_stats_label);

        alts_hdr_spc = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        alerts_header_layout->addItem(alts_hdr_spc);

        alert_filter_label = new QLabel(page_alerts);
        alert_filter_label->setObjectName("alert_filter_label");

        alerts_header_layout->addWidget(alert_filter_label);

        alert_severity_filter = new QComboBox(page_alerts);
        alert_severity_filter->addItem(QString());
        alert_severity_filter->addItem(QString());
        alert_severity_filter->addItem(QString());
        alert_severity_filter->addItem(QString());
        alert_severity_filter->addItem(QString());
        alert_severity_filter->setObjectName("alert_severity_filter");

        alerts_header_layout->addWidget(alert_severity_filter);


        alerts_layout->addLayout(alerts_header_layout);

        alert_view = new QTableView(page_alerts);
        alert_view->setObjectName("alert_view");
        alert_view->setFrameShape(QFrame::NoFrame);
        alert_view->setAlternatingRowColors(true);
        alert_view->setShowGrid(false);
        alert_view->setSelectionBehavior(QAbstractItemView::SelectRows);
        alert_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        alert_view->horizontalHeader()->setHighlightSections(false);
        alert_view->horizontalHeader()->setStretchLastSection(true);
        alert_view->verticalHeader()->setVisible(false);

        alerts_layout->addWidget(alert_view);

        alert_detail = new QTextEdit(page_alerts);
        alert_detail->setObjectName("alert_detail");
        alert_detail->setReadOnly(true);
        alert_detail->setMaximumHeight(72);

        alerts_layout->addWidget(alert_detail);

        pages->addWidget(page_alerts);
        page_honeypot = new QWidget();
        page_honeypot->setObjectName("page_honeypot");
        honey_layout = new QVBoxLayout(page_honeypot);
        honey_layout->setSpacing(10);
        honey_layout->setObjectName("honey_layout");
        honey_layout->setContentsMargins(24, 20, 24, 20);
        honey_header_layout = new QHBoxLayout();
        honey_header_layout->setSpacing(10);
        honey_header_layout->setObjectName("honey_header_layout");
        honey_title = new QLabel(page_honeypot);
        honey_title->setObjectName("honey_title");

        honey_header_layout->addWidget(honey_title);

        honey_stats_label = new QLabel(page_honeypot);
        honey_stats_label->setObjectName("honey_stats_label");

        honey_header_layout->addWidget(honey_stats_label);

        hny_hdr_spc = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        honey_header_layout->addItem(hny_hdr_spc);


        honey_layout->addLayout(honey_header_layout);

        honey_view = new QTableView(page_honeypot);
        honey_view->setObjectName("honey_view");
        honey_view->setFrameShape(QFrame::NoFrame);
        honey_view->setAlternatingRowColors(true);
        honey_view->setShowGrid(false);
        honey_view->setSelectionBehavior(QAbstractItemView::SelectRows);
        honey_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        honey_view->horizontalHeader()->setHighlightSections(false);
        honey_view->horizontalHeader()->setStretchLastSection(true);
        honey_view->verticalHeader()->setVisible(false);

        honey_layout->addWidget(honey_view);

        honey_detail = new QTextEdit(page_honeypot);
        honey_detail->setObjectName("honey_detail");
        honey_detail->setReadOnly(true);
        honey_detail->setMaximumHeight(72);

        honey_layout->addWidget(honey_detail);

        pages->addWidget(page_honeypot);
        page_network = new QWidget();
        page_network->setObjectName("page_network");
        network_layout = new QVBoxLayout(page_network);
        network_layout->setSpacing(10);
        network_layout->setObjectName("network_layout");
        network_layout->setContentsMargins(24, 20, 24, 20);
        network_header_layout = new QHBoxLayout();
        network_header_layout->setSpacing(10);
        network_header_layout->setObjectName("network_header_layout");
        network_title = new QLabel(page_network);
        network_title->setObjectName("network_title");

        network_header_layout->addWidget(network_title);

        net_hdr_spc = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        network_header_layout->addItem(net_hdr_spc);

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
        local_ip_table->setMaximumHeight(150);
        local_ip_table->setFrameShape(QFrame::NoFrame);
        local_ip_table->setAlternatingRowColors(true);
        local_ip_table->setShowGrid(false);
        local_ip_table->setColumnCount(2);
        local_ip_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        local_ip_table->setSelectionMode(QAbstractItemView::SingleSelection);
        local_ip_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        local_ip_table->horizontalHeader()->setVisible(true);
        local_ip_table->horizontalHeader()->setStretchLastSection(true);
        local_ip_table->verticalHeader()->setVisible(false);

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
        arp_table->setMaximumHeight(210);
        arp_table->setFrameShape(QFrame::NoFrame);
        arp_table->setAlternatingRowColors(true);
        arp_table->setShowGrid(false);
        arp_table->setColumnCount(2);
        arp_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        arp_table->setSelectionMode(QAbstractItemView::SingleSelection);
        arp_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        arp_table->horizontalHeader()->setVisible(true);
        arp_table->horizontalHeader()->setStretchLastSection(true);
        arp_table->verticalHeader()->setVisible(false);

        network_layout->addWidget(arp_table);

        quarantine_label = new QLabel(page_network);
        quarantine_label->setObjectName("quarantine_label");

        network_layout->addWidget(quarantine_label);

        quarantine_table = new QTableWidget(page_network);
        if (quarantine_table->columnCount() < 3)
            quarantine_table->setColumnCount(3);
        QTableWidgetItem *__qtablewidgetitem4 = new QTableWidgetItem();
        quarantine_table->setHorizontalHeaderItem(0, __qtablewidgetitem4);
        QTableWidgetItem *__qtablewidgetitem5 = new QTableWidgetItem();
        quarantine_table->setHorizontalHeaderItem(1, __qtablewidgetitem5);
        QTableWidgetItem *__qtablewidgetitem6 = new QTableWidgetItem();
        quarantine_table->setHorizontalHeaderItem(2, __qtablewidgetitem6);
        quarantine_table->setObjectName("quarantine_table");
        quarantine_table->setMaximumHeight(210);
        quarantine_table->setFrameShape(QFrame::NoFrame);
        quarantine_table->setAlternatingRowColors(true);
        quarantine_table->setShowGrid(false);
        quarantine_table->setColumnCount(3);
        quarantine_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        quarantine_table->setSelectionMode(QAbstractItemView::SingleSelection);
        quarantine_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        quarantine_table->horizontalHeader()->setVisible(true);
        quarantine_table->horizontalHeader()->setStretchLastSection(true);
        quarantine_table->verticalHeader()->setVisible(false);

        network_layout->addWidget(quarantine_table);

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
        QObject::connect(sidebar, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);

        sidebar->setCurrentRow(0);
        pages->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(monitor);
    } // setupUi

    void retranslateUi(QMainWindow *monitor)
    {
        monitor->setWindowTitle(QCoreApplication::translate("monitor", "\345\223\252\345\220\222\347\275\221\347\273\234\345\256\211\345\205\250 SIEM", nullptr));
        brand_badge->setText(QCoreApplication::translate("monitor", "NG", nullptr));
        app_title->setText(QCoreApplication::translate("monitor", "\345\223\252\345\220\222\347\275\221\347\273\234\345\256\211\345\205\250 SIEM", nullptr));
        clock_label->setText(QString());
        status_dot->setText(QString());
        status_text->setText(QCoreApplication::translate("monitor", "\350\277\220\350\241\214\344\270\255", nullptr));

        const bool __sortingEnabled = sidebar->isSortingEnabled();
        sidebar->setSortingEnabled(false);
        QListWidgetItem *___qlistwidgetitem = sidebar->item(0);
        ___qlistwidgetitem->setText(QCoreApplication::translate("monitor", "\344\273\252\350\241\250\347\233\230", nullptr));
        QListWidgetItem *___qlistwidgetitem1 = sidebar->item(1);
        ___qlistwidgetitem1->setText(QCoreApplication::translate("monitor", "\346\227\245\345\277\227\347\233\221\346\216\247", nullptr));
        QListWidgetItem *___qlistwidgetitem2 = sidebar->item(2);
        ___qlistwidgetitem2->setText(QCoreApplication::translate("monitor", "\345\256\211\345\205\250\345\221\212\350\255\246", nullptr));
        QListWidgetItem *___qlistwidgetitem3 = sidebar->item(3);
        ___qlistwidgetitem3->setText(QCoreApplication::translate("monitor", "\350\234\234\347\275\220\347\233\221\346\216\247", nullptr));
        QListWidgetItem *___qlistwidgetitem4 = sidebar->item(4);
        ___qlistwidgetitem4->setText(QCoreApplication::translate("monitor", "\347\275\221\347\273\234\344\277\241\346\201\257", nullptr));
        sidebar->setSortingEnabled(__sortingEnabled);

        dash_title->setText(QCoreApplication::translate("monitor", "\344\273\252\350\241\250\347\233\230\346\246\202\350\247\210", nullptr));
        card_logs_value->setText(QCoreApplication::translate("monitor", "0", nullptr));
        card_logs_label->setText(QCoreApplication::translate("monitor", "\346\227\245\345\277\227\346\200\273\346\225\260", nullptr));
        card_alerts_value->setText(QCoreApplication::translate("monitor", "0", nullptr));
        card_alerts_label->setText(QCoreApplication::translate("monitor", "\345\256\211\345\205\250\345\221\212\350\255\246", nullptr));
        card_threats_value->setText(QCoreApplication::translate("monitor", "0", nullptr));
        card_threats_label->setText(QCoreApplication::translate("monitor", "\345\267\262\351\232\224\347\246\273 IP", nullptr));
        card_uptime_value->setText(QCoreApplication::translate("monitor", "00:00", nullptr));
        card_uptime_label->setText(QCoreApplication::translate("monitor", "\350\277\220\350\241\214\346\227\266\351\227\264", nullptr));
        recent_alerts_label->setText(QCoreApplication::translate("monitor", "\346\234\200\350\277\221\345\221\212\350\255\246", nullptr));
        logs_title->setText(QCoreApplication::translate("monitor", "\346\227\245\345\277\227\347\233\221\346\216\247", nullptr));
        logs_filter_label->setText(QCoreApplication::translate("monitor", "\347\272\247\345\210\253", nullptr));
        level_filter->setItemText(0, QCoreApplication::translate("monitor", "\345\205\250\351\203\250", nullptr));
        level_filter->setItemText(1, QCoreApplication::translate("monitor", "Trace", nullptr));
        level_filter->setItemText(2, QCoreApplication::translate("monitor", "Debug", nullptr));
        level_filter->setItemText(3, QCoreApplication::translate("monitor", "Info", nullptr));
        level_filter->setItemText(4, QCoreApplication::translate("monitor", "Warn", nullptr));
        level_filter->setItemText(5, QCoreApplication::translate("monitor", "Error", nullptr));
        level_filter->setItemText(6, QCoreApplication::translate("monitor", "Critical", nullptr));

        alerts_title->setText(QCoreApplication::translate("monitor", "\345\256\211\345\205\250\345\221\212\350\255\246", nullptr));
        alert_stats_label->setText(QString());
        alert_filter_label->setText(QCoreApplication::translate("monitor", "\344\270\245\351\207\215\347\272\247\345\210\253", nullptr));
        alert_severity_filter->setItemText(0, QCoreApplication::translate("monitor", "\345\205\250\351\203\250", nullptr));
        alert_severity_filter->setItemText(1, QCoreApplication::translate("monitor", "CRIT", nullptr));
        alert_severity_filter->setItemText(2, QCoreApplication::translate("monitor", "ERROR", nullptr));
        alert_severity_filter->setItemText(3, QCoreApplication::translate("monitor", "WARN", nullptr));
        alert_severity_filter->setItemText(4, QCoreApplication::translate("monitor", "INFO", nullptr));

        alert_detail->setPlaceholderText(QCoreApplication::translate("monitor", "\351\200\211\346\213\251\345\221\212\350\255\246\346\237\245\347\234\213\350\257\246\346\203\205\342\200\246", nullptr));
        honey_title->setText(QCoreApplication::translate("monitor", "\350\234\234\347\275\220\347\233\221\346\216\247", nullptr));
        honey_stats_label->setText(QString());
        honey_detail->setPlaceholderText(QCoreApplication::translate("monitor", "\351\200\211\346\213\251\350\277\236\346\216\245\346\237\245\347\234\213\350\257\246\346\203\205\342\200\246", nullptr));
        network_title->setText(QCoreApplication::translate("monitor", "\347\275\221\347\273\234\344\277\241\346\201\257", nullptr));
        refresh_network->setText(QCoreApplication::translate("monitor", "\345\210\267\346\226\260", nullptr));
        local_ip_label->setText(QCoreApplication::translate("monitor", "\346\234\254\345\234\260\346\216\245\345\217\243", nullptr));
        QTableWidgetItem *___qtablewidgetitem = local_ip_table->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("monitor", "\346\216\245\345\217\243\345\220\215\347\247\260", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = local_ip_table->horizontalHeaderItem(1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("monitor", "IP \345\234\260\345\235\200", nullptr));
        arp_label->setText(QCoreApplication::translate("monitor", "ARP \347\274\223\345\255\230\350\241\250", nullptr));
        QTableWidgetItem *___qtablewidgetitem2 = arp_table->horizontalHeaderItem(0);
        ___qtablewidgetitem2->setText(QCoreApplication::translate("monitor", "IP \345\234\260\345\235\200", nullptr));
        QTableWidgetItem *___qtablewidgetitem3 = arp_table->horizontalHeaderItem(1);
        ___qtablewidgetitem3->setText(QCoreApplication::translate("monitor", "MAC \345\234\260\345\235\200", nullptr));
        quarantine_label->setText(QCoreApplication::translate("monitor", "\351\232\224\347\246\273\345\210\227\350\241\250", nullptr));
        QTableWidgetItem *___qtablewidgetitem4 = quarantine_table->horizontalHeaderItem(0);
        ___qtablewidgetitem4->setText(QCoreApplication::translate("monitor", "IP \345\234\260\345\235\200", nullptr));
        QTableWidgetItem *___qtablewidgetitem5 = quarantine_table->horizontalHeaderItem(1);
        ___qtablewidgetitem5->setText(QCoreApplication::translate("monitor", "\351\232\224\347\246\273\345\216\237\345\233\240", nullptr));
        QTableWidgetItem *___qtablewidgetitem6 = quarantine_table->horizontalHeaderItem(2);
        ___qtablewidgetitem6->setText(QCoreApplication::translate("monitor", "\345\250\201\350\203\201\350\257\204\345\210\206", nullptr));
        status_label->setText(QCoreApplication::translate("monitor", "\345\260\261\347\273\252", nullptr));
    } // retranslateUi

};

namespace Ui {
    class monitor: public Ui_monitor {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MONITOR_H
