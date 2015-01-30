#include "Simu.h"

Simu::Simu()
{
    x = 0;
}

/*
void Simu::computeM()
{
    for(;;)
    {
        QTextStream(stdout) << "Simu::computeM get called from?: " << QThread::currentThreadId() << endl;
        sleep(2);
    }
}
*/

void Simu::compute()
{
    QTextStream(stdout) << "Simu::compute get called from?: " << QThread::currentThreadId() << endl;
    x += rand()%10;
    emit xChanged(x);
}

void Simu::sleep(int s) {
    QTextStream(stdout) << "Simu::sleep get called from?: " << QThread::currentThreadId() << endl;
    QThread::sleep(s);
}
