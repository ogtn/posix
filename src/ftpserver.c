#define _POSIX_SOURCE 1

#include <stdlib.h>
#include "file.h"

int main(int argc, char** argv)
{
    runServer("/tmp/serv/", 15000);
	
    return EXIT_FAILURE;
}
