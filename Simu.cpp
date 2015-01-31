#include "Simu.h"

# define MAX 10

Simu::Simu()
{
    p.x = 0.0;
    p.y = 0.0;
    p.z = 0.0;
}


void Simu::compute()
{
//    qDebug();
//    qDebug() << "Simu::compute get called from?: " << QThread::currentThreadId();

    qsrand(QTime::currentTime().msec());

    usleep(qrand()%50);

    p.x = (double) qrand() / (double) RAND_MAX * MAX;
    p.y = (double) qrand() / (double) RAND_MAX * MAX;
    p.z = (double) qrand() / (double) RAND_MAX * MAX;

    emit positionChanged(p);
}


void Simu::usleep(int us) {
    //qDebug() << "Simu::usleep(" << us << ") get called from?: " << QThread::currentThreadId();
    QThread::usleep(us);
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
