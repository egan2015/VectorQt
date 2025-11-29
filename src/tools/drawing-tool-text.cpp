#include <QGraphicsScene>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFontDialog>
#include <QGraphicsTextItem>
#include <QTextCursor>
#include <QTextDocument>
#include <QPen>
#include <QBrush>
#include <QDebug>
#include "drawing-tool-text.h"
#include "../ui/drawingscene.h"
#include "../ui/drawingview.h"
#include "../core/drawing-shape.h"
#include "../core/drawing-layer.h"
#include "../core/layer-manager.h"
#include "../ui/mainwindow.h"
#include "../ui/colorpalette.h"
#include "../ui/command-manager.h"

DrawingToolText::DrawingToolText(QObject *parent)
    : ToolBase(parent)
    , m_scene(nullptr)
    , m_view(nullptr)
    , m_isEditing(false)
    , m_isCreating(false)
    , m_currentText(nullptr)
    , m_textEditor(nullptr)
    , m_font(QFont("Arial", 12))
    , m_currentStrokeColor(Qt::black)
    , m_currentFillColor(Qt::black)
    , m_defaultText("输入文本")
{
    // 设置默认字体
    m_font.setBold(false);
    m_font.setItalic(false);
    m_font.setUnderline(false);
}

void DrawingToolText::activate(DrawingScene *scene, DrawingView *view)
{
    m_scene = scene;
    m_view = view;
    setState(ToolState::Active);
    setToolCursor();
    
    // 连接颜色变化信号
    MainWindow *mainWindow = qobject_cast<MainWindow*>(scene->parent());
    if (mainWindow) {
        ColorPalette *palette = mainWindow->findChild<ColorPalette*>();
        if (palette) {
            connect(palette, &ColorPalette::strokeColorChanged, this, &DrawingToolText::onStrokeColorChanged);
            connect(palette, &ColorPalette::fillColorChanged, this, &DrawingToolText::onFillColorChanged);
            
            // 获取当前颜色
            m_currentStrokeColor = palette->getCurrentStrokeColor();
            m_currentFillColor = palette->getCurrentFillColor();
        }
    }
    
    onActivated();
}

void DrawingToolText::deactivate()
{
    if (m_isEditing) {
        finishTextEdit();
    }
    
    // 断开信号连接
    MainWindow *mainWindow = m_scene ? qobject_cast<MainWindow*>(m_scene->parent()) : nullptr;
    if (mainWindow) {
        ColorPalette *palette = mainWindow->findChild<ColorPalette*>();
        if (palette) {
            disconnect(palette, &ColorPalette::strokeColorChanged, this, &DrawingToolText::onStrokeColorChanged);
            disconnect(palette, &ColorPalette::fillColorChanged, this, &DrawingToolText::onFillColorChanged);
        }
    }
    
    setState(ToolState::Inactive);
    restoreDefaultCursor();
    onDeactivated();
}

bool DrawingToolText::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (!m_scene || !m_view) {
        return false;
    }
    
    if (event->button() == Qt::LeftButton) {
        if (m_isEditing) {
            // 如果正在编辑，检查是否点击了其他地方
            if (m_textEditor && !m_textEditor->contains(m_textEditor->mapFromScene(scenePos))) {
                finishTextEdit();
                return false;
            }
            return false; // 让文本编辑器处理事件
        }
        
        // 开始创建文本
        m_isCreating = true;
        m_startPos = scenePos;
        createText(scenePos);
        return true;
    }
    
    return false;
}

bool DrawingToolText::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    Q_UNUSED(event)
    Q_UNUSED(scenePos)
    
    if (m_isEditing) {
        return false; // 让文本编辑器处理事件
    }
    
    return false;
}

bool DrawingToolText::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    Q_UNUSED(event)
    Q_UNUSED(scenePos)
    
    if (m_isCreating) {
        m_isCreating = false;
    }
    
    return false;
}

bool DrawingToolText::mouseDoubleClickEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (!m_scene || !m_view) {
        return false;
    }
    
    if (event->button() == Qt::LeftButton) {
        // 检查是否双击了现有文本对象
        QGraphicsItem *item = m_scene->itemAt(scenePos, m_view->transform());
        if (item) {
            DrawingText *textItem = dynamic_cast<DrawingText*>(item);
            if (textItem) {
                beginTextEdit(textItem, scenePos);
                return true;
            }
        }
    }
    
    return false;
}

bool DrawingToolText::keyPressEvent(QKeyEvent *event)
{
    if (m_isEditing) {
        if (event->key() == Qt::Key_Escape) {
            cancelTextEdit();
            return true;
        } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            if (event->modifiers() & Qt::ShiftModifier) {
                // Shift+Enter 在文本中插入换行
                return false; // 让文本编辑器处理
            } else {
                // Enter 完成编辑
                finishTextEdit();
                return true;
            }
        }
        return false; // 让文本编辑器处理其他按键
    }
    
    return false;
}

void DrawingToolText::onStrokeColorChanged(const QColor &color)
{
    m_currentStrokeColor = color;
    
    if (m_currentText) {
        m_currentText->setStrokePen(QPen(color, m_currentText->strokePen().widthF()));
    }
}

void DrawingToolText::onFillColorChanged(const QColor &color)
{
    m_currentFillColor = color;
    
    if (m_currentText) {
        m_currentText->setFillBrush(QBrush(color));
    }
}

