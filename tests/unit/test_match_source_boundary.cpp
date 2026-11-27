#include "core/MatchDataHealth.h"
#include "core/GameData.h"
#include "core/GameDataMatchSource.h"
#include "core/MatchSourceController.h"
#include "core/MatchSnapshotJson.h"
#include "simulator/MatchRecorder.h"
#include "simulator/MatchReplaySource.h"
#include "simulator/MockMatchSource.h"
#include "ui/MatchUiProjection.h"

#include <QDateTime>
#include <QTemporaryDir>
#include <QtTest>

class TestMatchSourceBoundary : public QObject {
    Q_OBJECT

private slots:
    void duelRealDataAndReplayUseSameUiContract() {
        GameData data;
        data.setMatchProfile(RM::MatchProfile::Rmul1v1);
        data.setCurrentRobotId(103);
        data.updateGameStateFromS1(4, 120, 0, 0, 1, false);
        auto &blue = data.getRobotDataRef(103);
        blue.currentHP = 300;
        blue.maxHP = 400;
        blue.status = RobotStatus::NORMAL;
        blue.isClientConnected = true;
        blue.currentHeat = 40;
        blue.heatLimit = 200;
        blue.allowedAmmo17mm = 80;
        RM::GameDataMatchSource real(&data);
        QVERIFY2(real.isAvailable(), qPrintable(real.statusText()));
        RM::MatchUiProjection ui(real.matchSession(), nullptr, "real");
        QCOMPARE(ui.robotId(), 103);
        QCOMPARE(ui.currentHealth(), 300);
        QCOMPARE(ui.fieldWidth(), 5.0);
        QVERIFY(!ui.hasControlPoints());
        QCOMPARE(ui.allyRobotList().size(), 1);
        QCOMPARE(ui.enemyRobotList().size(), 1);
        QVERIFY(!ui.enemyRobotList()[0].toMap().contains("energy"));
        QVERIFY(ui.headerData()["matchMode"].toString().startsWith("1v1"));
        QCOMPARE(ui.mapData()["mapImageSource"].toString(),
                 QStringLiteral("qrc:/images/rmul1v1.png"));

        QTemporaryDir directory;
        RM::MockMatchSource mock;
        QVERIFY(mock.selectScenario("match_1v1_basic"));
        RM::MatchRecorder recorder;
        const QString path = directory.filePath("duel.jsonl");
        QVERIFY(recorder.start(mock.matchSession(), path));
        QVERIFY(mock.advanceByMs(12000));
        QVERIFY(mock.pause());
        recorder.stop();
        RM::MatchReplaySource replay;
        QVERIFY(replay.loadFromFile(path));
        QVERIFY(replay.start());
        QVERIFY(replay.advanceByMs(12000));
        QCOMPARE(replay.matchSession()->matchSnapshot().profile,
                 QStringLiteral("rmul1v1"));
        QCOMPARE(replay.matchSession()->matchSnapshot().redTeam.robots.size(), 1);
        QCOMPARE(replay.matchSession()->matchSnapshot().timeRemainingSec, 120);
    }

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
        QCOMPARE(health.publicRobots, 5);
        QCOMPARE(health.privateRobots, 1);
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

