#include "Simu.h"

# define MAX 10

Simu::Simu()
{
    srand = false;
    p.x = 0.0;
    p.y = 0.0;
    p.z = 0.0;
}


void Simu::compute()
{
    //qDebug() << endl << "Simu::compute get called from?: " << QThread::currentThreadId();

    if( !srand )
    {
        qsrand(QTime::currentTime().msec() + (uint)QThread::currentThreadId());
        srand = true;
    }

    usleep((qrand()%700) + 300);

    // Consommation des données stockées


    p.x = (double) qrand() / (double) RAND_MAX * MAX;
    p.y = (double) qrand() / (double) RAND_MAX * MAX;
    p.z = (double) qrand() / (double) RAND_MAX * MAX;

    emit positionChanged(p);
}


void Simu::usleep(int us) {
    //qDebug() << "Simu::usleep(" << us << ") get called from?: " << QThread::currentThreadId();
    QThread::usleep(us);
}
