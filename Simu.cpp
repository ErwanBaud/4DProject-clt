#include "Simu.h"

void Simu::compute()
{
    QTextStream(stdout) << "Simu::compute get called from?: " << QThread::currentThreadId() << endl;
}

void Simu::sleep(int s) {
    QTextStream(stdout) << "Simu::sleep get called from?: " << QThread::currentThreadId() << endl;
    QThread::sleep(s);
}
