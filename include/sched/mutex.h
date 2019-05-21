/*mutx.h - kerneltk - GPLV3, copyleft 2019 Raphael Outhier;*/

#ifndef KERNELTK_MUTEX_H
#define KERNELTK_MUTEX_H

#include "sched.h"

struct sched_mutex {

	/*A scheduler primitive;*/
	struct sprim m_prim;

};


/**
 * sched_mutex_ctor : constructs and registers the mutex to the provided
 * scheduler;
 * @param mutex : the mutex to construct;
 * @param prc : the process to register the mutex to;
 */
void sched_mutex_ctor(struct sched_mutex *mutex, struct sprocess *prc);

/**
 * sched_mutex_lock : if the mutex is unlocked, locks it and registers the
 * thread's task as the owner of the mutex; if the mutex is locked, stops
 * the task and assigns a new one to the thread;
 * @param mutex : the mutex to lock;
 * @param thread : the thread executing the task that must lock the mutex;
 */
void sched_mutex_lock(struct sched_mutex *mutex, struct sthread *thread);

/**
 * sched_mutex_lock_nb : non blocking lock function;
 * The function attempts to lock the mutex and returns;
 * @param mutex : the mutex to lock;
 * @param thread : the thread executing the task that must lock the mutex;
 * @return 1 if the locking succeeds, 0 if not;
 */
u8 sched_mutex_lock_nb(struct sched_mutex *mutex, struct sthread *thread);

/**
 * sched_mutex_is_locked : determines whether the mutex is locked or not;
 * the result is purely indicative;
 * @param mutex : the mutex to check;
 * @return 1 if the mutex is locked, 0 if not;
 */
static __inline__ u8 sched_mutex_is_locked(struct sched_mutex *mutex) {

	return (u8) (mutex->m_prim.p_nb_owning_tasks != 0);

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
err_t sched_mutex_unlock(struct sched_mutex *mutex, struct sthread *thread);


#endif //KERNELTK_MUTEX_H
