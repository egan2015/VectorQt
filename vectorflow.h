#ifndef VECTORFLOW_H
#define VECTORFLOW_H

#include <QApplication>
#include <QMainWindow>

class VectorFlow
{
public:
    VectorFlow();
    ~VectorFlow();
    
    int run(int argc, char *argv[]);
    
private:
    void setupApplication();
    void setupMainWindow();
    
    QApplication *m_application;
    QMainWindow *m_mainWindow;
};

#endif // VECTORFLOW_H