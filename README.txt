## This project implements a round-robin scheduler for the Linux Kernel, under a new scheduling policy called `Freezer`.


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

Part 3
------
We have implemented Freezer according to the lab specs, and it performs both work stealing (upon entering the idle task) as well as
simple round-robin scheduling with the given timeslice. To choose a compatible task for work stealing, we see if our current CPU has an empty
freezer runqueue, we know it will be scheduling the idle task, so we loop through every other CPUs runqueue (holding locks according to
double_lock_balance() accordingly) and find a task whose CPU mask allows it to run on our current CPU. We then remove that task from that other
CPU's runqueue and add it to ours.

Part 4
------
Our group was unable to write the code for this part but we designed the implementation for Heater as follows. To implement the global runqueue, 
we have a single "dispatcher" CPU (assigned to CPU 0), which has the global runqueue `heater_rq`. The pick_next_task() function would have an if-statement, where if the CPU is the dispatcher CPU we loop through all other CPUs (the worker CPUs) and add a task to their per-CPU runqueues
(holding a lock accordingly). If the current CPU is a worker CPU, we return the task at the head of its runqueue. For enqueue_task(), if the CPU is
not the dispatcher CPU, we would acquire a lock to the specified `global_enqueue_heater_queue ` in heater which is the interface between the worker CPUs and the dispatcher CPU allowing a task woken on a worker CPU to be enqueued to the global runqueue on the dispatcher CPU 
(without needing to acquire a lock on that global runqueue). The dispatcher CPU then reads all the entries in this global_enqueue_heater_queue and adds them to its `heater_rq` which is used by pick_next_task() to issue tasks to each worker CPU in every tick. 

Part 5
------

## Average Completion Time (ms)

| Scheduler | Task Set 1 (1 CPU) | Task Set 2 (1 CPU) | Task Set 1 (4 CPU) | Task Set 2 (4 CPU) |
|-----------|--------------------|--------------------|--------------------|--------------------|
| CFS       |     2310 ms        |     2332 ms        |    444 ms      |    291.354 ms      |
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

Part 6
------
We've attempted to implement SCHED_FREEZER as the default policy, but run into an issue where after we schedule the init_task and the first two
swapper tasks (kthreads with PIDs 1 and 2 respectively), no other threads get scheduled so we end up in an infinite loop scheduling these same tasks.
However, we set the policy in the init_task struct, and during SCHED_FORK, to both be SCHED_FREEZER so we have followed the lab directions.
We additionally implement wakeup_preempt() using the check_wakeup_preempt_freezer() function, which checks to see that if our currently running task
is the idle task, if we have a newly woken task of a different type then it should preempt the idle task (since the idle task should only run when no
other task exists in the runqueue).

Part 7
------
Our idle load-balancing code works similarly to the work-stealing code from Part 3: our balance_freezer() function checks that if our current runqueue
is empty (indicating the CPU will be idle), we loop through other CPUs and if they have a runqueue of more than 2 tasks (as specified in the specs), we find a task which is allowed to be migrated (with the !is_migration_disabled() function), and one which isn't required to run on that CPU (ex. a kthread like specified in the spec). If such a task is found (and its CPU mask allows it to be run on the current CPU), then we dequeue it from the other CPU's runqueue and add it to our current CPUs runqueue. Thus, we perform idle load-balancing.
