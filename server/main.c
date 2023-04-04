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
#define LG_MESSAGE 20000

#define VERSION 1

#ifndef BASE64_H
#define BASE64_H

void base64_decode(char *src, char dest[256]);

#endif /* BASE64_H */

typedef struct Pixel {
    char *b64;
} Pixel;

typedef struct User {
    int socketFD;
} User;

void addUser(User users[NB_CLIENTS], User u);
void removeUser(User users[NB_CLIENTS], int socketFD);
void analyse(int socketDialogue, char* messageRecu, int largeur, int hauteur, int limite, Pixel tab[][largeur]);
static unsigned int raw_base64_decode(unsigned char *in, unsigned char *out, int strict, int *err);
unsigned char *spc_base64_decode(unsigned char *buf, size_t *len, int strict, int *err);

int main(int argc, char* argv[])
{
    int port = 5000;
    int largeur = 80;
    int hauteur = 40;
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

    Pixel tab[40][80];

    // Modification des éléments dans la matrice
    for (int i = 0; i < hauteur; i++) {
        for (int j = 0; j < largeur; j++) {
            tab[i][j].b64 = "////";
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
    int lus;               /* nb d’octets ecrits et lus */

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
                    //printf("Message reçu : %s (%d octets) depuis %s:%d\n\n", messageRecu, lus, inet_ntoa(pointDeRencontreDistant.sin_addr), (int) ntohs(pointDeRencontreDistant.sin_port));
                    printf("%s\n\n", messageRecu); 
                    analyse(pollFDs[i].fd, messageRecu, 80, 40, rate_limit, tab);

                    memset(messageRecu, 0x00, LG_MESSAGE * sizeof(char));
                }
            }
        }
    }

    // On ferme la ressource avant de quitter
    close(socketEcoute);

    for (int i = 0; i < hauteur; i++) {
        free(tab[i]);
    }
    free(tab);
    
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

void analyse(int socketDialogue, char* messageRecu, int largeur, int hauteur, int limite, Pixel tab[][largeur]) {
    static const unsigned char base64_table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    int ecrits;

    if (!strncmp(messageRecu, "/getMatrix", 10)) {
        char messageEnvoi[LG_MESSAGE];
        memset(messageEnvoi, 0x00, LG_MESSAGE * sizeof(char));
        //sprintf(messageEnvoi, "");
        // Affichage des éléments de la matrice
        char *matrixStr = malloc(20000 * sizeof(char));
        for (int i = 0; i < hauteur; i++) {
            for (int j = 0; j < largeur; j++) {
                //printf("%s ", tab[i][j].b64);
                strncat(matrixStr, tab[i][j].b64, 4);
            }
            //printf("\n");
        }
        sprintf(messageEnvoi, matrixStr);
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
        char *sizeStr = malloc(20000 * sizeof(char));
        char *lengthMatrixStr = malloc(7 * sizeof(char));
        char *heightMatrixStr = malloc(7 * sizeof(char));
        sprintf(lengthMatrixStr, "%d", largeur);
        sprintf(heightMatrixStr, "%d", hauteur);
        strcat(sizeStr, lengthMatrixStr);
        strcat(sizeStr, "x");
        strcat(sizeStr, heightMatrixStr);
        sprintf(messageEnvoi, sizeStr);
        sprintf(sizeStr, "");
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
    } else if (!strncmp(messageRecu, "/setPixel", 9)) {
        char *pch;
        char *saveptr;
        pch = strtok_r(messageRecu," ",&saveptr);
        int compteurTours = 0;
        int pixelX, pixelY;
        char *b64Color;
        while (pch != NULL)
        {
            if (compteurTours == 1) {
                char *pixelCoordinate;
                char *tok2 = strdup(pch);
                
                pixelCoordinate = strtok(tok2, "x");

                int compteurPixel = 0;
                while(pixelCoordinate != NULL) {
                    if (compteurPixel == 0) {
                        pixelX = atoi(pixelCoordinate);
                    } else if (compteurPixel == 1) {
                        pixelY = atoi(pixelCoordinate);
                    }

                    pixelCoordinate = strtok(NULL, "x");
                    compteurPixel++;
                }

                free(tok2);

            } else if (compteurTours == 2) {
                // Chaine en base64 à décoder
                pch[strlen(pch)-1] = '\0';
                b64Color = strdup(pch);

                char* new_string;
                if (pch != NULL) {
                    new_string = strdup(pch);
                }
                const char* base64string = new_string;

                strcpy(base64string, pch);
            }

            compteurTours++;
            pch = strtok_r(NULL, " ", &saveptr);
        }
        char messageEnvoi[LG_MESSAGE];
        memset(messageEnvoi, 0x00, LG_MESSAGE * sizeof(char));
        
        if (pixelX < 0 || pixelX >= 80 || pixelY < 0 || pixelY >= 39) {
            sprintf(messageEnvoi, "11");
        } else {
            sprintf(messageEnvoi, "00");
            tab[pixelX][pixelY].b64 = b64Color;
        }
        
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
    } else {
        char messageEnvoi[LG_MESSAGE];
        memset(messageEnvoi, 0x00, LG_MESSAGE * sizeof(char));
        sprintf(messageEnvoi, "10");
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

void base64_decode(char *src, char dest[256])
{
    char binary[64][7] = {
        "000000", "000001", "000010", "000011",
        "000100", "000101", "000110", "000111",
        "001000", "001001", "001010", "001011",
        "001100", "001101", "001110", "001111",
        "010000", "010001", "010010", "010011",
        "010100", "010101", "010110", "010111",
        "011000", "011001", "011010", "011011",
        "011100", "011101", "011110", "011111",
        "100000", "100001", "100010", "100011",
        "100100", "100101", "100110", "100111",
        "101000", "101001", "101010", "101011",
        "101100", "101101", "101110", "101111",
        "110000", "110001", "110010", "110011",
        "110100", "110101", "110110", "110111",
        "111000", "111001", "111010", "111011",
        "111100", "111101", "111110", "111111"
    };

    char base64_chars[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    for (int i = 0; i < strlen(src); i++) {
        for (int j = 0; j < 64; j++) {
            if (src[i] == base64_chars[j]) {
                strcat(dest, binary[j]);
            }
        }
    }
}