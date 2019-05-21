/*tashman.h - kerneltk - GPLV3, copyleft 2019 Raphael Outhier;*/

#include <types.h>

#include <struct/list.h>

/**
 * taskman_ops : a set of operations that determine the behavior of a task
 *  manager instance;
 */
struct taskman_ops {
	
	/**
	 * e_ready : determines whether or not an mgr can be activated;
	 *  
	 * If so, updates *@args_p with the ref of a ready root arguments
	 * struct to provide to @e_exec, and returns 0;
	 *
	 * If not, resets *@args_p and returns the number of time unit to wait
	 * until next check; If -1 is returned, then no more check must be made
	 * in the related execution context;
	 */
	usize (*m_ready)(struct mgr *, void **args_p);
	
	/**
	 * e_exec : the function that the mgr aims to execute; required a ready
	 * root argument structto be executed;
	 */
	void (*m_exec)(void *args);
	
	/**
	 * e_cleanup : recycles a root argument struct, after a complete
	 * execution of @e_exec;
	 */
	void (*m_cleanup)(struct mgr *, void *args);
	
};


/**
 * taskman : contains all data required to manage a set of execution contexts;
 */
struct taskman {
	
	/*The lock protecting the mgr;*/
	/*TODO*/
	
	/*A set of mgr operations;*/
	struct taskman_ops m_ops;
	
	/*The list of free tasks;*/
	struct dlist m_free_tasks;
	
	/*The root of the task tree;*/
	struct avl_tree *m_tree;
	
};


/*Initialize a task manager;*/
usize taskman_init(
	struct taskman *mgr,
	struct taskman_ops *ops
);

void taskman_exec(
	struct taskman *mgr, usize stack_size, const volatile u8 *abort
);

/*Internal functions*/

/*Add a sub-task to a task;*/
void taskman_add_subtask(
	struct taskman *mgr,
	void (*t_function)(void *args,usize args_size),
	void *args
);

/*Waits till all sub-task of a task are executed;*/
void taskman_join(struct taskman *mgr);

