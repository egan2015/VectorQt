#include "../ui/ruler.h"
#include <QPainter>
#include <QFontMetrics>
#include <QDebug>
#include <QtMath>
#include <QMenu>
#include <QContextMenuEvent>
#include <QEnterEvent>

Ruler::Ruler(Orientation orientation, QWidget *parent)
    : QWidget(parent)
    , m_orientation(orientation)
    , m_unit(Pixels)
    , m_origin(0.0)
    , m_scale(1.0)
    , m_mouseTracking(false)
    , m_hovered(false)
, m_contextMenu(nullptr)
, m_view(nullptr)
, m_hasSelection(false)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_NoMousePropagation);
    setAttribute(Qt::WA_Hover, true);
    
    if (m_orientation == Horizontal) {
        setFixedHeight(RULER_SIZE);
    } else {
        setFixedWidth(RULER_SIZE);
    }
    
    // Create context menu
    createContextMenu();
    
    // 使用系统主题颜色，不设置硬编码颜色
}

void Ruler::setOrigin(qreal origin)
{
    if (m_origin != origin) {
        m_origin = origin;
        update();
    }
}

void Ruler::setScale(qreal scale)
{
    if (m_scale != scale) {
        m_scale = scale;
        update();
    }
}

void Ruler::setMousePos(const QPointF &pos)
{
    QPointF adjustedPos = pos;
    // 如果有视图，需要调整坐标以考虑视图和标尺的相对位置
    if (m_view && parentWidget()) {
        // 获取视图在父窗口中的位置
        QPoint viewPos = m_view->mapToParent(QPoint(0, 0));
        // 获取标尺在父窗口中的位置
        QPoint rulerPos = mapToParent(QPoint(0, 0));
        
        if (m_orientation == Horizontal) {
            // 水平标尺：调整X坐标
            adjustedPos.setX(pos.x() + viewPos.x());
        } else {
            // 垂直标尺：调整Y坐标
            adjustedPos.setY(pos.y() + viewPos.y());
        }
    }
    
    if (m_mousePos != adjustedPos) {
        m_mousePos = adjustedPos;
        m_mouseTracking = true;
        update();
    }
}

void Ruler::setView(QGraphicsView *view)
{
    m_view = view;
}

void Ruler::setUnit(Unit unit)
{
    if (m_unit != unit) {
        m_unit = unit;
        update();
        updateMenuCheckState(); // 更新菜单选中状态
        emit unitChanged(unit);
        emit unitChangedForAll(unit); // 发出信号，用于同步所有标尺的单位
    }
}

QSize Ruler::sizeHint() const
{
    if (m_orientation == Horizontal) {
        return QSize(width(), RULER_SIZE);
    } else {
        return QSize(RULER_SIZE, height());
    }
}

void Ruler::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    
    // Draw enhanced background with gradient
    drawBackground(&painter);
    
    // Draw border with system theme colors
    QColor borderColor = palette().color(QPalette::WindowText);
    if (m_hovered) {
        borderColor = palette().color(QPalette::Highlight);
    }
    painter.setPen(QPen(borderColor, 1));
    if (m_orientation == Horizontal) {
        painter.drawLine(0, RULER_SIZE - 1, width(), RULER_SIZE - 1);
    } else {
        painter.drawLine(RULER_SIZE - 1, 0, RULER_SIZE - 1, height());
    }
    
    // Draw ticks with unit conversion
    QRectF rulerRect = rect();
    if (m_orientation == Horizontal) {
        rulerRect.adjust(0, 0, 0, -1);
    } else {
        rulerRect.adjust(0, 0, -1, 0);
    }
    drawTicks(&painter, rulerRect);
    
    // Draw mouse indicator
    drawMouseIndicator(&painter);
    
    // 🌟 绘制选中对象边界
    if (m_hasSelection && !m_selectedBounds.isEmpty()) {
        drawSelectionBounds(&painter);
    }
}

void Ruler::drawHorizontalRuler(QPainter *painter)
{
    Q_UNUSED(painter)
    // 水平标尺的特定绘制逻辑
}

void Ruler::drawVerticalRuler(QPainter *painter)
{
    Q_UNUSED(painter)
    // 垂直标尺的特定绘制逻辑
}

