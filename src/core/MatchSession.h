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
    SelfCheck,
    Countdown,
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

enum class MatchChassisMode {
    Unknown,
    Normal,
    Follow,
    Spin,
    Protect,
};

enum class MatchChassisDriveType {
    Unknown,
    Wheel,
    WheelLeg,
};

enum class MatchWheelLegMode {
    Unknown,
    Wheel,
    Leg,
    Transition,
    Balance,
};

enum class MatchTelemetryScope {
    Public,
    Private,
};

QString matchPhaseKey(MatchPhase phase);
QString matchPhaseLabel(MatchPhase phase);
QString matchSideKey(MatchSide side);
QString matchRobotRoleKey(MatchRobotRole role);
QString matchRobotRoleLabel(MatchRobotRole role);
QString matchChassisModeKey(MatchChassisMode mode);
QString matchChassisModeLabel(MatchChassisMode mode);
QString matchChassisDriveTypeKey(MatchChassisDriveType type);
QString matchChassisDriveTypeLabel(MatchChassisDriveType type);
QString matchWheelLegModeKey(MatchWheelLegMode mode);
QString matchWheelLegModeLabel(MatchWheelLegMode mode);
QString matchTelemetryScopeKey(MatchTelemetryScope scope);
QString matchTelemetryScopeLabel(MatchTelemetryScope scope);

// -1 表示未知值，不能直接当成 0 显示给操作手。
constexpr int kUnknownMatchMetric = -1;
constexpr double kUnknownMatchReal = -1.0;

// 功率统一使用 W，能量统一使用 J，超级电容使用 0-100%。
// 字段拆分后，界面不再需要猜测 chassisPower 是功率还是能量。
struct MatchEnergySnapshot {
    double superCapPercent = kUnknownMatchReal;
    double bufferEnergyJ = kUnknownMatchReal;
    double bufferEnergyLimitJ = kUnknownMatchReal;
    double chassisEnergyJ = kUnknownMatchReal;
    double chassisEnergyLimitJ = kUnknownMatchReal;
    double chassisPowerW = kUnknownMatchReal;
    double chassisPowerLimitW = kUnknownMatchReal;

    QVariantMap toVariantMap() const;
};

struct MatchChassisSnapshot {
    MatchChassisMode mode = MatchChassisMode::Unknown;
    MatchChassisDriveType driveType = MatchChassisDriveType::Unknown;
    bool enabled = false;
    bool enabledReported = false;
    int overpowerLockRemainingSec = kUnknownMatchMetric;

    QVariantMap toVariantMap() const;
};

struct MatchWheelLegJointSnapshot {
    QString name;
    double angleDegrees = kUnknownMatchReal;
    double targetAngleDegrees = kUnknownMatchReal;

    QVariantMap toVariantMap() const;
};

struct MatchWheelLegSnapshot {
    bool telemetryAvailable = false;
    MatchWheelLegMode mode = MatchWheelLegMode::Unknown;
    double stanceHeightMm = kUnknownMatchReal;
    double pitchDegrees = kUnknownMatchReal;
    double rollDegrees = kUnknownMatchReal;
    double leftWheelSpeedMps = kUnknownMatchReal;
    double rightWheelSpeedMps = kUnknownMatchReal;
    QList<MatchWheelLegJointSnapshot> joints;

    QVariantMap toVariantMap() const;
};

struct MatchRobotSnapshot {
    QString id;
    int slot = 0;
    MatchSide side = MatchSide::Red;
    MatchRobotRole role = MatchRobotRole::Unknown;
    MatchTelemetryScope telemetryScope = MatchTelemetryScope::Public;
    int health = kUnknownMatchMetric;
    int maxHealth = kUnknownMatchMetric;
    int ammo = kUnknownMatchMetric;
    int heat = kUnknownMatchMetric;
    int heatLimit = kUnknownMatchMetric;
    int level = kUnknownMatchMetric;
    int experience = kUnknownMatchMetric;
    int maxExperience = kUnknownMatchMetric;
    int yellowCardCount = 0;
    double muzzleVelocityMps = kUnknownMatchReal;
    double shootSpeedLimitMps = kUnknownMatchReal;
    int speedLockState = 0;
    int speedLockSeconds = 0;
    MatchEnergySnapshot energy;
    MatchChassisSnapshot chassis;
    MatchWheelLegSnapshot wheelLeg;
    QVariantList moduleStates;
    QVariantList moduleData;
    QVariantMap buffTimedData;
    QVariantMap customData;
    bool online = false;
    bool alive = false;
    bool outOfCombat = true;
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
    int economy = kUnknownMatchMetric;
    int totalEconomyObtained = kUnknownMatchMetric;
    int techLevel = kUnknownMatchMetric;
    int encryptionLevel = kUnknownMatchMetric;
    int baseHealth = kUnknownMatchMetric;
    int baseMaxHealth = kUnknownMatchMetric;
    int baseStatus = kUnknownMatchMetric;
    int outpostHealth = kUnknownMatchMetric;
    int outpostMaxHealth = kUnknownMatchMetric;
    int outpostStatus = kUnknownMatchMetric;
    int controlPoints = kUnknownMatchMetric;
    int totalDamage = kUnknownMatchMetric;
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
    bool isPaused = false;
    int timeRemainingSec = 0;
    int round = 1;
    QString operatorRobotId = QStringLiteral("red-3");
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
