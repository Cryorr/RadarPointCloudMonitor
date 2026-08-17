#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include "protocol.h"

// 模拟雷达设备（SPEC 6.1）
//  - 作为 TCP 服务端监听端口，等待上位机连接
//  - 用 QTimer 定时生成并发送点云数据包
//  - 生成数据：在参考平面（略带斜坡）附近加噪声，模拟正常点
//  - 模拟异常：周期性让部分点的 z 位移超过阈值
class Simulator : public QObject
{
    Q_OBJECT

public:
    explicit Simulator(QObject *parent = nullptr);
    ~Simulator() override;

    // 开始监听端口并定时发送（对外接口）
    void start(quint16 port);
    // 停止发送并关闭监听（对外接口）
    void stop();

    // 可调参数（供测试与后续配置对话框使用）
    void setPointsPerPacket(int n) { m_pointsPerPacket = n; }
    void setSendIntervalMs(int ms) { m_sendIntervalMs = ms; }
    void setAnomalyRatio(double r) { m_anomalyRatio = r; }

signals:
    void clientConnected();                              // 有客户端连上
    void clientDisconnected();                           // 客户端断开
    void errorOccurred(const QString &message);          // 监听失败等错误
    void packetSent(int packetIndex, int pointCount);    // 已发送一包（调试/统计）

private slots:
    void onNewConnection();
    void onClientDisconnected();
    void onSendTimeout();

private:
    DataPacket generatePacket();                  // 生成一包点
    void sendPacket(const DataPacket &packet);    // 打包并发送

    QTcpServer *m_server = nullptr;
    QTcpSocket *m_client = nullptr;   // 当前唯一客户端
    QTimer *m_timer = nullptr;

    int m_pointsPerPacket = 50;       // 每包点数（SPEC 4.3）
    int m_sendIntervalMs = 50;        // 发送间隔 50ms → 每秒 20 包
    double m_anomalyRatio = 0.08;     // 异常期异常点占比（约 8%）
    qint64 m_packetIndex = 0;         // 已发送包序号（用于周期波动）
};

#endif // SIMULATOR_H
