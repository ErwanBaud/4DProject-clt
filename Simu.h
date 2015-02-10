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
        bool srand;

    private:
        Position p;

    private slots:
        void compute();
        void usleep(int us);

    signals:
        void finished();
        void positionChanged(Position);
};

#endif // SIMU_H
