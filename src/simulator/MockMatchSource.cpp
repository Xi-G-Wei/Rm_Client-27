#include "simulator/MockMatchSource.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <optional>

namespace RM {

namespace {

struct MockAction {
    int atSec = 0;
    QJsonObject object;
};

} // namespace

struct MockMatchSourcePrivate {
    QString id;
    QString name;
    int durationSec = 0;
    MatchSnapshot initial;
    QList<MockAction> actions;
    bool loaded = false;
    bool running = false;
    bool paused = false;
    bool completed = false;
    double playbackRate = 1.0;
    double virtualCarryMs = 0.0;
    qint64 elapsedMs = 0;
    quint64 nextEventSequence = 1;
    QSet<int> appliedActionIndexes;
    QString lastError;
};

namespace {

bool fail(QString *error, const QString &path, const QString &message) {
    if (error) {
        *error = path.isEmpty() ? message
                                : QStringLiteral("%1: %2").arg(path, message);
    }
    return false;
}

QJsonObject requireObject(const QJsonObject &parent,
                          const QString &key,
                          const QString &path,
                          QString *error) {
    const QJsonValue value = parent.value(key);
    if (!value.isObject()) {
        fail(error, path + key, QStringLiteral("必须是对象"));
        return {};
    }
    return value.toObject();
}

QJsonArray requireArray(const QJsonObject &parent,
                        const QString &key,
                        const QString &path,
                        QString *error) {
    const QJsonValue value = parent.value(key);
    if (!value.isArray()) {
        fail(error, path + key, QStringLiteral("必须是数组"));
        return {};
    }
    return value.toArray();
}

bool requireString(const QJsonObject &object,
                   const QString &key,
                   const QString &path,
                   QString *result,
                   QString *error) {
    const QJsonValue value = object.value(key);
    if (!value.isString() || value.toString().trimmed().isEmpty()) {
        return fail(error, path + key, QStringLiteral("必须是非空字符串"));
    }
    *result = value.toString();
    return true;
}

bool requireInt(const QJsonObject &object,
                const QString &key,
                const QString &path,
                int *result,
                QString *error) {
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        return fail(error, path + key, QStringLiteral("必须是整数"));
    }
    *result = value.toInt();
    return true;
}

bool optionalInt(const QJsonObject &object,
                 const QString &key,
                 const QString &path,
                 std::optional<int> *result,
                 QString *error) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    if (!value.isDouble()) {
        return fail(error, path + key, QStringLiteral("必须是整数"));
    }
    *result = value.toInt();
    return true;
}

bool optionalBool(const QJsonObject &object,
                  const QString &key,
                  const QString &path,
                  std::optional<bool> *result,
                  QString *error) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    if (!value.isBool()) {
        return fail(error, path + key, QStringLiteral("必须是布尔值"));
    }
    *result = value.toBool();
    return true;
}

bool optionalReal(const QJsonObject &object,
                  const QString &key,
                  const QString &path,
                  std::optional<double> *result,
                  QString *error) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    if (!value.isDouble()) {
        return fail(error, path + key, QStringLiteral("必须是数值"));
    }
    *result = value.toDouble();
    return true;
}

bool readMetric(const QJsonObject &object,
                const QString &key,
                const QString &path,
                int *result,
                QString *error) {
    if (!object.contains(key) || object.value(key).isNull()) {
        *result = kUnknownMatchMetric;
        return true;
    }
    return requireInt(object, key, path, result, error);
}

bool readReal(const QJsonObject &object,
              const QString &key,
              const QString &path,
              double *result,
              QString *error) {
    if (!object.contains(key) || object.value(key).isNull()) {
        *result = kUnknownMatchReal;
        return true;
    }
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        return fail(error, path + key, QStringLiteral("必须是数值"));
    }
    *result = value.toDouble();
    return true;
}

bool parsePhase(const QString &value,
                MatchPhase *phase,
                const QString &path,
                QString *error) {
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
        return fail(error, path, QStringLiteral("未知比赛阶段：%1").arg(value));
    }
    return true;
}

bool parseRole(const QString &value,
               MatchRobotRole *role,
               const QString &path,
               QString *error) {
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
        return fail(error, path, QStringLiteral("未知机器人角色：%1").arg(value));
    }
    return true;
}

bool parseChassisMode(const QString &value,
                      MatchChassisMode *mode,
                      const QString &path,
                      QString *error) {
    if (value == QStringLiteral("unknown")) {
        *mode = MatchChassisMode::Unknown;
    } else if (value == QStringLiteral("normal")) {
        *mode = MatchChassisMode::Normal;
    } else if (value == QStringLiteral("follow")) {
        *mode = MatchChassisMode::Follow;
    } else if (value == QStringLiteral("spin")) {
        *mode = MatchChassisMode::Spin;
    } else if (value == QStringLiteral("protect")) {
        *mode = MatchChassisMode::Protect;
    } else {
        return fail(error, path, QStringLiteral("未知底盘模式：%1").arg(value));
    }
    return true;
}

