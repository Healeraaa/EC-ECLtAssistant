#include "../ProtocolFrameParser.h"

#include <cmath>
#include <cstring>
#include <iostream>

namespace {

quint16 crc16(const QByteArray& data)
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

void appendU16(QByteArray& data, quint16 value)
{
    data.append(static_cast<char>(value & 0xFF));
    data.append(static_cast<char>((value >> 8) & 0xFF));
}

void appendU32(QByteArray& data, quint32 value)
{
    data.append(static_cast<char>(value & 0xFF));
    data.append(static_cast<char>((value >> 8) & 0xFF));
    data.append(static_cast<char>((value >> 16) & 0xFF));
    data.append(static_cast<char>((value >> 24) & 0xFF));
}

QByteArray makeFrame(quint8 type, quint32 sampleRate, const QVector<float>& values)
{
    QByteArray frame;
    frame.append('\x55');
    frame.append(static_cast<char>(0xAA));
    appendU16(frame, static_cast<quint16>(19 + values.size() * 4));
    frame.append(static_cast<char>(type));
    appendU32(frame, sampleRate);
    appendU32(frame, 1234);
    appendU16(frame, static_cast<quint16>(values.size()));
    for (float value : values) {
        quint32 rawValue = 0;
        std::memcpy(&rawValue, &value, sizeof(value));
        appendU32(frame, rawValue);
    }
    appendU16(frame, crc16(frame));
    frame.append('\x0D');
    frame.append('\x0A');
    return frame;
}

bool expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

}

int main()
{
    bool passed = true;
    ProtocolFrameParser parser;

    const QByteArray ivFrame = makeFrame(
        static_cast<quint8>(ProtocolFrame::Type::IV), 100, { 1.25f, -0.5f, 2.5f, 0.75f });
    parser.append(QByteArray("\x00\x11", 2) + ivFrame.left(7));
    passed &= expect(parser.takeFrames().isEmpty(), "split frame must wait for remaining bytes");
    parser.append(ivFrame.mid(7));
    QVector<ProtocolFrame> frames = parser.takeFrames();
    passed &= expect(frames.size() == 1, "split frame was not reconstructed");
    if (frames.size() == 1) {
        passed &= expect(frames.first().sampleRate == 100, "sample rate mismatch");
        passed &= expect(frames.first().values.size() == 4, "value count mismatch");
        passed &= expect(std::fabs(frames.first().values.at(1) + 0.5f) < 1.0e-6f, "float decode mismatch");
    }

    QByteArray corruptedFrame = ivFrame;
    corruptedFrame[16] = static_cast<char>(corruptedFrame.at(16) ^ 0x40);
    const QByteArray lightFrame = makeFrame(
        static_cast<quint8>(ProtocolFrame::Type::Light), 100, { 42.0f, 43.0f });
    parser.append(corruptedFrame + lightFrame);
    frames = parser.takeFrames();
    passed &= expect(frames.size() == 1, "parser did not recover after CRC failure");
    if (frames.size() == 1) {
        passed &= expect(frames.first().type == ProtocolFrame::Type::Light, "wrong frame recovered");
    }
    passed &= expect(parser.stats().crcErrors == 1, "CRC error counter mismatch");
    passed &= expect(parser.stats().discardedBytes >= 3, "discarded byte counter mismatch");

    const QByteArray unknownFrame = makeFrame(0x7F, 100, { 1.0f });
    parser.append(unknownFrame);
    passed &= expect(parser.takeFrames().isEmpty(), "unknown frame type must be rejected");
    passed &= expect(parser.stats().unknownFrames == 1, "unknown frame counter mismatch");

    if (passed) {
        std::cout << "ProtocolFrameParser tests passed.\n";
        return 0;
    }
    return 1;
}
