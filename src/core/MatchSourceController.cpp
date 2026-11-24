#include "core/MatchSourceController.h"

#include "core/MatchDataSource.h"
#include "core/MatchSession.h"
#include "simulator/MockMatchSource.h"
#include "simulator/MatchReplaySource.h"

namespace RM {
namespace {

class UnavailableMatchSource final : public QObject,
                                     public MatchDataSource {
public:
    explicit UnavailableMatchSource(QObject *parent)
        : QObject(parent), m_session(new MatchSession(this)) {
        m_session->clear();
    }

    MatchSession *matchSession() const override {
        return m_session;
    }
    QString sourceKind() const override {
        return QStringLiteral("real");
    }
    QString sourceName() const override {
        return QStringLiteral("真实比赛数据");
    }
    bool isAvailable() const override {
        return false;
    }
    QString statusText() const override {
        return QStringLiteral("真实协议接口已保留，等待通信接入");
    }

private:
    MatchSession *m_session = nullptr;
};

} // namespace

MatchSourceController::MatchSourceController(QObject *parent)
    : QObject(parent) {}

bool MatchSourceController::selectMock(const QString &scenarioPath) {
    auto *candidate = new MockMatchSource(this);
    const QString path = scenarioPath.isEmpty()
        ? QStringLiteral(":/mock/3v3/match_3v3_basic.json")
        : scenarioPath;
    if (!candidate->loadFromFile(path)) {
        candidate->deleteLater();
        return false;
    }
    candidate->start();
    m_mockSource = candidate;
    replaceSource(candidate);
    return true;
}

bool MatchSourceController::selectReal() {
    auto *candidate = new UnavailableMatchSource(this);
    m_mockSource = nullptr;
    replaceSource(candidate);
    return true;
}

bool MatchSourceController::selectReplay(const QString &recordingPath) {
    auto *candidate = new MatchReplaySource(this);
    if (!candidate->loadFromFile(recordingPath)) {
        candidate->deleteLater();
        return false;
    }
    candidate->start();
    m_mockSource = nullptr;
    replaceSource(candidate);
    return true;
}

MatchDataSource *MatchSourceController::currentSource() const {
    return m_source;
}

MatchSession *MatchSourceController::matchSession() const {
    return m_source ? m_source->matchSession() : nullptr;
}

MockMatchSource *MatchSourceController::mockSource() const {
    return m_mockSource.data();
}

QString MatchSourceController::sourceKind() const {
    return m_source ? m_source->sourceKind() : QStringLiteral("none");
}

QString MatchSourceController::sourceName() const {
    return m_source ? m_source->sourceName() : QStringLiteral("未选择数据源");
}

bool MatchSourceController::isActive() const {
    return m_source && m_source->isAvailable();
}

QString MatchSourceController::statusText() const {
    return m_source ? m_source->statusText()
                    : QStringLiteral("数据源尚未初始化");
}

void MatchSourceController::replaceSource(MatchDataSource *source) {
    if (m_sourceObject && m_sourceObject != dynamic_cast<QObject *>(source)) {
        m_sourceObject->deleteLater();
    }
    m_source = source;
    m_sourceObject = dynamic_cast<QObject *>(source);
    emit sourceChanged();
}

} // namespace RM
