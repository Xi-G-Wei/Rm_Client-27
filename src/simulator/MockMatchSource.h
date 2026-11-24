#pragma once

#include "core/MatchSession.h"
#include "core/MatchDataSource.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantMap>

#include <memory>

namespace RM {

class MockMatchSourcePrivate;

// JSON 场景驱动的本地比赛源。界面只读取 MatchSession，后续真实协议适配器
// 可以替换这个数据源而不改变 QML 契约。
class MockMatchSource : public QObject, public MatchDataSource {
    Q_OBJECT
    Q_PROPERTY(RM::MatchSession *matchSession READ matchSession CONSTANT)
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)
    Q_PROPERTY(QString scenarioId READ scenarioId NOTIFY scenarioLoaded)
    Q_PROPERTY(QString scenarioName READ scenarioName NOTIFY scenarioLoaded)
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY stateChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY stateChanged)
    Q_PROPERTY(bool paused READ isPaused NOTIFY stateChanged)
    Q_PROPERTY(bool completed READ isCompleted NOTIFY stateChanged)
    Q_PROPERTY(double playbackRate READ playbackRate NOTIFY stateChanged)
    Q_PROPERTY(int elapsedSec READ elapsedSec NOTIFY clockChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)

public:
    explicit MockMatchSource(QObject *parent = nullptr);
    ~MockMatchSource() override;

    MatchSession *matchSession() const;
    QString sourceKind() const override;
    QString sourceName() const override;
    bool isAvailable() const override;
    QString statusText() const override;
    QVariantMap snapshot() const;
    QString scenarioId() const;
    QString scenarioName() const;
    bool isLoaded() const;
    bool isRunning() const;
    bool isPaused() const;
    bool isCompleted() const;
    double playbackRate() const;
    int elapsedSec() const;
    QString lastError() const;

    bool loadFromFile(const QString &path);
    bool loadFromJson(const QByteArray &json,
                      const QString &sourceName = QString());
    Q_INVOKABLE bool start();
    Q_INVOKABLE bool pause();
    Q_INVOKABLE bool resume();
    Q_INVOKABLE bool reset();
    Q_INVOKABLE bool restart();
    Q_INVOKABLE void setPlaybackRate(double rate);
    Q_INVOKABLE bool togglePause();
    Q_INVOKABLE bool cycleScenario(int direction = 1);
    Q_INVOKABLE bool selectScenario(const QString &scenario);
    Q_INVOKABLE bool setTriggerHeld(bool held);

    // 测试和回放接口：显式推进真实时间，不受系统定时器抖动影响。
    bool advanceByMs(qint64 elapsedMs);

signals:
    void scenarioLoaded();
    void snapshotChanged();
    void stateChanged();
    void clockChanged();
    void scenarioCompleted();
    void errorOccurred(const QString &message);

private slots:
    void onTimer();

private:
    bool applyActionsAtElapsedSecond(int elapsedSec);
    bool applyLocalCombatStep(double virtualMs);
    bool fireImmediateShot();
    void initializeScenarioSnapshot();
    void clearError();
    void reportError(const QString &message);
    void markStateChanged();

    std::unique_ptr<MockMatchSourcePrivate> d;
    MatchSession *m_session = nullptr;
    QTimer m_timer;
    QElapsedTimer m_wallClock;
    qint64 m_lastWallClockMs = 0;
    bool m_triggerHeld = false;
    double m_fireAccumulatorMs = 0.0;
    double m_coolingAccumulatorMs = 0.0;
};

} // namespace RM
