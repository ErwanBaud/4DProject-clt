#ifndef SIMU_H
#define SIMU_H

#include <QObject>
#include <QDebug>
#include <QThread>

class Simu : public QObject
{
    Q_OBJECT

    public:

    private slots:
        void compute();
        void sleep(int s);

    signals:
        void finished();
        //void error(QString err);
};

#endif // SIMU_H
