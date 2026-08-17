#include <QApplication>

#include "mainwindow.h"
#include "protocol.h"

// 程序入口：注册自定义类型并创建主窗口
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 注册自定义类型，确保跨线程信号槽（QueuedConnection）能传递它们
    qRegisterMetaType<PointData>("PointData");
    qRegisterMetaType<DataPacket>("DataPacket");
    qRegisterMetaType<AlarmRecord>("AlarmRecord");

    MainWindow window;
    window.show();

    return app.exec();
}
