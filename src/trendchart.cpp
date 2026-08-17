#include "trendchart.h"

#include <QPen>
#include <QVBoxLayout>

TrendChart::TrendChart(QWidget *parent)
    : QWidget(parent)
{
    // 让 QCustomPlot 占满整个控件
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_plot = new QCustomPlot(this);
    layout->addWidget(m_plot);

    setupPlot();
}

TrendChart::~TrendChart() = default;

void TrendChart::setupPlot()
{
    // 数据曲线（最大位移）
    m_plot->addGraph();
    m_plot->graph(0)->setPen(QPen(QColor(0, 150, 255), 2));
    m_plot->graph(0)->setName(QStringLiteral("最大位移"));

    // 阈值水平线：用一条无限延伸的红色虚线
    m_thresholdLine = new QCPItemStraightLine(m_plot);
    m_thresholdLine->setPen(QPen(QColor(255, 60, 60), 1, Qt::DashLine));
    updateThresholdLine();

    // 坐标轴
    m_plot->xAxis->setLabel(QStringLiteral("时间 (秒)"));
    m_plot->yAxis->setLabel(QStringLiteral("位移"));
    m_plot->xAxis->setRange(0, m_maxPoints);
    m_plot->yAxis->setRange(-0.2, 1.5);

    // 图例（右上角）
    m_plot->legend->setVisible(true);
    m_plot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignRight);

    m_plot->replot();
}

void TrendChart::appendPoint(double value)
{
    m_values.append(value);
    m_time.append(m_count);
    ++m_count;

    // 更新曲线数据
    m_plot->graph(0)->setData(m_time, m_values);

    // 滚动窗口：x 轴跟随最新点，保留最近 60 秒
    if (m_count > m_maxPoints) {
        m_plot->xAxis->setRange(m_count - m_maxPoints, m_count);
    }
    m_plot->replot();
}

void TrendChart::setThreshold(double value)
{
    m_threshold = value;
    updateThresholdLine();
    m_plot->replot();
}

void TrendChart::clear()
{
    m_values.clear();
    m_time.clear();
    m_count = 0;
    m_plot->graph(0)->data()->clear();
    m_plot->xAxis->setRange(0, m_maxPoints);
    m_plot->replot();
}

void TrendChart::updateThresholdLine()
{
    // 两个 y 坐标相同的点定义一条水平直线（无限延伸）
    m_thresholdLine->point1->setCoords(0, m_threshold);
    m_thresholdLine->point2->setCoords(1, m_threshold);
}
