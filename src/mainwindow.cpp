#include "mainwindow.h"

#include <QAction>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include "alarmmanager.h"
#include "databasemanager.h"
#include "datareceiver.h"
#include "pointcloudview.h"
#include "simulator.h"
#include "trendchart.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    setupModules();
    setupConnections();
    loadHistoryAlarms();
}

MainWindow::~MainWindow()
{
    if (m_monitoring) {
        stopMonitoring();
    }
    // 停止接收线程，再安全删除 worker
    m_receiverThread.quit();
    m_receiverThread.wait();
    delete m_receiver;
    m_receiver = nullptr;
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("雷达点云监测系统"));
    resize(1200, 800);

    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    // ---- 左侧：点云（主要）+ 曲线（固定高 200px）----
    QWidget *leftWidget = new QWidget;
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    m_pointCloudView = new PointCloudView;
    m_trendChart = new TrendChart;
    m_trendChart->setFixedHeight(200);

    leftLayout->addWidget(m_pointCloudView, 1);
    leftLayout->addWidget(m_trendChart);

    // ---- 右侧：监测信息面板 + 报警表格 ----
    QWidget *rightWidget = new QWidget;
    rightWidget->setFixedWidth(380);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);

    QGroupBox *infoGroup = new QGroupBox(QStringLiteral("监测信息"));
    QFormLayout *formLayout = new QFormLayout(infoGroup);
    m_connStatusLabel = new QLabel(QStringLiteral("未连接"));
    m_rateLabel = new QLabel(QStringLiteral("0 点/秒"));
    m_pointsLabel = new QLabel(QStringLiteral("0"));
    m_maxDispLabel = new QLabel(QStringLiteral("0.000"));
    m_thresholdLabel = new QLabel(QStringLiteral("0.500"));
    formLayout->addRow(QStringLiteral("连接状态:"), m_connStatusLabel);
    formLayout->addRow(QStringLiteral("接收速率:"), m_rateLabel);
    formLayout->addRow(QStringLiteral("当前点数:"), m_pointsLabel);
    formLayout->addRow(QStringLiteral("当前最大位移:"), m_maxDispLabel);
    formLayout->addRow(QStringLiteral("报警阈值:"), m_thresholdLabel);
    rightLayout->addWidget(infoGroup);

    m_alarmTable = new QTableWidget(0, 7);
    m_alarmTable->setHorizontalHeaderLabels({
        QStringLiteral("时间"), QStringLiteral("类型"),
        QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z"),
        QStringLiteral("当前值"), QStringLiteral("阈值")});
    m_alarmTable->horizontalHeader()->setStretchLastSection(true);
    m_alarmTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_alarmTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    rightLayout->addWidget(m_alarmTable, 1);

    // ---- 左右用分割器组合 ----
    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setSizes({800, 380});

    QHBoxLayout *mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->addWidget(splitter);

    // ---- 工具栏 ----
    QToolBar *toolbar = addToolBar(QStringLiteral("主工具栏"));
    toolbar->setMovable(false);
    m_startStopAction = toolbar->addAction(QStringLiteral("开始连接"));
    toolbar->addSeparator();
    m_configAction = toolbar->addAction(QStringLiteral("配置"));

    // ---- 状态栏：常驻统计（右侧）+ 临时消息（左侧）----
    m_statusConnLabel = new QLabel(QStringLiteral("连接: 未连接"));
    m_statusRateLabel = new QLabel(QStringLiteral("速率: 0 点/秒"));
    m_statusPointsLabel = new QLabel(QStringLiteral("点数: 0"));
    statusBar()->addPermanentWidget(m_statusConnLabel);
    statusBar()->addPermanentWidget(m_statusRateLabel);
    statusBar()->addPermanentWidget(m_statusPointsLabel);
    statusBar()->showMessage(QStringLiteral("就绪"));
}

