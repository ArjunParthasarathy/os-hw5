/*
* Freezer Scheduling Class (implements round-robin)
*/
int sched_freezer_timeslice = FREEZER_TIMESLICE;
/* More than 4 hours if BW_SHIFT equals 20. */
static const u64 max_freezer_runtime = MAX_BW;
//static int do_sched_freezer_period_timer(struct freezer_bandwidth *freezer_b, int overrun);

//struct freezer_bandwidth def_freezer_bandwidth;

/* copied from RT
 * period over which we measure -rt task CPU usage in us.
 * default: 1s
 */
// int sysctl_sched_freezer_period = 1000000;

// /* copied from RT
//  * part of the period that we allow freezer tasks to run in us.
//  * default: 0.95s
//  */
// int sysctl_sched_freezer_runtime = 950000;

// #ifdef CONFIG_SYSCTL
// static int sched_freezer_handler(struct ctl_table *table, int write, void *buffer,
// 		size_t *lenp, loff_t *ppos);
// static struct ctl_table sched_freezer_sysctls[] = {
// 	{
// 		.procname       = "sched_freezer_period_us",
// 		.data           = &sysctl_sched_freezer_period,
// 		.maxlen         = sizeof(int),
// 		.mode           = 0644,
// 		.proc_handler   = sched_freezer_handler,
// 		.extra1         = SYSCTL_ONE,
// 		.extra2         = SYSCTL_INT_MAX,
// 	},
// 	{
// 		.procname       = "sched_freezer_runtime_us",
// 		.data           = &sysctl_sched_freezer_runtime,
// 		.maxlen         = sizeof(int),
// 		.mode           = 0644,
// 		.proc_handler   = sched_freezer_handler,
// 		.extra1         = SYSCTL_NEG_ONE,
// 		.extra2         = (void *)&sysctl_sched_freezer_period,
// 	},
// 	{}
// };

// static int __init sched_freezer_sysctl_init(void)
// {
// 	register_sysctl_init("kernel", sched_freezer_sysctls);
// 	return 0;
// }
// late_initcall(sched_freezer_sysctl_init);
// #endif

// static enum hrtimer_restart sched_freezer_period_timer(struct hrtimer *timer)
// {
// 	struct freezer_bandwidth *freezer_b =
// 		container_of(timer, struct freezer_bandwidth, freezer_period_timer);
// 	int idle = 0;
// 	int overrun;

// 	raw_spin_lock(&freezer_b->freezer_runtime_lock);
// 	for (;;) {
// 		overrun = hrtimer_forward_now(timer, freezer_b->freezer_period);
// 		if (!overrun)/* More than 4 hours if BW_SHIFT equals 20. */
// 			break;

// 		raw_spin_unlock(&freezer_b->freezer_runtime_lock);
// 		idle = do_sched_freezer_period_timer(freezer_b, overrun);
// 		raw_spin_lock(&freezer_b->freezer_runtime_lock);
// 	}
// 	if (idle)
// 		freezer_b->freezer_period_active = 0;
// 	raw_spin_unlock(&freezer_b->freezer_runtime_lock);

// 	return idle ? HRTIMER_NORESTART : HRTIMER_RESTART;
// }

// void init_freezer_bandwidth(struct freezer_bandwidth *freezer_b, u64 period, u64 runtime)
// {
// 	freezer_b->freezer_period = ns_to_ktime(period);
// 	freezer_b->freezer_runtime = runtime;

// 	raw_spin_lock_init(&freezer_b->freezer_runtime_lock);

// 	hrtimer_init(&freezer_b->freezer_period_timer, CLOCK_MONOTONIC,
// 		     HRTIMER_MODE_REL_HARD);
// 	freezer_b->freezer_period_timer.function = sched_freezer_period_timer;
// }

// static inline void do_start_freezer_bandwidth(struct freezer_bandwidth *freezer_b)
// {
// 	raw_spin_lock(&freezer_b->freezer_runtime_lock);
// 	if (!freezer_b->freezer_period_active) {
// 		freezer_b->freezer_period_active = 1;
// 		/*
// 		 * SCHED_DEADLINE updates the bandwidth, as a run away
// 		 * RT task with a DL task could hog a CPU. But DL does
// 		 * not reset the period. If a deadline task was running
// 		 * without an RT task running, it can cause RT tasks to
// 		 * throttle when they start up. Kick the timer right away
// 		 * to update the period.
// 		 */
// 		hrtimer_forward_now(&freezer_b->freezer_period_timer, ns_to_ktime(0));
// 		hrtimer_start_expires(&freezer_b->freezer_period_timer,
// 				      HRTIMER_MODE_ABS_PINNED_HARD);
// 	}
// 	raw_spin_unlock(&freezer_b->freezer_runtime_lock);
// }

// static void start_freezer_bandwidth(struct freezer_bandwidth *freezer_b)
// {
// 	if (!freezer_bandwidth_enabled() || freezer_b->freezer_runtime == RUNTIME_INF)
// 		return;

// 	do_start_freezer_bandwidth(freezer_b);
// }

void init_freezer_rq(struct freezer_rq *freezer_rq)
{
	// TODO we don't need to init w/ prios
	struct list_head *queue;
	queue = &freezer_rq->active;
	INIT_LIST_HEAD(queue);

#if defined CONFIG_SMP
#endif /* CONFIG_SMP */
	freezer_rq->freezer_rq_len = 0;
	raw_spin_lock_init(&freezer_rq->freezer_runtime_lock);
}

/* No group scheduling so don't need that part of RT */

#define freezer_entity_is_task(freezer_se) (1)

static inline struct task_struct *freezer_task_of(struct sched_freezer_entity *freezer_se)
{
	return container_of(freezer_se, struct task_struct, freezer);
}

static inline struct rq *rq_of_freezer_rq(struct freezer_rq *freezer_rq)
{
	return container_of(freezer_rq, struct rq, freezer);
}

static inline struct rq *rq_of_freezer_se(struct sched_freezer_entity *freezer_se)
{
	struct task_struct *p = freezer_task_of(freezer_se);

	return task_rq(p);
}

static inline struct freezer_rq *freezer_rq_of_se(struct sched_freezer_entity *freezer_se)
{
	struct rq *rq = rq_of_freezer_se(freezer_se);

	return &rq->freezer;
}

void unregister_freezer_sched_group(struct task_group *tg) { }

void free_freezer_sched_group(struct task_group *tg) { }

int alloc_freezer_sched_group(struct task_group *tg, struct task_group *parent)
{
	return 1;
}

#ifdef CONFIG_SMP

// static inline bool need_pull_freezer_task(struct rq *rq, struct task_struct *prev)
// {
// 	trace_printk("need_pull_freezer_task()\n");
// 	/* Try to pull RT tasks here if we lower this rq's prio */
// 	// We don't need freezer prios here
// 	return rq->online && rq->freezer.highest_prio.curr > prev->prio;
// }

// static inline int freezer_overloaded(struct rq *rq)
// {
// 	return atomic_read(&rq->rd->rto_count);
// }

// static inline void freezer_set_overload(struct rq *rq)
// {
// 	if (!rq->online)
// 		return;

// 	cpumask_set_cpu(rq->cpu, rq->rd->rto_mask);
// 	/*
// 	 * Make sure the mask is visible before we set
// 	 * the overload count. That is checked to determine
// 	 * if we should look at the mask. It would be a shame
// 	 * if we looked at the mask, but the mask was not
// 	 * updated yet.
// 	 *
// 	 * Matched by the barrier in pull_freezer_task().
// 	 */
// 	smp_wmb();
// 	atomic_inc(&rq->rd->rto_count);
// }

// static inline void freezer_clear_overload(struct rq *rq)
// {
// 	if (!rq->online)
// 		return;

// 	/* the order here really doesn't matter */
// 	atomic_dec(&rq->rd->rto_count);
// 	cpumask_clear_cpu(rq->cpu, rq->rd->rto_mask);
// }

static inline int has_pushable_tasks_freezer(struct rq *rq)
{
	// return !plist_head_empty(&rq->freezer.pushable_tasks);
	return 0;
}

// static DEFINE_PER_CPU(struct balance_callback, freezer_push_head);
// static DEFINE_PER_CPU(struct balance_callback, freezer_pull_head);

// static void push_freezer_tasks(struct rq *);
// static void pull_freezer_task(struct rq *);

/* push eligible tasks to CPU w/ lowest num of tasks - we don't need this auto load balancing
for freezer, we only need that if a CPU is idle it steals (pulls) a task
and that new tasks will choose the lowest runqueue
*/
// static inline void freezer_queue_push_tasks(struct rq *rq)
// {
// 	if (!has_pushable_tasks_freezer(rq))
// 		return;

