#define _POSIX_SOURCE 1

#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "file.h"
#include <stdlib.h>

#define NB_DOWNLOAD 2

int testDownload();
int testStopServer();

int main(void)
{
    printf("=== testDownload === \n");
    if(testDownload())
        fprintf(stderr, "testDownload OK \n");
    else
        fprintf(stderr, "testDownload FAIL \n");
    printf("==================== \n");
    puts("");
    printf("=== testStopServer === \n");
    if(testStopServer())
        fprintf(stderr, "testStopServer OK \n");
    else
        fprintf(stderr, "testStopServer FAIL \n");
    printf("====================== \n");
    
    return 0;
}

/**
 * Test du téléchargement de fichiers en parallèle.
 */
int testDownload()
{
    serverId id;
    int retVal = 0, cpt = 0;
    char tab[NB_DOWNLOAD][6] = {"test", "test2"};
    char path[21];
    
    /* Suppression des fichiers téléchargés */
    for(cpt=0; cpt<NB_DOWNLOAD; cpt++)
    {
        strcpy(path, "test/clientDir/");
        strcat(path, tab[cpt]);
        unlink(path);
    }
    
    /* Lancement du serveur */
    id = runServer("test/serverDir/", 58998);
    if(id.isValid == 0)
        goto out;
    
    /* Téléchargement des fichiers */
    for(cpt=0; cpt<NB_DOWNLOAD; cpt++)
    {
        int pid = fork();
        
        if(pid == 0)
        {
            if(downloadFile("test/clientDir/", tab[cpt], 58998, "127.0.0.1") != -1)
            {
                printf("'%s' est téléchargé\n", tab[cpt]);
                exit(1);
            }
            else
            {
                fprintf(stderr, "testDownload : downloadFile \n");
                exit(0);
            }
        }
        else if(pid == -1)
        {
            perror("testDownload : fork");
            goto out;
        }
    }
    
    /* Vérification de la bonne fin du téléchargement */
    retVal = 1;
    for(cpt=0; cpt<NB_DOWNLOAD; cpt++)
    {
        int status;
        wait(&status);
        
        if(retVal && WIFEXITED(status))
        {
            retVal |= WEXITSTATUS(status);
        }
    }
    
    /* Fermeture du serveur avant la fin des téléchargements */
    out :
        retVal &= stopServer(&id) == 0;
    
    sleep(5);
    
    return retVal;
}

/**
 * Test du téléchargement de fichiers en parallèle. Et fermeture du
 * serveur avant la terminaison du téléchargement. Les fichiers doivent 
 * alors être téléchargés avant la fin du serveur
 */
int testStopServer()
{
    serverId id;
    int retVal = 0, cpt = 0;
    char tab[NB_DOWNLOAD][6] = {"test", "test2"};
    char path[21];
    
    /* Suppression des fichiers téléchargés */
    for(cpt=0; cpt<NB_DOWNLOAD; cpt++)
    {
        strcpy(path, "test/clientDir/");
        strcat(path, tab[cpt]);
        unlink(path);
    }
    
    /* Lancement du serveur */
    id = runServer("test/serverDir/", 58999);
    if(id.isValid == 0)
        goto out;
    
    /* Téléchargement des fichiers */
    for(cpt=0; cpt<NB_DOWNLOAD; cpt++)
    {
        int pid = fork();
        
        if(pid == 0)
        {
            if(downloadFile("test/clientDir/", tab[cpt], 58999, "127.0.0.1") != -1)
            {
                printf("'%s' est téléchargé\n", tab[cpt]);
                exit(1);
            }
            else
            {
                fprintf(stderr, "testDownload : downloadFile \n");
                exit(0);
            }
        }
        else if(pid == -1)
        {
            perror("testDownload : fork");
            goto out;
        }
    }
    
    /* Fermeture du serveur avant la fin des téléchargements */
    if(stopServer(&id) == -1)
        goto out;
    
    /* Vérification de l'échec d'un nouveau téléchargement */
    if(downloadFile("test/clientDir/", "test", 58999, "127.0.0.1") != -1)
        goto out;
    
    /* Vérification de la bonne fin du téléchargement */
    retVal = 1;
    for(cpt=0; cpt<NB_DOWNLOAD; cpt++)
    {
        int status;
        
        wait(&status);
        
        if(retVal && WIFEXITED(status))
        {
            retVal |= WEXITSTATUS(status);
        }
    }
    
    out:
       retVal &= stopServer(&id) == -1 ? 0 : 1;
    
    return retVal;
}

