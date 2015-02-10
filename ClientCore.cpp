#include <iostream>
#include "ClientCore.h"

#define tAliveTimer 1000
#define tComputeTimer 4000

using namespace std;

/****** Constructeur    *****/
ClientCore::ClientCore()
{
    appPort = 50885;

    // Demarrage du client
    //fromServer = new QTcpServer(this); // Socket d'écoute hyperviseur
    fromServerSSL = new SslServer(); // Socket d'écoute hyperviseur
    fromClient = new QTcpServer(this); // Socket d'écoute clients

    if (!fromServerSSL->listen(QHostAddress("127.0.0.1"), 0)) // Démarrage de l'ecoute hyperviseur sur un port aleatoire
    // Si l'ecoute hyperviseur n'a pas été démarré correctement
        QTextStream(stdout) << "L'ecoute hyperviseur n'a pas pu etre demarre. Raison : " + fromServerSSL->errorString() << endl;

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
                simuThread = 0;
                state = false;
                host = fromServerSSL->serverAddress();
                portS = fromServerSSL->serverPort();
                portC = fromClient->serverPort();
                hostPortSC = host.toString() + ":" + QString::number(portS) + ":" + QString::number(portC);

                toServerSSL = NULL;

                connect(fromServerSSL, SIGNAL(newConnection()), this, SLOT(connexionHyperviseur()));
                connect(fromClient, SIGNAL(newConnection()), this, SLOT(connexionClient()));


                // Initialisation des composants utiles au broadcast iamAlive
                tAlive = new QTimer();
                tAlive->setInterval(tAliveTimer);
                connect(tAlive, SIGNAL(timeout()), this, SLOT(iamAlive()));
                tAlive->start();

                // Chronometre
                tChrono = new QTime();
                tChrono->start();


                QTextStream(stdout) << "I am alive on " + host.toString()<< " portS:" << portS << " portC:" << portC << endl;
                QTextStream(stdout) << "From main thread: " << QThread::currentThreadId() << endl;
}




/****** Broadcast   *****/


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

         QHostAddress peer = QHostAddress(QString(mess[0]));
         quint16 port = mess[2].toUShort();

         QString peerPort = peer.toString() + ":" + QString::number(port);

         if ( port != portC )
         {
             if( !toOthers.contains(peerPort))
             {
                 QTcpSocket *socket = new QTcpSocket(this);

                 QTextStream(stdout) << "    Tentative de connexion a " << peerPort << endl;

                 socket->connectToHost(host, port); // On se connecte au serveur demandé
                 socket->waitForConnected();

                 connect(socket, SIGNAL(readyRead()), this, SLOT(receptionACK()));
                 connect(socket, SIGNAL(disconnected()), this, SLOT(deconnexionClient()));

                 toOthers.insert(peerPort, socket);

                 QTextStream(stdout) << "    Connecte a " << peerPort << endl;
             }
         }
     }
}





/******   HYPERVISEUR   *****/


    /*---   connexion   ---*/

/* Connexion de l'hyperviseur
 * */
void ClientCore::connexionHyperviseur()
{
    qDebug() << "connexionHyperviseur";

    toServerSSL = fromServerSSL->nextPendingConnection();

    if ( toServerSSL )
    {
        connect(toServerSSL, SIGNAL(readyRead()), this, SLOT(receptionHyperviseur()));
        connect(toServerSSL, SIGNAL(disconnected()), this, SLOT(deconnexionHyperviseur()));

        QString serverKey = QDir::home().filePath("4DProject/4DProject-cert/server/server-key.pem");
        QString serverCrt = QDir::home().filePath("4DProject/4DProject-cert/server/server-crt.pem");
        QString ca = QDir::home().filePath("4DProject/4DProject-cert/ca/ca.pem");

        //clef privée
        QFile file(serverKey);

        if(!file.open(QIODevice::ReadOnly))
        {
            qDebug() << "Erreur: Ouverture de la cle impossible (" << file.fileName() << ") : " << file.errorString();
            return;
        }
        QSslKey key( &file, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey, "4DProject" );
        file.close();

        toServerSSL->setPrivateKey(key);

        //certificat local
        toServerSSL->setLocalCertificate(serverCrt);

        //ne pas vérifier l'identité
        toServerSSL->setPeerVerifyMode(QSslSocket::VerifyNone);

        //certificat de la CA
        if(!toServerSSL->addCaCertificates(ca))
        {
             qDebug() << "Erreur: Ouverture du certificat de CA impossible (" << ca << ")";
            return;
        }

       toServerSSL->startServerEncryption();

       qDebug() << "L'hyperviseur vient de se connecter : "
                << toServerSSL->peerAddress().toString() << ":" << toServerSSL->peerPort();
    }
    else
        qDebug() << "Probleme socket.";
}