// 	queue_balance_callback(rq, &per_cpu(freezer_push_head, rq->cpu), push_freezer_tasks);
// }

// static inline void freezer_queue_pull_task(struct rq *rq)
// {
// 	queue_balance_callback(rq, &per_cpu(freezer_pull_head, rq->cpu), pull_freezer_task);
// }

/* add task p to the list of pushable tasks from rq to other cpu rqs */
// static void enqueue_pushable_task_freezer(struct rq *rq, struct task_struct *p)
// {
// 	plist_del(&p->pushable_tasks, &rq->freezer.pushable_tasks);
// 	plist_del(&p->pushable_tasks, &rq->freezer.pushable_tasks);
// 	/* TODO we could probably use p->prio for the node value but hardcode 0 just to be safe */
// 	plist_node_init(&p->pushable_tasks, 0);
// 	plist_add(&p->pushable_tasks, &rq->freezer.pushable_tasks);

// 	/* Update the highest prio pushable task */
// 	if (p->prio < rq->freezer.highest_prio.next)
// 		rq->freezer.highest_prio.next = p->prio;

// 	if (!rq->freezer.overloaded) {
// 		freezer_set_overload(rq);
// 		rq->freezer.overloaded = 1;
// 	}
// }

// static void dequeue_pushable_task_freezer(struct rq *rq, struct task_struct *p)
// {
// 	plist_del(&p->pushable_tasks, &rq->freezer.pushable_tasks);

// 	// TODO no prios
// 	/* Update the new highest prio pushable task */
// 	if (has_pushable_tasks_freezer(rq)) {
// 		p = plist_first_entry(&rq->freezer.pushable_tasks,
// 				      struct task_struct, pushable_tasks);
// 		//rq->freezer.highest_prio.next = p->prio;
// 	} else {
// 		rq->freezer.highest_prio.next = MAX_FREEZER_PRIO-1;

// 		if (rq->freezer.overloaded) {
// 			freezer_clear_overload(rq);
// 			rq->freezer.overloaded = 0;
// 		}
// 	}
// }

#else

// static inline void enqueue_pushable_task_freezer(struct rq *rq, struct task_struct *p)
// {
// }

// static inline void dequeue_pushable_task_freezer(struct rq *rq, struct task_struct *p)
// {
// }

// static inline void freezer_queue_push_tasks(struct rq *rq)
// {
// }
#endif /* CONFIG_SMP */

// static void enqueue_top_freezer_rq(struct freezer_rq *freezer_rq);
// static void dequeue_top_freezer_rq(struct freezer_rq *freezer_rq, unsigned int count);

static inline int on_freezer_rq(struct sched_freezer_entity *freezer_se)
{
	return freezer_se->on_rq;
}
// #ifdef CONFIG_UCLAMP_TASK
// /*
//  * Verify the fitness of task @p to run on @cpu taking into account the uclamp
//  * settings.
//  *
//  * This check is only important for heterogeneous systems where uclamp_min value
//  * is higher than the capacity of a @cpu. For non-heterogeneous system this
//  * function will always return true.
//  *
//  * The function will return true if the capacity of the @cpu is >= the
//  * uclamp_min and false otherwise.
//  *
//  * Note that uclamp_min will be clamped to uclamp_max if uclamp_min
//  * > uclamp_max.
//  */
// static inline bool freezer_task_fits_capacity(struct task_struct *p, int cpu)
// {
// 	unsigned int min_cap;
// 	unsigned int max_cap;
// 	unsigned int cpu_cap;

// 	/* Only heterogeneous systems can benefit from this check */
// 	if (!sched_asym_cpucap_active())
// 		return true;

// 	min_cap = uclamp_eff_value(p, UCLAMP_MIN);
// 	max_cap = uclamp_eff_value(p, UCLAMP_MAX);

// 	cpu_cap = arch_scale_cpu_capacity(cpu);

// 	return cpu_cap >= min(min_cap, max_cap);
// }
// #else
// static inline bool freezer_task_fits_capacity(struct task_struct *p, int cpu)
// {
// 	return true;
// }
// #endif /* CONFIG_UCLAMP_TASK */

// static inline u64 sched_freezer_runtime(struct freezer_rq *freezer_rq)
// {
// 	return freezer_rq->freezer_runtime;
// }

// static inline u64 sched_freezer_period(struct freezer_rq *freezer_rq)
// {
// 	return ktime_to_ns(def_freezer_bandwidth.freezer_period);
// }

typedef struct freezer_rq *freezer_rq_iter_t;

#define for_each_freezer_rq(freezer_rq, iter, rq) \
	for ((void) iter, freezer_rq = &rq->freezer; freezer_rq; freezer_rq = NULL)

#define for_each_sched_freezer_entity(freezer_se) \
	for (; freezer_se; freezer_se = NULL)

// static inline struct freezer_rq *group_freezer_rq(struct sched_freezer_entity *freezer_se)
// {
// 	return NULL;
// }

#ifdef CONFIG_SMP
// /*
//  * We ran out of runtime, see if we can borrow some from our neighbours.
//  */
// static void do_balance_runtime_freezer(struct freezer_rq *freezer_rq)
// {
// 	struct freezer_bandwidth *freezer_b = sched_freezer_bandwidth(freezer_rq);
// 	struct root_domain *rd = rq_of_freezer_rq(freezer_rq)->rd;
// 	int i, weight;
// 	u64 freezer_period;

// 	weight = cpumask_weight(rd->span);

// 	raw_spin_lock(&freezer_b->freezer_runtime_lock);
// 	freezer_period = ktime_to_ns(freezer_b->freezer_period);
// 	for_each_cpu(i, rd->span) {
// 		struct freezer_rq *iter = sched_freezer_period_freezer_rq(freezer_b, i);
// 		s64 diff;

// 		if (iter == freezer_rq)
// 			continue;

// 		raw_spin_lock(&iter->freezer_runtime_lock);
// 		/*
// 		 * Either all rqs have inf runtime and there's nothing to steal
// 		 * or __disable_runtime_freezer() below sets a specific rq to inf to
// 		 * indicate its been disabled and disallow stealing.
// 		 */
// 		if (iter->freezer_runtime == RUNTIME_INF)
// 			goto next;

// 		/*
// 		 * From runqueues with spare time, take 1/n part of their
// 		 * spare time, but no more than our period.
// 		 */
// 		diff = iter->freezer_runtime - iter->freezer_time;
// 		if (diff > 0) {
// 			diff = div_u64((u64)diff, weight);
// 			if (freezer_rq->freezer_runtime + diff > freezer_period)
// 				diff = freezer_period - freezer_rq->freezer_runtime;
// 			iter->freezer_runtime -= diff;
// 			freezer_rq->freezer_runtime += diff;
// 			if (freezer_rq->freezer_runtime == freezer_period) {
// 				raw_spin_unlock(&iter->freezer_runtime_lock);
// 				break;
// 			}
// 		}
// next:
// 		raw_spin_unlock(&iter->freezer_runtime_lock);
// 	}
// 	raw_spin_unlock(&freezer_b->freezer_runtime_lock);
// }

// /*
//  * Ensure this RQ takes back all the runtime it lend to its neighbours.
//  */
// static void __disable_runtime_freezer(struct rq *rq)
// {
// 	struct root_domain *rd = rq->rd;
// 	freezer_rq_iter_t iter;
// 	struct freezer_rq *freezer_rq;

// 	if (unlikely(!scheduler_running))
// 		return;

// 	for_each_freezer_rq(freezer_rq, iter, rq) {
// 		struct freezer_bandwidth *freezer_b = sched_freezer_bandwidth(freezer_rq);
// 		s64 want;
// 		int i;

// 		raw_spin_lock(&freezer_b->freezer_runtime_lock);
// 		raw_spin_lock(&freezer_rq->freezer_runtime_lock);
// 		/*
// 		 * Either we're all inf and nobody needs to borrow, or we're
// 		 * already disabled and thus have nothing to do, or we have
// 		 * exactly the right amount of runtime to take out.
// 		 */
// 		if (freezer_rq->freezer_runtime == RUNTIME_INF ||
// 				freezer_rq->freezer_runtime == freezer_b->freezer_runtime)
// 			goto balanced;
// 		raw_spin_unlock(&freezer_rq->freezer_runtime_lock);

// 		/*
// 		 * Calculate the difference between what we started out with
// 		 * and what we current have, that's the amount of runtime
// 		 * we lend and now have to reclaim.
// 		 */
// 		want = freezer_b->freezer_runtime - freezer_rq->freezer_runtime;