void Ruler::drawBackground(QPainter *painter)
{
    // 使用系统主题颜色
    QColor baseColor = palette().color(QPalette::Window);
    QColor hoverColor = palette().color(QPalette::Highlight);
    
    // Inkscape-inspired gradient background
    QLinearGradient gradient;
    if (m_orientation == Horizontal) {
        gradient = QLinearGradient(0, 0, 0, RULER_SIZE);
    } else {
        gradient = QLinearGradient(0, 0, RULER_SIZE, 0);
    }
    
    QColor actualColor = m_hovered ? hoverColor.lighter(150) : baseColor;
    gradient.setColorAt(0, actualColor.lighter(110));
    gradient.setColorAt(0.7, actualColor);
    gradient.setColorAt(1, actualColor.darker(110));
    
    painter->fillRect(rect(), gradient);
    
    // 使用系统主题颜色绘制阴影效果
    QColor shadowColor = palette().color(QPalette::Shadow);
    shadowColor.setAlpha(50);
    painter->setPen(QPen(shadowColor, 1));
    if (m_orientation == Horizontal) {
        painter->drawLine(0, RULER_SIZE - 2, width(), RULER_SIZE - 2);
    } else {
        painter->drawLine(RULER_SIZE - 2, 0, RULER_SIZE - 2, height());
    }
}

void Ruler::drawMouseIndicator(QPainter *painter)
{
    if (!m_mouseTracking) return;
    
    // Enhanced mouse indicator with gradient - 增强黑夜模式下的可见性
    QLinearGradient indicatorGradient;
    if (m_orientation == Horizontal) {
        indicatorGradient = QLinearGradient(m_mousePos.x() - 3, 0, m_mousePos.x() + 3, RULER_SIZE);
    } else {
        indicatorGradient = QLinearGradient(0, m_mousePos.y() - 3, RULER_SIZE, m_mousePos.y() + 3);
    }
    
    QColor indicatorColor = palette().color(QPalette::Highlight);
    // 在黑夜模式下使用更亮的颜色
    if (palette().color(QPalette::Window).lightness() < 128) {
        // 黑夜模式：使用更亮的颜色
        indicatorColor = QColor(255, 100, 100); // 鲜红色
    }
    indicatorGradient.setColorAt(0, QColor(indicatorColor.red(), indicatorColor.green(), indicatorColor.blue(), 180));
    indicatorGradient.setColorAt(0.5, QColor(indicatorColor.red(), indicatorColor.green(), indicatorColor.blue(), 255));
    indicatorGradient.setColorAt(1, QColor(indicatorColor.red(), indicatorColor.green(), indicatorColor.blue(), 180));
    
    painter->setPen(QPen(QColor(indicatorColor.red(), indicatorColor.green(), indicatorColor.blue(), 255), 2, Qt::DashLine));
    if (m_orientation == Horizontal) {
        qreal x = m_mousePos.x();
        // 放宽显示条件，允许在边界外一定范围内显示
        if (x >= -10 && x <= width() + 10) {
            // 限制绘制范围在标尺内
            qreal drawX = qBound(0.0, x, qreal(width()));
            painter->drawLine(drawX, 0, drawX, RULER_SIZE - 1);
            // Add small triangle at top
            QPolygonF triangle;
            triangle << QPointF(drawX - 3, 0) << QPointF(drawX + 3, 0) << QPointF(drawX, 4);
            painter->setBrush(QBrush(indicatorGradient));
            painter->drawPolygon(triangle);
        }
    } else {
        qreal y = m_mousePos.y();
        // 放宽显示条件，允许在边界外一定范围内显示
        if (y >= -10 && y <= height() + 10) {
            // 限制绘制范围在标尺内
            qreal drawY = qBound(0.0, y, qreal(height()));
            painter->drawLine(0, drawY, RULER_SIZE - 1, drawY);
            // Add small triangle at left
            QPolygonF triangle;
            triangle << QPointF(0, drawY - 3) << QPointF(0, drawY + 3) << QPointF(4, drawY);
            painter->setBrush(QBrush(indicatorGradient));
            painter->drawPolygon(triangle);
        }
    }
}

