/*tashman.c - kerneltk - GPLV3, copyleft 2019 Raphael Outhier;*/

#include <taskman.h>

#include <struct/avl_tree.h>


/*---------------------------------------------------------------- task struct*/



/** 
 * taskman_task : contains all data required to execute a task
 *  in a task manager;
 */
struct taskman_task {
	
	/*
	 * Registration data;
	 */
	
	/*The mgr the task belongs to;*/
	struct taskman *t_mgr;
	
	/*Task contexts are referenced in a tree, sorted by the task block's lower
	 * bound;*/
	struct avl_tree t_tree;
	
	/*The size in bytes of the task's stack;*/
	usize t_stack_size;
	
	
	/*
	 * Referencing data;
	 */
	
	/*Free tasks are referenced in a linked list;*/
	struct dlist t_list;
	
	/*The list of active tasks combined with the depth defines a tree;*/
	usize t_depth;
	
	
	/*
	 * Task data;
	 */
	
	/*A flag set if the task is a root task for its mgr;*/
	u8 t_root;
	
	/*A flag, set if the task is ready;*/
	u8 t_ready;
	
	/*The function to execute;*/
	void (*t_function)(void *args);
	
	/*Arguments to pass to the function;*/
	void *t_args;
	
};


/*----------------------------------------------------------------- inline ops*/

/**
 * mgr_lock : locks a task manager;
 *  @param mgr : the task manager to lock;
 */
static __inline__ void mgr_lock(struct taskman *mgr)
{
	
	/*TODO;*/
	
}

/**
 * mgr_unlock : unlocks a task manager;
 *  @param mgr : the task manager to lock;
 */
static __inline__ void mgr_unlock(struct taskman *mgr)
{
	
	/*TODO;*/
}

/**
 * task_init : initializes a task's function, argument struct and state;
 *  @param task : the task to initialize;
 *  @param fnc : the function the task must execute;
 *  @param args : the ref of the arguments struct;
 */
static __inline__ void task_init(
	struct taskman_task *task, void (*fnc)(void *), void *args
)
{
	
	/*Initialize task data;*/
	task->t_function = fnc;
	task->t_args = args;
	task->t_ready = 1;
	
}

/**
 * task_deinit : resets a task's function, argument struct and state;
 *  @param task : the task to reset;
 */
static __inline__ void task_reset(struct taskman_task *task)
{
	
	/*Reset task data;*/
	task->t_function = 0;
	task->t_args = 0;
	task->t_ready = 0;
	
}

/**
 * task_exec : executes the function of a task, providing the arguments struct;
 *  @param task : the task to execute;
 */
static __inline__ void task_exec(struct taskman_task *task)
{
	
	/*Execute the task function providing task args;*/
	(*(task->t_function))(task->t_args);
	
}


/*--------------------------------------------- task manager private functions*/

/**
 * mgr_add_free_task : adds a task in its manager;
 * If a root instance can be created, the task is initialized with it;
 * Ifnot, the task is inserted in the free list;
 *
 * @param task : the task to add;
 * @return 0 if the task is initialized, if not, the number of time units to
 * wait;
 */
static usize mgr_add_free_task(struct taskman_task *task)
{
	
	usize delay;
	void *args;
	struct taskman *mgr;
	
	/*Fetch the task's manager;*/
	mgr = task->t_mgr;
	
	/*Lock the mgr;*/
	mgr_lock(mgr);
	
	/*Check if one instance is ready;*/
	delay = (*(mgr->m_ops.m_ready))(mgr, &args);
	
	/*If no instance is ready :*/
	if (delay) {
		
		/*Add the task in the mgr's free list;*/
		dlist_concat(&mgr->m_free_tasks, &task->t_list);
		
	} else {
		
		/*Initialize the task;*/
		task_init(task, mgr->m_ops.m_exec, args);
		
		/*Mark the task as root of the mgr;*/
		task->t_root = 1;
		
	}
	
	/*Unlock the mgr;*/
	mgr_unlock(mgr);
	
	/*Return the delay time;*/
	return delay;
	
}

