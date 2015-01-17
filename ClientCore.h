#ifndef HEADER_CLIENT
#define HEADER_CLIENT

#include <QtNetwork>
#include <QTextStream>

/* ClientCore : Processus client
 * */
class ClientCore : public QObject
{
    Q_OBJECT

    public:
        ClientCore();
        bool ready = false; // Le client est il pret ?

        bool socketIsIn(QHostAddress host, quint16 port, QList<QTcpSocket *> &others);

    private:
        QHostAddress host; // Adresse IP du client
        quint16 portS; // Port pour hyperviseur
        quint16 portC; // Port pour clients
        quint16 appPort; // Port sur lequel l'hyperviseur écoute

        QUdpSocket *udpBroadSocket; // Socket d'emission broadcast iamAlive
        QTcpServer *fromClient; // Socket d'écoute des clients
        QTcpServer *fromServer; // Socket d'écoute de l'hyperviseur
        QTcpSocket *toServer; // Socket pour la communication vers l'hyperviseur
        QList<QTcpSocket *> others; // Sockets vers les autres clients

        quint16 tailleMessage;

        QTimer *tAlive; // Timer pour le broadcast iamAlive

    public slots:
        void clientAlive();
        //void initConnexion(QString serveurIP, int serveurPort);
        //void envoyer(QString mess);

    private slots:
        //void donneesRecues();
        //void connecte();
        //void deconnecte();

        void connexionHyperviseur(); //Slot executé lors de la connexion de l'hyperviseur
        void deconnexionHyperviseur(); //Slot executé lors de la deconnexion de l'hyperviseur
        void connexionClient(); //Slot executé lors de la connexion d'un client
        void deconnexionClient(); //Slot executé lors de la deconnexion d'un client
        void iamAlive(); // Envoi un broadcast annonçant la présence du processus client
};

#endif
