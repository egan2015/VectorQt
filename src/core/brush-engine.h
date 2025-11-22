#ifndef BRUSH_ENGINE_H
#define BRUSH_ENGINE_H

#include <QObject>
#include <QPointF>
#include <QVector>
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <QElapsedTimer>
#include <QTabletEvent>

/**
 * 画笔引擎 - 模拟真实画笔的物理特性
 * 参考Inkscape的画笔系统设计
 */

struct BrushPoint {
    QPointF position;      // 位置
    qreal pressure;        // 压力 (0.0 - 1.0)
    qreal tiltX;          // X轴倾斜角度
    qreal tiltY;          // Y轴倾斜角度
    qreal rotation;       // 旋转角度
    qreal velocity;       // 速度（像素/秒）
    qreal timestamp;      // 时间戳（毫秒）
};

struct BrushProfile {
    QString name;          // 配置名称
    QString description;   // 描述
    
    // 基础参数
    qreal baseWidth;       // 基础宽度
    qreal minWidth;        // 最小宽度
    qreal maxWidth;        // 最大宽度
    
    // 压力响应
    bool pressureEnabled;  // 启用压力感应
    qreal pressureCurve;   // 压力曲线 (0.1 - 5.0)
    qreal pressureSensitivity; // 压感灵敏度
    
    // 速度响应
    bool velocityEnabled;  // 启用速度感应
    qreal velocityCurve;   // 速度曲线
    qreal velocitySensitivity; // 速度灵敏度
    
    // 倾斜响应
    bool tiltEnabled;      // 启用倾斜感应
    qreal tiltCurve;       // 倾斜曲线
    qreal tiltSensitivity; // 倾斜灵敏度
    
    // 平滑和抖动
    qreal smoothing;       // 平滑度 (0.0 - 1.0)
    qreal jitter;          // 抖动幅度
    qreal randomization;   // 随机化程度
    
    // 纹理和效果
    bool textureEnabled;   // 启用纹理
    QString texturePath;   // 纹理路径
    qreal opacity;         // 不透明度
    qreal scattering;      // 散射程度
    
    // 颜色变化
    bool colorVariation;   // 启用颜色变化
    qreal hueVariation;    // 色调变化范围
    qreal saturationVariation; // 饱和度变化范围
    qreal brightnessVariation; // 亮度变化范围
};

class BrushEngine : public QObject
{
    Q_OBJECT

public:
    explicit BrushEngine(QObject *parent = nullptr);
    ~BrushEngine();

    // 笔刷配置管理
    void loadProfile(const BrushProfile& profile);
    BrushProfile currentProfile() const { return m_currentProfile; }
    
    // 预设配置
    static QVector<BrushProfile> getDefaultProfiles();
    void loadDefaultProfile(const QString& name);
    
    // 绘制控制
    void beginStroke(const QPointF& pos, qreal pressure = 1.0);
    void addPoint(const QPointF& pos, qreal pressure = 1.0, 
                  qreal tiltX = 0.0, qreal tiltY = 0.0, qreal rotation = 0.0);
    void endStroke();
    
    // 获取绘制结果
    QPainterPath getStrokePath() const;
    QVector<QPen> getStrokePens() const;
    
    // 实时预览
    void updatePreview(const QPointF& currentPos);
    QPainterPath getPreviewPath() const;
    
    // 状态查询
    bool isDrawing() const { return m_isDrawing; }
    qreal getCurrentWidth() const { return m_currentWidth; }
    QColor getCurrentColor() const { return m_currentColor; }

signals:
    void strokeStarted();
    void strokeUpdated();
    void strokeEnded();
    void previewUpdated();

private:
    // 核心计算方法
    qreal calculateWidth(const BrushPoint& point) const;
    QColor calculateColor(const BrushPoint& point) const;
    QPointF applySmoothing(const QPointF& pos) const;
    QPointF applyJitter(const QPointF& pos) const;
    
    // 物理模拟
    qreal calculateVelocity(const QPointF& pos, qreal timestamp) const;
    qreal calculateTiltEffect(qreal tiltX, qreal tiltY) const;
    qreal calculatePressureEffect(qreal pressure) const;
    
    // 路径生成
    void generateStrokePath();
    void generatePreviewPath();
    
    // 平滑算法
    QVector<QPointF> smoothPoints(const QVector<BrushPoint>& points) const;
    QPointF applyGaussianSmoothing(const QVector<BrushPoint>& points, int index) const;
    
    // 颜色变化
    QColor applyColorVariation(const QColor& baseColor, const BrushPoint& point) const;
    
    // 成员变量
    BrushProfile m_currentProfile;
    QVector<BrushPoint> m_points;
    QVector<QPen> m_strokePens;
    QPainterPath m_strokePath;
    QPainterPath m_previewPath;
    
    // 状态变量
    bool m_isDrawing;
    qreal m_currentWidth;
    QColor m_currentColor;
    QPointF m_lastPosition;
    qreal m_lastTimestamp;
    qreal m_lastPressure;
    QElapsedTimer m_timer;
    
    // 平滑缓冲
    QVector<QPointF> m_positionBuffer;
    QVector<qreal> m_pressureBuffer;
    static const int BUFFER_SIZE = 5;
};

#endif // BRUSH_ENGINE_H