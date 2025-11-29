#include <iostream>
#include <QApplication>
#include <QGraphicsView>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsScene>
#include <QTranslator>
#include <QLibraryInfo>
#include "mainwindow.h"
#include "../core/memory-manager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // 设置应用程序信息
    a.setApplicationName("VectorQt");
    a.setApplicationVersion("1.0.0");
    a.setOrganizationName("VectorQt Team");
    
    // 加载翻译
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "vectorqt_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    
    MainWindow window;
    window.show();
    
    return a.exec();
}