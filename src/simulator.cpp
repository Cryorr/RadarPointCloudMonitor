#include "simulator.h"

#include <QDateTime>
#include <QHostAddress>
#include <QRandomGenerator>

#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;

// 用 Box-Muller 变换生成高斯分布随机数，用于给参考平面加噪声
double gaussianNoise(QRandomGenerator *rng, double mean, double stddev)
{
    double u1 = rng->generateDouble();
    if (u1 < 1e-12) {
        u1 = 1e-12;   // 避免 log(0)
    }
    const double u2 = rng->generateDouble();
    const double mag = std::sqrt(-2.0 * std::log(u1));
    return mean + stddev * mag * std::cos(2.0 * kPi * u2);
}

} // namespace

Simulator::Simulator(QObject *parent)
    : QObject(parent)
{
    m_server = new QTcpServer(this);
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);   // 让发送间隔更接近 50ms

    connect(m_server, &QTcpServer::newConnection, this, &Simulator::onNewConnection);
    connect(m_timer, &QTimer::timeout, this, &Simulator::onSendTimeout);
}

Simulator::~Simulator() = default;

void Simulator::start(quint16 port)
{
    // 若已在监听则先关闭，便于重复 start
    if (m_server->isListening()) {
        m_server->close();
    }

    // 监听本地回环地址，模拟本机运行的雷达设备
    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        emit errorOccurred(m_server->errorString());
        return;
    }

    m_timer->start(m_sendIntervalMs);
}

void Simulator::stop()
{
    m_timer->stop();
    if (m_client) {
        m_client->disconnectFromHost();
    }
    m_server->close();
}

void Simulator::onNewConnection()
{
    // 只服务一个客户端：已有连接时，拒绝多余连接
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        if (m_client == nullptr) {
            m_client = socket;
            connect(m_client, &QTcpSocket::disconnected, this, &Simulator::onClientDisconnected);
            emit clientConnected();
        } else {
            socket->disconnectFromHost();
            socket->deleteLater();
        }
    }
}

void Simulator::onClientDisconnected()
{
    if (m_client) {
        m_client->deleteLater();
        m_client = nullptr;
    }
    emit clientDisconnected();
}

void Simulator::onSendTimeout()
{
    // 没有客户端连接时不发送
    if (m_client == nullptr || m_client->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    sendPacket(generatePacket());
}

DataPacket Simulator::generatePacket()
{
    DataPacket packet;
    packet.pointCount = static_cast<quint16>(m_pointsPerPacket);
    packet.points.reserve(m_pointsPerPacket);

    QRandomGenerator *rng = QRandomGenerator::global();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 周期波动：每 200 包（约 10 秒）一个周期，后 100 包（约 5 秒）为“异常期”
    const bool anomalyActive = (m_packetIndex % 200) >= 100;
    const double ratio = anomalyActive ? m_anomalyRatio : 0.0;

    for (int i = 0; i < m_pointsPerPacket; ++i) {
        PointData p;
        // x/y 在 [-5, 5] 监测区域内均匀分布
        p.x = static_cast<float>(-5.0 + rng->generateDouble() * 10.0);
        p.y = static_cast<float>(-5.0 + rng->generateDouble() * 10.0);
        // 参考平面（略带斜坡），正常点在其附近加小噪声
        const double baseZ = 0.05 * p.x + 0.03 * p.y;
        p.z = static_cast<float>(baseZ + gaussianNoise(rng, 0.0, 0.04));
        // 强度 0~1
        p.intensity = static_cast<float>(rng->generateDouble());
        p.timestamp = now;
        packet.points.append(p);
    }

    // 随机挑选部分点作为“异常点”，让其 z 位移明显超过阈值
    if (ratio > 0.0) {
        for (int i = 0; i < m_pointsPerPacket; ++i) {
            if (rng->generateDouble() < ratio) {
                // 位移 0.8~1.2，明显超过默认阈值 0.5
                packet.points[i].z += static_cast<float>(0.8 + rng->generateDouble() * 0.4);
            }
        }
    }

    return packet;
}

void Simulator::sendPacket(const DataPacket &packet)
{
    // 打包成小端序二进制，写入 TCP
    const QByteArray data = Protocol::serializePacket(packet);
    m_client->write(data);
    m_client->flush();

    ++m_packetIndex;
    emit packetSent(static_cast<int>(m_packetIndex), packet.pointCount);
}
