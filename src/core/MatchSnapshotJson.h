#pragma once

#include "core/MatchSession.h"

#include <QJsonObject>
#include <QString>

namespace RM {

QJsonObject matchSnapshotToJson(const MatchSnapshot &snapshot);
bool matchSnapshotFromJson(const QJsonObject &object,
                           MatchSnapshot *snapshot,
                           QString *error = nullptr);

} // namespace RM
