#include "list.h"
#include "tools.h"
#include "file.h"

/**
 * 
 * 
 */
enum lockError
{
	OK,
    FILE_NOT_FOUND,
	FILE_IS_ALREADY_LOCKED,
	FILE_IS_NOT_LOCKED,
    CAN_T_DOWNLOAD,
	CAN_T_SEND,
	CAN_T_RECV,
	CAN_T_ALLOC,
    BAD_PARAMETER
};

/**
 * 
 * 
 */
typedef struct locker
{
    int sd;     			    /* Socket de client -> serveur */
    list * lockedReadFiles;		/* Liste des fichiers lockés en lecture par le client */
    list * lockedWriteFiles;	/* Liste des fichiers lockés en écriture par le client */
    list * files;		        /* Liste des fichiers pouvant être lockés par le client */
    char * path;                /* Path du dossier contenant les fichiers */
    serverId servId;            /* Identifiant du serveut de téléchargements de fichiers */
} locker;

int lockerInit(locker *locker, char const *dirPath, char const *servAddr, unsigned int server_port, unsigned int client_port);

enum lockError lock(locker const *locker, char const *fileName, enum msgType type);

enum lockError unlock(locker const *locker, char const *fileName);

int lockerDestroy(locker * locker);
