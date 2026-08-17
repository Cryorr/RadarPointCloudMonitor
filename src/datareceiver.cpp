#include "datareceiver.h"

DataReceiver::DataReceiver(QObject *parent)
    : QObject(parent)
{
    // 统计定时器：每秒触发一次，汇总并上报速率
    m_statsTimer = new QTimer(this);
    m_statsTimer->setInterval(1000);
    connect(m_statsTimer, &QTimer::timeout, this, &DataReceiver::onStatsTimeout);
}

DataReceiver::~DataReceiver() = default;

void DataReceiver::connectToDevice(const QHostAddress &addr, quint16 port)
{
    ensureSocket();

    // 若已处于连接中，先中断再重连
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    m_socket->connectToHost(addr, port);
}

void DataReceiver::disconnectFromDevice()
{
    if (m_socket) {
        m_socket->disconnectFromHost();
    }
}

void DataReceiver::ensureSocket()
{
    if (m_socket) {
        return;
    }
    // socket 在本对象所在线程创建（正常应在子线程），事件也在该线程处理
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &DataReceiver::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &DataReceiver::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &DataReceiver::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &DataReceiver::onSocketError);
}

void DataReceiver::onReadyRead()
{
    // 把新到的数据追加到缓冲区，再尝试解析（解决“半包”：一次没收齐就留待下次）
    m_buffer.append(m_socket->readAll());
    processBuffer();
}

void DataReceiver::processBuffer()
{
    DataPacket packet;
    int consumed = 0;

    for (;;) {
        const Protocol::ParseResult r = Protocol::parsePacket(m_buffer, packet, consumed);

        if (r == Protocol::ParseResult::Complete) {
            // 解析出一包：丢弃已消费字节（解决“粘包”：多包连在一起逐个拆出）
            m_buffer.remove(0, consumed);
            ++m_packetsInWindow;
            m_pointsInWindow += packet.pointCount;
            emit packetReceived(packet);
        } else if (r == Protocol::ParseResult::Incomplete) {
            // 半包：剩余字节不足一个完整包，退出等待更多数据
            break;
        } else {
            // 数据错位（Invalid）：丢弃 1 字节尝试重新同步，避免卡死
            m_buffer.remove(0, 1);
            if (m_buffer.isEmpty()) {
                break;
            }
        }
    }
}

void DataReceiver::onConnected()
{
    // 连接建立后，清空统计并开始计时
    m_packetsInWindow = 0;
    m_pointsInWindow = 0;
    m_statsTimer->start();
    emit connectionChanged(true);
}

void DataReceiver::onDisconnected()
{
    m_statsTimer->stop();
    m_buffer.clear();
    if (m_socket) {
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    emit connectionChanged(false);
}

void DataReceiver::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    // 连接失败/中断时，同样对外报告“未连接”状态
    emit connectionChanged(false);
}

void DataReceiver::onStatsTimeout()
{
    // 每秒上报一次速率，然后清零重新统计
    emit statsUpdated(m_pointsInWindow, m_packetsInWindow);
    m_pointsInWindow = 0;
    m_packetsInWindow = 0;
}
