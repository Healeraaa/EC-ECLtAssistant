#ifndef PROTOCOLFRAMEPARSER_H
#define PROTOCOLFRAMEPARSER_H

#include <QByteArray>
#include <QVector>

struct ProtocolFrame
{
    enum class Type : quint8 {
        IV = 0x01,
        Light = 0x02
    };

    Type type = Type::IV;
    quint32 sampleRate = 0;
    quint32 timestampMs = 0;
    QVector<float> values;
};

struct ProtocolParserStats
{
    quint64 validFrames = 0;
    quint64 crcErrors = 0;
    quint64 lengthErrors = 0;
    quint64 unknownFrames = 0;
    quint64 discardedBytes = 0;
};

class ProtocolFrameParser
{
public:
    void append(const QByteArray& data);
    QVector<ProtocolFrame> takeFrames();
    void clear();

    const ProtocolParserStats& stats() const { return m_stats; }
    int bufferedBytes() const { return m_buffer.size(); }

private:
    static quint16 readU16(const char* data);
    static quint32 readU32(const char* data);
    static quint16 calculateCrc16(const QByteArray& data);
    void discardUntilHeader();

    QByteArray m_buffer;
    ProtocolParserStats m_stats;
};

#endif // PROTOCOLFRAMEPARSER_H
