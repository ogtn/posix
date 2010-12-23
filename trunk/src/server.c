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
    server *s;
    socklen_t sz = sizeof(struct sockaddr_in);
    struct sockaddr_in ad;
    int clients[BACKLOG];
    int i;
    int nbClients = 0;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    
    for(i = 0; i < BACKLOG; i++)
		clients[i] = UNUSED;
    
    if((sock = serverInitSocket(port, BACKLOG)) == -1)
        perror("initSocket()");
    
    sharedFiles = getFiles(&nbFiles);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    
    while(1)
    {
        s = malloc(sizeof(server));
        s->sharedFiles = sharedFiles;
        s->nbFiles = nbFiles;
        s->clients = clients;
        s->nbClients = &nbClients;
        s->mutex = &mutex;

        if((s->sd = accept(sock, (struct sockaddr *)&ad, &sz)) != -1)
        {
            long p;
            
            if(recv(s->sd, &p, sizeof(long), 0) == -1)
            {
                perror("recv");
                close(s->sd);
                continue;
            }
            
            pthread_mutex_lock(&mutex);
            for(i = 0; i < BACKLOG; i++)
			{
				if(clients[i] == UNUSED)
				{
					clients[i] = USED;
					s->clientIndex = i;
					break;
				}
			}
			
			if(i == BACKLOG)
			{
				puts("Oups, trop de clients connectés!");
				return EXIT_FAILURE;
			}
			
			pthread_mutex_unlock(&mutex);
            
            strcpy(s->clientAddr, inet_ntoa(ad.sin_addr));
            s->clientPort = ntohl(p);
            pthread_create(&tid, &attr, mainLoop, s);    
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
    server *s = data;
    sharedFile *sf;
    messageCS msg;
    int index;
    
    pthread_mutex_lock(s->mutex);
    *s->nbClients++;
    printf("Un nouveau client d'identifiant %d vient de se connecter", s->clientIndex);
    printf("Il y a maintenant %d clients connectés\n", *s->nbClients);
    pthread_mutex_unlock(s->mutex);
    
    while(!stop)
    {
        if(recv(s->sd, &msg, sizeof(messageCS), 0) == -1)
        {
            perror("recv");
            return NULL;
        }
        
        index = find(s->sharedFiles, s->nbFiles, msg.fileName);
        if(index != NOT_FOUND)
        {
            sf = &s->sharedFiles[index];
            msg.version = ntohl(msg.version);
        }
        
        /* Nom de fichier incorrect */
        if(index == NOT_FOUND && msg.type != QUIT)
        {
            messageSC ack;
            
            ack.type = ERROR;
            printf("Le fichier \"%s\" ne fait pas partit du repertoire.\n", msg.fileName);
            
            if(send(s->sd, &ack, sizeof(messageSC), 0) == -1)
                perror("send()");
                
            continue;
        }
           
        switch(msg.type)
        {
            case LOCK_READ:
                lockRead(sf, &msg, s->sd);
            break;
            case LOCK_WRITE:
                lockWrite(sf, &msg, s->sd);
            break;
            
            case UNLOCK_READ:
                unlockRead(sf);
            break;
            
            case UNLOCK_WRITE:
                unlockWrite(sf, s->clientAddr, s->clientPort);
            break;
            
            case QUIT:
                deco(sf, s);
                stop = QUIT;
            break;
            
            default:
                puts("ouatafocindaplace");
        }
        puts("===============");
    }
    
    free(s);
     
    return NULL;
}


void deco(sharedFile *sharedFiles, server *s)
{
    int i;
    
    for(i = 0; i < s->nbFiles; i++)
    {          
        if(strcmp(sharedFiles[i].lastVersionAddr, s->clientAddr) == 0
        && sharedFiles[i].lastVersionPort == s->clientPort)
        {
            printf("besoin de transferer %s depuis %s:%ld\n", sharedFiles[i].fileName, s->clientAddr, s->clientPort);
        }
    }
    
    pthread_mutex_lock(s->mutex);
    *s->nbClients--;
    printf("Le client d'identifiant %d vient de se deconnecter", s->clientIndex);
    printf("Il reste maintenant %d clients connectés\n", *s->nbClients);
    pthread_mutex_unlock(s->mutex);
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



/* Pour le tableau de conditions:
	il faut noter dans les requetes le numero du client appelant => l'indice
	de la condition dans le tableau de conditions.
	
	La merde étant d'attriber un bon numero au client qui se connecte
	(à faire dans la thread main, avec un tableau des clients connectés.
	
	La structure agrs devient server: elle contient uniquement des ptrs,
	et un entier qui est l'identifiant du client.
	
	Il faut en passer une copie à chaque thread mainloop, car l'entier doit être
	différent pour chauque, et les ptrs doivent pointer sur les mêmes données
	communes (sharedFile, nb de client loggés etc...)
	
	Rajouter un ptr de semaphore va peut être être necessaire dans la structure,
	pour poteger les membres (à voir si on les modifie au cours sde l'execution */
