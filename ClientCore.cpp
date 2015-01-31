#include <iostream>
#include "ClientCore.h"

#define tAliveTimer 2000
#define tComputeTimer 1000

using namespace std;

/* Constructeur
 * */
ClientCore::ClientCore()
{
    appPort = 50885;

    // Demarrage du client
    fromServer = new QTcpServer(this); // Socket d'écoute hyperviseur
    fromClient = new QTcpServer(this); // Socket d'écoute clients

    if (!fromServer->listen(QHostAddress("127.0.0.1"), 0)) // Démarrage de l'ecoute hyperviseur sur un port aleatoire
        // Si l'ecoute hyperviseur n'a pas été démarré correctement
        QTextStream(stdout) << "L'ecoute hyperviseur n'a pas pu etre demarre. Raison : " + fromServer->errorString() << endl;

    else
        if (!fromClient->listen(QHostAddress("127.0.0.1"), 0)) // Démarrage de l'ecoute client sur un port aleatoire
            // Si l'ecoute client n'a pas été démarré correctement
            QTextStream(stdout) << "L'ecoute client n'a pas pu etre demarre. Raison : " + fromClient->errorString() << endl;
        else

            // Initialisation des composants iamAlive
            udpBroadSocket = new QUdpSocket(this);
            if ( !(udpBroadSocket->bind(appPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) )
                QTextStream(stdout) << "    Erreur socket UDP." << endl;
            else
                connect(udpBroadSocket, SIGNAL(readyRead()), this, SLOT(clientAlive()));

                // Le client est démarré et toutes ses sockets sont opérationnelles

                // Renseignement des attributs
                tailleMessage = 0;
                state = false;
                host = fromServer->serverAddress();
                portS = fromServer->serverPort();
                portC = fromClient->serverPort();
                hostPortSC = host.toString() + ":" + QString::number(portS) + ":" + QString::number(portC);

                connect(fromServer, SIGNAL(newConnection()), this, SLOT(connexionHyperviseur()));
                connect(fromClient, SIGNAL(newConnection()), this, SLOT(connexionClient()));


                // Initialisation des composants utiles au broadcast iamAlive
                tAlive = new QTimer();
                tAlive->setInterval(tAliveTimer);
                connect(tAlive, SIGNAL(timeout()), this, SLOT(iamAlive()));
                tAlive->start();


                QTextStream(stdout) << "I am alive on " + host.toString()<< " portS:" << portS << " portC:" << portC << endl;
                QTextStream(stdout) << "From main thread: " << QThread::currentThreadId() << endl;
}


/* Envoi du message iamAlive
 * du type host#portS#portC#state
 * exemple : "127.0.0.1#12345#67890#0"
 * */
void ClientCore::iamAlive()
{
    QByteArray datagram;
    datagram.append(host.toString());
    datagram.append("#");
    datagram.append(QByteArray::number(portS));
    datagram.append("#");
    datagram.append(QByteArray::number(portC));
    datagram.append("#");
    datagram.append(QString::number(state));
    udpBroadSocket->writeDatagram(datagram.data(), datagram.size(), QHostAddress::Broadcast, appPort);

    //QTextStream(stdout) << "ClientCore::iamAlive " << datagram << " get called from?: " << QThread::currentThreadId() << endl;
}


/* Executé a chaque reception d'un iamAlive
 * Ajout des nouveaux clients à la liste
 * */
void ClientCore::clientAlive()
{
    // Tant qu'il y a des paquets reçus
     while (udpBroadSocket->hasPendingDatagrams())
     {
         QByteArray datagram;
         QList<QByteArray> mess;
         datagram.resize(udpBroadSocket->pendingDatagramSize());
         udpBroadSocket->readDatagram(datagram.data(), datagram.size());
         mess = datagram.split('#');

         QHostAddress host = QHostAddress(QString(mess[0]));
         quint16 port = mess[2].toUShort();

         if( port != portC && !socketIsIn(host, port, others))
         {
             QTcpSocket *socket = new QTcpSocket(this);

             QTextStream(stdout) << "    Tentative de connexion a " << host.toString() << ":" << port << endl;

             socket->connectToHost(host, port); // On se connecte au serveur demandé
             socket->waitForConnected();
             others << socket;

             QTextStream(stdout) << "    Connecte a " << host.toString() << ":" << port << endl;

             //connect(client->toClient, SIGNAL(readyRead()), this, SLOT(donneesRecues()));
             //connect(client->toClient, SIGNAL(disconnected()), this, SLOT(deconnexionClient()));
         }
     }
}


/* Connexion de l'hyperviseur
 * */
void ClientCore::connexionHyperviseur()
{
    QTcpSocket *hyperviseur = fromServer->nextPendingConnection();
    toServer = hyperviseur;

    QTextStream(stdout) << "L'hyperviseur vient de se connecter : "
         << hyperviseur->peerAddress().toString() << ":" << hyperviseur->peerPort() << endl;

    connect(hyperviseur, SIGNAL(readyRead()), this, SLOT(receptionHyperviseur()));
    connect(hyperviseur, SIGNAL(disconnected()), this, SLOT(deconnexionHyperviseur()));
}


/* Deconnexion d'un processus client
 * */
void ClientCore::deconnexionHyperviseur()
{
    QTextStream(stdout) << "L'hyperviseur vient de se deconnecter." << endl;
    toServer->deleteLater();
}


/* Connexion des clients
 * */
void ClientCore::connexionClient()
{
    QTcpSocket *nouveauClient = fromClient->nextPendingConnection();
    others << nouveauClient;

    QTextStream(stdout) << "Un nouveau client vient de se connecter : "
         << nouveauClient->peerAddress().toString() << ":" << nouveauClient->peerPort() << endl;

    connect(nouveauClient, SIGNAL(readyRead()), this, SLOT(receptionClient()));
    connect(nouveauClient, SIGNAL(disconnected()), this, SLOT(deconnexionClient()));
}


/* Deconnexion d'un processus client
 * */
void ClientCore::deconnexionClient()
{
    // On détermine quel client se déconnecte
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket == 0) // Si par hasard on n'a pas trouvé le client à l'origine du signal, on arrête la méthode
        return;

    QTextStream(stdout) << "Un client vient de se deconnecter" << endl;
    others.removeOne(socket);
    socket->deleteLater();
}


