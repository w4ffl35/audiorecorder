#include "WavWriterPcm.h"

#include <QDataStream>
#include <QFile>

#include <limits>

namespace WavWriterDetail {

bool WavWriterPcm::write(
    const QString& filePath,
    std::span<const qint16> samples,
    quint32 sampleRate,
    quint16 channelCount,
    QString* errorMessage)
{
    if (!validateFormat(sampleRate, channelCount, errorMessage)) {
        return false;
    }

    const quint64 dataBytes64 = static_cast<quint64>(samples.size()) * BytesPerSample;
    if (dataBytes64 > std::numeric_limits<quint32>::max()) {
        setError(
            errorMessage,
            QStringLiteral("Recording is too large for standard WAV output."));
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(errorMessage, file.errorString());
        return false;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);

    const quint32 dataBytes = static_cast<quint32>(dataBytes64);
    if (!writeHeader(stream, sampleRate, channelCount, dataBytes)) {
        setError(errorMessage, QStringLiteral("Failed to write WAV header."));
        return false;
    }

    if (!writeSampleData(stream, samples, dataBytes, errorMessage)) {
        return false;
    }

    if (stream.status() != QDataStream::Ok) {
        setError(errorMessage, QStringLiteral("Failed to finalize WAV file."));
        return false;
    }

    return true;
}

bool WavWriterPcm::validateFormat(
    quint32 sampleRate,
    quint16 channelCount,
    QString* errorMessage)
{
    if (channelCount == 0 || sampleRate == 0) {
        setError(errorMessage, QStringLiteral("Invalid WAV format parameters."));
        return false;
    }

    return true;
}

bool WavWriterPcm::writeHeader(
    QDataStream& stream,
    quint32 sampleRate,
    quint16 channelCount,
    quint32 dataBytes)
{
    const quint32 byteRate = sampleRate * channelCount * BytesPerSample;
    const quint16 blockAlign = channelCount * BytesPerSample;
    const quint32 riffChunkSize = RiffChunkHeaderSize + dataBytes;

    stream.writeRawData("RIFF", 4);
    stream << riffChunkSize;
    stream.writeRawData("WAVE", 4);
    stream.writeRawData("fmt ", 4);
    stream << quint32(PcmFormatChunkSize);
    stream << quint16(PcmFormatTag);
    stream << channelCount;
    stream << sampleRate;
    stream << byteRate;
    stream << blockAlign;
    stream << quint16(BitsPerSample);
    stream.writeRawData("data", 4);
    stream << dataBytes;

    return stream.status() == QDataStream::Ok;
}

bool WavWriterPcm::writeSampleData(
    QDataStream& stream,
    std::span<const qint16> samples,
    quint32 dataBytes,
    QString* errorMessage)
{
    if (samples.empty()) {
        return true;
    }

    const auto* rawBytes = reinterpret_cast<const char*>(samples.data());
    if (stream.writeRawData(rawBytes, static_cast<int>(dataBytes)) != static_cast<int>(dataBytes)) {
        setError(errorMessage, QStringLiteral("Failed to write WAV sample data."));
        return false;
    }

    return true;
}

void WavWriterPcm::setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

}
