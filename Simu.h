#ifndef SIMU_H
#define SIMU_H

#include <QObject>
#include <QDebug>
#include <QThread>
#include <QtGlobal>
#include <QTime>
#include <cstdlib>

typedef struct
{
    double x;
    double y;
    double z;
} Position;

class Simu : public QObject
{
    Q_OBJECT

    public:
        Simu();

    private:
        Position p;

    private slots:
        void compute();
        void usleep(int us);
        //void computeM();

    signals:
        void finished();
        void positionChanged(Position);
        //void error(QString err);
};

#endif // SIMU_H
