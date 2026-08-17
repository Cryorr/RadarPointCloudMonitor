#ifndef ALARMMANAGER_H
#define ALARMMANAGER_H

#include <QObject>

#include "protocol.h"

// 预警逻辑（SPEC 6.5）
//  - 继承 QObject，接收 DataPacket 判断是否触发报警
//  - 基准值：取前 100 帧的平均 z 作为参考平面
//  - 单点位移 = abs(z - referenceZ)，位移阈值默认 0.5
//  - 异常点比例 > 5% 且连续 3 帧触发报警（去抖）
//  - 比例回落 < 2% 且连续 3 帧解除报警
class AlarmManager : public QObject
{
    Q_OBJECT

public:
    explicit AlarmManager(QObject *parent = nullptr);

    void setThreshold(double value);   // 设置位移阈值
    void resetBaseline();              // 重置基准值，重新收集 100 帧

    double threshold() const;
    double referenceZ() const;              // 当前参考平面 z
    double currentMaxDisplacement() const;  // 最近一帧的最大位移
    bool baselineReady() const;             // 基准值是否已建立
    bool isAlarming() const;                // 当前是否报警中

signals:
    void alarmTriggered(const AlarmRecord &record);   // 触发报警
    void alarmCleared();                              // 解除报警

public slots:
    void processPacket(const DataPacket &packet);     // 处理一包点云

private:
    // 基准值相关
    bool m_baselineReady = false;
    int m_baselineFrames = 0;
    double m_baselineSumZ = 0.0;
    qint64 m_baselinePointCount = 0;
    double m_referenceZ = 0.0;

    // 报警判定相关
    double m_threshold = 0.5;   // 位移阈值
    bool m_alarming = false;    // 是否报警中
    int m_triggerCount = 0;     // 连续超比例帧数
    int m_clearCount = 0;       // 连续回落帧数
    double m_currentMaxDisplacement = 0.0;   // 最近一帧最大位移
};

#endif // ALARMMANAGER_H
