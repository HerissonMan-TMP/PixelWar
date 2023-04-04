#include <stdio.h>
#include <stdlib.h> /* pour exit */
#include <unistd.h> /* pour close */
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>     /* pour memset */
#include <netinet/in.h> /* pour struct sockaddr_in */
#include <arpa/inet.h>  /* pour htons et inet_aton */

#define LG_MESSAGE 20000

int main(int argc, char* argv[])
{
    char *ip = NULL;
    int port = 0;
    
    if (argc < 3) {
        printf("Usage: %s -s IP [-p PORT]\n", argv[0]);
        return 1;
    }
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i+1 < argc) {
            ip = argv[i+1];
            i++;
        } else if (strcmp(argv[i], "-p") == 0 && i+1 < argc) {
            port = atoi(argv[i+1]);
            i++;
        }
    }
    
    if (ip == NULL) {
        printf("IP address not specified.\n");
        return 1;
    }
    
    printf("Connecting to %s:%d...\n", ip, port);

    int descripteurSocket, quitter = 0;
    char choix[25] = {0}, QuitChar[10] = "/quit";
    struct sockaddr_in pointDeRencontreDistant;
    socklen_t longueurAdresse;
    char messageEnvoi[LG_MESSAGE]; /* le message de la couche Application ! */
    char messageRecu[LG_MESSAGE];  /* le message de la couche Application ! */
    int ecrits, lus;               /* nb d’octets ecrits et lus */
    int retour;

    // Crée un socket de communication
    descripteurSocket = socket(PF_INET, SOCK_STREAM, 0); /* 0 indique que l’on utilisera le
       protocole par défaut associé à SOCK_STREAM soit TCP */

    // Teste la valeur renvoyée par l’appel système socket()
    if (descripteurSocket < 0) /* échec ? */
    {
        perror("socket"); // Affiche le message d’erreur
        exit(-1);         // On sort en indiquant un code erreur
    }

    printf("Socket créée avec succès ! (%d)\n", descripteurSocket);

    // Obtient la longueur en octets de la structure sockaddr_in
    longueurAdresse = sizeof(pointDeRencontreDistant);
    // Initialise à 0 la structure sockaddr_in
    memset(&pointDeRencontreDistant, 0x00, longueurAdresse);
    // Renseigne la structure sockaddr_in avec les informations du serveur distant
    pointDeRencontreDistant.sin_family = PF_INET;
    // On choisit le numéro de port d’écoute du serveur
    pointDeRencontreDistant.sin_port = htons(port); // = 5000
    // On choisit l’adresse IPv4 du serveur
    inet_aton(ip, &pointDeRencontreDistant.sin_addr); // à modifier selon ses besoins

    // Débute la connexion vers le processus serveur distant
    if ((connect(descripteurSocket, (struct sockaddr *)&pointDeRencontreDistant, longueurAdresse)) == -1)
    {
        perror("connect");        // Affiche le message d’erreur
        close(descripteurSocket); // On ferme la ressource avant de quitter
        exit(-2);                 // On sort en indiquant un code erreur
    }

    printf("Connexion au serveur réussie avec succès !\n");

    //--- Début de l’étape n°4 :
    // Initialise à 0 les messages
    memset(messageEnvoi, 0x00, LG_MESSAGE * sizeof(char));
    memset(messageRecu, 0x00, LG_MESSAGE * sizeof(char));

    // Envoie un message au serveur
    while (quitter != 1)
    {
        printf("\n----- Client -----\n\n");
        printf("/getMatrix\n");
        printf("/getSize\n");
        printf("/getLimits\n");
        printf("/getVersion\n");
        printf("/getWaitTime\n");
        printf("/setPixel\n");
        printf("/quit\n");

        printf("Message a transmettre : ");
        //scanf("%s", choix);
        fgets(choix, 25, stdin);
        printf("CHOIX: %d\n", strcmp(choix, QuitChar));
        if (strncmp(choix, QuitChar, 5) == 0) {
            quitter = 1;
        } 
        else {
            strcpy(messageEnvoi, choix);
            printf("\nDEBUG - %s\n", messageEnvoi);
        }


        ecrits = write(descripteurSocket, messageEnvoi, strlen(messageEnvoi)); // message à TAILLE variable

        switch (ecrits)
        {
        case -1: /* une erreur ! */
            perror("write");
            close(descripteurSocket);
            exit(-3);
        case 0: /* la socket est fermée */
            fprintf(stderr, "La socket a été fermée par le serveur !\n\n");
            close(descripteurSocket);
            return 0;
        default: /* envoi de n octets */
            printf("Message %s envoyé avec succès (%d octets)\n\n", messageEnvoi, ecrits);
        }

        memset(messageEnvoi, 0x00, LG_MESSAGE * sizeof(char));

        /* Reception des données du serveur */
        lus = read(descripteurSocket, messageRecu, LG_MESSAGE * sizeof(char)); /* attend un message de TAILLE fixe */
        
        switch (lus)
        {
        case -1: /* une erreur ! */
            perror("read");
            close(descripteurSocket);
            exit(-4);
        case 0: /* la socket est fermée */
            fprintf(stderr, "La socket a été fermée par le serveur !\n\n");
            close(descripteurSocket);
            return 0;
        default: /* réception de n octets */
            printf("Message reçu du serveur : %s (%d octets)\n\n", messageRecu, lus);
        }

        memset(messageRecu, 0x00, LG_MESSAGE * sizeof(char));
    }

return 0;
}