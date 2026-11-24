#include "simulator/MatchReplaySource.h"

#include "core/MatchSnapshotJson.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace RM {

MatchReplaySource::MatchReplaySource(QObject *parent)
    : QObject(parent), m_session(new MatchSession(this)) {
    m_timer.setInterval(100);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &MatchReplaySource::onTimer);
}

bool MatchReplaySource::loadFromFile(const QString &path) {
    m_timer.stop();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("无法读取录制文件：%1").arg(file.errorString());
        emit errorOccurred(m_error);
        return false;
    }
    QList<Frame> loaded;
    qint64 lastOffset = 0;
    bool firstLine = true;
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.isEmpty())
            continue;
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError ||
            !document.isObject()) {
            m_error = QStringLiteral("录制文件格式无效");
            emit errorOccurred(m_error);
            return false;
        }
        const QJsonObject object = document.object();
        if (firstLine) {
            firstLine = false;
            if (object.value(QStringLiteral("format")).toString()
                == QStringLiteral("rm-match-recording")) {
                continue;
            }
        }
        Frame frame;
        frame.offsetMs = static_cast<qint64>(
            object.value(QStringLiteral("offsetMs")).toDouble());
        QString error;
        if (!matchSnapshotFromJson(
                object.value(QStringLiteral("snapshot")).toObject(),
                &frame.snapshot, &error)) {
            m_error = error;
            emit errorOccurred(m_error);
            return false;
        }
        frame.offsetMs = qMax(lastOffset, frame.offsetMs);
        lastOffset = frame.offsetMs;
        loaded.append(frame);
    }
    if (loaded.isEmpty()) {
        m_error = QStringLiteral("录制文件不包含比赛快照");
        emit errorOccurred(m_error);
        return false;
    }
    m_frames = loaded;
    std::stable_sort(m_frames.begin(), m_frames.end(),
                     [](const Frame &left, const Frame &right) {
                         return left.offsetMs < right.offsetMs;
                     });
    m_name = QFileInfo(path).fileName();
    m_error.clear();
    reset();
    emit stateChanged();
    emit frameChanged();
    return true;
}

bool MatchReplaySource::start() {
    if (m_frames.isEmpty() || m_completed)
        return false;
    if (m_paused)
        return resume();
    if (m_running)
        return true;
    m_running = true;
    m_paused = false;
    m_wallClock.start();
    m_lastWallMs = 0;
    m_timer.start();
    emit stateChanged();
    return true;
}

bool MatchReplaySource::pause() {
    if (!m_running || m_paused)
        return false;
    m_paused = true;
    m_timer.stop();
    emit stateChanged();
    return true;
}

bool MatchReplaySource::resume() {
    if (!m_paused || m_completed)
        return false;
    m_paused = false;
    m_running = true;
    m_wallClock.start();
    m_lastWallMs = 0;
    m_timer.start();
    emit stateChanged();
    return true;
}

bool MatchReplaySource::reset() {
    if (m_frames.isEmpty())
        return false;
    m_timer.stop();
    m_running = false;
    m_paused = false;
    m_completed = false;
    m_elapsedMs = 0;
    m_nextFrame = 0;
    m_carryMs = 0.0;
    applyFrame(0);
    m_nextFrame = 1;
    emit stateChanged();
    emit frameChanged();
    return true;
}

void MatchReplaySource::setPlaybackRate(double rate) {
    m_rate = std::clamp(rate, 0.25, 4.0);
    emit stateChanged();
}

bool MatchReplaySource::advanceByMs(qint64 elapsedMs) {
    if (!m_running || m_paused || m_frames.isEmpty() || elapsedMs < 0)
        return false;
    m_carryMs += elapsedMs * m_rate;
    int seconds = static_cast<int>(std::floor(m_carryMs / 1000.0));
    if (seconds <= 0)
        return true;
    m_carryMs -= seconds * 1000.0;
    m_elapsedMs += seconds * 1000LL;

    int applied = -1;
    while (m_nextFrame < m_frames.size() &&
           m_frames.at(m_nextFrame).offsetMs <= m_elapsedMs) {
        applied = m_nextFrame++;
    }
    if (applied >= 0)
        applyFrame(applied);
    if (m_nextFrame >= m_frames.size() &&
        m_elapsedMs >= m_frames.last().offsetMs) {
        m_running = false;
        m_completed = true;
        m_timer.stop();
        emit stateChanged();
    }
    emit frameChanged();
    return true;
}

MatchSession *MatchReplaySource::matchSession() const {
    return m_session;
}

QString MatchReplaySource::sourceKind() const {
    return QStringLiteral("replay");
}

QString MatchReplaySource::sourceName() const {
    return m_name;
}

bool MatchReplaySource::isAvailable() const {
    return !m_frames.isEmpty() && m_session->isValid();
}

QString MatchReplaySource::statusText() const {
    if (m_frames.isEmpty())
        return m_error.isEmpty() ? QStringLiteral("回放数据未加载") : m_error;
    return m_completed ? QStringLiteral("回放已结束")
                       : m_paused ? QStringLiteral("回放已暂停")
                                  : m_running ? QStringLiteral("回放运行中")
                                              : QStringLiteral("回放就绪");
}

bool MatchReplaySource::isLoaded() const {
    return !m_frames.isEmpty();
}

bool MatchReplaySource::isRunning() const {
    return m_running && !m_paused;
}

bool MatchReplaySource::isPaused() const {
    return m_paused;
}

bool MatchReplaySource::isCompleted() const {
    return m_completed;
}

double MatchReplaySource::playbackRate() const {
    return m_rate;
}

int MatchReplaySource::currentFrame() const {
    return qMax(0, m_nextFrame - 1);
}

int MatchReplaySource::frameCount() const {
    return m_frames.size();
}

void MatchReplaySource::onTimer() {
    if (!m_running || m_paused)
        return;
    const qint64 now = m_wallClock.elapsed();
    const qint64 delta = qMax<qint64>(0, now - m_lastWallMs);
    m_lastWallMs = now;
    if (delta > 0)
        advanceByMs(delta);
}

bool MatchReplaySource::applyFrame(int index) {
    if (index < 0 || index >= m_frames.size())
        return false;
    QString error;
    if (!m_session->replaceSnapshot(m_frames.at(index).snapshot, &error)) {
        m_error = error;
        emit errorOccurred(error);
        return false;
    }
    return true;
}

} // namespace RM
