#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>

#include "server.h"

int main(int argc, char** argv)
{
    pthread_t tid;
    sharedFile *sharedFiles;
    pthread_attr_t attr;
    int port = 10000;
    int nbFiles;
	int sock;
    args *a;
    socklen_t sz = sizeof(struct sockaddr_in);
    struct sockaddr_in ad;
    
    if((sock = serverInitSocket(port, BACKLOG)) == -1)
        perror("initSocket()");
    
    sharedFiles = getFiles(&nbFiles);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    
    while(1)
    {
        a = malloc(sizeof(args));
        a->sharedFiles = sharedFiles;
        a->nbFiles = nbFiles;

        if((a->sd = accept(sock, (struct sockaddr *)&ad, &sz)) != -1)
        {
            long p;
            
            if(recv(a->sd, &p, sizeof(long), 0) == -1)
            {
                perror("recv");
                close(a->sd);
                continue;
            }
            
            strcpy(a->lastVersionAddr, inet_ntoa(ad.sin_addr));
            a->lastVersionPort = ntohl(p);
            pthread_create(&tid, &attr, mainLoop, a);    
        }
        else
            perror("accept()");
	}

    free(sharedFiles);
        	
	return EXIT_SUCCESS;
}


void *mainLoop(void *data)
{
    int stop = !QUIT;
    args *a = data;
    sharedFile *sf;
    messageCS msg;
    int index;
    
    puts("Un nouveau client vient de se connecter");
    
    while(!stop)
    {
        if(recv(a->sd, &msg, sizeof(messageCS), 0) == -1)
        {
            perror("recv");
            return NULL;
        }
        
        index = find(a->sharedFiles, a->nbFiles, msg.fileName);
        if(index != NOT_FOUND)
        {
            sf = &a->sharedFiles[index];
            msg.version = ntohl(msg.version);
        }
        
        /* Nom de fichier incorrect */
        if(index == NOT_FOUND && msg.type != QUIT)
        {
            messageSC ack;
            
            ack.type = ERROR;
            printf("Le fichier \"%s\" ne fait pas partit du repertoire.\n", msg.fileName);
            
            if(send(a->sd, &ack, sizeof(messageSC), 0) == -1)
                perror("send()");
                
            continue;
        }
           
        switch(msg.type)
        {
            case LOCK_READ:
                lockRead(sf, &msg, a->sd);
            break;
            case LOCK_WRITE:
                lockWrite(sf, &msg, a->sd);
            break;
            
            case UNLOCK_READ:
                unlockRead(sf);
            break;
            
            case UNLOCK_WRITE:
                unlockWrite(sf, a->lastVersionAddr, a->lastVersionPort);
            break;
            
            case QUIT:
                deco(sf, a);
                stop = QUIT;
            break;
            
            default:
                puts("ouatafocindaplace");
        }
        puts("===============");
    }
    
    free(a);
    puts("Un client vient de se deconnecter");
    
    return NULL;
}


void deco(sharedFile *sharedFiles, args *a)
{
    int i;
    
    for(i = 0; i < a->nbFiles; i++)
    {          
        if(strcmp(sharedFiles[i].lastVersionAddr, a->lastVersionAddr) == 0
        && sharedFiles[i].lastVersionPort == a->lastVersionPort)
        {
            printf("besoin de transferer %s depuis %s:%ld\n", sharedFiles[i].fileName, a->lastVersionAddr, a->lastVersionPort);
        }
    }
}


int find(sharedFile *sharedFiles, int size, char *fileName)
{
    int i;
    
    for(i = 0; i < size; i++)
    {
        if(strcmp(sharedFiles[i].fileName, fileName) == 0)
            return i;
    }
    
    return NOT_FOUND;
}


sharedFile *getFiles(int *nbFiles)
{
    char buff[256];
    sharedFile *sharedFiles, *file;
    FILE *fic = fopen("files.txt", "r");
    *nbFiles = 0;
        
    while(fgets(buff, 256, fic))
        (*nbFiles)++;
    
    sharedFiles = malloc(sizeof(sharedFile) * (*nbFiles));
    rewind(fic);
    file = sharedFiles;
    
    while(fgets(buff, 256, fic))
    {
        *strchr(buff, '\n') = '\0';
        printf("Ajout du fichier \"%s\"\n", buff);
        
        strncpy(file->fileName, buff, FILE_SIZE);        
        file->isWriteLock = 0;	
        file->nbReadLock = 0;
        file->lastVersionAddr[0] = '\0';
        file->lastVersionPort = 0;
        file->version = 0;
        file->lockList = newList();
        pthread_mutex_init(&file->mutex, NULL);
        pthread_cond_init(&file->cond, NULL);
        
        file++;
    }
    
    fclose(fic);
    
    return sharedFiles;
}


