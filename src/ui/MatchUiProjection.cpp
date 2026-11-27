#include "ui/MatchUiProjection.h"

#include "core/MatchDataHealth.h"
#include "core/MatchSession.h"

#include <QDateTime>
#include <QStringList>

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
    const bool privateTelemetry =
        robot.telemetryScope == MatchTelemetryScope::Private;
    QVariantMap card{
        {QStringLiteral("number"), robot.slot},
        {QStringLiteral("id"), robot.id},
        {QStringLiteral("name"), robotName(robot)},
        {QStringLiteral("role"), matchRobotRoleKey(robot.role)},
        {QStringLiteral("roleLabel"), matchRobotRoleLabel(robot.role)},
        {QStringLiteral("telemetryScope"),
         matchTelemetryScopeKey(robot.telemetryScope)},
        {QStringLiteral("telemetryScopeLabel"),
         matchTelemetryScopeLabel(robot.telemetryScope)},
        {QStringLiteral("selected"), false},
        {QStringLiteral("online"), robot.online},
        {QStringLiteral("alive"), robot.alive},
        {QStringLiteral("effects"), QVariantList{}},
    };

    if (robot.health != kUnknownMatchMetric) {
        card.insert(QStringLiteral("hp"), robot.health);
    }
    if (robot.maxHealth > 0) {
        card.insert(QStringLiteral("maxHp"), robot.maxHealth);
    }
    if (privateTelemetry && robot.ammo != kUnknownMatchMetric) {
        card.insert(QStringLiteral("ammo"), robot.ammo);
        card.insert(QStringLiteral("ammoPeak"), robot.ammo);
    }
    if (privateTelemetry && robot.heat != kUnknownMatchMetric) {
        card.insert(QStringLiteral("heat"), robot.heat);
    }
    if (privateTelemetry && robot.heatLimit > 0) {
        card.insert(QStringLiteral("maxHeat"), robot.heatLimit);
        card.insert(QStringLiteral("heatLimit"), robot.heatLimit);
    }
    if (privateTelemetry && !robot.alive && robot.respawnRemainingSec >= 0) {
        card.insert(QStringLiteral("respawnLeft"),
                    robot.respawnRemainingSec);
        card.insert(QStringLiteral("respawnTotal"),
                    qMax(1, robot.respawnRemainingSec));
    }
    if (privateTelemetry) {
        card.insert(QStringLiteral("energy"), robot.energy.toVariantMap());
        card.insert(QStringLiteral("chassis"), robot.chassis.toVariantMap());
        card.insert(QStringLiteral("wheelLeg"), robot.wheelLeg.toVariantMap());
        card.insert(QStringLiteral("moduleStates"), robot.moduleStates);
        card.insert(QStringLiteral("moduleData"), robot.moduleData);
        card.insert(QStringLiteral("buffTimedData"), robot.buffTimedData);
        card.insert(QStringLiteral("customData"), robot.customData);
        card.insert(QStringLiteral("level"), robot.level);
        card.insert(QStringLiteral("yellowCardCount"), robot.yellowCardCount);
    }
    if (privateTelemetry && robot.energy.chassisPowerLimitW >= 0.0) {
        card.insert(QStringLiteral("maxPower"),
                    robot.energy.chassisPowerLimitW);
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

MatchUiProjection::MatchUiProjection(MatchSession *session,
                                     QObject *parent,
                                     const QString &sourceKind)
    : QObject(parent),
      m_session(session),
      m_sourceKind(sourceKind.isEmpty() ? QStringLiteral("mock") : sourceKind) {
    if (m_session) {
        connect(m_session,
                &MatchSession::snapshotChanged,
                this,
                &MatchUiProjection::updated);
    }
}

bool MatchUiProjection::active() const {
    return m_enabled && m_session && m_session->isValid();
}

bool MatchUiProjection::isEnabled() const {
    return m_enabled;
}

void MatchUiProjection::setEnabled(bool enabled) {
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit updated();
}

bool MatchUiProjection::mockMode() const {
    return m_sourceKind == QStringLiteral("mock");
}

bool MatchUiProjection::replayMode() const {
    return m_sourceKind == QStringLiteral("replay");
}

QString MatchUiProjection::sourceKind() const {
    return m_sourceKind;
}

QString MatchUiProjection::dataHealth() const {
    if (!m_session)
        return QStringLiteral("unavailable");
    return matchDataHealthKey(
        assessMatchDataHealth(
            m_session->matchSnapshot(),
            QDateTime::currentMSecsSinceEpoch()).level);
}

int MatchUiProjection::dataAgeMs() const {
    if (!m_session)
        return -1;
    return static_cast<int>(
        assessMatchDataHealth(
            m_session->matchSnapshot(),
            QDateTime::currentMSecsSinceEpoch()).ageMs);
}

int MatchUiProjection::unknownFieldCount() const {
    if (!m_session)
        return 0;
    return assessMatchDataHealth(
        m_session->matchSnapshot(),
        QDateTime::currentMSecsSinceEpoch()).unknownFieldCount;
}

QVariantMap MatchUiProjection::snapshot() const {
    return m_session ? m_session->snapshot() : QVariantMap{};
}

QVariantMap MatchUiProjection::dashboardData() const {
    if (!m_session) {
        return {};
    }
    const MatchSnapshot &snapshot = m_session->matchSnapshot();
    const MatchDataHealth health = assessMatchDataHealth(
        snapshot, QDateTime::currentMSecsSinceEpoch());
    const int redScore = snapshot.redTeam.score;
    const int blueScore = snapshot.blueTeam.score;
    const QString sourcePrefix = mockMode()
        ? QStringLiteral("本地模拟")
        : replayMode() ? QStringLiteral("回放")
                       : QStringLiteral("真实数据");
    int leadingSide = 0;
    QString leadText = sourcePrefix + QStringLiteral(" · 比分持平");
    if (redScore > blueScore) {
        leadingSide = 1;
        leadText = sourcePrefix + QStringLiteral(" · 红方领先");
    } else if (blueScore > redScore) {
        leadingSide = 2;
        leadText = sourcePrefix + QStringLiteral(" · 蓝方领先");
    }
    const auto totalHealth = [](const MatchTeamSnapshot &team) {
        int total = 0;
        for (const auto &robot : team.robots) {
            if (robot.health < 0)
                return -1;
            total += robot.health;
        }
        return total;
    };
    return {
        {QStringLiteral("redHealth"), totalHealth(snapshot.redTeam)},
        {QStringLiteral("blueHealth"), totalHealth(snapshot.blueTeam)},
        {QStringLiteral("redDamage"), snapshot.redTeam.totalDamage},
        {QStringLiteral("blueDamage"), snapshot.blueTeam.totalDamage},
        {QStringLiteral("redControl"), snapshot.redTeam.controlPoints},
        {QStringLiteral("blueControl"), snapshot.blueTeam.controlPoints},
        {QStringLiteral("redRobots"), robotCards(snapshot.redTeam)},
        {QStringLiteral("blueRobots"), robotCards(snapshot.blueTeam)},
        {QStringLiteral("leadingSide"), leadingSide},
        {QStringLiteral("leadText"), leadText},
        {QStringLiteral("leadReason"),
         QStringLiteral("%1 : %2").arg(redScore).arg(blueScore)},
        {QStringLiteral("statusFresh"), true},
        {QStringLiteral("mockMode"), mockMode()},
        {QStringLiteral("replayMode"), replayMode()},
        {QStringLiteral("sourceKind"), sourceKind()},
        {QStringLiteral("dataHealth"), matchDataHealthKey(health.level)},
        {QStringLiteral("dataHealthLabel"),
         matchDataHealthLabel(health)},
        {QStringLiteral("dataAgeMs"), health.ageMs},
        {QStringLiteral("unknownFieldCount"), health.unknownFieldCount},
        {QStringLiteral("economyReported"),
         snapshot.redTeam.economy >= 0 || snapshot.blueTeam.economy >= 0},
        {QStringLiteral("redDarts"), QVariantList{}},
        {QStringLiteral("blueDarts"), QVariantList{}},
        {QStringLiteral("playerName"), sourcePrefix + QStringLiteral("操作手")},
    };
}

QVariantMap MatchUiProjection::headerData() const {
    if (!m_session) {
        return {};
    }
    const MatchSnapshot &snapshot = m_session->matchSnapshot();
    const QString sourceLabel = m_sourceKind == QStringLiteral("real")
        ? QStringLiteral("真实比赛")
        : m_sourceKind == QStringLiteral("replay")
            ? QStringLiteral("录像回放")
            : QStringLiteral("本地模拟");
    return {
        {QStringLiteral("matchMode"), QStringLiteral("%1 %2")
             .arg(snapshot.profile == QStringLiteral("rmul1v1")
                      ? QStringLiteral("1v1") : QStringLiteral("3v3"), sourceLabel)},
        {QStringLiteral("redScore"), snapshot.redTeam.score},
        {QStringLiteral("blueScore"), snapshot.blueTeam.score},
        {QStringLiteral("currentRound"), snapshot.round},
        {QStringLiteral("totalRounds"), 1},
        {QStringLiteral("timeRemaining"),
         formatClock(snapshot.timeRemainingSec)},
        {QStringLiteral("stage"), matchPhaseKey(snapshot.phase)},
        {QStringLiteral("stageLabel"), matchPhaseLabel(snapshot.phase)},
        {QStringLiteral("linkStatus"), sourceLabel},
        {QStringLiteral("linkLatency"), 0},
    };
}

QVariantMap MatchUiProjection::resourceData() const {
    if (!m_session) {
        return {};
    }
    const MatchSnapshot &snapshot = m_session->matchSnapshot();
    const bool allyRed = operatorRobot().side == MatchSide::Red;
    const MatchTeamSnapshot &ally = allyRed
        ? snapshot.redTeam : snapshot.blueTeam;
    const MatchTeamSnapshot &enemy = allyRed
        ? snapshot.blueTeam : snapshot.redTeam;
    return {
        {QStringLiteral("allyBaseHp"), ally.baseHealth},
        {QStringLiteral("allyBaseMax"), ally.baseMaxHealth},
        {QStringLiteral("enemyBaseHp"), enemy.baseHealth},
        {QStringLiteral("enemyBaseMax"), enemy.baseMaxHealth},
        {QStringLiteral("allyOutpostHp"), ally.outpostHealth},
        {QStringLiteral("allyOutpostMax"), ally.outpostMaxHealth},
        {QStringLiteral("enemyOutpostHp"), enemy.outpostHealth},
        {QStringLiteral("enemyOutpostMax"), enemy.outpostMaxHealth},
        {QStringLiteral("economyDiff"),
         ally.economy >= 0 && enemy.economy >= 0
             ? ally.economy - enemy.economy : 0},
        {QStringLiteral("damageDiff"),
         ally.totalDamage >= 0 && enemy.totalDamage >= 0
             ? ally.totalDamage - enemy.totalDamage : 0},
        {QStringLiteral("hpDiff"), 0},
    };
}

QVariantMap MatchUiProjection::topStatusData() const {
    const MatchSnapshot snapshot =
        m_session ? m_session->matchSnapshot() : MatchSnapshot{};
    const MatchTeamSnapshot &ally = operatorRobot().side == MatchSide::Blue
        ? snapshot.blueTeam : snapshot.redTeam;
    return {
        {QStringLiteral("allyRemainingEconomy"),
         qMax(0, ally.economy)},
        {QStringLiteral("allyTotalEconomyObtained"),
         qMax(0, ally.totalEconomyObtained)},
        {QStringLiteral("allyTechLevel"),
         qMax(0, ally.techLevel)},
        {QStringLiteral("allyEncryptionLevel"),
         qMax(0, ally.encryptionLevel)},
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
    const MatchSnapshot &snapshot = m_session->matchSnapshot();
    return robotCards(operatorRobot().side == MatchSide::Blue
                          ? snapshot.blueTeam : snapshot.redTeam);
}

QVariantList MatchUiProjection::enemyRobotList() const {
    if (!m_session) {
        return {};
    }
    const MatchSnapshot &snapshot = m_session->matchSnapshot();
    return robotCards(operatorRobot().side == MatchSide::Blue
                          ? snapshot.redTeam : snapshot.blueTeam);
}

QVariantMap MatchUiProjection::mapData() const {
    if (!m_session) {
        return {};
    }
    const MatchSnapshot &snapshot = m_session->matchSnapshot();
    const bool allyRed = operatorRobot().side == MatchSide::Red;
    return {
        {QStringLiteral("radarAgeMs"), 0},
        {QStringLiteral("allyRobots"),
         positionedRobots(allyRed ? snapshot.redTeam : snapshot.blueTeam, true)},
        {QStringLiteral("enemyRobots"),
         positionedRobots(allyRed ? snapshot.blueTeam : snapshot.redTeam, false)},
        {QStringLiteral("focusTargetId"), QString()},
        {QStringLiteral("mapImageSource"),
         snapshot.profile == QStringLiteral("rmul1v1")
             ? QStringLiteral("qrc:/images/rmul1v1.png")
             : QStringLiteral("qrc:/images/rmul3v3.png")},
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
    const bool available = active();
    const QString health = dataHealth();
    const int ageMs = dataAgeMs();
    const QString linkStatus = !available
        ? QStringLiteral("lost")
        : health == QStringLiteral("fresh") ? QStringLiteral("ok")
        : health == QStringLiteral("stale") ? QStringLiteral("stale")
                                             : QStringLiteral("lost");
    return {
        {QStringLiteral("mqttStatus"),
         m_sourceKind == QStringLiteral("mock")
             ? QStringLiteral("mock") : linkStatus},
        {QStringLiteral("mqttLatencyMs"), ageMs},
        {QStringLiteral("videoStatus"), QStringLiteral("disabled")},
        {QStringLiteral("videoLatencyMs"), -1},
        {QStringLiteral("radarStatus"),
         m_sourceKind == QStringLiteral("mock")
             ? QStringLiteral("ok") : linkStatus},
        {QStringLiteral("radarAgeMs"), ageMs},
        {QStringLiteral("commandStatus"),
         m_sourceKind == QStringLiteral("mock")
             ? QStringLiteral("disabled") : linkStatus},
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
    return !m_session ||
        m_session->matchSnapshot().profile != QStringLiteral("rmul1v1");
}

double MatchUiProjection::fieldWidth() const {
    return hasControlPoints() ? 12.0 : 5.0;
}

double MatchUiProjection::fieldHeight() const {
    return hasControlPoints() ? 8.0 : 5.0;
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

bool MatchUiProjection::isPaused() const {
    return m_session && m_session->matchSnapshot().isPaused;
}

bool MatchUiProjection::uses2027Protocol() const {
    return true;
}

QVariantMap MatchUiProjection::territoryStatus() const {
    if (!m_session) {
        return {};
    }
    const MatchSnapshot &snapshot = m_session->matchSnapshot();
    return {
        {QStringLiteral("redPoints"), snapshot.redTeam.controlPoints},
        {QStringLiteral("bluePoints"), snapshot.blueTeam.controlPoints},
        {QStringLiteral("redControlPoints"), snapshot.redTeam.controlPoints},
        {QStringLiteral("blueControlPoints"), snapshot.blueTeam.controlPoints},
    };
}

QString MatchUiProjection::redTeamName() const {
    return m_session ? m_session->matchSnapshot().redTeam.name
                     : QStringLiteral("红方");
}

QString MatchUiProjection::blueTeamName() const {
    return m_session ? m_session->matchSnapshot().blueTeam.name
                     : QStringLiteral("蓝方");
}

int MatchUiProjection::zeroInt() const {
    return 0;
}

bool MatchUiProjection::falseValue() const {
    return false;
}

MatchRobotSnapshot MatchUiProjection::operatorRobot() const {
    if (!m_session) {
        return {};
    }
    const MatchSnapshot &snapshot = m_session->matchSnapshot();
    const QList<MatchRobotSnapshot> allRobots =
        snapshot.redTeam.robots + snapshot.blueTeam.robots;
    for (const MatchRobotSnapshot &robot : allRobots) {
        if (!snapshot.operatorRobotId.isEmpty() &&
            robot.id == snapshot.operatorRobotId) {
            return robot;
        }
    }
    for (const MatchRobotSnapshot &robot : allRobots) {
        if (robot.telemetryScope == MatchTelemetryScope::Private) {
            return robot;
        }
    }
    return allRobots.isEmpty() ? MatchRobotSnapshot{} : allRobots.first();
}

int MatchUiProjection::currentHealth() const {
    const MatchRobotSnapshot robot = operatorRobot();
    return robot.health >= 0 ? robot.health : 0;
}

int MatchUiProjection::maxHealth() const {
    return qMax(0, operatorRobot().maxHealth);
}

int MatchUiProjection::chassisEnergy() const {
    return qMax(0, qRound(operatorRobot().energy.chassisEnergyJ));
}

int MatchUiProjection::maxChassisEnergy() const {
    return qMax(0, qRound(operatorRobot().energy.chassisEnergyLimitJ));
}

int MatchUiProjection::bufferEnergy() const {
    return qMax(0, qRound(operatorRobot().energy.bufferEnergyJ));
}

int MatchUiProjection::maxBufferEnergy() const {
    return qMax(0, qRound(operatorRobot().energy.bufferEnergyLimitJ));
}

double MatchUiProjection::superCapEnergy() const {
    return operatorRobot().energy.superCapPercent;
}

int MatchUiProjection::robotLevel() const {
    const MatchRobotSnapshot robot = operatorRobot();
    return robot.level > 0 ? robot.level : 1;
}

int MatchUiProjection::maxlevel() const {
    return 10;
}

int MatchUiProjection::robotId() const {
    const auto robot = operatorRobot();
    return robot.slot + (robot.side == MatchSide::Blue ? 100 : 0);
}

int MatchUiProjection::currentExp() const {
    return qMax(0, operatorRobot().experience);
}

int MatchUiProjection::maxExp() const {
    const int maximum = operatorRobot().maxExperience;
    return maximum > 0 ? maximum : 100;
}

int MatchUiProjection::yellowCardCount() const {
    return qMax(0, operatorRobot().yellowCardCount);
}

QString MatchUiProjection::outOfCombatStatus() const {
    const MatchRobotSnapshot robot = operatorRobot();
    if (!robot.online) {
        return QStringLiteral("数据中断");
    }
    return !robot.alive ? QStringLiteral("等待复活")
        : robot.outOfCombat ? QStringLiteral("已脱战")
                            : QStringLiteral("交战中");
}

QString MatchUiProjection::chassisStatus() const {
    const MatchRobotSnapshot robot = operatorRobot();
    if (robot.chassis.overpowerLockRemainingSec > 0) {
        return QStringLiteral("超限断电 %1s")
            .arg(robot.chassis.overpowerLockRemainingSec);
    }
    QStringList parts;
    if (robot.chassis.mode != MatchChassisMode::Unknown) {
        parts.append(matchChassisModeLabel(robot.chassis.mode));
    }
    if (robot.energy.chassisPowerW >= 0.0) {
        parts.append(
            QStringLiteral("%1 W").arg(qRound(robot.energy.chassisPowerW)));
    }
    if (robot.wheelLeg.telemetryAvailable) {
        parts.append(matchWheelLegModeLabel(robot.wheelLeg.mode));
    }
    return parts.join(QStringLiteral(" · "));
}

bool MatchUiProjection::isOutOfCombat() const {
    const MatchRobotSnapshot robot = operatorRobot();
    return robot.online && robot.alive && robot.outOfCombat;
}

QVariantList MatchUiProjection::moduleStates() const {
    const MatchRobotSnapshot robot = operatorRobot();
    if (!robot.moduleStates.isEmpty()) {
        return robot.moduleStates;
    }
    const int state = operatorRobot().online ? 0 : 2;
    QVariantList states;
    for (int index = 0; index < 10; ++index) {
        states.append(state);
    }
    return states;
}

QVariantList MatchUiProjection::moduleData() const {
    const MatchRobotSnapshot robot = operatorRobot();
    if (!robot.moduleData.isEmpty()) {
        return robot.moduleData;
    }
    const bool online = operatorRobot().online;
    QVariantList modules;
    for (int index = 0; index < 10; ++index) {
        modules.append(QVariantMap{
            {QStringLiteral("online"), online},
            {QStringLiteral("state"), online ? 0 : 2},
        });
    }
    return modules;
}

QVariantMap MatchUiProjection::buffTimedData() const {
    return operatorRobot().buffTimedData;
}

QVariantMap MatchUiProjection::myRobot() const {
    const MatchRobotSnapshot robot = operatorRobot();
    int robotType = 3;
    if (robot.role == MatchRobotRole::Heavy)
        robotType = 1;
    else if (robot.role == MatchRobotRole::Engineer)
        robotType = 2;
    else if (robot.role == MatchRobotRole::Aerial)
        robotType = 6;
    return {
        {QStringLiteral("type"), robotType},
        {QStringLiteral("isClientConnected"), robot.online},
        {QStringLiteral("isOutOfCombat"),
         robot.online && robot.alive && robot.outOfCombat},
        {QStringLiteral("isRedTeam"), robot.side == MatchSide::Red},
        {QStringLiteral("ammo"), robot.ammo},
        {QStringLiteral("chassisPowerW"), robot.energy.chassisPowerW},
        {QStringLiteral("currentHeat"), qMax(0, robot.heat)},
        {QStringLiteral("heatLimit"),
         robot.heatLimit > 0 ? robot.heatLimit : 240},
        {QStringLiteral("isHeatOverLimit"),
         robot.heat >= 0 && robot.heatLimit > 0
             && robot.heat >= robot.heatLimit},
        {QStringLiteral("allowedAmmo17mm"),
         robot.role == MatchRobotRole::Heavy ? 0 : qMax(0, robot.ammo)},
        {QStringLiteral("allowedAmmo42mm"),
         robot.role == MatchRobotRole::Heavy ? qMax(0, robot.ammo) : 0},
        {QStringLiteral("muzzleVelocity"),
         robot.muzzleVelocityMps >= 0.0 ? robot.muzzleVelocityMps : 0.0},
        {QStringLiteral("shootSpeedLimit"),
         robot.shootSpeedLimitMps >= 0.0 ? robot.shootSpeedLimitMps : 0.0},
        {QStringLiteral("speedLockState"), robot.speedLockState},
        {QStringLiteral("speedLockSeconds"), robot.speedLockSeconds},
        {QStringLiteral("superCapEnergy"),
         robot.energy.superCapPercent},
        {QStringLiteral("robotLevel"), qMax(0, robot.level)},
        {QStringLiteral("yellowCardCount"), robot.yellowCardCount},
        {QStringLiteral("chassisMode"),
         matchChassisModeLabel(robot.chassis.mode)},
        {QStringLiteral("driveType"),
         matchChassisDriveTypeLabel(robot.chassis.driveType)},
        {QStringLiteral("wheelLegMode"),
         matchWheelLegModeLabel(robot.wheelLeg.mode)},
    };
}

QVariantMap MatchUiProjection::myRobotCustomData() const {
    const MatchRobotSnapshot robot = operatorRobot();
    if (!robot.customData.isEmpty()) {
        QVariantMap data = robot.customData;
        if (!data.contains(QStringLiteral("superCapEnergy"))) {
            data.insert(QStringLiteral("superCapEnergy"),
                        robot.energy.superCapPercent);
        }
        return data;
    }
    int chassisMode = 0;
    switch (robot.chassis.mode) {
    case MatchChassisMode::Follow: chassisMode = 1; break;
    case MatchChassisMode::Spin: chassisMode = 2; break;
    case MatchChassisMode::Protect: chassisMode = 3; break;
    default: chassisMode = 0; break;
    }
    return {
        {QStringLiteral("fricEnabled"), robot.online && robot.alive},
        {QStringLiteral("rammerEnabled"), robot.online && robot.alive},
        {QStringLiteral("chassisMode"), chassisMode},
        {QStringLiteral("spinMode"), chassisMode == 2},
        {QStringLiteral("followMode"), chassisMode == 1},
        {QStringLiteral("chassisProtect"), chassisMode == 3},
        {QStringLiteral("chassisWarning"), false},
        {QStringLiteral("superCapEnergy"), robot.energy.superCapPercent},
        {QStringLiteral("gimbalChassisAngle"), 0.0},
        {QStringLiteral("targetDistance"), 0},
        {QStringLiteral("ballisticCompensation"), 0},
    };
}

} // namespace RM
