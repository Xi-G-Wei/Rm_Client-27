#pragma once

#include <QElapsedTimer>
#include <QFile>
#include <QObject>

namespace RM {

class MatchSession;

class MatchRecorder : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool recording READ isRecording NOTIFY recordingChanged)
    Q_PROPERTY(QString recordingPath READ recordingPath NOTIFY recordingChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY frameCountChanged)

public:
    explicit MatchRecorder(QObject *parent = nullptr);

    bool start(MatchSession *session, const QString &path);
    void stop();
    bool isRecording() const;
    QString recordingPath() const;
    int frameCount() const;

signals:
    void recordingChanged();
    void frameCountChanged();
    void errorOccurred(const QString &message);

private:
    void captureSnapshot();

    MatchSession *m_session = nullptr;
    QFile m_file;
    QElapsedTimer m_elapsed;
    QString m_path;
    int m_frameCount = 0;
};

} // namespace RM
