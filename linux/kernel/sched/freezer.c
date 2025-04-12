/*
 * Freezer Scheduling Class (implements round-robin)
 */
int sched_freezer_timeslice = FREEZER_TIMESLICE;
/* More than 4 hours if BW_SHIFT equals 20. */
static const u64 max_freezer_runtime = MAX_BW;

void init_freezer_rq(struct freezer_rq *freezer_rq)
{
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

static inline int has_pushable_tasks_freezer(struct rq *rq)
{
	return 0;
}

#else

#endif /* CONFIG_SMP */

static inline int on_freezer_rq(struct sched_freezer_entity *freezer_se)
{
	return freezer_se->on_rq;
}

typedef struct freezer_rq *freezer_rq_iter_t;

#define for_each_freezer_rq(freezer_rq, iter, rq) \
	for ((void) iter, freezer_rq = &rq->freezer; freezer_rq; freezer_rq = NULL)

#define for_each_sched_freezer_entity(freezer_se) \
	for (; freezer_se; freezer_se = NULL)


#ifdef CONFIG_SMP

#else /* !CONFIG_SMP */
static inline void balance_runtime_freezer(struct freezer_rq *freezer_rq) {}
#endif /* CONFIG_SMP */

static void update_curr_freezer(struct rq *rq) {}

static void enqueue_freezer_entity(struct sched_freezer_entity *freezer_se, unsigned int flags)
{

	struct freezer_rq *freezer_rq = freezer_rq_of_se(freezer_se);
	struct list_head *queue = &freezer_rq->active;
	list_add_tail(&freezer_se->run_list, queue);
	freezer_se->on_list = 1;
	freezer_se->on_rq = 1;
	
	freezer_rq->freezer_rq_len += 1;
}

static void dequeue_freezer_entity(struct sched_freezer_entity *freezer_se, unsigned int flags)
{
	struct freezer_rq *freezer_rq = freezer_rq_of_se(freezer_se);

	if (freezer_se->on_list) {
		// list_del() maintains the order of things on the queue
		list_del_init(&freezer_se->run_list);
	}
	freezer_se->on_list = 0;
	freezer_se->on_rq = 0;
	freezer_rq->freezer_rq_len -= 1;
}

/*
 * Adding/removing a task to/from rq:
 */
static void
enqueue_task_freezer(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_freezer_entity *freezer_se = &p->freezer;

	enqueue_freezer_entity(freezer_se, flags);
}

/* Remove task from runqueue */
static void dequeue_task_freezer(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_freezer_entity *freezer_se = &p->freezer;

	dequeue_freezer_entity(freezer_se, flags);
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

		list_move_tail(&freezer_se->run_list, queue);
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
static void yield_task_freezer(struct rq *rq) {}

#ifdef CONFIG_SMP
static struct rq *find_lowest_rq_freezer(struct task_struct *task);

/* find which CPU to put this runqueue on - the one with the lowest num procs */
static int
select_task_rq_freezer(struct task_struct *p, int cpu, int flags)
{
	struct rq *rq = find_lowest_rq_freezer(p);
	WARN_ON_ONCE(!rq);
	int target = cpu_of(rq);

	return target;
}

static int balance_freezer(struct rq *rq, struct task_struct *p, struct rq_flags *rf)
{
	return 0;
}
#endif /* CONFIG_SMP */

static void wakeup_preempt_freezer(struct rq *rq, struct task_struct *p, int flags) {}

/* NO-OP */
static inline void set_next_task_freezer(struct rq *rq, struct task_struct *p, bool first) {}

static struct sched_freezer_entity *pick_next_freezer_entity(struct freezer_rq *freezer_rq)
{
	struct sched_freezer_entity *next = NULL;

	WARN_ON_ONCE(!freezer_rq);
	struct list_head *queue = &freezer_rq->active;

	if (SCHED_WARN_ON(list_empty(queue)))
		return NULL;
	next = list_entry(queue->next, struct sched_freezer_entity, run_list);

	return next;
}

static struct task_struct *_pick_next_task_freezer(struct rq *rq)
{
	struct sched_freezer_entity *freezer_se;
	struct freezer_rq *freezer_rq  = &rq->freezer;

	WARN_ON_ONCE(!freezer_rq);
	freezer_se = pick_next_freezer_entity(freezer_rq);
	if (unlikely(!freezer_se))
		return NULL;
	
	return freezer_task_of(freezer_se);
}

static struct task_struct *pick_task_freezer(struct rq *rq)
{
	return NULL;
}

static struct task_struct *freezer_steal_task(struct rq *rq);

static struct task_struct *pick_next_task_freezer(struct rq *rq)
{
	struct task_struct *p;

	/* Nothing in queue so we steal from CPU w/ least tasks */
	if (!sched_freezer_runnable(rq))
		return freezer_steal_task(rq);	

	p = _pick_next_task_freezer(rq);
	return p;
}

static void put_prev_task_freezer(struct rq *rq, struct task_struct *p) {}

#ifdef CONFIG_SMP

/* returns true if we can run this task on this CPU */
static int can_pick_freezer_task(struct rq *rq, struct task_struct *p, int cpu)
{
	/* If the task is not currently running, its allowed to be migrated,
	 * and its CPU mask lets it run on this CPU
	 */
	if (!task_on_cpu(rq, p) && !is_migration_disabled(p) &&
		cpumask_test_cpu(cpu, &p->cpus_mask))
		return 1;

	return 0;
}

/*
 * Steal a task from another CPU
 */
static struct task_struct *freezer_steal_task(struct rq *this_rq)
{
	struct list_head *head, *pos;
	struct sched_freezer_entity *freezer_se;
	struct task_struct *task;
	int i = 0;
	int this_cpu = cpu_of(this_rq);
	struct rq *rq_i;

	/* Loop through all CPUs and steal a task from
	 * someone (doesn't matter who)
	 * We keep looping til we find a task on some CPU which can be run on the current CPU (based on its CPU mask)
	 */
	for_each_cpu(i, cpu_present_mask) {
		if (i == this_cpu)
			continue;

		rq_i = cpu_rq(i);
		double_lock_balance(this_rq, rq_i);

		if (!sched_freezer_runnable(rq_i)) {
			double_unlock_balance(this_rq, rq_i);
			continue;
		}
		double_lock_balance(this_rq, rq_i);
		head = &rq_i->freezer.active;
		list_for_each(pos, head) {
			freezer_se = list_entry(pos, struct sched_freezer_entity, run_list);
			BUG_ON(!freezer_se);
			task = freezer_task_of(freezer_se);
			/* If the task can run on this cpu and its allowed to be migrated */
			if (can_pick_freezer_task(rq_i, task, this_cpu)) {
				// Remove task from other cpu and add to ours
				deactivate_task(rq_i, task, 0);
				set_task_cpu(task, this_cpu);
				activate_task(this_rq, task, 0);
				/* this CPU has the idle task right now so preempt it to the new task
				 *  we just got
				 */
				resched_curr(this_rq);
				double_unlock_balance(this_rq, rq_i);
				//trace_printk("Stole task from other CPU");
				return task;
			}
		}
		double_unlock_balance(this_rq, rq_i);
		//trace_printk("Trying next CPU");
	}

	/* No other task found on any other CPU :( */
	//trace_printk("No found tasks");
	return NULL;
}

static DEFINE_PER_CPU(cpumask_var_t, local_cpu_mask);

/* find the runqueue with the lowest number of tasks */
static struct rq *find_lowest_rq_freezer(struct task_struct *task)
{
	int cpu = task_cpu(task);
	struct rq *this_rq = task_rq(task);

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

	int min_rq_len = this_rq->freezer.freezer_rq_len;

	for_each_cpu(i, cpu_present_mask) {
		rq_i = cpu_rq(i);

		/* use irq b/c we're looking across cpus so have multiple interrupts */
		double_lock_balance(this_rq, rq_i);
		if (rq_i->freezer.freezer_rq_len < min_rq_len) {
			min_rq = rq_i;
			min_rq_len = rq_i->freezer.freezer_rq_len;
		}
		double_unlock_balance(this_rq, rq_i);
	}
	
	return min_rq;
}

/* Will lock the rq it finds */
static struct rq *find_lock_lowest_rq_freezer(struct task_struct *task, struct rq *rq)
{
	struct rq *lowest_rq = find_lowest_rq_freezer(task);

	WARN_ON_ONCE(!lowest_rq);
	/* If min_rq is not our own rq (which is already locked),
	 * then we need to lock before return
	 */
	if (rq != lowest_rq)
		double_lock_balance(rq, lowest_rq);

	return lowest_rq;
}

void task_woken_freezer(struct rq *rq, struct task_struct *p) {}

/* Assumes rq->lock is held */
static void rq_online_freezer(struct rq *rq) {}

static void rq_offline_freezer(struct rq *rq) {}

/*
 * When switch from the freezer queue, we bring ourselves to a position
 * that we might want to pull freezer tasks from other runqueues.
 */
static void switched_from_freezer(struct rq *rq, struct task_struct *p) {}

void __init init_sched_freezer_class(void) {}
#endif /* CONFIG_SMP */

/*
 * When switching a task to freezer, we may overload the runqueue
 * with freezer tasks. In this case we try to push them off to
 * other runqueues.
 */
/* For freezer this function doesn't do anything */
static void switched_to_freezer(struct rq *rq, struct task_struct *p)
{
	#ifdef CONFIG_SMP
	#endif
}

/*
 * Priority of the task has changed. This may cause
 * us to initiate a push or pull.
 */
static void
prio_changed_freezer(struct rq *rq, struct task_struct *p, int oldprio){}

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