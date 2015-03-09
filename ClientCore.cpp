#include <iostream>
#include "ClientCore.h"

#define tAliveTimer 1000
#define tComputeTimer 250
#define ITER 500

using namespace std;

/****** Constructeur    *****/

ClientCore::ClientCore()
{
    log = QString("");
    logStream.setString(&log);
    logStream << endl << QString("-------- Debut du log --------") << endl;
    appPort = 50885;
    QHostAddress *add;

    foreach (const QHostAddress &address, QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && address != QHostAddress(QHostAddress::LocalHost))
            add= new QHostAddress(address.toString());
    }

    // Demarrage du client
    //fromServer = new QTcpServer(this); // Socket d'écoute hyperviseur
    serverSSL = new SslServer(); // Socket d'écoute hyperviseur
    fromClient = new QTcpServer(this); // Socket d'écoute clients

    if (!serverSSL->listen(QHostAddress(add->toString()), 0)) // Démarrage de l'ecoute hyperviseur sur un port aleatoire
    // Si l'ecoute hyperviseur n'a pas été démarré correctement
    {
        QTextStream(stdout) << "L'ecoute hyperviseur n'a pas pu etre demarre. Raison : " + serverSSL->errorString() << endl;
        logStream << "L'ecoute hyperviseur n'a pas pu etre demarre. Raison : " + serverSSL->errorString() << endl;
    }

    else
    {
        if (!fromClient->listen(QHostAddress(add->toString()), 0)) // Démarrage de l'ecoute client sur un port aleatoire
            // Si l'ecoute client n'a pas été démarré correctement
        {
            QTextStream(stdout) << "L'ecoute client n'a pas pu etre demarre. Raison : " + fromClient->errorString() << endl;
            logStream << "L'ecoute client n'a pas pu etre demarre. Raison : " + fromClient->errorString() << endl;
        }
        else
        {
            // Initialisation des composants iamAlive
            udpBroadSocket = new QUdpSocket(this);
            if ( !(udpBroadSocket->bind(appPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) )
            {
                QTextStream(stdout) << "    Erreur socket UDP." << endl;
                logStream << "    Erreur socket UDP." << endl;
            }
            else
            {
                connect(udpBroadSocket, SIGNAL(readyRead()), this, SLOT(clientAlive()));
            }

                // Le client est démarré et toutes ses sockets sont opérationnelles

                // Renseignement des attributs
                tailleMessage = 0;
                simuThread = 0;
                state = false;
                host = serverSSL->serverAddress();
                portS = serverSSL->serverPort();
                portC = fromClient->serverPort();
                hostPortSC = host.toString() + ":" + QString::number(portS) + ":" + QString::number(portC);

                fromServerSSL = NULL;

                connect(serverSSL, SIGNAL(newConnection()), this, SLOT(connexionHyperviseur()));
                connect(fromClient, SIGNAL(newConnection()), this, SLOT(connexionClient()));


                // Initialisation des composants utiles au broadcast iamAlive
                tAlive = new QTimer();
                tAlive->setInterval(tAliveTimer);
                connect(tAlive, SIGNAL(timeout()), this, SLOT(iamAlive()));
                tAlive->start();

                // Chronometre
                tChrono = new QElapsedTimer();

                total = 0;
                t = 0;

                QTextStream(stdout) << "I am alive on " + host.toString()<< " portS:" << portS << " portC:" << portC << endl;
                QTextStream(stdout) << "From main thread: " << QThread::currentThreadId() << endl;
                logStream << "I am alive on " + host.toString()<< " portS:" << portS << " portC:" << portC << endl;
                logStream << "From main thread: " << QThread::currentThreadId() << endl;
        }
    }
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
    //logStream << "ClientCore::iamAlive " << datagram << " get called from?: " << QThread::currentThreadId() << endl;
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
         quint16 port1 = mess[1].toUShort();
         quint16 port2 = mess[2].toUShort();

         QString peerPort = peer.toString() + ":" + QString::number(port1) + ":" + QString::number(port2);

         if ( peerPort != hostPortSC )
         {
             if( !toOthers.contains(peerPort))
             {
                 QTcpSocket *socket = new QTcpSocket(this);

                 QTextStream(stdout) << "    Tentative de connexion a " << peerPort << endl;
                 logStream << "    Tentative de connexion a " << peerPort << endl;

                 socket->connectToHost(host, port2); // On se connecte au serveur demandé
                 socket->waitForConnected();

                 connect(socket, SIGNAL(readyRead()), this, SLOT(receptionACK()));
                 connect(socket, SIGNAL(disconnected()), this, SLOT(deconnexionClient()));

                 toOthers.insert(peerPort, socket);

                 QTextStream(stdout) << "    Connecte a " << peerPort << endl;
                 logStream << "    Connecte a " << peerPort << endl;
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
    fromServerSSL = serverSSL->nextPendingConnection();

    if ( fromServerSSL )
    {
        connect(fromServerSSL, SIGNAL(readyRead()), this, SLOT(receptionHyperviseur()));
        connect(fromServerSSL, SIGNAL(disconnected()), this, SLOT(deconnexionHyperviseur()));

        QString serverKey = QDir::home().filePath("4DProject/4DProject-cert/server/server-key.pem");
        QString serverCrt = QDir::home().filePath("4DProject/4DProject-cert/server/server-crt.pem");
        QString ca = QDir::home().filePath("4DProject/4DProject-cert/ca/ca.pem");

        //clef privée
        QFile file(serverKey);

        if(!file.open(QIODevice::ReadOnly))
        {
            qDebug() << "Erreur: Ouverture de la cle impossible (" << file.fileName() << ") : " << file.errorString();
            logStream << "Erreur: Ouverture de la cle impossible (" << file.fileName() << ") : " << file.errorString() << endl;
            return;
        }
        QSslKey key( &file, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey, "4DProject" );
        file.close();

        fromServerSSL->setPrivateKey(key);

        //certificat local
        fromServerSSL->setLocalCertificate(serverCrt);

        //ne pas vérifier l'identité
        fromServerSSL->setPeerVerifyMode(QSslSocket::VerifyNone);

        //certificat de la CA
        if(!fromServerSSL->addCaCertificates(ca))
        {
            qDebug() << "Erreur: Ouverture du certificat de CA impossible (" << ca << ")";
            logStream << "Erreur: Ouverture du certificat de CA impossible (" << ca << ")" << endl;
            return;
        }

       fromServerSSL->startServerEncryption();

       qDebug() << "L'hyperviseur vient de se connecter : "
                << fromServerSSL->peerAddress().toString() << ":" << fromServerSSL->peerPort();
       logStream << "L'hyperviseur vient de se connecter : "
                << fromServerSSL->peerAddress().toString() << ":" << fromServerSSL->peerPort() << endl;
    }
    else
    {
        qDebug() << "Probleme socket.";
        logStream << "Probleme socket." << endl;
    }
}


/* Deconnexion d'un processus client
 * */
void ClientCore::deconnexionHyperviseur()
{
    QTextStream(stdout) << "L'hyperviseur vient de se deconnecter." << endl;
    logStream << "L'hyperviseur vient de se deconnecter." << endl;
    fromServerSSL->deleteLater();
}



    /*---   communication   ---*/

/* Envoi d'un message a l'hyperviseur
 * */
void ClientCore::envoiHyperviseur(QString log)
{
    // Préparation du paquet
    QByteArray paquet;
    QDataStream out(&paquet, QIODevice::WriteOnly);

    out << (quint16) 0; // On écrit 0 au début du paquet pour réserver la place pour écrire la taille
    out << host.toString() + ":" + QString::number(portC) << QTime::currentTime() << log;
    out.device()->seek(0); // On se replace au début du paquet
    out << (quint16) (paquet.size() - sizeof(quint16)); // On écrase le 0 qu'on avait réservé par la longueur du message


    // Envoi du paquet préparé a l'hyperviseur
    fromServerSSL->write(paquet);
    fromServerSSL->waitForBytesWritten();
    QTextStream(stdout) << "Log envoye a l'hyperviseur." << endl;
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
            logStream << time.toString() << " : START recu de l'hyperviseur." << endl;

            // Demarrer thread
            startSimu();
            break;

        case STOP:
            QTextStream(stdout) << time.toString() << " : STOP recu de l'hyperviseur." << endl;
            logStream << time.toString() << " : STOP recu de l'hyperviseur." << endl;

            // Stopper le thread
            stopSimu();
            break;

        case EXIT:
            QTextStream(stdout) << time.toString() << " : EXIT recu de l'hyperviseur." << endl;
            logStream << time.toString() << " : EXIT recu de l'hyperviseur." << endl;

            // Stopper le thread
            stopSimu();

            doOnExit();
            exit(EXIT_SUCCESS);
            break;

        default:
            QTextStream(stdout) << "Ordre inconnu." << endl;
            logStream << "Ordre inconnu." << endl;
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
    logStream << peerPort << " vient de se connecter." << endl;

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
    logStream << peerPort << " vient de se deconnecter." << endl;

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
    //timeTX = QTime::currentTime();

    // Préparation du paquet
    QByteArray paquet;
    QDataStream out(&paquet, QIODevice::WriteOnly);

    out << (quint16) 0; // On écrit 0 au début du paquet pour réserver la place pour écrire la taille

    // ID pour ACK
    uint id = (uint)QThread::currentThreadId() * ((uint)p.x + (uint)p.y +(uint)p.z);

    // Nombre ACK à recevoir
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
    //qDebug() << "ACK recu :" << tChrono->nsecsElapsed() / 1000 << "us.";
    // Recuperation socket
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket == 0) // Si par hasard on n'a pas trouvé, on arrête la méthode
    {
        return;
    }

    // Si tout va bien, on continue : on récupère le message
    QDataStream in(socket);

    if (tailleMessage == 0) // Si on ne connaît pas encore la taille du message, on essaie de la récupérer
    {
        if (socket->bytesAvailable() < (int)sizeof(quint16)) // On n'a pas reçu la taille du message en entier
        {
            return;
        }

        in >> tailleMessage; // Si on a reçu la taille du message en entier, on la récupère
    }

    // Si on connaît la taille du message, on vérifie si on a reçu le message en entier
    if (socket->bytesAvailable() < tailleMessage) // Si on n'a pas encore tout reçu, on arrête la méthode
    {
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

    //qDebug() << "ClientCore::receptionClient TX is : " << timeTX.msecsTo(timeRX) << "ms." << endl;

    if( n == 0 )
    {
        total += tChrono->nsecsElapsed();
        ++t;
    }

    // Remise de la taille du message à 0 pour permettre la réception des futurs messages
    tailleMessage = 0;
}



    /*---   fermeture   ---*/

/* Actions lors de la fermeture du client
 * */
void ClientCore::doOnExit()
{
    logStream << endl << QString("-------- Fin du log --------") << endl;

    writeLog();
    envoiHyperviseur(log);
    //qDebug() << logStream.readAll();
}



/* Enregistrement du log dans un fichier
 * */
void ClientCore::writeLog()
{
    hostPortSC.replace(".", "_");
    hostPortSC.replace(":", "_");

    QString filename = QDir::home().filePath("4DProject/log/clt/" + hostPortSC + "_" + QDate::currentDate().toString(Qt::ISODate) + ".txt");
    QFile file( filename );
    if ( file.open(QIODevice::ReadWrite) )
    {
        QTextStream stream( &file );
        stream << log << endl;
        file.close();
    }

    logStream << endl << QString("*** Log saved to ") << filename << " ***" << endl;
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

        // On vide le QMap pour lancer plusieurs simu successives
        data.clear();


        total = 0;
        t = 0;

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

        qDebug() << "Simu demarree.";
        logStream << "Simu demarree." << endl;
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
                qDebug() << "Simu stoppee.";
                logStream << "Simu stoppee." << endl;
            }

        if( t )
        {
            qDebug() << endl << "ACK delay avg : " << (total/t)/1000 << "us.";
            logStream << endl << "ACK delay avg : " << (total/t)/1000 << "us." << endl;
        }

        // Affichage donnees
        displayData();
    }

}




/* Affiche toutes les positions stockées
 * */
void ClientCore::displayData()
{
    qDebug() << endl << "Data : ";
    logStream << endl << "Data : " << endl;

    QMapIterator<QString, Position> iter(data);
    while(iter.hasNext())
        {
            iter.next();
            qDebug() << qSetRealNumberPrecision( 3 ) << iter.key()
                     << "  x:" << iter.value().x << " y:"<< iter.value().y << " z:"<< iter.value().z;
            logStream << qSetRealNumberPrecision( 3 ) << iter.key()
                     << "  x:" << iter.value().x << " y:"<< iter.value().y << " z:"<< iter.value().z << endl;
        }

    qDebug();
    logStream << endl;
}




/* Envoi position
 * */
void ClientCore::sendPosition(Position p)
{
    //QTextStream(stdout) << "ClientCore::sendPosition " << "" << " get called from?: " << QThread::currentThreadId() << endl;
/*
    if( data.isEmpty() )
    {
        n = ITER * toOthers.size();
        qDebug() << "n =" << n << "tChrono.start()";
        tChrono->start();
    }
*/
    // Rafraichissement position tableau interne
    data.insert(hostPortSC, p);

    // Envoi de la position aux autres
    envoiData(p);
}
