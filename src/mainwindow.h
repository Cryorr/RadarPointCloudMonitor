#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QHostAddress>
#include <QMainWindow>
#include <QThread>

#include "protocol.h"

class QLabel;
class QTableWidget;
class QAction;
class QTimer;
class Simulator;
class DataReceiver;
class AlarmManager;
class DatabaseManager;
class PointCloudView;
class TrendChart;

// 主窗口（SPEC 6.7）：负责组装所有模块并连接信号槽
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onStartStop();                                  // 开始/停止连接
    void onConfig();                                     // 配置对话框
    void onPacketReceived(const DataPacket &packet);     // 收到一包
    void onConnectionChanged(bool connected);            // 连接状态变化
    void onStatsUpdated(int pointsPerSecond, int packetsPerSecond); // 速率统计
    void onAlarmTriggered(const AlarmRecord &record);    // 报警触发
    void onAlarmCleared();                               // 报警解除
    void onTrendTick();                                  // 每秒更新曲线

private:
    void setupUi();          // 构建界面
    void setupModules();     // 创建各业务模块
    void setupConnections(); // 连接信号槽
    void startMonitoring();  // 启动模拟器并连接
    void stopMonitoring();   // 停止并断开
    void appendAlarmRow(const AlarmRecord &record); // 往表格加一行
    void loadHistoryAlarms();                       // 加载历史报警

    // 业务模块
    Simulator *m_simulator = nullptr;
    DataReceiver *m_receiver = nullptr;
    QThread m_receiverThread;      // 接收器所在的工作线程
    AlarmManager *m_alarmManager = nullptr;
    DatabaseManager *m_database = nullptr;

    // UI 控件（右侧面板）
    PointCloudView *m_pointCloudView = nullptr;
    TrendChart *m_trendChart = nullptr;
    QTableWidget *m_alarmTable = nullptr;
    QLabel *m_connStatusLabel = nullptr;
    QLabel *m_rateLabel = nullptr;
    QLabel *m_pointsLabel = nullptr;
    QLabel *m_maxDispLabel = nullptr;
    QLabel *m_thresholdLabel = nullptr;

    // UI 控件（状态栏常驻统计）
    QLabel *m_statusConnLabel = nullptr;
    QLabel *m_statusRateLabel = nullptr;
    QLabel *m_statusPointsLabel = nullptr;

    QAction *m_startStopAction = nullptr;
    QAction *m_configAction = nullptr;
    QTimer *m_trendTimer = nullptr;

    // 状态
    bool m_monitoring = false;
    double m_threshold = 0.5;
    QHostAddress m_deviceAddr = QHostAddress::LocalHost;
    quint16 m_devicePort = 9000;
};

#endif // MAINWINDOW_H