// 		/*
// 		 * Greedy reclaim, take back as much as we can.
// 		 */
// 		for_each_cpu(i, rd->span) {
// 			struct freezer_rq *iter = sched_freezer_period_freezer_rq(freezer_b, i);
// 			s64 diff;

// 			/*
// 			 * Can't reclaim from ourselves or disabled runqueues.
// 			 */
// 			if (iter == freezer_rq || iter->freezer_runtime == RUNTIME_INF)
// 				continue;

// 			raw_spin_lock(&iter->freezer_runtime_lock);
// 			if (want > 0) {
// 				diff = min_t(s64, iter->freezer_runtime, want);
// 				iter->freezer_runtime -= diff;
// 				want -= diff;
// 			} else {
// 				iter->freezer_runtime -= want;
// 				want -= want;
// 			}
// 			raw_spin_unlock(&iter->freezer_runtime_lock);

// 			if (!want)
// 				break;
// 		}

// 		raw_spin_lock(&freezer_rq->freezer_runtime_lock);
// 		/*
// 		 * We cannot be left wanting - that would mean some runtime
// 		 * leaked out of the system.
// 		 */
// 		WARN_ON_ONCE(want);
// balanced:
// 		/*
// 		 * Disable all the borrow logic by pretending we have inf
// 		 * runtime - in which case borrowing doesn't make sense.
// 		 */
// 		freezer_rq->freezer_runtime = RUNTIME_INF;
// 		freezer_rq->freezer_throttled = 0;
// 		raw_spin_unlock(&freezer_rq->freezer_runtime_lock);
// 		raw_spin_unlock(&freezer_b->freezer_runtime_lock);

// 		/* Make freezer_rq available for pick_next_task() */
// 		sched_freezer_rq_enqueue(freezer_rq);
// 	}
// }

// static void __enable_runtime_freezer(struct rq *rq)
// {
// 	freezer_rq_iter_t iter;
// 	struct freezer_rq *freezer_rq;

// 	if (unlikely(!scheduler_running))
// 		return;

// 	/*
// 	 * Reset each runqueue's bandwidth settings
// 	 */
// 	for_each_freezer_rq(freezer_rq, iter, rq) {
// 		struct freezer_bandwidth *freezer_b = sched_freezer_bandwidth(freezer_rq);

// 		raw_spin_lock(&freezer_b->freezer_runtime_lock);
// 		raw_spin_lock(&freezer_rq->freezer_runtime_lock);
// 		freezer_rq->freezer_runtime = freezer_b->freezer_runtime;
// 		freezer_rq->freezer_time = 0;
// 		freezer_rq->freezer_throttled = 0;
// 		raw_spin_unlock(&freezer_rq->freezer_runtime_lock);
// 		raw_spin_unlock(&freezer_b->freezer_runtime_lock);
// 	}
// }

// static void balance_runtime_freezer(struct freezer_rq *freezer_rq)
// {
// 	if (!sched_feat(FREEZER_RUNTIME_SHARE))
// 		return;

// 	if (freezer_rq->freezer_time > freezer_rq->freezer_runtime) {
// 		raw_spin_unlock(&freezer_rq->freezer_runtime_lock);
// 		do_balance_runtime_freezer(freezer_rq);
// 		raw_spin_lock(&freezer_rq->freezer_runtime_lock);
// 	}
// }
#else /* !CONFIG_SMP */
static inline void balance_runtime_freezer(struct freezer_rq *freezer_rq) {}
#endif /* CONFIG_SMP */

// static int do_sched_freezer_period_timer(struct freezer_bandwidth *freezer_b, int overrun)
// {
// 	int i, idle = 1, throttled = 0;
// 	const struct cpumask *span;

// 	span = sched_freezer_period_mask();

// 	for_each_cpu(i, span) {
// 		int enqueue = 0;
// 		struct freezer_rq *freezer_rq = sched_freezer_period_freezer_rq(freezer_b, i);
// 		struct rq *rq = rq_of_freezer_rq(freezer_rq);
// 		struct rq_flags rf;
// 		int skip;

// 		/*
// 		 * When span == cpu_online_mask, taking each rq->lock
// 		 * can be time-consuming. Try to avoid it when possible.
// 		 */
// 		raw_spin_lock(&freezer_rq->freezer_runtime_lock);
// 		if (!sched_feat(FREEZER_RUNTIME_SHARE) && freezer_rq->freezer_runtime != RUNTIME_INF)
// 			freezer_rq->freezer_runtime = freezer_b->freezer_runtime;
// 		skip = !freezer_rq->freezer_time && !freezer_rq->freezer_nr_running;
// 		raw_spin_unlock(&freezer_rq->freezer_runtime_lock);
// 		if (skip)
// 			continue;

// 		rq_lock(rq, &rf);
// 		update_rq_clock(rq);

// 		if (freezer_rq->freezer_time) {
// 			u64 runtime;

// 			raw_spin_lock(&freezer_rq->freezer_runtime_lock);
// 			if (freezer_rq->freezer_throttled)
// 				balance_runtime_freezer(freezer_rq);
// 			runtime = freezer_rq->freezer_runtime;
// 			freezer_rq->freezer_time -= min(freezer_rq->freezer_time, overrun*runtime);
// 			if (freezer_rq->freezer_throttled && freezer_rq->freezer_time < runtime) {
// 				freezer_rq->freezer_throttled = 0;
// 				enqueue = 1;

// 				/*
// 				 * When we're idle and a woken (rt) task is
// 				 * throttled wakeup_preempt() will set
// 				 * skip_update and the time between the wakeup
// 				 * and this unthrottle will get accounted as
// 				 * 'runtime'.
// 				 */
// 				if (freezer_rq->freezer_nr_running && rq->curr == rq->idle)
// 					rq_clock_cancel_skipupdate(rq);
// 			}
// 			if (freezer_rq->freezer_time || freezer_rq->freezer_nr_running)
// 				idle = 0;
// 			raw_spin_unlock(&freezer_rq->freezer_runtime_lock);
// 		} else if (freezer_rq->freezer_nr_running) {
// 			idle = 0;
// 			if (!freezer_rq_throttled(freezer_rq))
// 				enqueue = 1;
// 		}
// 		if (freezer_rq->freezer_throttled)
// 			throttled = 1;

// 		if (enqueue)
// 			sched_freezer_rq_enqueue(freezer_rq);
// 		rq_unlock(rq, &rf);
// 	}

// 	if (!throttled && (!freezer_bandwidth_enabled() || freezer_b->freezer_runtime == RUNTIME_INF))
// 		return 1;

// 	return idle;
// }

// static inline int freezer_se_prio(struct sched_freezer_entity *freezer_se)
// {
// 	return freezer_task_of(freezer_se)->prio;
// }

// static int sched_freezer_runtime_exceeded(struct freezer_rq *freezer_rq)
// {
// 	u64 runtime = sched_freezer_runtime(freezer_rq);

// 	if (freezer_rq->freezer_throttled)
// 		return freezer_rq_throttled(freezer_rq);

// 	if (runtime >= sched_freezer_period(freezer_rq))
// 		return 0;

// 	balance_runtime_freezer(freezer_rq);
// 	runtime = sched_freezer_runtime(freezer_rq);
// 	if (runtime == RUNTIME_INF)
// 		return 0;

// 	if (freezer_rq->freezer_time > runtime) {
// 		struct freezer_bandwidth *freezer_b = sched_freezer_bandwidth(freezer_rq);

// 		/*
// 		 * Don't actually throttle groups that have no runtime assigned
// 		 * but accrue some time due to boosting.
// 		 */
// 		if (likely(freezer_b->freezer_runtime)) {
// 			freezer_rq->freezer_throttled = 1;
// 			printk_deferred_once("sched: RT throttling activated\n");
// 		} else {
// 			/*
// 			 * In case we did anyway, make it go away,
// 			 * replenishment is a joke, since it will replenish us
// 			 * with exactly 0 ns.
// 			 */
// 			freezer_rq->freezer_time = 0;
// 		}

// 		if (freezer_rq_throttled(freezer_rq)) {
// 			sched_freezer_rq_dequeue(freezer_rq);
// 			return 1;
// 		}
// 	}

// 	return 0;
// }