void MainWindow::setupModules()
{
    m_simulator = new Simulator(this);
    m_alarmManager = new AlarmManager(this);
    m_database = new DatabaseManager(this);

    // 接收器是 worker 对象，移到工作线程（网络接收与解析在子线程完成）
    m_receiver = new DataReceiver();
    m_receiver->moveToThread(&m_receiverThread);
    m_receiverThread.start();

    // 初始化数据库
    if (!m_database->init()) {
        statusBar()->showMessage(QStringLiteral("数据库初始化失败"), 5000);
    }

    // 初始阈值同步到各模块
    m_alarmManager->setThreshold(m_threshold);
    m_pointCloudView->setThreshold(m_threshold);
    m_trendChart->setThreshold(m_threshold);

    // 每秒更新曲线的定时器
    m_trendTimer = new QTimer(this);
    m_trendTimer->setInterval(1000);
    connect(m_trendTimer, &QTimer::timeout, this, &MainWindow::onTrendTick);
}

void MainWindow::setupConnections()
{
    // 接收器（子线程）→ 报警判断 / 界面（主线程），自动走队列连接
    connect(m_receiver, &DataReceiver::packetReceived,
            m_alarmManager, &AlarmManager::processPacket);
    connect(m_receiver, &DataReceiver::packetReceived,
            this, &MainWindow::onPacketReceived);
    connect(m_receiver, &DataReceiver::connectionChanged,
            this, &MainWindow::onConnectionChanged);
    connect(m_receiver, &DataReceiver::statsUpdated,
            this, &MainWindow::onStatsUpdated);

    // 报警 → 数据库 + 表格
    connect(m_alarmManager, &AlarmManager::alarmTriggered,
            this, &MainWindow::onAlarmTriggered);
    connect(m_alarmManager, &AlarmManager::alarmCleared,
            this, &MainWindow::onAlarmCleared);

    // 模拟器错误提示
    connect(m_simulator, &Simulator::errorOccurred, this, [this](const QString &msg) {
        statusBar()->showMessage(QStringLiteral("模拟设备错误: ") + msg, 5000);
    });

    // 工具栏
    connect(m_startStopAction, &QAction::triggered, this, &MainWindow::onStartStop);
    connect(m_configAction, &QAction::triggered, this, &MainWindow::onConfig);
}

void MainWindow::startMonitoring()
{
    m_monitoring = true;
    m_startStopAction->setText(QStringLiteral("停止连接"));
    m_connStatusLabel->setText(QStringLiteral("连接中..."));

    // 启动模拟设备并连接（连接动作通过队列调用，在子线程执行）
    m_simulator->start(m_devicePort);
    QMetaObject::invokeMethod(m_receiver, "connectToDevice", Qt::QueuedConnection,
                              Q_ARG(QHostAddress, m_deviceAddr),
                              Q_ARG(quint16, m_devicePort));
}

void MainWindow::stopMonitoring()
{
    m_monitoring = false;
    m_startStopAction->setText(QStringLiteral("开始连接"));
    m_connStatusLabel->setText(QStringLiteral("未连接"));

    if (m_receiver) {
        QMetaObject::invokeMethod(m_receiver, "disconnectFromDevice", Qt::QueuedConnection);
    }
    m_simulator->stop();
    m_trendTimer->stop();

    m_pointCloudView->clear();
    m_trendChart->clear();
    m_alarmManager->resetBaseline();
}

void MainWindow::onStartStop()
{
    if (m_monitoring) {
        stopMonitoring();
    } else {
        startMonitoring();
    }
}

void MainWindow::onConfig()
{
    // 配置对话框：设备 IP、端口、位移阈值
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("配置"));

    QFormLayout *form = new QFormLayout(&dialog);

    QLineEdit *ipEdit = new QLineEdit(m_deviceAddr.toString());
    QSpinBox *portSpin = new QSpinBox;
    portSpin->setRange(1, 65535);
    portSpin->setValue(m_devicePort);
    QDoubleSpinBox *thresholdSpin = new QDoubleSpinBox;
    thresholdSpin->setRange(0.0, 10.0);
    thresholdSpin->setDecimals(2);
    thresholdSpin->setSingleStep(0.05);
    thresholdSpin->setValue(m_threshold);

    form->addRow(QStringLiteral("设备 IP:"), ipEdit);
    form->addRow(QStringLiteral("端口:"), portSpin);
    form->addRow(QStringLiteral("位移阈值:"), thresholdSpin);

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    // 应用新配置
    m_deviceAddr = QHostAddress(ipEdit->text().trimmed());
    m_devicePort = static_cast<quint16>(portSpin->value());
    m_threshold = thresholdSpin->value();

    m_alarmManager->setThreshold(m_threshold);
    m_pointCloudView->setThreshold(m_threshold);
    m_trendChart->setThreshold(m_threshold);
    m_thresholdLabel->setText(QString::number(m_threshold, 'f', 3));
    statusBar()->showMessage(QStringLiteral("配置已更新"), 3000);
}

