#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <errno.h>
#include <string.h>

// Define our custom scheduling policy
#ifndef SCHED_FREEZER
#define SCHED_FREEZER 7
#endif

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
    int policy = 0; // Default to normal scheduling
    int set_scheduler = 0;

    // Parse command line arguments
    if (argc < 2) {
        fprintf(stderr, "Usage: ./fibonacci [number] [policy]\n");
        fprintf(stderr, "Policies: 0=Default, 7=FREEZER\n");
        exit(1);
    }

    // Get the Fibonacci number to calculate
    n = atoi(argv[1]);

    // Get the scheduling policy if provided
    if (argc >= 3) {
        policy = atoi(argv[2]);
        if (policy == SCHED_FREEZER) {
            set_scheduler = 1;
        } else if (policy != 0) {
            fprintf(stderr, "Only FREEZER policy (7) is supported directly\n");
            fprintf(stderr, "For FIFO, use 'chrt --fifo' when launching\n");
            exit(1);
        }
    }

    // Only try to set scheduler for FREEZER policy
    if (set_scheduler) {
        ret = sched_setscheduler(0, policy, &params);
        if (ret) {
            perror("Error setting scheduler policy");
            fprintf(stderr, "Failed to set scheduling policy %d: %s\n", 
                   policy, strerror(errno));
            exit(1);
        }
    }

    // Calculate the Fibonacci number
    return fib(n);
}