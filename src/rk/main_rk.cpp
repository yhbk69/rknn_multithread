/*
 * main_rk.cpp - RK3588 Qt GUI 版本入口
 */

#include <QApplication>
#include "rk/mainwindow.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow window;
    window.show();

    return app.exec();
}
