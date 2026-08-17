#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include "protocol.h"

// 报警记录数据库管理（SPEC 6.6）
//  - 使用 Qt SQL + SQLite，数据库文件 radar_monitor.db（程序工作目录）
//  - 表结构按 SPEC 第 7 节
class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager() override;

    // 打开数据库并建表（对外方法）
    bool init();
    // 插入一条报警记录（对外方法）
    bool insertAlarm(const AlarmRecord &record);
    // 加载最近 limit 条报警记录，最新的在前（对外方法）
    QVector<AlarmRecord> loadRecentAlarms(int limit);

    bool isOpen() const;

private:
    bool createTable();

    QSqlDatabase m_db;
    QString m_dbFile = QStringLiteral("radar_monitor.db");
};

#endif // DATABASEMANAGER_H
