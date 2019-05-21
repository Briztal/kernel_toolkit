/*sched.c - kerneltk - GPLV3, copyleft 2019 Raphael Outhier;*/

#include <sched/sched.h>

#include <debug.h>

#include <spinlock.h>


/*-------------------------------------------------------------- active checks*/

static __inline__ u8 task_active(struct stask *task) {

	return (task->t_commit == task->t_process->p_sched->s_commit_index);

}

/*TODO REDO CHECKINGS;*/

static __inline__ u8 thread_active(struct sthread *proc) {

	return (proc->t_commit == proc->t_sched->s_commit_index) &&
		   (proc->t_task != 0);

}


/*--------------------------------------------------------- update propagation*/

/**
 * task_propagate_update : marks the task and its parents in the
 * tasks-primitives ownership tree, until an object that is marked updated is
 * encountered, or the root of the tree is reached;
 * @param task : the task that is to be marked updated;
 */
static void task_propagate_update(struct stask *task) {

	struct sprim *prim;

	while (1) {

		/*If the task is already marked updated :*/
		if (task->t_status & STASK_STATUS_UPDATED) {
			break;
		}

		/*Mark the task updated;*/
		task->t_status |= STASK_STATUS_UPDATED;

		/*Fetch the task's owner;*/
		prim = task->t_stopper;

		/*If the task has no owner, complete;*/
		if (!prim)
			break;


		/*If the primitive is already marked updated, complete*/
		if (prim->p_status & SCHED_PRIM_STATUS_UPDATED) {
			break;
		}

		/*Mark the primitive updated;*/
		prim->p_status |= SCHED_PRIM_STATUS_UPDATED;

		/*Fetch the primitive's overridden task;*/
		task = prim->p_overridden;

		/*If the primitive has no owner, complete;*/
		if (!task)
			break;

	}

}

/**
 * primitive_propagate_update : marks the primitive and its parents in the
 * tasks-primitives ownership tree, until an object that is marked updated is
 * encountered, or the root of the tree is reached;
 * @param prim : the primitive that is to be marked updated;
 */
static void primitive_propagate_update(struct sprim *prim) {

	struct stask *task;

	while (1) {

		/*If the primitive is already marked updated, complete*/
		if (prim->p_status & SCHED_PRIM_STATUS_UPDATED) {
			break;
		}

		/*Mark the primitive updated;*/
		prim->p_status |= SCHED_PRIM_STATUS_UPDATED;

		/*Fetch the primitive's overridden task;*/
		task = prim->p_overridden;

		/*If the primitive has no owner, complete;*/
		if (!task)
			break;

		/*If the task is already marked updated :*/
		if (task->t_status & STASK_STATUS_UPDATED) {
			break;
		}

		/*Mark the task updated;*/
		task->t_status |= STASK_STATUS_UPDATED;

		/*Fetch the task's owner;*/
		prim = task->t_stopper;

		/*If the task has no owner, complete;*/
		if (!prim)
			break;

	}
}



/*----------------------------------------------------------- thread functions*/

/**
 * proc_unregister_task : unregisters the task from its processor if any;
 * @param task : the task that must be unregistered from its processor;
 */
static void thread_unregister_task(struct stask *task) {

	struct sthread *proc;

	/*Fetch the processor the task relates to;*/
	proc = task->t_thread;

	/*If the task has no processor, complete;*/
	if (!proc) {
		return;
	}

	/*Update the proc history size;*/
	proc->t_history_size--;

	/*Reset the processor reference;*/
	task->t_thread = 0;

	/*Remove the task from its history list;*/
	dlist_remove(&task->t_history);

	/*If the processor was executing this task when active :*/
	if (proc->t_task == task) {

		/*Reset the proc's active task ref;*/
		proc->t_task = 0;

		/*Mark the proc inactive;*/
		proc->t_commit = (usize) -1;

	}

}