bool parseDriveType(const QString &value,
                    MatchChassisDriveType *type,
                    const QString &path,
                    QString *error) {
    if (value == QStringLiteral("unknown")) {
        *type = MatchChassisDriveType::Unknown;
    } else if (value == QStringLiteral("wheel")) {
        *type = MatchChassisDriveType::Wheel;
    } else if (value == QStringLiteral("wheel_leg")) {
        *type = MatchChassisDriveType::WheelLeg;
    } else {
        return fail(error, path, QStringLiteral("未知底盘驱动类型：%1").arg(value));
    }
    return true;
}

bool parseWheelLegMode(const QString &value,
                       MatchWheelLegMode *mode,
                       const QString &path,
                       QString *error) {
    if (value == QStringLiteral("unknown")) {
        *mode = MatchWheelLegMode::Unknown;
    } else if (value == QStringLiteral("wheel")) {
        *mode = MatchWheelLegMode::Wheel;
    } else if (value == QStringLiteral("leg")) {
        *mode = MatchWheelLegMode::Leg;
    } else if (value == QStringLiteral("transition")) {
        *mode = MatchWheelLegMode::Transition;
    } else if (value == QStringLiteral("balance")) {
        *mode = MatchWheelLegMode::Balance;
    } else {
        return fail(error, path, QStringLiteral("未知轮腿模式：%1").arg(value));
    }
    return true;
}

