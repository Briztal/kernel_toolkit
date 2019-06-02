/*sem.c - kerneltk - GPLV3, copyleft 2019 Raphael Outhier;*/

#include <debug.h>

#include <sched/sem.h>

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
) {

	/*Args check;*/
	check(sem);
	check(prc);

	/*Initialize and register the semaphore;*/
	sem->s_nb_owners = nb_takings;
	process_register_prim(prc, &sem->s_prim);

}

/**
 * sched_sem_take : if the semaphore's ownership can be taken once, it is taken
 * by the task executed by the provided thread; if not, the executed task is
 * stopped and a new one is assigned to the thread
 * @param sem : the semaphore to take;
 */
void sched_sem_take(struct sched_sem *sem, struct sthread *thread) {

	/*Args check;*/
	check(sem);
	check(thread);

	/*If the semaphore is not available :*/
	if (sem->s_prim.p_nb_owning_tasks == sem->s_nb_owners) {

		/*Stop the thread's task and assign a new task;*/
		primitive_stop_thread(&sem->s_prim, thread);

	} else {

		/*If the semaphore can be taken :*/

		/*Make the task take the ownership of the semaphore;*/
		primitive_take_ownership(&sem->s_prim, thread->t_task);

	}

}

/**
 * sched_sem_take_nb : if the semaphore's ownership can be taken once, it is
 * taken by the task executed by the provided thread and the function
 * returns 1; if not, the function returns 0;
 * @param sem : the semaphore to take;
 * @return 1 if the taking succeeds, 0 if not;
 */
u8 sched_sem_take_nb(struct sched_sem *sem, struct sthread *thread) {

	/*Args check;*/
	check(sem);
	check(thread);

	/*If the semaphore is not available, fail;*/
	if (sem->s_prim.p_nb_owning_tasks == sem->s_nb_owners) {

		return 0;

	}

	/*If the semaphore can be taken :*/

	/*Make the task take the ownership of the semaphore;*/
	primitive_take_ownership(&sem->s_prim, thread->t_task);

	/*Complete;*/
	return 1;

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
err_t sched_sem_release(struct sched_sem *sem, struct sthread *thread) {

	err_t error;

	/*Args check;*/
	check(sem);
	check(thread);

	/*Release the ownership of the primitive;*/
	error = primitive_release_ownership(&sem->s_prim, thread->t_task);

	/*If an error occurred, fail;*/
	if (error) {
		return 1;
	}

	/*If the primitive has a stopped task :*/
	if (!dlist_empty(&sem->s_prim.p_stopped)) {

		/*Resume the first task;*/
		primitive_resume_task(
				container_of(&sem->s_prim.p_stopped, struct stask, t_stopped)
		);

	}
	
	/*Complete;*/
	return 0;

}
