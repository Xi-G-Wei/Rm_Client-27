#include "core/MatchSession.h"

#include <QSignalSpy>
#include <QtTest>

namespace {

RM::MatchRobotSnapshot makeRobot(RM::MatchSide side,
                                 int slot,
                                 RM::MatchRobotRole role,
                                 const QString &id) {
    RM::MatchRobotSnapshot robot;
    robot.id = id;
    robot.slot = slot;
    robot.side = side;
    robot.role = role;
    robot.health = 400;
    robot.maxHealth = 400;
    robot.ammo = 100;
    robot.heat = 20;
    robot.heatLimit = 200;
    robot.chassisPower = 60;
    robot.online = true;
    robot.alive = true;
    robot.respawnRemainingSec = 0;
    robot.hasPosition = true;
    robot.mapX = side == RM::MatchSide::Red ? 0.25 : 0.75;
    robot.mapY = 0.5;
    robot.headingDegrees = 90.0;
    robot.updatedAtMs = 1000;
    return robot;
}

RM::MatchSnapshot makeValidSnapshot() {
    RM::MatchSnapshot snapshot;
    snapshot.matchId = QStringLiteral("mock-3v3-001");
    snapshot.profile = QStringLiteral("rmul3v3");
    snapshot.phase = RM::MatchPhase::Battle;
    snapshot.timeRemainingSec = 300;
    snapshot.round = 1;
    snapshot.updatedAtMs = 1000;

    snapshot.redTeam.name = QStringLiteral("红方");
    snapshot.redTeam.score = 1;
    snapshot.redTeam.robots = {
        makeRobot(RM::MatchSide::Red, 1, RM::MatchRobotRole::Heavy,
                  QStringLiteral("R1")),
        makeRobot(RM::MatchSide::Red, 2, RM::MatchRobotRole::Infantry,
                  QStringLiteral("R2")),
        makeRobot(RM::MatchSide::Red, 3, RM::MatchRobotRole::Infantry,
                  QStringLiteral("R3")),
    };

    snapshot.blueTeam.name = QStringLiteral("蓝方");
    snapshot.blueTeam.score = 0;
    snapshot.blueTeam.robots = {
        makeRobot(RM::MatchSide::Blue, 1, RM::MatchRobotRole::Heavy,
                  QStringLiteral("B1")),
        makeRobot(RM::MatchSide::Blue, 2, RM::MatchRobotRole::Infantry,
                  QStringLiteral("B2")),
        makeRobot(RM::MatchSide::Blue, 3, RM::MatchRobotRole::Infantry,
                  QStringLiteral("B3")),
    };

    snapshot.events = {
        {1, 1000, QStringLiteral("battle_started"),
         QStringLiteral("比赛开始"), false, RM::MatchSide::Red, -1},
    };
    return snapshot;
}

bool hasErrorAt(const QList<RM::MatchValidationIssue> &issues,
                const QString &pathFragment) {
    for (const RM::MatchValidationIssue &issue : issues) {
        if (issue.severity == RM::MatchValidationSeverity::Error &&
            issue.path.contains(pathFragment)) {
            return true;
        }
    }
    return false;
}

} // namespace

class TestMatchSession : public QObject {
    Q_OBJECT

private slots:
    void validSnapshotProducesQmlFriendlyMap() {
        RM::MatchSnapshot snapshot = makeValidSnapshot();
        const QList<RM::MatchValidationIssue> issues =
            RM::validateMatchSnapshot(snapshot);

        QVERIFY(!RM::hasMatchValidationError(issues));

        const QVariantMap map = snapshot.toVariantMap();
        QCOMPARE(map.value(QStringLiteral("profile")).toString(),
                 QStringLiteral("rmul3v3"));
        QCOMPARE(map.value(QStringLiteral("phase")).toString(),
                 QStringLiteral("battle"));
        QCOMPARE(map.value(QStringLiteral("timeRemainingSec")).toInt(), 300);

        const QVariantMap red =
            map.value(QStringLiteral("redTeam")).toMap();
        QCOMPARE(red.value(QStringLiteral("robots")).toList().size(), 3);
        const QVariantMap firstRobot =
            red.value(QStringLiteral("robots"))
                .toList()
                .first()
                .toMap();
        QCOMPARE(firstRobot.value(QStringLiteral("role")).toString(),
                 QStringLiteral("heavy"));
        QCOMPARE(firstRobot.value(QStringLiteral("roleLabel")).toString(),
                 QStringLiteral("重装"));
        QVERIFY(firstRobot.contains(QStringLiteral("position")));
    }