void Ruler::drawTicks(QPainter *painter, const QRectF &rect)
{
    // Set up modern font and colors - 增大字体
    QFont font = painter->font();
    font.setPointSize(9);  // 从8增加到9
    font.setFamily("Arial");
    painter->setFont(font);
    QFontMetrics fm(font);
    
    // Calculate display range in scene coordinates (参考Inkscape实现)
    qreal start, end;
    if (m_orientation == Horizontal) {
        start = (rect.left() - m_origin) / m_scale;
        end = (rect.right() - m_origin) / m_scale;
    } else {
        start = (rect.top() - m_origin) / m_scale;
        end = (rect.bottom() - m_origin) / m_scale;
    }
    
    // Get unit-specific spacing
    qreal unitScale = getUnitScale();
    
    // Dynamic tick spacing based on zoom level and unit
    qreal majorTickSpacing, minorTickSpacing;
    
    if (m_unit == Pixels) {
        if (m_scale < 0.01) {
            majorTickSpacing = 10000.0; minorTickSpacing = 1000.0;
        } else if (m_scale < 0.05) {
            majorTickSpacing = 5000.0; minorTickSpacing = 500.0;
        } else if (m_scale < 0.1) {
            majorTickSpacing = 2000.0; minorTickSpacing = 200.0;
        } else if (m_scale < 0.25) {
            majorTickSpacing = 1000.0; minorTickSpacing = 100.0;
        } else if (m_scale < 0.5) {
            majorTickSpacing = 500.0; minorTickSpacing = 50.0;
        } else if (m_scale < 1.0) {
            majorTickSpacing = 100.0; minorTickSpacing = 10.0;
        } else if (m_scale < 2.0) {
            majorTickSpacing = 50.0; minorTickSpacing = 5.0;
        } else if (m_scale < 5.0) {
            majorTickSpacing = 20.0; minorTickSpacing = 2.0;
        } else if (m_scale < 10.0) {
            majorTickSpacing = 10.0; minorTickSpacing = 1.0;
        } else if (m_scale < 20.0) {
            majorTickSpacing = 5.0; minorTickSpacing = 0.5;
        } else {
            majorTickSpacing = 1.0; minorTickSpacing = 0.1;
        }
    } else {
        // For physical units (mm, cm, inches, points)
        if (m_scale < 0.1) {
            majorTickSpacing = 500.0; minorTickSpacing = 100.0;
        } else if (m_scale < 0.25) {
            majorTickSpacing = 100.0; minorTickSpacing = 20.0;
        } else if (m_scale < 0.5) {
            majorTickSpacing = 50.0; minorTickSpacing = 10.0;
        } else if (m_scale < 1.0) {
            majorTickSpacing = 20.0; minorTickSpacing = 5.0;
        } else if (m_scale < 2.0) {
            majorTickSpacing = 10.0; minorTickSpacing = 2.0;
        } else if (m_scale < 5.0) {
            majorTickSpacing = 5.0; minorTickSpacing = 1.0;
        } else {
            majorTickSpacing = 1.0; minorTickSpacing = 0.2;
        }
        
        // Convert to physical units
        majorTickSpacing *= unitScale;
        minorTickSpacing *= unitScale;
    }
    
    // Draw ticks with enhanced styling
    for (qreal pos = qFloor(start / minorTickSpacing) * minorTickSpacing; pos <= end; pos += minorTickSpacing) {
        int pixelPos = qRound(pos * m_scale + m_origin);
        
        if (pixelPos < -50 || pixelPos > (m_orientation == Horizontal ? width() : height()) + 50) {
            continue;
        }
        
        bool isMajor = (qAbs(fmod(pos, majorTickSpacing)) < 0.0001 || qAbs(fmod(pos, majorTickSpacing) - majorTickSpacing) < 0.0001);
        
        if (qAbs(pos) < 0.0001) {
            // Origin line - 使用系统主题颜色
            painter->setPen(QPen(palette().color(QPalette::WindowText), 2.0));
            if (m_orientation == Horizontal) {
                painter->drawLine(pixelPos, 0, pixelPos, RULER_SIZE - 1);
            } else {
                painter->drawLine(0, pixelPos, RULER_SIZE - 1, pixelPos);
            }
        } else if (isMajor) {
            // Major tick - 使用系统主题颜色
            painter->setPen(QPen(palette().color(QPalette::WindowText).darker(120), 1.5));
            
            // pos已经是场景坐标，直接显示
            QString text = formatNumber(pos);
            
            if (m_orientation == Horizontal) {
                painter->drawLine(pixelPos, RULER_SIZE - MAJOR_TICK_LENGTH, pixelPos, RULER_SIZE - 1);
                
                int textWidth = fm.horizontalAdvance(text);
                if (pixelPos - textWidth/2 >= 0 && pixelPos + textWidth/2 <= width()) {
                    painter->setPen(QPen(palette().color(QPalette::WindowText), 1));
                    // 调整水平标尺文本位置，使其更靠近刻度
                    int textY = RULER_SIZE - MAJOR_TICK_LENGTH - 2;
                    painter->drawText(pixelPos - textWidth / 2, textY, text);
                    painter->setPen(QPen(palette().color(QPalette::WindowText).darker(120), 1.5));
                }
            } else {
                painter->drawLine(RULER_SIZE - MAJOR_TICK_LENGTH, pixelPos, RULER_SIZE - 1, pixelPos);
                
                int textWidth = fm.horizontalAdvance(text);
                int textHeight = fm.height();
                if (pixelPos - textWidth/2 >= 0 && pixelPos + textWidth/2 <= height()) {
                    painter->setPen(QPen(palette().color(QPalette::WindowText), 1));
                    painter->save();
                    // 调整垂直标尺文本位置，使其更靠近刻度
                    int textX = RULER_SIZE - MAJOR_TICK_LENGTH - 2;
                    int textY = pixelPos + textWidth/2;
                    painter->translate(textX, textY);
                    painter->rotate(-90);
                    painter->drawText(0, 0, text);
                    painter->restore();
                    painter->setPen(QPen(palette().color(QPalette::WindowText).darker(120), 1.5));
                }
            }
        } else {
            // Minor tick - 使用系统主题颜色
            painter->setPen(QPen(palette().color(QPalette::WindowText).darker(150), 1.2));
            if (m_orientation == Horizontal) {
                painter->drawLine(pixelPos, RULER_SIZE - MINOR_TICK_LENGTH, pixelPos, RULER_SIZE - 1);
            } else {
                painter->drawLine(RULER_SIZE - MINOR_TICK_LENGTH, pixelPos, RULER_SIZE - 1, pixelPos);
            }
        }
    }
}

