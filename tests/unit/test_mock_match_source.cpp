#include "simulator/MockMatchSource.h"

#include <QDir>
#include <QSignalSpy>
#include <QVariantList>
#include <QVariantMap>
#include <QtTest>

namespace {

QString scenarioPath(const QString &fileName) {
    return QDir(QStringLiteral(RM_SOURCE_DIR))
        .absoluteFilePath(QStringLiteral("mock/3v3/") + fileName);
}

QVariantMap findRobot(const QVariantMap &snapshot, const QString &robotId) {
    for (const QString &teamKey :
         {QStringLiteral("redTeam"), QStringLiteral("blueTeam")}) {
        const QVariantList robots =
            snapshot.value(teamKey)
                .toMap()
                .value(QStringLiteral("robots"))
                .toList();
        for (const QVariant &entry : robots) {
            const QVariantMap robot = entry.toMap();
            if (robot.value(QStringLiteral("id")).toString() == robotId) {
                return robot;
            }
        }
    }
    return {};
}

} // namespace

class TestMockMatchSource : public QObject {
    Q_OBJECT

private slots:
    void duelScenariosExerciseCombatAndRecovery() {
        const QString prefix = QDir(QStringLiteral(RM_SOURCE_DIR))
            .absoluteFilePath(QStringLiteral("mock/1v1/"));
        RM::MockMatchSource source;
        QVERIFY2(source.loadFromFile(prefix + "match_1v1_basic.json"),
                 qPrintable(source.lastError()));
        QCOMPARE(source.matchSession()->matchSnapshot().profile,
                 QStringLiteral("rmul1v1"));
        QCOMPARE(source.matchSession()->matchSnapshot().redTeam.robots.size(), 1);
        QCOMPARE(source.matchSession()->matchSnapshot().blueTeam.robots.size(), 1);
        QVERIFY(source.start());
        QVERIFY(source.advanceByMs(12000));
        QCOMPARE(source.matchSession()->matchSnapshot().timeRemainingSec, 120);
        const int before = source.matchSession()->matchSnapshot().redTeam.robots[0].ammo;
        QVERIFY(source.setTriggerHeld(true));
        QVERIFY(source.advanceByMs(1000));
        QVERIFY(source.setTriggerHeld(false));
        QVERIFY(source.matchSession()->matchSnapshot().redTeam.robots[0].ammo < before);
        QVERIFY(source.pause());
        QVERIFY(source.matchSession()->matchSnapshot().isPaused);
        QVERIFY(source.resume());
        QVERIFY(source.advanceByMs(12000));
        QCOMPARE(source.matchSession()->matchSnapshot().redTeam.robots[0].health, 260);
        QCOMPARE(source.matchSession()->matchSnapshot().redTeam.robots[0].moduleStates[5].toInt(), 1);
        QVERIFY(source.advanceByMs(35000));
        QCOMPARE(source.matchSession()->matchSnapshot().phase, RM::MatchPhase::Settlement);
        QCOMPARE(source.matchSession()->matchSnapshot().redTeam.score, 1);

        QVERIFY(source.selectScenario("match_1v1_damage"));
        QVERIFY(source.advanceByMs(12000));
        QVERIFY(!source.matchSession()->matchSnapshot().redTeam.robots[0].alive);
        QVERIFY(source.advanceByMs(5000));
        QVERIFY(source.matchSession()->matchSnapshot().redTeam.robots[0].alive);
        QVERIFY(source.selectScenario("match_1v1_offline"));
        QVERIFY(source.advanceByMs(5000));
        QVERIFY(!source.matchSession()->matchSnapshot().redTeam.robots[0].online);
        QVERIFY(source.advanceByMs(15000));
        QVERIFY(source.matchSession()->matchSnapshot().redTeam.robots[0].online);
        QVERIFY(source.matchSession()->matchSnapshot().blueTeam.robots[0].online);
        QVERIFY(source.cycleScenario());
        QCOMPARE(source.scenarioId(), QStringLiteral("match_1v1_settlement"));
        QVERIFY(source.advanceByMs(12000));
        QCOMPARE(source.matchSession()->matchSnapshot().blueTeam.score, 1);
        QVERIFY(source.cycleScenario());
        QCOMPARE(source.scenarioId(), QStringLiteral("match_1v1_basic"));
    }

