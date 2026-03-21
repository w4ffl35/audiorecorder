#include "WavWriter.h"

#include <QDataStream>
#include <QFile>

#include <limits>

namespace {

constexpr quint32 kBytesPerSample = sizeof(qint16);

}

namespace WavWriter {

bool writePcm16(
    const QString& filePath,
    std::span<const qint16> samples,
    quint32 sampleRate,
    quint16 channelCount,
    QString* errorMessage)
{
    if (channelCount == 0 || sampleRate == 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Invalid WAV format parameters.");
        }
        return false;
    }

    const quint64 dataBytes = static_cast<quint64>(samples.size()) * kBytesPerSample;
    if (dataBytes > std::numeric_limits<quint32>::max()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Recording is too large for standard WAV output.");
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);

    const quint32 byteRate = sampleRate * channelCount * kBytesPerSample;
    const quint16 blockAlign = channelCount * kBytesPerSample;
    const quint32 riffChunkSize = 36u + static_cast<quint32>(dataBytes);

    stream.writeRawData("RIFF", 4);
    stream << riffChunkSize;
    stream.writeRawData("WAVE", 4);
    stream.writeRawData("fmt ", 4);
    stream << quint32(16);
    stream << quint16(1);
    stream << channelCount;
    stream << sampleRate;
    stream << byteRate;
    stream << blockAlign;
    stream << quint16(16);
    stream.writeRawData("data", 4);
    stream << static_cast<quint32>(dataBytes);

    if (!samples.empty()) {
        const auto rawBytes = reinterpret_cast<const char*>(samples.data());
        if (stream.writeRawData(rawBytes, static_cast<int>(dataBytes)) != static_cast<int>(dataBytes)) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Failed to write WAV sample data.");
            }
            return false;
        }
    }

    if (stream.status() != QDataStream::Ok) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to finalize WAV file.");
        }
        return false;
    }

    return true;
}

}