bool parseRobot(const QJsonObject &object,
                MatchSide side,
                const QString &path,
                MatchRobotSnapshot *robot,
                QString *error) {
    robot->side = side;
    robot->telemetryScope = side == MatchSide::Red
        ? MatchTelemetryScope::Private
        : MatchTelemetryScope::Public;
    QString role;
    QString phaseText;
    if (!requireString(object, QStringLiteral("id"), path, &robot->id, error) ||
        !requireInt(object, QStringLiteral("slot"), path, &robot->slot, error) ||
        !requireString(object, QStringLiteral("role"), path, &role, error) ||
        !parseRole(role, &robot->role, path + QStringLiteral("role"), error) ||
        !readMetric(object, QStringLiteral("health"), path, &robot->health, error) ||
        !readMetric(object,
                    QStringLiteral("maxHealth"),
                    path,
                    &robot->maxHealth,
                    error) ||
        !readMetric(object, QStringLiteral("ammo"), path, &robot->ammo, error) ||
        !readMetric(object, QStringLiteral("heat"), path, &robot->heat, error) ||
        !readMetric(object,
                    QStringLiteral("heatLimit"),
                    path,
                    &robot->heatLimit,
                    error) ||
        !readMetric(object,
                    QStringLiteral("respawnRemainingSec"),
                    path,
                    &robot->respawnRemainingSec,
                    error)) {
        return false;
    }
    robot->online = object.value(QStringLiteral("online")).toBool(false);
    robot->alive = object.value(QStringLiteral("alive")).toBool(false);

    const QJsonValue positionValue = object.value(QStringLiteral("position"));
    if (!positionValue.isUndefined() && !positionValue.isNull()) {
        if (!positionValue.isObject()) {
            return fail(error,
                        path + QStringLiteral("position"),
                        QStringLiteral("必须是对象"));
        }
        const QJsonObject position = positionValue.toObject();
        const QString positionPath = path + QStringLiteral("position.");
        if (!readReal(position,
                      QStringLiteral("x"),
                      positionPath,
                      &robot->mapX,
                      error) ||
            !readReal(position,
                      QStringLiteral("y"),
                      positionPath,
                      &robot->mapY,
                      error) ||
            !readReal(position,
                      QStringLiteral("headingDegrees"),
                      positionPath,
                      &robot->headingDegrees,
                      error)) {
            return false;
        }
        robot->hasPosition = true;
    }

    const QJsonValue energyValue = object.value(QStringLiteral("energy"));
    if (!energyValue.isUndefined() && !energyValue.isNull()) {
        if (!energyValue.isObject()) {
            return fail(error,
                        path + QStringLiteral("energy"),
                        QStringLiteral("必须是对象"));
        }
        const QJsonObject energy = energyValue.toObject();
        const QString energyPath = path + QStringLiteral("energy.");
        if (!readReal(energy,
                      QStringLiteral("superCapPercent"),
                      energyPath,
                      &robot->energy.superCapPercent,
                      error) ||
            !readReal(energy,
                      QStringLiteral("bufferEnergyJ"),
                      energyPath,
                      &robot->energy.bufferEnergyJ,
                      error) ||
            !readReal(energy,
                      QStringLiteral("bufferEnergyLimitJ"),
                      energyPath,
                      &robot->energy.bufferEnergyLimitJ,
                      error) ||
            !readReal(energy,
                      QStringLiteral("chassisEnergyJ"),
                      energyPath,
                      &robot->energy.chassisEnergyJ,
                      error) ||
            !readReal(energy,
                      QStringLiteral("chassisEnergyLimitJ"),
                      energyPath,
                      &robot->energy.chassisEnergyLimitJ,
                      error) ||
            !readReal(energy,
                      QStringLiteral("chassisPowerW"),
                      energyPath,
                      &robot->energy.chassisPowerW,
                      error) ||
            !readReal(energy,
                      QStringLiteral("chassisPowerLimitW"),
                      energyPath,
                      &robot->energy.chassisPowerLimitW,
                      error)) {
            return false;
        }
    }

    const QJsonValue chassisValue = object.value(QStringLiteral("chassis"));
    if (!chassisValue.isUndefined() && !chassisValue.isNull()) {
        if (!chassisValue.isObject()) {
            return fail(error,
                        path + QStringLiteral("chassis"),
                        QStringLiteral("必须是对象"));
        }
        const QJsonObject chassis = chassisValue.toObject();
        const QString chassisPath = path + QStringLiteral("chassis.");
        if (chassis.contains(QStringLiteral("mode"))) {
            phaseText = chassis.value(QStringLiteral("mode")).toString();
            if (!parseChassisMode(phaseText,
                                  &robot->chassis.mode,
                                  chassisPath + QStringLiteral("mode"),
                                  error)) {
                return false;
            }
        }
        if (chassis.contains(QStringLiteral("driveType"))) {
            phaseText = chassis.value(QStringLiteral("driveType")).toString();
            if (!parseDriveType(phaseText,
                                &robot->chassis.driveType,
                                chassisPath + QStringLiteral("driveType"),
                                error)) {
                return false;
            }
        }
        robot->chassis.enabled =
            chassis.value(QStringLiteral("enabled")).toBool(false);
        robot->chassis.enabledReported =
            chassis.value(QStringLiteral("enabledReported")).toBool(false);
        if (!readMetric(chassis,
                        QStringLiteral("overpowerLockRemainingSec"),
                        chassisPath,
                        &robot->chassis.overpowerLockRemainingSec,
                        error)) {
            return false;
        }
    }

    const QJsonValue wheelLegValue = object.value(QStringLiteral("wheelLeg"));
    if (!wheelLegValue.isUndefined() && !wheelLegValue.isNull()) {
        if (!wheelLegValue.isObject()) {
            return fail(error,
                        path + QStringLiteral("wheelLeg"),
                        QStringLiteral("必须是对象"));
        }
        const QJsonObject wheelLeg = wheelLegValue.toObject();
        const QString wheelLegPath = path + QStringLiteral("wheelLeg.");
        robot->wheelLeg.telemetryAvailable =
            wheelLeg.value(QStringLiteral("telemetryAvailable")).toBool(false);
        if (wheelLeg.contains(QStringLiteral("mode"))) {
            phaseText = wheelLeg.value(QStringLiteral("mode")).toString();
            if (!parseWheelLegMode(phaseText,
                                   &robot->wheelLeg.mode,
                                   wheelLegPath + QStringLiteral("mode"),
                                   error)) {
                return false;
            }
        }
        if (!readReal(wheelLeg,
                      QStringLiteral("stanceHeightMm"),
                      wheelLegPath,
                      &robot->wheelLeg.stanceHeightMm,
                      error) ||
            !readReal(wheelLeg,
                      QStringLiteral("pitchDegrees"),
                      wheelLegPath,
                      &robot->wheelLeg.pitchDegrees,
                      error) ||
            !readReal(wheelLeg,
                      QStringLiteral("rollDegrees"),
                      wheelLegPath,
                      &robot->wheelLeg.rollDegrees,
                      error) ||
            !readReal(wheelLeg,
                      QStringLiteral("leftWheelSpeedMps"),
                      wheelLegPath,
                      &robot->wheelLeg.leftWheelSpeedMps,
                      error) ||
            !readReal(wheelLeg,
                      QStringLiteral("rightWheelSpeedMps"),
                      wheelLegPath,
                      &robot->wheelLeg.rightWheelSpeedMps,
                      error)) {
            return false;
        }
        if (wheelLeg.contains(QStringLiteral("joints"))) {
            const QJsonArray joints = requireArray(wheelLeg,
                                                   QStringLiteral("joints"),
                                                   wheelLegPath,
                                                   error);
            if (!error->isEmpty()) {
                return false;
            }
            for (int index = 0; index < joints.size(); ++index) {
                if (!joints.at(index).isObject()) {
                    return fail(error,
                                wheelLegPath +
                                    QStringLiteral("joints[%1]").arg(index),
                                QStringLiteral("必须是对象"));
                }
                const QJsonObject joint = joints.at(index).toObject();
                const QString jointPath =
                    wheelLegPath +
                    QStringLiteral("joints[%1].").arg(index);
                MatchWheelLegJointSnapshot parsed;
                if (!requireString(joint,
                                   QStringLiteral("name"),
                                   jointPath,
                                   &parsed.name,
                                   error) ||
                    !readReal(joint,
                              QStringLiteral("angleDegrees"),
                              jointPath,
                              &parsed.angleDegrees,
                              error) ||
                    !readReal(joint,
                              QStringLiteral("targetAngleDegrees"),
                              jointPath,
                              &parsed.targetAngleDegrees,
                              error)) {
                    return false;
                }
                robot->wheelLeg.joints.append(parsed);
            }
        }
    }
    return true;
}

