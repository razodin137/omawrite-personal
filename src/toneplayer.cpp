#include "toneplayer.h"

#include "alternativetone.h"

#include <QAudioFormat>
#include <QAudioSink>
#include <QBuffer>
#include <QMediaDevices>

TonePlayer::TonePlayer(QObject *parent)
    : QObject(parent) {
}

void TonePlayer::playForFlip(bool landedOnOriginal) {
    const AlternativeTone::Kind kind = landedOnOriginal
        ? AlternativeTone::Kind::Lower
        : AlternativeTone::Kind::Higher;

    const QAudioDevice output = QMediaDevices::defaultAudioOutput();
    if (output.isNull())
        return;

    QAudioFormat format;
    format.setSampleRate(AlternativeTone::sampleRate);
    format.setChannelCount(AlternativeTone::channels);
    format.setSampleFormat(QAudioFormat::Int16);
    if (!output.isFormatSupported(format))
        return;

    if (!m_sink || m_device != output) {
        delete m_sink;
        m_sink = new QAudioSink(output, format, this);
        m_device = output;
        connect(m_sink, &QAudioSink::stateChanged, this, [this](QAudio::State state) {
            if (state == QAudio::IdleState)
                m_sink->stop();
        });
    }

    m_sink->stop();
    if (!m_buffer)
        m_buffer = new QBuffer(this);
    m_buffer->close();
    m_buffer->setData(AlternativeTone::pcm(kind));
    m_buffer->open(QIODevice::ReadOnly);
    m_sink->start(m_buffer);
}