void Ruler::mouseMoveEvent(QMouseEvent *event)
{
    m_mouseTracking = true;
    setMousePos(event->pos());
    QWidget::mouseMoveEvent(event);
}

void Ruler::mousePressEvent(QMouseEvent *event)
{
    m_mouseTracking = true;
    setMousePos(event->pos());
    
    // 🌟 点击标尺创建参考线
    if (m_view && event->button() == Qt::LeftButton) {
        QPointF rulerPos = event->position();
        QPointF scenePos;
        
        if (m_orientation == Horizontal) {
            // 水平标尺：创建垂直参考线，使用鼠标的X坐标
            scenePos = m_view->mapToScene(QPoint(rulerPos.x(), 0));
        } else {
            // 垂直标尺：创建水平参考线，使用鼠标的Y坐标
            scenePos = m_view->mapToScene(QPoint(0, rulerPos.y()));
        }
        
        // 发射参考线创建信号
        Qt::Orientation orientation = (m_orientation == Horizontal) ? Qt::Vertical : Qt::Horizontal;
        emit guideRequested(scenePos, orientation);
    }
    
    QWidget::mousePressEvent(event);
}

void Ruler::mouseReleaseEvent(QMouseEvent *event)
{
    m_mouseTracking = false;
    update();
    QWidget::mouseReleaseEvent(event);
}

void Ruler::contextMenuEvent(QContextMenuEvent *event)
{
    m_contextMenu->exec(event->globalPos());
}

void Ruler::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event)
    m_hovered = true;
    update();
}

