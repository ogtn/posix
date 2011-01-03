#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tools.h"
#include "client.h"
#include "locker.h"


void affichage(char *dirPath, char *fileName)
{
    char fullName[256];
    int fd;
    char c;
    
    sprintf(fullName, "%s/%s", dirPath, fileName);
    fd = open(fullName, O_RDONLY);

    printf("====Voici le contenu du fichier %s====\n", fullName);
    
    while(read(fd, &c, 1) == 1)
        putchar(c);

    printf("==========Fin du fichier %s===========\n", fullName);
    
    close(fd);
}


void ecriture(char *dirPath, char *fileName, int port)
{
    char fullName[256];
    int fd;
    char msg[256];

    sprintf(fullName, "%s/%s", dirPath, fileName);
    fd = open(fullName, O_WRONLY);
    sprintf(msg, "Le client de port %d est passé par là!\n", port);
    write(fd, msg, strlen(msg));
    close(fd);
}


int main(int argc, char** argv)
{
    int type = !QUIT;
    char cmdLine[FILE_SIZE];
	char cmd[FILE_SIZE];
    char fileName[FILE_SIZE];
    locker l;
    char * dirPath = NULL, * serverAddress;
    int serverPort = -1, clientPort = -1;
    int result = 0;
    
    if(argc == 5)
    {
        dirPath = argv[1];
        serverPort = strToLong(argv[2]);
        serverAddress = argv[3];
        clientPort = strToLong(argv[4]);
    }
    
    /* Un des paramètres passés en paramètre est incorrect */
    if(argc != 5 || serverPort < 0 || clientPort < 0 || 
		fileExist(dirPath, DIRECTORY) == 0)
	{
		fprintf(stderr, "Paramètres incorrects : ");
		fprintf(stderr, "$ client [path du répertoire client] ");
		fprintf(stderr, "[port serveur] [adresse serveur] [port client] \n");
        
        return EXIT_FAILURE;
	}
    
	/* Connection au serveur */
	if(lockerInit(&l, dirPath, serverAddress, serverPort, clientPort) == -1)
        return EXIT_FAILURE;
    
    while(type != QUIT)
    {
        printf("> ");
        fgets(cmdLine, FILE_SIZE, stdin);
        
        sscanf(cmdLine, "%s %s", cmd, fileName);
        
        cmd[FILE_SIZE -1 ] = '\0';
        fileName[FILE_SIZE -1 ] = '\0'; 
        
        if(strcmp(cmd, "lockR") == 0) 
            result = lock(&l, fileName, LOCK_READ);
        else if(strcmp(cmd, "unlock") == 0)
            result = unlock(&l, fileName);
        else if(strcmp(cmd, "lockW") == 0)
            result = lock(&l, fileName, LOCK_WRITE);
        else if(strcmp(cmd, "quit") == 0)
        {
            type = QUIT;
            lockerDestroy(&l);
            result = OK;
        }
        else
            printf("Mauvaise commande \n");
        
        switch(result)
        {
            case OK:
                if(strcmp(cmd, "lockR") == 0)
                    affichage(dirPath, fileName);
                if(strcmp(cmd, "lockW") == 0)
                    ecriture(dirPath, fileName, clientPort);
            break;
            case FILE_NOT_FOUND:
                fprintf(stderr, "'%s' n'est pas présent dans le répertoire partagé.\n", fileName);
            break;
            case FILE_IS_ALREADY_LOCKED:
                fprintf(stderr, "'%s' est déjà locké.\n", fileName);
            break;
            case FILE_IS_NOT_LOCKED:
                fprintf(stderr, "'%s' n'est pas locké.\n", fileName);
            break;
            default:
                fprintf(stderr, "Erreur non traitée : '%d' \n", result);
            break;
        }
    }
		
    return EXIT_SUCCESS;
}
