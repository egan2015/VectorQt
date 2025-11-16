#include "vectorflow.h"
#include "mainwindow.h"
#include <QApplication>

VectorFlow::VectorFlow()
{
}

VectorFlow::~VectorFlow()
{
}

void VectorFlow::setupApplication()
{
}

void VectorFlow::setupMainWindow()
{
}

int VectorFlow::run(int argc, char *argv[])
{
    m_application = new QApplication(argc, argv);
    
    setupApplication();
    setupMainWindow();
    
    return m_application->exec();
}

