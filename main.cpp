// main.cpp

#include <QApplication>
#include <QDesktopWidget>

#include "window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Window window;
    window.setWindowTitle("OpenGL with Qt");

    window.show();
    //window.showMaximized();

    return app.exec();
}
