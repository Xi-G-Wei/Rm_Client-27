#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtGlobal>

namespace RM {

// 与通信协议无关的 3v3 比赛领域模型。Mock、回放和后续真实协议适配器
// 都应先生成 MatchSnapshot，再交给界面层消费。
enum class MatchPhase {
    NotStarted,
    Preparation,
    Battle,
    Settlement,
};

enum class MatchSide {
    Red,
    Blue,
};

enum class MatchRobotRole {
    Heavy,
    Infantry,
    Engineer,
    Aerial,
    Sentry,
    Unknown,
};

QString matchPhaseKey(MatchPhase phase);
QString matchPhaseLabel(MatchPhase phase);
QString matchSideKey(MatchSide side);
QString matchRobotRoleKey(MatchRobotRole role);
QString matchRobotRoleLabel(MatchRobotRole role);

// -1 表示未知值，不能直接当成 0 显示给操作手。
constexpr int kUnknownMatchMetric = -1;

struct MatchRobotSnapshot {
    QString id;
    int slot = 0;
    MatchSide side = MatchSide::Red;
    MatchRobotRole role = MatchRobotRole::Unknown;
    int health = kUnknownMatchMetric;
    int maxHealth = kUnknownMatchMetric;
    int ammo = kUnknownMatchMetric;
    int heat = kUnknownMatchMetric;
    int heatLimit = kUnknownMatchMetric;
    int chassisPower = kUnknownMatchMetric;
    bool online = false;
    bool alive = false;
    int respawnRemainingSec = kUnknownMatchMetric;
    bool hasPosition = false;
    double mapX = 0.0;
    double mapY = 0.0;
    double headingDegrees = 0.0;
    qint64 updatedAtMs = 0;

    QVariantMap toVariantMap() const;
};

struct MatchTeamSnapshot {
    QString name;
    int score = 0;
    QList<MatchRobotSnapshot> robots;

    QVariantMap toVariantMap() const;
};

struct MatchEventSnapshot {
    quint64 sequence = 0;
    qint64 occurredAtMs = 0;
    QString type;
    QString message;
    bool hasSide = false;
    MatchSide side = MatchSide::Red;
    int robotSlot = -1;

    QVariantMap toVariantMap() const;
};

struct MatchSnapshot {
    QString matchId;
    QString profile = QStringLiteral("rmul3v3");
    MatchPhase phase = MatchPhase::NotStarted;
    int timeRemainingSec = 0;
    int round = 1;
    MatchTeamSnapshot redTeam;
    MatchTeamSnapshot blueTeam;
    QList<MatchEventSnapshot> events;
    qint64 updatedAtMs = 0;

    QVariantMap toVariantMap() const;
};

enum class MatchValidationSeverity {
    Warning,
    Error,
};

struct MatchValidationIssue {
    MatchValidationSeverity severity = MatchValidationSeverity::Error;
    QString path;
    QString message;
};

// 校验的是数据契约，不是具体赛事规则。未确认的字段允许使用 -1。
QList<MatchValidationIssue> validateMatchSnapshot(const MatchSnapshot &snapshot);
bool hasMatchValidationError(const QList<MatchValidationIssue> &issues);
QString matchValidationErrorMessage(const QList<MatchValidationIssue> &issues);

// QML 入口。只有校验通过的新快照才会替换当前快照，避免界面收到半份状态。
class MatchSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)
    Q_PROPERTY(bool valid READ isValid NOTIFY validationChanged)
    Q_PROPERTY(QString validationError READ validationError NOTIFY validationChanged)

public:
    explicit MatchSession(QObject *parent = nullptr);

    const MatchSnapshot &matchSnapshot() const;
    QVariantMap snapshot() const;
    bool isValid() const;
    QString validationError() const;
    QList<MatchValidationIssue> validationIssues() const;

    bool replaceSnapshot(const MatchSnapshot &snapshot,
                         QString *errorMessage = nullptr);
    void clear();

signals:
    void snapshotChanged();
    void validationChanged();

private:
    MatchSnapshot m_snapshot;
    QList<MatchValidationIssue> m_validationIssues;
    bool m_valid = false;
};

} // namespace RM

Q_DECLARE_METATYPE(RM::MatchSnapshot)