    void threeRobotsPerTeamAreRequired() {
        RM::MatchSnapshot snapshot = makeValidSnapshot();
        snapshot.blueTeam.robots.removeLast();

        const QList<RM::MatchValidationIssue> issues =
            RM::validateMatchSnapshot(snapshot);

        QVERIFY(hasErrorAt(issues, QStringLiteral("blueTeam.robots")));
        QVERIFY(RM::hasMatchValidationError(issues));
    }

    void duplicateRobotIdsAreRejected() {
        RM::MatchSnapshot snapshot = makeValidSnapshot();
        snapshot.blueTeam.robots[0].id =
            snapshot.redTeam.robots[0].id.toLower();

        const QList<RM::MatchValidationIssue> issues =
            RM::validateMatchSnapshot(snapshot);

        QVERIFY(hasErrorAt(issues, QStringLiteral("robots.id")));
    }

    void robotSideMustMatchOwningTeam() {
        RM::MatchSnapshot snapshot = makeValidSnapshot();
        snapshot.redTeam.robots[1].side = RM::MatchSide::Blue;

        const QList<RM::MatchValidationIssue> issues =
            RM::validateMatchSnapshot(snapshot);

        QVERIFY(hasErrorAt(issues, QStringLiteral(".side")));
    }

    void healthCannotExceedMaximum() {
        RM::MatchSnapshot snapshot = makeValidSnapshot();
        snapshot.redTeam.robots[0].health = 401;

        const QList<RM::MatchValidationIssue> issues =
            RM::validateMatchSnapshot(snapshot);

        QVERIFY(hasErrorAt(issues, QStringLiteral("redTeam.robots[1].health")));
    }

    void unknownMetricsRemainUnknown() {
        RM::MatchSnapshot snapshot = makeValidSnapshot();
        RM::MatchRobotSnapshot &robot = snapshot.redTeam.robots[0];
        robot.health = RM::kUnknownMatchMetric;
        robot.maxHealth = RM::kUnknownMatchMetric;
        robot.ammo = RM::kUnknownMatchMetric;
        robot.heat = RM::kUnknownMatchMetric;
        robot.heatLimit = RM::kUnknownMatchMetric;
        robot.chassisPower = RM::kUnknownMatchMetric;

        const QList<RM::MatchValidationIssue> issues =
            RM::validateMatchSnapshot(snapshot);
        QVERIFY(!RM::hasMatchValidationError(issues));

        const QVariantList robots =
            snapshot.redTeam.toVariantMap()
                .value(QStringLiteral("robots"))
                .toList();
        const QVariantMap robotMap = robots.first().toMap();
        QCOMPARE(robotMap.value(QStringLiteral("health")).toInt(), -1);
        QCOMPARE(robotMap.value(QStringLiteral("ammo")).toInt(), -1);
    }

    void invalidReplacementDoesNotOverwriteValidSnapshot() {
        RM::MatchSession session;
        const RM::MatchSnapshot valid = makeValidSnapshot();
        QVERIFY(session.replaceSnapshot(valid));
        QVERIFY(session.isValid());
        QCOMPARE(session.matchSnapshot().matchId,
                 QStringLiteral("mock-3v3-001"));

        RM::MatchSnapshot invalid = makeValidSnapshot();
        invalid.matchId = QStringLiteral("broken");
        invalid.redTeam.robots.clear();
        QString error;
        QVERIFY(!session.replaceSnapshot(invalid, &error));
        QVERIFY(!error.isEmpty());

        QCOMPARE(session.matchSnapshot().matchId,
                 QStringLiteral("mock-3v3-001"));
        QCOMPARE(session.snapshot().value(QStringLiteral("matchId")).toString(),
                 QStringLiteral("mock-3v3-001"));
    }

    void snapshotSignalsOnlyFireForAcceptedData() {
        RM::MatchSession session;
        const RM::MatchSnapshot valid = makeValidSnapshot();
        QSignalSpy snapshotSpy(&session, &RM::MatchSession::snapshotChanged);
        QSignalSpy validationSpy(&session, &RM::MatchSession::validationChanged);

        QVERIFY(session.replaceSnapshot(valid));
        QCOMPARE(snapshotSpy.count(), 1);
        QCOMPARE(validationSpy.count(), 1);

        RM::MatchSnapshot invalid = makeValidSnapshot();
        invalid.profile = QStringLiteral("unsupported");
        QVERIFY(!session.replaceSnapshot(invalid));
        QCOMPARE(snapshotSpy.count(), 1);
        QCOMPARE(validationSpy.count(), 2);
    }
};

QTEST_GUILESS_MAIN(TestMatchSession)
#include "test_match_session.moc"
