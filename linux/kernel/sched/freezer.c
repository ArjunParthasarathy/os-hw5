/*
* Freezer Scheduling Class (implements round-robin)
*/
int sched_freezer_timeslice = FREEZER_TIMESLICE;

struct freezer_bandwidth def_freezer_bandwidth;

/* copied from RT
 * period over which we measure -rt task CPU usage in us.
 * default: 1s
 */
int sysctl_sched_freezer_period = 1000000;

/* copied from RT
 * part of the period that we allow rt tasks to run in us.
 * default: 0.95s
 */
int sysctl_sched_freezer_runtime = 950000;

#ifdef CONFIG_SYSCTL
static int sysctl_sched_freezer_timeslice = (MSEC_PER_SEC * FREEZER_TIMESLICE) / HZ;
static int sched_freezer_handler(struct ctl_table *table, int write, void *buffer,
		size_t *lenp, loff_t *ppos);
static struct ctl_table sched_freezer_sysctls[] = {
	{
		.procname       = "sched_freezer_period_us",
		.data           = &sysctl_sched_freezer_period,
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = sched_freezer_handler,
		.extra1         = SYSCTL_ONE,
		.extra2         = SYSCTL_INT_MAX,
	},
	{
		.procname       = "sched_freezer_runtime_us",
		.data           = &sysctl_sched_freezer_runtime,
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = sched_freezer_handler,
		.extra1         = SYSCTL_NEG_ONE,
		.extra2         = (void *)&sysctl_sched_freezer_period,
	},
	{}
};

static int __init sched_freezer_sysctl_init(void)
{
	register_sysctl_init("kernel", sched_freezer_sysctls);
	return 0;
}
late_initcall(sched_freezer_sysctl_init);
#endif

static enum hrtimer_restart sched_freezer_period_timer(struct hrtimer *timer)
{
	struct freezer_bandwidth *freezer_b =
		container_of(timer, struct freezer_bandwidth, freezer_period_timer);
	int idle = 0;
	int overrun;

	raw_spin_lock(&freezer_b->freezer_runtime_lock);
	for (;;) {
		overrun = hrtimer_forward_now(timer, freezer_b->freezer_period);
		if (!overrun)
			break;

		raw_spin_unlock(&freezer_b->freezer_runtime_lock);
		idle = do_sched_freezer_period_timer(freezer_b, overrun);
		raw_spin_lock(&freezer_b->freezer_runtime_lock);
	}
	if (idle)
		freezer_b->freezer_period_active = 0;
	raw_spin_unlock(&freezer_b->freezer_runtime_lock);

	return idle ? HRTIMER_NORESTART : HRTIMER_RESTART;
}

void init_freezer_bandwidth(struct freezer_bandwidth *freezer_b, u64 period, u64 runtime)
{
	freezer_b->freezer_period = ns_to_ktime(period);
	freezer_b->freezer_runtime = runtime;

	raw_spin_lock_init(&freezer_b->freezer_runtime_lock);

	hrtimer_init(&freezer_b->freezer_period_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL_HARD);
	freezer_b->freezer_period_timer.function = sched_freezer_period_timer;
}

static inline void do_start_freezer_bandwidth(struct freezer_bandwidth *freezer_b)
{
	raw_spin_lock(&freezer_b->freezer_runtime_lock);
	if (!freezer_b->freezer_period_active) {
		freezer_b->freezer_period_active = 1;
		/*
		 * SCHED_DEADLINE updates the bandwidth, as a run away
		 * RT task with a DL task could hog a CPU. But DL does
		 * not reset the period. If a deadline task was running
		 * without an RT task running, it can cause RT tasks to
		 * throttle when they start up. Kick the timer right away
		 * to update the period.
		 */
		hrtimer_forward_now(&freezer_b->freezer_period_timer, ns_to_ktime(0));
		hrtimer_start_expires(&freezer_b->freezer_period_timer,
				      HRTIMER_MODE_ABS_PINNED_HARD);
	}
	raw_spin_unlock(&freezer_b->freezer_runtime_lock);
}

static void start_freezer_bandwidth(struct freezer_bandwidth *freezer_b)
{
	if (!freezer_bandwidth_enabled() || freezer_b->freezer_runtime == RUNTIME_INF)
		return;

	do_start_freezer_bandwidth(freezer_b);
}

