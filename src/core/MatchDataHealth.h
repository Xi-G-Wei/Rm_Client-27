#pragma once

#include "core/MatchSession.h"

#include <QString>
#include <QtGlobal>

namespace RM {

enum class MatchDataHealthLevel {
    Unavailable,
    Fresh,
    Stale,
    Expired,
};

struct MatchDataHealth {
    MatchDataHealthLevel level = MatchDataHealthLevel::Unavailable;
    qint64 ageMs = -1;
    int onlineRobots = 0;
    int offlineRobots = 0;
    int publicRobots = 0;
    int privateRobots = 0;
    int unknownFieldCount = 0;
};

MatchDataHealth assessMatchDataHealth(
    const MatchSnapshot &snapshot,
    qint64 nowMs,
    qint64 freshThresholdMs = 1500,
    qint64 staleThresholdMs = 5000);
QString matchDataHealthKey(MatchDataHealthLevel level);
QString matchDataHealthLabel(const MatchDataHealth &health);

} // namespace RM
