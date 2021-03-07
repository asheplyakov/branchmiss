#include <stdlib.h>
#include <stdio.h>
#define LOOPS 100000
#define NOINLINE __attribute__((noinline))

long long NOINLINE partsum(const unsigned *const vec, size_t len, unsigned limit) {
	long long sum = 0;
	for (int k = 0; k < LOOPS; k++) {
		for (size_t i = 0; i < len; i++) {
			if (vec[i] >= limit) {
				sum += vec[i];
			}
		}
	}
	return sum;
}

void NOINLINE random_shuffle(unsigned *const arr, unsigned N) {
	unsigned i, j;
	unsigned tmp;

	for (i = N - 1; i > 0; i--) {
		j = (unsigned) (drand48()*(i+1));
		tmp = arr[j];
		arr[j] = arr[i];
		arr[i] = tmp;
	}
}

long long NOINLINE expected(unsigned n, unsigned limit) {
	unsigned long long N = n;
	unsigned long long L = limit;
	return (((L+N)*(N-L+1))/2)*LOOPS;
}

int NOINLINE benchmark(unsigned N, unsigned limit, int randomize, const char *name) {
	int ret = 0;
	long long s, s_expected;
	unsigned vec[N];
	for (unsigned i = 0; i < N; i++) {
		vec[i] = i + 1;
	}
	if (randomize) {
		random_shuffle(vec, N);
	}
	s = partsum(vec, N, limit);
	s_expected = expected(N, limit);
	if (s != s_expected) {
		fprintf(stderr,
			"*** Error: %s: N: %u, limit: %u, expected: %lld, actual: %lld\n",
			name, N, limit, s_expected, s);
		ret = 1;
	}
	return ret;
}

int NOINLINE b_part_ordered(unsigned N) {
	return benchmark(N, N/2, 0, "part_ordered");
}

int NOINLINE b_all_ordered(unsigned N) {
	return benchmark(N, 0, 0, "all_ordered");
}

int NOINLINE b_part_random(unsigned N) {
	return benchmark(N, N/2, 1, "part_random");
}

int NOINLINE b_all_random(unsigned N) {
	return benchmark(N, 0, 1, "all_random");
}

int main(int argc, char **argv) {
	int err = 0;
	unsigned N = atoi(argc > 1 ? argv[1] : "");
	if (!N) {
		N = 5000;
	}
	err += b_all_ordered(N);
	err += b_all_random(N);
	err += b_part_ordered(N);
	err += b_part_random(N);
	return err;
}

/*  
 * gcc-8 -O0 -g -fno-omit-frame-pointer -static -o randsum_both randsum_both.c
 *
 * perf record -o bmiss.data -g --call-graph=fp --event=branch-miss ./randsum_both
 * perf script -i bmiss.data > randsum_both_branchmiss.txt
 * stackcollapse-perf.pl < randsum_both_branchmiss.txt > randsum_both_branchmiss.stacks
 * flamegraph.pl --title "Branch misses" randsum_both_branchmiss.stacks > randsum_both_branchmiss.svg
 *
 * perf record -o cycles.data -g --call-graph=fp ./randsum_both
 * perf script -i cycles.data > randsum_both_cycles.txt
 * stackcollapse-perf.pl < randsum_both_cycles.txt > randsum_both_cycles.stacks
 * flamegraph.pl --title "Cycles" randsum_both_cycles.stacks > randsum_both_cycles.svg
 */
