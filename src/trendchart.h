#ifndef TRENDCHART_H
#define TRENDCHART_H

#include <QVector>
#include <QWidget>

#include "qcustomplot.h"

// 实时曲线控件（SPEC 6.4）
//  - 封装 QCustomPlot
//  - 每秒追加一个点（当前帧最大/平均位移），并画一条阈值水平线
class TrendChart : public QWidget
{
    Q_OBJECT

public:
    explicit TrendChart(QWidget *parent = nullptr);
    ~TrendChart() override;

    // 追加一个数据点（对外方法）
    void appendPoint(double value);
    // 设置阈值水平线（对外方法）
    void setThreshold(double value);
    // 清空曲线
    void clear();

private:
    void setupPlot();          // 初始化 QCustomPlot
    void updateThresholdLine(); // 更新阈值线位置

    QCustomPlot *m_plot = nullptr;
    QCPItemStraightLine *m_thresholdLine = nullptr;

    QVector<double> m_values;   // 数据值（y 轴）
    QVector<double> m_time;     // 时间轴（x 轴，用序号表示秒）
    double m_threshold = 0.5;   // 默认阈值
    int m_count = 0;            // 已追加点数
    int m_maxPoints = 60;       // 最多显示 60 个点（约 60 秒）
};

#endif // TRENDCHART_H
