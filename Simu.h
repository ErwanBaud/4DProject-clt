#ifndef SIMU_H
#define SIMU_H

#include <QObject>
#include <QDebug>
#include <QThread>
#include <cstdlib>

class Simu : public QObject
{
    Q_OBJECT

    public:
        Simu();

    private:
        double x;

    private slots:
        void compute();
        void sleep(int s);
        //void computeM();

    signals:
        void finished();
        void xChanged(double);
        //void error(QString err);
};

#endif // SIMU_H
