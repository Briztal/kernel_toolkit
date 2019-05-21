/*sem.h - kerneltk - GPLV3, copyleft 2019 Raphael Outhier;*/

#ifndef KERNELTK_SEM_H
#define KERNELTK_SEM_H

#include "sched.h"

struct sched_sem {

	/*A scheduler primitive;*/
	struct sprim s_prim;

	/*The maximal number of takings;*/
	usize s_nb_owners;

};


/**
 * sched_sem_ctor : constructs the semaphore with the provided number of
 * takings and registers it to the provided process;
 * @param sem : the semaphore to construct;
 * @param prc : the process to register the semaphore's primitive to;
 */
void sched_sem_ctor(
		struct sched_sem *sem,
		usize nb_takings,
		struct sprocess *prc
);

/**
 * sched_sem_take : if the semaphore's ownership can be taken once, it is taken
 * by the task executed by the provided thread; if not, the executed task is
 * stopped and a new one is assigned to the thread
 * @param sem : the semaphore to take;
 */
void sched_sem_take(struct sched_sem *sem, struct sthread *thread);

/**
 * sched_sem_take_nb : non blocking take function;
 * The function attempts to take the semaphore and returns;
 * @param sem : the semaphore to take;
 * @return 1 if the locking succeeds, 0 if not;
 */
u8 sched_sem_take_nb(struct sched_sem *sem, struct sthread *thread);

/**
 * sched_sem_is_locked : determines whether the semaphore can be taken or
 * not; the result is purely indicative;
 * @param sem : the semaphore to check;
 * @return 1 if the semaphore can't be taken once, 0 if it can;
 */
static __inline__ u8 sched_sem_is_locked(struct sched_sem *sem) {

	/*The sempaphore is available if it has not been taken
	 * more than its limit;*/
	return (u8) (sem->s_nb_owners == sem->s_prim.p_nb_owning_tasks);

}


/**
 * sched_mutex_unlock : release once the ownership of the semaphore, and
 * resumes a stopped task if no ownership release error occurred;
 * @param sem : the semaphore to release;
 * @param thread : the thread executing the task that should release its
 * ownership of the semaphore;
 * @return 0 if the semaphore's ownership was successfully released, 1 if
 * an ownership release error occurred;
 */
err_t sched_sem_release(struct sched_sem *sem, struct sthread *thread);


#endif //KERNELTK_SEM_H
