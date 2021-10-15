/**
 * Test inheriting a special privileged capability only available and then
 * dropping privileges preserving the capability (using libcap). In this case I
 * test with CAP_DAC_OVERRIDE.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/capability.h>

unsigned long get_effective_caps(void) {
	cap_t caps = cap_get_proc();
	unsigned long value = 0;
	unsigned i;

	if (caps == NULL) {
		perror("show_effective_caps: cap_get_proc() failed");
		return -1;
	}

	for (i = 0; i <= CAP_LAST_CAP; i++) {
		cap_flag_value_t v;

		if (cap_get_flag(caps, i, CAP_EFFECTIVE, &v)) {
			perror("show_effective_caps: cap_get_flag() failed");
			return -1;
		}

		value |= ((unsigned long)(v == CAP_SET ? 1 : 0) << i);
	}

	cap_free(caps);
	return value;
}

int main(void) {
	cap_t caps;
	const cap_value_t cap_list[] = {CAP_DAC_OVERRIDE, CAP_SETUID, CAP_SETGID, CAP_SETPCAP};

	printf("Initial CapEff: %016lx\n", get_effective_caps());

	if (!CAP_IS_SUPPORTED(CAP_DAC_OVERRIDE)) {
		fputs("CAP_DAC_OVERRIDE not supported!\n", stderr);
		return 1;
	}

	if ((caps = cap_get_proc()) == NULL) {
		perror("cap_get_proc() failed");
		return 1;
	}

	if (cap_clear(caps)) {
		perror("cap_clear() failed");
		return 1;
	}

	if (cap_set_flag(caps, CAP_EFFECTIVE, 4, cap_list, CAP_SET)) {
		perror("cap_set_flag() failed");
		return 1;
	}

	if (cap_set_flag(caps, CAP_INHERITABLE, 1, cap_list, CAP_SET)) {
		perror("cap_set_flag() failed");
		return 1;
	}

	if (cap_set_flag(caps, CAP_PERMITTED, 4, cap_list, CAP_SET)) {
		perror("cap_set_flag() failed");
		return 1;
	}

	if (cap_set_proc(caps)) {
		perror("cap_set_proc() failed");
		return 1;
	}

	printf("After set, CapEff: %016lx\n", get_effective_caps());

	if (prctl(PR_SET_KEEPCAPS, 1L)) {
		perror("prctl() failed");
		return 1;
	}

	if (setresgid(1000, 1000, 1000)) {
		perror("setresgid() failed");
		return 1;
	}

	if (setresuid(1000, 1000, 1000)) {
		perror("setresuid() failed");
		return 1;
	}

	printf("After dropping privs, CapEff: %016lx\n", get_effective_caps());

	if (cap_clear(caps)) {
		perror("cap_clear() failed");
		return 1;
	}

	if (cap_set_flag(caps, CAP_EFFECTIVE, 1, cap_list, CAP_SET)) {
		perror("cap_set_flag() failed");
		return 1;
	}

	if (cap_set_flag(caps, CAP_PERMITTED, 1, cap_list, CAP_SET)) {
		perror("cap_set_flag() failed");
		return 1;
	}

	if (cap_set_flag(caps, CAP_EFFECTIVE, 3, cap_list + 1, CAP_CLEAR)) {
		perror("cap_set_flag() failed");
		return 1;
	}

	if (cap_set_flag(caps, CAP_PERMITTED, 3, cap_list + 1, CAP_CLEAR)) {
		perror("cap_set_flag() failed");
		return 1;
	}

	if (cap_set_proc(caps)) {
		perror("cap_set_proc() failed");
		return 1;
	}

	puts("-------------------------");
	printf("euid=%d, egid=%d, uid=%d, gid=%d\n", geteuid(), getegid(), getuid(), getgid());
	printf("After re-setting only CAP_DAC_OVERRIDE, CapEff: %016lx\n", get_effective_caps());

	cap_free(caps);

	puts("Trying to open secret file...");

	// This file should be only readable by the owner and owned by root.
	if (fopen("secret", "r") == NULL) {
		perror("fopen() failed");
		return 1;
	}

	puts("SUCCESS!");

	return 0;
}

