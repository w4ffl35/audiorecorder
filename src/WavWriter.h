#pragma once

#include <QString>

#include <span>

#include <QtTypes>

namespace WavWriter {

bool writePcm16(
    const QString& filePath,
    std::span<const qint16> samples,
    quint32 sampleRate,
    quint16 channelCount,
    QString* errorMessage);

}
