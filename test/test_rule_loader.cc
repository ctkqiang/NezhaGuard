#include <QtTest>
#include "../src/core/rule_loader.h"
#include "../src/core/detector.h"

using namespace Nezha::Core;

class TestRuleLoader : public QObject {
    Q_OBJECT

private slots:
    void default_rules_not_empty() {
        const auto &rules = RuleLoader::default_rules();
        QVERIFY(!rules.empty());
        QVERIFY(rules.size() >= 10); // at least some built-in rules
    }

    void load_from_file() {
        RuleLoader loader;
        auto result = loader.load_from_file("../../rules/default.yaml");
        QVERIFY(result.has_value());
        QVERIFY(!result->empty());
        QVERIFY(result->size() >= 50);
    }

    void file_not_found() {
        RuleLoader loader;
        auto result = loader.load_from_file("rules/does_not_exist.yaml");
        QVERIFY(!result.has_value());
        QCOMPARE(result.error(), RuleLoadError::FileNotFound);
    }

    void detector_loads_rules() {
        AttackDetector d;
        QVERIFY(d.rule_count() > 0);
        bool ok = d.load_rules("../../rules/default.yaml");
        QVERIFY(ok);
        QVERIFY(d.rule_count() > 0);
    }

    void detector_fallback_on_bad_path() {
        AttackDetector d;
        std::size_t before = d.rule_count();
        bool ok = d.load_rules("rules/does_not_exist.yaml");
        QVERIFY(!ok);
        QCOMPARE(d.rule_count(), before); // rules preserved
    }

    void rule_type_mapping() {
        RuleLoader loader;
        auto result = loader.load_from_file("../../rules/default.yaml");
        QVERIFY(result.has_value());
        for (const auto &r : *result) {
            QVERIFY(r.score >= 0.0 && r.score <= 100.0);
            QVERIFY(!r.pattern.empty());
            QVERIFY(!r.desc.empty());
        }
    }
};

QObject *make_test_rule_loader() { return new TestRuleLoader; }
#include "test_rule_loader.moc"