/**
 * mgr_remove_free_task : attempts to remove a task from its manager;
 * @param task : the task to remove;
 * @return 1 if the task was not ready (present in the free list);
 *   	   0 if the task is ready (not present in the free list);
 */
static u8 mgr_remove_free_task(struct taskman_task *task)
{
	
	struct taskman *mgr;
	u8 removed;
	
	/*Fetch the task's manager;*/
	mgr = task->t_mgr;
	
	/*Initialize the removal flag;*/
	removed = 0;
	
	/*Lock the mgr;*/
	mgr_lock(mgr);
	
	/*If the task is present in the free list :*/
	if (!task->t_ready) {
		
		/*Remove the task from the free list;*/
		dlist_remove(&task->t_list);
		
		/*Report the removal;*/
		removed = 1;
		
	}
	
	/*Unlock the mgr;*/
	mgr_unlock(mgr);
	
	/*Return the removal status;*/
	return removed;
	
}


/**
 * mgr_cleanup_task : recycles a task after its execution; If the task is a
 * root task, its arguments is recycled with the proprietary function;
 * @param task : the task to cleanup;
 */
static void mgr_cleanup_task(struct taskman_task *task)
{
	
	/*If the task was a root task :*/
	if (task->t_root) {
		
		struct taskman *mgr;
		
		/*Fetch the task's manager;*/
		mgr = task->t_mgr;
		
		/*Lock the mgr;*/
		mgr_lock(mgr);
		
		/*Recycle the argument struct;*/
		(*(mgr->m_ops.m_cleanup))(mgr, task->t_args);
		
		/*Unlock the mgr;*/
		mgr_unlock(mgr);
		
	}
	
	/*Reset the task's execution data;*/
	task_reset(task);
	
	
}


/**
 * mgr_register_task : attempts to insert the provided task in the provided
 *  manager's task tree, and if it succeeds, initializes the task's
 *  registration data;
 *  @param mgr : the task manager that must register the task;
 *  @param task : the task to register;
 *  @return 1 if the registration succeeds, 0 if it fails;
 */
static u8 mgr_register_task(
	struct taskman *mgr, usize stack_size, struct taskman_task *task
)
{
	u8 flags;
	
	/*Reset flags;*/
	flags = 0;
	
	/*Initialize the task manager;*/
	task->t_mgr = mgr;
	
	/*Initialize the task's memory data;*/
	task->t_stack_size = stack_size;
	
	/*TODO VERIFY THAT NO TASK HAS THIS MEMORY SPACE.*/
	/*TODO VERIFY THAT NO TASK HAS THIS MEMORY SPACE.*/
	/*TODO VERIFY THAT NO TASK HAS THIS MEMORY SPACE.*/
	/*TODO VERIFY THAT NO TASK HAS THIS MEMORY SPACE.*/
	
	/*Insert the task in the tasks tree;*/
	mgr->m_tree = avl_tree_insert(mgr->m_tree, &task->t_tree, (usize) (task));
	
	/*Check that no error occurred;*/
	return (u8)(flags == 0);
	
}


/**
 * mgr_get_task : searches the provided task manager's task tree for a task 
 *  whose stack block would contain the calling stack; 
 *  If such task is found, it is returned; If not, 0 is returned;
 *  @param mgr : the task manager in which we should search;
 *  @return the ref of the found task if such, 0 if none;
 */
static struct taskman_task *mgr_get_task(struct taskman *mgr)
{
	
	usize stack_id;
	struct bstree_bsearch result;
	
	
	
	/*Search for a task with the appropriate memory block;*/
	bstree_bound_search(
		&mgr->m_tree->t_tree, (usize) (&stack_id), &result
	);
	
	
	/*If an inferior exists*/
	/*TODO!!*/
	/*TODO!!*/
	/*TODO!!*/
	/*TODO!!*/
	
}

