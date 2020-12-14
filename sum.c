#include <stdlib.h>
#define LOOPS 100000

long long nsum(unsigned N) {
	long long sum = 0;
	unsigned vec[N];
	for (unsigned i = 0; i < N; i++) {
		vec[i] = i + 1;
	}
	for (int k = 0; k < LOOPS; k++) {
		for (unsigned i = 0; i < N; i++) {
			sum += vec[i];
		}
	}
	return sum;
}

unsigned long long expected(unsigned n) {
	unsigned long long N = n;
	return ((N*(N + 1U))/2)*LOOPS;
}

int main(int argc, char** argv) {
	long long sum = 0;
	unsigned N = atoi(argc > 1 ? argv[1] : "");
	if (!N)
		N = 5000;

	sum = nsum(N);
	return expected(N) == sum ? 0 : 1;
}