bool parseTeam(const QJsonObject &initial,
               const QString &key,
               MatchSide side,
               MatchTeamSnapshot *team,
               QString *error) {
    const QJsonObject object =
        requireObject(initial, key, QStringLiteral("initial."), error);
    if (!error->isEmpty()) {
        return false;
    }
    const QString path = QStringLiteral("initial.") + key + QStringLiteral(".");
    if (!requireString(object,
                       QStringLiteral("name"),
                       path,
                       &team->name,
                       error) ||
        !requireInt(object,
                    QStringLiteral("score"),
                    path,
                    &team->score,
                    error)) {
        return false;
    }
    const QJsonArray robots =
        requireArray(object, QStringLiteral("robots"), path, error);
    if (!error->isEmpty()) {
        return false;
    }
    for (int index = 0; index < robots.size(); ++index) {
        if (!robots.at(index).isObject()) {
            return fail(error,
                        path + QStringLiteral("robots[%1]").arg(index),
                        QStringLiteral("必须是对象"));
        }
        MatchRobotSnapshot robot;
        if (!parseRobot(robots.at(index).toObject(),
                        side,
                        path + QStringLiteral("robots[%1].").arg(index),
                        &robot,
                        error)) {
            return false;
        }
        team->robots.append(robot);
    }
    return true;
}

bool parseInitialSnapshot(const QJsonObject &root,
                          MatchSnapshot *snapshot,
                          QString *error) {
    const QJsonObject initial =
        requireObject(root, QStringLiteral("initial"), QString(), error);
    if (!error->isEmpty()) {
        return false;
    }
    QString phase;
    if (!requireString(initial,
                       QStringLiteral("matchId"),
                       QStringLiteral("initial."),
                       &snapshot->matchId,
                       error) ||
        !requireString(initial,
                       QStringLiteral("phase"),
                       QStringLiteral("initial."),
                       &phase,
                       error) ||
        !parsePhase(phase,
                    &snapshot->phase,
                    QStringLiteral("initial.phase"),
                    error) ||
        !requireInt(initial,
                    QStringLiteral("timeRemainingSec"),
                    QStringLiteral("initial."),
                    &snapshot->timeRemainingSec,
                    error) ||
        !requireInt(initial,
                    QStringLiteral("round"),
                    QStringLiteral("initial."),
                    &snapshot->round,
                    error) ||
        !parseTeam(initial,
                   QStringLiteral("redTeam"),
                   MatchSide::Red,
                   &snapshot->redTeam,
                   error) ||
        !parseTeam(initial,
                   QStringLiteral("blueTeam"),
                   MatchSide::Blue,
                   &snapshot->blueTeam,
                   error)) {
        return false;
    }
    snapshot->profile = QStringLiteral("rmul3v3");
    return true;
}

bool readPatchInt(const QJsonObject &object,
                  const QString &key,
                  const QString &path,
                  int *target,
                  QString *error) {
    std::optional<int> value;
    if (!optionalInt(object, key, path, &value, error)) {
        return false;
    }
    if (value) {
        *target = *value;
    }
    return true;
}

bool readPatchBool(const QJsonObject &object,
                   const QString &key,
                   const QString &path,
                   bool *target,
                   QString *error) {
    std::optional<bool> value;
    if (!optionalBool(object, key, path, &value, error)) {
        return false;
    }
    if (value) {
        *target = *value;
    }
    return true;
}

bool readPatchReal(const QJsonObject &object,
                   const QString &key,
                   const QString &path,
                   double *target,
                   QString *error) {
    std::optional<double> value;
    if (!optionalReal(object, key, path, &value, error)) {
        return false;
    }
    if (value) {
        *target = *value;
    }
    return true;
}

MatchRobotSnapshot *findRobot(MatchSnapshot *snapshot, const QString &id) {
    for (MatchRobotSnapshot &robot : snapshot->redTeam.robots) {
        if (robot.id == id) {
            return &robot;
        }
    }
    for (MatchRobotSnapshot &robot : snapshot->blueTeam.robots) {
        if (robot.id == id) {
            return &robot;
        }
    }
    return nullptr;
}