/* Deconnexion d'un processus client
 * */
void ClientCore::deconnexionHyperviseur()
{
    QTextStream(stdout) << "L'hyperviseur vient de se deconnecter." << endl;
    toServerSSL->deleteLater();
}



    /*---   communication   ---*/

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
    toServerSSL->write(paquet);
    QTextStream(stdout) << "Envoye a l'hyperviseur" << endl;
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




/******   CLIENTS   ******/


    /*---   connexion   ---*/

/* Connexion des clients
 * */
void ClientCore::connexionClient()
{
    QTcpSocket *nouveauClient = fromClient->nextPendingConnection();

    QHostAddress peer = nouveauClient->peerAddress();
    quint16 port = nouveauClient->peerPort();
    QString peerPort = peer.toString() + ":" + QString::number(port);

    QTextStream(stdout) << peerPort << " vient de se connecter." << endl;

    if( !fromOthers.contains(peerPort) )
    {
        connect(nouveauClient, SIGNAL(readyRead()), this, SLOT(receptionData()));
        connect(nouveauClient, SIGNAL(disconnected()), this, SLOT(deconnexionClient()));
        fromOthers.insert(peerPort, nouveauClient);
    }
}


/* Deconnexion d'un processus client
 * */
void ClientCore::deconnexionClient()
{
    // On détermine quel client se déconnecte
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket == 0) // Si par hasard on n'a pas trouvé le client à l'origine du signal, on arrête la méthode
        return;

    QHostAddress peer = socket->peerAddress();
    quint16 port = socket->peerPort();
    QString peerPort = peer.toString() + ":" + QString::number(port);

    QTextStream(stdout) << peerPort << " vient de se deconnecter." << endl;
    toOthers.remove(peerPort);
    fromOthers.remove(peerPort);
    socket->deleteLater();
}



    /*---   communication   ---*/

/* Envoi de la position aux autres clients
 * */
void ClientCore::envoiData(Position p)
{
    // Initialisation tChrono
    tChrono->restart();
    timeTX = QTime::currentTime();
    //qDebug() << "Envoi :" << tChrono->elapsed() << "ms.";

    // Préparation du paquet
    QByteArray paquet;
    QDataStream out(&paquet, QIODevice::WriteOnly);

    out << (quint16) 0; // On écrit 0 au début du paquet pour réserver la place pour écrire la taille

    // ID pour ACK
    uint id = (uint)QThread::currentThreadId() * ((uint)p.x + (uint)p.y +(uint)p.z);
    n = toOthers.size();

    out << hostPortSC << id << p.x << p.y << p.z;
    out.device()->seek(0); // On se replace au début du paquet
    out << (quint16) (paquet.size() - sizeof(quint16)); // On écrase le 0 qu'on avait réservé par la longueur du message


    // Envoi du paquet préparé à tous les autres clients
    for(QMap<QString, QTcpSocket *>::Iterator clientIterator = toOthers.begin(); clientIterator != toOthers.end(); ++clientIterator )
    {
        (*clientIterator)->write(paquet);
//        qDebug() << qSetRealNumberPrecision( 3 ) << hostPortSC << (*clientIterator)
//                 << "Envoye" << id << " x:" << p.x << " y:"<< p.y << " z:"<< p.z;
    }

    //qDebug() << "Fin Envoi :" << tChrono->elapsed() << "ms.";
}


/* Reception d'un paquet venant d'un client
 * */
void ClientCore::receptionData()
{
    // Recuperation socket
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket == 0) // Si par hasard on n'a pas trouvé, on arrête la méthode
    {
        qDebug() << "Pas trouve";
        return;
    }

    // Si tout va bien, on continue : on récupère le message
    QDataStream in(socket);

    if (tailleMessage == 0) // Si on ne connaît pas encore la taille du message, on essaie de la récupérer
    {
        if (socket->bytesAvailable() < (int)sizeof(quint16)) // On n'a pas reçu la taille du message en entier
        {
            qDebug() << "Taille pas entier";
            return;
        }

        in >> tailleMessage; // Si on a reçu la taille du message en entier, on la récupère
    }

    // Si on connaît la taille du message, on vérifie si on a reçu le message en entier
    if (socket->bytesAvailable() < tailleMessage) // Si on n'a pas encore tout reçu, on arrête la méthode
    {
        qDebug() << "Msg pas entier";
        return;
    }

    // Si on arrive jusqu'à cette ligne, on peut récupérer le message entier
    QString sender;
    Position p;
    uint id;
    in >> sender;
    in >> id;
    in >> p.x;
    in >> p.y;
    in >> p.z;

