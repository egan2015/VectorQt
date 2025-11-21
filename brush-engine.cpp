#include "brush-engine.h"
#include <QDebug>
#include <QtMath>
#include <QRandomGenerator>
#include <QColor>
#include <QPen>

BrushEngine::BrushEngine(QObject *parent)
    : QObject(parent)
    , m_isDrawing(false)
    , m_currentWidth(2.0)
    , m_currentColor(Qt::black)
    , m_lastTimestamp(0)
    , m_lastPressure(1.0)
{
    // 加载默认配置
    loadDefaultProfile("Basic Pen");
    
    // 初始化缓冲区
    m_positionBuffer.reserve(BUFFER_SIZE);
    m_pressureBuffer.reserve(BUFFER_SIZE);
    
    m_timer.start();
}

BrushEngine::~BrushEngine()
{
}

void BrushEngine::loadProfile(const BrushProfile& profile)
{
    m_currentProfile = profile;
    m_currentWidth = profile.baseWidth;
}

QVector<BrushProfile> BrushEngine::getDefaultProfiles()
{
    QVector<BrushProfile> profiles;
    
    // 基础钢笔
    BrushProfile basicPen;
    basicPen.name = "Basic Pen";
    basicPen.description = "标准钢笔，支持压感";
    basicPen.baseWidth = 2.0;
    basicPen.minWidth = 0.5;
    basicPen.maxWidth = 8.0;
    basicPen.pressureEnabled = true;
    basicPen.pressureCurve = 1.5;
    basicPen.pressureSensitivity = 0.8;
    basicPen.velocityEnabled = true;
    basicPen.velocityCurve = 0.3;
    basicPen.velocitySensitivity = 0.2;
    basicPen.tiltEnabled = false;
    basicPen.smoothing = 0.3;
    basicPen.jitter = 0.1;
    basicPen.randomization = 0.05;
    basicPen.opacity = 1.0;
    basicPen.scattering = 0.0;
    basicPen.colorVariation = false;
    profiles.append(basicPen);
    
    // 钢笔
    BrushProfile fountainPen;
    fountainPen.name = "Fountain Pen";
    fountainPen.description = "钢笔效果，带有笔锋";
    fountainPen.baseWidth = 3.0;  // 增加基础线宽
    fountainPen.minWidth = 0.5;
    fountainPen.maxWidth = 8.0;  // 增加最大线宽
    fountainPen.pressureEnabled = true;
    fountainPen.pressureCurve = 2.2;
    fountainPen.pressureSensitivity = 0.9;
    fountainPen.velocityEnabled = true;
    fountainPen.velocityCurve = 0.4;
    fountainPen.velocitySensitivity = 0.3;
    fountainPen.tiltEnabled = false;
    fountainPen.smoothing = 0.4;
    fountainPen.jitter = 0.05;
    fountainPen.randomization = 0.02;
    fountainPen.opacity = 1.0;
    fountainPen.scattering = 0.0;
    fountainPen.colorVariation = false;
    profiles.append(fountainPen);
    
    // 圆珠笔
    BrushProfile ballpointPen;
    ballpointPen.name = "Ballpoint Pen";
    ballpointPen.description = "圆珠笔效果，线条均匀";
    ballpointPen.baseWidth = 1.0;
    ballpointPen.minWidth = 0.8;
    ballpointPen.maxWidth = 1.5;
    ballpointPen.pressureEnabled = true;
    ballpointPen.pressureCurve = 0.8;
    ballpointPen.pressureSensitivity = 0.3;
    ballpointPen.velocityEnabled = false;
    ballpointPen.velocityCurve = 0.0;
    ballpointPen.velocitySensitivity = 0.0;
    ballpointPen.tiltEnabled = false;
    ballpointPen.smoothing = 0.2;
    ballpointPen.jitter = 0.02;
    ballpointPen.randomization = 0.01;
    ballpointPen.opacity = 1.0;
    ballpointPen.scattering = 0.0;
    ballpointPen.colorVariation = false;
    profiles.append(ballpointPen);
    
    // 签字笔
    BrushProfile markerPen;
    markerPen.name = "Marker Pen";
    markerPen.description = "签字笔效果，线条较粗";
    markerPen.baseWidth = 3.0;
    markerPen.minWidth = 2.0;
    markerPen.maxWidth = 5.0;
    markerPen.pressureEnabled = true;
    markerPen.pressureCurve = 1.0;
    markerPen.pressureSensitivity = 0.5;
    markerPen.velocityEnabled = true;
    markerPen.velocityCurve = 0.2;
    markerPen.velocitySensitivity = 0.1;
    markerPen.tiltEnabled = false;
    markerPen.smoothing = 0.3;
    markerPen.jitter = 0.1;
    markerPen.randomization = 0.05;
    markerPen.opacity = 0.95;
    markerPen.scattering = 0.0;
    markerPen.colorVariation = false;
    profiles.append(markerPen);
    
    // 铅笔
    BrushProfile pencil;
    pencil.name = "Pencil";
    pencil.description = "铅笔效果，带有纹理";
    pencil.baseWidth = 1.5;
    pencil.minWidth = 0.3;
    pencil.maxWidth = 6.0;
    pencil.pressureEnabled = true;
    pencil.pressureCurve = 1.2;
    pencil.pressureSensitivity = 0.6;
    pencil.velocityEnabled = true;
    pencil.velocityCurve = 0.5;
    pencil.velocitySensitivity = 0.3;
    pencil.tiltEnabled = true;
    pencil.tiltCurve = 0.8;
    pencil.tiltSensitivity = 0.4;
    pencil.smoothing = 0.2;
    pencil.jitter = 0.8;
    pencil.randomization = 0.3;
    pencil.opacity = 0.9;
    pencil.scattering = 0.1;
    pencil.colorVariation = true;
    pencil.hueVariation = 0.05;
    pencil.saturationVariation = 0.1;
    pencil.brightnessVariation = 0.15;
    profiles.append(pencil);
    
    // 马克笔
    BrushProfile marker;
    marker.name = "Marker";
    marker.description = "马克笔效果，边缘柔和";
    marker.baseWidth = 8.0;
    marker.minWidth = 4.0;
    marker.maxWidth = 20.0;
    marker.pressureEnabled = true;
    marker.pressureCurve = 0.8;
    marker.pressureSensitivity = 0.4;
    marker.velocityEnabled = false;
    marker.tiltEnabled = false;
    marker.smoothing = 0.6;
    marker.jitter = 0.2;
    marker.randomization = 0.1;
    marker.opacity = 0.8;
    marker.scattering = 0.0;
    marker.colorVariation = false;
    profiles.append(marker);
    
    // 毛笔
    BrushProfile calligraphy;
    calligraphy.name = "Calligraphy";
    calligraphy.description = "毛笔效果，支持倾斜";
    calligraphy.baseWidth = 3.0;
    calligraphy.minWidth = 0.5;
    calligraphy.maxWidth = 15.0;
    calligraphy.pressureEnabled = true;
    calligraphy.pressureCurve = 2.0;
    calligraphy.pressureSensitivity = 1.0;
    calligraphy.velocityEnabled = true;
    calligraphy.velocityCurve = 0.2;
    calligraphy.velocitySensitivity = 0.15;
    calligraphy.tiltEnabled = true;
    calligraphy.tiltCurve = 1.5;
    calligraphy.tiltSensitivity = 0.8;
    calligraphy.smoothing = 0.4;
    calligraphy.jitter = 0.3;
    calligraphy.randomization = 0.2;
    calligraphy.opacity = 1.0;
    calligraphy.scattering = 0.05;
    calligraphy.colorVariation = true;
    calligraphy.hueVariation = 0.03;
    calligraphy.saturationVariation = 0.05;
    calligraphy.brightnessVariation = 0.08;
    profiles.append(calligraphy);
    
    // 喷枪
    BrushProfile airbrush;
    airbrush.name = "Airbrush";
    airbrush.description = "喷枪效果，带有散射";
    airbrush.baseWidth = 10.0;
    airbrush.minWidth = 2.0;
    airbrush.maxWidth = 50.0;
    airbrush.pressureEnabled = true;
    airbrush.pressureCurve = 1.8;
    airbrush.pressureSensitivity = 0.7;
    airbrush.velocityEnabled = true;
    airbrush.velocityCurve = 0.6;
    airbrush.velocitySensitivity = 0.4;
    airbrush.tiltEnabled = false;
    airbrush.smoothing = 0.8;
    airbrush.jitter = 1.5;
    airbrush.randomization = 0.4;
    airbrush.opacity = 0.6;
    airbrush.scattering = 0.8;
    airbrush.colorVariation = false;
    profiles.append(airbrush);
    
    return profiles;
}

