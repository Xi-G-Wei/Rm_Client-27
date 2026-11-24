#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace RM {

class MatchSession;
struct MatchRobotSnapshot;

// 将协议无关的 MatchSnapshot 投影成现有战术页面需要的字段。
// 后续真实协议接入时也应通过同一投影，而不是让 QML 解释原始报文。
class MatchUiProjection : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY updated)
    Q_PROPERTY(bool mockMode READ mockMode CONSTANT)
    Q_PROPERTY(bool replayMode READ replayMode CONSTANT)
    Q_PROPERTY(QString sourceKind READ sourceKind CONSTANT)
    Q_PROPERTY(QString dataHealth READ dataHealth NOTIFY updated)
    Q_PROPERTY(int dataAgeMs READ dataAgeMs NOTIFY updated)
    Q_PROPERTY(int unknownFieldCount READ unknownFieldCount NOTIFY updated)
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY updated)
    Q_PROPERTY(QVariantMap dashboardData READ dashboardData NOTIFY updated)
    Q_PROPERTY(QVariantMap headerData READ headerData NOTIFY updated)
    Q_PROPERTY(QVariantMap resourceData READ resourceData NOTIFY updated)
    Q_PROPERTY(QVariantMap topStatusData READ topStatusData NOTIFY updated)
    Q_PROPERTY(QVariantList keyEvents READ keyEvents NOTIFY updated)
    Q_PROPERTY(QVariantList allyRobotList READ allyRobotList NOTIFY updated)
    Q_PROPERTY(QVariantList enemyRobotList READ enemyRobotList NOTIFY updated)
    Q_PROPERTY(QVariantMap mapData READ mapData NOTIFY updated)
    Q_PROPERTY(QVariantMap cameraPreviewData READ cameraPreviewData NOTIFY updated)
    Q_PROPERTY(QVariantMap linkHealth READ linkHealth NOTIFY updated)
    Q_PROPERTY(QVariantList analysisMetrics READ analysisMetrics NOTIFY updated)
    Q_PROPERTY(QVariantMap mainDecision READ mainDecision NOTIFY updated)
    Q_PROPERTY(QVariantMap predictionData READ predictionData NOTIFY updated)
    Q_PROPERTY(QString layoutMode READ layoutMode NOTIFY updated)

    // DashboardMatchHeader 使用的 GameData 兼容字段。
    Q_PROPERTY(bool hasBases READ hasBases CONSTANT)
    Q_PROPERTY(bool hasControlPoints READ hasControlPoints CONSTANT)
    Q_PROPERTY(double fieldWidth READ fieldWidth CONSTANT)
    Q_PROPERTY(double fieldHeight READ fieldHeight CONSTANT)
    Q_PROPERTY(int remainingTime READ remainingTime NOTIFY updated)
    Q_PROPERTY(int redScore READ redScore NOTIFY updated)
    Q_PROPERTY(int blueScore READ blueScore NOTIFY updated)
    Q_PROPERTY(int currentRound READ currentRound NOTIFY updated)
    Q_PROPERTY(QString gamePhase READ gamePhase NOTIFY updated)
    Q_PROPERTY(bool is_paused READ isPaused NOTIFY updated)
    Q_PROPERTY(bool uses2027Protocol READ uses2027Protocol CONSTANT)
    Q_PROPERTY(QVariantMap territoryStatus READ territoryStatus NOTIFY updated)
    Q_PROPERTY(QString redTeamName READ redTeamName NOTIFY updated)
    Q_PROPERTY(QString blueTeamName READ blueTeamName NOTIFY updated)
    Q_PROPERTY(int redBaseHealth READ zeroInt NOTIFY updated)
    Q_PROPERTY(int redBaseMaxHealth READ zeroInt NOTIFY updated)
    Q_PROPERTY(int blueBaseHealth READ zeroInt NOTIFY updated)
    Q_PROPERTY(int blueBaseMaxHealth READ zeroInt NOTIFY updated)
    Q_PROPERTY(int redOutpostHealth READ zeroInt NOTIFY updated)
    Q_PROPERTY(int redOutpostMaxHealth READ zeroInt NOTIFY updated)
    Q_PROPERTY(int blueOutpostHealth READ zeroInt NOTIFY updated)
    Q_PROPERTY(int blueOutpostMaxHealth READ zeroInt NOTIFY updated)
    Q_PROPERTY(int redOutpostRebuildCount READ zeroInt NOTIFY updated)
    Q_PROPERTY(int blueOutpostRebuildCount READ zeroInt NOTIFY updated)
    Q_PROPERTY(int redOutpostMaxRebuildCount READ zeroInt NOTIFY updated)
    Q_PROPERTY(int blueOutpostMaxRebuildCount READ zeroInt NOTIFY updated)
    Q_PROPERTY(int redEconomy READ zeroInt NOTIFY updated)
    Q_PROPERTY(int blueEconomy READ zeroInt NOTIFY updated)
    Q_PROPERTY(int redDefenseBonus READ zeroInt NOTIFY updated)
    Q_PROPERTY(int blueDefenseBonus READ zeroInt NOTIFY updated)
    Q_PROPERTY(bool redBaseInvincible READ falseValue NOTIFY updated)
    Q_PROPERTY(bool blueBaseInvincible READ falseValue NOTIFY updated)
    Q_PROPERTY(int currentHealth READ currentHealth NOTIFY updated)
    Q_PROPERTY(int maxHealth READ maxHealth NOTIFY updated)
    Q_PROPERTY(int chassisEnergy READ chassisEnergy NOTIFY updated)
    Q_PROPERTY(int maxChassisEnergy READ maxChassisEnergy NOTIFY updated)
    Q_PROPERTY(int bufferEnergy READ bufferEnergy NOTIFY updated)
    Q_PROPERTY(int maxBufferEnergy READ maxBufferEnergy NOTIFY updated)
    Q_PROPERTY(double superCapEnergy READ superCapEnergy NOTIFY updated)
    Q_PROPERTY(int robotLevel READ robotLevel NOTIFY updated)
    Q_PROPERTY(int maxlevel READ maxlevel NOTIFY updated)
    Q_PROPERTY(int robotId READ robotId NOTIFY updated)
    Q_PROPERTY(int currentExp READ currentExp NOTIFY updated)
    Q_PROPERTY(int maxExp READ maxExp NOTIFY updated)
    Q_PROPERTY(int yellowCardCount READ yellowCardCount NOTIFY updated)
    Q_PROPERTY(QString outOfCombatStatus READ outOfCombatStatus NOTIFY updated)
    Q_PROPERTY(QString chassisStatus READ chassisStatus NOTIFY updated)
    Q_PROPERTY(bool isOutOfCombat READ isOutOfCombat NOTIFY updated)
    Q_PROPERTY(QVariantList moduleStates READ moduleStates NOTIFY updated)
    Q_PROPERTY(QVariantList moduleData READ moduleData NOTIFY updated)
    Q_PROPERTY(QVariantMap buffTimedData READ buffTimedData NOTIFY updated)
    Q_PROPERTY(QVariantMap myRobot READ myRobot NOTIFY updated)

