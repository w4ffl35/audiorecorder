#pragma once

#include <QString>
#include <span>
#include <QtTypes>

class QDataStream;

namespace WavWriterDetail {

class WavWriterPcm
{
public:
    static bool write(
        const QString& filePath,
        std::span<const qint16> samples,
        quint32 sampleRate,
        quint16 channelCount,
        QString* errorMessage);

private:
    static constexpr quint32 BytesPerSample = sizeof(qint16);
    static constexpr quint32 PcmFormatChunkSize = 16;
    static constexpr quint16 PcmFormatTag = 1;
    static constexpr quint16 BitsPerSample = 16;
    static constexpr quint32 RiffChunkHeaderSize = 36;

    static bool validateFormat(
        quint32 sampleRate,
        quint16 channelCount,
        QString* errorMessage);
    static bool writeHeader(
        QDataStream& stream,
        quint32 sampleRate,
        quint16 channelCount,
        quint32 dataBytes);
    static bool writeSampleData(
        QDataStream& stream,
        std::span<const qint16> samples,
        quint32 dataBytes,
        QString* errorMessage);
    static void setError(QString* errorMessage, const QString& message);
};

}
