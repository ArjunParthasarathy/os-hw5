/*
 * Adding/removing a task to/from a priority array:
 */
static void
enqueue_task_freezer(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_rt_entity *fr_se = &p->fe;

	if (flags & ENQUEUE_WAKEUP)
		fr_se->timeout = 0;

	check_schedstat_required();
	update_stats_wait_start_rt(rt_rq_of_se(rt_se), rt_se);

	enqueue_rt_entity(fr_se, flags);

	if (!task_current(rq, p) && p->nr_cpus_allowed > 1)
		enqueue_pushable_task(rq, p);
}

static void dequeue_task_freezer(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_freezer_entity *fr_se = &p->fe;

	update_curr_rt(rq);
	dequeue_rt_entity(fr_se, flags);

	dequeue_pushable_task(rq, p);
}


/*
 * freezer task scheduling class.
 */

#ifdef CONFIG_SMP
static int
select_task_rq_freezer(struct task_struct *p, int cpu, int flags)
{
	return task_cpu(p);
}

static int
balance_freezer(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
{
	return WARN_ON_ONCE(1);
}
#endif

static void wakeup_preempt_freezer(struct rq *rq, struct task_struct *p, int flags)
{
	resched_curr(rq);
}

static void put_prev_task_freezer(struct rq *rq, struct task_struct *prev)
{
}

static void set_next_task_freezer(struct rq *rq, struct task_struct *next, bool first)
{
	//update_idle_core(rq);
	schedstat_inc(rq->sched_goidle);
}

#ifdef CONFIG_SMP
static struct task_struct *pick_task_freezer(struct rq *rq)
{
	return rq->curr;
}
#endif

struct task_struct *pick_next_task_freezer(struct rq *rq)
{
	struct task_struct *next = rq->curr;

	set_next_task_freezer(rq, next, true);

	return next;
}

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
}

static void switched_to_freezer(struct rq *rq, struct task_struct *p)
{
	BUG();
}

static void
prio_changed_freezer(struct rq *rq, struct task_struct *p, int oldprio)
{
	BUG();
}

static void update_curr_freezer(struct rq *rq)
{
}


/*
 * Freezer scheduling policy implementation;
 */
DEFINE_SCHED_CLASS(freezer_sched_class) = {

	/* TODO enqueue/yield */

	/* dequeue is not valid, we print a debug message there: */
	.dequeue_task		= dequeue_task_freezer,

	.wakeup_preempt		= wakeup_preempt_freezer,

	.pick_next_task		= pick_next_task_freezer,
	.put_prev_task		= put_prev_task_freezer
,
	.set_next_task          = set_next_task_freezer
,

#ifdef CONFIG_SMP
	.balance		= balance_freezer,
	.pick_task		= pick_task_freezer,
	.select_task_rq		= select_task_rq_freezer,
	.set_cpus_allowed	= set_cpus_allowed_common,
#endif

	.task_tick		= task_tick_freezer,

	.prio_changed		= prio_changed_freezer,
	.switched_to		= switched_to_freezer,
	.update_curr		= update_curr_freezer,
};