/**
 * proc_assign_task : assigns the provided task to the provided processor;
 * If the task is already assigned to a different processor that the new one,
 * the task is unregistered from it before;
 * @param thread : the thread to assign the task to;
 * @param task : the task that must be assigned to the processor;
 */
void thread_assign_task(struct sthread *thread, struct stask *task) {

	struct scheduler *sched;

	/*Parameters check;*/
	ns_check(thread != 0);

	/*If the processor must be deactivated :*/
	if (!task) {

		/*Reset the thread's active task ref;*/
		thread->t_task = 0;

		/*Mark the thread inactive;*/
		thread->t_commit = (usize) -1;

		/*Complete;*/
		return;

	}

	/*Fetch the ref of the scheduler;*/
	sched = thread->t_sched;

	/*Check objects are registered to the same scheduler;*/
	ns_check(sched != 0);
	ns_check(task->t_process != 0);
	ns_check(task->t_process->p_sched == sched);

	/*If the task must be transferred :*/
	if (task->t_thread != thread) {

		/*Unregister the task from its processor if any;*/
		thread_unregister_task(task);

		/*Update the processor reference;*/
		task->t_thread = thread;

		/*Update the history size;*/
		thread->t_history_size++;

		/*Add the task in the thread's history list;*/
		dlist_insert_single_after(&thread->t_history, &task->t_history);

	}

	/*Set the task as the active one;*/
	thread->t_task = task;

	/*Mark the processor and the task active;*/
	thread->t_commit = task->t_commit = sched->s_commit_index;

}



/*-------------------------------------------------------- primitive functions*/


/**
 * primitive_take_owner : give a task the ownership of a primitive;
 * Both ownership counters of task and primitive are increased;
 * The task must be active;
 * @param prim : the primitive that the task must take the ownership of;
 * @param task : the task that must take the ownership of the primitive;
 */
void primitive_take_ownership(struct sprim *prim, struct stask *task) {

	/*Debug checks;*/
	ns_check(task != 0)
	ns_check(prim != 0)
	ns_check(task->t_process != 0);
	ns_check(prim->p_process == task->t_process);
	ns_check(task->t_process->p_sched != 0);
	ns_check(task->t_status == SCHED_STATUS_ACTIVE);

	/*Increase the couple of ownership counters;*/
	prim->p_nb_owning_tasks++;
	task->t_nb_owned_primitives++;

}

/**
 * sched_prim_release_owner : removes the task's ownership of the primitive;
 * Both ownership counters of task and primitive are decreased;
 * If one counter is null before decrease, the function returns 1;
 * The task must be active;
 * @param prim : the primitive that the task must release the ownership of;
 * @param task : the task that must release the ownership of the primitive;
 * @return 1 if an ownership counter is null before release, 0 else;
 */
err_t primitive_release_ownership(struct sprim *prim, struct stask *task) {

	/*Debug checks;*/
	ns_check(task != 0)
	ns_check(prim != 0)
	ns_check(task->t_process != 0);
	ns_check(prim->p_process == task->t_process);
	ns_check(task->t_process->p_sched != 0);
	ns_check(task->t_status == SCHED_STATUS_ACTIVE);

	/*Decrease the couple of counters if possible and report if not;*/
	if (!prim->p_nb_owning_tasks) {
		return 1;
	}
	prim->p_nb_owning_tasks--;

	/*Decrease the couple of ownership counters;*/
	if (!task->t_nb_owned_primitives) {
		return 1;
	}
	task->t_nb_owned_primitives--;

	/*Complete;*/
	return 0;

}


/**
 * primitive_reset_owner : resets the owner of the provided primitive, and
 * activates the first stopped task;
 * @param prim : the primitive to reset the owner of;
 */
