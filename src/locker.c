#define _POSIX_SOURCE 1

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <dirent.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>

#include "file.h"
#include "locker.h"

typedef struct localFile
{
    char name[FILE_SIZE];
    int version;
} localFile;

/**
 * Ferme et libère les ressources d'un locker.
 * 
 * \param locker
 *      Le locker dont on ferme et libère les ressources.
 */
static void lockerClose(locker * locker)
{
    if(locker != NULL)
    {        
        if(locker->sd != -1)
        {
            shutdown(locker->sd, SHUT_RDWR);
            
            if(close(locker->sd) == -1)
            {
                perror("lockerClose : close");
            }
        }
        
        if(locker->lockedReadFiles != NULL)
        {
            listDestroyed(&(locker->lockedReadFiles), 1);
        }
        
        if(locker->lockedWriteFiles != NULL)
        {
            listDestroyed(&(locker->lockedWriteFiles), 1);
        }
        
        if(locker->files != NULL)
        {
            listDestroyed(&(locker->files), 1);
        }
        
        if(locker->servId.isValid)
            stopServer(&locker->servId);
    }
}

/**
 * Fonction de recherche d'un fichier via son nom.
 * 
 * \param file
 *      Le fichier recherché.
 * \param name
 *      Le nom du fichier recherché.
 * 
 * \return Renvoie un entier négatif, nul, ou positif, si le nom du 
 *      fichier est respectivement inférieure, égale ou supérieure au 
 *      nom du fichier recherché.
 */
static int findFile(void const * file, void const * name)
{
    return strcmp(((localFile*)file)->name, name);
}

/**
 * Initialise un locker de fichiers.
 * 
 * \param locker
 *      Le locker à initialiser.
 * \param dirPath
 *      Le path vers le dossier contenant les fichiers vérouillables.
 * \param servAddr
 *      L'adresse du serveur gérant le lockage des fichiers.
 * \param port
 *      Le port de connection au serveur gérant le lockage des fichiers.
 * 
 * \return -1 Si la fonction échoue, 0 sinon.
 */
int lockerInit(locker *locker, char const *dirPath, char const *servAddr,
                unsigned int serverPort, unsigned int clientPort)
{
    int retVal = -1;
    DIR * dir = NULL;
    int convClientPort = htonl(clientPort);
    struct dirent * entry = NULL;
    
    if(locker == NULL || dirPath == NULL || servAddr == NULL)
        goto out;
    
    locker->sd = -1;
    locker->lockedReadFiles = NULL;
    locker->lockedWriteFiles = NULL;
    locker->files = NULL;
    locker->path = NULL;
    locker->servId.isValid = 0;      
    
    dir = opendir(dirPath);
    if(dir == NULL)
    {
        perror("lockerInit : opendir");
        goto out;
    }
    
    locker->path = malloc(strlen(dirPath) + 1);
    if(locker->path == NULL)
    {
        perror("lockerInit : malloc");
        goto out;
    }
        
    strcpy(locker->path, dirPath);
    
    locker->files = newList();
    if(locker->files == NULL)
        goto out;
    
    /* Ajout des noms des fichiers partagés */
    entry = readdir(dir);
    while(entry != NULL)
    {
        if(entry->d_name[0] != '.')
        {
            localFile * file = malloc(sizeof(localFile));
            if(file != NULL)
            {
                strncpy(file->name, entry->d_name, FILE_SIZE);
                file->name[FILE_SIZE-1] = '\0';
                file->version = 0;
                add(locker->files, file);
            }
            else
                perror("lockerInit : malloc");
        }
        
        entry = readdir(dir);
    }
                
    locker->lockedReadFiles = newList();
    if(locker->lockedReadFiles == NULL)
        goto out;
    
    locker->lockedWriteFiles = newList();    
    if(locker->lockedWriteFiles == NULL)
        goto out;
        
    /* Socket client */
    locker->sd = clientInitSocket(serverPort, servAddr);
    if(locker->sd == -1)
        goto out;
    
    if(send(locker->sd, &convClientPort, sizeof(long), 0) == -1)
        goto out;
    
    /* Lancement du serveur */
    locker->servId = runServer(dirPath, clientPort);
    
    if(locker->servId.isValid)
        retVal = 0;
    else
        goto out;
    
    out : 
        if(retVal == -1)
            lockerClose(locker);
            
        if(dir != NULL)
            closedir(dir);
    
    return retVal;
}

/**
 * Vérouille un fichier en lecture ou en écriture.
 * 
 * \param locker 
 *      Le locker à utiliser pour vérouiller le fichier.
 * \param fileName
 *      Le nom du fichier à locker.
 * \param type
 *      Le type d'opération à effectuer.
 * 
 * \return OK si le vérouillage est fait une erreur sinon.
 */
enum lockError lock(locker const *locker, char const *fileName, enum msgType type)
{
	enum lockError retVal = -1;
    messageCS msgCS;
    messageSC msgSC;
    
    msgCS.type = type;
    strncpy(msgCS.fileName, fileName, FILE_SIZE);
    msgCS.fileName[FILE_SIZE - 1] = '\0';
        
