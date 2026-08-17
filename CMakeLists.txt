cmake_minimum_required(VERSION 3.16)
project(RadarPointCloudMonitor VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 自动处理 Q_OBJECT 宏所需的 moc
set(CMAKE_AUTOMOC ON)

# QCustomPlot 需要 PrintSupport（导出 PDF 用）
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Sql PrintSupport)

# QCustomPlot 2.1.1 是第三方库、非本项目代码，单独设置该文件的编译选项：
#  - -Wno-deprecated-declarations：抑制 Qt6.11 上的弃用告警，保证本项目自身代码零告警；
#  - -g0：禁用调试信息。qcustomplot.cpp 是约 3 万行的巨型单文件，Debug 构建带 -g 时
#         目标文件 section 数会超 MinGW 汇编器上限（报 "too many sections"），
#         第三方库无需调试信息，故单独关闭。
set_source_files_properties(third_party/qcustomplot.cpp PROPERTIES
    COMPILE_OPTIONS "-Wno-deprecated-declarations;-g0"
)

# 主程序：按开发顺序逐步追加源文件
add_executable(RadarPointCloudMonitor
    src/main.cpp
    src/mainwindow.cpp
    src/protocol.cpp
    src/simulator.cpp
    src/datareceiver.cpp
    src/pointcloudview.cpp
    src/trendchart.cpp
    src/alarmmanager.cpp
    src/databasemanager.cpp
    third_party/qcustomplot.cpp
)

target_include_directories(RadarPointCloudMonitor PRIVATE src third_party)
target_link_libraries(RadarPointCloudMonitor PRIVATE Qt6::Widgets Qt6::Network Qt6::Sql Qt6::PrintSupport)

# 编译告警要求（MinGW 使用 -Wall -Wextra，做到零警告）
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(RadarPointCloudMonitor PRIVATE -Wall -Wextra)
endif()
