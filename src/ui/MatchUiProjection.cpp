#include "ui/MatchUiProjection.h"

#include "core/MatchSession.h"

#include <QDateTime>

namespace RM {
namespace {

QString formatClock(int seconds) {
    const int bounded = qMax(0, seconds);
    return QStringLiteral("%1:%2")
        .arg(bounded / 60, 2, 10, QLatin1Char('0'))
        .arg(bounded % 60, 2, 10, QLatin1Char('0'));
}

QString gamePhaseText(MatchPhase phase) {
    switch (phase) {
    case MatchPhase::NotStarted: return QStringLiteral("等待开赛");
    case MatchPhase::Preparation: return QStringLiteral("准备阶段");
    case MatchPhase::SelfCheck: return QStringLiteral("裁判系统自检");
    case MatchPhase::Countdown: return QStringLiteral("5秒倒计时");
    case MatchPhase::Battle: return QStringLiteral("战斗阶段");
    case MatchPhase::Settlement: return QStringLiteral("比赛结算中");
    }
    return QStringLiteral("等待开赛");
}

QString robotName(const MatchRobotSnapshot &robot) {
    const QString role = matchRobotRoleLabel(robot.role);
    if (robot.role == MatchRobotRole::Infantry) {
        return role + QString::number(robot.slot);
    }
    return role;
}

QVariantMap robotCard(const MatchRobotSnapshot &robot) {
    QVariantMap card{
        {QStringLiteral("number"), robot.slot},
        {QStringLiteral("id"), robot.id},
        {QStringLiteral("name"), robotName(robot)},
        {QStringLiteral("role"), matchRobotRoleKey(robot.role)},
        {QStringLiteral("roleLabel"), matchRobotRoleLabel(robot.role)},
        {QStringLiteral("selected"), false},
        {QStringLiteral("online"), robot.online},
        {QStringLiteral("alive"), robot.alive},
        {QStringLiteral("effects"), QVariantList{}},
        {QStringLiteral("energy"), robot.energy.toVariantMap()},
        {QStringLiteral("chassis"), robot.chassis.toVariantMap()},
        {QStringLiteral("wheelLeg"), robot.wheelLeg.toVariantMap()},
    };

    if (robot.health != kUnknownMatchMetric) {
        card.insert(QStringLiteral("hp"), robot.health);
    }
    if (robot.maxHealth > 0) {
        card.insert(QStringLiteral("maxHp"), robot.maxHealth);
    }
    if (robot.ammo != kUnknownMatchMetric) {
        card.insert(QStringLiteral("ammo"), robot.ammo);
        card.insert(QStringLiteral("ammoPeak"), robot.ammo);
    }
    if (robot.heat != kUnknownMatchMetric) {
        card.insert(QStringLiteral("heat"), robot.heat);
    }
    if (robot.heatLimit > 0) {
        card.insert(QStringLiteral("maxHeat"), robot.heatLimit);
        card.insert(QStringLiteral("heatLimit"), robot.heatLimit);
    }
    if (robot.energy.chassisPowerLimitW >= 0.0) {
        card.insert(QStringLiteral("maxPower"),
                    robot.energy.chassisPowerLimitW);
    }
    if (!robot.alive && robot.respawnRemainingSec >= 0) {
        card.insert(QStringLiteral("respawnLeft"),
                    robot.respawnRemainingSec);
        card.insert(QStringLiteral("respawnTotal"),
                    qMax(1, robot.respawnRemainingSec));
    }
    if (robot.hasPosition) {
        card.insert(QStringLiteral("x"), robot.mapX);
        card.insert(QStringLiteral("y"), robot.mapY);
        card.insert(QStringLiteral("angle"), robot.headingDegrees);
    }
    return card;
}

QVariantList robotCards(const MatchTeamSnapshot &team) {
    QVariantList cards;
    cards.reserve(team.robots.size());
    for (const MatchRobotSnapshot &robot : team.robots) {
        cards.append(robotCard(robot));
    }
    return cards;
}

QVariantList positionedRobots(const MatchTeamSnapshot &team,
                              bool ally) {
    QVariantList robots;
    for (const MatchRobotSnapshot &robot : team.robots) {
        if (!robot.hasPosition) {
            continue;
        }
        QVariantMap map = robotCard(robot);
        map.insert(QStringLiteral("team"),
                   ally ? QStringLiteral("ally") : QStringLiteral("enemy"));
        map.insert(QStringLiteral("ageMs"), 0);
        map.insert(QStringLiteral("stale"), false);
        map.insert(QStringLiteral("visible"), true);
        map.insert(QStringLiteral("predicted"), false);
        map.insert(QStringLiteral("focus"), false);
        map.insert(QStringLiteral("hpPct"),
                   robot.maxHealth > 0
                       ? qBound(0.0,
                                robot.health * 100.0 / robot.maxHealth,
                                100.0)
                       : -1.0);
        robots.append(map);
    }
    return robots;
}

QString eventColor(const QString &type) {
    if (type.contains(QStringLiteral("damage"))
        || type.contains(QStringLiteral("kill"))
        || type.contains(QStringLiteral("offline"))) {
        return QStringLiteral("#ff9b8e");
    }
    if (type.contains(QStringLiteral("score"))
        || type.contains(QStringLiteral("battle"))
        || type.contains(QStringLiteral("respawn"))) {
        return QStringLiteral("#8fe2ae");
    }
    if (type.contains(QStringLiteral("cap"))
        || type.contains(QStringLiteral("power"))
        || type.contains(QStringLiteral("wheel"))) {
        return QStringLiteral("#f4cb87");
    }
    return QStringLiteral("#a9c6d8");
}

QString eventPriority(const QString &type) {
    if (type.contains(QStringLiteral("offline"))
        || type.contains(QStringLiteral("kill"))) {
        return QStringLiteral("P0");
    }
    if (type.contains(QStringLiteral("damage"))
        || type.contains(QStringLiteral("cap"))) {
        return QStringLiteral("P1");
    }
    return QStringLiteral("P2");
}

} // namespace

MatchUiProjection::MatchUiProjection(MatchSession *session, QObject *parent)
    : QObject(parent), m_session(session) {
    if (m_session) {
        connect(m_session,
                &MatchSession::snapshotChanged,
                this,
                &MatchUiProjection::updated);
    }
}

bool MatchUiProjection::active() const {
    return m_session && m_session->isValid();
}

bool MatchUiProjection::mockMode() const {
    return true;
}

QVariantMap MatchUiProjection::snapshot() const {
    return m_session ? m_session->snapshot() : QVariantMap{};
}

QVariantMap MatchUiProjection::dashboardData() const {
    if (!m_session) {
        return {};
    }
    const MatchSnapshot &snapshot = m_session->matchSnapshot();
    const int redScore = snapshot.redTeam.score;
    const int blueScore = snapshot.blueTeam.score;
    int leadingSide = 0;
    QString leadText = QStringLiteral("本地模拟 · 比分持平");
    if (redScore > blueScore) {
        leadingSide = 1;
        leadText = QStringLiteral("本地模拟 · 红方领先");
    } else if (blueScore > redScore) {
        leadingSide = 2;
        leadText = QStringLiteral("本地模拟 · 蓝方领先");
    }
    return {
        {QStringLiteral("redRobots"), robotCards(snapshot.redTeam)},
        {QStringLiteral("blueRobots"), robotCards(snapshot.blueTeam)},
        {QStringLiteral("leadingSide"), leadingSide},
        {QStringLiteral("leadText"), leadText},
        {QStringLiteral("leadReason"),
         QStringLiteral("%1 : %2").arg(redScore).arg(blueScore)},
        {QStringLiteral("statusFresh"), true},
        {QStringLiteral("mockMode"), true},
        {QStringLiteral("economyReported"), false},
        {QStringLiteral("redDarts"), QVariantList{}},
        {QStringLiteral("blueDarts"), QVariantList{}},
        {QStringLiteral("playerName"), QStringLiteral("本地模拟操作手")},
    };
}

QVariantMap MatchUiProjection::headerData() const {
    if (!m_session) {
        return {};
    }
    const MatchSnapshot &snapshot = m_session->matchSnapshot();
    return {
        {QStringLiteral("matchMode"), QStringLiteral("3v3 模拟")},
        {QStringLiteral("redScore"), snapshot.redTeam.score},
        {QStringLiteral("blueScore"), snapshot.blueTeam.score},
        {QStringLiteral("currentRound"), snapshot.round},
        {QStringLiteral("totalRounds"), 1},
        {QStringLiteral("timeRemaining"),
         formatClock(snapshot.timeRemainingSec)},
        {QStringLiteral("stage"), matchPhaseKey(snapshot.phase)},
        {QStringLiteral("stageLabel"), matchPhaseLabel(snapshot.phase)},
        {QStringLiteral("linkStatus"), QStringLiteral("本地模拟")},
        {QStringLiteral("linkLatency"), 0},
    };
}

QVariantMap MatchUiProjection::resourceData() const {
    return {
        {QStringLiteral("allyBaseHp"), 0},
        {QStringLiteral("allyBaseMax"), 0},
        {QStringLiteral("enemyBaseHp"), 0},
        {QStringLiteral("enemyBaseMax"), 0},
        {QStringLiteral("allyOutpostHp"), 0},
        {QStringLiteral("allyOutpostMax"), 0},
        {QStringLiteral("enemyOutpostHp"), 0},
        {QStringLiteral("enemyOutpostMax"), 0},
        {QStringLiteral("economyDiff"), 0},
        {QStringLiteral("damageDiff"), 0},
        {QStringLiteral("hpDiff"), 0},
    };
}

QVariantMap MatchUiProjection::topStatusData() const {
    return {
        {QStringLiteral("allyRemainingEconomy"), 0},
        {QStringLiteral("allyTotalEconomyObtained"), 0},
        {QStringLiteral("allyTechLevel"), 0},
        {QStringLiteral("allyEncryptionLevel"), 0},
        {QStringLiteral("allyFortressOccupationSec"), 0},
        {QStringLiteral("enemyFortressOccupationSec"), 0},
        {QStringLiteral("respawnGoldCost"), 0},
        {QStringLiteral("affordableRespawnCount"), 0},
        {QStringLiteral("respawnEconomyVisible"), false},
    };
}

QVariantList MatchUiProjection::keyEvents() const {
    if (!m_session) {
        return {};
    }
    const QList<MatchEventSnapshot> &events =
        m_session->matchSnapshot().events;
    QVariantList rows;
    for (int index = events.size() - 1; index >= 0; --index) {
        const MatchEventSnapshot &event = events.at(index);
        rows.append(QVariantMap{
            {QStringLiteral("time"),
             QDateTime::fromMSecsSinceEpoch(event.occurredAtMs)
                 .toString(QStringLiteral("hh:mm:ss"))},
            {QStringLiteral("icon"), QStringLiteral("!")},
            {QStringLiteral("text"), event.message},
            {QStringLiteral("color"), eventColor(event.type)},
            {QStringLiteral("priority"), eventPriority(event.type)},
        });
    }
    return rows;
}

QVariantList MatchUiProjection::allyRobotList() const {
    if (!m_session) {
        return {};
    }
    return robotCards(m_session->matchSnapshot().redTeam);
}

QVariantList MatchUiProjection::enemyRobotList() const {
    if (!m_session) {
        return {};
    }
    return robotCards(m_session->matchSnapshot().blueTeam);
}

QVariantMap MatchUiProjection::mapData() const {
    if (!m_session) {
        return {};
    }
    const MatchSnapshot &snapshot = m_session->matchSnapshot();
    return {
        {QStringLiteral("radarAgeMs"), 0},
        {QStringLiteral("allyRobots"),
         positionedRobots(snapshot.redTeam, true)},
        {QStringLiteral("enemyRobots"),
         positionedRobots(snapshot.blueTeam, false)},
        {QStringLiteral("focusTargetId"), QString()},
        {QStringLiteral("mapImageSource"),
         QStringLiteral("qrc:/images/rmul3v3.png")},
        {QStringLiteral("routes"), QVariantList{}},
        {QStringLiteral("dangerZones"), QVariantList{}},
        {QStringLiteral("buffZones"), QVariantList{}},
    };
}

QVariantMap MatchUiProjection::cameraPreviewData() const {
    return {
        {QStringLiteral("connected"), false},
        {QStringLiteral("fps"), QStringLiteral("--")},
        {QStringLiteral("latencyMs"), 0},
        {QStringLiteral("decodeMs"), 0},
        {QStringLiteral("renderMs"), 0},
        {QStringLiteral("grayFrameRate"), 0},
        {QStringLiteral("stall"), false},
        {QStringLiteral("sourceName"), QStringLiteral("本地模拟")},
    };
}

QVariantMap MatchUiProjection::linkHealth() const {
    return {
        {QStringLiteral("mqttStatus"), QStringLiteral("mock")},
        {QStringLiteral("mqttLatencyMs"), 0},
        {QStringLiteral("videoStatus"), QStringLiteral("disabled")},
        {QStringLiteral("videoLatencyMs"), 0},
        {QStringLiteral("radarStatus"), QStringLiteral("mock")},
        {QStringLiteral("radarAgeMs"), 0},
        {QStringLiteral("commandStatus"), QStringLiteral("disabled")},
    };
}

QVariantList MatchUiProjection::analysisMetrics() const {
    return {};
}

QVariantMap MatchUiProjection::mainDecision() const {
    return {};
}

QVariantMap MatchUiProjection::predictionData() const {
    return {};
}

QString MatchUiProjection::layoutMode() const {
    return QStringLiteral("map_primary");
}

bool MatchUiProjection::hasBases() const {
    return false;
}

bool MatchUiProjection::hasControlPoints() const {
    return true;
}

double MatchUiProjection::fieldWidth() const {
    return 12.0;
}

double MatchUiProjection::fieldHeight() const {
    return 8.0;
}

int MatchUiProjection::remainingTime() const {
    return m_session ? m_session->matchSnapshot().timeRemainingSec : 0;
}

int MatchUiProjection::redScore() const {
    return m_session ? m_session->matchSnapshot().redTeam.score : 0;
}

int MatchUiProjection::blueScore() const {
    return m_session ? m_session->matchSnapshot().blueTeam.score : 0;
}

int MatchUiProjection::currentRound() const {
    return m_session ? m_session->matchSnapshot().round : 1;
}

QString MatchUiProjection::gamePhase() const {
    return m_session ? gamePhaseText(m_session->matchSnapshot().phase)
                     : QStringLiteral("等待开赛");
}

QVariantMap MatchUiProjection::territoryStatus() const {
    return {};
}

QString MatchUiProjection::redTeamName() const {
    return m_session ? m_session->matchSnapshot().redTeam.name
                     : QStringLiteral("红方");
}

QString MatchUiProjection::blueTeamName() const {
    return m_session ? m_session->matchSnapshot().blueTeam.name
                     : QStringLiteral("蓝方");
}

} // namespace RM
