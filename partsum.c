#include <stdlib.h>
#define LOOPS 100000

long long partsum(unsigned N, unsigned limit) {
	long long sum = 0;
	unsigned vec[N];

	for (unsigned i = 0; i < N; i++) {
		vec[i] = i + 1;
	}
	for (int k = 0; k < LOOPS; k++) {
		for (unsigned i = 0; i < N; i++) {
			if (vec[i] >= limit) {
				sum += vec[i];
			}
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

	sum = partsum(N, N/2);
	return expected(N, N/2) == sum ? 0 : 1;
}