static void _prim_un_override(
		struct sprim *prim
) {

	struct scheduler *sched;
	struct stask *task;

	/*Fetch the overridden task;*/
	task = prim->p_overridden;

	/*If the primitive overrides no task, complete;*/
	if (!task)
		return;

	/*Fetch the primitive's scheduler;*/
	sched = prim->p_process->p_sched;

	/*Reset the owner ref;*/
	prim->p_overridden = 0;

	/*Remove the primitive from the task's overriders list;*/
	dlist_remove(&prim->p_siblings);

	/*Update the number of overrider primitives;*/
	task->t_nb_overrides--;

	/*Propagate the task update;*/
	task_propagate_update(task);

	/*Report the overrider release;*/
	(*(sched->s_ops->s_primitive_override_released))(sched, prim, task);

}


/**
 * primitive_override_task : mark the task overridden by the primitive;
 * The primitive is inserted in the list of the task's overriding primitives;
 * If the primitive already has an owner, its ownership is released before;
 * If the task is not active, the function returns 1;
 * @param prim : the primitive that the task must take the ownership of;
 * @param task : the task that must take the ownership of the primitive;
 * @return 1 if the task is not active, 0 else;
 */
void primitive_override_task(struct sprim *prim, struct stask *task) {

	struct scheduler *sched;

	/*Debug checks;*/
	ns_check(task != 0)
	ns_check(prim != 0)
	ns_check(task->t_process != 0);
	ns_check(prim->p_process == task->t_process);
	ns_check(task->t_process->p_sched != 0);
	ns_check(task->t_status == SCHED_STATUS_ACTIVE);

	/*Fetch the scheduler of the primitive;*/
	sched = prim->p_process->p_sched;

	/*Un-override our task if any;*/
	_prim_un_override(prim);

	/*Update the primitive's overridden task;*/
	prim->p_overridden = task;

	/*Insert the primitive in the task's overrider list;*/
	dlist_insert_single_after(&task->t_overriders, &prim->p_siblings);

	/*Update the number of overrider primitives;*/
	task->t_nb_overrides++;

	/*Propagate the task update;*/
	task_propagate_update(task);

	/*Report the override take;*/
	(*(sched->s_ops->s_primitive_override_taken))(sched, prim, task);

}

/**
 * primitive_un_override : if the primitive overrides a task, the overriding
 * is released; primitive is removed from the task's overrider list;
 * @param prim : the primitive that the task must take the ownership of;
 * @param task : the task that must take the ownership of the primitive;
 */
void primitive_unoverride_task(struct sprim *prim) {

	/*Debug checks;*/
	ns_check(prim != 0)

	/*Un-override if required;*/
	_prim_un_override(prim);

}


/**
 * primitive_activate_task : removes the task from its eventual scheduler
 * list, un-stops the task relatively to its stopping primitive if any, inserts
 * it in the active list, and calls the s_activated scheduler function;
 * Aborts if the task is not stopped;
 * @param prim : the primitive the task relates to;
 * @param task : the task that must be stopped by the primitive;
 */
void primitive_resume_task(struct stask *task) {

	struct scheduler *sched;
	struct sprim *prim;

	/*Check parameter;*/
	ns_check(task != 0)
	ns_check(task->t_process != 0)

	/*Fetch the scheduler and the stopper primitive;*/
	sched = task->t_process->p_sched;
	prim = task->t_stopper;

	/*Debug checks;*/
	ns_check(sched != 0)
	ns_check(prim != 0);
	ns_check(prim->p_process == task->t_process);
	ns_check(task->t_status == SCHED_STATUS_STOPPED)

	/*Insert the task in the active list;*/
	dlist_insert_single_after(&sched->s_actives, &task->t_sched_list);

	/*Un-reference the primitive;*/
	task->t_stopper = 0;

	/*Remove the task of the primitive's stopped list;*/
	dlist_remove(&task->t_stopped);

	/*Update the number of stopped tasks;*/
	prim->nb_stopped_tasks--;

	/*Propagate the primitive update;*/
	primitive_propagate_update(prim);

	/*Update the state of the task;*/
	task->t_status = SCHED_STATUS_ACTIVE;

	/*Call the scheduler implementation hook;*/
	(*(sched->s_ops->s_resumed))(sched, task);

}


