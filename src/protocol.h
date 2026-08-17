#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QByteArray>
#include <QMetaType>
#include <QVector>
#include <QtGlobal>

// ==================== 核心数据结构（SPEC 第 5 节） ====================
// 这些结构体是整个项目共享的“数据类型”，多个模块都会 include 本头文件来使用。

// 单个雷达点
struct PointData {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float intensity = 0.0f;
    qint64 timestamp = 0;   // Unix 毫秒时间戳
};

// 一个数据包
struct DataPacket {
    quint16 pointCount = 0;
    QVector<PointData> points;
};

// 报警记录（与 SQLite 表 alarms 字段一一对应）
struct AlarmRecord {
    qint64 timestamp = 0;
    int type = 0;           // 0=位移超限，1=异常点数比例超限
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float value = 0.0f;
    float threshold = 0.0f;
};

// 注册到 Qt 元类型系统。
// 用途：后续 DataReceiver 在“子线程”通过信号把 DataPacket 发到“主线程”时，
// 属于跨线程的排队连接（QueuedConnection），Qt 需要知道这些类型才能复制传递。
Q_DECLARE_METATYPE(PointData)
Q_DECLARE_METATYPE(DataPacket)
Q_DECLARE_METATYPE(AlarmRecord)

// ==================== 协议常量与函数（SPEC 第 4 节） ====================
namespace Protocol {

    constexpr quint16 MAGIC = 0xAA55;   // 固定帧头
    constexpr quint8  VERSION = 1;      // 固定版本
    constexpr int HEADER_SIZE = 9;      // 2(magic) + 1(version) + 2(pointCount) + 4(payloadLength)
    constexpr int POINT_SIZE = 24;      // 单点大小：4*float32 + 1*double

    // 解析结果枚举
    enum class ParseResult {
        Complete,    // 成功解析出一个完整包
        Incomplete,  // 数据不足（半包），需等待更多数据
        Invalid      // 数据不合法（帧头/版本/长度校验失败）
    };

    // 序列化：把 DataPacket 打包成小端序二进制 QByteArray（供 Simulator 发送）
    QByteArray serializePacket(const DataPacket &packet);

    // 解析：尝试从缓冲区解析出第一个完整包（供 DataReceiver 处理粘包/半包）
    //  - 返回 Complete  ：packet 被填充，consumedBytes 为该包总字节数，调用方需据此丢弃已消费字节；
    //  - 返回 Incomplete：consumedBytes = 0，表示还需要更多数据再试；
    //  - 返回 Invalid   ：consumedBytes = 0，调用方自行决定如何丢弃坏字节以恢复同步。
    ParseResult parsePacket(const QByteArray &buffer, DataPacket &packet, int &consumedBytes);

} // namespace Protocol

#endif // PROTOCOL_H
