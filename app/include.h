#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <stdint.h>

/*****************************************
* Structs/Variables
******************************************/

typedef struct payload_info {
	char *string;
	//size_t size;
	off_t offset;
	void *loc_in_mem;
    int target_fd;
    int input_fd;
    bool append;

} PayloadInfo;


/*****************************************
* Functions
******************************************/
void printHelp ();
void printError (int error);
int getOpt(PayloadInfo *, int, char * argv[]);