bool applyRobotPatch(MatchRobotSnapshot *robot,
                     const QJsonObject &patch,
                     const QString &path,
                     QString *error) {
    if (!readPatchInt(patch, QStringLiteral("health"), path, &robot->health, error) ||
        !readPatchInt(patch,
                      QStringLiteral("maxHealth"),
                      path,
                      &robot->maxHealth,
                      error) ||
        !readPatchInt(patch, QStringLiteral("ammo"), path, &robot->ammo, error) ||
        !readPatchInt(patch, QStringLiteral("heat"), path, &robot->heat, error) ||
        !readPatchInt(patch,
                      QStringLiteral("heatLimit"),
                      path,
                      &robot->heatLimit,
                      error) ||
        !readPatchBool(patch,
                       QStringLiteral("online"),
                       path,
                       &robot->online,
                       error) ||
        !readPatchBool(patch,
                       QStringLiteral("alive"),
                       path,
                       &robot->alive,
                       error) ||
        !readPatchInt(patch,
                      QStringLiteral("respawnRemainingSec"),
                      path,
                      &robot->respawnRemainingSec,
                      error)) {
        return false;
    }

    for (const auto &[key, target] :
         std::initializer_list<std::pair<const char *, double *>>{
             {"superCapPercent", &robot->energy.superCapPercent},
             {"bufferEnergyJ", &robot->energy.bufferEnergyJ},
             {"bufferEnergyLimitJ", &robot->energy.bufferEnergyLimitJ},
             {"chassisEnergyJ", &robot->energy.chassisEnergyJ},
             {"chassisEnergyLimitJ", &robot->energy.chassisEnergyLimitJ},
             {"chassisPowerW", &robot->energy.chassisPowerW},
             {"chassisPowerLimitW", &robot->energy.chassisPowerLimitW},
         }) {
        if (!readPatchReal(patch,
                           QString::fromLatin1(key),
                           path,
                           target,
                           error)) {
            return false;
        }
    }

    QString text;
    if (patch.contains(QStringLiteral("chassisMode"))) {
        text = patch.value(QStringLiteral("chassisMode")).toString();
        if (!parseChassisMode(text,
                              &robot->chassis.mode,
                              path + QStringLiteral("chassisMode"),
                              error)) {
            return false;
        }
    }
    if (patch.contains(QStringLiteral("driveType"))) {
        text = patch.value(QStringLiteral("driveType")).toString();
        if (!parseDriveType(text,
                            &robot->chassis.driveType,
                            path + QStringLiteral("driveType"),
                            error)) {
            return false;
        }
    }
    if (!readPatchBool(patch,
                       QStringLiteral("chassisEnabled"),
                       path,
                       &robot->chassis.enabled,
                       error) ||
        !readPatchBool(patch,
                       QStringLiteral("chassisEnabledReported"),
                       path,
                       &robot->chassis.enabledReported,
                       error) ||
        !readPatchInt(patch,
                      QStringLiteral("overpowerLockRemainingSec"),
                      path,
                      &robot->chassis.overpowerLockRemainingSec,
                      error) ||
        !readPatchBool(patch,
                       QStringLiteral("wheelLegTelemetryAvailable"),
                       path,
                       &robot->wheelLeg.telemetryAvailable,
                       error)) {
        return false;
    }
    if (patch.contains(QStringLiteral("wheelLegMode"))) {
        text = patch.value(QStringLiteral("wheelLegMode")).toString();
        if (!parseWheelLegMode(text,
                               &robot->wheelLeg.mode,
                               path + QStringLiteral("wheelLegMode"),
                               error)) {
            return false;
        }
    }
    for (const auto &[key, target] :
         std::initializer_list<std::pair<const char *, double *>>{
             {"stanceHeightMm", &robot->wheelLeg.stanceHeightMm},
             {"pitchDegrees", &robot->wheelLeg.pitchDegrees},
             {"rollDegrees", &robot->wheelLeg.rollDegrees},
             {"leftWheelSpeedMps", &robot->wheelLeg.leftWheelSpeedMps},
             {"rightWheelSpeedMps", &robot->wheelLeg.rightWheelSpeedMps},
             {"mapX", &robot->mapX},
             {"mapY", &robot->mapY},
             {"headingDegrees", &robot->headingDegrees},
         }) {
        if (!readPatchReal(patch,
                           QString::fromLatin1(key),
                           path,
                           target,
                           error)) {
            return false;
        }
    }
    if (patch.contains(QStringLiteral("mapX")) ||
        patch.contains(QStringLiteral("mapY")) ||
        patch.contains(QStringLiteral("headingDegrees"))) {
        robot->hasPosition = true;
    }
    return true;
}