    void initTestCase() {
        Q_INIT_RESOURCE(mock);
    }

    void cleanupTestCase() {
        Q_CLEANUP_RESOURCE(mock);
    }

    void allBundledScenariosLoad() {
        for (const QString &fileName :
             {QStringLiteral("match_3v3_basic.json"),
              QStringLiteral("match_3v3_damage.json"),
              QStringLiteral("match_3v3_offline.json"),
              QStringLiteral("match_3v3_settlement.json")}) {
            RM::MockMatchSource source;
            QVERIFY2(source.loadFromFile(scenarioPath(fileName)),
                     qPrintable(source.lastError()));
            QVERIFY(source.isLoaded());
            QVERIFY(source.matchSession()->isValid());
            QCOMPARE(source.snapshot()
                         .value(QStringLiteral("redTeam"))
                         .toMap()
                         .value(QStringLiteral("robots"))
                         .toList()
                         .size(),
                     3);
        }
    }

    void clockAdvancesOfficialPreparationAndCountdownStages() {
        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(
            scenarioPath(QStringLiteral("match_3v3_basic.json"))));
        QVERIFY(source.start());

        QVERIFY(source.advanceByMs(2000));
        QCOMPARE(source.elapsedSec(), 2);
        QCOMPARE(source.snapshot().value(QStringLiteral("phase")).toString(),
                 QStringLiteral("self_check"));
        QCOMPARE(source.snapshot()
                     .value(QStringLiteral("timeRemainingSec"))
                     .toInt(),
                 15);
        QCOMPARE(source.snapshot()
                     .value(QStringLiteral("phaseLabel"))
                     .toString(),
                 QStringLiteral("裁判系统自检"));

        QVERIFY(source.advanceByMs(5000));
        QCOMPARE(source.elapsedSec(), 7);
        QCOMPARE(source.snapshot().value(QStringLiteral("phase")).toString(),
                 QStringLiteral("countdown"));
        QCOMPARE(source.snapshot()
                     .value(QStringLiteral("timeRemainingSec"))
                     .toInt(),
                 5);
        QCOMPARE(source.snapshot()
                     .value(QStringLiteral("phaseLabel"))
                     .toString(),
                 QStringLiteral("5 秒倒计时"));

