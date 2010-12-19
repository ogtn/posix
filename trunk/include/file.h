#ifndef H_FILE
#define H_FILE
    pthread_t runServer(char const * dirPath, unsigned int port);

    int downloadFile(char const * path, char * const name, int port, char * servAddr);

#endif