void BrushEngine::loadDefaultProfile(const QString& name)
{
    QVector<BrushProfile> profiles = getDefaultProfiles();
    for (const BrushProfile& profile : profiles) {
        if (profile.name == name) {
            loadProfile(profile);
            return;
        }
    }
    
    // 如果没找到，使用第一个
    if (!profiles.isEmpty()) {
        loadProfile(profiles.first());
    }
}

void BrushEngine::beginStroke(const QPointF& pos, qreal pressure)
{
    m_isDrawing = true;
    m_points.clear();
    m_strokePens.clear();
    m_strokePath = QPainterPath();
    m_positionBuffer.clear();
    m_pressureBuffer.clear();
    
    // 添加起始点
    BrushPoint point;
    point.position = pos;
    point.pressure = pressure;
    point.tiltX = 0.0;
    point.tiltY = 0.0;
    point.rotation = 0.0;
    point.velocity = 0.0;
    point.timestamp = m_timer.elapsed();
    
    m_points.append(point);
    m_lastPosition = pos;
    m_lastTimestamp = point.timestamp;
    m_lastPressure = pressure;
    
    // 更新缓冲区
    m_positionBuffer.append(pos);
    m_pressureBuffer.append(pressure);
    
    // 计算初始宽度和颜色
    m_currentWidth = calculateWidth(point);
    m_currentColor = calculateColor(point);
    
    emit strokeStarted();
}

