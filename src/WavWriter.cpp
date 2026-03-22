#include "WavWriter.h"
#include "WavWriterPcm.h"

namespace WavWriter {

bool writePcm16(
    const QString& filePath,
    std::span<const qint16> samples,
    quint32 sampleRate,
    quint16 channelCount,
    QString* errorMessage)
{
    return WavWriterDetail::WavWriterPcm::write(
        filePath,
        samples,
        sampleRate,
        channelCount,
        errorMessage);
}

}