void DrawingToolText::setFontFamily(const QString &family)
{
    m_font.setFamily(family);
    
    if (m_currentText) {
        m_currentText->setFont(m_font);
    }
}

void DrawingToolText::setFontSize(qreal size)
{
    m_font.setPointSizeF(size);
    
    if (m_currentText) {
        m_currentText->setFont(m_font);
    }
}

void DrawingToolText::setFontBold(bool bold)
{
    m_font.setBold(bold);
    
    if (m_currentText) {
        m_currentText->setFont(m_font);
    }
}

void DrawingToolText::setFontItalic(bool italic)
{
    m_font.setItalic(italic);
    
    if (m_currentText) {
        m_currentText->setFont(m_font);
    }
}

void DrawingToolText::setFontUnderline(bool underline)
{
    m_font.setUnderline(underline);
    
    if (m_currentText) {
        m_currentText->setFont(m_font);
    }
}

void DrawingToolText::createText(const QPointF &scenePos)
{
    // 创建新的文本对象
    m_currentText = new DrawingText(m_defaultText);
    m_currentText->setFont(m_font);
    m_currentText->setFillBrush(QBrush(m_currentFillColor));
    m_currentText->setStrokePen(QPen(m_currentStrokeColor, 1.0));
    m_currentText->setPosition(scenePos);
    
    // 使用CreateCommand添加到场景
    CommandManager *commandManager = CommandManager::instance();
    if (commandManager) {
        commandManager->pushCommand(new CreateCommand(commandManager, m_currentText, "创建文本"));
    } else {
        // 如果没有CommandManager，直接添加到场景
        m_scene->addItem(m_currentText);
    }
    
    // 开始编辑文本
    beginTextEdit(m_currentText, scenePos);
    
    // 发出形状创建完成信号
    emit shapeFinished(m_currentText);
}

void DrawingToolText::beginTextEdit(DrawingText *textItem, const QPointF &scenePos)
{
    if (!textItem || m_isEditing) {
        return;
    }
    
    m_isEditing = true;
    m_currentText = textItem;
    
    // 创建文本编辑器
    m_textEditor = new QGraphicsTextItem();
    m_textEditor->setFont(textItem->font());
    m_textEditor->setDefaultTextColor(textItem->fillBrush().color());
    m_textEditor->setPlainText(textItem->text());
    m_textEditor->setPos(scenePos);
    
    // 设置文本编辑器属性
    m_textEditor->setTextInteractionFlags(Qt::TextEditorInteraction);
    m_textEditor->setFlag(QGraphicsItem::ItemIsFocusable, true);
    m_textEditor->setFlag(QGraphicsItem::ItemIsSelectable, true);
    
    // 添加到场景
    m_scene->addItem(m_textEditor);
    
    // 设置焦点并选中所有文本
    m_textEditor->setFocus();
    QTextCursor cursor = m_textEditor->textCursor();
    cursor.select(QTextCursor::Document);
    m_textEditor->setTextCursor(cursor);
    
    // 隐藏原始文本对象
    textItem->setVisible(false);
}

void DrawingToolText::finishTextEdit()
{
    if (!m_isEditing || !m_textEditor || !m_currentText) {
        return;
    }
    
    // 更新文本内容
    QString newText = m_textEditor->toPlainText();
    QString oldText = m_currentText->text();
    
    // 如果文本有变化，使用TextEditCommand
    if (oldText != newText) {
        CommandManager *commandManager = CommandManager::instance();
        if (commandManager) {
            commandManager->pushCommand(new TextEditCommand(commandManager, m_currentText, oldText, newText));
        } else {
            m_currentText->setText(newText);
        }
    }
    
    // 显示原始文本对象
    m_currentText->setVisible(true);
    
    // 清理文本编辑器
    m_scene->removeItem(m_textEditor);
    delete m_textEditor;
    m_textEditor = nullptr;
    
    m_isEditing = false;
    m_currentText = nullptr;
}

void DrawingToolText::cancelTextEdit()
{
    if (!m_isEditing || !m_textEditor || !m_currentText) {
        return;
    }
    
    // 如果是新创建的文本且内容为默认文本，则删除它
    if (m_textEditor->toPlainText() == m_defaultText) {
        CommandManager *commandManager = CommandManager::instance();
        if (commandManager) {
            // 撤销创建命令
            commandManager->undo();
        } else {
            m_scene->removeItem(m_currentText);
            delete m_currentText;
        }
    } else {
        // 显示原始文本对象
        m_currentText->setVisible(true);
    }
    
    // 清理文本编辑器
    m_scene->removeItem(m_textEditor);
    delete m_textEditor;
    m_textEditor = nullptr;
    
    m_isEditing = false;
    m_currentText = nullptr;
}

QColor DrawingToolText::getCurrentStrokeColor() const
{
    MainWindow *mainWindow = m_scene ? qobject_cast<MainWindow*>(m_scene->parent()) : nullptr;
    if (mainWindow) {
        ColorPalette *palette = mainWindow->findChild<ColorPalette*>();
        if (palette) {
            return palette->getCurrentStrokeColor();
        }
    }
    return m_currentStrokeColor;
}

QColor DrawingToolText::getCurrentFillColor() const
{
    MainWindow *mainWindow = m_scene ? qobject_cast<MainWindow*>(m_scene->parent()) : nullptr;
    if (mainWindow) {
        ColorPalette *palette = mainWindow->findChild<ColorPalette*>();
        if (palette) {
            return palette->getCurrentFillColor();
        }
    }
    return m_currentFillColor;
}