// /*
//  * Update the current task's runtime statistics. Skip current tasks that
//  * are not in our scheduling class.
//  */
static void update_curr_freezer(struct rq *rq)
{
// 	struct task_struct *curr = rq->curr;
// 	struct sched_freezer_entity *freezer_se = &curr->freezer;
// 	s64 delta_exec;

// 	if (curr->sched_class != &freezer_sched_class)
// 		return;

// 	delta_exec = update_curr_common(rq);
// 	if (unlikely(delta_exec <= 0))
// 		return;

// 	if (!freezer_bandwidth_enabled())
// 		return;

// 	for_each_sched_freezer_entity(freezer_se) {
// 		struct freezer_rq *freezer_rq = freezer_rq_of_se(freezer_se);
// 		int exceeded;

// 		if (sched_freezer_runtime(freezer_rq) != RUNTIME_INF) {
// 			raw_spin_lock(&freezer_rq->freezer_runtime_lock);
// 			freezer_rq->freezer_time += delta_exec;
// 			exceeded = sched_freezer_runtime_exceeded(freezer_rq);
// 			if (exceeded)
// 				resched_curr(rq);
// 			raw_spin_unlock(&freezer_rq->freezer_runtime_lock);
// 			if (exceeded)
// 				do_start_freezer_bandwidth(sched_freezer_bandwidth(freezer_rq));
// 		}
// 	}
}

// static void
// dequeue_top_freezer_rq(struct freezer_rq *freezer_rq, unsigned int count)
// {
// }

// /* add top freezer rq to CPU runqueue */
// static void
// enqueue_top_freezer_rq(struct freezer_rq *freezer_rq)
// {
// }

static void enqueue_freezer_entity(struct sched_freezer_entity *freezer_se, unsigned int flags)
{

	struct freezer_rq *freezer_rq = freezer_rq_of_se(freezer_se);
	raw_spin_lock(&freezer_rq->freezer_runtime_lock);
	struct list_head *queue = &freezer_rq->active;
	list_add_tail(&freezer_se->run_list, queue);
	freezer_se->on_list = 1;
	freezer_se->on_rq = 1;
	
	freezer_rq->freezer_rq_len += 1;
	raw_spin_unlock(&freezer_rq->freezer_runtime_lock);
}

static void dequeue_freezer_entity(struct sched_freezer_entity *freezer_se, unsigned int flags)
{
	struct freezer_rq *freezer_rq = freezer_rq_of_se(freezer_se);
	raw_spin_lock(&freezer_rq->freezer_runtime_lock);

	if (freezer_se->on_list) {
		// list_del() maintains the order of things on the queue
		list_del_init(&freezer_se->run_list);
	}
	freezer_se->on_list = 0;
	freezer_se->on_rq = 0;
	freezer_rq->freezer_rq_len -= 1;

	raw_spin_unlock(&freezer_rq->freezer_runtime_lock);
}

/*
 * Adding/removing a task to/from rq:
 */
static void
enqueue_task_freezer(struct rq *rq, struct task_struct *p, int flags)
{
	trace_printk("enqueue_task_freezer()\n");

	struct sched_freezer_entity *freezer_se = &p->freezer;
	enqueue_freezer_entity(freezer_se, flags);

	// if (!task_current(rq, p) && p->nr_cpus_allowed > 1)
	// 	/* If we're not the currently executing task on this runqueue, 
	// 	add ourselves to list of pushable tasks */
	// 	enqueue_pushable_task_freezer(rq, p);
}

/* Remove task from runqueue */
static void dequeue_task_freezer(struct rq *rq, struct task_struct *p, int flags)
{
	trace_printk("dequeue_task_freezer()\n");
	struct sched_freezer_entity *freezer_se = &p->freezer;
	dequeue_freezer_entity(freezer_se, flags);

	//dequeue_pushable_task_freezer(rq, p);
}

/*
 * Put task to the end of the run list without the overhead of
 * dequeue followed by enqueue.
 */
static void
requeue_freezer_entity(struct freezer_rq *freezer_rq, struct sched_freezer_entity *freezer_se)
{
	if (on_freezer_rq(freezer_se)) {
		struct list_head *queue = &freezer_rq->active;
		raw_spin_lock(&freezer_rq->freezer_runtime_lock);
		
		list_move_tail(&freezer_se->run_list, queue);

		raw_spin_unlock(&freezer_rq->freezer_runtime_lock);
	}
}

static void requeue_task_freezer(struct rq *rq, struct task_struct *p)
{
	trace_printk("requeue_task_freezer()\n");
	struct sched_freezer_entity *freezer_se = &p->freezer;
	struct freezer_rq *freezer_rq;

	freezer_rq = freezer_rq_of_se(freezer_se);
	requeue_freezer_entity(freezer_rq, freezer_se);
}

/* No need for lock here b/c its called w/ rq lock by scheduler */
static void yield_task_freezer(struct rq *rq)
{
	trace_printk("yield_task_freezer()\n");
	/* current task needs to go to back of queue when it gets preempted
	and have its timeslice reset */
	// struct task_struct *p = rq->curr;
	// p->freezer.time_slice = sched_freezer_timeslice;

	// requeue_task_freezer(rq, rq->curr);
}

#ifdef CONFIG_SMP
static struct rq *find_lowest_rq_freezer(struct task_struct *task);

/* find which CPU to put this runqueue on - the one with the lowest num procs */
static int
select_task_rq_freezer(struct task_struct *p, int cpu, int flags)
{
	trace_printk("select_task_rq_freezer()\n");
	//struct task_struct *curr;

	/* the current CPU (inputted to this argument) doesn't matter
	because for Freezer we just choose the CPU with the lowest number of Freezer tasks
	(ignoring CPU affinity) */

	struct rq *rq = find_lowest_rq_freezer(p);
	BUG_ON(!rq);
	int target = cpu_of(rq);

	return target;
}

// static void check_preempt_equal_prio_freezer(struct rq *rq, struct task_struct *p)
// {
// 	/*
// 	 * Current can't be migrated, useless to reschedule,
// 	 * let's hope p can move out.
// 	 */
// 	if (rq->curr->nr_cpus_allowed == 1 ||
// 	    !cpupri_find(&rq->rd->cpupri, rq->curr, NULL))
// 		return;

// 	/*
// 	 * p is migratable, so let's not schedule it and
// 	 * see if it is pushed or pulled somewhere else.
// 	 */
// 	if (p->nr_cpus_allowed != 1 &&
// 	    cpupri_find(&rq->rd->cpupri, p, NULL))
// 		return;

// 	/*
// 	 * There appear to be other CPUs that can accept
// 	 * the current task but none can run 'p', so lets reschedule
// 	 * to try and push the current task away:
// 	 */
// 	requeue_task_freezer(rq, p, 1);
// 	resched_curr(rq);
// }

static int balance_freezer(struct rq *rq, struct task_struct *p, struct rq_flags *rf)
{
	// if (!on_freezer_rq(&p->freezer) && need_pull_freezer_task(rq, p)) {
	// 	/*
	// 	 * This is OK, because current is on_cpu, which avoids it being
	// 	 * picked for load-balance and preemption/IRQs are still
	// 	 * disabled avoiding further scheduler activity on it and we've
	// 	 * not yet started the picking loop.
	// 	 */
	// 	rq_unpin_lock(rq, rf);
	// 	pull_freezer_task(rq);
	// 	rq_repin_lock(rq, rf);
	// }

	// return sched_stop_runnable(rq) || sched_dl_runnable(rq) || sched_rt_runnable(rq) || sched_freezer_runnable(rq);
	return 0;
}
#endif /* CONFIG_SMP */

// /*
//  * Preempt the current task with a newly woken task if needed:
//  */
static void wakeup_preempt_freezer(struct rq *rq, struct task_struct *p, int flags)
{
	// if (p->prio < rq->curr->prio) {
	// 	resched_curr(rq);
	// 	return;
	// }

// #ifdef CONFIG_SMP
// 	/*
// 	 * If:
// 	 *
// 	 * - the newly woken task is of equal priority to the current task
// 	 * - the newly woken task is non-migratable while current is migratable
// 	 * - current will be preempted on the next reschedule
// 	 *
// 	 * we should check to see if current can readily move to a different
// 	 * cpu.  If so, we will reschedule to allow the push logic to try
// 	 * to move current somewhere else, making room for our non-migratable
// 	 * task.
// 	 */
// 	if (p->prio == rq->curr->prio && !test_tsk_need_resched(rq->curr))
// 		check_preempt_equal_prio_freezer(rq, p);
// #endif
}