void BrushEngine::addPoint(const QPointF& pos, qreal pressure, 
                          qreal tiltX, qreal tiltY, qreal rotation)
{
    if (!m_isDrawing) {
        return;
    }
    
    qreal currentTime = m_timer.elapsed();
    
    // 创建新点
    BrushPoint point;
    point.position = pos;
    point.pressure = pressure;
    point.tiltX = tiltX;
    point.tiltY = tiltY;
    point.rotation = rotation;
    point.velocity = calculateVelocity(pos, currentTime);
    point.timestamp = currentTime;
    
    m_points.append(point);
    
    // 更新缓冲区
    m_positionBuffer.append(pos);
    m_pressureBuffer.append(pressure);
    
    if (m_positionBuffer.size() > BUFFER_SIZE) {
        m_positionBuffer.removeFirst();
        m_pressureBuffer.removeFirst();
    }
    
    // 应用平滑
    QPointF smoothedPos = applySmoothing(pos);
    point.position = smoothedPos;
    
    // 应用抖动
    if (m_currentProfile.jitter > 0) {
        point.position = applyJitter(point.position);
    }
    
    // 更新当前状态
    m_currentWidth = calculateWidth(point);
    m_currentColor = calculateColor(point);
    
    m_lastPosition = pos;
    m_lastTimestamp = currentTime;
    m_lastPressure = pressure;
    
    // 实时生成路径以支持预览
    generateStrokePath();
    
    emit strokeUpdated();
}

void BrushEngine::endStroke()
{
    if (!m_isDrawing) {
        return;
    }
    
    m_isDrawing = false;
    generateStrokePath();
    
    emit strokeEnded();
}

