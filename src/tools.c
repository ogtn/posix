#define _POSIX_SOURCE 1
#define _BSD_SOURCE

#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <signal.h>
#include <arpa/inet.h>
 
#include "tools.h"

/**
 * Crée initialise et connecte une socket TCP client à une socket TCP
 * serveur.
 * 
 * \param port
 * 		Le port sur laquelle la socket TCP doit se connecter.
 * \param servAddr
 * 		L'adresse de la machine serveur sur laquelle la socket doit se 
 * 		connecter.
 * 
 * \return -1 si la fonction échoue, le descripteur de la socket sinon.
 */
int clientInitSocket(unsigned int port, char const *servAddr)
{
	int sd = -1, error = 1;
    struct sockaddr_in addrIn;
    struct sockaddr * addr = (struct sockaddr *) &addrIn;
    struct hostent *hp = gethostbyname(servAddr);
	
    if(hp != NULL)
    {   
		memset(&addrIn, 0, sizeof(addrIn));
		memcpy(hp->h_addr_list[0], &addrIn.sin_addr, hp->h_length);
		
		if(port > 0 && servAddr != NULL)
		{
			/* IPV4 */
			addrIn.sin_family = AF_INET;
			/* Port de connection au serveur */
			addrIn.sin_port = htons(port);
            
			/* Création de la socket */
			if((sd = socket(AF_INET, SOCK_STREAM, 0)) >= 0) 
			{
				/* Connection à la socket */	
				if(connect(sd, addr, sizeof(struct sockaddr_in)) == 0)
				{
					error = 0;
				}
				else
				{
					perror("clientInitSocket : connect");
				}
			}
			else
			{
				perror("clientInitSocket : socket");
			}
			
			if(error && sd != -1)
			{
				if(shutdown(sd, SHUT_RDWR) == -1)
				{
					perror("clientInitSocket : shutdown");
				}
				
				if(close(sd) == -1)
				{
					perror("clientInitSocket : close");
				}
				sd = -1;
			}
		}
		else
		{
			fprintf(stderr, "clientInitSocket : Paramètre incorrect \n");
		}
	}
	else
	{
		fprintf(stderr, "clientInitSocket : gethostbyname");
	}
	
	return sd;
}

/**
 * Crée initialise et attache une socket TCP en attente de connection.
 * 
 * \param port
 * 		Le port sur lequel la socket TCP écoutera.
 * \param backlog
 * 		Longueur maximale pour la file des connexions en attente pour la
 * 		socket.
 * 
 * \return -1 si la fonction échoue, le descripteur de la socket sinon.
 */
int serverInitSocket(unsigned int port, unsigned int backlog)
{
	int sd = -1, error = 1;
	
    struct sockaddr_in addrIn;
    struct sockaddr * addr = (struct sockaddr *)&addrIn;
    
    /* Reset à 0 de l'adresse */
    memset(&addrIn, 0, sizeof(struct sockaddr_in));
    /* IPV4 */
    addrIn.sin_family = AF_INET;
    /* La socket peut-être associée à n'importe quelle adresse IP 
     * de la machine locale (s'il en existe plusieurs) */
    addrIn.sin_addr.s_addr = htonl(INADDR_ANY);
    /* Port sur lequel écoute le serveur */
    addrIn.sin_port = htons(port);
    
    /* Création de la socket */
    if((sd = socket(AF_INET, SOCK_STREAM, 0)) != -1)
    {
        int yes = 1;
        /* Autorise la réutilisation d'adresses */
        setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        
        /* Attache et nomme la socket */
        if(bind(sd, addr, sizeof(struct sockaddr_in)) == 0)
        {
            /* Attente des connexions sur une socket */
            if(listen(sd, backlog) == 0)
            {
                error = 0;
            }
            else
            {
                perror("serverInitSocket : listen");
            }
        }
        else
        {
            perror("serverInitSocket : bind");
        }
    }
    else
    {
        perror("serverInitSocket : socket");
    }
    
    if(error && sd != -1)
    {
        if(close(sd) == -1)
        {
            perror("serverInitSocket : close");
        }
        sd = -1;
    }
	
	return sd;	
}

/**
 * Détermine si un fichier ou un répertoire existe.
 * 
 * \param path
 * 		Le path vers le fichier à tester.
 * \param type
 * 		Le type de fichier à tester : répertoire ou fichier régulier.
 * 
 * \return 1 si le fichier est du type demandé et existe, 0 sinon.
 */
int fileExist(char const * path, enum FileType type)
{
	int returnValue = 0;
	
	struct stat buff;
	
	if(stat(path, &buff) == 0)
	{
		if(type == DIRECTORY && S_ISDIR(buff.st_mode))
		{
			returnValue = 1;
		}
		else if(type == REGULAR_FILE && S_ISREG(buff.st_mode))
		{
			returnValue = 1;
		}	
	}
		
	return returnValue;
}

/**
 * Convertit une chaine de caractères contenant un long en long.
 * 
 * \param str
 * 		La chaine de caractère à convertir.
 * 
 * \return Le résultat de la conversion ou -1 si elle échoue.
 */
long strToLong(char const * str)
{
	char * endptr = NULL;
	long val = -1;
	
	/* Pour distinguer la réussite/échec après l’appel */
	errno = 0;
	val = strtol(str, &endptr, 10);
	
	/* Vérification de certaines erreurs possibles */
	if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
		|| (errno != 0 && val == 0)) 
	{
		perror("strToLong : strtol");
		val = -1;
	}
	else if (endptr == str) 
	{
		fprintf(stderr, "strToLong : Pas de chiffre trouvé \n");
		val = -1;
	}
	/* Overflow */
	else if(val == LONG_MIN || val == LONG_MAX) 
	{
		perror("strToLong : Dépassement");
	}
	
	return val;
}

/**
 * Récupère l'adresse IP de la 1er interface du PC.
 * 
 * \param address
 *      L'adresse sui sera rempli par la fonction.
 * 
 * \return -1 si la fonction échoue, 0 sinon.
 */
int ipAddress(char address[16])
{
	char s[256];
	struct hostent * host = NULL;
    
	if(address == NULL)
		return -1;

	if(gethostname(s, 256) == -1)
	{
		perror("ipAddress : gethostname");
		return -1;
	}

	host = gethostbyname(s);
	if(host == NULL)
	{
		perror("ipAddress : gethostbyname");
		return -1;
	}

	strcpy(address, inet_ntoa(*(struct in_addr *)host->h_addr));

	return 0;
}

