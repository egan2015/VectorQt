#ifndef VECTORFLOW_H
#define VECTORQT_H

#include <QApplication>
#include <QMainWindow>

class VectorQt
{
public:
    VectorQt();
    ~VectorQt();
    
    int run(int argc, char *argv[]);
    
private:
    void setupApplication();
    void setupMainWindow();
    
    QApplication *m_application;
    QMainWindow *m_mainWindow;
};

#endif // VECTORFLOW_H