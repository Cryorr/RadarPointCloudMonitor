#ifndef POINTCLOUDVIEW_H
#define POINTCLOUDVIEW_H

#include <QColor>
#include <QVector>
#include <QWidget>

#include "protocol.h"

// 点云绘图控件（SPEC 6.3）
//  - 继承 QWidget，重写 paintEvent
//  - 保存最近一包点云，将 x/y 映射到控件坐标，intensity 映射颜色
//  - 正常点按强度着色（蓝→绿→黄），异常点（z 位移超阈值）画红色
class PointCloudView : public QWidget
{
    Q_OBJECT

public:
    explicit PointCloudView(QWidget *parent = nullptr);

    // 更新最近一包点云并触发重绘（对外方法）
    void updatePoints(const QVector<PointData> &points);
    // 清空点云
    void clear();
    // 设置位移阈值：|z - 参考z| 超过该值视为异常点，画红色
    void setThreshold(double threshold);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor intensityToColor(float intensity) const;   // 强度 → 颜色
    QPoint worldToScreen(float x, float y) const;     // 世界坐标 → 屏幕坐标
    void drawGrid(QPainter &painter);                 // 画网格与坐标轴

    QVector<PointData> m_points;   // 最近一包点云
    double m_threshold = 0.5;      // 位移阈值（默认 0.5）
    double m_referenceZ = 0.0;     // 参考平面 z（模拟器数据围绕 z≈0）
};

#endif // POINTCLOUDVIEW_H
