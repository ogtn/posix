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
int lockerInit(locker *locker, char const *dirPath, char const *servAddr, unsigned int server_port, unsigned int client_port)
{
    int retVal = -1;
    
    if(locker != NULL && dirPath != NULL && servAddr != NULL)
    {
        DIR * dir = opendir(dirPath);
        
        locker->path = malloc(strlen(dirPath) + 1);
        if(locker->path == NULL)
        {
            printf("PVB \n");
        }
        
        strcpy(locker->path, dirPath);
        
        if(dir != NULL)
        {
            locker->files = newList();
            if(locker->files != NULL)
            {
                /* Ajout des noms des fichiers partagés */
                struct dirent * entry = readdir(dir);
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
                    }
                    
                    entry = readdir(dir);
                }
                
                locker->lockedReadFiles = newList();
                if(locker->lockedReadFiles != NULL)
                {
                    locker->lockedWriteFiles = newList();    
                    if(locker->lockedWriteFiles != NULL)
                    {
                        locker->sd = clientInitSocket(server_port, servAddr);
                        if(locker->sd != -1)
                        {
                            printf("port: %d\n", client_port);
                            client_port = htonl(client_port);
                            printf("port: %d\n", client_port);
                            
                            if(send(locker->sd, &client_port, sizeof(long), 0) == -1)
                            {
                                lockerClose(locker);
                                retVal = -1; 
                            }
                            else
                            {
                                locker->servId = runServer(dirPath, client_port);
                                if(locker->servId.isValid)
                                    retVal = 0;
                            }
                        }
                    }
                }
            }
            
            closedir(dir);
        }
    }
    
    if(retVal == -1)
    {
        lockerClose(locker);
    }
    
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
                                char type;                  /* Type du message, voir enum msgType */
                                unsigned int port;          /* Le port du propriétaire de la derniere version du fichier */
                                long clientAddr;
                            
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
    int fileIsLocked = 1;
    strncpy(msgCS.fileName, fileName, FILE_SIZE);
    msgCS.fileName[FILE_SIZE - 1] = '\0';
    msgCS.version = htonl(0);
    
    if(locker != NULL && fileName != NULL)
    {
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
            fileIsLocked = 0;
        }
        
        if(fileIsLocked)
        {        
            /* Envoi du type d'opération effectuée par le client */
            if(send(locker->sd, &msgCS, sizeof(messageCS), 0) != -1)
            {
                if(msgCS.type == UNLOCK_READ)
                {
                    free(listRemove(locker->lockedReadFiles, fileName, (compar)strcmp));
                }
                else
                {
                    localFile *lf = listSearch(locker->files, fileName, (compar)findFile);
                    lf->version++;     
                    free(listRemove(locker->lockedWriteFiles, fileName, (compar)strcmp));
                }
            }
            else
            {
                retVal = CAN_T_SEND;
            }
        }
        else
        {
            retVal = FILE_IS_NOT_LOCKED;
        }
    }
    else
    {
        retVal = BAD_PARAMETER;
    }

    return retVal;
}

int lockerDestroy(locker const *l)
{
    if(l != NULL)
    {
        messageCS msgCS;
        msgCS.type = QUIT;

        /* Envoi du type d'opération effectuée par le client */
        if(send(l->sd, &msgCS, sizeof(messageCS), 0) == -1)
            perror("destroy:send()");
        
        return 0;
    }

    return -1;
}
