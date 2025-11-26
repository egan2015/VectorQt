#include "file-manager.h"
#include "mainwindow.h"
#include "drawingscene.h"
#include "../core/drawing-canvas.h"
#include "../core/drawing-document.h"
#include "../core/svghandler.h"
#include "../core/layer-manager.h"
#include "../tools/tool-manager.h"
#include <QFileInfo>

FileManager::FileManager(MainWindow *parent)
    : QObject(parent)
    , m_mainWindow(parent)
    , m_canvas(nullptr)
    , m_isUntitled(true)
{
}

FileManager::~FileManager()
{
}

void FileManager::setCanvas(DrawingCanvas *canvas)
{
    m_canvas = canvas;
}

DrawingCanvas *FileManager::canvas() const
{
    return m_canvas;
}

void FileManager::newFile()
{
    if (!m_canvas) return;
    
    if (maybeSave()) {
        // 清除现有内容
        if (DrawingScene* drawingScene = qobject_cast<DrawingScene*>(m_canvas->scene())) {
            drawingScene->clearScene();
        }
        m_currentFilePath.clear();
        m_isUntitled = true;
        
        updateWindowTitle();
        emit statusMessageChanged("新文档已创建");
    }
}

void FileManager::openFile()
{
    if (!m_canvas) return;
    
    if (maybeSave()) {
        QString fileName = QFileDialog::getOpenFileName(m_mainWindow,
                                                        "打开文档", "", "SVG Files (*.svg);;VectorQt Files (*.vfp)");

        if (!fileName.isEmpty()) {
            loadFile(fileName);
        }
    }
}

bool FileManager::saveFile()
{
    if (!m_canvas) return false;
    
    if (m_isUntitled) {
        return saveFileAs();
    } else {
        return saveFile(m_currentFilePath);
    }
}

bool FileManager::saveFileAs()
{
    if (!m_canvas) return false;
    
    QString fileName = QFileDialog::getSaveFileName(m_mainWindow,
                                                    "保存文档", "", "SVG Files (*.svg)");

    if (!fileName.isEmpty()) {
        // 确保文件有 .svg 扩展名
        if (!fileName.endsWith(".svg", Qt::CaseInsensitive)) {
            fileName += ".svg";
        }
        
        return saveFile(fileName);
    }
    
    return false;
}

void FileManager::exportFile()
{
    if (!m_canvas) return;
    
    QString fileName = QFileDialog::getSaveFileName(m_mainWindow, 
                                                    tr("导出文档"), "", "SVG Files (*.svg)");

    if (!fileName.isEmpty()) {
        exportFile(fileName);
    }
}

bool FileManager::maybeSave()
{
    if (!m_canvas) return true;
    
    if (DrawingScene* drawingScene = qobject_cast<DrawingScene*>(m_canvas->scene())) {
        if (drawingScene->isModified()) {
            QMessageBox::StandardButton reply = QMessageBox::question(m_mainWindow, "VectorQt",
                                                                      "文档已修改，是否保存？",
                                                                      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

            if (reply == QMessageBox::Save) {
                return saveFile();
            } else if (reply == QMessageBox::Cancel) {
                return false;
            }
        }
    }
    
    return true;
}

void FileManager::loadFile(const QString &filePath)
{
    if (!m_canvas) return;
    
    QFileInfo fileInfo(filePath);
    
    // 清除现有内容
    if (DrawingScene* drawingScene = qobject_cast<DrawingScene*>(m_canvas->scene())) {
        drawingScene->clearScene();
        drawingScene->clearSelection();
    }
    m_currentFilePath.clear();
    
    // 重置当前工具 - TODO: 通过信号获取工具管理器
    
    bool success = false;
    
    if (fileInfo.suffix().toLower() == "svg") {
        // SVG导入
        if (DrawingScene* drawingScene = qobject_cast<DrawingScene*>(m_canvas->scene())) {
            success = SvgHandler::importFromSvg(drawingScene, filePath);
        }
    } else {
        // 其他格式文件加载
        // TODO: 实现其他格式支持
        success = false;
    }
    
    if (success) {
        m_currentFilePath = filePath;
        m_isUntitled = false;
        
        // 加载完成后重置视图到100%
        m_canvas->resetZoom();
        m_canvas->centerOnContent();
        
        updateWindowTitle();
        emit fileOpened(filePath);
        emit statusMessageChanged(QString("文件已打开: %1").arg(fileInfo.fileName()));
    } else {
        emit statusMessageChanged("无法打开文件");
        QMessageBox::warning(m_mainWindow, "打开错误", "无法打开文件");
    }
}

bool FileManager::saveFile(const QString &filePath)
{
    if (!m_canvas) return false;
    
    bool success = false;
    if (DrawingScene* drawingScene = qobject_cast<DrawingScene*>(m_canvas->scene())) {
        success = SvgHandler::exportToSvg(drawingScene, filePath);
    }
    
    if (success) {
        m_currentFilePath = filePath;
        m_isUntitled = false;
        
        updateWindowTitle();
        emit fileSaved(filePath);
        emit statusMessageChanged(QString("文档已保存: %1").arg(QFileInfo(filePath).fileName()));
    } else {
        emit statusMessageChanged("保存失败");
        QMessageBox::warning(m_mainWindow, "保存错误", "无法保存文件");
    }
    
    return success;
}

bool FileManager::exportFile(const QString &filePath)
{
    if (!m_canvas) return false;
    
    bool success = false;
    if (DrawingScene* drawingScene = qobject_cast<DrawingScene*>(m_canvas->scene())) {
        success = SvgHandler::exportToSvg(drawingScene, filePath);
    }
    
    if (success) {
        emit fileExported(filePath);
        emit statusMessageChanged("文档已导出");
    } else {
        emit statusMessageChanged("导出失败");
        QMessageBox::warning(m_mainWindow, "导出失败", "无法导出文档");
    }
    
    return success;
}

void FileManager::setCurrentFile(const QString &filePath)
{
    m_currentFilePath = filePath;
    m_isUntitled = filePath.isEmpty();
    updateWindowTitle();
}

void FileManager::updateWindowTitle()
{
    QString title;
    
    if (m_isUntitled) {
        title = "未命名";
    } else {
        QFileInfo fileInfo(m_currentFilePath);
        title = fileInfo.fileName();
    }
    
    if (DrawingScene* drawingScene = qobject_cast<DrawingScene*>(m_canvas ? m_canvas->scene() : nullptr)) {
        if (drawingScene->isModified()) {
            title += " *";
        }
    }
    
    title += " - VectorQt";
    
    emit windowTitleChanged(title);
}