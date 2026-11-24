#include "core/GameDataMatchSource.h"

#include "core/GameData.h"

#include <QDateTime>

namespace RM {
namespace {

MatchRobotRole roleForSlot(int slot) {
    return slot == 1 ? MatchRobotRole::Heavy : MatchRobotRole::Infantry;
}

} // namespace

GameDataMatchSource::GameDataMatchSource(::GameData *gameData, QObject *parent)
    : QObject(parent),
      m_gameData(gameData),
      m_session(new MatchSession(this)) {
    m_timer.setInterval(100);
    connect(&m_timer, &QTimer::timeout, this, &GameDataMatchSource::refresh);
    if (m_gameData) {
        connect(m_gameData,
                &GameData::matchProfileChanged,
                this,
                &GameDataMatchSource::refresh);
        connect(m_gameData,
                &GameData::gameStateUpdated,
                this,
                &GameDataMatchSource::refresh);
        connect(m_gameData,
                &GameData::myRobotUpdated,
                this,
                &GameDataMatchSource::refresh);
    }
    m_timer.start();
    refresh();
}

void GameDataMatchSource::refresh() {
    if (!m_gameData) {
        m_lastError = QStringLiteral("GameData 不可用");
        if (m_session->isValid())
            m_session->clear();
        emit stateChanged();
        return;
    }
    if (m_gameData->matchProfile() != MatchProfile::Rmul3v3) {
        m_lastError = QStringLiteral("等待 RMUL 3v3 比赛配置");
        if (m_session->isValid())
            m_session->clear();
        emit stateChanged();
        return;
    }

    QString error;
    const MatchSnapshot snapshot = buildSnapshot(&error);
    if (!error.isEmpty()) {
        m_lastError = error;
        emit stateChanged();
        return;
    }
    if (!m_session->replaceSnapshot(snapshot, &error)) {
        m_lastError = error;
        emit stateChanged();
        return;
    }
    m_lastError.clear();
    emit stateChanged();
}

MatchSession *GameDataMatchSource::matchSession() const {
    return m_session;
}

QString GameDataMatchSource::sourceKind() const {
    return QStringLiteral("real");
}

QString GameDataMatchSource::sourceName() const {
    return QStringLiteral("GameData 真实比赛数据");
}

bool GameDataMatchSource::isAvailable() const {
    return m_session && m_session->isValid();
}

QString GameDataMatchSource::statusText() const {
    if (isAvailable())
        return QStringLiteral("真实比赛数据已接入");
    return m_lastError.isEmpty()
        ? QStringLiteral("等待真实比赛数据")
        : m_lastError;
}

MatchPhase GameDataMatchSource::mapPhase(::GameStage stage) const {
    switch (stage) {
    case GameStage::NOT_STARTED: return MatchPhase::NotStarted;
    case GameStage::PREPARATION: return MatchPhase::Preparation;
    case GameStage::SELF_CHECK: return MatchPhase::SelfCheck;
    case GameStage::COUNTDOWN: return MatchPhase::Countdown;
    case GameStage::BATTLE: return MatchPhase::Battle;
    case GameStage::SETTLEMENT: return MatchPhase::Settlement;
    }
    return MatchPhase::NotStarted;
}

MatchSnapshot GameDataMatchSource::buildSnapshot(QString *error) const {
    MatchSnapshot snapshot;
    snapshot.matchId = QStringLiteral("real-game-data");
    snapshot.profile = QStringLiteral("rmul3v3");
    snapshot.phase = mapPhase(m_gameData->getCurrentStage());
    snapshot.timeRemainingSec = m_gameData->getGameTime();
    snapshot.round = qMax(1, m_gameData->currentRound());
    snapshot.redTeam.name = m_gameData->redTeamName();
    snapshot.redTeam.score = m_gameData->redScore();
    snapshot.blueTeam.name = m_gameData->blueTeamName();
    snapshot.blueTeam.score = m_gameData->blueScore();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    snapshot.updatedAtMs = now;

    const QList<int> slotIds{1, 3, 4};
    const auto buildRobot = [&](bool red, int slot) {
        const int id = red ? slot : 100 + slot;
        MatchRobotSnapshot robot;
        robot.id = QStringLiteral("%1-%2")
                       .arg(red ? QStringLiteral("red") : QStringLiteral("blue"))
                       .arg(slot);
        robot.slot = slot;
        robot.side = red ? MatchSide::Red : MatchSide::Blue;
        robot.telemetryScope = id == m_gameData->currentRobotId()
            ? MatchTelemetryScope::Private
            : MatchTelemetryScope::Public;
        const RobotData *data = m_gameData->getRobotById(static_cast<quint8>(id));
        if (!data) {
            robot.role = roleForSlot(slot);
            robot.telemetryScope = MatchTelemetryScope::Public;
            return robot;
        }
        robot.role = roleForSlot(slot);
        robot.health = data->hasTabGlobalSnapshot
            ? data->tabGlobalCurrentHP
            : data->currentHP;
        const int maxHealth =
            qMax<int>(data->currentRoundMaxHP, data->maxHP);
        robot.maxHealth =
            maxHealth > 0 ? maxHealth : kUnknownMatchMetric;
        robot.ammo = data->allowedAmmo17mm + data->allowedAmmo42mm;
        robot.heat = data->currentHeat;
        robot.heatLimit = data->heatLimit > 0
            ? data->heatLimit
            : kUnknownMatchMetric;
        robot.online = data->tabStaticConnected || data->isClientConnected ||
            data->status != RobotStatus::OFFLINE;
        robot.alive = robot.online && robot.health > 0 &&
            data->status != RobotStatus::DESTROYED;
        robot.respawnRemainingSec = 0;
        robot.updatedAtMs = data->lastUpdateTime.isValid()
            ? data->lastUpdateTime.toMSecsSinceEpoch()
            : now;
        if (data->lastPositionTime.isValid()) {
            robot.hasPosition = true;
            robot.mapX = qBound(0.0, static_cast<double>(data->posX) /
                                        m_gameData->fieldWidth(), 1.0);
            robot.mapY = qBound(0.0, static_cast<double>(data->posY) /
                                        m_gameData->fieldHeight(), 1.0);
            robot.headingDegrees = data->angle;
        }
        if (robot.telemetryScope == MatchTelemetryScope::Private) {
            robot.energy.superCapPercent = qMax(0.0f, data->superCapEnergyPercent);
            robot.energy.bufferEnergyJ = data->bufferEnergy;
            robot.energy.bufferEnergyLimitJ = data->maxBufferEnergy > 0
                ? data->maxBufferEnergy
                : kUnknownMatchReal;
            robot.energy.chassisEnergyJ = data->currentChassisEnergy;
            robot.energy.chassisEnergyLimitJ = data->maxChassisEnergy > 0
                ? data->maxChassisEnergy
                : kUnknownMatchReal;
            robot.energy.chassisPowerW = data->power;
            robot.energy.chassisPowerLimitW = data->maxPower > 0
                ? data->maxPower
                : kUnknownMatchReal;
            robot.chassis.enabled = data->chassisEnabled;
            robot.chassis.enabledReported = true;
            robot.chassis.overpowerLockRemainingSec =
                data->chassisOverPowerCutSeconds;
            const int mode = data->chassisMode;
            robot.chassis.mode = mode == 1 ? MatchChassisMode::Follow
                : mode == 2 ? MatchChassisMode::Spin
                : mode == 3 ? MatchChassisMode::Protect
                : MatchChassisMode::Normal;
            robot.wheelLeg.mode = MatchWheelLegMode::Unknown;
        }
        return robot;
    };

    for (const int slot : slotIds)
        snapshot.redTeam.robots.append(buildRobot(true, slot));
    for (const int slot : slotIds)
        snapshot.blueTeam.robots.append(buildRobot(false, slot));

    const QList<MatchValidationIssue> issues = validateMatchSnapshot(snapshot);
    if (hasMatchValidationError(issues)) {
        if (error)
            *error = matchValidationErrorMessage(issues);
        return {};
    }
    return snapshot;
}

} // namespace RM
