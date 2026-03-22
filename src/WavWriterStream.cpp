#include "WavWriterStream.h"

#include "WavWriterPcm.h"

#include <QDataStream>

#include <limits>

namespace WavWriterDetail {

bool WavWriterStream::open(
    const QString& filePath,
    quint32 sampleRate,
    quint16 channelCount,
    QString* errorMessage)
{
    discard();

    if (!WavWriterPcm::validateFormat(sampleRate, channelCount, errorMessage)) {
        return false;
    }

    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(errorMessage, m_file.errorString());
        return false;
    }

    m_filePath = filePath;
    m_sampleRate = sampleRate;
    m_channelCount = channelCount;
    m_dataBytes = 0;
    m_removeOnDiscard = true;

    if (!writeHeader(0, errorMessage)) {
        discard();
        return false;
    }

    return true;
}

bool WavWriterStream::appendSamples(std::span<const qint16> samples, QString* errorMessage)
{
    if (!isOpen() || samples.empty()) {
        return isOpen();
    }

    const quint64 appendBytes64 = static_cast<quint64>(samples.size()) * WavWriterPcm::BytesPerSample;
    const quint64 totalBytes64 = static_cast<quint64>(m_dataBytes) + appendBytes64;
    if (totalBytes64 > std::numeric_limits<quint32>::max()) {
        setError(errorMessage, QStringLiteral("Recording is too large for standard WAV output."));
        return false;
    }

    const auto* rawBytes = reinterpret_cast<const char*>(samples.data());
    const qint64 appendBytes = static_cast<qint64>(appendBytes64);
    if (m_file.write(rawBytes, appendBytes) != appendBytes) {
        setError(errorMessage, QStringLiteral("Failed to write WAV sample data."));
        return false;
    }

    m_dataBytes = static_cast<quint32>(totalBytes64);
    return true;
}

bool WavWriterStream::finalize(QString* errorMessage)
{
    if (!isOpen()) {
        return true;
    }

    if (!writeHeader(m_dataBytes, errorMessage)) {
        return false;
    }

    if (!m_file.flush()) {
        setError(errorMessage, m_file.errorString());
        return false;
    }

    m_removeOnDiscard = false;
    m_file.close();
    return true;
}

void WavWriterStream::discard()
{
    const QString stalePath = m_file.fileName();

    if (m_file.isOpen()) {
        m_file.close();
    }

    if (m_removeOnDiscard && !stalePath.isEmpty()) {
        QFile::remove(stalePath);
    }

    m_file.setFileName(QString());
    m_filePath.clear();
    m_sampleRate = 0;
    m_channelCount = 0;
    m_dataBytes = 0;
    m_removeOnDiscard = false;
}

bool WavWriterStream::hasAudio() const
{
    return m_dataBytes > 0;
}

bool WavWriterStream::isOpen() const
{
    return m_file.isOpen();
}

QString WavWriterStream::filePath() const
{
    return m_filePath;
}

bool WavWriterStream::writeHeader(quint32 dataBytes, QString* errorMessage)
{
    if (!m_file.isOpen()) {
        setError(errorMessage, QStringLiteral("WAV output file is not open."));
        return false;
    }

    if (!m_file.seek(0)) {
        setError(errorMessage, m_file.errorString());
        return false;
    }

    QDataStream stream(&m_file);
    stream.setByteOrder(QDataStream::LittleEndian);
    if (!WavWriterPcm::writeHeader(stream, m_sampleRate, m_channelCount, dataBytes)) {
        setError(errorMessage, QStringLiteral("Failed to write WAV header."));
        return false;
    }

    return m_file.seek(m_file.size());
}

void WavWriterStream::setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

}