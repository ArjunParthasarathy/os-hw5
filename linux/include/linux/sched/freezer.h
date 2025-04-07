/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_FREEZER_H
#define _LINUX_SCHED_FREEZER_H

#include <linux/sched.h>

/*
 * define FREEZER_TIMESLICE so that every task receives
 * a time slice of 100 milliseconds
 */
#define FREEZER_TIMESLICE       (100 * HZ / 1000)

#endif /* _LINUX_SCHED_FREEZER_H */