/**
 * mgr_unregister_task : removes the provided task from its task manager;
 */
static void mgr_unregister_task(struct taskman_task *task)
{
	
	/*Insert the task in the tasks tree;*/
	/*avl_remove();
	TODO;*/
	
}


/*----------------------------------------------------- task manager functions*/

/**
 * taskman_init : initializes all fields of a task manager;
 *  @param mgr : the task manager to initialize;
 *  @param ops : a ref to the operations structures to copy;
 */
usize taskman_init(
	struct taskman *mgr,
	struct taskman_ops *ops
)
{
	
	/*TODO LOCK INIT*/
	
	/*Initialize the list of free tasks;*/
	dlist_init(&mgr->m_free_tasks);
	
	/*Initialize the operations struct;*/
	mgr->m_ops = *ops;
	
}


/**
 * taskman_exec : adds an execution context to a task manager;
 *
 *  A task is first created and initialized; then attempts are made to create
 *  instances of a root task, via the @e_ready proprietary function;
 *  Depending on @e_ready's result, the function either executes the task, 
 *  waits and re-attempts to instantiate, or stalls until the task is
 *  initialized;
 *
 *  During a stall / wait period, the task is marked free, so that it is
 *  initialized by another execution context if needed;
 *
 *  When the task is executed, this sequence restarts;
 * 
 *  This function guarantees that any task that is started completes entirely;
 *
 *  This loop can be required to stop at any moment via the @abort flag; 
 *  If this flag is set, the function is guaranteed to stop, however, there is
 *  no guarantee about the time it will take to stop, due to the guarantee of
 *  completion of any started task;
 */
void taskman_exec(
	struct taskman *mgr, usize stack_size, const volatile u8 *abort
)
{
	
	struct taskman_task task;
	usize delay;
	
	/*Register the task in the manager;*/
	mgr_register_task(mgr, stack_size, &task);
	
	/*
	 * Main loop
	 */
	
	task_insertion :
	
	/*If the execution must be aborted, complete;*/
	if (*abort) {
		goto exec_exit;
	}
	
	/*Add the task in its manager's free list;*/
	delay = mgr_add_free_task(&task);
	
	/*
	 * Wait till the task is ready;
	 */
	
	/*If the task must be activated externally :*/
	if (delay == (usize) -1) {
		
		/*While the task is not ready, wait;*/
		while (1) {
			
			/*If the execution must be aborted, complete;*/
			if (*abort) {
				
				u8 removed;
				
				/*Attempt to remove the task from the mgr;*/
				removed = mgr_remove_free_task(&task);
				
				/*If the task was still free and could be removed, complete;*/
				if (removed)
					goto exec_exit;
				
				/*
				 * If the task was initialized between ready check and removal, 
				 * It must be executed;
				 */
				goto execution;
			}
			
			/*Wait the appropriate time;
			/*TODO TASK_IF_WAIT;*/
			
			/*If the task is ready, complete :*/
			if (task.t_ready) {
				goto execution;
			}
			
		}
		
	} else if (delay) {
		/*If we must attempt to re-activate the task another time :*/
		
		u8 removed;
		
		/*Wait the required time;
		/*TODO TASK_IF_WAIT;*/
		
		/*Attempt to remove the task from the mgr;*/
		removed = mgr_remove_free_task(&task);
		
		/*If the task was still free and could be removed, re-iterate;*/
		if (removed)
			goto task_insertion;
		
		/*
		 * If the task was initialized between ready check and removal, 
		 * It must be executed;
		 */
		goto execution;
		
	}
	
	
	/*
	 * Ready task execution;
	 */
	
	execution :
	
	/*Execute the task;*/
	task_exec(&task);
	
	/*Cleanup the task;*/
	mgr_cleanup_task(&task);
	
	/*Re-iterate;*/
	goto task_insertion;
	
	
	/*
	 * Function exit;
	 */
	
	exec_exit:
	
	/*Unregister the task from its manager;*/
	mgr_unregister_task(&task);
	
}

