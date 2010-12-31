#define _POSIX_SOURCE 1

#define N 5
#define BUFFER_SIZE 1000

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "file.h"
#include "tools.h"

typedef void (*routine)(void*);

/**
 * Paramètre des threads
 */
typedef struct Arg
{
    int sd;						/* Descripteur de socket */
    char * dirPath;				/* Path vers le dossier ou l'on 
                                récupére les fichiers du serveur */
} Arg;

/**
 * Libère une structure ThreadParam allouée dans le tas.
 * 
 * \param param 
 * 		Un pointeur de pointeur sur la structure qui sera mis à NULL une
 * 		fois libéré.
 */
static void freeArg(Arg ** arg)
{
	if(arg != NULL && *arg != NULL)
	{
		if((*arg)->dirPath != NULL)
		{
			free((*arg)->dirPath);
		}
		free((*arg));
		(*arg) = NULL;
	}
}

/**
 * Crée et initialise un objet ThreadParam allouée dans le tas.
 * 
 * \param path
 * 		Path vers le dossier ou l'on récupére les fichiers du serveur.
 * \param sd
 * 		Descripteur de socket traité par la thread.
 * 
 * \return Un objet alloué et initialisé ou NULL si l'allocation échoue.
 */
static Arg * initArg(char const * path, int sd)
{
	Arg * arg = malloc(sizeof(Arg));
	int error = 0;
	
	if(arg != NULL)
	{
		arg->dirPath = malloc(strlen(path) + 1);
		
		if(arg->dirPath != NULL)
		{
			strcpy(arg->dirPath, path);
			arg->sd = sd;
		}
		else
		{
			perror("initArg : malloc");
			error = 1;
		}
	}
	else
	{
		perror("initArg : malloc");	
		error = 1;
	}
	
	if(error)
		freeArg(&arg);
	
	return arg;
}

/**
 * Envoie un fichier dans une socket.
 * 
 * \param path
 * 		Le path du fichier à envoyer dans la socket.
 * 
 * \param sd
 * 		Le descripteur de la socket dans laquelle on envoie le fichier.
 * 
 * \return -1 si une erreur c'est produite, 0 sinon.
 */
static int sendFile(char const * path, int sd)
{
	int error = 0;
	
    int fd = open(path, O_RDONLY);
    
    if(fd != -1)
    {
        char file[1000];
        
        /* Lecture d'une partie du fichier */
        int nbRead = read(fd, file, 1000 * sizeof(char));
        
        while(nbRead != -1 && nbRead != 0 && error == 0)
        {
            /* Envoi d'une partie du fichier */
            if(send(sd, file, nbRead, 0) != -1)
            {
                nbRead = read(fd, file, 1000 * sizeof(char));
            }
            else
            {
                error = 1;
                perror("sendFile : send");
            }
        }
        
        if(nbRead == -1)
        {
            error = 1;
            perror("sendFile : read");
        }
    }
    else
    {
        error = 1;
        perror("sendFile : open");
    }
	
    if(fd != -1)
        close(fd);
    
	return error == 1 ? -1 : 0;
}

/**
 * Code de la thread qui traite une connection d'un client.
 * 
 * \param p 
 * 		Paramètre de création de la thread.
 */
static void * threadConnection(void * p)
{
	Arg * arg = p;
	
	int recvr = 0; 
	char name[256];
	
	/* Récupération du nom du fichier à télécharger */
	recvr = recv(arg->sd, name, sizeof(char) * 256, 0);
	if(recvr != -1)
	{
		/* Création du path vers le fichier à envoyer */
		size_t size = sizeof(char) * (strlen(arg->dirPath) + strlen(name) + 1);
		char * pathName = malloc(size);
		
		if(pathName != NULL)
		{
			strcpy(pathName, arg->dirPath);
			strcat(pathName, name);
			
			sendFile(pathName, arg->sd);
			
			free(pathName);
			pathName = NULL;
			
			close(arg->sd);
		}
	}
	else
	{
		perror("downloadFile : recv");
	}
	
    freeArg(&arg);
    
	return NULL;
}

static void myShutdown(void * arg)
{
    int * sd = arg;
    shutdown(*sd, SHUT_RDWR);
}

/**
 * Code du thread server qui attend les connections des clients.
 * 
 * \param p
 *      Les arguments passés au thread.
 */
static void * threadServer(void * p)
{
    Arg * serverArg = p;
    pthread_attr_t attr;
    int errorAttrInit;
    
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    
    pthread_cleanup_push((routine)freeArg, &serverArg);
    pthread_cleanup_push((routine)close, &(serverArg->sd));
    pthread_cleanup_push((routine)myShutdown, &(serverArg->sd));
    
    /* Thread daemon */
    errorAttrInit = pthread_attr_init(&attr);
    if(errorAttrInit != 0)
    {
        fprintf(stderr, "threadServer : pthread_attr_init %s", 
                strerror(errorAttrInit));
        pthread_exit(0);
    }
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    
    pthread_cleanup_push((routine)pthread_attr_destroy, &attr);
    
    while(1)
    {
        pthread_t tid;
        int error = 1, errCreate = -1;
        Arg * connectArg = NULL;
        int acceptSd = -1;
        
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        
        /* Accepte une connection du client */
        acceptSd = accept(serverArg->sd, NULL, NULL);
        if(acceptSd == -1)
        {
            perror("threadServer : accept");
            goto out;
        }
        
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
                
        /* Arguments de la thread */
        connectArg = initArg(serverArg->dirPath, acceptSd);            
        if(connectArg == NULL)
            goto out;
        
        /* Création du thread de gestion de la connection */
        errCreate = pthread_create(&tid, &attr, threadConnection, connectArg);
        if(errCreate != 0)
        {
            fprintf(stderr, "threadServer : pthread_create %s", 
                    strerror(errCreate));
            goto out;
        }
        else
            error = 0;
        
        /* Libération des ressources */
        out :
            if(error)
            {
                if(acceptSd != -1)
                    close(acceptSd);
                
                freeArg(&connectArg);
            }
    }
    
    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    
    return NULL;
}

