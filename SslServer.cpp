#include "SslServer.h"


/* Surcharge de QTcpServer::incomingConnection() pour
 * recuperer un QSslSocket * (au lieu de QTcpSocket *)
 * */
void SslServer::incomingConnection(qintptr handle)
{
    //qDebug() << "Incoming connection.";
    QSslSocket *newSocket = new QSslSocket(this);
    if(!newSocket->setSocketDescriptor(handle))
        return;

    queue.enqueue(newSocket);
}

QSslSocket* SslServer::nextPendingConnection()
{
    //qDebug() << "Next pending connection.";
    if(queue.isEmpty())
        return 0;
    else
        return queue.dequeue();
}

bool SslServer::hasPendingConnections() const
{
    //qDebug() << "Has pending connection.";
    return !queue.isEmpty();
}
