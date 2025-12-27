#include "ui/ps2snd.h"
#include <QApplication>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
