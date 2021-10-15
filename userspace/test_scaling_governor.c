/**
 * Test the behavior of the scaling governor under high CPU load running two
 * identical tests of which the second one sleeps before each run. A scaling
 * governor which keeps a fixed CPU clock should make this program report almost
 * identical timings for the two tests.
 *
 * gcc -O3 scaling_governor_test.c -o test
 * ./test [N_RUNS] [N_CYCLES_PER_RUN] [TEST2_DELAY]
 *
 * Sparkled from: https://stackoverflow.com/q/60351509/3889449
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <sched.h>
#include <sys/types.h>

#define DEFAULT_RUNS   1000
#define DEFAULT_CYCLES 1000 * 1000
#define DEFAULT_DELAY  100 * 1000

// Don't optimize this as GCC would basically trash the whole function.
#pragma GCC push_options
#pragma GCC optimize("O0")
void __attribute__ ((noinline)) func(unsigned n) {
	double sum = 1.0;

	for (unsigned i = 0; i < n; i++)
		sum += 0.001;
}
#pragma GCC pop_options

void warmup(unsigned runs, unsigned cycles) {
	for (unsigned n = 1; n <= runs; n++)
		func(cycles);
}

double bench(unsigned n) {
	struct timespec t0, t1;

	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t0);
	func(n);
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t1);

	return (t1.tv_sec - t0.tv_sec)*1000.0L + (t1.tv_nsec - t0.tv_nsec)/1000.0L/1000.0L;
}

void setup_affinity(void) {
	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(0, &set);

	if (geteuid() == 0) {
		if (sched_setaffinity(0, sizeof(set), &set) == 0)
			puts("Affinity set to CPU #0.");
		else
			perror("sched_setaffinity");
	} else {
		puts("Running as normal user, run as root to set CPU affinity.");
	}
}

int main(int argc, char **argv) {
	unsigned runs, cycles, delay;
	double cur, tot1, tot2, min, max, avg;

	if (argc < 2 || sscanf(argv[1], "%i", &runs) != 1 || runs < 1)
		runs = DEFAULT_RUNS;

	if (argc < 3 || sscanf(argv[2], "%i", &cycles) != 1 || cycles < 1)
		cycles = DEFAULT_CYCLES;

	if (argc < 4 || sscanf(argv[3], "%i", &delay) != 1 || delay < 1)
		delay = DEFAULT_DELAY;

	setup_affinity();

	printf("Benchmarking %u runs of %u cycles each.\n", runs, cycles);
	printf("Test #1 will proceed normally.\nTest #2 will usleep(%u) before each run.\n", delay);
	fputs("Warming up... ", stdout);
	fflush(stdout);

	warmup(10, cycles);

	puts("done.\n---");

	tot1 = 0;
	min = INFINITY;
	max = -INFINITY;

	for (unsigned n = 1; n <= runs; n++) {
		cur = bench(cycles);

		tot1 += cur;
		avg = tot1 / n;
		if (cur < min) min = cur;
		if (cur > max) max = cur;

		printf("\rTest #1: tot %-9.3f  avg %-7.3f  min %-7.3f  max %-7.3f [ms]", tot1, avg, min, max);
		fflush(stdout);
	}

	putchar('\n');

	tot2 = 0;
	min = INFINITY;
	max = -INFINITY;

	for (unsigned n = 1; n <= runs; n++) {
		usleep(delay);
		cur = bench(cycles);

		tot2 += cur;
		avg = tot2 / n;
		if (cur < min) min = cur;
		if (cur > max) max = cur;

		printf("\rTest #2: tot %-9.3f  avg %-7.3f  min %-7.3f  max %-7.3f [ms]", tot2, avg, min, max);
		fflush(stdout);
	}

	puts("\n---");

	if (tot1 < tot2)
		printf("Test #2 ran ~%.3fx slower than Test #1.\n", tot2/tot1);
	else if (tot1 > tot2)
		printf("Test #1 ran ~%.3fx slower than Test #2.\n", tot1/tot2);
	else
		puts("Reality is a simulation.\n");

	if (avg < 0.5)
		puts("Such low average times are not a good indicator. You should re-run the rest with different parameters.");

	return 0;
}
