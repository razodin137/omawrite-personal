#include "alternativetone.h"

#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace {
constexpr double twoPi = 6.28318530717958647692;
constexpr double attackSeconds = 0.0015;
}

double AlternativeTone::frequency(Kind kind) {
    return kind == Kind::Higher ? 880.0 : 660.0;
}

QByteArray AlternativeTone::pcm(Kind kind) {
    const int frames = sampleRate * durationMsecs / 1000;
    const int bytesPerFrame = channels * bitsPerSample / 8;
    QByteArray buffer(frames * bytesPerFrame, '\0');
    auto *samples = reinterpret_cast<qint16 *>(buffer.data());
    const double hz = frequency(kind);
    const double attackFrames = attackSeconds * sampleRate;

    for (int frame = 0; frame < frames; ++frame) {
        const double seconds = double(frame) / sampleRate;
        const double progress = double(frame) / frames;
        const double attack = std::min(1.0, double(frame) / attackFrames);
        const double release = (1.0 - progress) * (1.0 - progress);
        const double value = std::sin(twoPi * hz * seconds) * attack * release;
        samples[frame] = qint16(std::lround(value * peakAmplitude * 32767.0));
    }
    return buffer;
}