    if(locker != NULL && fileName != NULL && (type == LOCK_READ || type == LOCK_WRITE))
    {
        localFile *lf = listSearch(locker->files, fileName, (compar)findFile);
        
        /* Le fichier peut être locké */
        if(lf != NULL)
        {
            int fileIsNotLocked = 0;
            msgCS.version = htonl(lf->version);
            
            /* Le fichier n'est pas déjà locké */
            fileIsNotLocked = listContains(locker->lockedReadFiles, fileName, (compar)strcmp) == 0;
            fileIsNotLocked = fileIsNotLocked && listContains(locker->lockedWriteFiles, fileName, (compar)strcmp) == 0;
            
            if(fileIsNotLocked)
            {
                char * cpyFileName = malloc(strlen(fileName) + 1);
                if(cpyFileName != NULL)
                {
                    strcpy(cpyFileName, fileName);
                    
                    /* Envoi du type d'opération effectuée par le client */
                    if(send(locker->sd, &msgCS, sizeof(messageCS), 0) != -1)
                    {
                        /* Attente réponse du serveur */
                        if(recv(locker->sd, &msgSC, sizeof(messageSC), 0) != -1)
                        {
                            /* Ajout du fichier dans la liste des fichiers lockés */
                            if(type == LOCK_READ)
                            {
                                add(locker->lockedReadFiles, cpyFileName);
                            }
                            else
                            {
                                add(locker->lockedWriteFiles, cpyFileName);
                            }
                            
                            retVal = OK;
                            {                            
                                if(msgSC.type == UPDATE_NEEDED)
                                {
                                    printf("Besoin d'un update depuis %s:%d\n", msgSC.addr, ntohl(msgSC.port));
                                    
                                    if(downloadFile(locker->path, fileName, ntohl(msgSC.port), msgSC.addr) == -1)
                                    {
                                        retVal = CAN_T_DOWNLOAD;
                                    }
                                }
                            }
                        }
                        else
                        {
                            retVal = CAN_T_RECV;
                        }
                    }
                    else
                    {
                        retVal = CAN_T_SEND;
                    }
                    
                    if(retVal != OK)
                    {
                        free(cpyFileName);
                        cpyFileName = NULL;
                    }
                }
                else
                {
                    retVal = CAN_T_ALLOC;
                }
            }
            else
            {
                retVal = FILE_IS_ALREADY_LOCKED;
            }
        }
        else
        {
            retVal = FILE_NOT_FOUND;
        }
    }
    else
    {
        retVal = BAD_PARAMETER;
    }

    return retVal;
}

/**
 * Dévérouille un fichier préalablement vérouillé en lecture ou en écriture.
 * 
 * \param locker
 *      Le locker à utiliser pour dévérouiller le fichier.
 * \param fileName
 *      Le nom du fichier à dévérouiller.
 * 
 * \return OK si le vérouillage est fait une erreur sinon.
 */
enum lockError unlock(locker const *locker, char const *fileName)
{
    messageCS msgCS;
    enum lockError retVal = -1;
    
    if(locker == NULL || fileName == NULL)
    {
        retVal = BAD_PARAMETER;
        goto out;
    }
    
    strncpy(msgCS.fileName, fileName, FILE_SIZE);
    msgCS.fileName[FILE_SIZE - 1] = '\0';
    msgCS.version = htonl(0);
    
    /* Recherche du type de lockage */
    if(listContains(locker->lockedReadFiles, fileName, (compar)strcmp))
    {
        msgCS.type = UNLOCK_READ;
    }
    else if(listContains(locker->lockedWriteFiles, fileName, (compar)strcmp))
    {
        msgCS.type = UNLOCK_WRITE;
    }
    else
    {
        retVal = FILE_IS_NOT_LOCKED;
        goto out;
    }
    
    /* Envoi du type d'opération effectuée par le client */
    if(send(locker->sd, &msgCS, sizeof(messageCS), 0) == -1)
    {
        retVal = CAN_T_SEND;
        goto out;
    }
        
    retVal = OK;
    
    if(msgCS.type == UNLOCK_READ)
        free(listRemove(locker->lockedReadFiles, fileName, (compar)strcmp));
    else
    {
        localFile *lf = listSearch(locker->files, fileName, (compar)findFile);
        lf->version++;     
        free(listRemove(locker->lockedWriteFiles, fileName, (compar)strcmp));
    }
    
    out:
        return retVal;
}

/**
 * Détruit et libère un objet locker initialisé par l'appel à la 
 * fonction lockerInit.
 * 
 * \param locker
 *      Le locker à détruire.
 * 
 * \return -1 Si la fonction échoue, 0 sinon.
 */
int lockerDestroy(locker * locker)
{
    messageCS msgCS;
    messageSC msgSC;
    node * iter = NULL;
    
    if(locker == NULL)
    {
        return -1;    
    }
    
    iter = locker->lockedReadFiles->head;
    
    while(iter != NULL)
    {
        unlock(locker, ((localFile *)iter->data)->name);
        iter = iter->next;
    }
    
    iter = locker->lockedWriteFiles->head;
    
    while(iter != NULL)
    {
        unlock(locker, ((localFile *)iter->data)->name);
        iter = iter->next;
    }
    
    msgCS.type = QUIT;

    /* Envoi du quit */
    if(send(locker->sd, &msgCS, sizeof(messageCS), 0) == -1)
    {
        perror("lockerDestroy: send()");
        return -1;
    }
    
    /* Attente de la réponse du serveur */
    if(recv(locker->sd, &msgSC, sizeof(messageSC), 0) == -1)
    {
        perror("lockerDestroy: recv()");
    }
    
    lockerClose(locker);
    
    return 0;   
}
