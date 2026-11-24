#include "core/MatchDataHealth.h"
#include "core/MatchSourceController.h"
#include "simulator/MockMatchSource.h"

#include <QDateTime>
#include <QtTest>

class TestMatchSourceBoundary : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        Q_INIT_RESOURCE(mock);
    }

    void cleanupTestCase() {
        Q_CLEANUP_RESOURCE(mock);
    }

    void controllerSelectsMockSource() {
        RM::MatchSourceController controller;
        QVERIFY(controller.selectMock());
        QCOMPARE(controller.sourceKind(), QStringLiteral("mock"));
        QVERIFY(controller.isActive());
        QVERIFY(controller.matchSession());
        QVERIFY(controller.matchSession()->isValid());
        QVERIFY(controller.mockSource());
        QCOMPARE(controller.statusText(), QStringLiteral("模拟比赛运行中"));
    }

    void controllerSelectsReservedRealSource() {
        RM::MatchSourceController controller;
        QVERIFY(controller.selectReal());
        QCOMPARE(controller.sourceKind(), QStringLiteral("real"));
        QVERIFY(!controller.isActive());
        QVERIFY(controller.matchSession());
        QVERIFY(!controller.matchSession()->isValid());
        QVERIFY(controller.statusText().contains(QStringLiteral("等待通信接入")));
    }

    void invalidMockSelectionKeepsRealSource() {
        RM::MatchSourceController controller;
        QVERIFY(controller.selectReal());
        QVERIFY(!controller.selectMock(QStringLiteral(":/missing.json")));
        QCOMPARE(controller.sourceKind(), QStringLiteral("real"));
        QVERIFY(!controller.isActive());
    }

    void healthReportsFreshStaleAndExpired() {
        RM::MatchSourceController controller;
        QVERIFY(controller.selectMock());
        const RM::MatchSnapshot snapshot =
            controller.matchSession()->matchSnapshot();
        const qint64 now = QDateTime::currentMSecsSinceEpoch();

        RM::MatchDataHealth health =
            RM::assessMatchDataHealth(snapshot, now);
        QCOMPARE(health.level, RM::MatchDataHealthLevel::Fresh);
        QCOMPARE(health.onlineRobots, 6);
        QCOMPARE(health.offlineRobots, 0);
        QCOMPARE(health.publicRobots, 3);
        QCOMPARE(health.privateRobots, 3);
        QCOMPARE(health.unknownFieldCount, 0);

        RM::MatchSnapshot stale = snapshot;
        stale.updatedAtMs = now - 2500;
        health = RM::assessMatchDataHealth(stale, now);
        QCOMPARE(health.level, RM::MatchDataHealthLevel::Stale);

        RM::MatchSnapshot expired = snapshot;
        expired.updatedAtMs = now - 6000;
        health = RM::assessMatchDataHealth(expired, now);
        QCOMPARE(health.level, RM::MatchDataHealthLevel::Expired);
        QVERIFY(RM::matchDataHealthLabel(health).contains(
            QStringLiteral("已过期")));
    }

    void healthCountsUnknownAndUnavailableData() {
        RM::MatchSourceController controller;
        QVERIFY(controller.selectMock());
        RM::MatchSnapshot snapshot =
            controller.matchSession()->matchSnapshot();
        snapshot.redTeam.robots[0].health = RM::kUnknownMatchMetric;
        snapshot.redTeam.robots[0].hasPosition = false;
        const RM::MatchDataHealth health =
            RM::assessMatchDataHealth(
                snapshot, QDateTime::currentMSecsSinceEpoch());
        QCOMPARE(health.unknownFieldCount, 2);

        const RM::MatchDataHealth unavailable =
            RM::assessMatchDataHealth(RM::MatchSnapshot{}, 100000);
        QCOMPARE(unavailable.level,
                 RM::MatchDataHealthLevel::Unavailable);
        QCOMPARE(unavailable.ageMs, -1);
    }
};

QTEST_GUILESS_MAIN(TestMatchSourceBoundary)
#include "test_match_source_boundary.moc"
