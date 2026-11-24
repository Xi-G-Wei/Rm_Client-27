#include "core/MatchDataHealth.h"

namespace RM {

MatchDataHealth assessMatchDataHealth(const MatchSnapshot &snapshot,
                                      qint64 nowMs,
                                      qint64 freshThresholdMs,
                                      qint64 staleThresholdMs) {
    MatchDataHealth health;
    if (snapshot.updatedAtMs <= 0 ||
        (snapshot.redTeam.robots.isEmpty() &&
         snapshot.blueTeam.robots.isEmpty())) {
        return health;
    }

    health.ageMs = qMax<qint64>(0, nowMs - snapshot.updatedAtMs);
    health.level = health.ageMs <= freshThresholdMs
        ? MatchDataHealthLevel::Fresh
        : health.ageMs <= staleThresholdMs
            ? MatchDataHealthLevel::Stale
            : MatchDataHealthLevel::Expired;

    const auto inspectTeam = [&](const MatchTeamSnapshot &team) {
        for (const MatchRobotSnapshot &robot : team.robots) {
            if (robot.online)
                ++health.onlineRobots;
            else
                ++health.offlineRobots;

            if (robot.telemetryScope == MatchTelemetryScope::Private)
                ++health.privateRobots;
            else
                ++health.publicRobots;

            for (const int value :
                 {robot.health, robot.maxHealth, robot.ammo,
                  robot.heat, robot.heatLimit}) {
                if (value == kUnknownMatchMetric)
                    ++health.unknownFieldCount;
            }
            if (!robot.hasPosition)
                ++health.unknownFieldCount;

            if (robot.telemetryScope == MatchTelemetryScope::Private) {
                for (const double value :
                     {robot.energy.superCapPercent,
                      robot.energy.bufferEnergyJ,
                      robot.energy.chassisPowerW}) {
                    if (value == kUnknownMatchReal)
                        ++health.unknownFieldCount;
                }
            }
        }
    };
    inspectTeam(snapshot.redTeam);
    inspectTeam(snapshot.blueTeam);
    return health;
}

QString matchDataHealthKey(MatchDataHealthLevel level) {
    switch (level) {
    case MatchDataHealthLevel::Fresh: return QStringLiteral("fresh");
    case MatchDataHealthLevel::Stale: return QStringLiteral("stale");
    case MatchDataHealthLevel::Expired: return QStringLiteral("expired");
    case MatchDataHealthLevel::Unavailable: return QStringLiteral("unavailable");
    }
    return QStringLiteral("unavailable");
}

QString matchDataHealthLabel(const MatchDataHealth &health) {
    switch (health.level) {
    case MatchDataHealthLevel::Fresh:
        return QStringLiteral("数据正常 · %1 ms").arg(health.ageMs);
    case MatchDataHealthLevel::Stale:
        return QStringLiteral("数据延迟 · %1 ms").arg(health.ageMs);
    case MatchDataHealthLevel::Expired:
        return QStringLiteral("数据已过期 · %1 ms").arg(health.ageMs);
    case MatchDataHealthLevel::Unavailable:
        return QStringLiteral("暂无比赛数据");
    }
    return QStringLiteral("暂无比赛数据");
}

} // namespace RM