qreal BrushEngine::calculateWidth(const BrushPoint& point) const
{
    qreal width = m_currentProfile.baseWidth;
    
    // 压力影响
    if (m_currentProfile.pressureEnabled) {
        qreal pressureEffect = calculatePressureEffect(point.pressure);
        width *= pressureEffect;
    }
    
    // 速度影响
    if (m_currentProfile.velocityEnabled) {
        qreal velocityEffect = 1.0 - (point.velocity * m_currentProfile.velocitySensitivity * 0.01);
        velocityEffect = qPow(qMax(0.1, velocityEffect), m_currentProfile.velocityCurve);
        width *= velocityEffect;
    }
    
    // 倾斜影响
    if (m_currentProfile.tiltEnabled) {
        qreal tiltEffect = calculateTiltEffect(point.tiltX, point.tiltY);
        width *= tiltEffect;
    }
    
    // 应用随机化
    if (m_currentProfile.randomization > 0) {
        qreal randomFactor = 1.0 + (QRandomGenerator::global()->bounded(2000) - 1000) * 0.001 * m_currentProfile.randomization;
        width *= randomFactor;
    }
    
    // 限制在最小最大宽度之间
    width = qBound(m_currentProfile.minWidth, width, m_currentProfile.maxWidth);
    
    return width;
}

QColor BrushEngine::calculateColor(const BrushPoint& point) const
{
    QColor color = Qt::black; // 这里应该从全局设置获取基础颜色
    
    if (m_currentProfile.colorVariation) {
        color = applyColorVariation(color, point);
    }
    
    return color;
}

QPointF BrushEngine::applySmoothing(const QPointF& pos) const
{
    if (m_currentProfile.smoothing <= 0 || m_positionBuffer.size() < 2) {
        return pos;
    }
    
    // 使用高斯平滑
    qreal totalWeight = 0.0;
    QPointF smoothedPos(0, 0);
    
    for (int i = 0; i < m_positionBuffer.size(); ++i) {
        qreal weight = qExp(-0.5 * qPow((m_positionBuffer.size() - 1 - i) / (m_currentProfile.smoothing * 3.0), 2));
        smoothedPos += m_positionBuffer[i] * weight;
        totalWeight += weight;
    }
    
    if (totalWeight > 0) {
        smoothedPos /= totalWeight;
    }
    
    return smoothedPos;
}

QPointF BrushEngine::applyJitter(const QPointF& pos) const
{
    if (m_currentProfile.jitter <= 0) {
        return pos;
    }
    
    qreal jitterX = (QRandomGenerator::global()->bounded(2000) - 1000) * 0.001 * m_currentProfile.jitter;
    qreal jitterY = (QRandomGenerator::global()->bounded(2000) - 1000) * 0.001 * m_currentProfile.jitter;
    
    return pos + QPointF(jitterX, jitterY);
}

qreal BrushEngine::calculateVelocity(const QPointF& pos, qreal timestamp) const
{
    if (m_lastTimestamp <= 0) {
        return 0.0;
    }
    
    qreal deltaTime = (timestamp - m_lastTimestamp) / 1000.0; // 转换为秒
    if (deltaTime <= 0) {
        return 0.0;
    }
    
    qreal distance = QLineF(m_lastPosition, pos).length();
    return distance / deltaTime; // 像素/秒
}

qreal BrushEngine::calculateTiltEffect(qreal tiltX, qreal tiltY) const
{
    // 计算倾斜角度的复合影响
    qreal tiltMagnitude = qSqrt(tiltX * tiltX + tiltY * tiltY);
    qreal tiltEffect = 1.0 + tiltMagnitude * m_currentProfile.tiltSensitivity * 0.01;
    
    // 应用倾斜曲线
    tiltEffect = qPow(tiltEffect, m_currentProfile.tiltCurve);
    
    return qMax(0.1, tiltEffect);
}

qreal BrushEngine::calculatePressureEffect(qreal pressure) const
{
    // 应用压力曲线
    qreal effect = qPow(pressure, m_currentProfile.pressureCurve);
    
    // 应用压力灵敏度
    effect = 1.0 - (1.0 - effect) * m_currentProfile.pressureSensitivity;
    
    return qMax(0.1, effect);
}

