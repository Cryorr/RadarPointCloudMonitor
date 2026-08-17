#include "alarmmanager.h"

#include <QDateTime>

#include <cmath>

namespace {

constexpr double kAnomalyRatioThreshold = 0.05;   // 异常比例 > 5% 触发报警
constexpr double kClearRatioThreshold = 0.02;     // 异常比例 < 2% 解除报警
constexpr int kDebounceFrames = 3;                // 去抖：连续 3 帧
constexpr int kBaselineFrames = 100;              // 基准值：前 100 帧

} // namespace

AlarmManager::AlarmManager(QObject *parent)
    : QObject(parent)
{
}

void AlarmManager::setThreshold(double value)
{
    m_threshold = value;
}

double AlarmManager::threshold() const
{
    return m_threshold;
}

double AlarmManager::referenceZ() const
{
    return m_referenceZ;
}

bool AlarmManager::baselineReady() const
{
    return m_baselineReady;
}

bool AlarmManager::isAlarming() const
{
    return m_alarming;
}

double AlarmManager::currentMaxDisplacement() const
{
    return m_currentMaxDisplacement;
}

void AlarmManager::resetBaseline()
{
    // 清空基准值与去抖状态，重新开始收集 100 帧
    m_baselineReady = false;
    m_baselineFrames = 0;
    m_baselineSumZ = 0.0;
    m_baselinePointCount = 0;
    m_referenceZ = 0.0;
    m_alarming = false;
    m_triggerCount = 0;
    m_clearCount = 0;
}

void AlarmManager::processPacket(const DataPacket &packet)
{
    if (packet.points.isEmpty()) {
        return;
    }

    // ---- 阶段 1：基准值未建立，先累积前 100 帧的 z 求平均 ----
    if (!m_baselineReady) {
        for (const PointData &p : packet.points) {
            m_baselineSumZ += p.z;
        }
        m_baselinePointCount += packet.points.size();
        ++m_baselineFrames;

        if (m_baselineFrames >= kBaselineFrames) {
            m_referenceZ = m_baselineSumZ / static_cast<double>(m_baselinePointCount);
            m_baselineReady = true;
        }
        return;   // 基准期不判断报警
    }

    // ---- 阶段 2：计算本帧异常点比例 ----
    int anomalyCount = 0;
    double maxDisplacement = 0.0;
    PointData worstPoint;   // 位移最大的点（用于报警记录）

    for (const PointData &p : packet.points) {
        const double disp = std::fabs(p.z - m_referenceZ);
        if (disp > m_threshold) {
            ++anomalyCount;
            if (disp > maxDisplacement) {
                maxDisplacement = disp;
                worstPoint = p;
            }
        }
    }
    const double ratio = static_cast<double>(anomalyCount) / packet.points.size();
    m_currentMaxDisplacement = maxDisplacement;   // 记录本帧最大位移，供曲线/面板显示

    // ---- 阶段 3：更新去抖计数器 ----
    m_triggerCount = (ratio > kAnomalyRatioThreshold) ? m_triggerCount + 1 : 0;
    m_clearCount = (ratio < kClearRatioThreshold) ? m_clearCount + 1 : 0;

    // ---- 阶段 4：连续 3 帧超阈值 → 触发报警 ----
    if (!m_alarming && m_triggerCount >= kDebounceFrames) {
        m_alarming = true;

        AlarmRecord record;
        record.timestamp = QDateTime::currentMSecsSinceEpoch();
        record.type = 1;   // 1 = 异常点数比例超限
        record.x = worstPoint.x;
        record.y = worstPoint.y;
        record.z = worstPoint.z;
        // 报警记录里 value/threshold 表示“异常比例 / 比例阈值”
        record.value = static_cast<float>(ratio);
        record.threshold = static_cast<float>(kAnomalyRatioThreshold);

        emit alarmTriggered(record);
    }

    // ---- 阶段 5：连续 3 帧低于回落阈值 → 解除报警 ----
    if (m_alarming && m_clearCount >= kDebounceFrames) {
        m_alarming = false;
        emit alarmCleared();
    }
}