        QVERIFY(source.advanceByMs(5000));
        QCOMPARE(source.elapsedSec(), 12);
        QCOMPARE(source.snapshot().value(QStringLiteral("phase")).toString(),
                 QStringLiteral("battle"));
        QCOMPARE(source.snapshot()
                     .value(QStringLiteral("timeRemainingSec"))
                     .toInt(),
                 300);
    }

    void timelineUpdatesScoresEventsAndRobotEnergy() {
        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(
            scenarioPath(QStringLiteral("match_3v3_basic.json"))));
        QVERIFY(source.start());
        QVERIFY(source.advanceByMs(40000));

        const QVariantMap snapshot = source.snapshot();
        QCOMPARE(snapshot.value(QStringLiteral("redTeam"))
                     .toMap()
                     .value(QStringLiteral("score"))
                     .toInt(),
                 1);
        QVERIFY(snapshot.value(QStringLiteral("events"))
                    .toList()
                    .size() >= 5);

        const QVariantMap blueThree = findRobot(snapshot,
                                               QStringLiteral("blue-3"));
        QCOMPARE(blueThree.value(QStringLiteral("health")).toInt(), 220);
        const QVariantMap blueEnergy =
            blueThree.value(QStringLiteral("energy")).toMap();
        QCOMPARE(blueEnergy.value(QStringLiteral("superCapPercent"))
                     .toDouble(),
                 45.0);

        const QVariantMap redThree = findRobot(snapshot,
                                              QStringLiteral("red-3"));
        QCOMPARE(redThree.value(QStringLiteral("chassis"))
                     .toMap()
                     .value(QStringLiteral("mode"))
                     .toString(),
                 QStringLiteral("spin"));
        QCOMPARE(redThree.value(QStringLiteral("wheelLeg"))
                     .toMap()
                     .value(QStringLiteral("mode"))
                     .toString(),
                 QStringLiteral("transition"));
    }

    void pauseStopsClockAndResumeContinues() {
        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(
            scenarioPath(QStringLiteral("match_3v3_basic.json"))));
        QVERIFY(source.start());
        QVERIFY(source.advanceByMs(1000));
        QCOMPARE(source.elapsedSec(), 1);

        QVERIFY(source.pause());
        QVERIFY(source.isPaused());
        QVERIFY(!source.advanceByMs(5000));
        QCOMPARE(source.elapsedSec(), 1);

        QVERIFY(source.resume());
        QVERIFY(source.advanceByMs(1000));
        QCOMPARE(source.elapsedSec(), 2);
    }

    void playbackRateChangesVirtualTime() {
        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(
            scenarioPath(QStringLiteral("match_3v3_basic.json"))));
        source.setPlaybackRate(2.0);
        QVERIFY(source.start());
        QVERIFY(source.advanceByMs(1000));
        QCOMPARE(source.elapsedSec(), 2);
        QCOMPARE(source.playbackRate(), 2.0);
    }

    void togglePauseSwitchesRunningAndPaused() {
        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(
            scenarioPath(QStringLiteral("match_3v3_basic.json"))));
        QVERIFY(source.start());
        QVERIFY(source.togglePause());
        QVERIFY(source.isPaused());
        QVERIFY(!source.isRunning());
        QVERIFY(source.togglePause());
        QVERIFY(source.isRunning());
        QVERIFY(!source.isPaused());
    }

    void cycleScenarioMovesForwardAndBackward() {
        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(
            scenarioPath(QStringLiteral("match_3v3_basic.json"))));
        QVERIFY(source.cycleScenario(1));
        QCOMPARE(source.scenarioId(), QStringLiteral("match_3v3_damage"));
        QVERIFY(source.isRunning());

        QVERIFY(source.cycleScenario(-1));
        QCOMPARE(source.scenarioId(), QStringLiteral("match_3v3_basic"));
        QVERIFY(source.isRunning());
    }

    void selectScenarioLoadsSupportedScene() {
        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(
            scenarioPath(QStringLiteral("match_3v3_basic.json"))));
        QVERIFY(source.selectScenario(QStringLiteral("match_3v3_offline")));
        QCOMPARE(source.scenarioId(), QStringLiteral("match_3v3_offline"));
        QVERIFY(source.isRunning());
        QVERIFY(!source.selectScenario(QStringLiteral("missing_scene")));
        QVERIFY(!source.lastError().isEmpty());
    }

    void triggerUpdatesHeatAndAmmoThenCools() {
        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(
            scenarioPath(QStringLiteral("match_3v3_basic.json"))));
        QVERIFY(source.start());
        const int initialAmmo =
            findRobot(source.snapshot(), QStringLiteral("red-3"))
                .value(QStringLiteral("ammo"))
                .toInt();
        QVERIFY(source.setTriggerHeld(true));
        QVERIFY(source.advanceByMs(2000));
        QVariantMap robot =
            findRobot(source.snapshot(), QStringLiteral("red-3"));
        QVERIFY(robot.value(QStringLiteral("heat")).toInt()
                >= robot.value(QStringLiteral("heatLimit")).toInt());
        QVERIFY(robot.value(QStringLiteral("ammo")).toInt() < initialAmmo);

        QVERIFY(source.setTriggerHeld(false));
        QVERIFY(source.advanceByMs(2000));
        robot = findRobot(source.snapshot(), QStringLiteral("red-3"));
        QVERIFY(robot.value(QStringLiteral("heat")).toInt()
                < robot.value(QStringLiteral("heatLimit")).toInt());
    }

    void resetRestoresInitialAndZeroClock() {
        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(
            scenarioPath(QStringLiteral("match_3v3_basic.json"))));
        QVERIFY(source.start());
        QVERIFY(source.advanceByMs(12000));
        QCOMPARE(source.snapshot().value(QStringLiteral("phase")).toString(),
                 QStringLiteral("battle"));

        QVERIFY(source.reset());
        QCOMPARE(source.elapsedSec(), 0);
        QVERIFY(!source.isRunning());
        QCOMPARE(source.snapshot().value(QStringLiteral("phase")).toString(),
                 QStringLiteral("preparation"));
        QCOMPARE(source.snapshot()
                     .value(QStringLiteral("timeRemainingSec"))
                     .toInt(),
                 5);
    }

    void offlineSceneDisconnectsAndRecoversRobot() {
        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(
            scenarioPath(QStringLiteral("match_3v3_offline.json"))));
        QVERIFY(source.start());
        QVERIFY(source.advanceByMs(5000));
        QVERIFY(!findRobot(source.snapshot(), QStringLiteral("red-3"))
                     .value(QStringLiteral("online"))
                     .toBool());

        QVERIFY(source.advanceByMs(10000));
        QVERIFY(findRobot(source.snapshot(), QStringLiteral("red-3"))
                    .value(QStringLiteral("online"))
                    .toBool());
    }

    void damageSceneUpdatesDeathAndRespawn() {
        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(
            scenarioPath(QStringLiteral("match_3v3_damage.json"))));
        QVERIFY(source.start());
        QVERIFY(source.advanceByMs(10000));

        QVariantMap blueThree =
            findRobot(source.snapshot(), QStringLiteral("blue-3"));
        QCOMPARE(blueThree.value(QStringLiteral("health")).toInt(), 0);
        QVERIFY(!blueThree.value(QStringLiteral("alive")).toBool());
        QCOMPARE(blueThree.value(QStringLiteral("respawnRemainingSec")).toInt(),
                 36);

        QVERIFY(source.advanceByMs(30000));
        blueThree = findRobot(source.snapshot(), QStringLiteral("blue-3"));
        QCOMPARE(blueThree.value(QStringLiteral("health")).toInt(), 400);
        QVERIFY(blueThree.value(QStringLiteral("alive")).toBool());
    }

    void settlementSceneCompletesAtScenarioDuration() {
        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(
            scenarioPath(QStringLiteral("match_3v3_settlement.json"))));
        QSignalSpy completedSpy(&source,
                                &RM::MockMatchSource::scenarioCompleted);
        QVERIFY(source.start());
        QVERIFY(source.advanceByMs(8000));
        QCOMPARE(source.snapshot().value(QStringLiteral("phase")).toString(),
                 QStringLiteral("settlement"));
        QCOMPARE(source.snapshot()
                     .value(QStringLiteral("redTeam"))
                     .toMap()
                     .value(QStringLiteral("score"))
                     .toInt(),
                 3);

        QVERIFY(source.advanceByMs(12000));
        QVERIFY(source.isCompleted());
        QVERIFY(!source.isRunning());
        QCOMPARE(completedSpy.count(), 1);
    }

    void malformedScenarioIsRejectedWithoutReplacingCurrentScenario() {
        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(
            scenarioPath(QStringLiteral("match_3v3_basic.json"))));
        const QString previousId = source.scenarioId();

        const QByteArray invalid = R"JSON({
            "schemaVersion": 1,
            "id": "invalid",
            "name": "invalid",
            "durationSec": 10,
            "initial": {
                "matchId": "invalid",
                "phase": "unknown_phase",
                "timeRemainingSec": 10,
                "round": 1,
                "redTeam": {"name": "红方", "score": 0, "robots": []},
                "blueTeam": {"name": "蓝方", "score": 0, "robots": []}
            },
            "timeline": []
        })JSON";

        QVERIFY(!source.loadFromJson(invalid, QStringLiteral("inline")));
        QVERIFY(!source.lastError().isEmpty());
        QCOMPARE(source.scenarioId(), previousId);
        QVERIFY(source.isLoaded());
        QVERIFY(source.matchSession()->isValid());
    }
};

QTEST_GUILESS_MAIN(TestMockMatchSource)
#include "test_mock_match_source.moc"
