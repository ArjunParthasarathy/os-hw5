
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <errno.h>

unsigned long long fib(unsigned long long n)
{
	if (n <= 1)
		return n;
	else
		return fib(n - 1) + fib(n - 2);
}

int main(int argc, char **argv)
{
	struct sched_param params = {1};
	int ret;
	unsigned long long n;

	ret = sched_setscheduler(0, SCHED_RR, &params);
	if (ret) {
		perror("Error\n");
		fprintf(stderr, "Freezer scheduling policy does not exist\n");
		exit(1);
	}

	if (argc != 2) {
		fprintf(stderr, "Usage: ./fibonacci [number]\n");
		exit(1);
	}

	n = atoi(argv[1]);
	return fib(n);
}