#pragma once

#include "core/MatchDataSource.h"
#include "core/MatchSession.h"

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QTimer>

namespace RM {

class MatchReplaySource : public QObject, public MatchDataSource {
    Q_OBJECT
    Q_PROPERTY(RM::MatchSession *matchSession READ matchSession CONSTANT)
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY stateChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY stateChanged)
    Q_PROPERTY(bool paused READ isPaused NOTIFY stateChanged)
    Q_PROPERTY(bool completed READ isCompleted NOTIFY stateChanged)
    Q_PROPERTY(double playbackRate READ playbackRate NOTIFY stateChanged)
    Q_PROPERTY(int currentFrame READ currentFrame NOTIFY frameChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY frameChanged)

public:
    explicit MatchReplaySource(QObject *parent = nullptr);

    bool loadFromFile(const QString &path);
    Q_INVOKABLE bool start();
    Q_INVOKABLE bool pause();
    Q_INVOKABLE bool resume();
    Q_INVOKABLE bool reset();
    Q_INVOKABLE void setPlaybackRate(double rate);
    bool advanceByMs(qint64 elapsedMs);

    MatchSession *matchSession() const override;
    QString sourceKind() const override;
    QString sourceName() const override;
    bool isAvailable() const override;
    QString statusText() const override;

    bool isLoaded() const;
    bool isRunning() const;
    bool isPaused() const;
    bool isCompleted() const;
    double playbackRate() const;
    int currentFrame() const;
    int frameCount() const;

signals:
    void stateChanged();
    void frameChanged();
    void errorOccurred(const QString &message);

private slots:
    void onTimer();

private:
    bool applyFrame(int index);

    struct Frame {
        qint64 offsetMs = 0;
        MatchSnapshot snapshot;
    };

    MatchSession *m_session = nullptr;
    QList<Frame> m_frames;
    QString m_name;
    QString m_error;
    QTimer m_timer;
    QElapsedTimer m_wallClock;
    qint64 m_lastWallMs = 0;
    double m_carryMs = 0.0;
    double m_rate = 1.0;
    qint64 m_elapsedMs = 0;
    int m_nextFrame = 0;
    bool m_running = false;
    bool m_paused = false;
    bool m_completed = false;
};

} // namespace RM
