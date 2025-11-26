#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <QObject>
#include <QString>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>

class DrawingCanvas;
class DrawingScene;
class MainWindow;

class FileManager : public QObject
{
    Q_OBJECT

public:
    explicit FileManager(MainWindow *parent = nullptr);
    ~FileManager();

    void setCanvas(DrawingCanvas *canvas);
    DrawingCanvas *canvas() const;

signals:
    void fileOpened(const QString &filePath);
    void fileSaved(const QString &filePath);
    void fileExported(const QString &filePath);
    void statusMessageChanged(const QString &message);
    void windowTitleChanged(const QString &title);

public:
    void updateWindowTitle(); // 添加公共方法

public slots:
    void newFile();
    void openFile();
    bool saveFile();
    bool saveFileAs();
    void exportFile();

private:
    bool maybeSave();
    void loadFile(const QString &filePath);
    bool saveFile(const QString &filePath);
    bool exportFile(const QString &filePath);
    void setCurrentFile(const QString &filePath);

private:
    MainWindow *m_mainWindow;
    DrawingCanvas *m_canvas;
    QString m_currentFilePath;
    bool m_isUntitled;
};

#endif // FILE_MANAGER_H