request *initRequest(messageCS *msg, int sd)
{
    request *rq = malloc(sizeof(request));
    rq->type = msg->type;
    strncpy(rq->fileName, msg->fileName, FILE_SIZE);
    rq->version = msg->version;
    rq->socketDescriptor = sd;
    
    return rq;
}


void lockRead(sharedFile *sf, messageCS *msgCS, int sd)
{
    messageSC msgSC;
    request *rq = initRequest(msgCS, sd);
    
    pthread_mutex_lock(&sf->mutex);
    add(sf->lockList, rq);
    
    while(sf->isWriteLock || ((request *)(sf->lockList->head->data))->socketDescriptor != sd) 
        pthread_cond_wait(&sf->cond, &sf->mutex);
    
    rq = getHead(sf->lockList);

    printf("Demande de lock en écriture sur le fichier \"%s\"\n", sf->fileName);
    sf->nbReadLock++;

    if(sf->nbReadLock == 1)
    {
        printf("Lecture commencée: le fichier \"%s\" est locké\n", sf->fileName);
    }
    else
        printf("Il y a %d lecture(s) en cours sur le fichier \"%s\"\n", sf->nbReadLock, sf->fileName);
        
    /* Verification de la version */
    if(rq->version < sf->version)
    {
        printf("Une mise a jour est requise pour le fichier \"%s\"\n", sf->fileName);
        msgSC.type = UPDATE_NEEDED;
        strcpy(msgSC.addr, sf->lastVersionAddr);
        msgSC.port = htonl(sf->lastVersionPort);
    }
    else
    {
        printf("le fichier est a jour!\n");
        msgSC.type = LOCKED;
    }
    
    pthread_mutex_unlock(&sf->mutex);
    pthread_cond_broadcast(&sf->cond);
    if(send(rq->socketDescriptor, &msgSC, sizeof(messageSC), 0) == -1)
        perror("send()");
    
    free(rq);
}


void lockWrite(sharedFile *sf, messageCS *msgCS, int sd)
{
    messageSC msgSC;
    request *rq = initRequest(msgCS, sd);
    
    pthread_mutex_lock(&sf->mutex);
    add(sf->lockList, rq);
    
    while(sf->isWriteLock || sf->nbReadLock > 0 || ((request *)(sf->lockList->head->data))->socketDescriptor != sd) 
        pthread_cond_wait(&sf->cond, &sf->mutex);

    rq = getHead(sf->lockList);
    
    /* Verification de la version */
    if(rq->version < sf->version)
    {
        printf("Une mise a jour est requise pour le fichier \"%s\"\n", sf->fileName);
        msgSC.type = UPDATE_NEEDED;
        strcpy(msgSC.addr, sf->lastVersionAddr);
        msgSC.port = htonl(sf->lastVersionPort);
    }
    else
        msgSC.type = LOCKED;
    
    printf("Demande de lock en écriture sur le fichier \"%s\"\n", sf->fileName);
    sf->isWriteLock = 1;
    pthread_mutex_unlock(&sf->mutex);
    printf("Ecriture commencée: le fichier \"%s\" est locké\n", sf->fileName);
    
    if(send(rq->socketDescriptor, &msgSC, sizeof(messageSC), 0) == -1)
        perror("send()");
        
    free(rq);
}


void unlockRead(sharedFile *sf)
{
    pthread_mutex_lock(&sf->mutex);
    sf->nbReadLock--;
    
    /* On debloque le fichier si aucun autre lecteur ne persiste */
    if(sf->nbReadLock == 0)
    {
        pthread_cond_broadcast(&sf->cond);
        printf("Lecture terminée: le fichier \"%s\" est libre\n", sf->fileName);
    }
    else
        printf("Il reste %d lecture(s) en cours sur le fichier \"%s\"\n", sf->nbReadLock, sf->fileName);
        
    pthread_mutex_unlock(&sf->mutex);
}


void unlockWrite(sharedFile *sf, char *addr, long port)
{
    pthread_mutex_lock(&sf->mutex);
    sf->isWriteLock = 0;
    sf->version++;
    strcpy(sf->lastVersionAddr, addr);
    sf->lastVersionPort = port;
    pthread_mutex_unlock(&sf->mutex);
    pthread_cond_broadcast(&sf->cond);
    printf("Ecriture terminée: le fichier \"%s\" est libre\n", sf->fileName);
}