void MainWindow::onPacketReceived(const DataPacket &packet)
{
    m_pointCloudView->updatePoints(packet.points);
    m_pointsLabel->setText(QString::number(packet.pointCount));
    m_maxDispLabel->setText(
        QString::number(m_alarmManager->currentMaxDisplacement(), 'f', 3));
    m_statusPointsLabel->setText(QStringLiteral("点数: %1").arg(packet.pointCount));
}

void MainWindow::onConnectionChanged(bool connected)
{
    m_connStatusLabel->setText(connected ? QStringLiteral("已连接")
                                         : QStringLiteral("未连接"));
    m_statusConnLabel->setText(
        QStringLiteral("连接: %1").arg(connected ? QStringLiteral("已连接")
                                                 : QStringLiteral("未连接")));
    if (connected) {
        m_trendTimer->start();
        statusBar()->showMessage(QStringLiteral("已连接设备"), 3000);
    } else {
        m_trendTimer->stop();
    }
}

void MainWindow::onStatsUpdated(int pointsPerSecond, int packetsPerSecond)
{
    m_rateLabel->setText(QStringLiteral("%1 点/秒 (%2 包/秒)")
                             .arg(pointsPerSecond).arg(packetsPerSecond));
    m_statusRateLabel->setText(QStringLiteral("速率: %1 点/秒").arg(pointsPerSecond));
}

void MainWindow::onAlarmTriggered(const AlarmRecord &record)
{
    // 写入数据库并更新表格
    m_database->insertAlarm(record);
    appendAlarmRow(record);
    statusBar()->showMessage(QStringLiteral("⚠ 触发报警"), 3000);
}

void MainWindow::onAlarmCleared()
{
    statusBar()->showMessage(QStringLiteral("报警已解除"), 3000);
}

void MainWindow::onTrendTick()
{
    // 每秒把当前最大位移追加到曲线
    m_trendChart->appendPoint(m_alarmManager->currentMaxDisplacement());
}

void MainWindow::appendAlarmRow(const AlarmRecord &record)
{
    // 新报警插入到表格顶部
    m_alarmTable->insertRow(0);

    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(record.timestamp);
    const QString timeStr = dt.toString(QStringLiteral("HH:mm:ss"));
    const QString typeStr = (record.type == 0) ? QStringLiteral("位移超限")
                                               : QStringLiteral("比例超限");

    m_alarmTable->setItem(0, 0, new QTableWidgetItem(timeStr));
    m_alarmTable->setItem(0, 1, new QTableWidgetItem(typeStr));
    m_alarmTable->setItem(0, 2, new QTableWidgetItem(QString::number(record.x, 'f', 2)));
    m_alarmTable->setItem(0, 3, new QTableWidgetItem(QString::number(record.y, 'f', 2)));
    m_alarmTable->setItem(0, 4, new QTableWidgetItem(QString::number(record.z, 'f', 2)));
    m_alarmTable->setItem(0, 5, new QTableWidgetItem(QString::number(record.value, 'f', 3)));
    m_alarmTable->setItem(0, 6, new QTableWidgetItem(QString::number(record.threshold, 'f', 3)));
}

void MainWindow::loadHistoryAlarms()
{
    const QVector<AlarmRecord> alarms = m_database->loadRecentAlarms(100);
    // 倒序遍历（最旧→最新），每次插入顶部，最终顶部为最新
    for (int i = alarms.size() - 1; i >= 0; --i) {
        appendAlarmRow(alarms[i]);
    }
}
