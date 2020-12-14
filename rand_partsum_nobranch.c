#include <stdlib.h>
#include <stdint.h>
#ifdef __GNUC__
#define NOINLINE __attribute((noinline))
#define FORCE_INLINE __attribute__((always_inline))
#endif
#define LOOPS 100000

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

static inline FORCE_INLINE unsigned step2(unsigned x, unsigned y) {
	return x * (1U ^ ((x - y) >> 31));
}

long long rand_partsum(unsigned N, unsigned limit) {
	long long sum = 0;
	unsigned vec[N];

	for (unsigned i = 0; i < N; i++) {
		vec[i] = i + 1;
	}
	random_shuffle(vec, N);

	for (int k = 0; k < LOOPS; k++) {
		for (unsigned i = 0; i < N; i++) {
			sum += step2(vec[i], limit);
		}
	}
	return sum;
}

long long expected(unsigned n, unsigned limit) {
	unsigned long long N = n;
	unsigned long long L = limit;
	return (((L+N)*(N-L+1))/2)*LOOPS;
}

int main(int argc, char** argv) {
	long long sum;
	unsigned N = atoi(argc > 1 ? argv[1] : "");
	if (!N)
		N = 5000;

	sum = rand_partsum(N, N/2);
	return expected(N, N/2) == sum ? 0 : 1;
}
