#pragma once

#include <QAudio>
#include <QAudioDevice>
#include <QObject>

class QAudioSink;
class QBuffer;

class TonePlayer : public QObject {
    Q_OBJECT

public:
    explicit TonePlayer(QObject *parent = nullptr);

public slots:
    void playForFlip(bool landedOnOriginal);

private:
    QAudioSink *m_sink = nullptr;
    QBuffer *m_buffer = nullptr;
    QAudioDevice m_device;
};
