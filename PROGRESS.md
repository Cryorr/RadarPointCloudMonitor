# 项目进度记录

> 本文件随开发逐步更新，记录当前进度、已完成的模块、最近编译运行结果、下一步计划与待解决问题。

---

## 1. 当前完成到第几步

**全部 10 步已完成** ✅

```
[1]  项目骨架 + CMakeLists + MainWindow 空壳        ✅
[2]  protocol.h / protocol.cpp（协议）              ✅
[3]  Simulator（模拟设备）+ 命令行测试协议           ✅
[4]  DataReceiver（子线程接收与解析）               ✅
[5]  PointCloudView（点云绘制）                     ✅
[6]  TrendChart（实时曲线，QCustomPlot）            ✅
[7]  AlarmManager（预警与去抖）                     ✅
[8]  DatabaseManager（SQLite 报警存取）             ✅
[9]  MainWindow 组装全部信号槽与布局                ✅
[10] 配置对话框、状态栏统计、README、截图           ✅
```

---

## 2. 已完成哪些模块 / 文件

| 文件 | 说明 |
|------|------|
| `CMakeLists.txt` | 构建脚本（全部模块 + QCustomPlot + PrintSupport） |
| `src/main.cpp` | 入口 + qRegisterMetaType |
| `src/mainwindow.h/.cpp` | 主窗口组装（布局 + 信号槽 + 线程 + 配置对话框） |
| `src/protocol.h/.cpp` | 数据结构 + 打包/拆包 |
| `src/simulator.h/.cpp` | 模拟雷达设备 |
| `src/datareceiver.h/.cpp` | 接收 + 粘包/半包解析 + 统计 |
| `src/pointcloudview.h/.cpp` | 点云绘制 |
| `src/trendchart.h/.cpp` | 实时曲线 + 阈值线 |
| `src/alarmmanager.h/.cpp` | 预警判定 + 基准值 + 去抖 |
| `src/databasemanager.h/.cpp` | SQLite 报警存取 |
| `third_party/qcustomplot.h/.cpp` | QCustomPlot 曲线库 |
| `README.md` | 架构说明 + 编译方法 + 使用步骤 |
| `ERROR_LOG.md` | 错误记录（最终错误报告） |
| `screenshot.png` | 演示截图 |

---

## 3. 最近一次编译和运行结果

- **编译**：`ninja`（命令行）与 `mingw32-make`（Qt Creator 默认 MinGW Makefiles）**均编译通过，零警告**。
- **运行**：Qt Creator 中可正常编译并运行主窗口；命令行验证生成演示截图 `screenshot.png`（1200×800）。
- **已解决**：Qt Creator Debug 构建下 QCustomPlot "too many sections" 错误（对第三方库 qcustomplot.cpp 加 `-g0`，详见 `ERROR_LOG.md` 步骤 10）。

---

## 4. 下一步要做什么

无 —— 项目已按 SPEC 全部完成，可交付。

---

## 5. 当前还没解决的问题 / 注意点

无阻塞问题。可选扩展方向（非本项目要求）：

- 接入真实雷达硬件（替换 Simulator）；
- 支持多客户端连接；
- 报警记录导出 / 图表统计；
- 三维点云可视化（当前为二维投影）。
