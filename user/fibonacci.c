#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <errno.h>
#include <string.h>

unsigned long long fib(unsigned long long n)
{
    if (n <= 1)
        return n;
    else
        return fib(n - 1) + fib(n - 2);
}

int main(int argc, char **argv)
{
    struct sched_param params = {0};
    int ret;
    unsigned long long n;
    int policy = SCHED_FREEZER; // Default to FREEZER

    // Parse command line arguments
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: ./fibonacci [number] [policy]\n");
        fprintf(stderr, "Policies: 1=FIFO, 7=FREEZER, 8=HEATER\n");
        exit(1);
    }

    // Get the Fibonacci number to calculate
    n = atoi(argv[1]);

    // Get the scheduling policy if provided
    if (argc == 3) {
        policy = atoi(argv[2]);
        if (policy != SCHED_FIFO && policy != SCHED_FREEZER && policy != 8) {
            fprintf(stderr, "Invalid policy. Use 1 for FIFO, 7 for FREEZER, or 8 for HEATER\n");
            exit(1);
        }
    }

    // Set the scheduling policy
    ret = sched_setscheduler(0, policy, &params);
    if (ret) {
        perror("Error setting scheduler policy");
        fprintf(stderr, "Failed to set scheduling policy %d: %s\n", 
                policy, strerror(errno));
        exit(1);
    }

    // Calculate the Fibonacci number
    return fib(n);
}