void BrushEngine::generateStrokePath()
{
    if (m_points.size() < 2) {
        return;
    }
    
    m_strokePath = QPainterPath();
    m_strokePens.clear();
    
    // 使用平滑后的点生成路径
    QVector<BrushPoint> smoothedPoints;
    for (const BrushPoint& point : m_points) {
        smoothedPoints.append(point);
    }
    
    if (smoothedPoints.isEmpty()) {
        return;
    }
    
    // 生成路径和笔刷
    m_strokePath.moveTo(smoothedPoints[0].position);
    
    for (int i = 1; i < smoothedPoints.size(); ++i) {
        const BrushPoint& prevPoint = smoothedPoints[i-1];
        const BrushPoint& currPoint = smoothedPoints[i];
        
        // 计算控制点（贝塞尔曲线）
        QPointF controlPoint1 = prevPoint.position + (currPoint.position - prevPoint.position) * 0.3;
        QPointF controlPoint2 = currPoint.position - (currPoint.position - prevPoint.position) * 0.3;
        
        m_strokePath.cubicTo(controlPoint1, controlPoint2, currPoint.position);
        
        // 为每个段创建笔刷
        QPen pen = QPen(calculateColor(currPoint), calculateWidth(currPoint), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        // QPen没有setOpacity方法，透明度在QColor中设置
        QColor penColor = calculateColor(currPoint);
        penColor.setAlphaF(m_currentProfile.opacity);
        pen.setColor(penColor);
        m_strokePens.append(pen);
    }
}

QPainterPath BrushEngine::getStrokePath() const
{
    return m_strokePath;
}

QVector<QPen> BrushEngine::getStrokePens() const
{
    return m_strokePens;
}

void BrushEngine::updatePreview(const QPointF& currentPos)
{
    if (!m_isDrawing || m_points.isEmpty()) {
        return;
    }
    
    // 创建预览路径
    m_previewPath = m_strokePath;
    
    // 添加到当前位置的预览线段
    if (!m_points.isEmpty()) {
        const BrushPoint& lastPoint = m_points.last();
        m_previewPath.lineTo(currentPos);
    }
    
    emit previewUpdated();
}

QPainterPath BrushEngine::getPreviewPath() const
{
    return m_previewPath;
}

QVector<QPointF> BrushEngine::smoothPoints(const QVector<BrushPoint>& points) const
{
    if (points.size() < 3 || m_currentProfile.smoothing <= 0) {
        QVector<QPointF> result;
        for (const BrushPoint& point : points) {
            result.append(point.position);
        }
        return result;
    }
    
    QVector<QPointF> smoothedPoints;
    
    for (int i = 0; i < points.size(); ++i) {
        smoothedPoints.append(applyGaussianSmoothing(points, i));
    }
    
    return smoothedPoints;
}

QPointF BrushEngine::applyGaussianSmoothing(const QVector<BrushPoint>& points, int index) const
{
    QPointF smoothed = points[index].position;
    qreal totalWeight = 1.0;
    
    int radius = qCeil(m_currentProfile.smoothing * 2.0);
    
    for (int i = qMax(0, index - radius); i <= qMin(points.size() - 1, index + radius); ++i) {
        if (i == index) continue;
        
        qreal distance = qAbs(i - index);
        qreal weight = qExp(-0.5 * qPow(distance / (m_currentProfile.smoothing + 0.1), 2));
        
        smoothed += points[i].position * weight;
        totalWeight += weight;
    }
    
    return smoothed / totalWeight;
}

QColor BrushEngine::applyColorVariation(const QColor& baseColor, const BrushPoint& point) const
{
    float h, s, v, a;
    baseColor.getHsvF(&h, &s, &v, &a);
    
    // 色调变化
    if (m_currentProfile.hueVariation > 0) {
        qreal hueVariation = (QRandomGenerator::global()->bounded(2000) - 1000) * 0.001 * m_currentProfile.hueVariation;
        h += hueVariation;
        if (h < 0) h += 1.0;
        if (h > 1) h -= 1.0;
    }
    
    // 饱和度变化
    if (m_currentProfile.saturationVariation > 0) {
        qreal satVariation = (QRandomGenerator::global()->bounded(2000) - 1000) * 0.001 * m_currentProfile.saturationVariation;
        s = qBound(0.0, s + satVariation, 1.0);
    }
    
    // 亮度变化
    if (m_currentProfile.brightnessVariation > 0) {
        qreal valVariation = (QRandomGenerator::global()->bounded(2000) - 1000) * 0.001 * m_currentProfile.brightnessVariation;
        v = qBound(0.0, v + valVariation, 1.0);
    }
    
    QColor variedColor = QColor::fromHsvF(h, s, v, a);
    return variedColor;
}