#pragma once

#include <QObject>
#include <QPointer>

class GameData;

namespace RM {

class MatchDataSource;
class MatchSession;
class MockMatchSource;

class MatchSourceController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString sourceKind READ sourceKind NOTIFY sourceChanged)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY sourceChanged)
    Q_PROPERTY(bool active READ isActive NOTIFY sourceChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY sourceChanged)

public:
    explicit MatchSourceController(QObject *parent = nullptr);

    bool selectMock(const QString &scenarioPath = QString());
    bool selectReplay(const QString &recordingPath);
    bool selectReal(::GameData *gameData = nullptr);

    MatchDataSource *currentSource() const;
    MatchSession *matchSession() const;
    MockMatchSource *mockSource() const;
    QString sourceKind() const;
    QString sourceName() const;
    bool isActive() const;
    QString statusText() const;

signals:
    void sourceChanged();

private:
    void replaceSource(MatchDataSource *source);

    MatchDataSource *m_source = nullptr;
    QPointer<QObject> m_sourceObject;
    QPointer<MockMatchSource> m_mockSource;
};

} // namespace RM