bool applyAction(MatchSnapshot *snapshot,
                 const QJsonObject &action,
                 const QString &path,
                 quint64 *nextEventSequence,
                 QString *error) {
    QString text;
    if (action.contains(QStringLiteral("phase"))) {
        text = action.value(QStringLiteral("phase")).toString();
        if (!parsePhase(text,
                        &snapshot->phase,
                        path + QStringLiteral("phase"),
                        error)) {
            return false;
        }
    }
    if (!readPatchInt(action,
                      QStringLiteral("timeRemainingSec"),
                      path,
                      &snapshot->timeRemainingSec,
                      error) ||
        !readPatchInt(action,
                      QStringLiteral("redScore"),
                      path,
                      &snapshot->redTeam.score,
                      error) ||
        !readPatchInt(action,
                      QStringLiteral("blueScore"),
                      path,
                      &snapshot->blueTeam.score,
                      error)) {
        return false;
    }

    if (action.contains(QStringLiteral("robotUpdates"))) {
        const QJsonArray updates = requireArray(
            action, QStringLiteral("robotUpdates"), path, error);
        if (!error->isEmpty()) {
            return false;
        }
        for (int index = 0; index < updates.size(); ++index) {
            if (!updates.at(index).isObject()) {
                return fail(error,
                            path + QStringLiteral("robotUpdates[%1]").arg(index),
                            QStringLiteral("必须是对象"));
            }
            const QJsonObject patch = updates.at(index).toObject();
            QString robotId;
            if (!requireString(patch,
                               QStringLiteral("id"),
                               path + QStringLiteral("robotUpdates[%1].").arg(index),
                               &robotId,
                               error)) {
                return false;
            }
            MatchRobotSnapshot *robot = findRobot(snapshot, robotId);
            if (!robot) {
                return fail(error,
                            path + QStringLiteral("robotUpdates[%1].id").arg(index),
                            QStringLiteral("未知机器人：%1").arg(robotId));
            }
            if (!applyRobotPatch(
                    robot,
                    patch,
                    path + QStringLiteral("robotUpdates[%1].").arg(index),
                    error)) {
                return false;
            }
        }
    }

    if (action.contains(QStringLiteral("events"))) {
        const QJsonArray events =
            requireArray(action, QStringLiteral("events"), path, error);
        if (!error->isEmpty()) {
            return false;
        }
        for (int index = 0; index < events.size(); ++index) {
            if (!events.at(index).isObject()) {
                return fail(error,
                            path + QStringLiteral("events[%1]").arg(index),
                            QStringLiteral("必须是对象"));
            }
            const QJsonObject event = events.at(index).toObject();
            const QString eventPath =
                path + QStringLiteral("events[%1].").arg(index);
            MatchEventSnapshot parsed;
            parsed.sequence = (*nextEventSequence)++;
            parsed.occurredAtMs = QDateTime::currentMSecsSinceEpoch();
            if (!requireString(event,
                               QStringLiteral("type"),
                               eventPath,
                               &parsed.type,
                               error) ||
                !requireString(event,
                               QStringLiteral("message"),
                               eventPath,
                               &parsed.message,
                               error)) {
                return false;
            }
            if (event.contains(QStringLiteral("side"))) {
                text = event.value(QStringLiteral("side")).toString();
                if (text == QStringLiteral("red")) {
                    parsed.hasSide = true;
                    parsed.side = MatchSide::Red;
                } else if (text == QStringLiteral("blue")) {
                    parsed.hasSide = true;
                    parsed.side = MatchSide::Blue;
                } else {
                    return fail(error,
                                eventPath + QStringLiteral("side"),
                                QStringLiteral("必须是 red 或 blue"));
                }
            }
            if (!readPatchInt(event,
                              QStringLiteral("robotSlot"),
                              eventPath,
                              &parsed.robotSlot,
                              error)) {
                return false;
            }
            snapshot->events.append(parsed);
        }
    }
    return true;
}

bool parseScenario(const QByteArray &json,
                   MockMatchSourcePrivate *d,
                   QString *error) {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return fail(error,
                    QString(),
                    QStringLiteral("JSON 解析失败：%1")
                        .arg(parseError.errorString()));
    }
    if (!document.isObject()) {
        return fail(error, QString(), QStringLiteral("根节点必须是对象"));
    }
    const QJsonObject root = document.object();
    int schemaVersion = 0;
    if (!requireString(root,
                       QStringLiteral("id"),
                       QString(),
                       &d->id,
                       error) ||
        !requireString(root,
                       QStringLiteral("name"),
                       QString(),
                       &d->name,
                       error) ||
        !requireInt(root,
                    QStringLiteral("schemaVersion"),
                    QString(),
                    &schemaVersion,
                    error) ||
        !requireInt(root,
                    QStringLiteral("durationSec"),
                    QString(),
                    &d->durationSec,
                    error)) {
        return false;
    }
    if (schemaVersion != 1) {
        return fail(error,
                    QStringLiteral("schemaVersion"),
                    QStringLiteral("仅支持版本 1"));
    }
    if (d->durationSec <= 0) {
        return fail(error,
                    QStringLiteral("durationSec"),
                    QStringLiteral("必须大于 0"));
    }
    if (!parseInitialSnapshot(root, &d->initial, error)) {
        return false;
    }

    const QJsonArray timeline =
        requireArray(root, QStringLiteral("timeline"), QString(), error);
    if (!error->isEmpty()) {
        return false;
    }
    for (int index = 0; index < timeline.size(); ++index) {
        if (!timeline.at(index).isObject()) {
            return fail(error,
                        QStringLiteral("timeline[%1]").arg(index),
                        QStringLiteral("必须是对象"));
        }
        const QJsonObject action = timeline.at(index).toObject();
        int atSec = 0;
        if (!requireInt(action,
                        QStringLiteral("atSec"),
                        QStringLiteral("timeline[%1].").arg(index),
                        &atSec,
                        error)) {
            return false;
        }
        if (atSec < 0 || atSec > d->durationSec) {
            return fail(error,
                        QStringLiteral("timeline[%1].atSec").arg(index),
                        QStringLiteral("必须位于 0 到 durationSec 之间"));
        }
        d->actions.append({atSec, action});
    }
    std::stable_sort(
        d->actions.begin(),
        d->actions.end(),
        [](const MockAction &left, const MockAction &right) {
            return left.atSec < right.atSec;
        });

    // 载入时从初始快照顺序预演全部动作，避免运行到一半才发现场景错误。
    MatchSnapshot dryRun = d->initial;
    quint64 sequence = 1;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    dryRun.updatedAtMs = now;
    for (MatchRobotSnapshot &robot : dryRun.redTeam.robots) {
        robot.updatedAtMs = now;
    }
    for (MatchRobotSnapshot &robot : dryRun.blueTeam.robots) {
        robot.updatedAtMs = now;
    }
    for (int index = 0; index < d->actions.size(); ++index) {
        if (!applyAction(&dryRun,
                         d->actions.at(index).object,
                         QStringLiteral("timeline[%1].").arg(index),
                         &sequence,
                         error)) {
            return false;
        }
        const QList<MatchValidationIssue> issues =
            validateMatchSnapshot(dryRun);
        if (hasMatchValidationError(issues)) {
            return fail(error,
                        QStringLiteral("timeline[%1]").arg(index),
                        matchValidationErrorMessage(issues));
        }
    }
    return true;
}

} // namespace

