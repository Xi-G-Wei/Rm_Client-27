#include "simulator/MatchRecorder.h"

#include "core/MatchSession.h"
#include "core/MatchSnapshotJson.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace RM {

MatchRecorder::MatchRecorder(QObject *parent)
    : QObject(parent) {}

bool MatchRecorder::start(MatchSession *session, const QString &path) {
    stop();
    if (!session || !session->isValid() || path.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("录制需要有效比赛快照和文件路径"));
        return false;
    }
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit errorOccurred(
            QStringLiteral("无法创建录制文件：%1").arg(m_file.errorString()));
        return false;
    }
    m_session = session;
    m_path = path;
    m_frameCount = 0;
    m_elapsed.start();
    const QByteArray header = QJsonDocument(QJsonObject{
        {QStringLiteral("format"), QStringLiteral("rm-match-recording")},
        {QStringLiteral("version"), 1},
    }).toJson(QJsonDocument::Compact) + '\n';
    m_file.write(header);
    connect(m_session,
            &MatchSession::snapshotChanged,
            this,
            &MatchRecorder::captureSnapshot,
            Qt::UniqueConnection);
    captureSnapshot();
    emit recordingChanged();
    return true;
}

void MatchRecorder::stop() {
    if (!m_file.isOpen())
        return;
    disconnect(m_session, &MatchSession::snapshotChanged,
               this, &MatchRecorder::captureSnapshot);
    m_file.flush();
    m_file.close();
    m_session = nullptr;
    emit recordingChanged();
}

bool MatchRecorder::isRecording() const {
    return m_file.isOpen();
}

QString MatchRecorder::recordingPath() const {
    return m_path;
}

int MatchRecorder::frameCount() const {
    return m_frameCount;
}

void MatchRecorder::captureSnapshot() {
    if (!m_file.isOpen() || !m_session || !m_session->isValid())
        return;
    const QJsonObject frame{
        {QStringLiteral("offsetMs"), m_elapsed.elapsed()},
        {QStringLiteral("snapshot"),
         matchSnapshotToJson(m_session->matchSnapshot())},
    };
    m_file.write(QJsonDocument(frame).toJson(QJsonDocument::Compact) + '\n');
    m_file.flush();
    ++m_frameCount;
    emit frameCountChanged();
}

} // namespace RM
