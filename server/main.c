#include <stdio.h>
#include <stdlib.h> /* pour exit */
#include <unistd.h> /* pour close et sleep */
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>     /* pour memset */
#include <netinet/in.h> /* pour struct sockaddr_in */
#include <arpa/inet.h>  /* pour htons et inet_aton */

#define PORT IPPORT_USERRESERVED // = 5000

#define LG_MESSAGE 256

//#define hauteur = 40
//#define largeur = 80

typedef struct user{
    int SocketFD;
    char IP[20];
    int Port;
}user;

typedef struct pixel{
    int r;
    int g;
    int b;
}pixel;

int main()
{
    pixel tab[40][80];
    //pixel *tab = (pixel *)malloc(hauteur * largeur * sizeof(pixel));
    pixel pixel;
    for (int i = 0; i < 40; i++)
    {
        for (int j = 0; j < 80; j++)
        {
            tab[i][j] = pixel;
        }
    }
    
    int socketEcoute;
    struct sockaddr_in pointDeRencontreLocal;
    socklen_t longueurAdresse;
    int socketDialogue;
    struct sockaddr_in pointDeRencontreDistant;
    char messageEnvoi[LG_MESSAGE]; /* le message de la couche Application ! */
    char messageRecu[LG_MESSAGE];  /* le message de la couche Application ! */
    int ecrits, lus;               /* nb d’octets ecrits et lus */
    int retour;

    // Crée un socket de communication
    socketEcoute = socket(PF_INET, SOCK_STREAM, 0); /* 0 indique que l’on utilisera le
    protocole par défaut associé à SOCK_STREAM soit TCP */

    // Teste la valeur renvoyée par l’appel système socket()
    if (socketEcoute < 0) /* échec ? */
    {
        perror("socket"); // Affiche le message d’erreur
        exit(-1);         // On sort en indiquant un code erreur
    }

    printf("Socket créée avec succès ! (%d)\n", socketEcoute);

    // On prépare l’adresse d’attachement locale
    longueurAdresse = sizeof(struct sockaddr_in);
    memset(&pointDeRencontreLocal, 0x00, longueurAdresse);
    pointDeRencontreLocal.sin_family = PF_INET;
    pointDeRencontreLocal.sin_addr.s_addr = htonl(INADDR_ANY); // toutes les interfaces locales disponibles
    pointDeRencontreLocal.sin_port = htons(PORT);

    // On demande l’attachement local de la socket
    if ((bind(socketEcoute, (struct sockaddr *)&pointDeRencontreLocal, longueurAdresse)) < 0)
    {
        perror("bind");
        exit(-2);
    }

    printf("Socket attachée avec succès !\n");

    // On fixe la taille de la file d’attente à 5 (pour les demandes de connexion non encore traitées)
    if (listen(socketEcoute, 5) < 0)
    {
        perror("listen");
        exit(-3);
    }

    printf("Socket placée en écoute passive ...\n");

    //--- Début de l’étape n°7 :
    // boucle d’attente de connexion : en théorie, un serveur attend indéfiniment !
    while (1)
    {
        memset(messageEnvoi, 0x00, LG_MESSAGE * sizeof(char));
        memset(messageRecu, 0x00, LG_MESSAGE * sizeof(char));
        printf("Attente d’une demande de connexion (quitter avec Ctrl-C)\n\n");
        // c’est un appel bloquant
        socketDialogue = accept(socketEcoute, (struct sockaddr *)&pointDeRencontreDistant, &longueurAdresse);

        if (socketDialogue < 0)
        {
            perror("accept");
            close(socketDialogue);
            close(socketEcoute);
            exit(-4);
        }

        // On réceptionne les données du client (cf. protocole)
        lus = read(socketDialogue, messageRecu, LG_MESSAGE * sizeof(char)); // ici appel bloquant

        switch (lus)
        {
        case -1: /* une erreur ! */
            perror("read");
            close(socketDialogue);
            exit(-5);
        case 0: /* la socket est fermée */
            fprintf(stderr, "La socket a été fermée par le client !\n\n");
            close(socketDialogue);
            return 0;
        default: /* réception de n octets */
            printf("Message reçu : %s (%d octets) depuis %s:%d\n\n", messageRecu, lus, inet_ntoa(pointDeRencontreDistant.sin_addr), (int) ntohs(pointDeRencontreDistant.sin_port));
        }

        // On envoie des données vers le client (cf. protocole)
        sprintf(messageEnvoi, "ok\n");
        ecrits = write(socketDialogue, messageEnvoi, strlen(messageEnvoi));
        switch (ecrits)
        {
        case -1: /* une erreur ! */
            perror("write");
            close(socketDialogue);
            exit(-6);
        case 0: /* la socket est fermée */
            fprintf(stderr, "La socket a été fermée par le client !\n\n");
            close(socketDialogue);
            return 0;
        default: /* envoi de n octets */
            printf("Message %s envoyé (%d octets)\n\n", messageEnvoi, ecrits);
        }
        // On ferme la socket de dialogue et on se replace en attente ...
        close(socketDialogue);
    }
    //--- Fin de l’étape n°7 !

    // On ferme la ressource avant de quitter
    close(socketEcoute);
    
    return 0;
}
