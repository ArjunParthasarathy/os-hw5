Arjun Parthasarathy, Kshitig Seth, Veer Prasad
avp2145, ks4184, vsp2117

Part 1
------
Our trace.bt file is present in the `user/` folder. Our average completion times are listed as follows:

1 CPU, Task 1: 1447.51 ms
4 CPU, Task 1: 270.9 ms

1 CPU, Task 2: 967.84 ms
4 CPU, Task 2: 257.1 ms

Part 2
------
Our code correctly compiles and we have followed the instructions in the lab spec. 
We have placed the corresponding data structures in the following locations:

`struct freezer_entity` in `include/linux/sched.h`, 
`struct freezer_rq` in `kernel/sched/sched.h`,
`struct sched_class freezer_sched_class` in `kernel/sched/freezer.c`
`#define SCHED_FREEZER` in `include/uapi/linux/sched.h`,
`#define FREEZER_TIMESLICE` in `include/linux/sched/freezer.h`

TODO merge part3 branch into part2

Part 5
------

## Average Completion Time (ms)

| Scheduler | Task Set 1 (1 CPU) | Task Set 2 (1 CPU) | Task Set 1 (4 CPU) | Task Set 2 (4 CPU) |
|-----------|--------------------|--------------------|--------------------|--------------------|
| CFS       |     2310 ms        |     2332 ms        |    475.838 ms      |    291.354 ms      |
| FIFO      |     1527 ms        |     2016 ms        |    146.570 ms      |    167.937 ms      |
| Freezer   |     1918 ms        |     1623 ms        |    161.613 ms      |    212.832 ms      |

## Tail Completion Time (99th percentile, ms)

| Scheduler | Task Set 1 (1 CPU) | Task Set 2 (1 CPU) | Task Set 1 (4 CPU) | Task Set 2 (4 CPU) |
|-----------|--------------------|--------------------|--------------------|--------------------|
| CFS       |     5257 ms        |     5526 ms        |      2365 ms       |      1703 ms       |
| FIFO      |     5517 ms        |     2791 ms        |      1668 ms       |      1668 ms       |
| Freezer   |     5519 ms        |     5519 ms        |      1668 ms       |      1660 ms       |

## Disabling Load Balancing for FIFO

To ensure a fair comparison, I disabled load balancing for the FIFO scheduler by using CPU affinity constraints with the `taskset` command:

```bash
taskset -c 0,1,2,3 ./fibonacci $n 1