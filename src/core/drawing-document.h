#ifndef DRAWING_DOCUMENT_H
#define DRAWING_DOCUMENT_H

#include <QObject>
#include <QString>
#include <QRectF>

class DrawingScene;
class LayerManager;

/**
 * 文档管理器 - 统一管理文档的生命周期
 * 协调Scene和LayerManager，提供统一的文档操作接口
 */
class DrawingDocument : public QObject
{
    Q_OBJECT

public:
    explicit DrawingDocument(QObject *parent = nullptr);
    ~DrawingDocument();

    // 文档生命周期管理
    bool createDocument();          // 创建新文档
    bool closeDocument();           // 关闭文档
    bool isOpen() const;            // 文档是否已打开
    
    // 场景和图层管理
    void setScene(DrawingScene *scene);
    DrawingScene* scene() const { return m_scene; }
    
    LayerManager* layerManager() const { return m_layerManager; }
    
    // 文档属性
    void setFilePath(const QString &filePath);
    QString filePath() const { return m_filePath; }
    void setModified(bool modified);
    bool isModified() const { return m_modified; }
    bool isUntitled() const { return m_isUntitled; }
    
    // 文档操作
    bool save();
    bool saveAs(const QString &filePath);
    bool load(const QString &filePath);

signals:
    void documentCreated();
    void documentClosed();
    void modificationChanged(bool modified);
    void filePathChanged(const QString &filePath);

private:
    void initializeDocument();      // 初始化文档
    void cleanupDocument();         // 清理文档
    
    DrawingScene *m_scene;
    LayerManager *m_layerManager;   // 暂时使用单例，后续改为成员变量
    
    QString m_filePath;
    bool m_modified;
    bool m_isUntitled;
    bool m_open;
};

#endif // DRAWING_DOCUMENT_H