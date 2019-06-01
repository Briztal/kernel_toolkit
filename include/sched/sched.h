/*sched.h - kerneltk - GPLV3, copyleft 2019 Raphael Outhier;*/

#ifndef KERNELTK_SCHED_H
#define KERNELTK_SCHED_H

#include <types.h>

#include <struct/list.h>


struct scheduler;
struct sprocess;
struct sprim;
struct sthread;


/*TODO GENERAL DOCUMENTATION;*
 * TODO WORD ON REGISTRATION AND UNREGISTRATION, UNDEF BEHAVIOUR IF UNREGISTERED
 * OBJECT IS USED;*/

/*------------------------------------------------------------- scheduler task*/

/**
 * Scheduler processes and tasks have one of the following states;
 */
enum sched_status {

	/*The object is unregistered;*/
			SCHED_STATUS_UNREGISTERED = 0,

	/*The object is stopped;*/
			SCHED_STATUS_STOPPED = 1,

	/*The object is active;*/
			SCHED_STATUS_ACTIVE = 2

};


/**
 * the scheduler task : contains all data related to a task that must be 
 * scheduled by the scheduler, and assigned to a thread for execution;
 * It is registered to a scheduler process, and can be stopped by one scheduler
 * primitive; It also can take the ownership of several primitives, and can
 * see its priority overridden by a scheduler primitive;
 */

struct stask {

	/*
	 * Sched mgt;
	 */

	/*The status of the task;*/
	enum sched_status t_status;

	/*Active tasks are referenced by the scheduler in a linked list;*/
	struct dlist t_sched_list;

	/*The index of the last commit the task was active;*/
	usize t_commit;

	/*
	 * process;
	 */

	/*The scheduler the task is registered to if any;*/
	struct sprocess *t_process;

	/*Tasks belonging to the same process are referenced in a linked list;*/
	struct dlist t_siblings;

	/*
	 * Ownership;
	 */

	/*The number of primitive we own;*/
	usize t_nb_owned_primitives;

	/*
	 * Stopper primitive;
	 */

	/*The ref of the primitive that stopped us if any;*/
	struct sprim *t_stopper;

	/*The list of tasks stopped by the same primitive;*/
	struct dlist t_stopped;

	/*
	 * Overriding;
	 */

	/*The number of primitives that override us; TODO USEFULL ?*/
	usize t_nb_overrides;

	/*The list of overriding synchronization primitives the task owns;*/
	struct dlist t_overriders;

	/*
	 * Owner thread;
	 */

	/*The ref of the last thread to have executed the task;*/
	struct sthread *t_thread;

	/*The list of tasks stopped executed by the same thread;*/
	struct dlist t_history;

};


/*If set, the overriding tree of the task has been modified since this 
* flag was reset;*/
#define STASK_STATUS_UPDATED ((u8) (1 << 1))

/*-------------------------------------------------------- scheduler primitive*/

/**
 * the scheduler primitive contains all data related to a synchronization 
 * primitive, whose objective is to stop an undefined number of scheduler 
 * tasks, to be owned by an undefined number of scheduler tasks, and to 
 * override the priority of a scheduler task;
 * A primitive is registered to a scheduler process and can only operate on
 * tasks that are registered to the same process;
 */

struct sprim {

	/*The process the primitive is registered to;*/
	struct sprocess *p_process;

	/*Prims belonging to the same process are referenced in a linked list;*/
	struct dlist p_siblings;

	/*Primitive status flags;*/
	u8 p_status;

	/*
	 * Ownership;
	 */

	/*The number of task that own us;*/
	usize p_nb_owning_tasks;

	/*
	 * Stopped tasks;
	 */

	/*The number of tasks we stopped;*/
	usize nb_stopped_tasks;

	/*The list of tasks we stopped;*/
	struct dlist p_stopped;

	/*
	 * Overriding;
	 */

	/*The task we override;*/
	struct stask *p_overridden;

	/*The list of synchronization primitives overriding the same task;*/
	struct dlist p_overriders;

};

/*If set, the overriding tree of the primitive has been modified since this
* flag was reset;*/
#define SCHED_PRIM_STATUS_UPDATED ((u8) (1 << 0))

/**
 * sched_prim_init : resets all fields of the provided primitive;
 * @param prim : the primitive to reset;
 */
void sched_prim_reset(struct sprim *prim);


/*-------------------------------------------------------------- sched process*/

/**
 * A scheduler process references a set of scheduler tasks, a set of scheduler
 * primitives, and a scheduler; It determines which tasks and primitives are
 * compatible and can be used in functions that involve the two types of
 * objects; Any attempt to call such function with incompatible objects will
 * result in an abort, if the source code has been compiled in debug mode;
 */
struct sprocess {

	/*The scheduler the process relates to;*/
	struct scheduler *p_sched;

	/*Processes are referenced by their scheduler in a linked list;*/
	struct dlist p_list;

	/*The status of the process;*/
	enum sched_status p_status;

	/*Tasks belonging to the same process are referenced in a linked list;*/
	struct dlist p_tasks;

	/*The number of tasks registered to the process;*/
	usize p_nb_tasks;