/* NO-OP */
static inline void set_next_task_freezer(struct rq *rq, struct task_struct *p, bool first)
{
	//trace_printk("set_next_task_freezer\n");
	//struct sched_freezer_entity *freezer_se = &p->freezer;
	//struct freezer_rq *freezer_rq = &rq->freezer;

	//p->se.exec_start = rq_clock_task(rq);
	// if (on_freezer_rq(&p->freezer))
	// 	update_stats_wait_end_freezer(freezer_rq, freezer_se);

	/* The running task is never eligible for pushing */
	// dequeue_pushable_task_freezer(rq, p);

	// if (!first)
	// 	return;

	/*
	 * If prev task was freezer, put_prev_task() has already updated the
	 * utilization. We only care of the case where we start to schedule a
	 * freezer task
	 */
	// if (rq->curr->sched_class != &freezer_sched_class)
	// 	update_freezer_rq_load_avg(rq_clock_pelt(rq), rq, 0);

	// We don't want to push in this case, that can happen later
	//freezer_queue_push_tasks(rq);
}

static struct sched_freezer_entity *pick_next_freezer_entity(struct freezer_rq *freezer_rq)
{
	//trace_printk("pick_next_freezer_entity()\n");
	struct sched_freezer_entity *next = NULL;
	BUG_ON(!freezer_rq);
	raw_spin_lock(&freezer_rq->freezer_runtime_lock);
	struct list_head *queue = &freezer_rq->active;
	
	if (SCHED_WARN_ON(list_empty(queue))) {
		raw_spin_unlock(&freezer_rq->freezer_runtime_lock);
		return NULL;
	}
	next = list_entry(queue->next, struct sched_freezer_entity, run_list);
	raw_spin_unlock(&freezer_rq->freezer_runtime_lock);

	return next;
}

static struct task_struct *_pick_next_task_freezer(struct rq *rq)
{
	//trace_printk("_pick_next_task_freezer()\n");
	struct sched_freezer_entity *freezer_se;
	struct freezer_rq *freezer_rq  = &rq->freezer;
	BUG_ON(!freezer_rq);
	freezer_se = pick_next_freezer_entity(freezer_rq);
	if (unlikely(!freezer_se))
		return NULL;
	
	return freezer_task_of(freezer_se);
}

static struct task_struct *pick_task_freezer(struct rq *rq)
{
	return NULL;
}

static struct task_struct *freezer_pick_task_from_other_cpus(struct rq *rq);

static struct task_struct *pick_next_task_freezer(struct rq *rq)
{
	//trace_printk("pick_next_task_freezer()\n");
	struct task_struct *p;

	/* Nothing in queue so we steal from CPU w/ least tasks */
	if (!sched_freezer_runnable(rq)) {
		//return freezer_pick_task_from_other_cpus(rq);
		return NULL;
	}	

	p = _pick_next_task_freezer(rq);
	return p;
	//return NULL;
}

static void put_prev_task_freezer(struct rq *rq, struct task_struct *p)
{
	//struct sched_freezer_entity *freezer_se = &p->freezer;
	//struct freezer_rq *freezer_rq = &rq->freezer;

	// if (on_freezer_rq(&p->freezer))
	// 	update_stats_wait_start_freezer(freezer_rq, freezer_se);

	//update_curr_freezer(rq);

	//update_freezer_rq_load_avg(rq_clock_pelt(rq), rq, 1);

	/*
	 * The previous task needs to be made eligible for pushing
	 * if it is still active
	 */
	// if (on_freezer_rq(&p->freezer) && p->nr_cpus_allowed > 1)
	// 	enqueue_pushable_task_freezer(rq, p);
}

#ifdef CONFIG_SMP

/* returns true if we can run this task on this CPU */
static int can_pick_freezer_task(struct rq *rq, struct task_struct *p, int cpu)
{
	trace_printk("can_pick_freezer_task()\n");
	// If the task is not currently running and its CPU mask lets it run
	// on this CPU
	if (!task_on_cpu(rq, p) &&
	    cpumask_test_cpu(cpu, &p->cpus_mask))
		return 1;

	return 0;
}

/*
 * Return t
 */
static struct task_struct *freezer_pick_task_from_other_cpus(struct rq *rq)
{
	trace_printk("freezer_pick_task_from_other_cpus()\n");
	struct list_head *head;
	struct sched_freezer_entity *freezer_se;
	struct task_struct *task;
	int i = 0;
	int cpu = cpu_of(rq);
	struct rq_flags rf;
	struct rq *rq_i;

	/* Loop through all CPUs and still a task from someone (doesn't matter who) */
	/* We keep looping til we find a task on some CPU which can be run on the current CPU (based on its CPU mask) */
	for_each_cpu(i, cpu_present_mask) {
		rq_i = cpu_rq(i);
		rq_lock_irqsave(rq_i, &rf);
		head = &rq_i->freezer.active;
		struct list_head *pos = NULL;
		list_for_each(pos, head) {
			freezer_se = list_entry(pos, struct sched_freezer_entity, run_list);
			task = freezer_task_of(freezer_se);
			if (can_pick_freezer_task(rq_i, task, cpu)) {
				rq_unlock_irqrestore(rq_i, &rf);
				return task;
			}	
		}
		rq_unlock_irqrestore(rq_i, &rf);
	}

	/* No other task found on any other CPU :( */
	return NULL;
}

static DEFINE_PER_CPU(cpumask_var_t, local_cpu_mask);

/* find the runqueue with the lowest number of tasks */
static struct rq *find_lowest_rq_freezer(struct task_struct *task)
{
	trace_printk("find_lowest_rq_freezer()\n");
	//struct sched_domain *sd;
	//struct cpumask *lowest_mask = this_cpu_cpumask_var_ptr(local_cpu_mask);
	//int this_cpu = smp_processor_id();
	int cpu      = task_cpu(task);
	//int ret;

	/* Make sure the mask is initialized first */
	// if (unlikely(!lowest_mask))
	// 	return task_rq(task);

	if (task->nr_cpus_allowed == 1)
		return task_rq(task); /* No other targets possible */

	/* TODO do we really need this lock? */
	//rcu_read_lock();
	struct rq *rq_i;
	int i = 0;
	
	struct rq *min_rq = task_rq(task);
	struct rq_flags rf;
	rq_lock(min_rq, &rf);
	int min_rq_len = min_rq->freezer.freezer_rq_len;
	rq_unlock(min_rq, &rf);

	for_each_cpu(i, cpu_present_mask) {
		rq_i = cpu_rq(i);
		/* use irq b/c we're looking across cpus so have multiple interrupts */
		rq_lock_irqsave(rq_i, &rf);
		if (rq_i->freezer.freezer_rq_len < min_rq_len) {
			trace_printk("Found cpu %d w/ less tasks than curr cpu %d()\n", i, cpu);
			min_rq = rq_i;
			min_rq_len = rq_i->freezer.freezer_rq_len;
		}
		rq_unlock_irqrestore(rq_i, &rf);
	}
	//rcu_read_unlock();
	
	return min_rq;
	//return 0;
}

/* Will lock the rq it finds */
static struct rq *find_lock_lowest_rq_freezer(struct task_struct *task, struct rq *rq)
{
	trace_printk("find_lock_lowest_rq_freezer()\n");
	// for (tries = 0; tries < FREEZER_MAX_TRIES; tries++) {
	// 	cpu = find_lowest_rq_freezer(task);

	// 	if ((cpu == -1) || (cpu == rq->cpu))
	// 		break;

	// 	lowest_rq = cpu_rq(cpu);

	// 	/* if the prio of this runqueue changed, try again */
	// 	if (double_lock_balance(rq, lowest_rq)) {
	// 		/*
	// 		 * We had to unlock the run queue. In
	// 		 * the mean time, task could have
	// 		 * migrated already or had its affinity changed.
	// 		 * Also make sure that it wasn't scheduled on its rq.
	// 		 * It is possible the task was scheduled, set
	// 		 * "migrate_disabled" and then got preempted, so we must
	// 		 * check the task migration disable flag here too.
	// 		 */
	// 		if (unlikely(task_rq(task) != rq ||
	// 			     !cpumask_test_cpu(lowest_rq->cpu, &task->cpus_mask) ||
	// 			     task_on_cpu(rq, task) ||
	// 			     //!task->policy == SCHED_FREEZER ||
	// 			     is_migration_disabled(task) ||
	// 			     !task_on_rq_queued(task))) {

	// 			double_unlock_balance(rq, lowest_rq);
	// 			lowest_rq = NULL;
	// 			break;
	// 		}
	// 	}

	// 	/* try again */
	// 	double_unlock_balance(rq, lowest_rq);
	// 	lowest_rq = NULL;
	// }

	struct rq_flags rf;
	struct rq *lowest_rq = find_lowest_rq_freezer(task);
	BUG_ON(!lowest_rq);
	rq_lock(lowest_rq, &rf);

