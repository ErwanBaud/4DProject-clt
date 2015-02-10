#ifndef SSLSERVER_H
#define SSLSERVER_H

#include <QtNetwork>

class SslServer : public QTcpServer
{
    Q_OBJECT

    public:
        QSslSocket* nextPendingConnection();
        void incomingConnection(qintptr handle);
        bool hasPendingConnections() const;

    protected:

    private:
        QQueue<QSslSocket*> queue;
};

#endif // SSLSERVER_H