void init_freezer_rq(struct freezer_rq *freezer_rq)
{
	struct freezer_prio_array *array;
	int i;

	array = &freezer_rq->active;
	for (i = MAX_freezer_PRIO; i < MAX_FREEZER_PRIO; i++) {
		INIT_LIST_HEAD(array->queue + i);
		__clear_bit(i, array->bitmap);
	}
	/* delimiter for bitsearch: */
	__set_bit(MAX_FREEZER_PRIO, array->bitmap);

#if defined CONFIG_SMP
	freezer_rq->highest_prio.curr = MAX_FREEZER_PRIO-1;
	freezer_rq->highest_prio.next = MAX_FREEZER_PRIO-1;
	freezer_rq->overloaded = 0;
	plist_head_init(&freezer_rq->pushable_tasks);
#endif /* CONFIG_SMP */
	/* We start at dequeued state, because no freezer tasks are queued */
	freezer_rq->freezer_queued = 0;

	freezer_rq->freezer_time = 0;
	freezer_rq->freezer_throttled = 0;
	freezer_rq->freezer_runtime = 0;
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

static inline bool need_pull_freezer_task(struct rq *rq, struct task_struct *prev)
{
	/* Try to pull RT tasks here if we lower this rq's prio */
	return rq->online && rq->freezer.highest_prio.curr > prev->prio;
}

static inline int freezer_overloaded(struct rq *rq)
{
	return atomic_read(&rq->rd->rto_count);
}

static inline void freezer_set_overload(struct rq *rq)
{
	if (!rq->online)
		return;

	cpumask_set_cpu(rq->cpu, rq->rd->rto_mask);
	/*
	 * Make sure the mask is visible before we set
	 * the overload count. That is checked to determine
	 * if we should look at the mask. It would be a shame
	 * if we looked at the mask, but the mask was not
	 * updated yet.
	 *
	 * Matched by the barrier in pull_freezer_task().
	 */
	smp_wmb();
	atomic_inc(&rq->rd->rto_count);
}

static inline void freezer_clear_overload(struct rq *rq)
{
	if (!rq->online)
		return;

	/* the order here really doesn't matter */
	atomic_dec(&rq->rd->rto_count);
	cpumask_clear_cpu(rq->cpu, rq->rd->rto_mask);
}

static inline int has_pushable_tasks(struct rq *rq)
{
	return !plist_head_empty(&rq->freezer.pushable_tasks);
}

static DEFINE_PER_CPU(struct balance_callback, freezer_push_head);
static DEFINE_PER_CPU(struct balance_callback, freezer_pull_head);

static void push_freezer_tasks(struct rq *);
static void pull_freezer_task(struct rq *);

static inline void freezer_queue_push_tasks(struct rq *rq)
{
	if (!has_pushable_tasks(rq))
		return;

	queue_balance_callback(rq, &per_cpu(freezer_push_head, rq->cpu), push_freezer_tasks);
}

static inline void freezer_queue_pull_task(struct rq *rq)
{
	queue_balance_callback(rq, &per_cpu(freezer_pull_head, rq->cpu), pull_freezer_task);
}

static void enqueue_pushable_task(struct rq *rq, struct task_struct *p)
{
	plist_del(&p->pushable_tasks, &rq->freezer.pushable_tasks);
	plist_node_init(&p->pushable_tasks, p->prio);
	plist_add(&p->pushable_tasks, &rq->freezer.pushable_tasks);

	/* Update the highest prio pushable task */
	if (p->prio < rq->freezer.highest_prio.next)
		rq->freezer.highest_prio.next = p->prio;

	if (!rq->freezer.overloaded) {
		freezer_set_overload(rq);
		rq->freezer.overloaded = 1;
	}
}

static void dequeue_pushable_task(struct rq *rq, struct task_struct *p)
{
	plist_del(&p->pushable_tasks, &rq->rt.pushable_tasks);

	/* Update the new highest prio pushable task */
	if (has_pushable_tasks(rq)) {
		p = plist_first_entry(&rq->rt.pushable_tasks,
				      struct task_struct, pushable_tasks);
		rq->freezer.highest_prio.next = p->prio;
	} else {
		rq->freezer.highest_prio.next = MAX_FREEZDR_PRIO-1;

		if (rq->freezer.overloaded) {
			freezer_clear_overload(rq);
			rq->freezer.overloaded = 0;
		}
	}
}

#else

static inline void enqueue_pushable_task(struct rq *rq, struct task_struct *p)
{
}

static inline void dequeue_pushable_task(struct rq *rq, struct task_struct *p)
{
}

static inline void freezer_queue_push_tasks(struct rq *rq)
{
}
#endif /* CONFIG_SMP */

static void enqueue_top_freezer_rq(struct freezer_rq *freezer_rq);
static void dequeue_top_freezer_rq(struct freezer_rq *freezer_rq, unsigned int count);

static inline int on_freezer_rq(struct sched_freezer_entity *freezer_se)
{
	return freezer_se->on_rq;
}
#ifdef CONFIG_UCLAMP_TASK
/*
 * Verify the fitness of task @p to run on @cpu taking into account the uclamp
 * settings.
 *
 * This check is only important for heterogeneous systems where uclamp_min value
 * is higher than the capacity of a @cpu. For non-heterogeneous system this
 * function will always return true.
 *
 * The function will return true if the capacity of the @cpu is >= the
 * uclamp_min and false otherwise.
 *
 * Note that uclamp_min will be clamped to uclamp_max if uclamp_min
 * > uclamp_max.
 */
static inline bool freezer_task_fits_capacity(struct task_struct *p, int cpu)
{
	unsigned int min_cap;
	unsigned int max_cap;
	unsigned int cpu_cap;

	/* Only heterogeneous systems can benefit from this check */
	if (!sched_asym_cpucap_active())
		return true;

	min_cap = uclamp_eff_value(p, UCLAMP_MIN);
	max_cap = uclamp_eff_value(p, UCLAMP_MAX);

	cpu_cap = arch_scale_cpu_capacity(cpu);

	return cpu_cap >= min(min_cap, max_cap);
}
#else
static inline bool freezer_task_fits_capacity(struct task_struct *p, int cpu)
{
	return true;
}
#endif /* CONFIG_UCLAMP_TASK */

static inline u64 sched_freezer_runtime(struct freezer_rq *freezer_rq)
{
	return freezer_rq->freezer_runtime;
}

static inline u64 sched_freezer_period(struct freezer_rq *freezer_rq)
{
	return ktime_to_ns(def_freezer_bandwidth.freezer_period);
}

typedef struct freezer_rq *freezer_rq_iter_t;

#define for_each_freezer_rq(freezer_rq, iter, rq) \
	for ((void) iter, freezer_rq = &rq->freezer; freezer_rq; freezer_rq = NULL)

#define for_each_sched_freezer_entity(freezer_se) \
	for (; freezer_se; freezer_se = NULL)

static inline struct freezer_rq *group_freezer_rq(struct sched_freezer_entity *freezer_se)
{
	return NULL;
}

static inline void sched_freezer_rq_enqueue(struct freezer_rq *freezer_rq)
{
	struct rq *rq = rq_of_freezer_rq(freezer_rq);

	if (!freezer_rq->freezer_nr_running)
		return;

	enqueue_top_freezer_rq(freezer_rq);
	resched_curr(rq);
}

static inline void sched_freezer_rq_dequeue(struct freezer_rq *freezer_rq)
{
	dequeue_top_freezer_rq(freezer_rq, freezer_rq->freezer_nr_running);
}

static inline int freezer_rq_throttled(struct freezer_rq *freezer_rq)
{
	return freezer_rq->freezer_throttled;
}

static inline const struct cpumask *sched_freezer_period_mask(void)
{
	return cpu_online_mask;
}

static inline
struct freezer_rq *sched_freezer_period_freezer_rq(struct freezer_bandwidth *freezer_b, int cpu)
{
	return &cpu_rq(cpu)->freezer;
}

static inline struct freezer_bandwidth *sched_freezer_bandwidth(struct freezer_rq *freezer_rq)
{
	return &def_freezer_bandwidth;
}

bool sched_freezer_bandwidth_account(struct freezer_rq *freezer_rq)
{
	struct freezer_bandwidth *freezer_b = sched_freezer_bandwidth(freezer_rq);

	return (hrtimer_active(&freezer_b->freezer_period_timer) ||
		freezer_rq->freezer_time < freezer_b->freezer_runtime);
}

#ifdef CONFIG_SMP
/*
 * We ran out of runtime, see if we can borrow some from our neighbours.
 */
static void do_balance_runtime(struct freezer_rq *freezer_rq)
{
	struct freezer_bandwidth *freezer_b = sched_freezer_bandwidth(freezer_rq);
	struct root_domain *rd = rq_of_freezer_rq(freezer_rq)->rd;
	int i, weight;
	u64 freezer_period;

	weight = cpumask_weight(rd->span);

	raw_spin_lock(&freezer_b->freezer_runtime_lock);
	freezer_period = ktime_to_ns(freezer_b->freezer_period);
	for_each_cpu(i, rd->span) {
		struct freezer_rq *iter = sched_freezer_period_freezer_rq(freezer_b, i);
		s64 diff;

		if (iter == freezer_rq)
			continue;

		raw_spin_lock(&iter->freezer_runtime_lock);
		/*
		 * Either all rqs have inf runtime and there's nothing to steal
		 * or __disable_runtime() below sets a specific rq to inf to
		 * indicate its been disabled and disallow stealing.
		 */
		if (iter->freezer_runtime == RUNTIME_INF)
			goto next;

		/*
		 * From runqueues with spare time, take 1/n part of their
		 * spare time, but no more than our period.
		 */
		diff = iter->freezer_runtime - iter->freezer_time;
		if (diff > 0) {
			diff = div_u64((u64)diff, weight);
			if (freezer_rq->freezer_runtime + diff > freezer_period)
				diff = freezer_period - freezer_rq->freezer_runtime;
			iter->freezer_runtime -= diff;
			freezer_rq->freezer_runtime += diff;
			if (freezer_rq->freezer_runtime == freezer_period) {
				raw_spin_unlock(&iter->freezer_runtime_lock);
				break;
			}
		}
next:
		raw_spin_unlock(&iter->freezer_runtime_lock);
	}
	raw_spin_unlock(&freezer_b->freezer_runtime_lock);
}

/*
 * Ensure this RQ takes back all the runtime it lend to its neighbours.
 */
static void __disable_runtime(struct rq *rq)
{
	struct root_domain *rd = rq->rd;
	freezer_rq_iter_t iter;
	struct freezer_rq *freezer_rq;

	if (unlikely(!scheduler_running))
		return;

	for_each_freezer_rq(freezer_rq, iter, rq) {
		struct freezer_bandwidth *freezer_b = sched_freezer_bandwidth(freezer_rq);
		s64 want;
		int i;

		raw_spin_lock(&freezer_b->freezer_runtime_lock);
		raw_spin_lock(&freezer_rq->freezer_runtime_lock);
		/*
		 * Either we're all inf and nobody needs to borrow, or we're
		 * already disabled and thus have nothing to do, or we have
		 * exactly the right amount of runtime to take out.
		 */
		if (freezer_rq->freezer_runtime == RUNTIME_INF ||
				freezer_rq->freezer_runtime == freezer_b->freezer_runtime)
			goto balanced;
		raw_spin_unlock(&freezer_rq->freezer_runtime_lock);

		/*
		 * Calculate the difference between what we started out with
		 * and what we current have, that's the amount of runtime
		 * we lend and now have to reclaim.
		 */
		want = freezer_b->freezer_runtime - freezer_rq->freezer_runtime;

		/*
		 * Greedy reclaim, take back as much as we can.
		 */
		for_each_cpu(i, rd->span) {
			struct freezer_rq *iter = sched_freezer_period_freezer_rq(freezer_b, i);
			s64 diff;

			/*
			 * Can't reclaim from ourselves or disabled runqueues.
			 */
			if (iter == freezer_rq || iter->freezer_runtime == RUNTIME_INF)
				continue;

			raw_spin_lock(&iter->freezer_runtime_lock);
			if (want > 0) {
				diff = min_t(s64, iter->freezer_runtime, want);
				iter->freezer_runtime -= diff;
				want -= diff;
			} else {
				iter->freezer_runtime -= want;
				want -= want;
			}
			raw_spin_unlock(&iter->freezer_runtime_lock);

			if (!want)
				break;
		}

		raw_spin_lock(&freezer_rq->freezer_runtime_lock);
		/*
		 * We cannot be left wanting - that would mean some runtime
		 * leaked out of the system.
		 */
		WARN_ON_ONCE(want);
balanced:
		/*
		 * Disable all the borrow logic by pretending we have inf
		 * runtime - in which case borrowing doesn't make sense.
		 */
		freezer_rq->freezer_runtime = RUNTIME_INF;
		freezer_rq->freezer_throttled = 0;
		raw_spin_unlock(&freezer_rq->freezer_runtime_lock);
		raw_spin_unlock(&freezer_b->freezer_runtime_lock);

		/* Make freezer_rq available for pick_next_task() */
		sched_freezer_rq_enqueue(freezer_rq);
	}
}

static void __enable_runtime(struct rq *rq)
{
	freezer_rq_iter_t iter;
	struct freezer_rq *freezer_rq;

	if (unlikely(!scheduler_running))
		return;

	/*
	 * Reset each runqueue's bandwidth settings
	 */
	for_each_freezer_rq(freezer_rq, iter, rq) {
		struct freezer_bandwidth *freezer_b = sched_freezer_bandwidth(freezer_rq);

		raw_spin_lock(&freezer_b->freezer_runtime_lock);
		raw_spin_lock(&freezer_rq->freezer_runtime_lock);
		freezer_rq->freezer_runtime = freezer_b->freezer_runtime;
		freezer_rq->freezer_time = 0;
		freezer_rq->freezer_throttled = 0;
		raw_spin_unlock(&freezer_rq->freezer_runtime_lock);
		raw_spin_unlock(&freezer_b->freezer_runtime_lock);
	}
}

static void balance_runtime(struct freezer_rq *freezer_rq)
{
	if (!sched_feat(freezer_RUNTIME_SHARE))
		return;

	if (freezer_rq->freezer_time > freezer_rq->freezer_runtime) {
		raw_spin_unlock(&freezer_rq->freezer_runtime_lock);
		do_balance_runtime(freezer_rq);
		raw_spin_lock(&freezer_rq->freezer_runtime_lock);
	}
}
#else /* !CONFIG_SMP */
static inline void balance_runtime(struct freezer_rq *freezer_rq) {}
#endif /* CONFIG_SMP */

static int do_sched_freezer_period_timer(struct freezer_bandwidth *freezer_b, int overrun)
{
	int i, idle = 1, throttled = 0;
	const struct cpumask *span;

	span = sched_freezer_period_mask();

	for_each_cpu(i, span) {
		int enqueue = 0;
		struct freezer_rq *freezer_rq = sched_freezer_period_freezer_rq(freezer_b, i);
		struct rq *rq = rq_of_freezer_rq(freezer_rq);
		struct rq_flags rf;
		int skip;

		/*
		 * When span == cpu_online_mask, taking each rq->lock
		 * can be time-consuming. Try to avoid it when possible.
		 */
		raw_spin_lock(&freezer_rq->freezer_runtime_lock);
		if (!sched_feat(freezer_RUNTIME_SHARE) && freezer_rq->freezer_runtime != RUNTIME_INF)
			freezer_rq->freezer_runtime = freezer_b->freezer_runtime;
		skip = !freezer_rq->freezer_time && !freezer_rq->freezer_nr_running;
		raw_spin_unlock(&freezer_rq->freezer_runtime_lock);
		if (skip)
			continue;

		rq_lock(rq, &rf);
		update_rq_clock(rq);

		if (freezer_rq->freezer_time) {
			u64 runtime;

			raw_spin_lock(&freezer_rq->freezer_runtime_lock);
			if (freezer_rq->freezer_throttled)
				balance_runtime(freezer_rq);
			runtime = freezer_rq->freezer_runtime;
			freezer_rq->freezer_time -= min(freezer_rq->freezer_time, overrun*runtime);
			if (freezer_rq->freezer_throttled && freezer_rq->freezer_time < runtime) {
				freezer_rq->freezer_throttled = 0;
				enqueue = 1;

				/*
				 * When we're idle and a woken (rt) task is
				 * throttled wakeup_preempt() will set
				 * skip_update and the time between the wakeup
				 * and this unthrottle will get accounted as
				 * 'runtime'.
				 */
				if (freezer_rq->freezer_nr_running && rq->curr == rq->idle)
					rq_clock_cancel_skipupdate(rq);
			}
			if (freezer_rq->freezer_time || freezer_rq->freezer_nr_running)
				idle = 0;
			raw_spin_unlock(&freezer_rq->freezer_runtime_lock);
		} else if (freezer_rq->freezer_nr_running) {
			idle = 0;
			if (!freezer_rq_throttled(freezer_rq))
				enqueue = 1;
		}
		if (freezer_rq->freezer_throttled)
			throttled = 1;

		if (enqueue)
			sched_freezer_rq_enqueue(freezer_rq);
		rq_unlock(rq, &rf);
	}

	if (!throttled && (!freezer_bandwidth_enabled() || freezer_b->freezer_runtime == RUNTIME_INF))
		return 1;

	return idle;
}

static inline int freezer_se_prio(struct sched_freezer_entity *freezer_se)
{
	return freezer_task_of(freezer_se)->prio;
}

static int sched_freezer_runtime_exceeded(struct freezer_rq *freezer_rq)
{
	u64 runtime = sched_freezer_runtime(freezer_rq);

	if (freezer_rq->freezer_throttled)
		return freezer_rq_throttled(freezer_rq);

	if (runtime >= sched_freezer_period(freezer_rq))
		return 0;

	balance_runtime(freezer_rq);
	runtime = sched_freezer_runtime(freezer_rq);
	if (runtime == RUNTIME_INF)
		return 0;

	if (freezer_rq->freezer_time > runtime) {
		struct freezer_bandwidth *freezer_b = sched_freezer_bandwidth(freezer_rq);

		/*
		 * Don't actually throttle groups that have no runtime assigned
		 * but accrue some time due to boosting.
		 */
		if (likely(freezer_b->freezer_runtime)) {
			freezer_rq->freezer_throttled = 1;
			printk_deferred_once("sched: RT throttling activated\n");
		} else {
			/*
			 * In case we did anyway, make it go away,
			 * replenishment is a joke, since it will replenish us
			 * with exactly 0 ns.
			 */
			freezer_rq->freezer_time = 0;
		}

		if (freezer_rq_throttled(freezer_rq)) {
			sched_freezer_rq_dequeue(freezer_rq);
			return 1;
		}
	}

	return 0;
}

/*
 * Update the current task's runtime statistics. Skip current tasks that
 * are not in our scheduling class.
 */
static void update_curr_freezer(struct rq *rq)
{
	struct task_struct *curr = rq->curr;
	struct sched_freezer_entity *freezer_se = &curr->rt;
	s64 delta_exec;

	if (curr->sched_class != &freezer_sched_class)
		return;

	delta_exec = update_curr_common(rq);
	if (unlikely(delta_exec <= 0))
		return;

	if (!freezer_bandwidth_enabled())
		return;

	for_each_sched_freezer_entity(freezer_se) {
		struct freezer_rq *freezer_rq = freezer_rq_of_se(freezer_se);
		int exceeded;

		if (sched_freezer_runtime(freezer_rq) != RUNTIME_INF) {
			raw_spin_lock(&freezer_rq->freezer_runtime_lock);
			freezer_rq->freezer_time += delta_exec;
			exceeded = sched_freezer_runtime_exceeded(freezer_rq);
			if (exceeded)
				resched_curr(rq);
			raw_spin_unlock(&freezer_rq->freezer_runtime_lock);
			if (exceeded)
				do_start_freezer_bandwidth(sched_freezer_bandwidth(freezer_rq));
		}
	}
}

static void
dequeue_top_freezer_rq(struct freezer_rq *freezer_rq, unsigned int count)
{
	struct rq *rq = rq_of_freezer_rq(freezer_rq);

	BUG_ON(&rq->rt != freezer_rq);

	if (!freezer_rq->freezer_queued)
		return;

	BUG_ON(!rq->nr_running);

	sub_nr_running(rq, count);
	freezer_rq->freezer_queued = 0;

}

static voidfreezer(struct freezer_rq *freezer_rq)
{
	struct rq *rq = rq_of_freezer_rq(freezer_rq);

	BUG_ON(&rq->rt != freezer_rq);

	if (freezer_rq->freezer_queued)
		return;

	if (freezer_rq_throttled(freezer_rq))
		return;

	if (freezer_rq->freezer_nr_running) {
		add_nr_running(rq, freezer_rq->freezer_nr_running);
		freezer_rq->freezer_queued = 1;
	}

	/* Kick cpufreq (see the comment in kernel/sched/sched.h). */
	cpufreq_update_util(rq, 0);
}

#if defined CONFIG_SMP

static void
inc_freezer_prio_smp(struct freezer_rq *freezer_rq, int prio, int prev_prio)
{
	struct rq *rq = rq_of_freezer_rq(freezer_rq);

	if (rq->online && prio < prev_prio)
		cpupri_set(&rq->rd->cpupri, rq->cpu, prio);
}

static void
dec_freezer_prio_smp(struct freezer_rq *freezer_rq, int prio, int prev_prio)
{
	struct rq *rq = rq_of_freezer_rq(freezer_rq);

	if (rq->online && freezer_rq->highest_prio.curr != prev_prio)
		cpupri_set(&rq->rd->cpupri, rq->cpu, freezer_rq->highest_prio.curr);
}

#else /* CONFIG_SMP */

static inline
void inc_freezer_prio_smp(struct freezer_rq *freezer_rq, int prio, int prev_prio) {}
static inline
void dec_freezer_prio_smp(struct freezer_rq *freezer_rq, int prio, int prev_prio) {}

#endif /* CONFIG_SMP */

#if defined CONFIG_SMP /* for RT this has || defined CONFIG_RT_GROUP_SCHED */
static void
inc_freezer_prio(struct freezer_rq *freezer_rq, int prio)
{
	int prev_prio = freezer_rq->highest_prio.curr;

	if (prio < prev_prio)
		freezer_rq->highest_prio.curr = prio;

	inc_freezer_prio_smp(freezer_rq, prio, prev_prio);
}

static void
dec_freezer_prio(struct freezer_rq *freezer_rq, int prio)
{
	int prev_prio = freezer_rq->highest_prio.curr;

	if (freezer_rq->freezer_nr_running) {

		WARN_ON(prio < prev_prio);

		/*
		 * This may have been our highest task, and therefore
		 * we may have some recomputation to do
		 */
		if (prio == prev_prio) {
			struct freezer_prio_array *array = &freezer_rq->active;

			freezer_rq->highest_prio.curr =
				sched_find_first_bit(array->bitmap);
		}

	} else {
		freezer_rq->highest_prio.curr = MAX_freezer_PRIO-1;
	}

	dec_freezer_prio_smp(freezer_rq, prio, prev_prio);
}

#else

static inline void inc_freezer_prio(struct freezer_rq *freezer_rq, int prio) {}
static inline void dec_freezer_prio(struct freezer_rq *freezer_rq, int prio) {}

#endif /* CONFIG_SMP */

static void
inc_freezer_group(struct sched_freezer_entity *freezer_se, struct freezer_rq *freezer_rq)
{
	start_freezer_bandwidth(&def_freezer_bandwidth);
}

static inline
void dec_freezer_group(struct sched_freezer_entity *freezer_se, struct freezer_rq *freezer_rq) {}

static inline
unsigned int freezer_se_nr_running(struct sched_freezer_entity *freezer_se)
{
	struct freezer_rq *group_rq = group_freezer_rq(freezer_se);

	if (group_rq)
		return group_rq->freezer_nr_running;
	else
		return 1;
}

static inline
unsigned int freezer_se_rr_nr_running(struct sched_freezer_entity *freezer_se)
{
	struct freezer_rq *group_rq = group_freezer_rq(freezer_se);
	struct task_struct *tsk;

	if (group_rq)
		return group_rq->rr_nr_running;

	tsk = freezer_task_of(freezer_se);

	return (tsk->policy == SCHED_RR) ? 1 : 0;
}

static inline
void inc_freezer_tasks(struct sched_freezer_entity *freezer_se, struct freezer_rq *freezer_rq)
{
	int prio = freezer_se_prio(freezer_se);

	WARN_ON(!freezer_prio(prio));
	freezer_rq->freezer_nr_running += freezer_se_nr_running(freezer_se);
	freezer_rq->rr_nr_running += freezer_se_rr_nr_running(freezer_se);

	inc_freezer_prio(freezer_rq, prio);
	inc_freezer_group(freezer_se, freezer_rq);
}

static inline
void dec_freezer_tasks(struct sched_freezer_entity *freezer_se, struct freezer_rq *freezer_rq)
{
	WARN_ON(!freezer_prio(freezer_se_prio(freezer_se)));
	WARN_ON(!freezer_rq->freezer_nr_running);
	freezer_rq->freezer_nr_running -= freezer_se_nr_running(freezer_se);
	freezer_rq->rr_nr_running -= freezer_se_rr_nr_running(freezer_se);

	dec_freezer_prio(freezer_rq, freezer_se_prio(freezer_se));
	dec_freezer_group(freezer_se, freezer_rq);
}

/*
 * Change freezer_se->run_list location unless SAVE && !MOVE
 *
 * assumes ENQUEUE/DEQUEUE flags match
 */
static inline bool move_entity(unsigned int flags)
{
	if ((flags & (DEQUEUE_SAVE | DEQUEUE_MOVE)) == DEQUEUE_SAVE)
		return false;

	return true;
}

static void __delist_freezer_entity(struct sched_freezer_entity *freezer_se, struct freezer_prio_array *array)
{
	list_del_init(&freezer_se->run_list);

	if (list_empty(array->queue + freezer_se_prio(freezer_se)))
		__clear_bit(freezer_se_prio(freezer_se), array->bitmap);

	freezer_se->on_list = 0;
}

static inline struct sched_statistics *
__schedstats_from_freezer_se(struct sched_freezer_entity *freezer_se)
{
	return &freezer_task_of(freezer_se)->stats;
}

static inline void
update_stats_wait_start_freezer(struct freezer_rq *freezer_rq, struct sched_freezer_entity *freezer_se)
{
	struct sched_statistics *stats;
	struct task_struct *p = NULL;

	if (!schedstat_enabled())
		return;

	if (freezer_entity_is_task(freezer_se))
		p = freezer_task_of(freezer_se);

	stats = __schedstats_from_freezer_se(freezer_se);
	if (!stats)
		return;

	__update_stats_wait_start(rq_of_freezer_rq(freezer_rq), p, stats);
}

static inline void
update_stats_enqueue_sleeper_freezer(struct freezer_rq *freezer_rq, struct sched_freezer_entity *freezer_se)
{
	struct sched_statistics *stats;
	struct task_struct *p = NULL;

	if (!schedstat_enabled())
		return;

	if (freezer_entity_is_task(freezer_se))
		p = freezer_task_of(freezer_se);

	stats = __schedstats_from_freezer_se(freezer_se);
	if (!stats)
		return;

	__update_stats_enqueue_sleeper(rq_of_freezer_rq(freezer_rq), p, stats);
}

static inline void
update_stats_enqueue_freezer(struct freezer_rq *freezer_rq, struct sched_freezer_entity *freezer_se,
			int flags)
{
	if (!schedstat_enabled())
		return;

	if (flags & ENQUEUE_WAKEUP)
		update_stats_enqueue_sleeper_freezer(freezer_rq, freezer_se);
}

static inline void
update_stats_wait_end_freezer(struct freezer_rq *freezer_rq, struct sched_freezer_entity *freezer_se)
{
	struct sched_statistics *stats;
	struct task_struct *p = NULL;

	if (!schedstat_enabled())
		return;

	if (freezer_entity_is_task(freezer_se))
		p = freezer_task_of(freezer_se);

	stats = __schedstats_from_freezer_se(freezer_se);
	if (!stats)
		return;

	__update_stats_wait_end(rq_of_freezer_rq(freezer_rq), p, stats);
}

static inline void
update_stats_dequeue_rt(struct freezer_rq *freezer_rq, struct sched_freezer_entity *freezer_se,
			int flags)
{
	struct task_struct *p = NULL;

	if (!schedstat_enabled())
		return;

	if (freezer_entity_is_task(freezer_se))
		p = freezer_task_of(freezer_se);

	if ((flags & DEQUEUE_SLEEP) && p) {
		unsigned int state;

		state = READ_ONCE(p->__state);
		if (state & TASK_INTERRUPTIBLE)
			__schedstat_set(p->stats.sleep_start,
					rq_clock(rq_of_freezer_rq(freezer_rq)));

		if (state & TASK_UNINTERRUPTIBLE)
			__schedstat_set(p->stats.block_start,
					rq_clock(rq_of_freezer_rq(freezer_rq)));
	}
}

static void __enqueue_freezer_entity(struct sched_freezer_entity *freezer_se, unsigned int flags)
{
	struct freezer_rq *freezer_rq = freezer_rq_of_se(freezer_se);
	struct freezer_prio_array *array = &freezer_rq->active;
	/* this is NULL since no groups */
	struct freezer_rq *group_rq = group_freezer_rq(freezer_se);
	struct list_head *queue = array->queue + freezer_se_prio(freezer_se);

	if (move_entity(flags)) {
		WARN_ON_ONCE(freezer_se->on_list);
		if (flags & ENQUEUE_HEAD)
			list_add(&freezer_se->run_list, queue);
		else
			list_add_tail(&freezer_se->run_list, queue);

		__set_bit(freezer_se_prio(freezer_se), array->bitmap);
		freezer_se->on_list = 1;
	}
	freezer_se->on_rq = 1;

	inc_freezer_tasks(freezer_se, freezer_rq);
}

static void __dequeue_freezer_entity(struct sched_freezer_entity *freezer_se, unsigned int flags)
{
	struct freezer_rq *freezer_rq = freezer_rq_of_se(freezer_se);
	struct freezer_prio_array *array = &freezer_rq->active;

	if (move_entity(flags)) {
		WARN_ON_ONCE(!freezer_se->on_list);
		__delist_freezer_entity(freezer_se, array);
	}
	freezer_se->on_rq = 0;

	dec_freezer_tasks(freezer_se, freezer_rq);
}

/*
 * Because the prio of an upper entry depends on the lower
 * entries, we must remove entries top - down.
 */
static void dequeue_freezer_stack(struct sched_freezer_entity *freezer_se, unsigned int flags)
{
	struct sched_freezer_entity *back = NULL;
	unsigned int freezer_nr_running;

	for_each_sched_freezer_entity(freezer_se) {
		freezer_se->back = back;
		back = freezer_se;
	}

	freezer_nr_running = freezer_rq_of_se(back)->frezer_nr_running;

	for (freezer_se = back; freezer_se; freezer_se = freezer_se->back) {
		if (on_freezer_rq(freezer_se))
			__dequeue_freezer_entity(freezer_se, flags);
	}

	dequeue_top_freezer_rq(freezer_rq_of_se(back), freezer_nr_running);
}

static void enqueue_freezer_entity(struct sched_freezer_entity *freezer_se, unsigned int flags)
{
	struct rq *rq = rq_of_freezer_se(freezer_se);

	update_stats_enqueue_freezer(freezer_rq_of_se(freezer_se), freezer_se, flags);

	dequeue_freezer_stack(freezer_se, flags);
	for_each_sched_freezer_entity(freezer_se)
		__enqueue_freezer_entity(freezer_se, flags);
freezer(&rq->rt);
}

static void dequeue_freezer_entity(struct sched_freezer_entity *freezer_se, unsigned int flags)
{
	struct rq *rq = rq_of_freezer_se(freezer_se);

	update_stats_dequeue_rt(freezer_rq_of_se(freezer_se), freezer_se, flags);

	dequeue_freezer_stack(freezer_se, flags);

	for_each_sched_freezer_entity(freezer_se) {
		struct freezer_rq *freezer_rq = group_freezer_rq(freezer_se);

		if (freezer_rq && freezer_rq->freezer_nr_running)
			__enqueue_freezer_entity(freezer_se, flags);
	}
freezer(&rq->rt);
}

/*
 * Adding/removing a task to/from a priority array:
 */
static void
enqueue_task_freezer(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_freezer_entity *freezer_se = &p->freezer;

	if (flags & ENQUEUE_WAKEUP)
		freezer_se->timeout = 0;

	check_schedstat_required();
	update_stats_wait_start_freezer(freezer_rq_of_se(freezer_se), freezer_se);

	enqueue_freezer_entity(freezer_se, flags);

	if (!task_current(rq, p) && p->nr_cpus_allowed > 1)
		enqueue_pushable_task(rq, p);
}

static void dequeue_task_freezer(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_freezer_entity *freezer_se = &p->freezer;

	update_curr_freezer(rq);
	dequeue_freezer_entity(freezer_se, flags);

	dequeue_pushable_task(rq, p);
}

/*
 * Put task to the head or the end of the run list without the overhead of
 * dequeue followed by enqueue.
 */
static void
requeue_freezer_entity(struct freezer_rq *freezer_rq, struct sched_freezer_entity *freezer_se, int head)
{
	if (on_freezer_rq(freezer_se)) {
		struct freezer_prio_array *array = &freezer_rq->active;
		struct list_head *queue = array->queue + freezer_se_prio(freezer_se);

		if (head)
			list_move(&freezer_se->run_list, queue);
		else
			list_move_tail(&freezer_se->run_list, queue);
	}
}

static void requeue_task_freezer(struct rq *rq, struct task_struct *p, int head)
{
	struct sched_freezer_entity *freezer_se = &p->rt;
	struct freezer_rq *freezer_rq;

	for_each_sched_freezer_entity(freezer_se) {
		freezer_rq = freezer_rq_of_se(freezer_se);
		requeue_freezer_entity(freezer_rq, freezer_se, head);
	}
}

static void yield_task_freezer(struct rq *rq)
{
	requeue_task_freezer(rq, rq->curr, 0);
}

#ifdef CONFIG_SMP
static int find_lowest_rq(struct task_struct *task);

static int
select_task_rq_freezer(struct task_struct *p, int cpu, int flags)
{
	struct task_struct *curr;
	struct rq *rq;
	bool test;

	/* For anything but wake ups, just return the task_cpu */
	if (!(flags & (WF_TTWU | WF_FORK)))
		goto out;

	rq = cpu_rq(cpu);

	rcu_read_lock();
	curr = READ_ONCE(rq->curr); /* unlocked access */

	/*
	 * If the current task on @p's runqueue is an RT task, then
	 * try to see if we can wake this RT task up on another
	 * runqueue. Otherwise simply start this RT task
	 * on its current runqueue.
	 *
	 * We want to avoid overloading runqueues. If the woken
	 * task is a higher priority, then it will stay on this CPU
	 * and the lower prio task should be moved to another CPU.
	 * Even though this will probably make the lower prio task
	 * lose its cache, we do not want to bounce a higher task
	 * around just because it gave up its CPU, perhaps for a
	 * lock?
	 *
	 * For equal prio tasks, we just let the scheduler sort it out.
	 *
	 * Otherwise, just let it ride on the affined RQ and the
	 * post-schedule router will push the preempted task away
	 *
	 * This test is optimistic, if we get it wrong the load-balancer
	 * will have to sort it out.
	 *
	 * We take into account the capacity of the CPU to ensure it fits the
	 * requirement of the task - which is only important on heterogeneous
	 * systems like big.LITTLE.
	 */
	test = curr &&
	       unlikely(freezer_task(curr)) &&
	       (curr->nr_cpus_allowed < 2 || curr->prio <= p->prio);

	if (test || !freezer_task_fits_capacity(p, cpu)) {
		int target = find_lowest_rq(p);

		/*
		 * Bail out if we were forcing a migration to find a better
		 * fitting CPU but our search failed.
		 */
		if (!test && target != -1 && !freezer_task_fits_capacity(p, target))
			goto out_unlock;

		/*
		 * Don't bother moving it if the destination CPU is
		 * not running a lower priority task.
		 */
		if (target != -1 &&
		    p->prio < cpu_rq(target)->rt.highest_prio.curr)
			cpu = target;
	}

out_unlock:
	rcu_read_unlock();

out:
	return cpu;
}

static void check_preempt_equal_prio(struct rq *rq, struct task_struct *p)
{
	/*
	 * Current can't be migrated, useless to reschedule,
	 * let's hope p can move out.
	 */
	if (rq->curr->nr_cpus_allowed == 1 ||
	    !cpupri_find(&rq->rd->cpupri, rq->curr, NULL))
		return;

	/*
	 * p is migratable, so let's not schedule it and
	 * see if it is pushed or pulled somewhere else.
	 */
	if (p->nr_cpus_allowed != 1 &&
	    cpupri_find(&rq->rd->cpupri, p, NULL))
		return;

	/*
	 * There appear to be other CPUs that can accept
	 * the current task but none can run 'p', so lets reschedule
	 * to try and push the current task away:
	 */
	requeue_task_freezer(rq, p, 1);
	resched_curr(rq);
}

static int balance_freezer(struct rq *rq, struct task_struct *p, struct rq_flags *rf)
{
	if (!on_freezer_rq(&p->rt) && need_pull_freezer_task(rq, p)) {
		/*
		 * This is OK, because current is on_cpu, which avoids it being
		 * picked for load-balance and preemption/IRQs are still
		 * disabled avoiding further scheduler activity on it and we've
		 * not yet started the picking loop.
		 */
		rq_unpin_lock(rq, rf);
		pull_freezer_task(rq);
		rq_repin_lock(rq, rf);
	}

	return sched_stop_runnable(rq) || sched_dl_runnable(rq) || sched_rt_runnable(rq) || sched_freezer_runnable(rq);
}
#endif /* CONFIG_SMP */

/*
 * Preempt the current task with a newly woken task if needed:
 */
static void wakeup_preempt_freezer(struct rq *rq, struct task_struct *p, int flags)
{
	if (p->prio < rq->curr->prio) {
		resched_curr(rq);
		return;
	}

#ifdef CONFIG_SMP
	/*
	 * If:
	 *
	 * - the newly woken task is of equal priority to the current task
	 * - the newly woken task is non-migratable while current is migratable
	 * - current will be preempted on the next reschedule
	 *
	 * we should check to see if current can readily move to a different
	 * cpu.  If so, we will reschedule to allow the push logic to try
	 * to move current somewhere else, making room for our non-migratable
	 * task.
	 */
	if (p->prio == rq->curr->prio && !test_tsk_need_resched(rq->curr))
		check_preempt_equal_prio(rq, p);
#endif
}

static inline void set_next_task_freezer(struct rq *rq, struct task_struct *p, bool first)
{
	struct sched_freezer_entity *freezer_se = &p->freezer;
	struct freezer_rq *freezer_rq = &rq->freezer;

	p->se.exec_start = rq_clock_task(rq);
	if (on_freezer_rq(&p->freezer))
		update_stats_wait_end_freezer(freezer_rq, freezer_se);

	/* The running task is never eligible for pushing */
	dequeue_pushable_task(rq, p);

	if (!first)
		return;

	/*
	 * If prev task was freezer, put_prev_task() has already updated the
	 * utilization. We only care of the case where we start to schedule a
	 * freezer task
	 */
	if (rq->curr->sched_class != &freezer_sched_class)
		update_freezer_rq_load_avg(rq_clock_pelt(rq), rq, 0);

	freezer_queue_push_tasks(rq);
}

static struct sched_freezer_entity *pick_next_freezer_entity(struct freezer_rq *freezer_rq)
{
	struct freezer_prio_array *array = &freezer_rq->active;
	struct sched_freezer_entity *next = NULL;
	struct list_head *queue;
	int idx;

	idx = sched_find_first_bit(array->bitmap);
	BUG_ON(idx >= MAX_freezer_PRIO);

	queue = array->queue + idx;
	if (SCHED_WARN_ON(list_empty(queue)))
		return NULL;
	next = list_entry(queue->next, struct sched_freezer_entity, run_list);

	return next;
}

static struct task_struct *_pick_next_task_freezer(struct rq *rq)
{
	struct sched_freezer_entity *freezer_se;
	struct freezer_rq *freezer_rq  = &rq->rt;

	do {
		freezer_se = pick_next_freezer_entity(freezer_rq);
		if (unlikely(!freezer_se))
			return NULL;
		freezer_rq = group_freezer_rq(freezer_se);
	} while (freezer_rq);

	return freezer_task_of(freezer_se);
}

static struct task_struct *pick_task_freezer(struct rq *rq)
{
	struct task_struct *p;

	if (!sched_freezer_runnable(rq))
		return NULL;

	p = _pick_next_task_freezer(rq);

	return p;
}

static struct task_struct *pick_next_task_freezer(struct rq *rq)
{
	struct task_struct *p = pick_task_freezer(rq);

	if (p)
		set_next_task_freezer(rq, p, true);

	return p;
}

static void put_prev_task_freezer(struct rq *rq, struct task_struct *p)
{
	struct sched_freezer_entity *freezer_se = &p->rt;
	struct freezer_rq *freezer_rq = &rq->rt;

	if (on_freezer_rq(&p->rt))
		update_stats_wait_start_freezer(freezer_rq, freezer_se);

	update_curr_freezer(rq);

	update_freezer_rq_load_avg(rq_clock_pelt(rq), rq, 1);

	/*
	 * The previous task needs to be made eligible for pushing
	 * if it is still active
	 */
	if (on_freezer_rq(&p->rt) && p->nr_cpus_allowed > 1)
		enqueue_pushable_task(rq, p);
}

#ifdef CONFIG_SMP

/* Only try algorithms three times */
#define FREEZER_MAX_TRIES 3

static int pick_freezer_task(struct rq *rq, struct task_struct *p, int cpu)
{
	if (!task_on_cpu(rq, p) &&
	    cpumask_test_cpu(cpu, &p->cpus_mask))
		return 1;

	return 0;
}

/*
 * Return the highest pushable rq's task, which is suitable to be executed
 * on the CPU, NULL otherwise
 */
static struct task_struct *pick_highest_pushable_task(struct rq *rq, int cpu)
{
	struct plist_head *head = &rq->rt.pushable_tasks;
	struct task_struct *p;

	if (!has_pushable_tasks(rq))
		return NULL;

	plist_for_each_entry(p, head, pushable_tasks) {
		if (pick_freezer_task(rq, p, cpu))
			return p;
	}

	return NULL;
}

static DEFINE_PER_CPU(cpumask_var_t, local_cpu_mask);

static int find_lowest_rq(struct task_struct *task)
{
	struct sched_domain *sd;
	struct cpumask *lowest_mask = this_cpu_cpumask_var_ptr(local_cpu_mask);
	int this_cpu = smp_processor_id();
	int cpu      = task_cpu(task);
	int ret;

	/* Make sure the mask is initialized first */
	if (unlikely(!lowest_mask))
		return -1;

	if (task->nr_cpus_allowed == 1)
		return -1; /* No other targets possible */

	/*
	 * If we're on asym system ensure we consider the different capacities
	 * of the CPUs when searching for the lowest_mask.
	 */
	if (sched_asym_cpucap_active()) {

		ret = cpupri_find_fitness(&task_rq(task)->rd->cpupri,
					  task, lowest_mask,
					  freezer_task_fits_capacity);
	} else {

		ret = cpupri_find(&task_rq(task)->rd->cpupri,
				  task, lowest_mask);
	}

	if (!ret)
		return -1; /* No targets found */

	/*
	 * At this point we have built a mask of CPUs representing the
	 * lowest priority tasks in the system.  Now we want to elect
	 * the best one based on our affinity and topology.
	 *
	 * We prioritize the last CPU that the task executed on since
	 * it is most likely cache-hot in that location.
	 */
	if (cpumask_test_cpu(cpu, lowest_mask))
		return cpu;

	/*
	 * Otherwise, we consult the sched_domains span maps to figure
	 * out which CPU is logically closest to our hot cache data.
	 */
	if (!cpumask_test_cpu(this_cpu, lowest_mask))
		this_cpu = -1; /* Skip this_cpu opt if not among lowest */

	rcu_read_lock();
	for_each_domain(cpu, sd) {
		if (sd->flags & SD_WAKE_AFFINE) {
			int best_cpu;

			/*
			 * "this_cpu" is cheaper to preempt than a
			 * remote processor.
			 */
			if (this_cpu != -1 &&
			    cpumask_test_cpu(this_cpu, sched_domain_span(sd))) {
				rcu_read_unlock();
				return this_cpu;
			}

			best_cpu = cpumask_any_and_distribute(lowest_mask,
							      sched_domain_span(sd));
			if (best_cpu < nr_cpu_ids) {
				rcu_read_unlock();
				return best_cpu;
			}
		}
	}
	rcu_read_unlock();

	/*
	 * And finally, if there were no matches within the domains
	 * just give the caller *something* to work with from the compatible
	 * locations.
	 */
	if (this_cpu != -1)
		return this_cpu;

	cpu = cpumask_any_distribute(lowest_mask);
	if (cpu < nr_cpu_ids)
		return cpu;

	return -1;
}

/* Will lock the rq it finds */
static struct rq *find_lock_lowest_rq(struct task_struct *task, struct rq *rq)
{
	struct rq *lowest_rq = NULL;
	int tries;
	int cpu;

	for (tries = 0; tries < FREEZER_MAX_TRIES; tries++) {
		cpu = find_lowest_rq(task);

		if ((cpu == -1) || (cpu == rq->cpu))
			break;

		lowest_rq = cpu_rq(cpu);

		if (lowest_rq->rt.highest_prio.curr <= task->prio) {
			/*
			 * Target rq has tasks of equal or higher priority,
			 * retrying does not release any lock and is unlikely
			 * to yield a different result.
			 */
			lowest_rq = NULL;
			break;
		}

		/* if the prio of this runqueue changed, try again */
		if (double_lock_balance(rq, lowest_rq)) {
			/*
			 * We had to unlock the run queue. In
			 * the mean time, task could have
			 * migrated already or had its affinity changed.
			 * Also make sure that it wasn't scheduled on its rq.
			 * It is possible the task was scheduled, set
			 * "migrate_disabled" and then got preempted, so we must
			 * check the task migration disable flag here too.
			 */
			if (unlikely(task_rq(task) != rq ||
				     !cpumask_test_cpu(lowest_rq->cpu, &task->cpus_mask) ||
				     task_on_cpu(rq, task) ||
				     !freezer_task(task) ||
				     is_migration_disabled(task) ||
				     !task_on_rq_queued(task))) {

				double_unlock_balance(rq, lowest_rq);
				lowest_rq = NULL;
				break;
			}
		}

		/* If this rq is still suitable use it. */
		if (lowest_rq->rt.highest_prio.curr > task->prio)
			break;

		/* try again */
		double_unlock_balance(rq, lowest_rq);
		lowest_rq = NULL;
	}

	return lowest_rq;
}

static struct task_struct *pick_next_pushable_task(struct rq *rq)
{
	struct task_struct *p;

	if (!has_pushable_tasks(rq))
		return NULL;

	p = plist_first_entry(&rq->rt.pushable_tasks,
			      struct task_struct, pushable_tasks);

	BUG_ON(rq->cpu != task_cpu(p));
	BUG_ON(task_current(rq, p));
	BUG_ON(p->nr_cpus_allowed <= 1);

	BUG_ON(!task_on_rq_queued(p));
	BUG_ON(!freezer_task(p));

	return p;
}

/* TODO changes for work stealing?
 * If the current CPU has more than one freezer task, see if the non
 * running task can migrate over to a CPU that is running a task
 * of lesser priority.
 */
static int push_freezer_task(struct rq *rq, bool pull)
{
	struct task_struct *next_task;
	struct rq *lowest_rq;
	int ret = 0;

	if (!rq->rt.overloaded)
		return 0;

	next_task = pick_next_pushable_task(rq);
	if (!next_task)
		return 0;

retry:
	/*
	 * It's possible that the next_task slipped in of
	 * higher priority than current. If that's the case
	 * just reschedule current.
	 */
	if (unlikely(next_task->prio < rq->curr->prio)) {
		resched_curr(rq);
		return 0;
	}

	if (is_migration_disabled(next_task)) {
		struct task_struct *push_task = NULL;
		int cpu;

		if (!pull || rq->push_busy)
			return 0;

		/*
		 * Invoking find_lowest_rq() on anything but an RT task doesn't
		 * make sense. Per the above priority check, curr has to
		 * be of higher priority than next_task, so no need to
		 * reschedule when bailing out.
		 *
		 * Note that the stoppers are masqueraded as SCHED_FIFO
		 * (cf. sched_set_stop_task()), so we can't rely on freezer_task().
		 */
		if (rq->curr->sched_class != &freezer_sched_class)
			return 0;

		cpu = find_lowest_rq(rq->curr);
		if (cpu == -1 || cpu == rq->cpu)
			return 0;

		/*
		 * Given we found a CPU with lower priority than @next_task,
		 * therefore it should be running. However we cannot migrate it
		 * to this other CPU, instead attempt to push the current
		 * running task on this CPU away.
		 */
		push_task = get_push_task(rq);
		if (push_task) {
			preempt_disable();
			raw_spin_rq_unlock(rq);
			stop_one_cpu_nowait(rq->cpu, push_cpu_stop,
					    push_task, &rq->push_work);
			preempt_enable();
			raw_spin_rq_lock(rq);
		}

		return 0;
	}

	if (WARN_ON(next_task == rq->curr))
		return 0;

	/* We might release rq lock */
	get_task_struct(next_task);

	/* find_lock_lowest_rq locks the rq if found */
	lowest_rq = find_lock_lowest_rq(next_task, rq);
	if (!lowest_rq) {
		struct task_struct *task;
		/*
		 * find_lock_lowest_rq releases rq->lock
		 * so it is possible that next_task has migrated.
		 *
		 * We need to make sure that the task is still on the same
		 * run-queue and is also still the next task eligible for
		 * pushing.
		 */
		task = pick_next_pushable_task(rq);
		if (task == next_task) {
			/*
			 * The task hasn't migrated, and is still the next
			 * eligible task, but we failed to find a run-queue
			 * to push it to.  Do not retry in this case, since
			 * other CPUs will pull from us when ready.
			 */
			goto out;
		}

		if (!task)
			/* No more tasks, just exit */
			goto out;

		/*
		 * Something has shifted, try again.
		 */
		put_task_struct(next_task);
		next_task = task;
		goto retry;
	}

	deactivate_task(rq, next_task, 0);
	set_task_cpu(next_task, lowest_rq->cpu);
	activate_task(lowest_rq, next_task, 0);
	resched_curr(lowest_rq);
	ret = 1;

	double_unlock_balance(rq, lowest_rq);
out:
	put_task_struct(next_task);

	return ret;
}

static void push_freezer_tasks(struct rq *rq)
{
	/* push_freezer_task will return true if it moved an RT */
	while (push_freezer_task(rq, false))
		;
}

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
static int rto_next_cpu(struct root_domain *rd)
{
	int next;
	int cpu;

	/*
	 * When starting the IPI RT pushing, the rto_cpu is set to -1,
	 * rt_next_cpu() will simply return the first CPU found in
	 * the rto_mask.
	 *
	 * If rto_next_cpu() is called with rto_cpu is a valid CPU, it
	 * will return the next CPU found in the rto_mask.
	 *
	 * If there are no more CPUs left in the rto_mask, then a check is made
	 * against rto_loop and rto_loop_next. rto_loop is only updated with
	 * the rto_lock held, but any CPU may increment the rto_loop_next
	 * without any locking.
	 */
	for (;;) {

		/* When rto_cpu is -1 this acts like cpumask_first() */
		cpu = cpumask_next(rd->rto_cpu, rd->rto_mask);

		rd->rto_cpu = cpu;

		if (cpu < nr_cpu_ids)
			return cpu;

		rd->rto_cpu = -1;

		/*
		 * ACQUIRE ensures we see the @rto_mask changes
		 * made prior to the @next value observed.
		 *
		 * Matches WMB in freezer_set_overload().
		 */
		next = atomic_read_acquire(&rd->rto_loop_next);

		if (rd->rto_loop == next)
			break;

		rd->rto_loop = next;
	}

	return -1;
}

static inline bool rto_start_trylock(atomic_t *v)
{
	return !atomic_cmpxchg_acquire(v, 0, 1);
}

static inline void rto_start_unlock(atomic_t *v)
{
	atomic_set_release(v, 0);
}

static void tell_cpu_to_push(struct rq *rq)
{
	int cpu = -1;

	/* Keep the loop going if the IPI is currently active */
	atomic_inc(&rq->rd->rto_loop_next);

	/* Only one CPU can initiate a loop at a time */
	if (!rto_start_trylock(&rq->rd->rto_loop_start))
		return;

	raw_spin_lock(&rq->rd->rto_lock);

	/*
	 * The rto_cpu is updated under the lock, if it has a valid CPU
	 * then the IPI is still running and will continue due to the
	 * update to loop_next, and nothing needs to be done here.
	 * Otherwise it is finishing up and an ipi needs to be sent.
	 */
	if (rq->rd->rto_cpu < 0)
		cpu = rto_next_cpu(rq->rd);

	raw_spin_unlock(&rq->rd->rto_lock);

	rto_start_unlock(&rq->rd->rto_loop_start);

	if (cpu >= 0) {
		/* Make sure the rd does not get freed while pushing */
		sched_get_rd(rq->rd);
		irq_work_queue_on(&rq->rd->rto_push_work, cpu);
	}
}

/* Called from hardirq context */
void rto_push_irq_work_func(struct irq_work *work)
{
	struct root_domain *rd =
		container_of(work, struct root_domain, rto_push_work);
	struct rq *rq;
	int cpu;

	rq = this_rq();

	/*
	 * We do not need to grab the lock to check for has_pushable_tasks.
	 * When it gets updated, a check is made if a push is possible.
	 */
	if (has_pushable_tasks(rq)) {
		raw_spin_rq_lock(rq);
		while (push_freezer_task(rq, true))
			;
		raw_spin_rq_unlock(rq);
	}

	raw_spin_lock(&rd->rto_lock);

	/* Pass the IPI to the next rt overloaded queue */
	cpu = rto_next_cpu(rd);

	raw_spin_unlock(&rd->rto_lock);

	if (cpu < 0) {
		sched_put_rd(rd);
		return;
	}

	/* Try the next RT overloaded CPU */
	irq_work_queue_on(&rd->rto_push_work, cpu);
}
#endif /* HAVE_RT_PUSH_IPI */

static void pull_freezer_task(struct rq *this_rq)
{
	int this_cpu = this_rq->cpu, cpu;
	bool resched = false;
	struct task_struct *p, *push_task;
	struct rq *src_rq;
	int freezer_overload_count = freezer_overloaded(this_rq);

	if (likely(!freezer_overload_count))
		return;

	/*
	 * Match the barrier from freezer_set_overloaded; this guarantees that if we
	 * see overloaded we must also see the rto_mask bit.
	 */
	smp_rmb();

	/* If we are the only overloaded CPU do nothing */
	if (freezer_overload_count == 1 &&
	    cpumask_test_cpu(this_rq->cpu, this_rq->rd->rto_mask))
		return;

#ifdef HAVE_RT_PUSH_IPI
	if (sched_feat(RT_PUSH_IPI)) {
		tell_cpu_to_push(this_rq);
		return;
	}
#endif

	for_each_cpu(cpu, this_rq->rd->rto_mask) {
		if (this_cpu == cpu)
			continue;

		src_rq = cpu_rq(cpu);

		/*
		 * Don't bother taking the src_rq->lock if the next highest
		 * task is known to be lower-priority than our current task.
		 * This may look racy, but if this value is about to go
		 * logically higher, the src_rq will push this task away.
		 * And if its going logically lower, we do not care
		 */
		if (src_rq->rt.highest_prio.next >=
		    this_rq->rt.highest_prio.curr)
			continue;

		/*
		 * We can potentially drop this_rq's lock in
		 * double_lock_balance, and another CPU could
		 * alter this_rq
		 */
		push_task = NULL;
		double_lock_balance(this_rq, src_rq);

		/*
		 * We can pull only a task, which is pushable
		 * on its rq, and no others.
		 */
		p = pick_highest_pushable_task(src_rq, this_cpu);

		/*
		 * Do we have an RT task that preempts
		 * the to-be-scheduled task?
		 */
		if (p && (p->prio < this_rq->rt.highest_prio.curr)) {
			WARN_ON(p == src_rq->curr);
			WARN_ON(!task_on_rq_queued(p));

			/*
			 * There's a chance that p is higher in priority
			 * than what's currently running on its CPU.
			 * This is just that p is waking up and hasn't
			 * had a chance to schedule. We only pull
			 * p if it is lower in priority than the
			 * current task on the run queue
			 */
			if (p->prio < src_rq->curr->prio)
				goto skip;

			if (is_migration_disabled(p)) {
				push_task = get_push_task(src_rq);
			} else {
				deactivate_task(src_rq, p, 0);
				set_task_cpu(p, this_cpu);
				activate_task(this_rq, p, 0);
				resched = true;
			}
			/*
			 * We continue with the search, just in
			 * case there's an even higher prio task
			 * in another runqueue. (low likelihood
			 * but possible)
			 */
		}
skip:
		double_unlock_balance(this_rq, src_rq);

		if (push_task) {
			preempt_disable();
			raw_spin_rq_unlock(this_rq);
			stop_one_cpu_nowait(src_rq->cpu, push_cpu_stop,
					    push_task, &src_rq->push_work);
			preempt_enable();
			raw_spin_rq_lock(this_rq);
		}
	}

	if (resched)
		resched_curr(this_rq);
}

/*
 * If we are not running and we are not going to reschedule soon, we should
 * try to push tasks away now
 */
static void task_woken_freezer(struct rq *rq, struct task_struct *p)
{
	bool need_to_push = !task_on_cpu(rq, p) &&
			    !test_tsk_need_resched(rq->curr) &&
			    p->nr_cpus_allowed > 1 &&
			    (dl_task(rq->curr) || freezer_task(rq->curr)) &&
			    (rq->curr->nr_cpus_allowed < 2 ||
			     rq->curr->prio <= p->prio);

	if (need_to_push)
		push_freezer_tasks(rq);
}

/* Assumes rq->lock is held */
static void rq_online_freezer(struct rq *rq)
{
	if (rq->rt.overloaded)
		freezer_set_overload(rq);

	__enable_runtime(rq);

	cpupri_set(&rq->rd->cpupri, rq->cpu, rq->rt.highest_prio.curr);
}

/* Assumes rq->lock is held */
static void rq_offline_freezer(struct rq *rq)
{
	if (rq->freezer.overloaded)
		freezer_clear_overload(rq);

	__disable_runtime(rq);

	cpupri_set(&rq->rd->cpupri, rq->cpu, CPUPRI_INVALID);
}

/*
 * When switch from the rt queue, we bring ourselves to a position
 * that we might want to pull RT tasks from other runqueues.
 */
static void switched_from_freezer(struct rq *rq, struct task_struct *p)
{
	/*
	 * If there are other RT tasks then we will reschedule
	 * and the scheduling of the other RT tasks will handle
	 * the balancing. But if we are the last RT task
	 * we may need to handle the pulling of RT tasks
	 * now.
	 */
	if (!task_on_rq_queued(p) || rq->rt.freezer_nr_running)
		return;

	freezer_queue_pull_task(rq);
}

void __init init_sched_freezer_class(void)
{
	unsigned int i;

	for_each_possible_cpu(i) {
		zalloc_cpumask_var_node(&per_cpu(local_cpu_mask, i),
					GFP_KERNEL, cpu_to_node(i));
	}
}
#endif /* CONFIG_SMP */

/*
 * When switching a task to RT, we may overload the runqueue
 * with RT tasks. In this case we try to push them off to
 * other runqueues.
 */
static void switched_to_freezer(struct rq *rq, struct task_struct *p)
{
	/*
	 * If we are running, update the avg_rt tracking, as the running time
	 * will now on be accounted into the latter.
	 */
	if (task_current(rq, p)) {
		update_freezer_rq_load_avg(rq_clock_pelt(rq), rq, 0);
		return;
	}

	/*
	 * If we are not running we may need to preempt the current
	 * running task. If that current running task is also an RT task
	 * then see if we can move to another run queue.
	 */
	if (task_on_rq_queued(p)) {
#ifdef CONFIG_SMP
		if (p->nr_cpus_allowed > 1 && rq->rt.overloaded)
			freezer_queue_push_tasks(rq);
#endif /* CONFIG_SMP */
		if (p->prio < rq->curr->prio && cpu_online(cpu_of(rq)))
			resched_curr(rq);
	}
}

/*
 * Priority of the task has changed. This may cause
 * us to initiate a push or pull.
 */
static void
prio_changed_freezer(struct rq *rq, struct task_struct *p, int oldprio)
{
	if (!task_on_rq_queued(p))
		return;

	if (task_current(rq, p)) {
#ifdef CONFIG_SMP
		/*
		 * If our priority decreases while running, we
		 * may need to pull tasks to this runqueue.
		 */
		if (oldprio < p->prio)
			freezer_queue_pull_task(rq);

		/*
		 * If there's a higher priority task waiting to run
		 * then reschedule.
		 */
		if (p->prio > rq->rt.highest_prio.curr)
			resched_curr(rq);
#else
		/* For UP simply resched on drop of prio */
		if (oldprio < p->prio)
			resched_curr(rq);
#endif /* CONFIG_SMP */
	} else {
		/*
		 * This task is not running, but if it is
		 * greater than the current running task
		 * then reschedule.
		 */
		if (p->prio < rq->curr->prio)
			resched_curr(rq);
	}
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
static void task_tick_freezer(struct rq *rq, struct task_struct *curr, int queued)
{
	struct sched_freezer_entity *freezer_se = &p->freezer;

	update_curr_freezer(rq);
	// TODO freezer stats
	//update_freezer_rq_load_avg(rq_clock_pelt(rq), rq, 1);

	watchdog(rq, p);

	if (--p->freezer.time_slice)
		return;

	p->freezer.time_slice = sched_freezer_timeslice;

	/*
	 * Requeue to the end of queue if we (and all of our ancestors) are not
	 * the only element on the queue
	 */
	for_each_sched_freezer_entity(freezer_se) {
		if (freezer_se->run_list.prev != freezer_se->run_list.next) {
			requeue_task_freezer(rq, p, 0);
			resched_curr(rq);
			return;
		}
	}
}

static unsigned int get_rr_interval_freezer(struct rq *rq, struct task_struct *task)
{
	return sched_freezer_timeslice;
}

#ifdef CONFIG_SCHED_CORE
static int task_is_throttled_freezer(struct task_struct *p, int cpu)
{
	struct freezer_rq *freezer_rq;

	freezer_rq = &cpu_rq(cpu)->freezer;

	return freezer_rq_throttled(freezer_rq);
}
#endif


DEFINE_SCHED_CLASS(freezer) = {

	.enqueue_task		= enqueue_task_freezer,
	.dequeue_task		= dequeue_task_freezer,
	.yield_task		= yield_task_freezer,

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
	.task_woken		= task_woken_freezer,
	.switched_from		= switched_from_freezer,
	.find_lock_rq		= find_lock_lowest_rq,
#endif

	.task_tick		= task_tick_freezer,

	.get_rr_interval	= get_rr_interval_freezer,

	.prio_changed		= prio_changed_freezer,
	.switched_to		= switched_to_freezer,

	.update_curr		= update_curr_freezer,

#ifdef CONFIG_SCHED_CORE
	.task_is_throttled	= task_is_throttled_freezer,
#endif

#ifdef CONFIG_UCLAMP_TASK
	.uclamp_enabled		= 1,
#endif
};

#ifdef CONFIG_SYSCTL
static int sched_freezer_global_constraints(void)
{
	unsigned long flags;
	int i;

	raw_spin_lock_irqsave(&def_freezer_bandwidth.freezer_runtime_lock, flags);
	for_each_possible_cpu(i) {
		struct freezer_rq *freezer_rq = &cpu_rq(i)->rt;

		raw_spin_lock(&freezer_rq->freezer_runtime_lock);
		freezer_rq->freezer_runtime = global_freezer_runtime();
		raw_spin_unlock(&freezer_rq->freezer_runtime_lock);
	}
	raw_spin_unlock_irqrestore(&def_freezer_bandwidth.freezer_runtime_lock, flags);

	return 0;
}
#endif /* CONFIG_SYSCTL */

#ifdef CONFIG_SYSCTL
static int sched_freezer_global_validate(void)
{
	if ((sysctl_sched_freezer_runtime != RUNTIME_INF) &&
		((sysctl_sched_freezer_runtime > sysctl_sched_freezer_period) ||
		 ((u64)sysctl_sched_freezer_runtime *
			NSEC_PER_USEC > max_freezer_runtime)))
		return -EINVAL;

	return 0;
}

static void sched_freezer_do_global(void)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&def_freezer_bandwidth.freezer_runtime_lock, flags);
	def_freezer_bandwidth.freezer_runtime = global_freezer_runtime();
	def_freezer_bandwidth.freezer_period = ns_to_ktime(global_freezer_period());
	raw_spin_unlock_irqrestore(&def_freezer_bandwidth.freezer_runtime_lock, flags);
}

