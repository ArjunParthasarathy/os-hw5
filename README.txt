Arjun Parthasarathy, Kshitig Seth, Veer Prasad
avp2145, [insert], [insert]

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