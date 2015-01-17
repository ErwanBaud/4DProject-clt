# include <QCoreApplication>
# include <cstring>
# include <iostream>
# include "ClientCore.h"

using namespace std;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    ClientCore clientCore;

    return a.exec();
}
