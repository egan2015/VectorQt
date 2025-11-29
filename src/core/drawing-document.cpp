#include <QDebug>
#include "drawing-document.h"
#include "../ui/drawingscene.h"
#include "layer-manager.h"
#include "svghandler.h"
#include "../ui/command-manager.h"

DrawingDocument::DrawingDocument(QObject *parent)
    : QObject(parent)
    , m_scene(nullptr)
    , m_layerManager(nullptr)
    , m_modified(false)
    , m_isUntitled(true)
    , m_open(false)
{
}

DrawingDocument::~DrawingDocument()
{
    closeDocument();
}

bool DrawingDocument::createDocument()
{
    // 如果已有文档打开，先关闭
    if (m_open) {
        closeDocument();
    }
    
    // 使用LayerManager单例（后续改为成员变量）
    m_layerManager = LayerManager::instance();
    
    // 初始化新文档
    initializeDocument();
    
    m_open = true;
    m_modified = false;
    m_isUntitled = true;
    m_filePath.clear();
    
    emit documentCreated();
    emit modificationChanged(false);
    emit filePathChanged(QString());
    
    return true;
}

bool DrawingDocument::closeDocument()
{
    if (!m_open) {
        return true;
    }
    
    // 检查是否需要保存
    if (m_modified) {
        // 这里应该询问用户是否保存，暂时返回false
        return false;
    }
    
    cleanupDocument();
    
    m_open = false;
    emit documentClosed();
    
    return true;
}

bool DrawingDocument::isOpen() const
{
    return m_open;
}

void DrawingDocument::setScene(DrawingScene *scene)
{
    m_scene = scene;
    if (m_layerManager && m_scene) {
        m_layerManager->setScene(m_scene);
    }
}

void DrawingDocument::setFilePath(const QString &filePath)
{
    if (m_filePath != filePath) {
        m_filePath = filePath;
        m_isUntitled = filePath.isEmpty();
        emit filePathChanged(filePath);
    }
}

void DrawingDocument::setModified(bool modified)
{
    if (m_modified != modified) {
        m_modified = modified;
        emit modificationChanged(modified);
    }
}

bool DrawingDocument::save()
{
    if (!m_open || !m_scene) {
        return false;
    }
    
    if (m_isUntitled || m_filePath.isEmpty()) {
        return false;  // 需要调用saveAs
    }
    
    return SvgHandler::exportToSvg(m_scene, m_filePath);
}

bool DrawingDocument::saveAs(const QString &filePath)
{
    if (!m_open || !m_scene) {
        return false;
    }
    
    if (SvgHandler::exportToSvg(m_scene, filePath)) {
        setFilePath(filePath);
        setModified(false);
        return true;
    }
    
    return false;
}

bool DrawingDocument::load(const QString &filePath)
{
    if (!m_scene) {
        return false;
    }
    
    // 如果已有文档打开，先关闭
    if (m_open) {
        closeDocument();
    }
    
    // 初始化文档
    initializeDocument();
    
    // 加载SVG文件
    if (SvgHandler::importFromSvg(m_scene, filePath)) {
        m_open = true;
        m_modified = false;
        m_isUntitled = false;
        m_filePath = filePath;
        
        emit documentCreated();
        emit modificationChanged(false);
        emit filePathChanged(filePath);
        
        return true;
    }
    
    // 加载失败，清理
    cleanupDocument();
    return false;
}

void DrawingDocument::initializeDocument()
{
    // 清理场景
    if (m_scene) {
        m_scene->clearScene();
    }
    
    // 清理图层
    if (m_layerManager) {
        m_layerManager->clearAllLayers();
        // 重新设置Scene，这会触发创建默认图层
        m_layerManager->setScene(m_scene);
    }
    
    // 清理撤销栈
    if (CommandManager::hasInstance()) {
        CommandManager::instance()->clear();
    }
}

void DrawingDocument::cleanupDocument()
{
    // 清理场景
    if (m_scene) {
        m_scene->clearScene();
    }
    
    // 清理图层
    if (m_layerManager) {
        m_layerManager->clearAllLayers();
    }
    
    // 清理撤销栈
    if (CommandManager::hasInstance()) {
        CommandManager::instance()->clear();
    }
}


