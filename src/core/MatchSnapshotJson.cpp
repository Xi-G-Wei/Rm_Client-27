#include "core/MatchSnapshotJson.h"

#include <QJsonArray>

namespace RM {
namespace {

bool fail(QString *error, const QString &message) {
    if (error) {
        *error = message;
    }
    return false;
}

QString phaseKey(MatchPhase phase) {
    return matchPhaseKey(phase);
}

bool parsePhase(const QString &value, MatchPhase *phase) {
    if (value == QStringLiteral("not_started")) {
        *phase = MatchPhase::NotStarted;
    } else if (value == QStringLiteral("preparation")) {
        *phase = MatchPhase::Preparation;
    } else if (value == QStringLiteral("self_check")) {
        *phase = MatchPhase::SelfCheck;
    } else if (value == QStringLiteral("countdown")) {
        *phase = MatchPhase::Countdown;
    } else if (value == QStringLiteral("battle")) {
        *phase = MatchPhase::Battle;
    } else if (value == QStringLiteral("settlement")) {
        *phase = MatchPhase::Settlement;
    } else {
        return false;
    }
    return true;
}

bool parseRole(const QString &value, MatchRobotRole *role) {
    if (value == QStringLiteral("heavy")) {
        *role = MatchRobotRole::Heavy;
    } else if (value == QStringLiteral("infantry")) {
        *role = MatchRobotRole::Infantry;
    } else if (value == QStringLiteral("engineer")) {
        *role = MatchRobotRole::Engineer;
    } else if (value == QStringLiteral("aerial")) {
        *role = MatchRobotRole::Aerial;
    } else if (value == QStringLiteral("sentry")) {
        *role = MatchRobotRole::Sentry;
    } else if (value == QStringLiteral("unknown")) {
        *role = MatchRobotRole::Unknown;
    } else {
        return false;
    }
    return true;
}

bool parseScope(const QString &value, MatchTelemetryScope *scope) {
    if (value == QStringLiteral("private")) {
        *scope = MatchTelemetryScope::Private;
    } else if (value == QStringLiteral("public")) {
        *scope = MatchTelemetryScope::Public;
    } else {
        return false;
    }
    return true;
}

QJsonObject robotToJson(const MatchRobotSnapshot &robot) {
    QJsonObject energy{
        {QStringLiteral("superCapPercent"), robot.energy.superCapPercent},
        {QStringLiteral("bufferEnergyJ"), robot.energy.bufferEnergyJ},
        {QStringLiteral("bufferEnergyLimitJ"), robot.energy.bufferEnergyLimitJ},
        {QStringLiteral("chassisEnergyJ"), robot.energy.chassisEnergyJ},
        {QStringLiteral("chassisEnergyLimitJ"), robot.energy.chassisEnergyLimitJ},
        {QStringLiteral("chassisPowerW"), robot.energy.chassisPowerW},
        {QStringLiteral("chassisPowerLimitW"), robot.energy.chassisPowerLimitW},
    };
    QJsonObject chassis{
        {QStringLiteral("mode"), matchChassisModeKey(robot.chassis.mode)},
        {QStringLiteral("driveType"), matchChassisDriveTypeKey(robot.chassis.driveType)},
        {QStringLiteral("enabled"), robot.chassis.enabled},
        {QStringLiteral("enabledReported"), robot.chassis.enabledReported},
        {QStringLiteral("overpowerLockRemainingSec"),
         robot.chassis.overpowerLockRemainingSec},
    };
    QJsonArray joints;
    for (const MatchWheelLegJointSnapshot &joint : robot.wheelLeg.joints) {
        joints.append(QJsonObject{
            {QStringLiteral("name"), joint.name},
            {QStringLiteral("angleDegrees"), joint.angleDegrees},
            {QStringLiteral("targetAngleDegrees"), joint.targetAngleDegrees},
        });
    }
    QJsonObject wheelLeg{
        {QStringLiteral("telemetryAvailable"), robot.wheelLeg.telemetryAvailable},
        {QStringLiteral("mode"), matchWheelLegModeKey(robot.wheelLeg.mode)},
        {QStringLiteral("stanceHeightMm"), robot.wheelLeg.stanceHeightMm},
        {QStringLiteral("pitchDegrees"), robot.wheelLeg.pitchDegrees},
        {QStringLiteral("rollDegrees"), robot.wheelLeg.rollDegrees},
        {QStringLiteral("leftWheelSpeedMps"), robot.wheelLeg.leftWheelSpeedMps},
        {QStringLiteral("rightWheelSpeedMps"), robot.wheelLeg.rightWheelSpeedMps},
        {QStringLiteral("joints"), joints},
    };
    QJsonObject object{
        {QStringLiteral("id"), robot.id},
        {QStringLiteral("slot"), robot.slot},
        {QStringLiteral("side"), matchSideKey(robot.side)},
        {QStringLiteral("role"), matchRobotRoleKey(robot.role)},
        {QStringLiteral("telemetryScope"), matchTelemetryScopeKey(robot.telemetryScope)},
        {QStringLiteral("health"), robot.health},
        {QStringLiteral("maxHealth"), robot.maxHealth},
        {QStringLiteral("ammo"), robot.ammo},
        {QStringLiteral("heat"), robot.heat},
        {QStringLiteral("heatLimit"), robot.heatLimit},
        {QStringLiteral("online"), robot.online},
        {QStringLiteral("alive"), robot.alive},
        {QStringLiteral("respawnRemainingSec"), robot.respawnRemainingSec},
        {QStringLiteral("hasPosition"), robot.hasPosition},
        {QStringLiteral("updatedAtMs"), robot.updatedAtMs},
        {QStringLiteral("energy"), energy},
        {QStringLiteral("chassis"), chassis},
        {QStringLiteral("wheelLeg"), wheelLeg},
    };
    if (robot.hasPosition) {
        object.insert(QStringLiteral("mapX"), robot.mapX);
        object.insert(QStringLiteral("mapY"), robot.mapY);
        object.insert(QStringLiteral("headingDegrees"), robot.headingDegrees);
    }
    return object;
}

QJsonObject teamToJson(const MatchTeamSnapshot &team) {
    QJsonArray robots;
    for (const MatchRobotSnapshot &robot : team.robots) {
        robots.append(robotToJson(robot));
    }
    return {
        {QStringLiteral("name"), team.name},
        {QStringLiteral("score"), team.score},
        {QStringLiteral("robots"), robots},
    };
}

bool readInt(const QJsonObject &object,
             const QString &key,
             int *value,
             QString *error) {
    const QJsonValue field = object.value(key);
    if (!field.isDouble()) {
        return fail(error, QStringLiteral("%1 必须是整数").arg(key));
    }
    *value = field.toInt();
    return true;
}

bool readReal(const QJsonObject &object,
              const QString &key,
              double *value,
              QString *error) {
    const QJsonValue field = object.value(key);
    if (!field.isDouble()) {
        return fail(error, QStringLiteral("%1 必须是数值").arg(key));
    }
    *value = field.toDouble();
    return true;
}

bool parseRobot(const QJsonObject &object,
                MatchRobotSnapshot *robot,
                QString *error) {
    if (!object.value(QStringLiteral("id")).isString() ||
        !readInt(object, QStringLiteral("slot"), &robot->slot, error)) {
        return false;
    }
    robot->id = object.value(QStringLiteral("id")).toString();
    const QString side = object.value(QStringLiteral("side")).toString();
    if (side != QStringLiteral("red") && side != QStringLiteral("blue")) {
        return fail(error, QStringLiteral("机器人阵营无效"));
    }
    robot->side = side == QStringLiteral("red") ? MatchSide::Red : MatchSide::Blue;
    if (!parseRole(object.value(QStringLiteral("role")).toString(), &robot->role) ||
        !parseScope(object.value(QStringLiteral("telemetryScope")).toString(),
                    &robot->telemetryScope)) {
        return fail(error, QStringLiteral("机器人枚举值无效"));
    }
    if (!readInt(object, QStringLiteral("health"), &robot->health, error) ||
        !readInt(object, QStringLiteral("maxHealth"), &robot->maxHealth, error) ||
        !readInt(object, QStringLiteral("ammo"), &robot->ammo, error) ||
        !readInt(object, QStringLiteral("heat"), &robot->heat, error) ||
        !readInt(object, QStringLiteral("heatLimit"), &robot->heatLimit, error) ||
        !readInt(object, QStringLiteral("respawnRemainingSec"),
                 &robot->respawnRemainingSec, error)) {
        return false;
    }
    robot->online = object.value(QStringLiteral("online")).toBool();
    robot->alive = object.value(QStringLiteral("alive")).toBool();
    robot->hasPosition = object.value(QStringLiteral("hasPosition")).toBool();
    robot->updatedAtMs =
        static_cast<qint64>(object.value(QStringLiteral("updatedAtMs")).toDouble());
    if (robot->hasPosition &&
        (!readReal(object, QStringLiteral("mapX"), &robot->mapX, error) ||
         !readReal(object, QStringLiteral("mapY"), &robot->mapY, error) ||
         !readReal(object, QStringLiteral("headingDegrees"),
                   &robot->headingDegrees, error))) {
        return false;
    }

    const QJsonObject energy =
        object.value(QStringLiteral("energy")).toObject();
    if (!readReal(energy, QStringLiteral("superCapPercent"),
                  &robot->energy.superCapPercent, error) ||
        !readReal(energy, QStringLiteral("bufferEnergyJ"),
                  &robot->energy.bufferEnergyJ, error) ||
        !readReal(energy, QStringLiteral("bufferEnergyLimitJ"),
                  &robot->energy.bufferEnergyLimitJ, error) ||
        !readReal(energy, QStringLiteral("chassisEnergyJ"),
                  &robot->energy.chassisEnergyJ, error) ||
        !readReal(energy, QStringLiteral("chassisEnergyLimitJ"),
                  &robot->energy.chassisEnergyLimitJ, error) ||
        !readReal(energy, QStringLiteral("chassisPowerW"),
                  &robot->energy.chassisPowerW, error) ||
        !readReal(energy, QStringLiteral("chassisPowerLimitW"),
                  &robot->energy.chassisPowerLimitW, error)) {
        return false;
    }

    const QJsonObject chassis =
        object.value(QStringLiteral("chassis")).toObject();
    const QString mode = chassis.value(QStringLiteral("mode")).toString();
    const QString drive = chassis.value(QStringLiteral("driveType")).toString();
    if (mode == QStringLiteral("normal")) robot->chassis.mode = MatchChassisMode::Normal;
    else if (mode == QStringLiteral("follow")) robot->chassis.mode = MatchChassisMode::Follow;
    else if (mode == QStringLiteral("spin")) robot->chassis.mode = MatchChassisMode::Spin;
    else if (mode == QStringLiteral("protect")) robot->chassis.mode = MatchChassisMode::Protect;
    else if (mode != QStringLiteral("unknown")) return fail(error, QStringLiteral("底盘模式无效"));
    if (drive == QStringLiteral("wheel")) robot->chassis.driveType = MatchChassisDriveType::Wheel;
    else if (drive == QStringLiteral("wheel_leg")) robot->chassis.driveType = MatchChassisDriveType::WheelLeg;
    else if (drive != QStringLiteral("unknown")) return fail(error, QStringLiteral("驱动类型无效"));
    robot->chassis.enabled = chassis.value(QStringLiteral("enabled")).toBool();
    robot->chassis.enabledReported =
        chassis.value(QStringLiteral("enabledReported")).toBool();
    robot->chassis.overpowerLockRemainingSec =
        chassis.value(QStringLiteral("overpowerLockRemainingSec")).toInt(-1);

    const QJsonObject wheelLeg =
        object.value(QStringLiteral("wheelLeg")).toObject();
    robot->wheelLeg.telemetryAvailable =
        wheelLeg.value(QStringLiteral("telemetryAvailable")).toBool();
    const QString wheelMode = wheelLeg.value(QStringLiteral("mode")).toString();
    if (wheelMode == QStringLiteral("wheel")) robot->wheelLeg.mode = MatchWheelLegMode::Wheel;
    else if (wheelMode == QStringLiteral("leg")) robot->wheelLeg.mode = MatchWheelLegMode::Leg;
    else if (wheelMode == QStringLiteral("transition")) robot->wheelLeg.mode = MatchWheelLegMode::Transition;
    else if (wheelMode == QStringLiteral("balance")) robot->wheelLeg.mode = MatchWheelLegMode::Balance;
    else if (wheelMode != QStringLiteral("unknown")) return fail(error, QStringLiteral("轮腿模式无效"));
    if (!readReal(wheelLeg, QStringLiteral("stanceHeightMm"),
                  &robot->wheelLeg.stanceHeightMm, error) ||
        !readReal(wheelLeg, QStringLiteral("pitchDegrees"),
                  &robot->wheelLeg.pitchDegrees, error) ||
        !readReal(wheelLeg, QStringLiteral("rollDegrees"),
                  &robot->wheelLeg.rollDegrees, error) ||
        !readReal(wheelLeg, QStringLiteral("leftWheelSpeedMps"),
                  &robot->wheelLeg.leftWheelSpeedMps, error) ||
        !readReal(wheelLeg, QStringLiteral("rightWheelSpeedMps"),
                  &robot->wheelLeg.rightWheelSpeedMps, error)) {
        return false;
    }
    const QJsonArray joints = wheelLeg.value(QStringLiteral("joints")).toArray();
    for (const QJsonValue &value : joints) {
        const QJsonObject joint = value.toObject();
        MatchWheelLegJointSnapshot parsed;
        parsed.name = joint.value(QStringLiteral("name")).toString();
        if (!readReal(joint, QStringLiteral("angleDegrees"),
                      &parsed.angleDegrees, error) ||
            !readReal(joint, QStringLiteral("targetAngleDegrees"),
                      &parsed.targetAngleDegrees, error)) {
            return false;
        }
        robot->wheelLeg.joints.append(parsed);
    }
    return true;
}

bool parseTeam(const QJsonObject &object,
               MatchTeamSnapshot *team,
               QString *error) {
    team->name = object.value(QStringLiteral("name")).toString();
    if (!readInt(object, QStringLiteral("score"), &team->score, error)) {
        return false;
    }
    const QJsonArray robots = object.value(QStringLiteral("robots")).toArray();
    for (const QJsonValue &value : robots) {
        MatchRobotSnapshot robot;
        if (!parseRobot(value.toObject(), &robot, error)) {
            return false;
        }
        team->robots.append(robot);
    }
    return true;
}

} // namespace

QJsonObject matchSnapshotToJson(const MatchSnapshot &snapshot) {
    QJsonArray events;
    for (const MatchEventSnapshot &event : snapshot.events) {
        events.append(QJsonObject{
            {QStringLiteral("sequence"), QString::number(event.sequence)},
            {QStringLiteral("occurredAtMs"), event.occurredAtMs},
            {QStringLiteral("type"), event.type},
            {QStringLiteral("message"), event.message},
            {QStringLiteral("hasSide"), event.hasSide},
            {QStringLiteral("side"), matchSideKey(event.side)},
            {QStringLiteral("robotSlot"), event.robotSlot},
        });
    }
    return {
        {QStringLiteral("matchId"), snapshot.matchId},
        {QStringLiteral("profile"), snapshot.profile},
        {QStringLiteral("phase"), phaseKey(snapshot.phase)},
        {QStringLiteral("timeRemainingSec"), snapshot.timeRemainingSec},
        {QStringLiteral("round"), snapshot.round},
        {QStringLiteral("redTeam"), teamToJson(snapshot.redTeam)},
        {QStringLiteral("blueTeam"), teamToJson(snapshot.blueTeam)},
        {QStringLiteral("events"), events},
        {QStringLiteral("updatedAtMs"), snapshot.updatedAtMs},
    };
}

bool matchSnapshotFromJson(const QJsonObject &object,
                           MatchSnapshot *snapshot,
                           QString *error) {
    if (!snapshot) {
        return fail(error, QStringLiteral("snapshot 不能为空"));
    }
    MatchSnapshot parsed;
    parsed.matchId = object.value(QStringLiteral("matchId")).toString();
    parsed.profile = object.value(QStringLiteral("profile")).toString();
    if (!parsePhase(object.value(QStringLiteral("phase")).toString(),
                    &parsed.phase) ||
        !readInt(object, QStringLiteral("timeRemainingSec"),
                 &parsed.timeRemainingSec, error) ||
        !readInt(object, QStringLiteral("round"), &parsed.round, error)) {
        return fail(error, QStringLiteral("比赛快照头部无效"));
    }
    parsed.updatedAtMs =
        static_cast<qint64>(object.value(QStringLiteral("updatedAtMs")).toDouble());
    if (!parseTeam(object.value(QStringLiteral("redTeam")).toObject(),
                   &parsed.redTeam, error) ||
        !parseTeam(object.value(QStringLiteral("blueTeam")).toObject(),
                   &parsed.blueTeam, error)) {
        return false;
    }
    for (const QJsonValue &value :
         object.value(QStringLiteral("events")).toArray()) {
        const QJsonObject event = value.toObject();
        MatchEventSnapshot parsedEvent;
        bool ok = false;
        parsedEvent.sequence =
            event.value(QStringLiteral("sequence")).toString().toULongLong(&ok);
        if (!ok) {
            return fail(error, QStringLiteral("事件序号无效"));
        }
        parsedEvent.occurredAtMs =
            static_cast<qint64>(event.value(QStringLiteral("occurredAtMs")).toDouble());
        parsedEvent.type = event.value(QStringLiteral("type")).toString();
        parsedEvent.message = event.value(QStringLiteral("message")).toString();
        parsedEvent.hasSide = event.value(QStringLiteral("hasSide")).toBool();
        parsedEvent.side = event.value(QStringLiteral("side")).toString()
            == QStringLiteral("blue") ? MatchSide::Blue : MatchSide::Red;
        parsedEvent.robotSlot = event.value(QStringLiteral("robotSlot")).toInt(-1);
        parsed.events.append(parsedEvent);
    }
    *snapshot = parsed;
    return true;
}

} // namespace RM
