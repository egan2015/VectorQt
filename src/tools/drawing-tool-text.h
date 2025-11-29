#ifndef DRAWING_TOOL_TEXT_H
#define DRAWING_TOOL_TEXT_H

#include <QPointF>
#include <QFont>
#include <QColor>
#include "../core/toolbase.h"

class DrawingScene;
class DrawingView;
class DrawingText;
class QGraphicsTextItem;

/**
 * 文本工具
 * 用于创建和编辑文本对象
 */
class DrawingToolText : public ToolBase
{
    Q_OBJECT

public:
    explicit DrawingToolText(QObject *parent = nullptr);

    void activate(DrawingScene *scene, DrawingView *view) override;
    void deactivate() override;

    bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseDoubleClickEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool keyPressEvent(QKeyEvent *event) override;

    // 获取工具光标类型
    CursorManager::CursorType getCursorType() const override { return CursorManager::TextCursor; }

public slots:
    // 响应颜色变化
    void onStrokeColorChanged(const QColor &color);
    void onFillColorChanged(const QColor &color);

    // 字体设置
    void setFontFamily(const QString &family);
    void setFontSize(qreal size);
    void setFontBold(bool bold);
    void setFontItalic(bool italic);
    void setFontUnderline(bool underline);

    // 获取当前设置
    QString fontFamily() const { return m_font.family(); }
    qreal fontSize() const { return m_font.pointSizeF(); }
    bool isFontBold() const { return m_font.bold(); }
    bool isFontItalic() const { return m_font.italic(); }
    bool isFontUnderline() const { return m_font.underline(); }

private:
    // 创建文本对象
    void createText(const QPointF &scenePos);

    // 开始编辑文本
    void beginTextEdit(DrawingText *textItem, const QPointF &scenePos);

    // 结束文本编辑
    void finishTextEdit();

    // 取消文本编辑
    void cancelTextEdit();

    // 获取当前描边颜色
    QColor getCurrentStrokeColor() const;

    // 获取当前填充颜色
    QColor getCurrentFillColor() const;

private:
    DrawingScene *m_scene;
    DrawingView *m_view;

    bool m_isEditing;
    bool m_isCreating;

    // 当前编辑的文本对象
    DrawingText *m_currentText;
    QGraphicsTextItem *m_textEditor;

    // 文本样式
    QFont m_font;
    QColor m_currentStrokeColor;
    QColor m_currentFillColor;

    // 文本内容
    QString m_defaultText;
    QPointF m_startPos;
};

#endif // DRAWING_TOOL_TEXT_H