MockMatchSource::MockMatchSource(QObject *parent)
    : QObject(parent),
      d(std::make_unique<MockMatchSourcePrivate>()),
      m_session(new MatchSession(this)) {
    m_timer.setInterval(100);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &MockMatchSource::onTimer);
    connect(m_session,
            &MatchSession::snapshotChanged,
            this,
            &MockMatchSource::snapshotChanged);
}

MockMatchSource::~MockMatchSource() = default;

MatchSession *MockMatchSource::matchSession() const {
    return m_session;
}

QString MockMatchSource::sourceKind() const {
    return QStringLiteral("mock");
}

QString MockMatchSource::sourceName() const {
    return scenarioName();
}

bool MockMatchSource::isAvailable() const {
    return d->loaded && m_session && m_session->isValid();
}

QString MockMatchSource::statusText() const {
    if (!d->loaded) {
        return QStringLiteral("模拟场景未加载");
    }
    return d->completed ? QStringLiteral("模拟比赛已结束")
                        : d->paused ? QStringLiteral("模拟比赛已暂停")
                                    : QStringLiteral("模拟比赛运行中");
}

QVariantMap MockMatchSource::snapshot() const {
    return m_session->snapshot();
}

QString MockMatchSource::scenarioId() const {
    return d->id;
}

QString MockMatchSource::scenarioName() const {
    return d->name;
}

bool MockMatchSource::isLoaded() const {
    return d->loaded;
}

bool MockMatchSource::isRunning() const {
    return d->running && !d->paused;
}

bool MockMatchSource::isPaused() const {
    return d->paused;
}

bool MockMatchSource::isCompleted() const {
    return d->completed;
}

double MockMatchSource::playbackRate() const {
    return d->playbackRate;
}

int MockMatchSource::elapsedSec() const {
    return static_cast<int>(d->elapsedMs / 1000);
}

QString MockMatchSource::lastError() const {
    return d->lastError;
}

bool MockMatchSource::loadFromFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        reportError(QStringLiteral("无法读取模拟场景 %1：%2")
                        .arg(path, file.errorString()));
        return false;
    }
    return loadFromJson(file.readAll(), path);
}

bool MockMatchSource::loadFromJson(const QByteArray &json,
                                   const QString &sourceName) {
    m_timer.stop();
    auto parsed = std::make_unique<MockMatchSourcePrivate>();
    QString error;
    if (!parseScenario(json, parsed.get(), &error)) {
        reportError(sourceName.isEmpty()
                        ? error
                        : QStringLiteral("%1: %2").arg(sourceName, error));
        return false;
    }
    parsed->loaded = true;
    d = std::move(parsed);
    clearError();
    initializeScenarioSnapshot();
    emit scenarioLoaded();
    markStateChanged();
    emit clockChanged();
    return m_session->isValid();
}

bool MockMatchSource::start() {
    if (!d->loaded || d->completed) {
        return false;
    }
    if (d->paused) {
        return resume();
    }
    if (d->running) {
        return true;
    }
    d->running = true;
    m_wallClock.start();
    m_lastWallClockMs = 0;
    m_timer.start();
    markStateChanged();
    return true;
}

bool MockMatchSource::pause() {
    if (!d->running || d->paused) {
        return false;
    }
    d->paused = true;
    m_timer.stop();
    markStateChanged();
    return true;
}

bool MockMatchSource::resume() {
    if (!d->loaded || !d->paused || d->completed) {
        return false;
    }
    d->paused = false;
    d->running = true;
    m_wallClock.start();
    m_lastWallClockMs = 0;
    m_timer.start();
    markStateChanged();
    return true;
}

bool MockMatchSource::reset() {
    if (!d->loaded) {
        return false;
    }
    m_timer.stop();
    d->running = false;
    d->paused = false;
    d->completed = false;
    d->virtualCarryMs = 0.0;
    d->elapsedMs = 0;
    d->nextEventSequence = 1;
    d->appliedActionIndexes.clear();
    initializeScenarioSnapshot();
    markStateChanged();
    emit clockChanged();
    return m_session->isValid();
}

bool MockMatchSource::restart() {
    return reset() && start();
}

bool MockMatchSource::togglePause() {
    if (!d->loaded || d->completed) {
        return false;
    }
    if (d->paused) {
        return resume();
    }
    if (d->running) {
        return pause();
    }
    return start();
}

