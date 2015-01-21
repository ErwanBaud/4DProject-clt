#include <iostream>
#include "ClientCore.h"

#define tAliveTimer 2000

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
                ready = false;
                host = fromServer->serverAddress();
                portS = fromServer->serverPort();
                portC = fromClient->serverPort();

                QTextStream(stdout) << "I am alive on " + host.toString()<< " portS:" << portS << " portC:" << portC << endl;

                // Initialisation des composants utiles au broadcast iamAlive
                tAlive = new QTimer();
                tAlive->setInterval(tAliveTimer);
                connect(tAlive, SIGNAL(timeout()), this, SLOT(iamAlive()));
                tAlive->start();

                connect(fromServer, SIGNAL(newConnection()), this, SLOT(connexionHyperviseur()));
                connect(fromClient, SIGNAL(newConnection()), this, SLOT(connexionClient()));
}


/* Envoi du message iamAlive
 * du type host#portS#portC
 * exemple : "127.0.0.1#12345#67890"
 * */
void ClientCore::iamAlive()
{
    QByteArray datagram;
    datagram.append(host.toString());
    datagram.append("#");
    datagram.append(QByteArray::number(portS));
    datagram.append("#");
    datagram.append(QByteArray::number(portC));
    udpBroadSocket->writeDatagram(datagram.data(), datagram.size(), QHostAddress::Broadcast, appPort);
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
    QTextStream(stdout) << "Message recu !" << endl;

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
    QString sender, messageRecu;
    QTime time;
    in >> sender;
    in >> time;
    in >> messageRecu;

    QTextStream(stdout) << time.toString() << " - Recu de l'hyperviseur" << " : " << messageRecu << endl;

    // 3 : remise de la taille du message à 0 pour permettre la réception des futurs messages
    tailleMessage = 0;

    envoiHyperviseur();
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


/*
// Envoi d'un message au serveur
void ClientCore::envoyer(QString mess)
{
    QByteArray paquet;
    QDataStream out(&paquet, QIODevice::WriteOnly);

    out << (quint16) 0;
    out << hostPort << QTime::currentTime() << mess;
    out.device()->seek(0);
    out << (quint16) (paquet.size() - sizeof(quint16));

    socket->write(paquet); // On envoie le paquet
    socket->waitForBytesWritten();
}

// On a reçu un paquet (ou un sous-paquet)
void ClientCore::donneesRecues()
{
    // Même principe que lorsque le serveur reçoit un paquet :
    // On essaie de récupérer la taille du message
    // Une fois qu'on l'a, on attend d'avoir reçu le message entier (en se basant sur la taille annoncée tailleMessage)

    QDataStream in(socket);

    if (tailleMessage == 0)
    {
        if (socket->bytesAvailable() < (int)sizeof(quint16))
             return;

        in >> tailleMessage;
    }

    if (socket->bytesAvailable() < tailleMessage)
        return;


    // Si on arrive jusqu'à cette ligne, on peut récupérer le message entier
    QString hostPort, messageRecu;
    QTime time;
    int ms;

    in >> hostPort;
    in >> time;
    in >> messageRecu;

    ms = QTime::currentTime().msecsTo(time);

    // On affiche le message sur la zone de Chat
   cout << hostPort.toStdString() << " " << ms << " : " << messageRecu.toStdString() << endl;

    // On remet la taille du message à 0 pour pouvoir recevoir de futurs messages
    tailleMessage = 0;
}


*/

