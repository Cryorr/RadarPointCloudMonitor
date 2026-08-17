#ifndef DATARECEIVER_H
#define DATARECEIVER_H

#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

#include "protocol.h"

// 数据接收与解析（SPEC 6.2）
//  - 内部用 QTcpSocket 连接模拟设备，持续读取数据
//  - 用 QByteArray 缓冲区解决 TCP 粘包/半包
//  - 设计为 worker 对象：由外部把它 moveToThread 到工作线程，
//    这样 readyRead 事件、协议解析、统计都在子线程完成，不阻塞主线程（SPEC 10.3）。
class DataReceiver : public QObject
{
    Q_OBJECT

public:
    explicit DataReceiver(QObject *parent = nullptr);
    ~DataReceiver() override;

signals:
    // 解析出一包完整点云数据（跨线程发给主线程显示）
    void packetReceived(const DataPacket &packet);
    // 连接状态变化
    void connectionChanged(bool connected);
    // 每秒统计：点数/秒、包数/秒
    void statsUpdated(int pointsPerSecond, int packetsPerSecond);

public slots:
    // 连接设备（在子线程中执行）
    void connectToDevice(const QHostAddress &addr, quint16 port);
    // 断开连接（在子线程中执行）
    void disconnectFromDevice();

private slots:
    void onReadyRead();
    void onConnected();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onStatsTimeout();

private:
    void ensureSocket();      // 懒创建 socket
    void processBuffer();     // 从缓冲区解析出所有完整包

    QTcpSocket *m_socket = nullptr;
    QByteArray m_buffer;      // 接收缓冲区（存未解析完的字节）

    QTimer *m_statsTimer = nullptr;
    int m_packetsInWindow = 0;   // 当前统计窗口内的包数
    int m_pointsInWindow = 0;    // 当前统计窗口内的点数
};

#endif // DATARECEIVER_H