void primitive_stop_task(struct sprim *prim, struct stask *task) {

	struct scheduler *sched;

	/*Debug checks;*/
	ns_check(task != 0)
	ns_check(prim != 0)
	ns_check(task->t_process != 0);
	ns_check(prim->p_process == task->t_process);
	ns_check(task->t_process->p_sched != 0);
	ns_check(task_active(task) != 0);
	ns_check(task->t_stopper == 0);

	/*Fetch the scheduler;*/
	sched = task->t_process->p_sched;

	/*Reference the primitive;*/
	task->t_stopper = prim;

	/*Insert the task at the end of the primitive's stopped list;*/
	dlist_insert_single_before(&prim->p_stopped, &task->t_stopped);

	/*Update the number of stopped tasks;*/
	prim->nb_stopped_tasks++;

	/*Propagate the primitive update;*/
	primitive_propagate_update(prim);

	/*Remove the task from the active list;*/
	dlist_remove(&task->t_sched_list);

	/*Update the state of the task;*/
	task->t_status = SCHED_STATUS_STOPPED;

	/*Call the scheduler implementation hook;*/
	(*(sched->s_ops->s_stopped))(sched, task);

}


/**
 * primitive_stop_thread : stops the task being executed by the thread, and
 * assign a new task to it;
 * Aborts if no task is being executed;
 * @param prim : the primitive that will stop the task;
 * @param thread : the thread executing the task that must be stopped;
 */
void primitive_stop_thread(struct sprim *prim, struct sthread *thread) {

	struct scheduler *sched;
	struct stask *task;

	/*Check parameters;*/
	ns_check(prim != 0);
	ns_check(prim->p_process != 0);
	ns_check(thread != 0);
	ns_check(thread->t_sched != 0);
	ns_check(prim->p_process->p_sched == thread->t_sched);

	/*Fetch the scheduler and task;*/
	sched = thread->t_sched;
	task = thread->t_task;

	/*Stop the task;*/
	primitive_stop_task(prim, task);

	/*Assign a new task to the thread;*/
	(*(sched->s_ops->s_assign_one))(sched, thread);

}




/*---------------------------------------------------------- process functions*/

/**
 * process_register_task : initializes the task and registers it to the
 * provided process;
 * @param prc : the scheduler the task must be registered to;
 * @param task : the task that must be initialized and registered;
 */
void process_register_task(struct sprocess *prc, struct stask *task) {

	struct scheduler *sched;

	/*Parameters checks;*/
	ns_check(prc)
	ns_check(task)

	/*Fetch the scheduler;*/
	sched = prc->p_sched;

	/*Check the scheduler;*/
	ns_check(sched != 0);

	/*Initialize the task;*/
	task->t_status = SCHED_STATUS_ACTIVE;
	dlist_insert_single_after(&sched->s_actives, &task->t_sched_list);
	task->t_commit = (usize) -1;
	task->t_process = prc;
	dlist_insert_single_after(&prc->p_tasks, &task->t_siblings);
	task->t_nb_owned_primitives = 0;
	task->t_nb_overrides = 0;
	dlist_init(&task->t_overriders);
	task->t_stopper = 0;
	dlist_init(&task->t_stopped);
	task->t_thread = 0;
	dlist_init(&task->t_history);

	/*Report the task registration;*/
	prc->p_nb_tasks++;

}

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
err_t process_unregister_task(struct sthread *thread) {

	struct scheduler *sched;
	struct stask *task;
	struct sprocess *prc;
	struct dlist *head;
	struct dlist *save;
	struct sprim *overrider;

	/*Check parameter;*/
	ns_check(thread != 0);

	/*Fetch the scheduler and task;*/
	sched = thread->t_sched;
	task = thread->t_task;

	/*Debug checks;*/
	ns_check(sched != 0);
	ns_check(thread_active(thread) != 0);
	ns_check(task_active(task) != 0);
	ns_check(task->t_process);
	ns_check(task->t_process->p_sched == sched);


	/*If the task is stopped :*/
	if (task->t_status == SCHED_STATUS_STOPPED) {

		/*Resume the task;*/
		primitive_resume_task(task);

	}

	/*Remove the task from the active list if present;*/
	dlist_remove(&task->t_sched_list);

	/*Fetch the head of the overriding list;*/
	head = &task->t_overriders;

	/*For each overrider :*/
	dlist_head_for_each_object(
			overrider, save, head, struct sprim, p_overriders
	) {

		/*Un-override the task;*/
		primitive_unoverride_task(overrider);

	}

	/*Unregister the task from its thread;*/
	thread_unregister_task(task);

	/*Fetch the task's process;*/
	prc = task->t_process;

	/*Remove the task from its processor list if present;*/
	dlist_remove(&task->t_sched_list);

	/*Report the removal;*/
	prc->p_nb_tasks--;

	/*Call the scheduler implementation hook;*/
	(*(sched->s_ops->s_unregistered))(sched, task);

	/*Assign a new task to the thread;*/
	(*(sched->s_ops->s_assign_one))(sched, thread);

	/*Return 1 if the task still has owned primitives;*/
	return task->t_nb_owned_primitives ? (u8) 1 : (u8) 0;
}


