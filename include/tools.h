#ifndef H_TOOLS
#define H_TOOLS

/*			=======================[Includes]=========================		  */
/*			=======================[Defines/Enums]====================		  */

#define FILE_SIZE 256

#ifndef h_addr
#define h_addr  h_addr_list[0]
#endif


/* Types de messages envoyés des clients au serveur */
enum msgType
{
	LOCK_READ,
	UNLOCK_READ,
    LOCK_WRITE,
    UNLOCK_WRITE,
    QUIT,
    ERROR,
    UPDATE_NEEDED,      /* La version du client n'est pas la derniere: telechargement necessaire */
    LOCKED              /* Le lock s'est bien passé */
};

/*			=======================[Structures]=======================		  */

/* Structure des messages envoyés des clients au serveur */
typedef struct messageCS
{
	char type;                  /* Type du message */
	char fileName[FILE_SIZE];   /* Nom du fichier */
	unsigned int version;   


    /* Version du fichier possédée par le client */
} messageCS;


/* Structure des messages envoyés du serveur aux clients */
typedef struct messageSC
{
    char type;                  /* Type du message, voir enum msgType */
    unsigned int port;          /* Le port du propriétaire de la derniere version du fichier */
    char addr[16];              /* L'adresse du propriétaire de la derniere version du fichier */
} messageSC;


/*			=======================[Prototypes]=======================		  */

	enum FileType
	{
		DIRECTORY, REGULAR_FILE
	};

	typedef void (*handler)(int);
    
    int clientInitSocket(unsigned int port, char const *servAddr);
    
    int serverInitSocket(unsigned int port, unsigned int backlog);
    
    int fileExist(char const * path, enum FileType type);

    long strToLong(char const * str);

    int ipAddress(char address[16]);

#endif
