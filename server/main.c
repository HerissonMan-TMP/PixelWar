#include <stdio.h>
#include <stdlib.h> /* pour exit */
#include <unistd.h> /* pour close et sleep */
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>     /* pour memset */
#include <netinet/in.h> /* pour struct sockaddr_in */
#include <arpa/inet.h>  /* pour htons et inet_aton */
#include <sys/poll.h>

#define PORT IPPORT_USERRESERVED // = 5000
#define NB_CLIENTS 10
#define LG_MESSAGE 256

#define VERSION 1

typedef struct Pixel {
    int r;
    int g;
    int b;
} Pixel;

typedef struct User {
    int socketFD;
} User;

void addUser(User users[NB_CLIENTS], User u);
void removeUser(User users[NB_CLIENTS], int socketFD);
void analyse(int socketDialogue, char* messageRecu, int largeur, int hauteur, int limite);

int main(int argc, char* argv[])
{
    int port = 8080;
    int surface[2] = {640, 480};
    int largeur = 640;
    int hauteur = 480;
    int rate_limit = 100;

    int option;
    while ((option = getopt(argc, argv, "p:s:l:")) != -1) {
        switch (option) {
            case 'p':
                port = atoi(optarg);
                break;
            case 's':
                sscanf(optarg, "%dx%d", &largeur, &hauteur);
                break;
            case 'l':
                rate_limit = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [-p PORT] [-s LxH] [-l RATE_LIMIT]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    printf("Port: %d\n", port);
    printf("Surface: %dx%d\n", largeur, hauteur);
    printf("Rate limit: %d\n", rate_limit);

    Pixel tab[640][480];
    //pixel *tab = (pixel *)malloc(hauteur * largeur * sizeof(pixel));
    Pixel pixel;
    for (int i = 0; i < 480; i++)
    {
        for (int j = 0; j < 640; j++)
        {
            pixel.r = 0;
            pixel.g = 0;
            pixel.b = 0;
            tab[i][j] = pixel;
        }
    }

    User users[NB_CLIENTS];

    for (int i = 0; i < NB_CLIENTS; i++) {
        User user;
        user.socketFD = 0;
        users[i] = user;
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
    pointDeRencontreLocal.sin_port = htons(port);

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


    //type revent, event
    //liste chainee pour users et pollfd
    //remove après close?

    while (1)
    {
        memset(messageEnvoi, 0x00, LG_MESSAGE * sizeof(char));
        memset(messageRecu, 0x00, LG_MESSAGE * sizeof(char));

        //malloc n+1 * fds

        struct pollfd pollFDs[NB_CLIENTS+1];
        pollFDs[0].fd = socketEcoute;
        pollFDs[0].events = POLLIN;

        int n = 1;

        for (int i = 0; i < NB_CLIENTS; i++) {
            if (users[i].socketFD != 0) {
                pollFDs[n].fd = users[i].socketFD;
                pollFDs[n].events = POLLIN;
                n++;
            }
        }

        poll(pollFDs, n, 0);

        for (int i = 0; i < n; i++) {
            if (!pollFDs[i].revents) {
                continue;
            }

            if (i == 0) {
                //Ajout de l'utilisateur
                socketDialogue = accept(socketEcoute, (struct sockaddr *)&pointDeRencontreDistant, &longueurAdresse);
                User u;
                u.socketFD = socketDialogue;
                addUser(users, u);
            } else {
                //Lecture de la socket
                lus = read(pollFDs[i].fd, messageRecu, LG_MESSAGE * sizeof(char));

                switch (lus)
                {
                case -1: /* une erreur ! */
                    perror("read");
                    close(pollFDs[i].fd);
                    removeUser(users, pollFDs[i].fd);
                    exit(-5);
                case 0: /* la socket est fermée */
                    fprintf(stderr, "La socket a été fermée par le client !\n\n");
                    close(pollFDs[i].fd);
                    removeUser(users, pollFDs[i].fd);
                    return 0;
                default: /* réception de n octets */
                    printf("Message reçu : %s (%d octets) depuis %s:%d\n\n", messageRecu, lus, inet_ntoa(pointDeRencontreDistant.sin_addr), (int) ntohs(pointDeRencontreDistant.sin_port));
                
                    analyse(pollFDs[i].fd, messageRecu, largeur, hauteur, rate_limit);

                    memset(messageRecu, 0x00, LG_MESSAGE * sizeof(char));
                }
            }
        }
    }


    // //--- Début de l’étape n°7 :
    // // boucle d’attente de connexion : en théorie, un serveur attend indéfiniment !
    // while (1)
    // {
    //     memset(messageEnvoi, 0x00, LG_MESSAGE * sizeof(char));
    //     memset(messageRecu, 0x00, LG_MESSAGE * sizeof(char));
    //     printf("Attente d’une demande de connexion (quitter avec Ctrl-C)\n\n");
    //     // c’est un appel bloquant
    //     socketDialogue = accept(socketEcoute, (struct sockaddr *)&pointDeRencontreDistant, &longueurAdresse);

    //     if (socketDialogue < 0)
    //     {
    //         perror("accept");
    //         close(socketDialogue);
    //         close(socketEcoute);
    //         exit(-4);
    //     }

    //     // On réceptionne les données du client (cf. protocole)
    //     lus = read(socketDialogue, messageRecu, LG_MESSAGE * sizeof(char)); // ici appel bloquant

    //     switch (lus)
    //     {
    //     case -1: /* une erreur ! */
    //         perror("read");
    //         close(socketDialogue);
    //         exit(-5);
    //     case 0: /* la socket est fermée */
    //         fprintf(stderr, "La socket a été fermée par le client !\n\n");
    //         close(socketDialogue);
    //         return 0;
    //     default: /* réception de n octets */
    //         printf("Message reçu : %s (%d octets) depuis %s:%d\n\n", messageRecu, lus, inet_ntoa(pointDeRencontreDistant.sin_addr), (int) ntohs(pointDeRencontreDistant.sin_port));
    //     }

    //     // On envoie des données vers le client (cf. protocole)
    //     sprintf(messageEnvoi, "ok\n");
    //     ecrits = write(socketDialogue, messageEnvoi, strlen(messageEnvoi));
    //     switch (ecrits)
    //     {
    //     case -1: /* une erreur ! */
    //         perror("write");
    //         close(socketDialogue);
    //         exit(-6);
    //     case 0: /* la socket est fermée */
    //         fprintf(stderr, "La socket a été fermée par le client !\n\n");
    //         close(socketDialogue);
    //         return 0;
    //     default: /* envoi de n octets */
    //         printf("Message %s envoyé (%d octets)\n\n", messageEnvoi, ecrits);
    //     }
    //     // On ferme la socket de dialogue et on se replace en attente ...
    //     close(socketDialogue);
    // }
    // //--- Fin de l’étape n°7 !

    // On ferme la ressource avant de quitter
    close(socketEcoute);
    
    return 0;
}



void addUser(User users[NB_CLIENTS], User u) {
    for (int i = 0; i < NB_CLIENTS; i++) {
        if (users[i].socketFD == 0) {
            users[i].socketFD = u.socketFD;
        }
    }
}

void removeUser(User users[NB_CLIENTS], int socketFD) {
    int found = 0;
    for (int i = 0; i < NB_CLIENTS; i++) {
        if (users[i].socketFD == socketFD) {
            found = 1;
        }

        if (found) {
            if (i < 9) {
                users[i] = users[i+1];
            } else {
                users[i].socketFD = 0;
            }
        }
    }
}

void analyse(int socketDialogue, char* messageRecu, int largeur, int hauteur, int limite) {
    int ecrits;

    if (!strncmp(messageRecu, "/getMatrix", 10)) {
        char messageEnvoi[LG_MESSAGE];
        memset(messageEnvoi, 0x00, LG_MESSAGE * sizeof(char));
        sprintf(messageEnvoi, "matrice\n");
        ecrits = write(socketDialogue, messageEnvoi, strlen(messageEnvoi));
        switch (ecrits) {
        case -1: /* une erreur ! */
            perror("write");
            close(socketDialogue);
            exit(-3);
        case 0: /* la socket est fermée */
            fprintf(stderr, "La socket a été fermée par le serveur !\n\n");
            close(socketDialogue);
            return;
        default: /* envoi de n octets */
            printf("Message %s envoyé avec succès (%d octets)\n\n", messageEnvoi, ecrits);
        }
    } else if (!strncmp(messageRecu, "/getSize", 8)) {
        char messageEnvoi[LG_MESSAGE];
        memset(messageEnvoi, 0x00, LG_MESSAGE * sizeof(char));
        char msg[15];
        char *lengthMatrixStr = malloc(7 * sizeof(char));
        char *heightMatrixStr = malloc(7 * sizeof(char));
        sprintf(lengthMatrixStr, "%d", largeur);
        sprintf(heightMatrixStr, "%d", hauteur);
        strcat(msg, lengthMatrixStr);
        strcat(msg, "x");
        strcat(msg, heightMatrixStr);
        sprintf(messageEnvoi, msg);
        sprintf(msg, "");
        ecrits = write(socketDialogue, messageEnvoi, strlen(messageEnvoi));
        switch (ecrits) {
        case -1: /* une erreur ! */
            perror("write");
            close(socketDialogue);
            exit(-3);
        case 0: /* la socket est fermée */
            fprintf(stderr, "La socket a été fermée par le serveur !\n\n");
            close(socketDialogue);
            return;
        default: /* envoi de n octets */
            printf("Message %s envoyé avec succès (%d octets)\n\n", messageEnvoi, ecrits);
        }
    } else if (!strncmp(messageRecu, "/getLimits", 10)) {
        char messageEnvoi[LG_MESSAGE];
        memset(messageEnvoi, 0x00, LG_MESSAGE * sizeof(char));
        char *limitPerMinStr = malloc(7 * sizeof(char));
        sprintf(limitPerMinStr, "%d", limite);
        sprintf(messageEnvoi, limitPerMinStr);
        ecrits = write(socketDialogue, messageEnvoi, strlen(messageEnvoi));
        switch (ecrits) {
        case -1: /* une erreur ! */
            perror("write");
            close(socketDialogue);
            exit(-3);
        case 0: /* la socket est fermée */
            fprintf(stderr, "La socket a été fermée par le serveur !\n\n");
            close(socketDialogue);
            return;
        default: /* envoi de n octets */
            printf("Message %s envoyé avec succès (%d octets)\n\n", messageEnvoi, ecrits);
        }
    } else if (!strncmp(messageRecu, "/getVersion", 11)) {
        char messageEnvoi[LG_MESSAGE];
        memset(messageEnvoi, 0x00, LG_MESSAGE * sizeof(char));
        char *versionStr = malloc(7 * sizeof(char));
        sprintf(versionStr, "%d", VERSION);
        sprintf(messageEnvoi, versionStr);
        ecrits = write(socketDialogue, messageEnvoi, strlen(messageEnvoi));
        switch (ecrits) {
        case -1: /* une erreur ! */
            perror("write");
            close(socketDialogue);
            exit(-3);
        case 0: /* la socket est fermée */
            fprintf(stderr, "La socket a été fermée par le serveur !\n\n");
            close(socketDialogue);
            return;
        default: /* envoi de n octets */
            printf("Message %s envoyé avec succès (%d octets)\n\n", messageEnvoi, ecrits);
        }
    } else if (!strncmp(messageRecu, "/getWaitTime", 12)) {
        char messageEnvoi[LG_MESSAGE];
        memset(messageEnvoi, 0x00, LG_MESSAGE * sizeof(char));
        char *waitTimeStr = malloc(7 * sizeof(char));
        sprintf(waitTimeStr, "%d", 0);
        sprintf(messageEnvoi, waitTimeStr);
        ecrits = write(socketDialogue, messageEnvoi, strlen(messageEnvoi));
        switch (ecrits) {
        case -1: /* une erreur ! */
            perror("write");
            close(socketDialogue);
            exit(-3);
        case 0: /* la socket est fermée */
            fprintf(stderr, "La socket a été fermée par le serveur !\n\n");
            close(socketDialogue);
            return;
        default: /* envoi de n octets */
            printf("Message %s envoyé avec succès (%d octets)\n\n", messageEnvoi, ecrits);
        }
    }
}