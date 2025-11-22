#include "../core/vectorflow.h"
#include "../ui/mainwindow.h"
#include <QApplication>

VectorQt::VectorQt()
{
}

VectorQt::~VectorQt()
{
}

void VectorQt::setupApplication()
{
}

void VectorQt::setupMainWindow()
{
}

int VectorQt::run(int argc, char *argv[])
{
    m_application = new QApplication(argc, argv);
    
    setupApplication();
    setupMainWindow();
    
    return m_application->exec();
}

