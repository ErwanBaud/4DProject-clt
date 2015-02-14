#ifndef HEADER_CLIENT
#define HEADER_CLIENT

#include <QtNetwork>
#include <QTextStream>
#include <QTime>
#include "SslServer.h"
#include "Simu.h"

class Simu;

/* ClientCore : Processus client
 * */
class ClientCore : public QObject
{
    Q_OBJECT

    enum Ordre
    {
        START = 0,
        STOP,
        EXIT
    };

    public:
        ClientCore();
        QString hostPortSC; // Identité du client


    private:
        QHostAddress host; // Adresse IP du client
        quint16 portS; // Port pour hyperviseur
        quint16 portC; // Port pour clients
        quint16 appPort; // Port sur lequel l'hyperviseur écoute

        bool state = false; // Le client calcule (true) ou pas (false)

        QUdpSocket *udpBroadSocket; // Socket d'emission broadcast iamAlive
        QTcpServer *fromClient; // Socket d'écoute des clients

        SslServer *fromServerSSL; // "Module" d'ecoute SSL
        QSslSocket *toServerSSL; // Socket d'écoute de l'hyperviseur

        QMap<QString, QTcpSocket *> toOthers; // Sockets vers les autres clients
        QMap<QString, QTcpSocket *> fromOthers; // Sockets vers les autres clients

        QMap<QString, Position> data;

        quint16 tailleMessage;

        QTimer *tAlive; // Timer pour le broadcast iamAlive
        QTimer *tCompute; // Timer pour compute()

        QElapsedTimer *tChrono; // Time pour chronometrer
        quint64 total;
        quint32 t;

        int time;
        QTime timeTX;
        QTime timeTot;
        quint32 n;

        QThread *simuThread; // Thread pour executer les objets Simu
        Simu *simu; // Objet effectuant les calculs

        void startSimu();
        void stopSimu();

        QString log;
        QTextStream logStream;
        void doOnExit();
        void writeLog();


    private slots:

        void iamAlive(); // Envoi un broadcast annonçant la présence du processus client
        void clientAlive(); // Reception d'un broadcast annonçant un processus client


        void connexionHyperviseur(); //Slot executé lors de la connexion de l'hyperviseur
        void deconnexionHyperviseur(); //Slot executé lors de la deconnexion de l'hyperviseur

        void envoiHyperviseur(QString log);
        void receptionHyperviseur();


        void connexionClient(); //Slot executé lors de la connexion d'un client
        void deconnexionClient(); //Slot executé lors de la deconnexion d'un client

        void envoiData(Position p);
        void receptionData();

        void envoiACK(QTcpSocket *socket, uint id);
        void receptionACK();

        void displayData(); // Affiche toutes les positions stockées
        void sendPosition(Position);
};

#endif
