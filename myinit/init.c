#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <linux/reboot.h>

#define LOG_PREFIX "[init] "
#define log(msg) fprintf(stderr, LOG_PREFIX msg)
#define logf(fmt, ...) fprintf(stderr, LOG_PREFIX fmt, __VA_ARGS__)
#define log_perror(msg) perror(LOG_PREFIX msg)

struct mount {
	char *src;
	char *target;
	char *fs_type;
	int   flags;
	void *data;
};

const struct mount mounts[] = {
	{.src="nodev", .target="/proc", .fs_type="proc", .flags=0, .data=NULL},
	{.src="nodev", .target="/sys", .fs_type="sysfs", .flags=0, .data=NULL},
	{.src="nodev", .target="/sys/kernel/debug", .fs_type="debugfs", .flags=0, .data=NULL},
};

extern int reboot(int);

void do_mounts(void) {
	unsigned i;

	for (i = 0; i < sizeof mounts / sizeof mounts[0]; i++) {
		const struct mount m = mounts[i];
		int res;

		res = mkdir(m.target, 0555);
		if (res != 0 && errno != EEXIST) {
			logf("Failed to create %s: %s\n", m.target, strerror(errno));
			return;
		}

		res = mount(m.src, m.target, m.fs_type, m.flags, m.data);
		if (res != 0)
			logf("Failed to mount %s at %s: %s\n", m.fs_type, m.target, strerror(errno));
	}
}

void open_console(void) {
	if (!freopen("/dev/console", "r", stdin))
		log_perror("Failed to open /dev/console as stdin");
}

void fork_into_shell_and_wait(void) {
	pid_t child;
	int wstatus;

	child = vfork();
	if (child == -1) {
		log_perror("vfork failed");
		return;
	}

	if (child == 0) {
		// execl("/bin/sh", "sh", "+m", NULL);
		execl("/linuxrc", "sh", "+m", NULL);
		log_perror("Child shell execl failed");
		_exit(69);
	}

	if (wait(&wstatus) == -1) {
		log_perror("Child shell wait failed");
		return;
	}

	if (WIFEXITED(wstatus))
		logf("Child exited with status %d.\n", WEXITSTATUS(wstatus));
	else if (WIFSIGNALED(wstatus))
		logf("Child terminated by signal %d.\n", WTERMSIG(wstatus));
	else
		log("WTF?\n");
}

void shutdown(void) {
	sync();
	reboot(LINUX_REBOOT_CMD_POWER_OFF);
	log_perror("reboot failed");
	_exit(1);
}

int main(void) {
	log("Yo wassup!\n");

	do_mounts();
	open_console();
	fork_into_shell_and_wait();
	shutdown();

	return 1;
}
