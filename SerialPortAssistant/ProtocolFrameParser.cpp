#include "ProtocolFrameParser.h"

#include <cstring>

namespace {
constexpr int kMinimumFrameLength = 19;
constexpr int kMaximumValueCount = 2048;
constexpr int kMaximumFrameLength = kMinimumFrameLength + kMaximumValueCount * 4;
const QByteArray kFrameHeader("\x55\xAA", 2);
}

void ProtocolFrameParser::append(const QByteArray& data)
{
    if (!data.isEmpty()) {
        m_buffer.append(data);
    }
}

QVector<ProtocolFrame> ProtocolFrameParser::takeFrames()
{
    QVector<ProtocolFrame> frames;

    while (true) {
        discardUntilHeader();
        if (m_buffer.size() < 4) {
            break;
        }

        const quint16 frameLength = readU16(m_buffer.constData() + 2);
        if (frameLength < kMinimumFrameLength || frameLength > kMaximumFrameLength) {
            ++m_stats.lengthErrors;
            ++m_stats.discardedBytes;
            m_buffer.remove(0, 1);
            continue;
        }
        if (m_buffer.size() < frameLength) {
            break;
        }

        if (static_cast<quint8>(m_buffer.at(frameLength - 2)) != 0x0D
            || static_cast<quint8>(m_buffer.at(frameLength - 1)) != 0x0A) {
            ++m_stats.lengthErrors;
            ++m_stats.discardedBytes;
            m_buffer.remove(0, 1);
            continue;
        }

        const quint16 valueCount = readU16(m_buffer.constData() + 13);
        const int expectedLength = kMinimumFrameLength + static_cast<int>(valueCount) * 4;
        if (valueCount == 0 || valueCount > kMaximumValueCount || frameLength != expectedLength) {
            ++m_stats.lengthErrors;
            m_stats.discardedBytes += frameLength;
            m_buffer.remove(0, frameLength);
            continue;
        }

        const quint8 rawType = static_cast<quint8>(m_buffer.at(4));
        if ((rawType != static_cast<quint8>(ProtocolFrame::Type::IV)
             && rawType != static_cast<quint8>(ProtocolFrame::Type::Light))
            || (rawType == static_cast<quint8>(ProtocolFrame::Type::IV) && valueCount % 2 != 0)) {
            ++m_stats.unknownFrames;
            m_stats.discardedBytes += frameLength;
            m_buffer.remove(0, frameLength);
            continue;
        }

        const quint32 sampleRate = readU32(m_buffer.constData() + 5);
        if (sampleRate == 0) {
            ++m_stats.lengthErrors;
            m_stats.discardedBytes += frameLength;
            m_buffer.remove(0, frameLength);
            continue;
        }

        const quint16 expectedCrc = readU16(m_buffer.constData() + frameLength - 4);
        const quint16 actualCrc = calculateCrc16(m_buffer.left(frameLength - 4));
        if (expectedCrc != actualCrc) {
            ++m_stats.crcErrors;
            ++m_stats.discardedBytes;
            m_buffer.remove(0, 1);
            continue;
        }

        ProtocolFrame frame;
        frame.type = static_cast<ProtocolFrame::Type>(rawType);
        frame.sampleRate = sampleRate;
        frame.timestampMs = readU32(m_buffer.constData() + 9);
        frame.values.reserve(valueCount);

        const char* valueData = m_buffer.constData() + 15;
        for (quint16 index = 0; index < valueCount; ++index) {
            const quint32 rawValue = readU32(valueData + index * 4);
            float value = 0.0f;
            static_assert(sizeof(value) == sizeof(rawValue), "Unexpected float size");
            std::memcpy(&value, &rawValue, sizeof(value));
            frame.values.append(value);
        }

        frames.append(frame);
        ++m_stats.validFrames;
        m_buffer.remove(0, frameLength);
    }

    return frames;
}

void ProtocolFrameParser::clear()
{
    m_buffer.clear();
    m_stats = ProtocolParserStats{};
}

quint16 ProtocolFrameParser::readU16(const char* data)
{
    return static_cast<quint8>(data[0])
        | (static_cast<quint16>(static_cast<quint8>(data[1])) << 8);
}

quint32 ProtocolFrameParser::readU32(const char* data)
{
    return static_cast<quint8>(data[0])
        | (static_cast<quint32>(static_cast<quint8>(data[1])) << 8)
        | (static_cast<quint32>(static_cast<quint8>(data[2])) << 16)
        | (static_cast<quint32>(static_cast<quint8>(data[3])) << 24);
}

quint16 ProtocolFrameParser::calculateCrc16(const QByteArray& data)
{
    quint16 crc = 0xFFFF;
    for (char byte : data) {
        crc ^= static_cast<quint16>(static_cast<quint8>(byte)) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) ? static_cast<quint16>((crc << 1) ^ 0x1021)
                                 : static_cast<quint16>(crc << 1);
        }
    }
    return crc;
}

void ProtocolFrameParser::discardUntilHeader()
{
    const int headerPosition = m_buffer.indexOf(kFrameHeader);
    if (headerPosition > 0) {
        m_stats.discardedBytes += headerPosition;
        m_buffer.remove(0, headerPosition);
        return;
    }
    if (headerPosition == 0) {
        return;
    }

    const bool keepTrailingHeaderByte = !m_buffer.isEmpty()
        && static_cast<quint8>(m_buffer.back()) == 0x55;
    const int bytesToDiscard = m_buffer.size() - (keepTrailingHeaderByte ? 1 : 0);
    if (bytesToDiscard > 0) {
        m_stats.discardedBytes += bytesToDiscard;
        m_buffer.remove(0, bytesToDiscard);
    }
}
