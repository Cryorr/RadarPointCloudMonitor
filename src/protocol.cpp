#include "protocol.h"

#include <QDataStream>
#include <QIODevice>

#include <cstring>

namespace {

// ---- 浮点数与整数位模式互转 ----
// 协议要求 float 字段 4 字节、double 字段 8 字节（SPEC 4.2）。
// QDataStream 的浮点精度设置是“统一精度”（要么全 32 位、要么全 64 位），
// 无法同时满足 float32 与 double 混用，因此这里手动按位转换：
// 把浮点数按 IEEE 754 位模式复制到整数，再用整数类型写出，字节数完全可控。

quint32 floatBits(float f)
{
    quint32 bits = 0;
    static_assert(sizeof(bits) == sizeof(f), "float must be 32-bit");
    std::memcpy(&bits, &f, sizeof(bits));
    return bits;
}

float bitsToFloat(quint32 bits)
{
    float f = 0.0f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

quint64 doubleBits(double d)
{
    quint64 bits = 0;
    static_assert(sizeof(bits) == sizeof(d), "double must be 64-bit");
    std::memcpy(&bits, &d, sizeof(bits));
    return bits;
}

double bitsToDouble(quint64 bits)
{
    double d = 0.0;
    std::memcpy(&d, &bits, sizeof(d));
    return d;
}

} // namespace

namespace Protocol {

// 序列化：把一个 DataPacket 打包成小端序二进制字节流
QByteArray serializePacket(const DataPacket &packet)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    // 协议要求小端序（SPEC 4.1）
    stream.setByteOrder(QDataStream::LittleEndian);

    // ---- 帧头 ----
    stream << MAGIC;                        // quint16，2 字节，固定 0xAA55
    stream << VERSION;                      // quint8，1 字节，固定 1
    stream << packet.pointCount;            // quint16，2 字节，本包点数
    // 数据区字节数 = 点数 * 单点大小（quint32，4 字节）
    stream << quint32(packet.pointCount * POINT_SIZE);

    // ---- 数据区：逐点写入 ----
    for (const PointData &p : packet.points) {
        stream << floatBits(p.x);           // float32，4 字节
        stream << floatBits(p.y);           // float32，4 字节
        stream << floatBits(p.z);           // float32，4 字节
        stream << floatBits(p.intensity);   // float32，4 字节
        // 协议约定线上用 double（8 字节）传输毫秒时间戳，
        // 而内存结构 PointData.timestamp 是 qint64，这里做一次类型转换。
        stream << doubleBits(static_cast<double>(p.timestamp));  // double，8 字节
    }

    return data;
}

// 解析：从缓冲区尝试解析出第一个完整包（处理粘包/半包）
ParseResult parsePacket(const QByteArray &buffer, DataPacket &packet, int &consumedBytes)
{
    consumedBytes = 0;

    // 1. 连完整帧头（9 字节）都不够 → 半包，等待更多数据
    if (buffer.size() < HEADER_SIZE) {
        return ParseResult::Incomplete;
    }

    QDataStream stream(buffer);
    stream.setByteOrder(QDataStream::LittleEndian);

    quint16 magic = 0;
    quint8 version = 0;
    quint16 pointCount = 0;
    quint32 payloadLength = 0;

    stream >> magic;
    stream >> version;
    stream >> pointCount;
    stream >> payloadLength;

    // 2. 帧头校验：magic 与 version 必须固定，否则说明字节流错位或损坏
    if (magic != MAGIC || version != VERSION) {
        return ParseResult::Invalid;
    }

    // 3. 长度一致性校验：数据区长度必须等于 点数 * 单点大小
    if (payloadLength != quint32(pointCount) * POINT_SIZE) {
        return ParseResult::Invalid;
    }

    // 4. 整包总字节数 = 帧头 + 数据区
    const int totalSize = HEADER_SIZE + static_cast<int>(payloadLength);
    // 数据还没收齐 → 半包，等待更多数据
    if (buffer.size() < totalSize) {
        return ParseResult::Incomplete;
    }

    // 5. 数据区解析
    packet.pointCount = pointCount;
    packet.points.clear();
    packet.points.reserve(pointCount);

    for (quint16 i = 0; i < pointCount; ++i) {
        PointData p;
        quint32 xb = 0, yb = 0, zb = 0, ib = 0;
        quint64 tb = 0;

        stream >> xb;
        stream >> yb;
        stream >> zb;
        stream >> ib;
        stream >> tb;

        p.x = bitsToFloat(xb);
        p.y = bitsToFloat(yb);
        p.z = bitsToFloat(zb);
        p.intensity = bitsToFloat(ib);
        p.timestamp = static_cast<qint64>(bitsToDouble(tb));

        packet.points.append(p);
    }

    consumedBytes = totalSize;
    return ParseResult::Complete;
}

} // namespace Protocol