	/*Prims belonging to the same process are referenced in a linked list;*/
	struct dlist p_primitives;

	/*The number of primitives registered to the process;*/
	usize p_nb_primitives;

	/*The synchronization primitive used to stop the whole process;*/
	struct sprim p_prim;

};

/*----------------------------------------------------------- scheduler thread*/

/**
 * a scheduler thread is a unit whose purpose is to be assigned, reference, 
 * execute, stop and unregiser scheduler tasks;
 */

struct sthread {

	/*The scheduler the thread is registered to;*/
	struct scheduler *t_sched;

	/*Threads are referenced by their scheduler in a linked list;*/
	struct dlist t_list;

	/*The index of the last commit the thread was active;*/
	usize t_commit;

	/*The ref of the lastly active task;*/
	struct stask *t_task;

	/*The list of tasks executed by the thread; ordered by recency;*/
	struct dlist t_history;

	/*The number of tasks in the history;*/
	usize t_history_size;

};

/*------------------------------------------------------------------ scheduler*/

struct sched_ops {

	/*
	 * State transitions;
	 */

	/*Called when an unregistered task has been registered;*/
	void (*s_registered)(
			struct scheduler *sched,
			struct stask *task
	);

	/*Called when an active task has been unregistered;*/
	void (*s_unregistered)(
			struct scheduler *sched,
			struct stask *task
	);


	/*Called when an active task has been stopped;*/
	void (*s_stopped)(
			struct scheduler *sched,
			struct stask *task
	);

	/*Called when a stopped task has been activated;*/
	void (*s_resumed)(
			struct scheduler *sched,
			struct stask *task
	);


	/*
	 * Scheduling;
	 */

	/* Update priorities of all tasks according to the priority function and 
	 * dependencies between tasks;*/
	void (*s_schedule)(struct scheduler *sched);

	/*Assign a task to each thread, according to the priority order;*/
	void (*s_assign_all)(struct scheduler *sched);

	/*Assign an un-assigned task;*/
	struct stask *(*s_assign_one)(
			struct scheduler *sched,
			struct sthread *thread
	);


	/*
	 * Synchronization primitives;
	 */

	/*Report that a task has taken the ownership of a primitive;*/
	void (*s_primitive_override_taken)(
			struct scheduler *sched,
			struct sprim *prim,
			struct stask *task
	);

	/*Report that a task has released the ownership of a primitive;*/
	void (*s_primitive_override_released)(
			struct scheduler *sched,
			struct sprim *prim,
			struct stask *task
	);


	/*
	 * Task priorities;
	 */

	/*Notify the scheduler that the priority of a task has been updated;*/
	void (*s_task_priority_updtaed)(
			struct scheduler *sched,
			struct stask *task
	);

	/*Get the priority of a single task;*/
	usize (*s_get_task_priority)(
			struct scheduler *sched,
			struct stask *task
	);

	/*Determine the priority of a synchronization primitive;*/
	usize (*s_prim_get_prio)(
			struct sprim *prim
	);


};


struct scheduler {

	/*Lock;*/
	struct arch_spinlock s_lock;
	
	/*Operations;*/
	struct sched_ops *s_ops;

	/*
	 * Commit;
	 */

	/*A flag set if a commit is opened;*/
	u8 s_commit_opened;

	/*The index of the current commit;*/
	usize s_commit_index;

	/*
	 * Tasks;
	 */

	/*The list of active tasks;*/
	struct dlist s_actives;

	/*
	 * Processes;
	 */

	/*The list of processes;*/
	struct dlist s_processes;

	/*The number of processes;*/
	usize s_nb_processes;

	/*
	 * Threads;
	 */

	/*The list of threads;*/
	struct dlist s_threads;

	/*The number of threads;*/
	usize s_nb_threads;


};

/*TODO CTOR DTOR*/

/*------------------------------------------------------------ access barriers*/

/**
 * sched_lock : attempts to locks the scheduler;
 * This function must be called before any operation is made on the scheduler;
 * @param sched : the scheduler to lock;
 * @return 0 if the lock succeeded, 1 if the scheduler was already locked;
 */
u8 sched_lock(struct scheduler *sched);

/**
 * sched_unlock : unlocks the scheduler; aborts if the scheduler is
 * unlocked;
 * @param sched : the scheduler to unlock;
 */
void sched_unlock(struct scheduler *sched);

/*-------------------------------------------------------- scheduler functions*/

/**
 * sched_register_thread : registers the thread in the scheduler;
 * @param sched : the scheduler to update;
 * @param thread : the thread to register;
 */
void sched_register_thread(struct scheduler *sched, struct sthread *thread);

/**
 * sched_register_process : registers the process in the scheduler;
 * @param sched : the scheduler to update;
 * @param prc : the process to register;
 */
void sched_register_process(struct scheduler *sched, struct sprocess *prc);

/**
 * sched_register_process : reactivates all tasks stopped by the process
 * primitive;
 * Aborts if the process is not stopped;
 * @param sched : the scheduler to update;
 * @param prc : the process to register;
 */
