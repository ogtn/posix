#ifndef H_SERVER
#define H_SERVER

/*			=======================[Includes]=========================		  */

#include "list.h"
#include "tools.h"


/*			=======================[Defines/Enums]====================		  */

#define BACKLOG     5
#define NOT_FOUND   -1
#define UNUSED		0
#define USED		1

/*			=======================[Structures]=======================		  */

/* Représentation d'un fichier partagé */
typedef struct sharedFile
{
	char fileName[FILE_SIZE];   	/* Nom du fichier concerné */ 
	int isWriteLock;            	/* Si le fichier est bloqué en écriture */
	int nbReadLock;             	/* Nombre de clients ayant locké le fichier en lecture */
    long lastVersionPort;       	/* Port du possesseur de la derniere version */
    char lastVersionAddr[16];   	/* Adresse ip du possesseur de la derniere version */
	unsigned int version;       	/* Dernière version du fichier */
	list *lockList;             	/* Liste des requetes de lock de fichiers */
    pthread_mutex_t mutex;      	/* Mutex pour proteger l'acces à la structure */
    pthread_cond_t cond;        	/* Condition signalant la présence d'un unlock dans la file */
    pthread_cond_t conds[BACKLOG];	/* Tableau de conditions signalant la présence d'un unlock dans la file */
} sharedFile;


/* Requete en attente de traitement dans la liste */
typedef struct request
{
	char type;                  	/* Type du message */
	unsigned int version;       	/* Version du fichier possédée par le client */
    int socketDescriptor;       	/* Descripteur de socket utilisé pour les communications serveur -> client */
    int clientIndex;				/* Identifiant du client */
} request;


typedef struct server
{
    sharedFile *sharedFiles;		/* Tableau des fichiers partagés */
    int nbFiles;					/* Nombre de fichiers partagés */
    int clientIndex;				/* Identifiant du client */
    long clientPort;       			/* Port du client */
    char clientAddr[16];   			/* Adresse ip du client */
    int sd;							/* Socket de communication avec le client */
    int *clients;					/* Tableau des clients */
    int *nbClients;					/* Nombre de clients connectés */
    pthread_mutex_t *mutex;			/* Mutex pour proteger l'accès aux données du serveur */
    char * dirPath;                 /* Path du répertoire du serveur 
                                    contenant les fichiers téléchargés */
    int serverFilePort;             /* Port du serveur de téléchargement 
                                    de fichiers du serveur */
    char servIpAddress[16];         /* Adresse IP du serveur de téléchargement 
                                    de fichiers du serveur */
} server;

/*			=======================[Prototypes]=======================		  */

int initSocket(int port, int backlog);

void defile(sharedFile *sf);

void *mainLoop(void *data);

sharedFile *getFiles(char *fileName, int *nbFiles);

int find(sharedFile *sharedFiles, int size, char *fileName);

void lockWrite(sharedFile *sf, messageCS *msg, server *s);

void lockRead(sharedFile *sf, messageCS *msgCS, server *s);

void unlockRead(sharedFile *sf);

void unlockWrite(sharedFile *sf, char *addr, long port);

void deco(sharedFile *sharedFiles, server *s);

request *initRequest(messageCS *msg, server *s);

#endif