/* Renvoie true si la socket correspondante est deja dans la liste others
 * */
bool ClientCore::socketIsIn(QHostAddress host, quint16 port, QList<QTcpSocket *> &others)
{
    for( QList<QTcpSocket *>::Iterator socketIterator = others.begin(); socketIterator != others.end(); ++socketIterator )
        if( ((*socketIterator)->peerAddress() == host) && ((*socketIterator)->peerPort() == port) )
            return true;

    return false;
}


/* Reception d'un paquet venant du serveur
 * */
void ClientCore::receptionHyperviseur()
{
    // Recuperation socket
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket == 0) // Si par hasard on n'a pas trouvé, on arrête la méthode
        return;

    // Si tout va bien, on continue : on récupère le message
    QDataStream in(socket);

    if (tailleMessage == 0) // Si on ne connaît pas encore la taille du message, on essaie de la récupérer
    {
        if (socket->bytesAvailable() < (int)sizeof(quint16)) // On n'a pas reçu la taille du message en entier
             return;

        in >> tailleMessage; // Si on a reçu la taille du message en entier, on la récupère
    }

    // Si on connaît la taille du message, on vérifie si on a reçu le message en entier
    if (socket->bytesAvailable() < tailleMessage) // Si on n'a pas encore tout reçu, on arrête la méthode
        return;


    // Si on arrive jusqu'à cette ligne, on peut récupérer le message entier
    QString sender;
    Ordre ordre;
    QTime time;
    in >> sender;
    in >> time;
    in >> (int&) ordre;

    // 3 : remise de la taille du message à 0 pour permettre la réception des futurs messages
    tailleMessage = 0;

    switch( ordre )
    {
        case START:
            QTextStream(stdout) << time.toString() << " : START recu de l'hyperviseur." << endl;

            // Demarrer thread
            startSimu();
            break;

        case STOP:
            QTextStream(stdout) << time.toString() << " : STOP recu de l'hyperviseur." << endl;

            // Stopper le thread
            stopSimu();
            break;

        default:
            QTextStream(stdout) << "Ordre inconnu." << endl;
            break;
    }
}


/* Envoi d'un message a l'hyperviseur
 * */
void ClientCore::envoiHyperviseur()
{
    // Préparation du paquet
    QByteArray paquet;
    QDataStream out(&paquet, QIODevice::WriteOnly);

    out << (quint16) 0; // On écrit 0 au début du paquet pour réserver la place pour écrire la taille
    out << host.toString() + ":" + QString::number(portC) << QTime::currentTime() << QString("Coucou hyp!");
    out.device()->seek(0); // On se replace au début du paquet
    out << (quint16) (paquet.size() - sizeof(quint16)); // On écrase le 0 qu'on avait réservé par la longueur du message


    // Envoi du paquet préparé a l'hyperviseur
    toServer->write(paquet);
    QTextStream(stdout) << "Envoye a l'hyperviseur" << endl;
}


/* Reception d'un paquet venant d'un client
 * */
