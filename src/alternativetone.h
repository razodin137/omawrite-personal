#pragma once

#include <QByteArray>

class AlternativeTone {
public:
    enum class Kind {
        Higher,
        Lower,
    };

    static constexpr int sampleRate = 48000;
    static constexpr int channels = 1;
    static constexpr int bitsPerSample = 16;
    static constexpr int durationMsecs = 30;
    static constexpr double peakAmplitude = 0.06;

    static double frequency(Kind kind);
    static QByteArray pcm(Kind kind);
};