	return lowest_rq;
}

/* picks the next task from this rq we can push to other rqs */
/* basically dependent on if nr_cpus>allowed > 1 */
// static struct task_struct *pick_next_pushable_task_freezer(struct rq *rq)
// {
// 	trace_printk("pick_next_pushable_task_freezer()\n");
// 	struct task_struct *p;

// 	if (!has_pushable_tasks_freezer(rq))
// 		return NULL;

// 	p = plist_first_entry(&rq->freezer.pushable_tasks,
// 			      struct task_struct, pushable_tasks);

// 	BUG_ON(rq->cpu != task_cpu(p));
// 	BUG_ON(task_current(rq, p));
// 	BUG_ON(p->nr_cpus_allowed <= 1);

// 	BUG_ON(!task_on_rq_queued(p));
// 	//BUG_ON(!freezer_task(p));

// 	return p;
// }

// /* 
//  * Migrates a task to the CPU w/ least tasks (for freezer we don't need to do this)
//  */
// static int push_freezer_task(struct rq *rq, bool pull)
// {
// 	trace_printk("push_freezer_task()\n");
// 	struct task_struct *next_task;
// 	struct rq *lowest_rq;
// 	int ret = 0;

// 	next_task = pick_next_pushable_task_freezer(rq);
// 	if (!next_task)
// 		return 0;

// // Don't look at if migration is disabled for this assignment
// // retry:
// // 	if (is_migration_disabled(next_task)) {
// // 		struct task_struct *push_task = NULL;
// // 		int cpu;

// // 		if (!pull || rq->push_busy)
// // 			return 0;

// // 		if (rq->curr->sched_class != &freezer_sched_class)
// // 			return 0;

// // 		cpu = find_lowest_rq_freezer(rq->curr);
// // 		if (cpu == -1 || cpu == rq->cpu)
// // 			return 0;

// // 		/*
// // 		 * Given we found a CPU with lower priority than @next_task,
// // 		 * therefore it should be running. However we cannot migrate it
// // 		 * to this other CPU, instead attempt to push the current
// // 		 * running task on this CPU away.
// // 		 */
// // 		push_task = get_push_task(rq);
// // 		if (push_task) {
// // 			preempt_disable();
// // 			raw_spin_rq_unlock(rq);
// // 			stop_one_cpu_nowait(rq->cpu, push_cpu_stop,
// // 					    push_task, &rq->push_work);
// // 			preempt_enable();
// // 			raw_spin_rq_lock(rq);
// // 		}

// // 		return 0;
// // 	}

// 	if (WARN_ON(next_task == rq->curr))
// 		return 0;

// 	/* We might release rq lock */
// 	get_task_struct(next_task);

// 	/* find_lock_lowest_rq_freezer locks the rq if found */
// 	lowest_rq = find_lock_lowest_rq_freezer(next_task, rq);
// 	if (!lowest_rq) {
// 		struct task_struct *task;
// 		/*
// 		 * find_lock_lowest_rq_freezer releases rq->lock
// 		 * so it is possible that next_task has migrated.
// 		 *
// 		 * We need to make sure that the task is still on the same
// 		 * run-queue and is also still the next task eligible for
// 		 * pushing.
// 		 */
// 		task = pick_next_pushable_task_freezer(rq);
// 		if (task == next_task) {
// 			/*
// 			 * The task hasn't migrated, and is still the next
// 			 * eligible task, but we failed to find a run-queue
// 			 * to push it to.  Do not retry in this case, since
// 			 * other CPUs will pull from us when ready.
// 			 */
// 			goto out;
// 		}

// 		if (!task)
// 			/* No more tasks, just exit */
// 			goto out;

// 		/*
// 		 * Something has shifted, try again.
// 		 */
// 		// put_task_struct(next_task);
// 		// next_task = task;
// 		// goto retry;
// 	}

// 	// move this task to the other CPU
// 	deactivate_task(rq, next_task, 0);
// 	set_task_cpu(next_task, lowest_rq->cpu);
// 	activate_task(lowest_rq, next_task, 0);
// 	resched_curr(lowest_rq);
// 	ret = 1;

// 	double_unlock_balance(rq, lowest_rq);
// out:
// 	put_task_struct(next_task);

// 	return ret;
// }

// static void push_freezer_tasks(struct rq *rq)
// {
// 	/* push_freezer_task will return true if it moved an RT */
// 	while (push_freezer_task(rq, false))
// 		;
// }

#ifdef HAVE_RT_PUSH_IPI

/*
 * When a high priority task schedules out from a CPU and a lower priority
 * task is scheduled in, a check is made to see if there's any RT tasks
 * on other CPUs that are waiting to run because a higher priority RT task
 * is currently running on its CPU. In this case, the CPU with multiple RT
 * tasks queued on it (overloaded) needs to be notified that a CPU has opened
 * up that may be able to run one of its non-running queued RT tasks.
 *
 * All CPUs with overloaded RT tasks need to be notified as there is currently
 * no way to know which of these CPUs have the highest priority task waiting
 * to run. Instead of trying to take a spinlock on each of these CPUs,
 * which has shown to cause large latency when done on machines with many
 * CPUs, sending an IPI to the CPUs to have them push off the overloaded
 * RT tasks waiting to run.
 *
 * Just sending an IPI to each of the CPUs is also an issue, as on large
 * count CPU machines, this can cause an IPI storm on a CPU, especially
 * if its the only CPU with multiple RT tasks queued, and a large number
 * of CPUs scheduling a lower priority task at the same time.
 *
 * Each root domain has its own irq work function that can iterate over
 * all CPUs with RT overloaded tasks. Since all CPUs with overloaded RT
 * task must be checked if there's one or many CPUs that are lowering
 * their priority, there's a single irq work iterator that will try to
 * push off RT tasks that are waiting to run.
 *
 * When a CPU schedules a lower priority task, it will kick off the
 * irq work iterator that will jump to each CPU with overloaded RT tasks.
 * As it only takes the first CPU that schedules a lower priority task
 * to start the process, the rto_start variable is incremented and if
 * the atomic result is one, then that CPU will try to take the rto_lock.
 * This prevents high contention on the lock as the process handles all
 * CPUs scheduling lower priority tasks.
 *
 * All CPUs that are scheduling a lower priority task will increment the
 * rt_loop_next variable. This will make sure that the irq work iterator
 * checks all RT overloaded CPUs whenever a CPU schedules a new lower
 * priority task, even if the iterator is in the middle of a scan. Incrementing
 * the rt_loop_next will cause the iterator to perform another scan.
 *
 */
// static int rto_next_cpu(struct root_domain *rd)
// {
// 	int next;
// 	int cpu;

// 	/*
// 	 * When starting the IPI RT pushing, the rto_cpu is set to -1,
// 	 * rt_next_cpu() will simply return the first CPU found in
// 	 * the rto_mask.
// 	 *
// 	 * If rto_next_cpu() is called with rto_cpu is a valid CPU, it
// 	 * will return the next CPU found in the rto_mask.
// 	 *
// 	 * If there are no more CPUs left in the rto_mask, then a check is made
// 	 * against rto_loop and rto_loop_next. rto_loop is only updated with
// 	 * the rto_lock held, but any CPU may increment the rto_loop_next
// 	 * without any locking.
// 	 */
// 	for (;;) {

// 		/* When rto_cpu is -1 this acts like cpumask_first() */
// 		cpu = cpumask_next(rd->rto_cpu, rd->rto_mask);

// 		rd->rto_cpu = cpu;

// 		if (cpu < nr_cpu_ids)
// 			return cpu;

// 		rd->rto_cpu = -1;

// 		/*
// 		 * ACQUIRE ensures we see the @rto_mask changes
// 		 * made prior to the @next value observed.
// 		 *
// 		 * Matches WMB in freezer_set_overload().
// 		 */
// 		next = atomic_read_acquire(&rd->rto_loop_next);

// 		if (rd->rto_loop == next)
// 			break;

// 		rd->rto_loop = next;
// 	}

// 	return -1;
// }

// static inline bool rto_start_trylock(atomic_t *v)
// {
// 	return !atomic_cmpxchg_acquire(v, 0, 1);
// }

// static inline void rto_start_unlock(atomic_t *v)
// {
// 	atomic_set_release(v, 0);
// }

// static void tell_cpu_to_push(struct rq *rq)
// {
// 	int cpu = -1;

// 	/* Keep the loop going if the IPI is currently active */
// 	atomic_inc(&rq->rd->rto_loop_next);

