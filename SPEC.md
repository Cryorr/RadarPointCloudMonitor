# Qt 雷达点云监测上位机 - 开发规格

## 1. 项目目标

开发一个基于 Qt6 的雷达点云监测上位机 Demo，用于模拟工业雷达监测场景：

- 模拟雷达设备通过 TCP 持续发送点云数据
- 上位机接收、解析、实时显示点云
- 对位移超阈值的数据触发预警
- 将报警记录保存到 SQLite，重启后可查询历史

本项目为技术演示项目，要求代码结构清晰、可编译运行、可现场演示，不要求接入真实硬件。

## 2. 技术栈

- Qt 6.5 或更高版本，MinGW 64-bit
- Qt Creator 或命令行 CMake 构建
- C++17
- Qt Widgets（界面）
- QPainter（点云绘制）
- QCustomPlot（实时曲线，需将 qcustomplot.h / qcustomplot.cpp 放入 third_party/）
- QTcpServer / QTcpSocket（模拟设备与通信）
- QThread 或 QtConcurrent（子线程接收与解析）
- Qt SQL + SQLite（报警记录）
- CMake

## 3. 目录结构

```text
RadarPointCloudMonitor/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── mainwindow.h
│   ├── mainwindow.cpp
│   ├── simulator.h
│   ├── simulator.cpp
│   ├── datareceiver.h
│   ├── datareceiver.cpp
│   ├── pointcloudview.h
│   ├── pointcloudview.cpp
│   ├── trendchart.h
│   ├── trendchart.cpp
│   ├── alarmmanager.h
│   ├── alarmmanager.cpp
│   ├── databasemanager.h
│   ├── databasemanager.cpp
│   ├── protocol.h
│   └── protocol.cpp
├── third_party/
│   ├── qcustomplot.h
│   └── qcustomplot.cpp
└── README.md
```

## 4. 数据协议

模拟雷达设备作为 TCP 服务端，监听 `127.0.0.1:9000`；上位机作为 TCP 客户端连接。

### 4.1 二进制包格式（小端序）

```text
偏移  长度    字段
0     2      magic     固定 0xAA55
2     1      version   固定 1
3     2      pointCount   本包点数 N，uint16
5     4      payloadLength 数据区字节数，uint32
9     N*24   数据区
```

### 4.2 单点数据（24 字节）

```text
类型      字段
float32  x
float32  y
float32  z
float32  intensity
double   timestamp  （Unix 毫秒时间戳）
```

### 4.3 发送频率

- 每秒 20 个数据包
- 每包 50 个点
- 上位机收到的点数为每秒约 1000 点

## 5. 核心数据结构

```cpp
struct PointData {
    float x;
    float y;
    float z;
    float intensity;
    qint64 timestamp; // 毫秒
};

struct DataPacket {
    quint16 pointCount;
    QVector<PointData> points;
};

struct AlarmRecord {
    qint64 timestamp;
    int type;        // 0=位移超限，1=异常点数比例超限
    float x;
    float y;
    float z;
    float value;
    float threshold;
};
```

## 6. 类职责与接口

### 6.1 Simulator（模拟设备）

- 继承 QObject
- 使用 QTcpServer 监听端口
- 使用 QTimer 定时生成数据包并发送
- 生成数据：在一个参考平面或斜坡附近加入噪声，模拟正常点
- 模拟异常：随机让部分点的 z 坐标位移超过阈值
- 对外接口：
  - `void start(quint16 port)`
  - `void stop()`

### 6.2 DataReceiver（接收与解析）

- 继承 QObject，内部使用 QTcpSocket
- 工作线程中持续读取，按第 4 节协议解析
- 使用 QByteArray 接收缓冲区解决 TCP 粘包/半包
- 对外信号：
  - `void packetReceived(const DataPacket &packet)`
  - `void connectionChanged(bool connected)`
  - `void statsUpdated(int pointsPerSecond, int packetsPerSecond)`
- 对外槽：
  - `void connectToDevice(const QHostAddress &addr, quint16 port)`
  - `void disconnectFromDevice()`

### 6.3 PointCloudView（点云绘图控件）

