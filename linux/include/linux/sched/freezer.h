/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_FREEZER_H
#define _LINUX_SCHED_FREEZER_H

#include <linux/sched.h>

//struct task_struct;

// static inline int freezer_task(struct task_struct *p)
// {
//	return freezer_policy(p->policy);
// }

/*
 * define FREEZER_TIMESLICE so that every task receives
 * a time slice of 100 milliseconds
 */
#define FREEZER_TIMESLICE       (100 * HZ / 1000)

#endif /* _LINUX_SCHED_FREEZER_H */