// 	/* Only one CPU can initiate a loop at a time */
// 	if (!rto_start_trylock(&rq->rd->rto_loop_start))
// 		return;

// 	raw_spin_lock(&rq->rd->rto_lock);

// 	/*
// 	 * The rto_cpu is updated under the lock, if it has a valid CPU
// 	 * then the IPI is still running and will continue due to the
// 	 * update to loop_next, and nothing needs to be done here.
// 	 * Otherwise it is finishing up and an ipi needs to be sent.
// 	 */
// 	if (rq->rd->rto_cpu < 0)
// 		cpu = rto_next_cpu(rq->rd);

// 	raw_spin_unlock(&rq->rd->rto_lock);

// 	rto_start_unlock(&rq->rd->rto_loop_start);

// 	if (cpu >= 0) {
// 		/* Make sure the rd does not get freed while pushing */
// 		sched_get_rd(rq->rd);
// 		irq_work_queue_on(&rq->rd->rto_push_work, cpu);
// 	}
// }

// /* Called from hardirq context */
// void rto_push_irq_work_func(struct irq_work *work)
// {
// 	struct root_domain *rd =
// 		container_of(work, struct root_domain, rto_push_work);
// 	struct rq *rq;
// 	int cpu;

// 	rq = this_rq();

// 	/*
// 	 * We do not need to grab the lock to check for has_pushable_tasks_freezer.
// 	 * When it gets updated, a check is made if a push is possible.
// 	 */
// 	if (has_pushable_tasks_freezer(rq)) {
// 		raw_spin_rq_lock(rq);
// 		while (push_freezer_task(rq, true))
// 			;
// 		raw_spin_rq_unlock(rq);
// 	}

// 	raw_spin_lock(&rd->rto_lock);

// 	/* Pass the IPI to the next rt overloaded queue */
// 	cpu = rto_next_cpu(rd);

// 	raw_spin_unlock(&rd->rto_lock);

// 	if (cpu < 0) {
// 		sched_put_rd(rd);
// 		return;
// 	}

// 	/* Try the next RT overloaded CPU */
// 	irq_work_queue_on(&rd->rto_push_work, cpu);
// }
#endif /* HAVE_RT_PUSH_IPI */

/* pull a task into this CPU's runqueue (from another CPU) - called in switch_from() */
// static void pull_freezer_task(struct rq *this_rq)
// {
// 	trace_printk("pull_freezer_task()\n");
// 	int this_cpu = this_rq->cpu, cpu;
// 	bool resched = false;
// 	struct task_struct *p, *push_task;
// 	struct rq *src_rq;
// 	// int freezer_overload_count = freezer_overloaded(this_rq);

// 	// if (likely(!freezer_overload_count))
// 	// 	return;

// 	/*
// 	 * Match the barrier from freezer_set_overloaded; this guarantees that if we
// 	 * see overloaded we must also see the rto_mask bit.
// 	 */
// 	//smp_rmb();

// 	// /* If we are the only overloaded CPU do nothing */
// 	// if (freezer_overload_count == 1 &&
// 	//     cpumask_test_cpu(this_rq->cpu, this_rq->rd->rto_mask))
// 	// 	return;

// // #ifdef HAVE_RT_PUSH_IPI
// // 	if (sched_feat(RT_PUSH_IPI)) {
// // 		tell_cpu_to_push(this_rq);
// // 		return;
// // 	}
// // #endif

// 	for_each_cpu(cpu, this_rq->rd->rto_mask) {
// 		if (this_cpu == cpu)
// 			continue;

// 		src_rq = cpu_rq(cpu);

// 		/*
// 		 * Don't bother taking the src_rq->lock if the next highest
// 		 * task is known to be lower-priority than our current task.
// 		 * This may look racy, but if this value is about to go
// 		 * logically higher, the src_rq will push this task away.
// 		 * And if its going logically lower, we do not care
// 		 */
// 		// if (src_rq->freezer.highest_prio.next >=
// 		//     this_rq->freezer.highest_prio.curr)
// 		// 	continue;

// 		/*
// 		 * We can potentially drop this_rq's lock in
// 		 * double_lock_balance, and another CPU could
// 		 * alter this_rq
// 		 */
// 		push_task = NULL;
// 		double_lock_balance(this_rq, src_rq);

// 		/*
// 		 * We can pull only a task, which is pushable
// 		 * on its rq, and no others.
// 		 */
// 		p = freezer_pick_task_from_cpu(src_rq, this_cpu);

// 		/*
// 		 * Do we have an RT task that preempts
// 		 * the to-be-scheduled task?
// 		 */
// 		if (p) {
// 			WARN_ON(p == src_rq->curr);
// 			WARN_ON(!task_on_rq_queued(p));

// 			/*
// 			 * There's a chance that p is higher in priority
// 			 * than what's currently running on its CPU.
// 			 * This is just that p is waking up and hasn't
// 			 * had a chance to schedule. We only pull
// 			 * p if it is lower in priority than the
// 			 * current task on the run queue
// 			 */
// 			// if (p->prio < src_rq->curr->prio)
// 			// 	goto skip;

// 			if (is_migration_disabled(p)) {
// 				push_task = get_push_task(src_rq);
// 			} else {
// 				// Remove CPU from src rq
// 				// and add to this cpu rq
// 				deactivate_task(src_rq, p, 0);
// 				set_task_cpu(p, this_cpu);
// 				activate_task(this_rq, p, 0);
// 				resched = true;
// 			}
// 		}
// //skip:
// 		double_unlock_balance(this_rq, src_rq);

// 		// If CPU migration disabled
// 		if (push_task) {
// 			preempt_disable();
// 			raw_spin_rq_unlock(this_rq);
// 			stop_one_cpu_nowait(src_rq->cpu, push_cpu_stop,
// 					    push_task, &src_rq->push_work);
// 			preempt_enable();
// 			raw_spin_rq_lock(this_rq);
// 		}
// 	}

// 	if (resched)
// 		resched_curr(this_rq);
// }

// /*
//  * If we are not running and we are not going to reschedule soon, we should
//  * try to push tasks away now
//  */
static void task_woken_freezer(struct rq *rq, struct task_struct *p)
{
	// bool need_to_push = !task_on_cpu(rq, p) &&
	// 		    !test_tsk_need_resched(rq->curr) &&
	// 		    p->nr_cpus_allowed > 1 &&
	// 		    (dl_task(rq->curr) || freezer_task(rq->curr)) &&
	// 		    (rq->curr->nr_cpus_allowed < 2 ||
	// 		     rq->curr->prio <= p->prio);

	// if (need_to_push)
	// 	push_freezer_tasks(rq);
}

/* Assumes rq->lock is held */
static void rq_online_freezer(struct rq *rq)
{
}
static void rq_offline_freezer(struct rq *rq)
{
}

/*
 * When switch from the freezer queue, we bring ourselves to a position
 * that we might want to pull freezer tasks from other runqueues.
 */
static void switched_from_freezer(struct rq *rq, struct task_struct *p)
{
	trace_printk("switched_from_freezer()\n");
	// if (!task_on_rq_queued(p) || rq->freezer.freezer_nr_running)
	// 	return;

	// /* If we're idle, we need to steal a task from diff rq */
	// freezer_queue_pull_task(rq);
}

void __init init_sched_freezer_class(void)
{
	trace_printk("init_sched_freezer_class()\n");
	// unsigned int i;

	// for_each_possible_cpu(i) {
	// 	zalloc_cpumask_var_node(&per_cpu(local_cpu_mask, i),
	// 				GFP_KERNEL, cpu_to_node(i));
	// }
}
#endif /* CONFIG_SMP */

/*
 * When switching a task to freezer, we may overload the runqueue
 * with freezer tasks. In this case we try to push them off to
 * other runqueues.
 */
/* For freezer this function doesn't do anything */
static void switched_to_freezer(struct rq *rq, struct task_struct *p)
{
	trace_printk("switched_to_freezer()\n");
	/*
	 * If we are running, update the avg_rt tracking, as the running time
	 * will now on be accounted into the latter.
	 */
	// if (task_current(rq, p)) {
	// 	update_freezer_rq_load_avg(rq_clock_pelt(rq), rq, 0);
	// 	return;
	// }

	/*
	 * If we are not running we may need to preempt the current
	 * running task. If that current running task is also an RT task
	 * then see if we can move to another run queue.
	 */
	//if (task_on_rq_queued(p)) {
#ifdef CONFIG_SMP
		// if (p->nr_cpus_allowed > 1 && rq->freezer.overloaded)
		// 	freezer_queue_push_tasks(rq);
#endif /* CONFIG_SMP */
		/* we don't need this context-switch logic */
		// if (p->prio < rq->curr->prio && cpu_online(cpu_of(rq)))
		// 	resched_curr(rq);
	//}
}

