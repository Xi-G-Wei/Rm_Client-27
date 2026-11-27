#include "core/MatchSession.h"

#include <algorithm>
#include <cmath>

#include <QSet>
#include <QStringList>

namespace RM {
namespace {

void addIssue(QList<MatchValidationIssue> *issues,
              MatchValidationSeverity severity,
              const QString &path,
              const QString &message) {
    issues->append({severity, path, message});
}

void validateMetric(QList<MatchValidationIssue> *issues,
                    const QString &path,
                    int value,
                    bool allowZero) {
    if (value == kUnknownMatchMetric) {
        return;
    }
    if (value < 0 || (!allowZero && value == 0)) {
        addIssue(issues,
                 MatchValidationSeverity::Error,
                 path,
                 allowZero ? QStringLiteral("不能小于 -1")
                           : QStringLiteral("必须是正整数或 -1"));
    }
}

void validateRealMetric(QList<MatchValidationIssue> *issues,
                        const QString &path,
                        double value,
                        bool allowZero,
                        double maximum = -1.0) {
    if (value == kUnknownMatchReal) {
        return;
    }
    if (!std::isfinite(value) || value < 0.0 ||
        (!allowZero && value == 0.0) ||
        (maximum >= 0.0 && value > maximum)) {
        QString message = allowZero ? QStringLiteral("不能小于 -1")
                                    : QStringLiteral("必须是正数或 -1");
        if (maximum >= 0.0) {
            message += QStringLiteral("，且不能大于 %1").arg(maximum);
        }
        addIssue(issues,
                 MatchValidationSeverity::Error,
                 path,
                 message);
    }
}

void validateSignedRealMetric(QList<MatchValidationIssue> *issues,
                              const QString &path,
                              double value,
                              double minimum,
                              double maximum) {
    if (value == kUnknownMatchReal) {
        return;
    }
    if (!std::isfinite(value) || value < minimum || value > maximum) {
        addIssue(issues,
                 MatchValidationSeverity::Error,
                 path,
                 QStringLiteral("必须是 %1 到 %2 之间的有限数值或 -1")
                     .arg(minimum)
                     .arg(maximum));
    }
}

void validateEnergy(QList<MatchValidationIssue> *issues,
                    const MatchEnergySnapshot &energy,
                    const QString &robotPath) {
    const QString path = robotPath + QStringLiteral(".energy.");
    validateRealMetric(issues,
                       path + QStringLiteral("superCapPercent"),
                       energy.superCapPercent,
                       true,
                       100.0);
    validateRealMetric(issues,
                       path + QStringLiteral("bufferEnergyJ"),
                       energy.bufferEnergyJ,
                       true);
    validateRealMetric(issues,
                       path + QStringLiteral("bufferEnergyLimitJ"),
                       energy.bufferEnergyLimitJ,
                       false);
    validateRealMetric(issues,
                       path + QStringLiteral("chassisEnergyJ"),
                       energy.chassisEnergyJ,
                       true);
    validateRealMetric(issues,
                       path + QStringLiteral("chassisEnergyLimitJ"),
                       energy.chassisEnergyLimitJ,
                       false);
    validateRealMetric(issues,
                       path + QStringLiteral("chassisPowerW"),
                       energy.chassisPowerW,
                       true);
    validateRealMetric(issues,
                       path + QStringLiteral("chassisPowerLimitW"),
                       energy.chassisPowerLimitW,
                       false);

    if (energy.bufferEnergyJ >= 0.0 &&
        energy.bufferEnergyLimitJ > 0.0 &&
        energy.bufferEnergyJ > energy.bufferEnergyLimitJ) {
        addIssue(issues,
                 MatchValidationSeverity::Error,
                 path + QStringLiteral("bufferEnergyJ"),
                 QStringLiteral("缓冲能量不能大于缓冲能量上限"));
    }
    if (energy.chassisEnergyJ >= 0.0 &&
        energy.chassisEnergyLimitJ > 0.0 &&
        energy.chassisEnergyJ > energy.chassisEnergyLimitJ) {
        addIssue(issues,
                 MatchValidationSeverity::Error,
                 path + QStringLiteral("chassisEnergyJ"),
                 QStringLiteral("底盘能量不能大于底盘能量上限"));
    }
}

void validateChassis(QList<MatchValidationIssue> *issues,
                     const MatchChassisSnapshot &chassis,
                     const QString &robotPath) {
    const QString path = robotPath + QStringLiteral(".chassis.");
    if (chassis.overpowerLockRemainingSec < kUnknownMatchMetric) {
        addIssue(issues,
                 MatchValidationSeverity::Error,
                 path + QStringLiteral("overpowerLockRemainingSec"),
                 QStringLiteral("超功率断电时间不能小于 -1"));
    }
}

void validateWheelLeg(QList<MatchValidationIssue> *issues,
                      const MatchWheelLegSnapshot &wheelLeg,
                      const QString &robotPath) {
    const QString path = robotPath + QStringLiteral(".wheelLeg.");
    validateRealMetric(issues,
                       path + QStringLiteral("stanceHeightMm"),
                       wheelLeg.stanceHeightMm,
                       false);
    validateSignedRealMetric(issues,
                             path + QStringLiteral("pitchDegrees"),
                             wheelLeg.pitchDegrees,
                             -180.0,
                             180.0);
    validateSignedRealMetric(issues,
                             path + QStringLiteral("rollDegrees"),
                             wheelLeg.rollDegrees,
                             -180.0,
                             180.0);

    const auto validateWheelSpeed = [&](const QString &name, double value) {
        if (value == kUnknownMatchReal) {
            return;
        }
        if (!std::isfinite(value)) {
            addIssue(issues,
                     MatchValidationSeverity::Error,
                     path + name,
                     QStringLiteral("轮速必须是有限数值或 -1"));
        }
    };
    validateWheelSpeed(QStringLiteral("leftWheelSpeedMps"),
                       wheelLeg.leftWheelSpeedMps);
    validateWheelSpeed(QStringLiteral("rightWheelSpeedMps"),
                       wheelLeg.rightWheelSpeedMps);

    if (!wheelLeg.joints.isEmpty() && !wheelLeg.telemetryAvailable) {
        addIssue(issues,
                 MatchValidationSeverity::Error,
                 path + QStringLiteral("telemetryAvailable"),
                 QStringLiteral("存在关节数据时必须标记轮腿遥测可用"));
    }

    QSet<QString> jointNames;
    for (int index = 0; index < wheelLeg.joints.size(); ++index) {
        const MatchWheelLegJointSnapshot &joint =
            wheelLeg.joints.at(index);
        const QString jointPath =
            path + QStringLiteral("joints[%1].").arg(index);
        const QString normalizedName = joint.name.trimmed().toLower();
        if (normalizedName.isEmpty()) {
            addIssue(issues,
                     MatchValidationSeverity::Error,
                     jointPath + QStringLiteral("name"),
                     QStringLiteral("轮腿关节名称不能为空"));
        } else if (jointNames.contains(normalizedName)) {
            addIssue(issues,
                     MatchValidationSeverity::Error,
                     jointPath + QStringLiteral("name"),
                     QStringLiteral("轮腿关节名称不能重复"));
        }
        jointNames.insert(normalizedName);

        const auto validateAngle = [&](const QString &field, double value) {
            if (value == kUnknownMatchReal) {
                return;
            }
            if (!std::isfinite(value) || value < -180.0 || value > 180.0) {
                addIssue(issues,
                         MatchValidationSeverity::Error,
                         jointPath + field,
                         QStringLiteral("关节角度必须是 -180 到 180 度或 -1"));
            }
        };
        validateAngle(QStringLiteral("angleDegrees"),
                      joint.angleDegrees);
        validateAngle(QStringLiteral("targetAngleDegrees"),
                      joint.targetAngleDegrees);
    }
}

void validateRobot(QList<MatchValidationIssue> *issues,
                   const MatchRobotSnapshot &robot,
                   MatchSide expectedSide,
                   const QString &teamPath) {
    const QString path =
        QStringLiteral("%1.robots[%2]").arg(teamPath).arg(robot.slot);

    if (robot.id.trimmed().isEmpty()) {
        addIssue(issues,
                 MatchValidationSeverity::Error,
                 path + QStringLiteral(".id"),
                 QStringLiteral("机器人 ID 不能为空"));
    }
    if (robot.slot <= 0) {
        addIssue(issues,
                 MatchValidationSeverity::Error,
                 path + QStringLiteral(".slot"),
                 QStringLiteral("机器人车位必须为正数"));
    }
    if (robot.side != expectedSide) {
        addIssue(issues,
                 MatchValidationSeverity::Error,
                 path + QStringLiteral(".side"),
                 QStringLiteral("机器人阵营与所在队伍不一致"));
    }

    validateMetric(issues,
                   path + QStringLiteral(".health"),
                   robot.health,
                   true);
    validateMetric(issues,
                   path + QStringLiteral(".maxHealth"),
                   robot.maxHealth,
                   false);
    validateMetric(issues,
                   path + QStringLiteral(".ammo"),
                   robot.ammo,
                   true);
    validateMetric(issues,
                   path + QStringLiteral(".heat"),
                   robot.heat,
                   true);
    validateMetric(issues,
                   path + QStringLiteral(".heatLimit"),
                   robot.heatLimit,
                   false);
    validateEnergy(issues, robot.energy, path);
    validateChassis(issues, robot.chassis, path);
    validateWheelLeg(issues, robot.wheelLeg, path);

    if (robot.health >= 0 && robot.maxHealth > 0 &&
        robot.health > robot.maxHealth) {
        addIssue(issues,
                 MatchValidationSeverity::Error,
                 path + QStringLiteral(".health"),
                 QStringLiteral("当前血量不能大于最大血量"));
    }
    if (robot.alive && robot.health == 0) {
        addIssue(issues,
                 MatchValidationSeverity::Error,
                 path + QStringLiteral(".alive"),
                 QStringLiteral("血量为 0 的机器人不能标记为存活"));
    }
    if (robot.respawnRemainingSec < kUnknownMatchMetric) {
        addIssue(issues,
                 MatchValidationSeverity::Error,
                 path + QStringLiteral(".respawnRemainingSec"),
                 QStringLiteral("复活剩余时间不能小于 -1"));
    }
    if (robot.updatedAtMs < 0) {
        addIssue(issues,
                 MatchValidationSeverity::Error,
                 path + QStringLiteral(".updatedAtMs"),
                 QStringLiteral("更新时间戳不能为负数"));
    }
    if (robot.hasPosition) {
        const bool finite = std::isfinite(robot.mapX) &&
                            std::isfinite(robot.mapY) &&
                            std::isfinite(robot.headingDegrees);
        const bool normalized =
            robot.mapX >= 0.0 && robot.mapX <= 1.0 &&
            robot.mapY >= 0.0 && robot.mapY <= 1.0;
        if (!finite || !normalized) {
            addIssue(issues,
                     MatchValidationSeverity::Error,
                     path + QStringLiteral(".position"),
                     QStringLiteral("地图坐标必须是 0 到 1 之间的有限数值"));
        }
    }
}

void validateTeam(QList<MatchValidationIssue> *issues,
                  const MatchTeamSnapshot &team,
                  MatchSide side,
                  const QString &path,
                  int expectedRobots) {
    if (team.name.trimmed().isEmpty()) {
        addIssue(issues,
                 MatchValidationSeverity::Warning,
                 path + QStringLiteral(".name"),
                 QStringLiteral("队伍名称为空，界面将使用默认名称"));
    }
    if (team.robots.size() != expectedRobots) {
        addIssue(issues,
                 MatchValidationSeverity::Error,
                 path + QStringLiteral(".robots"),
                 QStringLiteral("当前赛项每队必须恰好包含 %1 台机器人")
                     .arg(expectedRobots));
    }
    validateMetric(issues, path + QStringLiteral(".economy"),
                   team.economy, true);
    validateMetric(issues, path + QStringLiteral(".totalEconomyObtained"),
                   team.totalEconomyObtained, true);
    validateMetric(issues, path + QStringLiteral(".techLevel"),
                   team.techLevel, true);
    validateMetric(issues, path + QStringLiteral(".encryptionLevel"),
                   team.encryptionLevel, true);
    validateMetric(issues, path + QStringLiteral(".baseHealth"),
                   team.baseHealth, true);
    validateMetric(issues, path + QStringLiteral(".baseMaxHealth"),
                   team.baseMaxHealth, false);
    validateMetric(issues, path + QStringLiteral(".baseStatus"),
                   team.baseStatus, true);
    validateMetric(issues, path + QStringLiteral(".outpostHealth"),
                   team.outpostHealth, true);
    validateMetric(issues, path + QStringLiteral(".outpostMaxHealth"),
                   team.outpostMaxHealth, false);
    validateMetric(issues, path + QStringLiteral(".outpostStatus"),
                   team.outpostStatus, true);
    validateMetric(issues, path + QStringLiteral(".controlPoints"),
                   team.controlPoints, true);
    validateMetric(issues, path + QStringLiteral(".totalDamage"),
                   team.totalDamage, true);

    QSet<int> usedSlots;
    for (const MatchRobotSnapshot &robot : team.robots) {
        validateRobot(issues, robot, side, path);
        if (usedSlots.contains(robot.slot)) {
            addIssue(issues,
                     MatchValidationSeverity::Error,
                     path + QStringLiteral(".robots"),
                     QStringLiteral("同一队伍存在重复车位 %1").arg(robot.slot));
        }
        usedSlots.insert(robot.slot);
    }
}

} // namespace

QString matchPhaseKey(MatchPhase phase) {
    switch (phase) {
    case MatchPhase::NotStarted: return QStringLiteral("not_started");
    case MatchPhase::Preparation: return QStringLiteral("preparation");
    case MatchPhase::SelfCheck: return QStringLiteral("self_check");
    case MatchPhase::Countdown: return QStringLiteral("countdown");
    case MatchPhase::Battle: return QStringLiteral("battle");
    case MatchPhase::Settlement: return QStringLiteral("settlement");
    }
    return QStringLiteral("unknown");
}

QString matchPhaseLabel(MatchPhase phase) {
    switch (phase) {
    case MatchPhase::NotStarted: return QStringLiteral("未开始");
    case MatchPhase::Preparation: return QStringLiteral("准备阶段");
    case MatchPhase::SelfCheck: return QStringLiteral("裁判系统自检");
    case MatchPhase::Countdown: return QStringLiteral("5 秒倒计时");
    case MatchPhase::Battle: return QStringLiteral("比赛中");
    case MatchPhase::Settlement: return QStringLiteral("结算中");
    }
    return QStringLiteral("未知阶段");
}

QString matchSideKey(MatchSide side) {
    return side == MatchSide::Red ? QStringLiteral("red")
                                  : QStringLiteral("blue");
}

QString matchRobotRoleKey(MatchRobotRole role) {
    switch (role) {
    case MatchRobotRole::Heavy: return QStringLiteral("heavy");
    case MatchRobotRole::Infantry: return QStringLiteral("infantry");
    case MatchRobotRole::Engineer: return QStringLiteral("engineer");
    case MatchRobotRole::Aerial: return QStringLiteral("aerial");
    case MatchRobotRole::Sentry: return QStringLiteral("sentry");
    case MatchRobotRole::Unknown: return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

QString matchRobotRoleLabel(MatchRobotRole role) {
    switch (role) {
    case MatchRobotRole::Heavy: return QStringLiteral("重装");
    case MatchRobotRole::Infantry: return QStringLiteral("步兵");
    case MatchRobotRole::Engineer: return QStringLiteral("工程");
    case MatchRobotRole::Aerial: return QStringLiteral("空中系统");
    case MatchRobotRole::Sentry: return QStringLiteral("哨兵");
    case MatchRobotRole::Unknown: return QStringLiteral("未知");
    }
    return QStringLiteral("未知");
}

QString matchChassisModeKey(MatchChassisMode mode) {
    switch (mode) {
    case MatchChassisMode::Unknown: return QStringLiteral("unknown");
    case MatchChassisMode::Normal: return QStringLiteral("normal");
    case MatchChassisMode::Follow: return QStringLiteral("follow");
    case MatchChassisMode::Spin: return QStringLiteral("spin");
    case MatchChassisMode::Protect: return QStringLiteral("protect");
    }
    return QStringLiteral("unknown");
}

QString matchChassisModeLabel(MatchChassisMode mode) {
    switch (mode) {
    case MatchChassisMode::Unknown: return QStringLiteral("未知");
    case MatchChassisMode::Normal: return QStringLiteral("普通");
    case MatchChassisMode::Follow: return QStringLiteral("跟随");
    case MatchChassisMode::Spin: return QStringLiteral("小陀螺");
    case MatchChassisMode::Protect: return QStringLiteral("保护");
    }
    return QStringLiteral("未知");
}

QString matchChassisDriveTypeKey(MatchChassisDriveType type) {
    switch (type) {
    case MatchChassisDriveType::Unknown: return QStringLiteral("unknown");
    case MatchChassisDriveType::Wheel: return QStringLiteral("wheel");
    case MatchChassisDriveType::WheelLeg: return QStringLiteral("wheel_leg");
    }
    return QStringLiteral("unknown");
}

QString matchChassisDriveTypeLabel(MatchChassisDriveType type) {
    switch (type) {
    case MatchChassisDriveType::Unknown: return QStringLiteral("未知");
    case MatchChassisDriveType::Wheel: return QStringLiteral("轮式");
    case MatchChassisDriveType::WheelLeg: return QStringLiteral("轮腿");
    }
    return QStringLiteral("未知");
}

QString matchWheelLegModeKey(MatchWheelLegMode mode) {
    switch (mode) {
    case MatchWheelLegMode::Unknown: return QStringLiteral("unknown");
    case MatchWheelLegMode::Wheel: return QStringLiteral("wheel");
    case MatchWheelLegMode::Leg: return QStringLiteral("leg");
    case MatchWheelLegMode::Transition: return QStringLiteral("transition");
    case MatchWheelLegMode::Balance: return QStringLiteral("balance");
    }
    return QStringLiteral("unknown");
}

QString matchWheelLegModeLabel(MatchWheelLegMode mode) {
    switch (mode) {
    case MatchWheelLegMode::Unknown: return QStringLiteral("未知");
    case MatchWheelLegMode::Wheel: return QStringLiteral("轮式");
    case MatchWheelLegMode::Leg: return QStringLiteral("腿式");
    case MatchWheelLegMode::Transition: return QStringLiteral("切换中");
    case MatchWheelLegMode::Balance: return QStringLiteral("平衡");
    }
    return QStringLiteral("未知");
}

QString matchTelemetryScopeKey(MatchTelemetryScope scope) {
    return scope == MatchTelemetryScope::Private
        ? QStringLiteral("private")
        : QStringLiteral("public");
}

QString matchTelemetryScopeLabel(MatchTelemetryScope scope) {
    return scope == MatchTelemetryScope::Private
        ? QStringLiteral("己方内部遥测")
        : QStringLiteral("公开数据");
}

QVariantMap MatchEnergySnapshot::toVariantMap() const {
    return {
        {QStringLiteral("superCapPercent"), superCapPercent},
        {QStringLiteral("bufferEnergyJ"), bufferEnergyJ},
        {QStringLiteral("bufferEnergyLimitJ"), bufferEnergyLimitJ},
        {QStringLiteral("chassisEnergyJ"), chassisEnergyJ},
        {QStringLiteral("chassisEnergyLimitJ"), chassisEnergyLimitJ},
        {QStringLiteral("chassisPowerW"), chassisPowerW},
        {QStringLiteral("chassisPowerLimitW"), chassisPowerLimitW},
    };
}

QVariantMap MatchChassisSnapshot::toVariantMap() const {
    return {
        {QStringLiteral("mode"), matchChassisModeKey(mode)},
        {QStringLiteral("modeLabel"), matchChassisModeLabel(mode)},
        {QStringLiteral("driveType"), matchChassisDriveTypeKey(driveType)},
        {QStringLiteral("driveTypeLabel"),
         matchChassisDriveTypeLabel(driveType)},
        {QStringLiteral("enabled"), enabled},
        {QStringLiteral("enabledReported"), enabledReported},
        {QStringLiteral("overpowerLockRemainingSec"),
         overpowerLockRemainingSec},
    };
}

QVariantMap MatchWheelLegJointSnapshot::toVariantMap() const {
    return {
        {QStringLiteral("name"), name},
        {QStringLiteral("angleDegrees"), angleDegrees},
        {QStringLiteral("targetAngleDegrees"), targetAngleDegrees},
    };
}

QVariantMap MatchWheelLegSnapshot::toVariantMap() const {
    QVariantList jointMaps;
    jointMaps.reserve(joints.size());
    for (const MatchWheelLegJointSnapshot &joint : joints) {
        jointMaps.append(joint.toVariantMap());
    }
    return {
        {QStringLiteral("telemetryAvailable"), telemetryAvailable},
        {QStringLiteral("mode"), matchWheelLegModeKey(mode)},
        {QStringLiteral("modeLabel"), matchWheelLegModeLabel(mode)},
        {QStringLiteral("stanceHeightMm"), stanceHeightMm},
        {QStringLiteral("pitchDegrees"), pitchDegrees},
        {QStringLiteral("rollDegrees"), rollDegrees},
        {QStringLiteral("leftWheelSpeedMps"), leftWheelSpeedMps},
        {QStringLiteral("rightWheelSpeedMps"), rightWheelSpeedMps},
        {QStringLiteral("joints"), jointMaps},
    };
}

QVariantMap MatchRobotSnapshot::toVariantMap() const {
    QVariantMap map{
        {QStringLiteral("id"), id},
        {QStringLiteral("slot"), slot},
        {QStringLiteral("side"), matchSideKey(side)},
        {QStringLiteral("role"), matchRobotRoleKey(role)},
        {QStringLiteral("roleLabel"), matchRobotRoleLabel(role)},
        {QStringLiteral("telemetryScope"),
         matchTelemetryScopeKey(telemetryScope)},
        {QStringLiteral("telemetryScopeLabel"),
         matchTelemetryScopeLabel(telemetryScope)},
        {QStringLiteral("health"), health},
        {QStringLiteral("maxHealth"), maxHealth},
        {QStringLiteral("ammo"), ammo},
        {QStringLiteral("heat"), heat},
        {QStringLiteral("heatLimit"), heatLimit},
        {QStringLiteral("level"), level},
        {QStringLiteral("experience"), experience},
        {QStringLiteral("maxExperience"), maxExperience},
        {QStringLiteral("yellowCardCount"), yellowCardCount},
        {QStringLiteral("muzzleVelocityMps"), muzzleVelocityMps},
        {QStringLiteral("shootSpeedLimitMps"), shootSpeedLimitMps},
        {QStringLiteral("speedLockState"), speedLockState},
        {QStringLiteral("speedLockSeconds"), speedLockSeconds},
        {QStringLiteral("energy"), energy.toVariantMap()},
        {QStringLiteral("chassis"), chassis.toVariantMap()},
        {QStringLiteral("wheelLeg"), wheelLeg.toVariantMap()},
        {QStringLiteral("moduleStates"), moduleStates},
        {QStringLiteral("moduleData"), moduleData},
        {QStringLiteral("buffTimedData"), buffTimedData},
        {QStringLiteral("customData"), customData},
        {QStringLiteral("online"), online},
        {QStringLiteral("alive"), alive},
        {QStringLiteral("outOfCombat"), outOfCombat},
        {QStringLiteral("respawnRemainingSec"), respawnRemainingSec},
        {QStringLiteral("hasPosition"), hasPosition},
        {QStringLiteral("updatedAtMs"), updatedAtMs},
    };
    if (hasPosition) {
        map.insert(QStringLiteral("position"),
                   QVariantMap{
                       {QStringLiteral("x"), mapX},
                       {QStringLiteral("y"), mapY},
                       {QStringLiteral("headingDegrees"), headingDegrees},
                   });
    }
    return map;
}

QVariantMap MatchTeamSnapshot::toVariantMap() const {
    QVariantList robotMaps;
    robotMaps.reserve(robots.size());
    for (const MatchRobotSnapshot &robot : robots) {
        robotMaps.append(robot.toVariantMap());
    }
    return {
        {QStringLiteral("name"), name},
        {QStringLiteral("score"), score},
        {QStringLiteral("economy"), economy},
        {QStringLiteral("totalEconomyObtained"), totalEconomyObtained},
        {QStringLiteral("techLevel"), techLevel},
        {QStringLiteral("encryptionLevel"), encryptionLevel},
        {QStringLiteral("baseHealth"), baseHealth},
        {QStringLiteral("baseMaxHealth"), baseMaxHealth},
        {QStringLiteral("baseStatus"), baseStatus},
        {QStringLiteral("outpostHealth"), outpostHealth},
        {QStringLiteral("outpostMaxHealth"), outpostMaxHealth},
        {QStringLiteral("outpostStatus"), outpostStatus},
        {QStringLiteral("controlPoints"), controlPoints},
        {QStringLiteral("totalDamage"), totalDamage},
        {QStringLiteral("robots"), robotMaps},
    };
}

QVariantMap MatchEventSnapshot::toVariantMap() const {
    QVariantMap map{
        {QStringLiteral("sequence"), QVariant::fromValue(sequence)},
        {QStringLiteral("occurredAtMs"), occurredAtMs},
        {QStringLiteral("type"), type},
        {QStringLiteral("message"), message},
        {QStringLiteral("hasSide"), hasSide},
        {QStringLiteral("robotSlot"), robotSlot},
    };
    if (hasSide) {
        map.insert(QStringLiteral("side"), matchSideKey(side));
    }
    return map;
}

QVariantMap MatchSnapshot::toVariantMap() const {
    QVariantList eventMaps;
    eventMaps.reserve(events.size());
    for (const MatchEventSnapshot &event : events) {
        eventMaps.append(event.toVariantMap());
    }
    return {
        {QStringLiteral("matchId"), matchId},
        {QStringLiteral("profile"), profile},
        {QStringLiteral("phase"), matchPhaseKey(phase)},
        {QStringLiteral("phaseLabel"), matchPhaseLabel(phase)},
        {QStringLiteral("isPaused"), isPaused},
        {QStringLiteral("timeRemainingSec"), timeRemainingSec},
        {QStringLiteral("round"), round},
        {QStringLiteral("operatorRobotId"), operatorRobotId},
        {QStringLiteral("redTeam"), redTeam.toVariantMap()},
        {QStringLiteral("blueTeam"), blueTeam.toVariantMap()},
        {QStringLiteral("events"), eventMaps},
        {QStringLiteral("updatedAtMs"), updatedAtMs},
    };
}

QList<MatchValidationIssue> validateMatchSnapshot(
    const MatchSnapshot &snapshot) {
    QList<MatchValidationIssue> issues;

    if (snapshot.matchId.trimmed().isEmpty()) {
        addIssue(&issues,
                 MatchValidationSeverity::Error,
                 QStringLiteral("matchId"),
                 QStringLiteral("比赛 ID 不能为空"));
    }
    if (snapshot.profile != QStringLiteral("rmul3v3") &&
        snapshot.profile != QStringLiteral("rmul1v1")) {
        addIssue(&issues,
                 MatchValidationSeverity::Error,
                 QStringLiteral("profile"),
                 QStringLiteral("仅接受 rmul3v3 或 rmul1v1 比赛快照"));
    }
    if (snapshot.timeRemainingSec < 0) {
        addIssue(&issues,
                 MatchValidationSeverity::Error,
                 QStringLiteral("timeRemainingSec"),
                 QStringLiteral("比赛剩余时间不能为负数"));
    }
    if (snapshot.round <= 0) {
        addIssue(&issues,
                 MatchValidationSeverity::Error,
                 QStringLiteral("round"),
                 QStringLiteral("比赛回合必须为正数"));
    }
    if (snapshot.updatedAtMs < 0) {
        addIssue(&issues,
                 MatchValidationSeverity::Error,
                 QStringLiteral("updatedAtMs"),
                 QStringLiteral("更新时间戳不能为负数"));
    }

    validateTeam(&issues,
                 snapshot.redTeam,
                 MatchSide::Red,
                 QStringLiteral("redTeam"),
                 snapshot.profile == QStringLiteral("rmul1v1") ? 1 : 3);
    validateTeam(&issues,
                 snapshot.blueTeam,
                 MatchSide::Blue,
                 QStringLiteral("blueTeam"),
                 snapshot.profile == QStringLiteral("rmul1v1") ? 1 : 3);

    QSet<QString> robotIds;
    const auto collectIds = [&](const MatchTeamSnapshot &team) {
        for (const MatchRobotSnapshot &robot : team.robots) {
            const QString normalizedId = robot.id.trimmed().toUpper();
            if (!normalizedId.isEmpty() && robotIds.contains(normalizedId)) {
                addIssue(&issues,
                         MatchValidationSeverity::Error,
                         QStringLiteral("robots.id"),
                         QStringLiteral("机器人 ID 在整场比赛中必须唯一：%1")
                             .arg(robot.id));
            }
            robotIds.insert(normalizedId);
        }
    };
    collectIds(snapshot.redTeam);
    collectIds(snapshot.blueTeam);

    QSet<quint64> eventSequences;
    qint64 previousOccurredAtMs = -1;
    for (int index = 0; index < snapshot.events.size(); ++index) {
        const MatchEventSnapshot &event = snapshot.events.at(index);
        const QString path =
            QStringLiteral("events[%1]").arg(index);
        if (event.type.trimmed().isEmpty()) {
            addIssue(&issues,
                     MatchValidationSeverity::Error,
                     path + QStringLiteral(".type"),
                     QStringLiteral("事件类型不能为空"));
        }
        if (event.message.trimmed().isEmpty()) {
            addIssue(&issues,
                     MatchValidationSeverity::Warning,
                     path + QStringLiteral(".message"),
                     QStringLiteral("事件消息为空"));
        }
        if (event.occurredAtMs < 0) {
            addIssue(&issues,
                     MatchValidationSeverity::Error,
                     path + QStringLiteral(".occurredAtMs"),
                     QStringLiteral("事件时间戳不能为负数"));
        }
        if (eventSequences.contains(event.sequence)) {
            addIssue(&issues,
                     MatchValidationSeverity::Error,
                     path + QStringLiteral(".sequence"),
                     QStringLiteral("事件序号不能重复"));
        }
        eventSequences.insert(event.sequence);
        if (previousOccurredAtMs >= 0 &&
            event.occurredAtMs < previousOccurredAtMs) {
            addIssue(&issues,
                     MatchValidationSeverity::Warning,
                     path + QStringLiteral(".occurredAtMs"),
                     QStringLiteral("事件未按发生时间排列"));
        }
        previousOccurredAtMs = event.occurredAtMs;
        if (event.robotSlot != -1 && event.robotSlot <= 0) {
            addIssue(&issues,
                     MatchValidationSeverity::Error,
                     path + QStringLiteral(".robotSlot"),
                     QStringLiteral("事件机器人车位必须为正数或 -1"));
        }
    }

    return issues;
}

bool hasMatchValidationError(
    const QList<MatchValidationIssue> &issues) {
    return std::any_of(
        issues.cbegin(),
        issues.cend(),
        [](const MatchValidationIssue &issue) {
            return issue.severity == MatchValidationSeverity::Error;
        });
}

QString matchValidationErrorMessage(
    const QList<MatchValidationIssue> &issues) {
    QStringList messages;
    for (const MatchValidationIssue &issue : issues) {
        if (issue.severity != MatchValidationSeverity::Error) {
            continue;
        }
        messages.append(
            QStringLiteral("%1: %2").arg(issue.path, issue.message));
    }
    return messages.join(QStringLiteral("；"));
}

MatchSession::MatchSession(QObject *parent)
    : QObject(parent) {}

const MatchSnapshot &MatchSession::matchSnapshot() const {
    return m_snapshot;
}

QVariantMap MatchSession::snapshot() const {
    return m_snapshot.toVariantMap();
}

bool MatchSession::isValid() const {
    return m_valid;
}

QString MatchSession::validationError() const {
    return matchValidationErrorMessage(m_validationIssues);
}

QList<MatchValidationIssue> MatchSession::validationIssues() const {
    return m_validationIssues;
}

bool MatchSession::replaceSnapshot(const MatchSnapshot &snapshot,
                                   QString *errorMessage) {
    const QList<MatchValidationIssue> issues =
        validateMatchSnapshot(snapshot);
    if (hasMatchValidationError(issues)) {
        m_validationIssues = issues;
        if (errorMessage) {
            *errorMessage = validationError();
        }
        emit validationChanged();
        return false;
    }

    m_snapshot = snapshot;
    m_validationIssues = issues;
    m_valid = true;
    if (errorMessage) {
        errorMessage->clear();
    }
    emit validationChanged();
    emit snapshotChanged();
    return true;
}

void MatchSession::clear() {
    m_snapshot = MatchSnapshot{};
    m_validationIssues.clear();
    m_valid = false;
    emit validationChanged();
    emit snapshotChanged();
}

} // namespace RM