    void snapshotJsonRoundTripPreservesMatchState() {
        RM::MatchSourceController controller;
        QVERIFY(controller.selectMock());
        const RM::MatchSnapshot original =
            controller.matchSession()->matchSnapshot();
        const QJsonObject json = RM::matchSnapshotToJson(original);
        RM::MatchSnapshot restored;
        QString error;
        QVERIFY2(RM::matchSnapshotFromJson(json, &restored, &error),
                 qPrintable(error));
        QCOMPARE(restored.matchId, original.matchId);
        QCOMPARE(restored.timeRemainingSec, original.timeRemainingSec);
        QCOMPARE(restored.redTeam.robots.size(),
                 original.redTeam.robots.size());
        QCOMPARE(restored.redTeam.robots.first().id,
                 original.redTeam.robots.first().id);
        QCOMPARE(restored.redTeam.robots.first().energy.superCapPercent,
                 original.redTeam.robots.first().energy.superCapPercent);
        QCOMPARE(restored.redTeam.robots.first().telemetryScope,
                 RM::MatchTelemetryScope::Public);
        QCOMPARE(restored.operatorRobotId, QStringLiteral("red-3"));
        QCOMPARE(restored.redTeam.robots[1].telemetryScope,
                 RM::MatchTelemetryScope::Private);
        QCOMPARE(restored.blueTeam.robots.first().telemetryScope,
                 RM::MatchTelemetryScope::Public);
        QCOMPARE(restored.redTeam.robots[1].moduleStates.size(), 10);
        QCOMPARE(restored.redTeam.robots[1].moduleStates[2].toInt(), 1);
        QVERIFY(restored.redTeam.robots[1].buffTimedData.contains(
            QStringLiteral("attack")));

        RM::MatchSnapshot extended = original;
        extended.redTeam.economy = 1200;
        extended.blueTeam.economy = 900;
        extended.redTeam.controlPoints = 8;
        extended.blueTeam.controlPoints = 7;
        extended.redTeam.totalDamage = 2500;
        extended.isPaused = true;
        extended.operatorRobotId = QStringLiteral("blue-3");
        RM::MatchSnapshot extendedRestored;
        QVERIFY(RM::matchSnapshotFromJson(
            RM::matchSnapshotToJson(extended), &extendedRestored, &error));
        QCOMPARE(extendedRestored.redTeam.economy, 1200);
        QCOMPARE(extendedRestored.blueTeam.controlPoints, 7);
        QVERIFY(extendedRestored.isPaused);
        QCOMPARE(extendedRestored.operatorRobotId,
                 QStringLiteral("blue-3"));
        QCOMPARE(extendedRestored.blueTeam.robots[1].telemetryScope,
                 RM::MatchTelemetryScope::Private);
    }

    void recordedMatchCanBeReplayed() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(
            QStringLiteral("match.jsonl"));

        RM::MockMatchSource source;
        QVERIFY(source.loadFromFile(
            QStringLiteral(":/mock/3v3/match_3v3_basic.json")));
        RM::MatchRecorder recorder;
        QVERIFY(recorder.start(source.matchSession(), path));
        QVERIFY(source.start());
        QVERIFY(source.advanceByMs(12000));
        QVERIFY(source.advanceByMs(8000));
        recorder.stop();
        QVERIFY(recorder.frameCount() >= 3);

