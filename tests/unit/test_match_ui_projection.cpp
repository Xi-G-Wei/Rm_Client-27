#include "simulator/MockMatchSource.h"
#include "ui/MatchUiProjection.h"

#include <QDir>
#include <QVariantList>
#include <QVariantMap>
#include <QtTest>

namespace {

QString scenarioPath() {
    return QDir(QStringLiteral(RM_SOURCE_DIR))
        .absoluteFilePath(
            QStringLiteral("mock/3v3/match_3v3_basic.json"));
}

QVariantMap firstRobot(const QVariantMap &dashboard,
                       const QString &teamKey) {
    return dashboard.value(teamKey)
        .toList()
        .value(0)
        .toMap();
}

} // namespace

class TestMatchUiProjection : public QObject {
    Q_OBJECT

private slots:
    void snapshotProjectsIntoExistingDashboardCards() {
        RM::MockMatchSource source;
        QVERIFY2(source.loadFromFile(scenarioPath()),
                 qPrintable(source.lastError()));
        RM::MatchUiProjection projection(source.matchSession());

        const QVariantMap dashboard = projection.dashboardData();
        QCOMPARE(dashboard.value(QStringLiteral("redRobots")).toList().size(),
                 3);
        QCOMPARE(dashboard.value(QStringLiteral("blueRobots")).toList().size(),
                 3);
        QVERIFY(dashboard.value(QStringLiteral("mockMode")).toBool());

        const QVariantMap redHeavy =
            firstRobot(dashboard, QStringLiteral("redRobots"));
        QCOMPARE(redHeavy.value(QStringLiteral("number")).toInt(), 1);
        QCOMPARE(redHeavy.value(QStringLiteral("name")).toString(),
                 QStringLiteral("重装"));
        const QVariantMap energy =
            redHeavy.value(QStringLiteral("energy")).toMap();
        QCOMPARE(energy.value(QStringLiteral("superCapPercent")).toDouble(),
                 82.0);
        const QVariantMap chassis =
            redHeavy.value(QStringLiteral("chassis")).toMap();
        QCOMPARE(chassis.value(QStringLiteral("driveTypeLabel")).toString(),
                 QStringLiteral("轮腿"));
    }

    void projectionUpdatesWithSimulationClockAndEvents() {
        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(scenarioPath()));
        RM::MatchUiProjection projection(source.matchSession());
        QSignalSpy updatedSpy(&projection, &RM::MatchUiProjection::updated);
        QVERIFY(source.start());

        QVERIFY(source.advanceByMs(12000));
        QVERIFY(updatedSpy.count() > 0);
        QCOMPARE(projection.gamePhase(), QStringLiteral("战斗阶段"));
        QCOMPARE(projection.remainingTime(), 300);
        QCOMPARE(projection.headerData()
                     .value(QStringLiteral("timeRemaining"))
                     .toString(),
                 QStringLiteral("05:00"));

        QVERIFY(source.advanceByMs(28000));
        QCOMPARE(projection.redScore(), 1);
        QCOMPARE(projection.dashboardData()
                     .value(QStringLiteral("leadingSide"))
                     .toInt(),
                 1);
        QVERIFY(projection.keyEvents().size() >= 4);
        QCOMPARE(projection.keyEvents()
                     .first()
                     .toMap()
                     .value(QStringLiteral("text"))
                     .toString(),
                 QStringLiteral("红方步兵 3 号进入轮腿切换"));
    }

    void mapProjectionContainsBothThreeRobotRosters() {
        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(scenarioPath()));
        RM::MatchUiProjection projection(source.matchSession());

        const QVariantMap map = projection.mapData();
        QCOMPARE(map.value(QStringLiteral("allyRobots")).toList().size(), 3);
        QCOMPARE(map.value(QStringLiteral("enemyRobots")).toList().size(), 3);
        QCOMPARE(map.value(QStringLiteral("mapImageSource")).toString(),
                 QStringLiteral("qrc:/images/rmul3v3.png"));
    }
};

QTEST_GUILESS_MAIN(TestMatchUiProjection)
#include "test_match_ui_projection.moc"
