#define _POSIX_SOURCE 1

#include <stdlib.h>
#include "file.h"

int main(int argc, char** argv)
{
	downloadFile("/tmp/client/", "test", 15000, "127.0.0.1");
    
    return EXIT_SUCCESS;
}