/**
 * Télécharge un fichier depuis un serveur.
 *
 * \param path
 * 		Le path du dossier dans lequel on stocke les fichiers téléchargés.
 * \param name
 * 		Nom du fichier à récupérer sur le serveur.
 * \param port
 *      Le port de connection au serveur.
 * \param servAddr
 *      L'adresse de connection au serveur.
 * \return -1 si la fonction échoue, 0 sinon.
 */
int downloadFile(char const * path, char const * name, int port, char * servAddr)
{
    int fd = -1;
    int sd = -1;
    size_t sizePath = -1;
    char * pathName = NULL;
    char buff[1000];
    int nbByte = 0;
    int error = 0;

	if(path == NULL || name == NULL || path[strlen(path)-1] != '/')
	{
        fprintf(stderr, "downloadFile : Paramètre incorrect \n");
        error = 1;
        goto out;
    }
    
    /* Création du path vers le fichier à stocker */
    sizePath = sizeof(char) * (strlen(path) + strlen(name) + 1);
    pathName = malloc(sizePath);
    if(pathName == NULL)
    {
        perror("downloadFile : malloc");
        error = 1;
        goto out;
    }
    
    strcpy(pathName, path);
    strcat(pathName, name);
		
    /* Création du fichier */
    fd = open(pathName, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU|S_IRWXG|S_IRWXO);
    if(fd == -1)
    {
        perror("downloadFile : open");
        error = 1;
        goto out;
    }
        		
    sd = clientInitSocket(port, servAddr);
    if(sd == -1)
    {
        error = 1;
        goto out;
    }
    
    /* Envoi du nom du fichier à télécharger au serveur */
    if(send(sd, name, FILE_SIZE * sizeof(char), 0) == -1)
    {
        perror("downloadFile : send");
        error = 1;
        goto out;
    }
    
    /* Récupération d'une partie du fichier */
    nbByte = recv(sd, buff, sizeof(char) * 1000, 0);
    
    while(nbByte != -1 && nbByte != 0)
    {
        /* Ecriture d'une partie du fichier */
        if(write(fd, buff, nbByte) != -1)
        {
            nbByte = recv(sd, buff, sizeof(char) * 1000, 0);
        }
        else
        {
            perror("downloadFile : write");
            error = 1;
            goto out;
        }
    }
					
    if(nbByte == -1)
    {
        perror("downloadFile : recv");
        error = 1;
        goto out;
    }
    
    /* Libération */
    out :
    
        if(sd != -1)
            close(sd);
    
        if(fd != -1 && error)
        {
            if(unlink(pathName) == -1)
            {
                perror("downloadFile : unlink");
            }
        }
        
        if(fd != -1)
            close(fd);
        
        if(pathName != NULL)
            free(pathName);
    
	return error != 1 ? 0 : -1;
}

/**
 * Lance un serveur de téléchargement de fichiers.
 * 
 * \param dirPath
 *      Le path du dossier contenant les fichiers à télécharger.
 * \param port
 *      Le port sur lequel le serveur se lance.
 * 
 * \return L'identifiant du thread lancé ou un identifiant invalide 
 * si la fonction échoue.
 */
serverId runServer(char const * dirPath, unsigned int port)
{
    serverId id;
    pthread_attr_t attr;
    int errorAttrInit = -1, error = 1, sd = -1;
    Arg * arg = NULL;
    
    if(dirPath == NULL || dirPath[strlen(dirPath)-1] != '/' || fileExist(dirPath, DIRECTORY) != 1)
    {
        fprintf(stderr, "runServer : mauvais paramètres ");
        goto out;
    }
    
    /* Création de la socket en attente de connection */
    sd = serverInitSocket(port, N);
    if(sd == -1)
        goto out;
    
    /* Arguments de la thread */
    arg = initArg(dirPath, sd);
    if(arg == NULL)
        goto out;
    
    /* Thread daemon */
    errorAttrInit = pthread_attr_init(&attr);
    if(errorAttrInit != 0)
    {
        fprintf(stderr, "runServer : pthread_create %s", strerror(error));
        goto out;
    }
    
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    
    /* Création du thread serveur */
    error = pthread_create(&id.tid, &attr, threadServer, arg);
    if(error != 0)
        fprintf(stderr, "runServer : pthread_create %s", strerror(error));
    
    /* Libération des ressources */
    out :
        if(error)
        {
            freeArg(&arg);
            if(sd != -1)
                close(sd);
        }
        
        if(errorAttrInit == 0)
            pthread_attr_destroy(&attr);
    
    id.isValid = error == 0;
    
	return id;
}

/**
 * Stop le serveur de téléchargement de fichiers.
 * 
 * \param dirPath
 *      Le path du dossier contenant les fichiers à télécharger.
 * \param port
 *      Le port sur lequel le serveur se lance.
 * 
 * \return 0 si le serveur a pu être stoppé -1 sinon.
 */
int stopServer(serverId * id)
{
    int retVal = 0, error = 1;
    
    if(id->isValid)
    {
        error = pthread_cancel(id->tid);
        if(error != 0)
        {
            fprintf(stderr, "stopServer : pthread_cancel %s \n", 
                    strerror(error));
            retVal = -1;
        }
        else
        {
            id->isValid = 0;
        }
    }
    
    return retVal;
}
