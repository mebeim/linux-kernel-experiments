/**
 * C helper program to automate the usage of the kernel function tracer with
 * minimal trace output noise. This file is intended to be manually edited to
 * add the code that needs tracing and then compiled and run to generate and
 * dump a trace to standard output.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/syscall.h>

#define TRACEFS_PATH "/sys/kernel/tracing"

#define errf_exit(...) do { \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, ": errno %d %s\n", errno, strerror(errno)); \
	exit(EXIT_FAILURE); \
} while (0)

static int tracing_on_fd = -1;

static void open_write_close(const char *path, const char *data) {
	ssize_t sz = strlen(data);
	int fd;

	fd = open(path, O_WRONLY);
	if (fd == -1)
		errf_exit("open \"%s\" failed", path);

	if (write(fd, data, sz) != sz)
		errf_exit("write to \"%s\" failed", path);

	close(fd);
}

static void set_tracer(const char *name) {
	open_write_close(TRACEFS_PATH "/current_tracer", name);
}

static void set_filter(const char *name) {
	open_write_close(TRACEFS_PATH "/set_ftrace_filter", name);
}

static void set_pid(int pid) {
	char buf[32];

	if (sprintf(buf, "%d", pid) < 0)
		errf_exit("sprintf failed");

	open_write_close(TRACEFS_PATH "/set_ftrace_pid", buf);
}

static void start_tracing(void) {
	if (write(tracing_on_fd, "1", 1) != 1)
		errf_exit("write to \"" TRACEFS_PATH "/tracing_on\" failed");
}

static void stop_tracing(void) {
	if (write(tracing_on_fd, "0", 1) != 1)
		errf_exit("write to \"" TRACEFS_PATH "/tracing_on\" failed");
}

static void clear_trace(void) {
	open_write_close(TRACEFS_PATH "/trace", "\n");
}

static void dump_trace(void) {
	char buf[0x100];
	FILE * fp;
	size_t n;

	fp = fopen(TRACEFS_PATH "/trace", "r");
	if (!fp)
		errf_exit("fopen \"" TRACEFS_PATH "/trace\" failed");

	while ((n = fread(buf, 1, sizeof(buf), fp)) != 0)
		fwrite(buf, 1, n, stdout);

	fclose(fp);
}

int main(void) {
	tracing_on_fd = open(TRACEFS_PATH "/tracing_on", O_WRONLY);
	if (tracing_on_fd == -1)
		errf_exit("open \"" TRACEFS_PATH "/tracing_on\" failed");

	stop_tracing();
	clear_trace();

	set_tracer("function");
	set_pid(getpid());

	/*
	 * A filter can also be used to trigger trace start/stop upon a specific
	 * function call, which can be pretty useful. See the README in tracefs.
	 */
	set_filter("\n");

	/* Preparation steps that don't need to be traced here */

	start_tracing();

	/* Have fun here */

	stop_tracing();
	dump_trace();
	return 0;
}