//    qDebug() << qSetRealNumberPrecision( 3 ) << sender << socket << "Recu"
//             << id << " x:" << p.x << " y:" << p.y << " z:" << p.z;


    envoiACK(socket, id);

    data.insert(sender, p);

    // Remise de la taille du message à 0 pour permettre la réception des futurs messages
    tailleMessage = 0;
}


/* Envoi ACK
 * */
void ClientCore::envoiACK(QTcpSocket *socket, uint id)
{
    QByteArray ack;
    QDataStream out(&ack, QIODevice::WriteOnly);

    out << (quint16) 0; // On écrit 0 au début du paquet pour réserver la place pour écrire la taille

    out << hostPortSC << QString("ACK") << id << QTime::currentTime();
    out.device()->seek(0); // On se replace au début du paquet
    out << (quint16) (ack.size() - sizeof(quint16)); // On écrase le 0 qu'on avait réservé par la longueur du message

    // Envoi de l'ACK
    socket->write(ack);
    //qDebug() << hostPortSC << socket << "Envoye" << QString("ACK ") << id;
}


/* Reception d'un ACK venant d'un client
 * */
void ClientCore::receptionACK()
{
    //qDebug() << "ACK recu :" << tChrono->elapsed() << "ms.";
    // Recuperation socket
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket == 0) // Si par hasard on n'a pas trouvé, on arrête la méthode
    {
        qDebug() << "Pas trouve";
        return;
    }

    // Si tout va bien, on continue : on récupère le message
    QDataStream in(socket);

    if (tailleMessage == 0) // Si on ne connaît pas encore la taille du message, on essaie de la récupérer
    {
        if (socket->bytesAvailable() < (int)sizeof(quint16)) // On n'a pas reçu la taille du message en entier
        {
            qDebug() << "Taille pas entier";
            return;
        }

        in >> tailleMessage; // Si on a reçu la taille du message en entier, on la récupère
    }

    // Si on connaît la taille du message, on vérifie si on a reçu le message en entier
    if (socket->bytesAvailable() < tailleMessage) // Si on n'a pas encore tout reçu, on arrête la méthode
    {
        qDebug() << "Msg pas entier";
        return;
    }

    // Si on arrive jusqu'à cette ligne, on peut récupérer le message entier
    QString sender, ack;
    uint id;
    QTime timeRX;
    in >> sender;
    in >> ack;
    in >> id;
    in >> timeRX;


    --n;

    qDebug() << "ClientCore::receptionClient TX is : " << timeTX.msecsTo(timeRX) << "ms." << endl;
    //qDebug() << sender << socket << "Recu" << ack << id << tChrono->elapsed() << "ms.";

    if( n == 0 )
    {
        //time = tChrono->elapsed();
        //qDebug() << "ClientCore::receptionClient all ACK delay is : " << time << "ms." << endl;
    }

    // Remise de la taille du message à 0 pour permettre la réception des futurs messages
    tailleMessage = 0;
}





/******   SIMULATION   ******/


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

    // Affichage donnees
    displayData();

}




/* Affiche toutes les positions stockées
 * */
void ClientCore::displayData()
{
    qDebug();
    qDebug() << "Data : ";
    QMapIterator<QString, Position> iter(data);
    while(iter.hasNext())
        {
            iter.next();
            qDebug() << qSetRealNumberPrecision( 3 ) << iter.key() << "  x:" << iter.value().x << " y:"<< iter.value().y << " z:"<< iter.value().z;
        }

    qDebug();
}




/* Envoi position
 * */
void ClientCore::sendPosition(Position p)
{
    //QTextStream(stdout) << "ClientCore::sendPosition " << "" << " get called from?: " << QThread::currentThreadId() << endl;

    // Rafraichissement position tableau interne
    data.insert(hostPortSC, p);

    // Envoi de la position aux autres
    envoiData(p);
}
