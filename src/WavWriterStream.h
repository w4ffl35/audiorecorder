#pragma once

#include <QFile>
#include <QString>
#include <QtGlobal>

#include <span>

namespace WavWriterDetail {

class WavWriterStream
{
public:
    bool open(const QString& filePath, quint32 sampleRate, quint16 channelCount, QString* errorMessage);
    bool appendSamples(std::span<const qint16> samples, QString* errorMessage);
    bool finalize(QString* errorMessage);
    void discard();

    bool hasAudio() const;
    bool isOpen() const;
    QString filePath() const;

private:
    bool writeHeader(quint32 dataBytes, QString* errorMessage);
    static void setError(QString* errorMessage, const QString& message);

    QFile m_file;
    QString m_filePath;
    quint32 m_sampleRate = 0;
    quint16 m_channelCount = 0;
    quint32 m_dataBytes = 0;
    bool m_removeOnDiscard = false;
};

}