#include "simulator/MockMatchSource.h"
#include "ui/MatchUiProjection.h"

#include <QDir>
#include <QFile>
#include <QVariantList>
#include <QVariantMap>
#include <QtTest>

namespace {

QString scenarioPath(const QString &fileName =
                         QStringLiteral("match_3v3_basic.json")) {
    return QDir(QStringLiteral(RM_SOURCE_DIR))
        .absoluteFilePath(QStringLiteral("mock/3v3/") + fileName);
}

QVariantMap firstRobot(const QVariantMap &dashboard,
                       const QString &teamKey) {
    return dashboard.value(teamKey)
        .toList()
        .value(0)
        .toMap();
}

QVariantMap findRobot(const QVariantMap &dashboard,
                      const QString &teamKey,
                      const QString &id) {
    const QVariantList robots = dashboard.value(teamKey).toList();
    for (const QVariant &entry : robots) {
        const QVariantMap robot = entry.toMap();
        if (robot.value(QStringLiteral("id")).toString() == id) {
            return robot;
        }
    }
    return {};
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
        QCOMPARE(dashboard.value(QStringLiteral("sourceKind")).toString(),
                 QStringLiteral("mock"));
        QCOMPARE(dashboard.value(QStringLiteral("dataHealth")).toString(),
                 QStringLiteral("fresh"));
        QCOMPARE(dashboard.value(QStringLiteral("unknownFieldCount")).toInt(),
                 0);

        const QVariantMap redHeavy =
            firstRobot(dashboard, QStringLiteral("redRobots"));
        QCOMPARE(redHeavy.value(QStringLiteral("number")).toInt(), 1);
        QCOMPARE(redHeavy.value(QStringLiteral("name")).toString(),
                 QStringLiteral("重装"));
        QCOMPARE(redHeavy.value(QStringLiteral("telemetryScope")).toString(),
                 QStringLiteral("public"));
        const QVariantMap operatorRobot =
            findRobot(dashboard, QStringLiteral("redRobots"),
                      QStringLiteral("red-3"));
        const QVariantMap energy =
            operatorRobot.value(QStringLiteral("energy")).toMap();
        QCOMPARE(energy.value(QStringLiteral("superCapPercent")).toDouble(),
                 74.0);
        const QVariantMap chassis =
            operatorRobot.value(QStringLiteral("chassis")).toMap();
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

    void operatorVehicleFieldsFollowSelectedRobot() {
        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(
            scenarioPath(QStringLiteral("match_3v3_basic.json"))));
        RM::MatchUiProjection projection(source.matchSession());

        QCOMPARE(projection.robotId(), 3);
        QCOMPARE(projection.robotLevel(), 3);
        QCOMPARE(projection.currentExp(), 68);
        QCOMPARE(projection.maxExp(), 100);
        QCOMPARE(projection.currentHealth(), 400);
        QCOMPARE(projection.maxHealth(), 400);
        QCOMPARE(projection.bufferEnergy(), 40);
        QCOMPARE(projection.maxBufferEnergy(), 60);
        QCOMPARE(projection.superCapEnergy(), 74.0);
        QCOMPARE(projection.chassisStatus(),
                 QStringLiteral("普通 · 55 W · 平衡"));
        QCOMPARE(projection.moduleStates().size(), 10);
        QCOMPARE(projection.moduleData().size(), 10);
        QCOMPARE(projection.moduleStates().first().toInt(), 0);
        QCOMPARE(projection.moduleStates().at(2).toInt(), 1);
        QVERIFY(projection.buffTimedData()
                    .value(QStringLiteral("attack"))
                    .toMap()
                    .value(QStringLiteral("level"))
                    .toInt() > 0);
        QCOMPARE(projection.territoryStatus()
                     .value(QStringLiteral("redPoints"))
                     .toInt(),
                 48);
        QCOMPARE(projection.territoryStatus()
                     .value(QStringLiteral("bluePoints"))
                     .toInt(),
                 52);
        QVERIFY(projection.myRobot()
                    .value(QStringLiteral("isClientConnected"))
                    .toBool());

        QVERIFY(source.start());
        QVERIFY(source.advanceByMs(40000));
        QCOMPARE(projection.superCapEnergy(), 25.0);
        QCOMPARE(projection.chassisStatus(),
                 QStringLiteral("小陀螺 · 97 W · 切换中"));
        QVERIFY(source.pause());
        QVERIFY(projection.isPaused());
        QVERIFY(source.resume());
        QVERIFY(!projection.isPaused());
    }