/**
 * primitive_register : registers the provided primitive to the provided
 * process;
 * @param prc : the process to register the primitive to;
 * @param prim : the primitive to register;
 */
void process_register_prim(struct sprocess *prc, struct sprim *prim) {

	/*Check parameters;*/
	ns_check(prc != 0);
	ns_check(prim != 0);

	/*Initialize the primitive;*/
	prim->p_process = prc;
	dlist_init(&prim->p_siblings);
	prim->p_status = 0;
	prim->p_nb_owning_tasks = 0;
	prim->nb_stopped_tasks = 0;
	dlist_init(&prim->p_stopped);
	prim->p_overridden = 0;
	dlist_init(&prim->p_overriders);

	/*Report the registration;*/
	prc->p_nb_primitives = 0;

}


/**
 * primitive_unregister : resumes all tasks stopped by the primitive,
 * un-override the overridden task if any and unregisters the primitive from
 * its process;
 * @param prim : the primitive to unregister;
 * @return 1 if the primitive's ownership counter is not null, else;
 */
err_t process_unregister_prim(struct sprim *prim) {

	struct sprocess *prc;
	struct dlist *head;
	struct dlist *save;
	struct stask *task;

	/*Arg check;*/
	ns_check(prim);

	/*Fetch the process;*/
	prc = prim->p_process;

	/*Registration check;*/
	ns_check(prc);

	/*Fetch the head of the stopped tasks list;*/
	head = &prim->p_stopped;

	/*For each stopped task :*/
	dlist_head_for_each_object(task, save, head, struct stask, t_stopped) {

		/*Resume the task;*/
		primitive_resume_task(task);

	}

	/*If we override a task :*/
	if (prim->p_overridden) {

		/*Un-override it;*/
		primitive_unoverride_task(prim);

	}

	/*Remove the primitive from its process list;*/
	dlist_remove_unsafe(&prim->p_siblings);

	/*Report the un-registration;*/
	prc->p_nb_primitives--;

}

/*-------------------------------------------------------- scheduler functions*/

/**
 * abort_if_commit_closed : aborts if no commit is opened;
 * @param sched : the scheduler the check is related to;
 */
static __inline__ void abort_if_commit_closed(struct scheduler *sched) {

	/*Check a commit is opened;*/
	ns_check(sched->s_commit_opened != 0)

}


/**
 * sched_register_thread : registers the thread in the scheduler;
 * @param sched : the scheduler to update;
 * @param thread : the thread to register;
 */
void sched_register_thread(struct scheduler *sched, struct sthread *thread) {

	/*Check parameters;*/
	ns_check(sched != 0);
	ns_check(thread != 0);

	/*Initialize the thread;*/
	thread->t_sched = sched;
	dlist_insert_single_after(&sched->s_threads, &thread->t_list);
	thread->t_commit = (usize) -1;
	thread->t_task = 0;
	dlist_init(&thread->t_history);
	thread->t_history_size = 0;

	/*Report the registration;*/
	sched->s_nb_threads++;

}

