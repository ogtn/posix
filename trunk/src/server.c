#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include "file.h"

#include "server.h"


int main(int argc, char** argv)
{
    pthread_t tid;
    sharedFile *sharedFiles;
    pthread_attr_t attr;
    int serverPort, serverFilePort;
    int nbFiles;
	int sock;
    server *s;
    socklen_t sz = sizeof(struct sockaddr_in);
    struct sockaddr_in ad;
    int clients[BACKLOG];
    int i;
    int nbClients = 0;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    char * dirPath = NULL, * configFile = NULL;
    serverId servId;
    char servIpAddress[16];

    if(argc == 5)
    {
        serverPort = strToLong(argv[1]);
        configFile = argv[2];
        serverFilePort = strToLong(argv[3]);
        dirPath = argv[4];
    }

    if(argc != 5 || serverPort < 0 || serverFilePort < 0 || 
        fileExist(dirPath, DIRECTORY) == 0 || 
        fileExist(configFile, REGULAR_FILE) == 0)
    {
        fprintf(stderr, "Paramètres incorrects : ");
        puts("Usage: $ server <port> <config_file> <ftp_port> <server_dir>");
        return EXIT_FAILURE;
    }

    if(ipAddress(servIpAddress) == -1)
        return EXIT_FAILURE;

    servId = runServer(dirPath, serverFilePort);
    if(!servId.isValid)
        return EXIT_FAILURE;

    for(i = 0; i < BACKLOG; i++)
		clients[i] = UNUSED;
    
    if((sock = serverInitSocket(serverPort, BACKLOG)) == -1)
        perror("initSocket()");
    
    sharedFiles = getFiles(argv[2], &nbFiles);
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
        s->dirPath = malloc(strlen(dirPath) + 1);
        s->serverFilePort = serverFilePort;

        strcpy(s->servIpAddress, servIpAddress);
        strcpy(s->dirPath, dirPath);

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
    (*s->nbClients)++;
    printf("Un nouveau client d'identifiant %d vient de se connecter\n", s->clientIndex);
    printf("Il y a maintenant %d client(s) connecté(s)\n", *s->nbClients);
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
                lockRead(sf, &msg, s);
            break;
            case LOCK_WRITE:
                lockWrite(sf, &msg, s);
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
    }
    
    free(s);
     
    return NULL;
}


void deco(sharedFile *sharedFiles, server *s)
{
    int i;
    messageSC msgSC;
    
    for(i = 0; i < s->nbFiles; i++)
    {
        pthread_mutex_lock(&sharedFiles[i].mutex);
          
        if(strcmp(sharedFiles[i].lastVersionAddr, s->clientAddr) == 0
        && sharedFiles[i].lastVersionPort == s->clientPort)
        {
            printf("besoin de transferer %s depuis %s:%ld\n", 
                    sharedFiles[i].fileName, s->clientAddr, s->clientPort);
            
            if(downloadFile(s->dirPath, sharedFiles[i].fileName, 
                            s->clientPort, s->clientAddr) == -1)
            {
                fprintf(stderr, "Echec du transfert de fichiers \n");
            }
            /* Le nouveau possesseur du fichier est le serveur */
            else
            {
                sharedFiles[i].lastVersionPort = s->serverFilePort;
                strcpy(sharedFiles[i].lastVersionAddr, s->servIpAddress);
            }
        }
        
        pthread_mutex_unlock(&sharedFiles[i].mutex);
    }
    
    /* Les transferts sont terminés on autorise le client à quitter 
     * l'application */
    msgSC.type = QUIT;
    
    if(send(s->sd, &msgSC, sizeof(messageSC), 0) == -1)
        perror("send()");
    
    pthread_mutex_lock(s->mutex);
    (*s->nbClients)--;
    s->clients[s->clientIndex] = UNUSED;
    printf("Le client d'identifiant %d vient de se deconnecter\n", s->clientIndex);
    printf("Il reste maintenant %d client(s) connecté(s)\n", *s->nbClients);
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


sharedFile *getFiles(char *fileName, int *nbFiles)
{
    char buff[256];
    sharedFile *sharedFiles, *file;
    int i;
    FILE *fic = fopen(fileName, "r");
    *nbFiles = 0;

    if(fic == NULL)
    {
        puts("Probleme d'ouverture du fichier de config");
        perror(fileName);
        exit(EXIT_FAILURE);
    }
        
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
        strcpy(file->lastVersionAddr, "nobody");
        file->lastVersionPort = 0;
        file->version = 0;
        file->lockList = newList();
        pthread_mutex_init(&file->mutex, NULL);
        
        for(i = 0; i < BACKLOG; i++)
			pthread_cond_init(&file->conds[i], NULL);
		
        pthread_cond_init(&file->cond, NULL);
        
        file++;
    }
    
    fclose(fic);
    
    return sharedFiles;
}


request *initRequest(messageCS *msg, server *s)
{
    request *rq = malloc(sizeof(request));
    rq->type = msg->type;
    rq->version = msg->version;
    rq->socketDescriptor = s->sd;
    rq->clientIndex = s->clientIndex;
    
    return rq;
}


void lockRead(sharedFile *sf, messageCS *msgCS, server *s)
{
    messageSC msgSC;
    request *rq = initRequest(msgCS, s);
    
    pthread_mutex_lock(&sf->mutex);
    add(sf->lockList, rq);
    printf("lockRead(): client %d avant cond\n", s->clientIndex);

    if(sf->isWriteLock)
		pthread_cond_wait(&sf->conds[s->clientIndex], &sf->mutex);

    printf("lockRead(): client %d après cond\n", s->clientIndex);
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
    
    if(sf->lockList->size != 0)
    {
		request *next = sf->lockList->head->data;
		
		if(next->type == LOCK_READ)
			pthread_cond_signal(&sf->conds[next->clientIndex]);
	}
      
    pthread_mutex_unlock(&sf->mutex);
    
    if(send(rq->socketDescriptor, &msgSC, sizeof(messageSC), 0) == -1)
        perror("send()");
    
    free(rq);
}


void lockWrite(sharedFile *sf, messageCS *msgCS, server *s)
{
    messageSC msgSC;
    request *rq = initRequest(msgCS, s);
    
    pthread_mutex_lock(&sf->mutex);
    add(sf->lockList, rq);
    printf("lockWrite(): client %d avant cond\n", s->clientIndex);

    if(sf->isWriteLock || sf->nbReadLock > 0)
		pthread_cond_wait(&sf->conds[s->clientIndex], &sf->mutex);

    printf("lockWrite(): client %d après cond\n", s->clientIndex);
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
		if(sf->lockList->size != 0)
		{
			request *next = sf->lockList->head->data;
			
			if(next->type == LOCK_WRITE)
				pthread_cond_signal(&sf->conds[next->clientIndex]);
		}
        
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
    
    if(sf->lockList->size != 0)
    {
		request *next = sf->lockList->head->data;
		pthread_cond_signal(&sf->conds[next->clientIndex]);
	}
    
    printf("Ecriture terminée: le fichier \"%s\" est libre\n", sf->fileName);
}
