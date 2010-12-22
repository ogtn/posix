#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#include "tools.h"
#include "client.h"
#include "list.h"
#include "locker.h"

int main(int argc, char** argv)
{
    int type = !QUIT;
    char cmdLine[FILE_SIZE];
	char cmd[FILE_SIZE];
    char fileName[FILE_SIZE];
    locker l;
	
    char * dirPath = "./test/serverDir";
    int port = 4242;
    
    if(argc == 3)
    {
        dirPath = argv[1];
        port = atoi(argv[2]);
    }
    
	/* Connection au serveur */
	if(lockerInit(&l, dirPath, "localhost", 10000, port) != -1)
	{            
        while(type != QUIT)
        {            
            printf("> ");
            fgets(cmdLine, FILE_SIZE, stdin);
            
            sscanf(cmdLine, "%s %s", cmd, fileName);
                
            cmd[FILE_SIZE -1 ] = '\0';
            fileName[FILE_SIZE -1 ] = '\0'; 
            
            if(strcmp(cmd, "lockR") == 0) 
            {
                if(lock(&l, fileName, LOCK_READ) == FILE_IS_ALREADY_LOCKED)
                {
					fprintf(stderr, "'rien' est déjà locké.\n");
				}
			}
            else if(strcmp(cmd, "unlock") == 0)
            {
               if(unlock(&l, fileName) == FILE_IS_NOT_LOCKED)
               {
				   fprintf(stderr, "'rien' n'est pas locké.\n");
			   }
		   }
            else if(strcmp(cmd, "lockW") == 0)
            {
                if(lock(&l, fileName, LOCK_WRITE) == FILE_IS_ALREADY_LOCKED)
                {
                    fprintf(stderr, "'rien' est déjà locké.\n");
                }
            }
            else if(strcmp(cmd, "quit") == 0)
            {
                type = QUIT;
                lockerDestroy(&l);
            }
            else
            {
				printf("Mauvaise commande \n");
                continue;
			}
        }

        return EXIT_SUCCESS;
	}

    return EXIT_FAILURE;
}