/**
 * sched_register_process : registers the process in the scheduler;
 * @param sched : the scheduler to update;
 * @param prc : the process to register;
 */
void sched_register_process(struct scheduler *sched, struct sprocess *prc) {

	/*Check parameters;*/
	ns_check(sched != 0);
	ns_check(prc != 0);

	/*Initialize the process;*/
	prc->p_sched = sched;
	dlist_insert_single_after(&sched->s_processes, &prc->p_list);
	prc->p_status = SCHED_STATUS_ACTIVE;
	dlist_init(&prc->p_tasks);
	prc->p_nb_tasks = 0;
	dlist_init(&prc->p_primitives);
	prc->p_nb_primitives = 0;

	/*Initialize and register the process primitive;*/
	process_register_prim(prc, &prc->p_prim);

	/*Report the registration;*/
	sched->s_nb_processes++;

}


/**
 * sched_unregister_thread : clears the history of the thread, and unregisters
 * the thread from its scheduler;
 * @param thread : the thread to unregister;
 */
void sched_unregister_thread(struct sthread *thread) {

	struct scheduler *sched;
	struct dlist *head;
	struct stask *task;
	struct dlist *save;

	/*Check parameter;*/
	ns_check(thread != 0)

	/*Fetch the scheduler;*/
	sched = thread->t_sched;

	/*Check the scheduler;*/
	ns_check(sched != 0)

	/*Abort if no commit is opened;*/
	abort_if_commit_closed(thread->t_sched);

	/*Fetch the head of the history list;*/
	head = &thread->t_history;

	/*For each task in the history :*/
	dlist_head_for_each_object(task, save, head, struct stask, t_history) {

		/*Unregister the task;*/
		thread_unregister_task(task);

	}

	/*Reset the scheduler ref;*/
	thread->t_sched = 0;

	/*Remove from the scheduler;*/
	dlist_remove_unsafe(&thread->t_list);

	/*Report the un-registration;*/
	sched->s_nb_threads--;
}


/**
 * sched_unregister_process : removes any task registered to the process from
 * the scheduler list and unregisters the process from its scheduler;
 * all tasks and primitives can be considered unregistered, even if links
 * are not explicitly reset;
 * @param prc : the process to unregister;
 */
void sched_unregister_process(struct sprocess *prc) {

	struct scheduler *sched;
	struct dlist *head;
	struct dlist *save;
	struct stask *task;

	/*Check arg;*/
	ns_check(prc != 0);

	/*Fetch the scheduler;*/
	sched = prc->p_sched;

	/*Check the scheduler;*/
	ns_check(sched != 0);

	/*Abort if no commit is opened;*/
	abort_if_commit_closed(sched);

	/*Fetch the ref of the task list;*/
	head = &prc->p_tasks;

	/*For each registered task :*/
	dlist_head_for_each_object(task, save, head, struct stask, t_siblings) {

		/*If the task is active :*/
		if (task->t_status == SCHED_STATUS_ACTIVE) {

			/*Remove the task from the scheduler list;*/
			dlist_remove_unsafe(&task->t_sched_list);

		}

	}

	/*Remove the process from the scheduler list;*/
	dlist_remove_unsafe(&prc->p_list);

	/*Report the un-registration;*/
	sched->s_nb_processes--;

}


/**
 * sched_pause_process : stops all active tasks registered to the process,
 * relatively to the process's synchronization primitive;
 * Aborts if the
 * @param prc : the process to stop;
 */