        RM::MatchReplaySource replay;
        QVERIFY2(replay.loadFromFile(path),
                 qPrintable(replay.statusText()));
        QVERIFY(replay.isAvailable());
        QCOMPARE(replay.sourceKind(), QStringLiteral("replay"));
        QVERIFY(replay.frameCount() >= 3);
        QVERIFY(replay.start());
        QVERIFY(replay.advanceByMs(20000));
        QCOMPARE(replay.matchSession()->matchSnapshot().phase,
                 RM::MatchPhase::Battle);
        QVERIFY(replay.matchSession()->matchSnapshot().redTeam.score >= 1);
    }

    void controllerSelectsReplaySource() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(
            QStringLiteral("match.jsonl"));

        RM::MockMatchSource mock;
        QVERIFY(mock.loadFromFile(
            QStringLiteral(":/mock/3v3/match_3v3_basic.json")));
        RM::MatchRecorder recorder;
        QVERIFY(recorder.start(mock.matchSession(), path));
        QVERIFY(mock.start());
        QVERIFY(mock.advanceByMs(12000));
        recorder.stop();

        RM::MatchSourceController controller;
        QVERIFY(controller.selectReplay(path));
        QCOMPARE(controller.sourceKind(), QStringLiteral("replay"));
        QVERIFY(controller.isActive());
        QVERIFY(controller.matchSession()->isValid());
    }

    void gameDataAdapterBuildsThreeVersusThreeSnapshot() {
        GameData gameData;
        gameData.setMatchProfile(RM::MatchProfile::Rmul3v3);
        gameData.setCurrentRobotId(3);
        gameData.updateGameStateFromS1(
            static_cast<int>(GameStage::BATTLE), 300, 2, 1, 1, true);
        RobotData &current = gameData.getRobotDataRef(3);
        current.currentHP = 320;
        current.maxHP = 400;
        current.currentHeat = 45;
        current.heatLimit = 200;
        current.level = 3;
        current.experience = 750;
        current.maxExperience = 1000;
        current.allowedAmmo17mm = 80;
        current.bufferEnergy = 42;
        current.maxBufferEnergy = 60;
        current.superCapEnergyPercent = 72.5f;
        current.muzzleVelocity = 24.0f;
        current.shootSpeedLimit = 30.0f;
        current.isOutOfCombat = false;
        current.fricEnabled = true;
        current.rammerEnabled = true;
        current.buffs.attack.level = 1;
        current.buffs.attack.maxTime = 10;
        current.buffs.attack.leftTimeAtSync = 6;
        current.status = RobotStatus::NORMAL;
        current.isClientConnected = true;
        current.lastUpdateTime = QDateTime::currentDateTime();
        robomaster::RobotModuleStatus modules;
        modules.set_rfid(1);
        modules.set_video_transmission(1);
        gameData.updateRobotModuleStatus(modules);
        gameData.setRedEconomy(1200);
        gameData.setBlueEconomy(900);
        DamageEventData damage{};
        damage.attackerId = 3;
        damage.victimId = 103;
        damage.damage = 250;
        damage.armorId = 1;
        damage.hurtType = 0;
        gameData.recordDamageEvent(damage);

        RM::GameDataMatchSource source(&gameData);
        gameData.killEventOccurred(KillRecord(3, 103, 1, false));
        source.refresh();
        QVERIFY2(source.isAvailable(), qPrintable(source.statusText()));
        QCOMPARE(source.sourceKind(), QStringLiteral("real"));
        const RM::MatchSnapshot snapshot =
            source.matchSession()->matchSnapshot();
        QCOMPARE(snapshot.phase, RM::MatchPhase::Battle);
        QVERIFY(snapshot.isPaused);
        QCOMPARE(snapshot.timeRemainingSec, 300);
        QCOMPARE(snapshot.redTeam.robots.size(), 3);
        QCOMPARE(snapshot.blueTeam.robots.size(), 3);
        QCOMPARE(snapshot.operatorRobotId, QStringLiteral("red-3"));
        QCOMPARE(snapshot.redTeam.robots[1].id, QStringLiteral("red-3"));
        QCOMPARE(snapshot.redTeam.robots[1].telemetryScope,
                 RM::MatchTelemetryScope::Private);
        QCOMPARE(snapshot.blueTeam.robots[1].telemetryScope,
                 RM::MatchTelemetryScope::Public);
        QCOMPARE(snapshot.redTeam.robots[1].health, 320);
        QCOMPARE(snapshot.redTeam.robots[1].level, 3);
        QCOMPARE(snapshot.redTeam.robots[1].experience, 750);
        QCOMPARE(snapshot.redTeam.robots[1].maxExperience, 1000);
        QCOMPARE(snapshot.redTeam.robots[1].muzzleVelocityMps, 24.0);
        QCOMPARE(snapshot.redTeam.robots[1].shootSpeedLimitMps, 30.0);
        QVERIFY(!snapshot.redTeam.robots[1].outOfCombat);
        QCOMPARE(snapshot.redTeam.robots[1].energy.superCapPercent, 72.5);
        QCOMPARE(snapshot.redTeam.robots[1].moduleStates[3].toInt(), 0);
        QCOMPARE(snapshot.redTeam.robots[1].moduleStates[4].toInt(), 0);
        QVERIFY(snapshot.redTeam.robots[1].buffTimedData.contains(
            QStringLiteral("attack")));
        QCOMPARE(snapshot.redTeam.robots[1]
                     .customData.value(QStringLiteral("fricEnabled"))
                     .toBool(),
                 true);
        QCOMPARE(snapshot.redTeam.economy, 1200);
        QCOMPARE(snapshot.blueTeam.economy, 900);
        QCOMPARE(snapshot.redTeam.totalDamage, 250);
        QVERIFY(!snapshot.events.isEmpty());
        QCOMPARE(snapshot.events.last().type, QStringLiteral("kill"));
    }
};

QTEST_GUILESS_MAIN(TestMatchSourceBoundary)
#include "test_match_source_boundary.moc"
