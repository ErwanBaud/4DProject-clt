#ifndef HEADER_CLIENT
#define HEADER_CLIENT

#include <QtNetwork>
#include <QTextStream>
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
        STOP
    };

    public:
        ClientCore();


    private:
        QHostAddress host; // Adresse IP du client
        quint16 portS; // Port pour hyperviseur
        quint16 portC; // Port pour clients
        quint16 appPort; // Port sur lequel l'hyperviseur écoute

        bool state = false; // Le client calcule (true) ou pas (false)

        QUdpSocket *udpBroadSocket; // Socket d'emission broadcast iamAlive
        QTcpServer *fromClient; // Socket d'écoute des clients
        QTcpServer *fromServer; // Socket d'écoute de l'hyperviseur
        QTcpSocket *toServer; // Socket pour la communication vers l'hyperviseur
        QList<QTcpSocket *> others; // Sockets vers les autres clients

        quint16 tailleMessage;

        QTimer *tAlive; // Timer pour le broadcast iamAlive
        QTimer *tCompute; // Timer pour compute()

        QThread *simuThread; // Thread pour executer les objets Simu
        Simu *simu; // Objet effectuant les calculs

        void startSimu();
        void stopSimu();
        bool socketIsIn(QHostAddress host, quint16 port, QList<QTcpSocket *> &others);

    private slots:

        void iamAlive(); // Envoi un broadcast annonçant la présence du processus client
        void clientAlive(); // Reception d'un broadcast annonçant un processus client

        void connexionHyperviseur(); //Slot executé lors de la connexion de l'hyperviseur
        void receptionHyperviseur();
        void envoiHyperviseur();
        void deconnexionHyperviseur(); //Slot executé lors de la deconnexion de l'hyperviseur

        void connexionClient(); //Slot executé lors de la connexion d'un client
        void deconnexionClient(); //Slot executé lors de la deconnexion d'un client

        void sendX(double);
};

#endif
