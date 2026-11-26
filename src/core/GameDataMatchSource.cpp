#include "core/GameDataMatchSource.h"

#include "core/GameData.h"

#include <QDateTime>

#include <climits>

namespace RM {
namespace {

MatchRobotRole roleForSlot(int slot) {
    return slot == 1 ? MatchRobotRole::Heavy : MatchRobotRole::Infantry;
}

MatchRobotRole roleForType(RobotType type, int slot) {
    switch (type) {
    case RobotType::HERO: return MatchRobotRole::Heavy;
    case RobotType::ENGINEER: return MatchRobotRole::Engineer;
    case RobotType::AERIAL: return MatchRobotRole::Aerial;
    case RobotType::SENTRY: return MatchRobotRole::Sentry;
    case RobotType::INFANTRY_3:
    case RobotType::INFANTRY_4:
    case RobotType::INFANTRY_5:
        return MatchRobotRole::Infantry;
    default:
        return roleForSlot(slot);
    }
}

} // namespace

GameDataMatchSource::GameDataMatchSource(::GameData *gameData, QObject *parent)
    : QObject(parent),
      m_gameData(gameData),
      m_session(new MatchSession(this)) {
    m_lastDataAtMs = QDateTime::currentMSecsSinceEpoch();
    m_timer.setInterval(100);
    connect(&m_timer, &QTimer::timeout, this, &GameDataMatchSource::refresh);
    if (m_gameData) {
        const auto markDataUpdated = [this]() {
            m_lastDataAtMs = QDateTime::currentMSecsSinceEpoch();
            refresh();
        };
        connect(m_gameData,
                &GameData::matchProfileChanged,
                this,
                markDataUpdated);
        connect(m_gameData,
                &GameData::gameStateUpdated,
                this,
                markDataUpdated);
        connect(m_gameData,
                &GameData::myRobotUpdated,
                this,
                markDataUpdated);
        connect(m_gameData,
                &GameData::robotDataUpdated,
                this,
                [markDataUpdated](quint8) { markDataUpdated(); });
        connect(m_gameData,
                &GameData::robotPositionUpdated,
                this,
                [markDataUpdated](quint8, float, float, float, quint8) {
                    markDataUpdated();
                });
        connect(m_gameData,
                &GameData::moduleListChanged,
                this,
                markDataUpdated);
        connect(m_gameData,
                &GameData::robotCustomDataUpdated,
                this,
                [markDataUpdated](quint8) { markDataUpdated(); });
        connect(m_gameData,
                &GameData::dataChanged,
                this,
                markDataUpdated);
        connect(m_gameData,
                &GameData::killEventOccurred,
                this,
                [this](const KillRecord &record) {
                    appendEvent(
                        QStringLiteral("kill"),
                        QStringLiteral("机器人 %1 击毁机器人 %2")
                            .arg(record.killerId)
                            .arg(record.victimId),
                        record.killerId > 0,
                        record.killerId >= 100 ? MatchSide::Blue
                                               : MatchSide::Red,
                        record.victimId % 100);
                });
        connect(m_gameData,
                &GameData::systemMessageReceived,
                this,
                [this](const QString &message) {
                    appendEvent(QStringLiteral("system"), message);
                });
        connect(m_gameData,
                &GameData::officialEventPopupRequested,
                this,
                [this](int eventId, const QString &message) {
                    appendEvent(
                        QStringLiteral("event_%1").arg(eventId), message);
                });
        connect(m_gameData,
                &GameData::baseUnderAttackEvent,
                this,
                [this](bool isRedTeam) {
                    appendEvent(
                        QStringLiteral("base_under_attack"),
                        QStringLiteral("%1基地遭到攻击")
                            .arg(isRedTeam ? QStringLiteral("红方")
                                           : QStringLiteral("蓝方")),
                        true,
                        isRedTeam ? MatchSide::Red : MatchSide::Blue);
                });
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

MatchSnapshot GameDataMatchSource::buildSnapshot(QString *error) {
    MatchSnapshot snapshot;
    snapshot.matchId = QStringLiteral("real-game-data");
    snapshot.profile = QStringLiteral("rmul3v3");
    snapshot.phase = mapPhase(m_gameData->getCurrentStage());
    snapshot.isPaused = m_gameData->is_paused();
    snapshot.timeRemainingSec = m_gameData->getGameTime();
    snapshot.round = qMax(1, m_gameData->currentRound());
    const int currentRobotId = m_gameData->currentRobotId();
    snapshot.operatorRobotId =
        QStringLiteral("%1-%2")
            .arg(currentRobotId >= 100 ? QStringLiteral("blue")
                                       : QStringLiteral("red"))
            .arg(currentRobotId % 100);
    snapshot.redTeam.name = m_gameData->redTeamName();
    snapshot.redTeam.score = m_gameData->redScore();
    snapshot.blueTeam.name = m_gameData->blueTeamName();
    snapshot.blueTeam.score = m_gameData->blueScore();
    snapshot.redTeam.economy = m_gameData->redEconomy();
    snapshot.blueTeam.economy = m_gameData->blueEconomy();
    snapshot.redTeam.totalEconomyObtained =
        qMin<qint64>(m_gameData->redTotalEconomyObtained(), INT_MAX);
    snapshot.blueTeam.totalEconomyObtained =
        qMin<qint64>(m_gameData->blueTotalEconomyObtained(), INT_MAX);
    snapshot.redTeam.techLevel = m_gameData->redTechLevel();
    snapshot.blueTeam.techLevel = m_gameData->blueTechLevel();
    snapshot.redTeam.encryptionLevel = m_gameData->redEncryptionLevel();
    snapshot.blueTeam.encryptionLevel = m_gameData->blueEncryptionLevel();
    snapshot.redTeam.totalDamage = m_gameData->redTotalDamage();
    snapshot.blueTeam.totalDamage = m_gameData->blueTotalDamage();

    const QVariantMap territory = m_gameData->territoryStatus();
    if (territory.contains(QStringLiteral("redPoints"))) {
        snapshot.redTeam.controlPoints =
            territory.value(QStringLiteral("redPoints")).toInt();
    }
    if (territory.contains(QStringLiteral("bluePoints"))) {
        snapshot.blueTeam.controlPoints =
            territory.value(QStringLiteral("bluePoints")).toInt();
    }
    if (m_gameData->hasBases()) {
        const BaseData &redBase = m_gameData->getRedBase();
        const BaseData &blueBase = m_gameData->getBlueBase();
        snapshot.redTeam.baseHealth = redBase.currentHP;
        snapshot.redTeam.baseMaxHealth = redBase.maxHP;
        snapshot.redTeam.baseStatus = redBase.status;
        snapshot.blueTeam.baseHealth = blueBase.currentHP;
        snapshot.blueTeam.baseMaxHealth = blueBase.maxHP;
        snapshot.blueTeam.baseStatus = blueBase.status;
        const OutpostData &redOutpost = m_gameData->getRedOutpost();
        const OutpostData &blueOutpost = m_gameData->getBlueOutpost();
        snapshot.redTeam.outpostHealth = redOutpost.currentHP;
        snapshot.redTeam.outpostMaxHealth = redOutpost.maxHP;
        snapshot.redTeam.outpostStatus = redOutpost.status;
        snapshot.blueTeam.outpostHealth = blueOutpost.currentHP;
        snapshot.blueTeam.outpostMaxHealth = blueOutpost.maxHP;
        snapshot.blueTeam.outpostStatus = blueOutpost.status;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    snapshot.updatedAtMs =
        m_lastDataAtMs > 0 ? m_lastDataAtMs : now;

    const QList<int> slotIds{1, 3, 4};
    const auto buildRobot = [&](bool red, int slot) {
        const int id = red ? slot : 100 + slot;
        MatchRobotSnapshot robot;
        robot.id = QStringLiteral("%1-%2")
                       .arg(red ? QStringLiteral("red") : QStringLiteral("blue"))
                       .arg(slot);
        robot.slot = slot;
        robot.side = red ? MatchSide::Red : MatchSide::Blue;
        robot.telemetryScope = id == currentRobotId
            ? MatchTelemetryScope::Private
            : MatchTelemetryScope::Public;
        const RobotData *data = m_gameData->getRobotById(static_cast<quint8>(id));
        if (!data) {
            robot.role = roleForSlot(slot);
            robot.telemetryScope = MatchTelemetryScope::Public;
            return robot;
        }
        robot.role = roleForType(data->type, slot);
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
        robot.level = data->level;
        robot.experience = data->experience;
        robot.maxExperience = data->maxExperience;
        robot.yellowCardCount = data->yellowCardCount;
        robot.muzzleVelocityMps = data->muzzleVelocity;
        robot.shootSpeedLimitMps = data->shootSpeedLimit;
        robot.speedLockState = static_cast<int>(data->speedLockState);
        robot.speedLockSeconds = data->speedLockSeconds;
        robot.outOfCombat = data->isOutOfCombat;
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
            robot.moduleStates = m_gameData->moduleStates();
            robot.moduleData = m_gameData->getModuleData();
            robot.buffTimedData = m_gameData->buffTimedData();
            robot.customData = m_gameData->getMyRobotCustomData();
            robot.wheelLeg.mode = MatchWheelLegMode::Unknown;
        }
        return robot;
    };

    for (const int slot : slotIds)
        snapshot.redTeam.robots.append(buildRobot(true, slot));
    for (const int slot : slotIds)
        snapshot.blueTeam.robots.append(buildRobot(false, slot));
    snapshot.events = m_events;

    const QList<MatchValidationIssue> issues = validateMatchSnapshot(snapshot);
    if (hasMatchValidationError(issues)) {
        if (error)
            *error = matchValidationErrorMessage(issues);
        return {};
    }
    return snapshot;
}

void GameDataMatchSource::appendEvent(const QString &type,
                                      const QString &message,
                                      bool hasSide,
                                      MatchSide side,
                                      int robotSlot) {
    if (message.trimmed().isEmpty())
        return;
    m_events.append(MatchEventSnapshot{
        m_nextEventSequence++,
        QDateTime::currentMSecsSinceEpoch(),
        type,
        message,
        hasSide,
        side,
        robotSlot,
    });
    while (m_events.size() > 64)
        m_events.removeFirst();
    refresh();
}

} // namespace RM
