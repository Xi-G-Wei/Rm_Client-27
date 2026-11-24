#include "core/MatchDataHealth.h"
#include "core/GameData.h"
#include "core/GameDataMatchSource.h"
#include "core/MatchSourceController.h"
#include "core/MatchSnapshotJson.h"
#include "simulator/MatchRecorder.h"
#include "simulator/MatchReplaySource.h"
#include "simulator/MockMatchSource.h"

#include <QDateTime>
#include <QTemporaryDir>
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
                 RM::MatchTelemetryScope::Private);
        QCOMPARE(restored.blueTeam.robots.first().telemetryScope,
                 RM::MatchTelemetryScope::Public);
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
            static_cast<int>(GameStage::BATTLE), 300, 2, 1, 1, false);
        RobotData &current = gameData.getRobotDataRef(3);
        current.currentHP = 320;
        current.maxHP = 400;
        current.currentHeat = 45;
        current.heatLimit = 200;
        current.allowedAmmo17mm = 80;
        current.bufferEnergy = 42;
        current.maxBufferEnergy = 60;
        current.superCapEnergyPercent = 72.5f;
        current.status = RobotStatus::NORMAL;
        current.isClientConnected = true;
        current.lastUpdateTime = QDateTime::currentDateTime();

        RM::GameDataMatchSource source(&gameData);
        source.refresh();
        QVERIFY2(source.isAvailable(), qPrintable(source.statusText()));
        QCOMPARE(source.sourceKind(), QStringLiteral("real"));
        const RM::MatchSnapshot snapshot =
            source.matchSession()->matchSnapshot();
        QCOMPARE(snapshot.phase, RM::MatchPhase::Battle);
        QCOMPARE(snapshot.timeRemainingSec, 300);
        QCOMPARE(snapshot.redTeam.robots.size(), 3);
        QCOMPARE(snapshot.blueTeam.robots.size(), 3);
        QCOMPARE(snapshot.redTeam.robots[1].id, QStringLiteral("red-3"));
        QCOMPARE(snapshot.redTeam.robots[1].telemetryScope,
                 RM::MatchTelemetryScope::Private);
        QCOMPARE(snapshot.blueTeam.robots[1].telemetryScope,
                 RM::MatchTelemetryScope::Public);
        QCOMPARE(snapshot.redTeam.robots[1].health, 320);
        QCOMPARE(snapshot.redTeam.robots[1].energy.superCapPercent, 72.5);
    }
};

QTEST_GUILESS_MAIN(TestMatchSourceBoundary)
#include "test_match_source_boundary.moc"
