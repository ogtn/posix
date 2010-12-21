#ifndef H_FILE
#define H_FILE

    typedef struct serverId
    {
        pthread_t tid;  
        short isValid;
    } serverId;

    serverId runServer(char const * dirPath, unsigned int port);

    int downloadFile(char const * path, char * const name, int port, char * servAddr);
    
    int stopServer(serverId * id);
    
#endif
