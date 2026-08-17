# 开发错误记录

本文件记录项目开发过程中遇到的所有错误及处理方法，按开发步骤组织，最终汇总成错误报告。

---

## 步骤 1：项目骨架

无错误（骨架已存在，仅做编译与运行验证）。

---

## 步骤 2：protocol（协议）

无编译错误。本步骤埋下一个隐患：`QDataStream` 浮点序列化的字节数问题，在步骤 3 测试时暴露。

---

## 步骤 3：Simulator + 协议测试

### 错误 1：`QRandomGenerator::bounded()` 重载不明确
- **现象**：编译报错 `error: call of overloaded 'bounded(double, double)' is ambiguous`
- **原因**：`QRandomGenerator::bounded()` 只提供整数重载（`quint32` / `int` / `quint64` / `qint64`），没有 `double` 版本。
- **处理**：改用 `generateDouble()`（返回 `[0,1)`），手动映射到目标区间，例如 `-5.0 + rng->generateDouble() * 10.0`。

### 错误 2：序列化字节数错误（预期 81，实际 129）
- **现象**：命令行测试报 `[FAIL] serialize: byte size == 81`，实际为 129；进而导致粘包第二包解析失败、模拟设备收包数不足。
- **定位**：加调试代码逐字节打印十六进制，发现 float 字段被写成 8 字节（如 x=1.0 本应 `00 00 80 3f`，实际 `00 00 00 00 00 00 f0 3f`）。
- **原因**：`QDataStream::setFloatingPointPrecision()` 是「统一精度」——`DoublePrecision` 会把 `float` 也写成 8 字节，`SinglePrecision` 会把 `double` 也压成 4 字节，无法满足协议「float=4 字节、double=8 字节」的混合要求。
- **处理**：绕开 QDataStream 的浮点处理，手动按位转换：用 `memcpy` 把 `float` 搬进 `quint32`、`double` 搬进 `quint64`，再让 QDataStream 写整数（大小完全可控）。

---

## 步骤 4：DataReceiver（子线程接收与解析）

无错误（一次编译通过，命令行测试全部通过）。

---

## 步骤 5：PointCloudView（点云绘制）

无错误（一次编译通过，像素颜色验证全部通过）。

---

## 步骤 6：TrendChart（实时曲线）

### 问题：QCustomPlot 第三方库产生 deprecated 警告
- **现象**：编译 `third_party/qcustomplot.cpp` 时产生 6 处 `warning: ... is deprecated`（`QImage::mirrored`、`QDateTime::toTimeSpec`、`QDate::startOfDay`）。
- **原因**：QCustomPlot 2.1.1（2022 年）使用了 Qt 6.11 已废弃的旧 API，属第三方库兼容性问题，非本项目代码。
- **处理**：在 CMakeLists.txt 中用 `set_source_files_properties(third_party/qcustomplot.cpp PROPERTIES COMPILE_OPTIONS "-Wno-deprecated-declarations")` 单独抑制该文件的弃用警告，保持本项目自身代码零警告。

---

## 步骤 7：AlarmManager（预警与去抖）

无错误（一次编译通过，报警判定与去抖测试全部通过）。

---

## 步骤 8：DatabaseManager（SQLite 报警存取）

无错误（一次编译通过，存取与持久化测试全部通过）。

---

## 步骤 9：MainWindow 组装

### 验证时发现报警记录为 0
- **现象**：运行主程序后，数据库 `alarms` 表为空（报警记录 0 条）。
- **原因**：主程序启动后需手动点击「开始连接」才启动监测，直接运行 exe 未触发数据链路。
- **处理**：临时在构造函数加 `QTimer::singleShot` 自动开始监测做验证，确认「模拟器→接收→报警→数据库」链路正常（写入 1 条报警记录），验证后移除临时代码，保持手动连接设计。

---

## 步骤 10：配置对话框、状态栏统计、README、演示截图

### 错误：QCustomPlot 在 Qt Creator 的 Debug 构建下编译失败（too many sections）
- **现象**：用户在 Qt Creator 里点运行报 `mingw32-make: Error 2`，实际错误为 `as.exe: too many sections (33153)`、`Fatal error: file too big`，发生在编译 `third_party/qcustomplot.cpp` 时。
- **原因**：`qcustomplot.cpp` 是约 3 万行的巨型单文件；Qt Creator 默认 Debug 构建带 `-g` 调试信息，使目标文件 section 数超 MinGW 汇编器上限。命令行用 Release 构建（无 `-g`）所以能通过。
- **处理**：在 CMakeLists.txt 中对 `qcustomplot.cpp` 单独加编译选项 `-g0`（禁用调试信息，第三方库无需调试），与之前的 `-Wno-deprecated-declarations` 合并为 `"-Wno-deprecated-declarations;-g0"`。修复后 mingw32-make 完整编译通过。