/*
 * Priority of the task has changed. This may cause
 * us to initiate a push or pull.
 */
static void
prio_changed_freezer(struct rq *rq, struct task_struct *p, int oldprio)
{
// 	if (!task_on_rq_queued(p))
// 		return;

// 	if (task_current(rq, p)) {
// #ifdef CONFIG_SMP
// 		/*
// 		 * If our priority decreases while running, we
// 		 * may need to pull tasks to this runqueue.
// 		 */
// 		if (oldprio < p->prio)
// 			freezer_queue_pull_task(rq);

// 		/*
// 		 * If there's a higher priority task waiting to run
// 		 * then reschedule.
// 		 */
// 		if (p->prio > rq->freezer.highest_prio.curr)
// 			resched_curr(rq);
// #else
// 		/* For UP simply resched on drop of prio */
// 		if (oldprio < p->prio)
// 			resched_curr(rq);
// #endif /* CONFIG_SMP */
// 	} else {
// 		/*
// 		 * This task is not running, but if it is
// 		 * greater than the current running task
// 		 * then reschedule.
// 		 */
// 		if (p->prio < rq->curr->prio)
// 			resched_curr(rq);
// 	}
}

/* For now we don't implement POSIX timers */
/* #ifdef CONFIG_POSIX_TIMERS
static void watchdog(struct rq *rq, struct task_struct *p)
{
	unsigned long soft, hard;

	soft = task_rlimit(p, RLIMIT_RTTIME);
	hard = task_rlimit_max(p, RLIMIT_RTTIME);

	if (soft != RLIM_INFINITY) {
		unsigned long next;

		if (p->freezer.watchdog_stamp != jiffies) {
			p->freezer.timeout++;
			p->freezer.watchdog_stamp = jiffies;
		}

		next = DIV_ROUND_UP(min(soft, hard), USEC_PER_SEC/HZ);
		if (p->freezer.timeout > next) {
			posix_cputimers_rt_watchdog(&p->posix_cputimers,
						    p->se.sum_exec_runtime);
		}
	}
}
#else
static inline void watchdog(struct rq *rq, struct task_struct *p) { }
#endif
*/

/*
 * scheduler tick hitting a task of our scheduling class.
 *
 * NOTE: This function can be called remotely by the tick offload that
 * goes along full dynticks. Therefore no local assumption can be made
 * and everything must be accessed through the @rq and @curr passed in
 * parameters.
 */
static void task_tick_freezer(struct rq *rq, struct task_struct *p, int queued)
{
	trace_printk("task_tick_freezer()\n");
	//struct sched_freezer_entity *freezer_se = &p->freezer;

	//update_curr_freezer(rq);
	// TODO freezer stats
	//update_freezer_rq_load_avg(rq_clock_pelt(rq), rq, 1);

	// watchdog(rq, p);

	/* decrements time slice and if nonzero then we don't do anything  */
	if (--p->freezer.time_slice)
		return;

	// /* if time slice is 0, reset it and move task to back of queue*/
	p->freezer.time_slice = sched_freezer_timeslice;
	requeue_task_freezer(rq, p);
	resched_curr(rq);
	
	return;
}

static unsigned int get_rr_interval_freezer(struct rq *rq, struct task_struct *task)
{
	return sched_freezer_timeslice;
}

#ifdef CONFIG_SCHED_CORE
static int task_is_throttled_freezer(struct task_struct *p, int cpu)
{
	return 0;
}
#endif


DEFINE_SCHED_CLASS(freezer) = {

	.enqueue_task		= enqueue_task_freezer,
	.dequeue_task		= dequeue_task_freezer,
	.yield_task		= yield_task_freezer,

	// we don't need to preempt tasks on wakeup
	.wakeup_preempt		= wakeup_preempt_freezer,

	.pick_next_task		= pick_next_task_freezer,
	.put_prev_task		= put_prev_task_freezer,
	.set_next_task          = set_next_task_freezer,

#ifdef CONFIG_SMP
	.balance		= balance_freezer,
	.pick_task		= pick_task_freezer,
	.select_task_rq		= select_task_rq_freezer,
	.set_cpus_allowed       = set_cpus_allowed_common,
	.rq_online              = rq_online_freezer,
	.rq_offline             = rq_offline_freezer,
	// we don't need to push away tasks
	.task_woken		= task_woken_freezer,
	.switched_from		= switched_from_freezer,
	.find_lock_rq		= find_lock_lowest_rq_freezer,
#endif

	.task_tick		= task_tick_freezer,

	.get_rr_interval	= get_rr_interval_freezer,

	// no prios for freezer
	.prio_changed		= prio_changed_freezer,
	.switched_to		= switched_to_freezer,

	// No stats here
	.update_curr		= update_curr_freezer,

#ifdef CONFIG_SCHED_CORE
	.task_is_throttled	= task_is_throttled_freezer,
#endif

#ifdef CONFIG_UCLAMP_TASK
	.uclamp_enabled		= 1,
#endif
};

// #ifdef CONFIG_SYSCTL
// static int sched_freezer_global_constraints(void)
// {
// 	unsigned long flags;
// 	int i;

// 	raw_spin_lock_irqsave(&def_freezer_bandwidth.freezer_runtime_lock, flags);
// 	for_each_possible_cpu(i) {
// 		struct freezer_rq *freezer_rq = &cpu_rq(i)->freezer;

// 		raw_spin_lock(&freezer_rq->freezer_runtime_lock);
// 		freezer_rq->freezer_runtime = global_freezer_runtime();
// 		raw_spin_unlock(&freezer_rq->freezer_runtime_lock);
// 	}
// 	raw_spin_unlock_irqrestore(&def_freezer_bandwidth.freezer_runtime_lock, flags);

// 	return 0;
// }
// #endif /* CONFIG_SYSCTL */

// #ifdef CONFIG_SYSCTL
// static int sched_freezer_global_validate(void)
// {
// 	if ((sysctl_sched_freezer_runtime != RUNTIME_INF) &&
// 		((sysctl_sched_freezer_runtime > sysctl_sched_freezer_period) ||
// 		 ((u64)sysctl_sched_freezer_runtime *
// 			NSEC_PER_USEC > max_freezer_runtime)))
// 		return -EINVAL;

// 	return 0;
// }

// static void sched_freezer_do_global(void)
// {
// 	unsigned long flags;

// 	raw_spin_lock_irqsave(&def_freezer_bandwidth.freezer_runtime_lock, flags);
// 	def_freezer_bandwidth.freezer_runtime = global_freezer_runtime();
// 	def_freezer_bandwidth.freezer_period = ns_to_ktime(global_freezer_period());
// 	raw_spin_unlock_irqrestore(&def_freezer_bandwidth.freezer_runtime_lock, flags);
// }

// static int sched_freezer_handler(struct ctl_table *table, int write, void *buffer,
// 		size_t *lenp, loff_t *ppos)
// {
// 	int old_period, old_runtime;
// 	static DEFINE_MUTEX(mutex);
// 	int ret;

// 	mutex_lock(&mutex);
// 	old_period = sysctl_sched_freezer_period;
// 	old_runtime = sysctl_sched_freezer_runtime;

// 	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);

// 	if (!ret && write) {
// 		ret = sched_freezer_global_validate();
// 		if (ret)
// 			goto undo;

// 		ret = sched_dl_global_validate();
// 		if (ret)
// 			goto undo;

// 		ret = sched_freezer_global_constraints();
// 		if (ret)
// 			goto undo;

// 		sched_freezer_do_global();
// 		sched_dl_do_global();
// 	}
// 	if (0) {
// undo:
// 		sysctl_sched_freezer_period = old_period;
// 		sysctl_sched_freezer_runtime = old_runtime;
// 	}
// 	mutex_unlock(&mutex);

// 	return ret;
// }
// #endif /* CONFIG_SYSCTL */

// #ifdef CONFIG_SCHED_DEBUG
// void print_freezer_stats(struct seq_file *m, int cpu)
// {
// 	freezer_rq_iter_t iter;
// 	struct freezer_rq *freezer_rq;

// 	rcu_read_lock();
// 	for_each_freezer_rq(freezer_rq, iter, cpu_rq(cpu))
// 		print_freezer_rq(m, cpu, freezer_rq);
// 	rcu_read_unlock();
// }
// #endif /* CONFIG_SCHED_DEBUG */