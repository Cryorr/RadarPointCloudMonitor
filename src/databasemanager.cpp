#include "databasemanager.h"

#include <QSqlError>
#include <QSqlQuery>

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{
}

DatabaseManager::~DatabaseManager()
{
    if (m_db.isValid() && m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseManager::isOpen() const
{
    return m_db.isOpen();
}

bool DatabaseManager::init()
{
    // 使用默认连接的 SQLite 驱动；若连接已存在则复用，避免重复 addDatabase
    if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
        m_db = QSqlDatabase::database(QSqlDatabase::defaultConnection, false);
    } else {
        m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    }

    // 数据库文件位于程序工作目录
    m_db.setDatabaseName(m_dbFile);

    if (!m_db.isOpen() && !m_db.open()) {
        qWarning("DatabaseManager: open failed: %s",
                 qPrintable(m_db.lastError().text()));
        return false;
    }

    return createTable();
}

bool DatabaseManager::createTable()
{
    QSqlQuery query(m_db);
    const QString sql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS alarms ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp INTEGER NOT NULL,"
        "type INTEGER NOT NULL,"
        "x REAL NOT NULL,"
        "y REAL NOT NULL,"
        "z REAL NOT NULL,"
        "value REAL NOT NULL,"
        "threshold REAL NOT NULL"
        ")");

    if (!query.exec(sql)) {
        qWarning("DatabaseManager: create table failed: %s",
                 qPrintable(query.lastError().text()));
        return false;
    }
    return true;
}

bool DatabaseManager::insertAlarm(const AlarmRecord &record)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO alarms (timestamp, type, x, y, z, value, threshold) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)"));

    query.addBindValue(record.timestamp);
    query.addBindValue(record.type);
    query.addBindValue(record.x);
    query.addBindValue(record.y);
    query.addBindValue(record.z);
    query.addBindValue(record.value);
    query.addBindValue(record.threshold);

    if (!query.exec()) {
        qWarning("DatabaseManager: insert failed: %s",
                 qPrintable(query.lastError().text()));
        return false;
    }
    return true;
}

QVector<AlarmRecord> DatabaseManager::loadRecentAlarms(int limit)
{
    QVector<AlarmRecord> result;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT timestamp, type, x, y, z, value, threshold FROM alarms "
        "ORDER BY id DESC LIMIT ?"));
    query.addBindValue(limit);

    if (!query.exec()) {
        qWarning("DatabaseManager: query failed: %s",
                 qPrintable(query.lastError().text()));
        return result;
    }

    while (query.next()) {
        AlarmRecord record;
        record.timestamp = query.value(0).toLongLong();
        record.type = query.value(1).toInt();
        record.x = query.value(2).toFloat();
        record.y = query.value(3).toFloat();
        record.z = query.value(4).toFloat();
        record.value = query.value(5).toFloat();
        record.threshold = query.value(6).toFloat();
        result.append(record);
    }
    return result;
}
