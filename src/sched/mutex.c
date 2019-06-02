/*mutex.c - kerneltk - GPLV3, copyleft 2019 Raphael Outhier;*/

#include <debug.h>

#include <sched/mutex.h>


/**
 * sched_mutex_ctor : constructs and registers the mutex to the provided
 * scheduler;
 * @param mutex : the mutex to construct;
 * @param prc : the process to register the mutex to;
 */
void sched_mutex_ctor(struct sched_mutex *mutex, struct sprocess *prc) {

	/*Args check;*/
	check(mutex);
	check(prc);

	/*Register the primitive to the scheduler;*/
	process_register_prim(prc, &mutex->m_prim);

}

/**
 * sched_mutex_lock : if the mutex is unlocked, locks it and registers the
 * thread's task as the owner of the mutex; if the mutex is locked, stops
 * the task and assigns a new one to the thread;
 * @param mutex : the mutex to lock;
 * @param thread : the thread executing the task that must lock the mutex;
 */
void sched_mutex_lock(struct sched_mutex *mutex, struct sthread *thread) {

	/*Args check;*/
	check(mutex);
	check(thread);

	/*If the mutex is locked :*/
	if (mutex->m_prim.p_nb_owning_tasks) {

		/*Stop the thread's task;*/
		primitive_stop_thread(&mutex->m_prim, thread);

	} else {
		/*If the mutex is unlocked :*/

		/*Give the thread's task the ownership of the mutex;*/
		primitive_take_ownership(&mutex->m_prim, thread->t_task);
		
		/*Make the primitive override the task;*/
		primitive_override_task(&mutex->m_prim, thread->t_task);
		
	}
}

/**
 * sched_mutex_lock_nb : non blocking lock function;
 * The function attempts to lock the mutex and returns;
 * @param mutex : the mutex to lock;
 * @param thread : the thread executing the task that must lock the mutex;
 * @return 1 if the locking succeeds, 0 if not;
 */
u8 sched_mutex_lock_nb(struct sched_mutex *mutex, struct sthread *thread) {

	/*Args check;*/
	check(mutex);
	check(thread);

	/*If the mutex is locked :*/
	if (mutex->m_prim.p_nb_owning_tasks) {

		/*Fail;*/
		return 0;

	}

	/*If the mutex is unlocked :*/

	/*Give the thread's task the ownership of the mutex;*/
	primitive_take_ownership(&mutex->m_prim, thread->t_task);

	/*Make the primitive override the task;*/
	primitive_override_task(&mutex->m_prim, thread->t_task);
	
	/*Complete;*/
	return 1;

}

/**
 * sched_mutex_unlock : if the mutex is locked, release its owner, and if any,
 * picks one stopped task and select it as the new owner;
 * @param mutex : the mutex to unlock;
 * @param thread : the thread executing the task that must unlock the mutex;
 * @return 0 if the mutex has been unlocked, 1 if it was already unlocked,
 *  2 if the thread's task didn't own the mutex, and 3 if an ownership 
 *  release error occurred;
 */
err_t sched_mutex_unlock(struct sched_mutex *mutex, struct sthread *thread) {

	struct stask *task;
	struct stask *owner;
	err_t error;

	/*Args check;*/
	check(mutex);
	check(thread);

	/*Fetch the task and the owner of the mutex;*/
	task = thread->t_task;
	owner = mutex->m_prim.p_overridden;

	/*If the mutex is unlocked :*/
	if (!owner) {

		/*Fail;*/
		return 1;

	}

	/*The mutex is locked;*/

	/*If the task doesn't own the mutex :*/
	if (task != owner) {
		return 2;
	}

	/*The task owns the mutex;*/
	
	/*Release the ownership of the primitive;*/
	error = primitive_release_ownership(&mutex->m_prim, task);
	
	/*If an error occurred during te ownership release, fail;*/
	if (error)
		return 3;
	
	/*Make the primitive unoverride the task;*/
	primitive_unoverride_task(&mutex->m_prim);
	
	/*If the primitive has a stopped task :*/
	if (!dlist_empty(&mutex->m_prim.p_stopped)) {
		
		/*Resume the first task;*/
		primitive_resume_task(
				container_of(&mutex->m_prim.p_stopped, struct stask, t_stopped)
		);
		
	}


	/*Complete;*/
	return 0;

}