- 继承 QWidget，重写 `paintEvent(QPaintEvent*)`
- 保存最近一包点云 `QVector<PointData>`
- 将 x/y 映射到控件坐标，z 或 intensity 映射颜色
- 正常点：低强度蓝色、中强度绿色、高强度黄色
- 异常点：红色
- 对外方法：`void updatePoints(const QVector<PointData> &points)`

### 6.4 TrendChart（实时曲线）

- 封装 QCustomPlot
- 每秒追加一个点：当前帧最大位移或平均位移
- 画一条固定的阈值水平线
- 对外方法：`void appendPoint(double value)`、`void setThreshold(double value)`

### 6.5 AlarmManager（预警逻辑）

- 继承 QObject
- 接收 DataPacket，判断是否触发报警
- 规则：
  - 基准值：取前 100 帧的平均 z 作为参考平面
  - 单点位移 = `abs(z - referenceZ)`
  - 位移阈值默认 0.5，可配置
  - 异常点比例超过 5% 且连续 3 帧触发报警（去抖）
  - 比例回落到 2% 以下且连续 3 帧后解除报警
- 对外信号：
  - `void alarmTriggered(const AlarmRecord &record)`
  - `void alarmCleared()`
- 对外方法：`void setThreshold(double value)`、`void resetBaseline()`

### 6.6 DatabaseManager（SQLite）

- 继承 QObject
- 数据库文件：`radar_monitor.db`，位于程序工作目录
- 对外方法：
  - `bool init()`
  - `bool insertAlarm(const AlarmRecord &record)`
  - `QVector<AlarmRecord> loadRecentAlarms(int limit)`

### 6.7 MainWindow（主窗口）

- 负责组装所有模块
- 左侧：PointCloudView 占主要区域，下方 TrendChart 固定高度约 200px
- 右侧：监测信息面板 + 报警记录表格
- 工具栏：开始连接 / 停止连接 / 配置
- 状态栏：连接状态、接收速率、当前点数
- 所有 UI 更新通过信号槽在主线程执行

## 7. 数据库表结构

```sql
CREATE TABLE IF NOT EXISTS alarms (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,
    type INTEGER NOT NULL,
    x REAL NOT NULL,
    y REAL NOT NULL,
    z REAL NOT NULL,
    value REAL NOT NULL,
    threshold REAL NOT NULL
);
```

启动时加载最近 100 条报警记录到表格。

## 8. 构建配置

CMakeLists.txt 要点：

```cmake
cmake_minimum_required(VERSION 3.16)
project(RadarPointCloudMonitor VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Sql)

add_executable(RadarPointCloudMonitor
    src/main.cpp
    src/mainwindow.cpp
    src/simulator.cpp
    src/datareceiver.cpp
    src/pointcloudview.cpp
    src/trendchart.cpp
    src/alarmmanager.cpp
    src/databasemanager.cpp
    src/protocol.cpp
    third_party/qcustomplot.cpp
)

target_include_directories(RadarPointCloudMonitor PRIVATE src third_party)
target_link_libraries(RadarPointCloudMonitor PRIVATE Qt6::Widgets Qt6::Network Qt6::Sql)
```

要求：编译零警告（MinGW 使用 `-Wall -Wextra`）。

## 9. 界面文案（中文）

- 标题：雷达点云监测系统
- 连接按钮：开始连接 / 停止连接
- 监测信息：连接状态 / 接收速率 / 当前点数 / 当前最大位移 / 报警阈值
- 报警表格列：时间 / 类型 / X / Y / Z / 当前值 / 阈值
- 配置项：设备 IP、端口、位移阈值

## 10. 开发顺序

1. 创建项目骨架、CMakeLists、MainWindow 空壳，能编译运行
2. 实现 protocol.h/protocol.cpp（包格式常量、序列化、解析函数）
3. 实现 Simulator（模拟设备数据源），先用命令行测试协议
4. 实现 DataReceiver（子线程接收、缓冲区、粘包解析）
5. 实现 PointCloudView（点云绘制）
6. 实现 TrendChart（实时曲线）
7. 实现 AlarmManager（预警与去抖）
8. 实现 DatabaseManager（SQLite 报警存取）
9. 在 MainWindow 中完成全部信号槽连接与界面布局
10. 配置对话框、状态栏统计、README、演示截图