void sched_pause_process(struct sprocess *prc) {

	struct scheduler *sched;
	struct dlist *head;
	struct dlist *save;
	struct stask *task;

	/*Check arg;*/
	ns_check(prc != 0);

	/*Fetch the scheduler;*/
	sched = prc->p_sched;
	
	/*Check the scheduler;*/
	ns_check(sched != 0);

	/*Check the process is active;*/
	ns_check(prc->p_status == SCHED_STATUS_ACTIVE);

	/*Abort if no commit is opened;*/
	abort_if_commit_closed(sched);

	/*Fetch the ref of the task list;*/
	head = &prc->p_tasks;

	/*For each registered task :*/
	dlist_head_for_each_object(task, save, head, struct stask, t_siblings) {

		/*If the task is active :*/
		if (task->t_status == SCHED_STATUS_ACTIVE) {

			/*Stop the task relatively to the process primitive;*/
			primitive_stop_task(&prc->p_prim, task);

		}

	}

	/*Mark the process stopped;*/
	prc->p_status = SCHED_STATUS_STOPPED;

}


/**
 * sched_resume_process : reactivates all tasks stopped by the process
 * primitive;
 * Aborts if the process is not stopped;
 * @param sched : the scheduler to update;
 * @param prc : the process to register;
 */
void sched_resume_process(struct sprocess *prc) {

	struct scheduler *sched;
	struct dlist *head;
	struct dlist *save;
	struct stask *task;

	/*Check arg;*/
	ns_check(prc != 0);
	
	/*Fetch the scheduler;*/
	sched = prc->p_sched;
	
	/*Check the scheduler;*/
	ns_check(sched != 0);

	/*Check the process is active;*/
	ns_check(prc->p_status == SCHED_STATUS_STOPPED);

	/*Abort if no commit is opened;*/
	abort_if_commit_closed(sched);

	/*Fetch the ref of the process's primitive's stopped tasks list;*/
	head = &prc->p_prim.p_stopped;

	/*For each task stopped by the process primitive :*/
	dlist_head_for_each_object(task, save, head, struct stask, t_siblings) {

		/*Resume the task;*/
		primitive_resume_task(task);

	}

	/*Mark the process active;*/
	prc->p_status = SCHED_STATUS_ACTIVE;

}

/*------------------------------------------------------------ access barriers*/

/**
 * sched_lock : attempts to locks the scheduler;
 * This function must be called before any operation is made on the scheduler;
 * @param sched : the scheduler to lock;
 * @return 01 if the lock succeeded, 0 if the scheduler was already locked;
 */
u8 sched_lock(struct scheduler *sched) {

	u8 locked;
	
	/*Attempt to lock the scheduler's spinlock;*/
	locked = arch_spin_lock_nb(&sched->s_lock);

	/*If we locked the scheduler :*/
	return locked;

}

/**
 * sched_unlock : unlocks the scheduler; aborts if the scheduler is unlocked;
 * @param sched : the scheduler to unlock;
 */
void sched_unlock(struct scheduler *sched) {
	
	u8 unlock_error;

	/*Unlock the scheduler;*/
	unlock_error = arch_spin_unlock(&sched->s_lock);

	/*Check the scheduler was locked;*/
	ns_check(unlock_error == 0);

}

/**
 * sched_open_commit : opens a new commit for the provided scheduler;
 * commit functions will be authorised after;
 * Aborts if a commit is already opened;
 * @param sched : the scheduler to open a commit in;
 */
void sched_open_commit(struct scheduler *sched) {

	/*Check that no commit is opened;*/
	ns_check(sched->s_commit_opened == 0)

	/*Mark the commit opened;*/
	sched->s_commit_opened = 1;

	/*Update the commit index;*/
	sched->s_commit_index++;

}

/**
 * sched_open_commit : closes the current commit for the provided scheduler;
 * no commit function will be authorised after;
 * Aborts if no commit is opened opened;
 * @param sched : the scheduler to open a commit in;
 */
void sched_close_commit(struct scheduler *sched) {

	/*Check that a commit is opened;*/
	ns_check(sched->s_commit_opened != 0)

	/*Mark the commit opened;*/
	sched->s_commit_opened = 0;

	/*Reschedule tasks;*/
	(*(sched->s_ops->s_schedule))(sched);

	/*Reassign tasks to processors;*/
	(*(sched->s_ops->s_assign_all))(sched);

}