bool MockMatchSource::cycleScenario(int direction) {
    static const QStringList scenarios{
        QStringLiteral("match_3v3_basic.json"),
        QStringLiteral("match_3v3_damage.json"),
        QStringLiteral("match_3v3_offline.json"),
        QStringLiteral("match_3v3_settlement.json"),
    };

    int currentIndex = -1;
    for (int index = 0; index < scenarios.size(); ++index) {
        if (d->id ==
            QFileInfo(scenarios.at(index)).completeBaseName()) {
            currentIndex = index;
            break;
        }
    }
    if (currentIndex < 0) {
        currentIndex = 0;
    } else {
        const int step = direction < 0 ? -1 : 1;
        currentIndex =
            (currentIndex + step + scenarios.size()) % scenarios.size();
    }
    const QString path =
        QStringLiteral(":/mock/3v3/") + scenarios.at(currentIndex);
    return loadFromFile(path) && start();
}

bool MockMatchSource::selectScenario(const QString &scenario) {
    static const QStringList supported{
        QStringLiteral("match_3v3_basic"),
        QStringLiteral("match_3v3_damage"),
        QStringLiteral("match_3v3_offline"),
        QStringLiteral("match_3v3_settlement"),
    };
    QString id = QFileInfo(scenario).completeBaseName();
    if (id.isEmpty()) {
        id = scenario;
    }
    if (!supported.contains(id)) {
        reportError(QStringLiteral("未知模拟场景：%1").arg(scenario));
        return false;
    }
    return loadFromFile(
               QStringLiteral(":/mock/3v3/%1.json").arg(id)) &&
        start();
}

void MockMatchSource::setPlaybackRate(double rate) {
    const double bounded = std::clamp(rate, 0.25, 4.0);
    if (qFuzzyCompare(d->playbackRate, bounded)) {
        return;
    }
    d->playbackRate = bounded;
    markStateChanged();
}

bool MockMatchSource::advanceByMs(qint64 elapsedMs) {
    if (!d->running || d->paused || elapsedMs < 0) {
        return false;
    }

    d->virtualCarryMs +=
        static_cast<double>(elapsedMs) * d->playbackRate;
    int fullSeconds =
        static_cast<int>(std::floor(d->virtualCarryMs / 1000.0));
    if (fullSeconds <= 0) {
        return true;
    }
    d->virtualCarryMs -= fullSeconds * 1000.0;

    for (int step = 0; step < fullSeconds; ++step) {
        d->elapsedMs += 1000;
        MatchSnapshot snapshot = m_session->matchSnapshot();
        if (snapshot.timeRemainingSec > 0) {
            --snapshot.timeRemainingSec;
        }
        snapshot.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
        QString error;
        if (!m_session->replaceSnapshot(snapshot, &error)) {
            reportError(error);
            return false;
        }
        if (!applyActionsAtElapsedSecond(
                static_cast<int>(d->elapsedMs / 1000))) {
            return false;
        }
        emit clockChanged();

        if (d->elapsedMs >= d->durationSec * 1000LL) {
            d->running = false;
            d->paused = false;
            d->completed = true;
            m_timer.stop();
            markStateChanged();
            emit scenarioCompleted();
            break;
        }
    }
    return true;
}

void MockMatchSource::onTimer() {
    if (!d->running || d->paused) {
        return;
    }
    const qint64 now = m_wallClock.elapsed();
    const qint64 delta = qMax<qint64>(0, now - m_lastWallClockMs);
    m_lastWallClockMs = now;
    if (delta > 0) {
        advanceByMs(delta);
    }
}

bool MockMatchSource::applyActionsAtElapsedSecond(int elapsedSec) {
    MatchSnapshot snapshot = m_session->matchSnapshot();
    bool changed = false;
    for (int index = 0; index < d->actions.size(); ++index) {
        const MockAction &action = d->actions.at(index);
        if (action.atSec != elapsedSec ||
            d->appliedActionIndexes.contains(index)) {
            continue;
        }
        d->appliedActionIndexes.insert(index);
        changed = true;
        QString error;
        if (!applyAction(&snapshot,
                         action.object,
                         QStringLiteral("timeline[%1].").arg(index),
                         &d->nextEventSequence,
                         &error)) {
            reportError(error);
            return false;
        }
    }
    if (!changed) {
        return true;
    }
    snapshot.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
    QString error;
    if (!m_session->replaceSnapshot(snapshot, &error)) {
        reportError(error);
        return false;
    }
    return true;
}

void MockMatchSource::initializeScenarioSnapshot() {
    MatchSnapshot snapshot = d->initial;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    snapshot.updatedAtMs = now;
    for (MatchRobotSnapshot &robot : snapshot.redTeam.robots) {
        robot.updatedAtMs = now;
    }
    for (MatchRobotSnapshot &robot : snapshot.blueTeam.robots) {
        robot.updatedAtMs = now;
    }
    QString error;
    if (!m_session->replaceSnapshot(snapshot, &error)) {
        reportError(error);
        return;
    }
    applyActionsAtElapsedSecond(0);
}

void MockMatchSource::clearError() {
    if (d->lastError.isEmpty()) {
        return;
    }
    d->lastError.clear();
    emit errorOccurred({});
}

void MockMatchSource::reportError(const QString &message) {
    d->lastError = message;
    emit errorOccurred(message);
}

void MockMatchSource::markStateChanged() {
    emit stateChanged();
}

} // namespace RM