public:
    explicit MatchUiProjection(MatchSession *session,
                               QObject *parent = nullptr,
                               const QString &sourceKind = QStringLiteral("mock"));

    bool active() const;
    bool mockMode() const;
    bool replayMode() const;
    QString sourceKind() const;
    QString dataHealth() const;
    int dataAgeMs() const;
    int unknownFieldCount() const;
    QVariantMap snapshot() const;
    QVariantMap dashboardData() const;
    QVariantMap headerData() const;
    QVariantMap resourceData() const;
    QVariantMap topStatusData() const;
    QVariantList keyEvents() const;
    QVariantList allyRobotList() const;
    QVariantList enemyRobotList() const;
    QVariantMap mapData() const;
    QVariantMap cameraPreviewData() const;
    QVariantMap linkHealth() const;
    QVariantList analysisMetrics() const;
    QVariantMap mainDecision() const;
    QVariantMap predictionData() const;
    QString layoutMode() const;

    bool hasBases() const;
    bool hasControlPoints() const;
    double fieldWidth() const;
    double fieldHeight() const;
    int remainingTime() const;
    int redScore() const;
    int blueScore() const;
    int currentRound() const;
    QString gamePhase() const;
    bool isPaused() const;
    bool uses2027Protocol() const;
    QVariantMap territoryStatus() const;
    QString redTeamName() const;
    QString blueTeamName() const;
    int zeroInt() const;
    bool falseValue() const;
    int currentHealth() const;
    int maxHealth() const;
    int chassisEnergy() const;
    int maxChassisEnergy() const;
    int bufferEnergy() const;
    int maxBufferEnergy() const;
    double superCapEnergy() const;
    int robotLevel() const;
    int maxlevel() const;
    int robotId() const;
    int currentExp() const;
    int maxExp() const;
    int yellowCardCount() const;
    QString outOfCombatStatus() const;
    QString chassisStatus() const;
    bool isOutOfCombat() const;
    QVariantList moduleStates() const;
    QVariantList moduleData() const;
    QVariantMap buffTimedData() const;
    QVariantMap myRobot() const;

signals:
    void updated();

private:
    MatchRobotSnapshot operatorRobot() const;

    MatchSession *m_session = nullptr;
    QString m_sourceKind = QStringLiteral("mock");
};

} // namespace RM