void ClientCore::receptionClient()
{
    // Recuperation socket
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket == 0) // Si par hasard on n'a pas trouvé, on arrête la méthode
        return;

    // Si tout va bien, on continue : on récupère le message
    QDataStream in(socket);

    if (tailleMessage == 0) // Si on ne connaît pas encore la taille du message, on essaie de la récupérer
    {
        if (socket->bytesAvailable() < (int)sizeof(quint16)) // On n'a pas reçu la taille du message en entier
             return;

        in >> tailleMessage; // Si on a reçu la taille du message en entier, on la récupère
    }

    // Si on connaît la taille du message, on vérifie si on a reçu le message en entier
    if (socket->bytesAvailable() < tailleMessage) // Si on n'a pas encore tout reçu, on arrête la méthode
        return;


    // Si on arrive jusqu'à cette ligne, on peut récupérer le message entier
    QString sender;
    Position p;
    in >> sender;
    in >> p.x;
    in >> p.y;
    in >> p.z;

    data.insert(sender, p);

    qDebug() << qSetRealNumberPrecision( 4 ) << "Position de " << sender << " recue - x:" << p.x << " y:"<< p.y << " z:"<< p.z;

    qDebug() << "Affichage data : ";
    QMapIterator<QString, Position> iter(data);
    while(iter.hasNext())
        {
            iter.next();
            qDebug() << qSetRealNumberPrecision( 4 ) << iter.key() << " - x:" << iter.value().x << " y:"<< iter.value().y << " z:"<< iter.value().z;
        }

    // 3 : remise de la taille du message à 0 pour permettre la réception des futurs messages
    tailleMessage = 0;
}


/* Envoi de la position aux autres clients
 * */
void ClientCore::envoiClient(Position p)
{
    // Préparation du paquet
    QByteArray paquet;
    QDataStream out(&paquet, QIODevice::WriteOnly);

    out << (quint16) 0; // On écrit 0 au début du paquet pour réserver la place pour écrire la taille
    out << hostPortSC << p.x << p.y << p.z;
    out.device()->seek(0); // On se replace au début du paquet
    out << (quint16) (paquet.size() - sizeof(quint16)); // On écrase le 0 qu'on avait réservé par la longueur du message


    // Envoi du paquet préparé à tous les autres clients
    for(QList<QTcpSocket *>::Iterator clientIterator = others.begin(); clientIterator != others.end(); ++clientIterator )
        (*clientIterator)->write(paquet);
}


/* Lancement d'une simulation
 * */
void ClientCore::startSimu()
{
    if (!simuThread)
    {
        simuThread = new QThread();
        simu = new Simu();

        // Initialisation tCompute
        tCompute = new QTimer();
        tCompute->setInterval(tComputeTimer);
        tCompute->start();
        connect(tCompute, SIGNAL(timeout()), simu, SLOT(compute()));

        simu->moveToThread(simuThread); // La simu s'executera dans le thread

        //connect(simu, SIGNAL(error(QString)), this, SLOT(errorString(QString)));
        //connect(simuThread, SIGNAL(started()), simu, SLOT(compute()));
        connect(simu, SIGNAL(finished()), simuThread, SLOT(quit()));
        connect(simu, SIGNAL(finished()), simu, SLOT(deleteLater()));
        connect(simuThread, SIGNAL(finished()), simuThread, SLOT(deleteLater()));

        qRegisterMetaType<Position>("Position");
        connect(simu, SIGNAL(positionChanged(Position)), this, SLOT(sendPosition(Position)));

        simuThread->start();

        state = true;

        //QMetaObject::invokeMethod( simu, "computeM");
    }
}


/* Arret d'une simulation
 * */
void ClientCore::stopSimu()
{
    if (simuThread)
    {
        if( simuThread->isRunning() )
            {
                simuThread->quit();
                simuThread->wait();
                simuThread->deleteLater();
                simuThread = NULL;

                state = false;
            }
    }
}


/* Envoi position
 * */
void ClientCore::sendPosition(Position p)
{
    //QTextStream(stdout) << "ClientCore::sendPosition " << "" << " get called from?: " << QThread::currentThreadId() << endl;

    // Rafraichissement position tableau interne
    data.insert(hostPortSC, p);

    // Envoi de la position aux autres
    envoiClient(p);

    //qDebug() << qSetRealNumberPrecision( 4 ) << hostPortSC << " - x:" << p.x << " y:"<< p.y << " z:"<< p.z << endl;

    // Affichage data
//    QMapIterator<QString, Position> iter(data);
//    while(iter.hasNext())
//        {
//            iter.next();
//            qDebug() << qSetRealNumberPrecision( 4 ) << iter.key() << " - x:" << iter.value().x << " y:"<< iter.value().y << " z:"<< iter.value().z << endl;
//        }
}

