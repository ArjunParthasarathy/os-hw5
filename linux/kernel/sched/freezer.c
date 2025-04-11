/*
* Freezer Scheduling Class (implements round-robin)
*/
int sched_freezer_timeslice = FREEZER_TIMESLICE;
/* More than 4 hours if BW_SHIFT equals 20. */
static const u64 max_freezer_runtime = MAX_BW;
//static int do_sched_freezer_period_timer(struct freezer_bandwidth *freezer_b, int overrun);

void init_freezer_rq(struct freezer_rq *freezer_rq)
{
	return NULL;

#if defined CONFIG_SMP
#endif /* CONFIG_SMP */
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
	return NULL;
}

static inline struct freezer_rq *freezer_rq_of_se(struct sched_freezer_entity *freezer_se)
{
	return NULL;
}

void unregister_freezer_sched_group(struct task_group *tg) { }

void free_freezer_sched_group(struct task_group *tg) { }

int alloc_freezer_sched_group(struct task_group *tg, struct task_group *parent)
{
	return 1;
}

#ifdef CONFIG_SMP

static inline int has_pushable_tasks_freezer(struct rq *rq)
{
	return 0;
}

#endif /* CONFIG_SMP */

typedef struct freezer_rq *freezer_rq_iter_t;

#define for_each_freezer_rq(freezer_rq, iter, rq) \
	for ((void) iter, freezer_rq = &rq->freezer; freezer_rq; freezer_rq = NULL)

#define for_each_sched_freezer_entity(freezer_se) \
	for (; freezer_se; freezer_se = NULL)


#ifdef CONFIG_SMP
#else /* !CONFIG_SMP */
static inline void balance_runtime_freezer(struct freezer_rq *freezer_rq) {}
#endif /* CONFIG_SMP */

static void update_curr_freezer(struct rq *rq)
{
	return;
}

static void enqueue_freezer_entity(struct sched_freezer_entity *freezer_se, unsigned int flags)
{

	return;
}

static void dequeue_freezer_entity(struct sched_freezer_entity *freezer_se, unsigned int flags)
{
	return;
}

/*
 * Adding/removing a task to/from rq:
 */
static void
enqueue_task_freezer(struct rq *rq, struct task_struct *p, int flags)
{
	return;
}

/* Remove task from runqueue */
static void dequeue_task_freezer(struct rq *rq, struct task_struct *p, int flags)
{
	return;
}

/*
 * Put task to the end of the run list without the overhead of
 * dequeue followed by enqueue.
 */
static void
requeue_freezer_entity(struct freezer_rq *freezer_rq, struct sched_freezer_entity *freezer_se)
{
	return;
}

static void requeue_task_freezer(struct rq *rq, struct task_struct *p)
{
	return;
}

/* No need for lock here b/c its called w/ rq lock by scheduler */
static void yield_task_freezer(struct rq *rq)
{
	trace_printk("yield_task_freezer()\n");
}

#ifdef CONFIG_SMP
static struct rq *find_lowest_rq_freezer(struct task_struct *task);

/* find which CPU to put this runqueue on - the one with the lowest num procs */
static int select_task_rq_freezer(struct task_struct *p, int cpu, int flags)
{
	return 0;
}

static int balance_freezer(struct rq *rq, struct task_struct *p, struct rq_flags *rf)
{
	return 0;
}
#endif

// /*
//  * Preempt the current task with a newly woken task if needed:
//  */
static void wakeup_preempt_freezer(struct rq *rq, struct task_struct *p, int flags)
{
	return;
}

/* NO-OP */
static inline void set_next_task_freezer(struct rq *rq, struct task_struct *p, bool first)
{
	return;
}

static struct sched_freezer_entity *pick_next_freezer_entity(struct freezer_rq *freezer_rq)
{
	return NULL;
}

static struct task_struct *_pick_next_task_freezer(struct rq *rq)
{
	return NULL;
}

static struct task_struct *pick_task_freezer(struct rq *rq)
{
	return NULL;
}

static struct task_struct *freezer_steal_task(struct rq *rq);

static struct task_struct *pick_next_task_freezer(struct rq *rq)
{
	return NULL;
}

static void put_prev_task_freezer(struct rq *rq, struct task_struct *p)
{
	return;
}

#ifdef CONFIG_SMP

/* returns true if we can run this task on this CPU */
static int can_pick_freezer_task(struct rq *rq, struct task_struct *p, int cpu)
{
	return 0;
}

/*
 * Steal a task from another CPU
 */
static struct task_struct *freezer_steal_task(struct rq *this_rq)
{
	return NULL;
}

static DEFINE_PER_CPU(cpumask_var_t, local_cpu_mask);

/* find the runqueue with the lowest number of tasks */
static struct rq *find_lowest_rq_freezer(struct task_struct *task)
{
	return NULL;
}

/* Will lock the rq it finds */
static struct rq *find_lock_lowest_rq_freezer(struct task_struct *task, struct rq *rq)
{
	return NULL;
}

#ifdef HAVE_RT_PUSH_IPI

/*
 * If we are not running and we are not going to reschedule soon, we should
 * try to push tasks away now
 */
static void task_woken_freezer(struct rq *rq, struct task_struct *p)
{
	return;
}

/* Assumes rq->lock is held */
static void rq_online_freezer(struct rq *rq)
{
	return;
}
static void rq_offline_freezer(struct rq *rq)
{
	return;
}

/*
 * When switch from the freezer queue, we bring ourselves to a position
 * that we might want to pull freezer tasks from other runqueues.
 */
static void switched_from_freezer(struct rq *rq, struct task_struct *p)
{
	trace_printk("switched_from_freezer()\n");
}

void __init init_sched_freezer_class(void)
{
	trace_printk("init_sched_freezer_class()\n");
}
#endif

/*
 * When switching a task to freezer, we may overload the runqueue
 * with freezer tasks. In this case we try to push them off to
 * other runqueues.
 */
/* For freezer this function doesn't do anything */
static void switched_to_freezer(struct rq *rq, struct task_struct *p)
{
	trace_printk("switched_to_freezer()\n");
#ifdef CONFIG_SMP
#endif 
}

/*
 * Priority of the task has changed. This may cause
 * us to initiate a push or pull.
 */
static void prio_changed_freezer(struct rq *rq, struct task_struct *p, int oldprio)
{
	return;
}

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