void Ruler::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    m_hovered = false;
    m_mouseTracking = false;
    update();
}

void Ruler::createContextMenu()
{
    m_contextMenu = new QMenu(this);
    
    // 创建可检查的菜单项
    m_pixelsAction = m_contextMenu->addAction("Pixels", this, &Ruler::setUnitPixels);
    m_millimetersAction = m_contextMenu->addAction("Millimeters", this, &Ruler::setUnitMillimeters);
    m_centimetersAction = m_contextMenu->addAction("Centimeters", this, &Ruler::setUnitCentimeters);
    m_inchesAction = m_contextMenu->addAction("Inches", this, &Ruler::setUnitInches);
    m_pointsAction = m_contextMenu->addAction("Points", this, &Ruler::setUnitPoints);
    
    // 设置为可检查
    m_pixelsAction->setCheckable(true);
    m_millimetersAction->setCheckable(true);
    m_centimetersAction->setCheckable(true);
    m_inchesAction->setCheckable(true);
    m_pointsAction->setCheckable(true);
    
    // 设置为互斥组
    QActionGroup *unitGroup = new QActionGroup(this);
    unitGroup->addAction(m_pixelsAction);
    unitGroup->addAction(m_millimetersAction);
    unitGroup->addAction(m_centimetersAction);
    unitGroup->addAction(m_inchesAction);
    unitGroup->addAction(m_pointsAction);
    unitGroup->setExclusive(true);
    
    // 初始化选中状态
    updateMenuCheckState();
}

void Ruler::updateMenuCheckState()
{
    if (!m_pixelsAction) return; // 菜单还未创建
    
    // 先取消所有选中状态
    m_pixelsAction->setChecked(false);
    m_millimetersAction->setChecked(false);
    m_centimetersAction->setChecked(false);
    m_inchesAction->setChecked(false);
    m_pointsAction->setChecked(false);
    
    // 根据当前单位设置选中状态
    switch (m_unit) {
        case Pixels:
            m_pixelsAction->setChecked(true);
            break;
        case Millimeters:
            m_millimetersAction->setChecked(true);
            break;
        case Centimeters:
            m_centimetersAction->setChecked(true);
            break;
        case Inches:
            m_inchesAction->setChecked(true);
            break;
        case Points:
            m_pointsAction->setChecked(true);
            break;
    }
}

void Ruler::setUnitPixels() { setUnit(Pixels); }
void Ruler::setUnitMillimeters() { setUnit(Millimeters); }
void Ruler::setUnitCentimeters() { setUnit(Centimeters); }
void Ruler::setUnitInches() { setUnit(Inches); }
void Ruler::setUnitPoints() { setUnit(Points); }

qreal Ruler::convertToUnit(qreal pixels) const
{
    switch (m_unit) {
        case Pixels: return pixels;
        case Millimeters: return pixels * 0.264583; // 96 DPI assumption
        case Centimeters: return pixels * 0.0264583;
        case Inches: return pixels * 0.0104167;
        case Points: return pixels * 0.75;
        default: return pixels;
    }
}

qreal Ruler::getUnitScale() const
{
    switch (m_unit) {
        case Pixels: return 1.0;
        case Millimeters: return 3.77953; // 96 DPI
        case Centimeters: return 37.7953;
        case Inches: return 96.0;
        case Points: return 1.33333;
        default: return 1.0;
    }
}

// 🌟 选中对象边界相关方法
void Ruler::setSelectedBounds(const QRectF &bounds)
{
    if (m_selectedBounds != bounds) {
        m_selectedBounds = bounds;
        m_hasSelection = !bounds.isEmpty();
        update();
    }
}

void Ruler::clearSelectedBounds()
{
    if (m_hasSelection) {
        m_selectedBounds = QRectF();
        m_hasSelection = false;
        update();
    }
}