void sched_resume_process(struct sprocess *prc);

/**
 * sched_open_commit : opens a new commit for the provided scheduler;
 * commit functions will be authorised after;
 * Aborts if a commit is already opened;
 * @param sched : the scheduler to open a commit in;
 */
void sched_open_commit(struct scheduler *sched);


/* Following scheduler functions can only be executed when a commit is opened,
 * as it gives the certitude they are idle;
 * Each one with no exception will abort if called when no commit is opened;
 */

/**
 * sched_unregister_thread : unregisters the thread from its scheduler;
 * @param thread : the thread to unregister;
 */
void sched_unregister_thread(struct sthread *thread);


/**
 * sched_unregister_process : removes any task registered to the process from
 * the scheduler list and unregisters the process from its scheduler;
 * all tasks and primitives can be considered unregistered, even if links
 * are not explicitly reset;
 * @param prc : the process to unregister;
 */
void sched_unregister_process(struct sprocess *prc);

/**
 * sched_pause_process : stops all active tasks registered to the process,
 * relatively to the process's synchronization primitive;
 * Aborts if the
 * @param prc : the process to stop;
 */
void sched_pause_process(struct sprocess *prc);

/**
 * sched_open_commit : closes the current commit for the provided
 * scheduler; no commit function will be authorised after;
 * @param sched : the scheduler to open a commit in;
 */
void sched_close_commit(struct scheduler *sched);

/*---------------------------------------------------------- process functions*/

/**
 * process_register_task : removes the task from its eventual scheduler
 * list, un-stops the task relatively to its stopping primitive if any, inserts
 * it in the active list, and calls the s_activated scheduler function;
 * Aborts if the task is not ;
 * @param sched : the scheduler that the task relates to;
 * @param task : the task that must be stopped by the primitive;
 */
void process_register_task(struct sprocess *prc, struct stask *task);

/**
 * process_unregister_task : Fetch the task executed by the thread, and
 * resumes it; then, unregisters it from its process and its scheduler and make
 * any overriding primitive release its override; finally, assigns a new task
 * to the thread; returns 1 if the task was still owning primitives when
 * unregistered;
 * Aborts if no task is being executed;
 * @param thread : the thread executing the task that must be unregistered;
 * @return 1 if the task owned primitives when unregistered;
 */
err_t process_unregister_task(struct sthread *thread);

/**
 * primitive_register : registers the provided primitive to the provided
 * process;
 * @param prc : the process to register the primitive to;
 * @param prim : the primitive to register;
 */
void process_register_prim(struct sprocess *prc, struct sprim *prim);

/**
 * primitive_unregister : un-stops all tasks stopped by the primitive,
 * un-override the overridden task if any and unregisters the primitive from
 * its process;
 * @param prim : the primitive to unregister;
 * @return 1 if the primitive's ownership counter is not null, else;
 */
err_t process_unregister_prim(struct sprim *prim);


/*-------------------------------------------------------- primitive functions*/

/**
 * primitive_take_owner : give a task the ownership of a primitive;
 * Both ownership counters of task and primitive are increased;
 * The task must be active;
 * @param prim : the primitive that the task must take the ownership of;
 * @param task : the task that must take the ownership of the primitive;
 */
void primitive_take_ownership(struct sprim *prim, struct stask *task);

/**
 * sched_prim_release_owner : removes the task's ownership of the primitive;
 * Both ownership counters of task and primitive are decreased;
 * If one counter is null before decrease, the function returns 1;
 * The task must be active;
 * @param prim : the primitive that the task must release the ownership of;
 * @param task : the task that must release the ownership of the primitive;
 * @return 1 if an ownership counter is null before release, 0 else;
 */
err_t primitive_release_ownership(struct sprim *prim, struct stask *task);

/**
 * primitive_override_task : mark the task overridden by the primitive;
 * The primitive is inserted in the list of the task's overriding primitives;
 * If the primitive already has an owner, its ownership is released before;
 * The task must be active;
 * @param prim : the primitive that must override the task;
 * @param task : the task the primitive must override;
 */
void primitive_override_task(struct sprim *prim, struct stask *task);

/**
 * primitive_un_override : if the primitive overrides a task, the overriding
 * is released; primitive is removed from the task's overrider list;
 * @param prim : the primitive that the task must take the ownership of;
 */
void primitive_unoverride_task(struct sprim *prim);

/**
 * primitive_activate_task : removes the task from its eventual scheduler
 * list, un-stops the task relatively to its stopping primitive if any, inserts
 * it in the active list, and calls the s_activated scheduler function;
 * Aborts if the task is not stopped;
 * @param prim : the primitive the task relates to;
 * @param task : the task that must be stopped by the primitive;
 */
void primitive_resume_task(struct stask *task);

/**
 * primitive_stop_thread : stops the task being executed by the thread, and
 * assign a new task to it;
 * Aborts if no task is being executed;
 * @param prim : the primitive that will stop the task;
 * @param thread : the thread executing the task that must be stopped;
 */
void primitive_stop_thread(struct sprim *prim, struct sthread *thread);

#endif /*KERNELTK_SCHED_H*/