    void publicEnemyTelemetryHidesPrivateFields() {
        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(scenarioPath()));
        RM::MatchUiProjection projection(source.matchSession());
        const QVariantMap dashboard = projection.dashboardData();

        const QVariantMap enemy = findRobot(dashboard,
                                            QStringLiteral("blueRobots"),
                                            QStringLiteral("blue-3"));
        QCOMPARE(enemy.value(QStringLiteral("telemetryScope")).toString(),
                 QStringLiteral("public"));
        QVERIFY(!enemy.contains(QStringLiteral("energy")));
        QVERIFY(!enemy.contains(QStringLiteral("chassis")));
        QVERIFY(!enemy.contains(QStringLiteral("wheelLeg")));
        QVERIFY(!enemy.contains(QStringLiteral("ammo")));
        QVERIFY(!enemy.contains(QStringLiteral("heat")));

        const QVariantMap ally = findRobot(dashboard,
                                           QStringLiteral("redRobots"),
                                           QStringLiteral("red-3"));
        QCOMPARE(ally.value(QStringLiteral("telemetryScope")).toString(),
                 QStringLiteral("private"));
        QVERIFY(ally.contains(QStringLiteral("energy")));
        QVERIFY(ally.contains(QStringLiteral("wheelLeg")));
    }

    void blueOperatorSelectionUsesBlueTeamAsAlly() {
        QFile file(scenarioPath());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QByteArray json = file.readAll();
        json.replace("\"operatorRobotId\": \"red-3\"",
                     "\"operatorRobotId\": \"blue-3\"");

        RM::MockMatchSource source;
        QVERIFY2(source.loadFromJson(json, QStringLiteral("blue-operator")),
                 qPrintable(source.lastError()));
        RM::MatchUiProjection projection(source.matchSession());

        QCOMPARE(projection.myRobot()
                     .value(QStringLiteral("isRedTeam"))
                     .toBool(),
                 false);
        QCOMPARE(projection.robotId(), 103);
        QCOMPARE(projection.superCapEnergy(), 69.0);
        const QVariantList allies = projection.allyRobotList();
        QCOMPARE(allies.size(), 3);
        for (const QVariant &entry : allies) {
            QVERIFY(entry.toMap()
                        .value(QStringLiteral("id"))
                        .toString()
                        .startsWith(QStringLiteral("blue-")));
        }
        const QVariantList enemies = projection.enemyRobotList();
        QCOMPARE(enemies.size(), 3);
        for (const QVariant &entry : enemies) {
            QVERIFY(entry.toMap()
                        .value(QStringLiteral("id"))
                        .toString()
                        .startsWith(QStringLiteral("red-")));
        }
    }

    void offlineScenarioMarksOperatorVehicleDisconnected() {
        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(
            scenarioPath(QStringLiteral("match_3v3_offline.json"))));
        RM::MatchUiProjection projection(source.matchSession());
        QVERIFY(source.start());
        QVERIFY(source.advanceByMs(5000));

        QCOMPARE(projection.outOfCombatStatus(),
                 QStringLiteral("数据中断"));
        QVERIFY(!projection.isOutOfCombat());
        QVERIFY(!projection.myRobot()
                     .value(QStringLiteral("isClientConnected"))
                     .toBool());
        QCOMPARE(projection.moduleStates().first().toInt(), 2);
    }

    void acceleratedLongRunCompletesWithoutHanging() {
        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(scenarioPath()));
        source.setPlaybackRate(4.0);
        QVERIFY(source.start());
        QVERIFY(source.advanceByMs(60000));
        QVERIFY(source.isCompleted());
        QCOMPARE(source.snapshot().value(QStringLiteral("phase")).toString(),
                 QStringLiteral("settlement"));
        QVERIFY(source.snapshot()
                    .value(QStringLiteral("events"))
                    .toList()
                    .size() >= 7);
    }
};

QTEST_GUILESS_MAIN(TestMatchUiProjection)
#include "test_match_ui_projection.moc"
