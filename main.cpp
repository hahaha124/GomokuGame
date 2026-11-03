#include <QApplication>
#include "CMainWidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    CMainWidget window;
    window.setWindowTitle("五子棋");
    window.resize(760, 700);
    window.show();
    return app.exec();
}