static int sched_freezer_handler(struct ctl_table *table, int write, void *buffer,
		size_t *lenp, loff_t *ppos)
{
	int old_period, old_runtime;
	static DEFINE_MUTEX(mutex);
	int ret;

	mutex_lock(&mutex);
	old_period = sysctl_sched_freezer_period;
	old_runtime = sysctl_sched_freezer_runtime;

	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);

	if (!ret && write) {
		ret = sched_freezer_global_validate();
		if (ret)
			goto undo;

		ret = sched_dl_global_validate();
		if (ret)
			goto undo;

		ret = sched_freezer_global_constraints();
		if (ret)
			goto undo;

		sched_freezer_do_global();
		sched_dl_do_global();
	}
	if (0) {
undo:
		sysctl_sched_freezer_period = old_period;
		sysctl_sched_freezer_runtime = old_runtime;
	}
	mutex_unlock(&mutex);

	return ret;
}

static int sched_rr_handler(struct ctl_table *table, int write, void *buffer,
		size_t *lenp, loff_t *ppos)
{
	int ret;
	static DEFINE_MUTEX(mutex);

	mutex_lock(&mutex);
	ret = proc_dointvec(table, write, buffer, lenp, ppos);
	/*
	 * Make sure that internally we keep jiffies.
	 * Also, writing zero resets the timeslice to default:
	 */
	if (!ret && write) {
		sched_rr_timeslice =
			sysctl_sched_rr_timeslice <= 0 ? RR_TIMESLICE :
			msecs_to_jiffies(sysctl_sched_rr_timeslice);

		if (sysctl_sched_rr_timeslice <= 0)
			sysctl_sched_rr_timeslice = jiffies_to_msecs(RR_TIMESLICE);
	}
	mutex_unlock(&mutex);

	return ret;
}
#endif /* CONFIG_SYSCTL */

#ifdef CONFIG_SCHED_DEBUG
void print_freezer_stats(struct seq_file *m, int cpu)
{
	freezer_rq_iter_t iter;
	struct freezer_rq *freezer_rq;

	rcu_read_lock();
	for_each_freezer_rq(freezer_rq, iter, cpu_rq(cpu))
		print_freezer_rq(m, cpu, freezer_rq);
	rcu_read_unlock();
}
#endif /* CONFIG_SCHED_DEBUG */