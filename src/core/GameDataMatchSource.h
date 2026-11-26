#pragma once

#include "core/MatchDataSource.h"
#include "core/GameData.h"
#include "core/MatchSession.h"

#include <QObject>
#include <QList>
#include <QTimer>

namespace RM {

// 将现有 MQTT/NetworkManager 已经写入的 GameData 投影成 MatchSnapshot。
// 未来通信协议变化时只需要保证 GameData 被正确更新，新的 QML 不需要改。
class GameDataMatchSource : public QObject, public MatchDataSource {
    Q_OBJECT
    Q_PROPERTY(RM::MatchSession *matchSession READ matchSession CONSTANT)
    Q_PROPERTY(bool active READ isAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)

public:
    explicit GameDataMatchSource(::GameData *gameData,
                                 QObject *parent = nullptr);

    void refresh();

    MatchSession *matchSession() const override;
    QString sourceKind() const override;
    QString sourceName() const override;
    bool isAvailable() const override;
    QString statusText() const override;

signals:
    void stateChanged();

private:
    MatchSnapshot buildSnapshot(QString *error);
    MatchPhase mapPhase(::GameStage stage) const;
    void appendEvent(const QString &type,
                     const QString &message,
                     bool hasSide = false,
                     MatchSide side = MatchSide::Red,
                     int robotSlot = -1);

    ::GameData *m_gameData = nullptr;
    MatchSession *m_session = nullptr;
    QTimer m_timer;
    QString m_lastError;
    QList<MatchEventSnapshot> m_events;
    quint64 m_nextEventSequence = 1;
    qint64 m_lastDataAtMs = 0;
};

} // namespace RM