void Ruler::drawSelectionBounds(QPainter *painter)
{
    if (!m_hasSelection || m_selectedBounds.isEmpty() || !m_view) {
        return;
    }
    
    // 设置绘制样式
    QColor boundsColor = QColor(255, 100, 0, 180); // 橙色半透明
    painter->setPen(QPen(boundsColor, 1.0, Qt::DashLine));
    painter->setBrush(Qt::NoBrush);
    
    // 将场景坐标转换为视图坐标
    QRect viewBounds = m_view->mapFromScene(m_selectedBounds).boundingRect();
    
    if (m_orientation == Horizontal) {
        // 水平标尺：绘制左右边界标记
        int left = viewBounds.left();
        int right = viewBounds.right();
        
        // 左边界标记
        if (left >= 0 && left <= width()) {
            painter->drawLine(left, RULER_SIZE - 8, left, RULER_SIZE - 2);
            painter->drawLine(left - 2, RULER_SIZE - 6, left + 2, RULER_SIZE - 6);
        }
        
        // 右边界标记
        if (right >= 0 && right <= width()) {
            painter->drawLine(right, RULER_SIZE - 8, right, RULER_SIZE - 2);
            painter->drawLine(right - 2, RULER_SIZE - 6, right + 2, RULER_SIZE - 6);
        }
        
        // 绘制宽度指示器mask
        if (left >= 0 && right <= width() && right - left > 10) {
            // 绘制mask图形（双向箭头）
            painter->setPen(QPen(boundsColor, 2.0, Qt::SolidLine));
            painter->setBrush(Qt::NoBrush);
            
            // 左箭头
            painter->drawLine(left + 5, RULER_SIZE - 12, left + 12, RULER_SIZE - 12);
            painter->drawLine(left + 5, RULER_SIZE - 12, left + 8, RULER_SIZE - 15);
            painter->drawLine(left + 5, RULER_SIZE - 12, left + 8, RULER_SIZE - 9);
            
            // 右箭头
            painter->drawLine(right - 5, RULER_SIZE - 12, right - 12, RULER_SIZE - 12);
            painter->drawLine(right - 5, RULER_SIZE - 12, right - 8, RULER_SIZE - 15);
            painter->drawLine(right - 5, RULER_SIZE - 12, right - 8, RULER_SIZE - 9);
            
            // 连接线
            painter->drawLine(left + 12, RULER_SIZE - 12, right - 12, RULER_SIZE - 12);
        }
    } else {
        // 垂直标尺：绘制上下边界标记
        int top = viewBounds.top();
        int bottom = viewBounds.bottom();
        
        // 上边界标记
        if (top >= 0 && top <= height()) {
            painter->drawLine(RULER_SIZE - 8, top, RULER_SIZE - 2, top);
            painter->drawLine(RULER_SIZE - 6, top - 2, RULER_SIZE - 6, top + 2);
        }
        
        // 下边界标记
        if (bottom >= 0 && bottom <= height()) {
            painter->drawLine(RULER_SIZE - 8, bottom, RULER_SIZE - 2, bottom);
            painter->drawLine(RULER_SIZE - 6, bottom - 2, RULER_SIZE - 6, bottom + 2);
        }
        
        // 绘制高度指示器mask
        if (top >= 0 && bottom <= height() && bottom - top > 10) {
            // 绘制mask图形（双向箭头）
            painter->setPen(QPen(boundsColor, 2.0, Qt::SolidLine));
            painter->setBrush(Qt::NoBrush);
            
            // 上箭头
            painter->drawLine(RULER_SIZE - 12, top + 5, RULER_SIZE - 12, top + 12);
            painter->drawLine(RULER_SIZE - 12, top + 5, RULER_SIZE - 15, top + 8);
            painter->drawLine(RULER_SIZE - 12, top + 5, RULER_SIZE - 9, top + 8);
            
            // 下箭头
            painter->drawLine(RULER_SIZE - 12, bottom - 5, RULER_SIZE - 12, bottom - 12);
            painter->drawLine(RULER_SIZE - 12, bottom - 5, RULER_SIZE - 15, bottom - 8);
            painter->drawLine(RULER_SIZE - 12, bottom - 5, RULER_SIZE - 9, bottom - 8);
            
            // 连接线
            painter->drawLine(RULER_SIZE - 12, top + 12, RULER_SIZE - 12, bottom - 12);
        }
    }
}



QString Ruler::formatNumber(qreal value) const
{
    // 始终显示为整数
    return QString::number(qRound(value));
}