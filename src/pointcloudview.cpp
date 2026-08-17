#include "pointcloudview.h"

#include <QPainter>
#include <QPaintEvent>

#include <cmath>

PointCloudView::PointCloudView(QWidget *parent)
    : QWidget(parent)
{
    // 设置最小尺寸，避免窗口过小时点云不可见
    setMinimumSize(400, 300);
}

void PointCloudView::updatePoints(const QVector<PointData> &points)
{
    m_points = points;
    update();   // 触发 paintEvent 重绘
}

void PointCloudView::clear()
{
    m_points.clear();
    update();
}

void PointCloudView::setThreshold(double threshold)
{
    m_threshold = threshold;
    update();
}

void PointCloudView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 深色背景，模拟雷达屏
    painter.fillRect(rect(), QColor(18, 18, 28));

    // 网格与中心十字，方便观察点的位置
    drawGrid(painter);

    // 绘制所有点
    for (const PointData &p : m_points) {
        const QPoint pt = worldToScreen(p.x, p.y);
        const double dz = std::fabs(p.z - m_referenceZ);
        const bool anomaly = (dz > m_threshold);

        // 异常点红色；正常点按强度着色
        const QColor color = anomaly ? QColor(255, 60, 60)
                                     : intensityToColor(p.intensity);
        painter.setPen(color);
        painter.setBrush(color);
        // 异常点画大一些，更醒目
        const int r = anomaly ? 4 : 2;
        painter.drawEllipse(pt, r, r);
    }
}

QColor PointCloudView::intensityToColor(float intensity) const
{
    // 强度 [0,1] 映射到色相：蓝(240°) → 绿(120°) → 黄(60°)
    const float t = std::clamp(intensity, 0.0f, 1.0f);
    const int hue = static_cast<int>(240.0 - 180.0 * t);
    return QColor::fromHsv(hue, 255, 255);
}

QPoint PointCloudView::worldToScreen(float x, float y) const
{
    // 世界坐标范围（模拟器 x/y ∈ [-5,5]），四周留边距
    const double worldMin = -5.5;
    const double worldMax = 5.5;
    const double worldSpan = worldMax - worldMin;
    const int margin = 20;

    const int w = width() - 2 * margin;
    const int h = height() - 2 * margin;

    // x 线性映射到 [margin, width-margin]
    int sx = margin + static_cast<int>((x - worldMin) / worldSpan * w);
    // y 需翻转（Qt 屏幕 y 轴向下，而世界坐标 y 向上）
    int sy = height() - margin - static_cast<int>((y - worldMin) / worldSpan * h);

    // 限制在控件范围内，避免越界
    sx = std::clamp(sx, 0, width() - 1);
    sy = std::clamp(sy, 0, height() - 1);
    return QPoint(sx, sy);
}

void PointCloudView::drawGrid(QPainter &painter)
{
    // 浅色网格
    painter.setPen(QColor(40, 40, 55));
    const int step = 40;
    for (int x = 0; x <= width(); x += step) {
        painter.drawLine(x, 0, x, height());
    }
    for (int y = 0; y <= height(); y += step) {
        painter.drawLine(0, y, width(), y);
    }

    // 中心十字（略亮）
    painter.setPen(QColor(70, 70, 90));
    painter.drawLine(width() / 2, 0, width() / 2, height());
    painter.drawLine(0, height() / 2, width(), height() / 2);
}
