#define _POSIX_SOURCE 1

#define N 5
#define BUFFER_SIZE 1000

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#include "file.h"
#include "tools.h"

/**
 * Paramètre des threads
 */
typedef struct Arg
{
    union
    {
        int port;                   /* Port */
        int sd;						/* Descripteur de socket */
    } integer;
    char * dirPath;					/* Path vers le dossier ou l'on 
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
 * 		Descripteur de socket traité par la thread.                     ......
 * 
 * \return Un objet alloué et initialisé ou NULL si l'allocation échoue.
 */
static Arg * initArg(char const * path, int integer)
{
	Arg * arg = malloc(sizeof(Arg));
	int error = 0;
	
	if(arg != NULL)
	{
		arg->dirPath = malloc(strlen(path) + 1);
		
		if(arg->dirPath != NULL)
		{
			strcpy(arg->dirPath, path);
			arg->integer.sd = integer;
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
	{
		freeArg(&arg);
	}
	
	return arg;
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
int downloadFile(char const * path, char * const name, int port, char * servAddr)
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
        int nbRead = read(sd, file, 1000 * sizeof(char));
        
        while(nbRead != -1 && nbRead != 0 && error == 0)
        {
            /* Envoi d'une partie du fichier */
            if(send(sd, file, nbRead, 0) != -1)
            {
                nbRead = read(sd, file, 1000 * sizeof(char));
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
	recvr = recv(arg->integer.sd, name, sizeof(char) * 256, 0);
	if(recvr != -1)
	{
		/* Création du path vers le fichier à envoyer */
		size_t size = sizeof(char) * (strlen(arg->dirPath) + strlen(name) + 1);
		char * pathName = malloc(size);
		
		if(pathName != NULL)
		{
			strcpy(pathName, arg->dirPath);
			strcat(pathName, name);
			
			printf("path %s \n", pathName);
			
			sendFile(pathName, arg->integer.sd);
			
			free(pathName);
			pathName = NULL;
			
			close(arg->integer.sd);
		}
	}
	else
	{
		perror("downloadFile : recv");
	}
	
	return NULL;
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
    int serverSd = -1;
    
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    
    /* Création de la socket en attente de connection */
    serverSd = serverInitSocket(serverArg->integer.port, N);
    
    if(serverSd == -1)
        return NULL;
    
    while(1)
    {
        pthread_t tid;
        pthread_attr_t attr;
        int error = 0, errorAttrInit = 0, errorThreadCreate = 0;
        Arg * connectArg = NULL;
        int acceptSd = -1;
        
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        
        /* Accepte une connection du client */
        acceptSd = accept(serverSd, NULL, NULL);
        if(acceptSd == -1)
        {
            perror("threadServer : accept");
            error = 1;
            goto out;
        }
        
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        
        /* Thread daemon */
        errorAttrInit = pthread_attr_init(&attr);
        if(errorAttrInit != 0)
        {
            fprintf(stderr, "pthread_attr_init %s", strerror(errorAttrInit));
            error = 1;
            goto out;
        }
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        
        /* Arguments de la thread */
        connectArg = initArg(serverArg->dirPath, acceptSd);            
        if(connectArg == NULL)
        {
            error = 1;
            goto out;
        }
        
        /* Création du thread de gestion de la connection */
        errorThreadCreate = pthread_create(&tid, &attr, threadConnection, connectArg);
        if(errorThreadCreate != 0)
        {
            fprintf(stderr, "pthread_create %s", strerror(errorThreadCreate));
            error = 1;
            goto out;
        }
        
        /* Libération des ressources */
        out :
            if(error)
            {
                if(acceptSd != -1)
                    close(acceptSd);
                
                freeArg(&connectArg);
            }
            
            if(errorAttrInit == 0)
                pthread_attr_destroy(&attr);
    }
}

/**
 * Lance un serveur de téléchargement de fichiers.
 * 
 * \param dirPath
 *      Le path du dossier contenant les fichiers à télécharger.
 * \param port
 *      Le port sur lequel le serveur se lance.
 * 
 * \return Le tid du thread lancé ou une -1 sinon.
 */
pthread_t runServer(char const * dirPath, unsigned int port)
{
    pthread_t tid = -1;
    pthread_attr_t attr;
    int errorAttrInit = 0, error = 0;
    Arg * arg = NULL;
    
    if(dirPath == NULL || dirPath[strlen(dirPath)-1] != '/' || fileExist(dirPath, DIRECTORY) != 1)
    {
        fprintf(stderr, "runServer : mauvais paramètres ");
        error = 1;
        goto out;
    }
    
    /* Arguments de la thread */
    arg = initArg(dirPath, port);
    if(arg == NULL)
    {
        error = 1;
        goto out;
    }
    
    /* Thread daemon */
    errorAttrInit = pthread_attr_init(&attr);
    if(errorAttrInit != 0)
    {
        fprintf(stderr, "pthread_create %s", strerror(error));
        error = 1;
        goto out;
    }
    
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    
    /* Création du thread serveur */
    error = pthread_create(&tid, &attr, threadServer, arg);
    if(error != 0)
    {
        fprintf(stderr, "pthread_create %s", strerror(error));
        error = 1;
    }
	
    /* Libération des ressources */
    out :
        if(error)
            freeArg(&arg);
        
        if(errorAttrInit == 0)
            pthread_attr_destroy(&attr);
    
	return error ? -1 : tid;
}

int stopServer(pthread_t tid)
{
    if(tid > 0)
    {
        pthread_cancel(tid);
    }
}

int main(void)
{
    pthread_t tid = runServer("/tmp/dir/", 58998);
    
    sleep(5);
    
    if(fork() == 0)
    {
        if(fork() == 0)
        {
            downloadFile("/tmp/dir2/", "test", 58998, "127.0.0.1");    
            exit(0);
        }
        else
            if(fork() == 0)
            {
                downloadFile("/tmp/dir2/", "test2", 58998, "127.0.0.1");    
                exit(0);
            }
            else
                exit(0);
    }
    
    sleep(10);
    
    stopServer(tid);
    
    return 0;
}






















