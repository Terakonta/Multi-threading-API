#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"

// States of a thread
typedef enum thread_state
{ 
	READY,
	RUNNING,
	EXITING,
	WAITING
}thread_state;

// Node to create a linked list of threads
typedef struct node
{
	struct thread * thread;
	struct node * next;
}node;

/* This is the wait queue structure */
struct wait_queue {
	/* ... Fill this in Assignment 2 ... */
	node * node;
};


/* This is the thread control block */
typedef struct thread {
	/* ... Fill this in ... */
	ucontext_t context;
	Tid tid;
	enum thread_state state;
	void * stack;
	int exit_code;
	
}thread;

// Keeps track of all existing threads
thread * threads[THREAD_MAX_THREADS];

node * ready_list=NULL;
node * exit_list=NULL;
struct wait_queue * wait_lists[THREAD_MAX_THREADS];
int thread_count;
thread * current_thread=NULL; 



// Need to add functions to interact with the linked list of thread nodes
// Create and returns a node for a thread
node * create_node(thread* thread)
{
	struct node * temp = malloc(sizeof(node));
	if (temp != NULL)
	{
		temp->thread = thread;
		temp->next = NULL;
	}

	return temp;
}

// Remove a node from somewhere in the list, makes thread target removed node and returns new root
node * remove_node(Tid tid, node * root, node ** thread)
{	
	//printf("Remove_node_begin\n");
	if (root != NULL)
	{
		//printf("RootNull\n");

		// If tid is at the root, remove root and return the new_root of the list
		if ( root->thread->tid == tid)
		{
			//printf("Remove_node_here\n");

			node * new_root = root->next;
			root->next = NULL;
			*thread = root;
			//printf("Remove_node_before_return\n");
			return new_root;
		}

		//printf("Root_NOt_Null2\n");


		// If tid anywhere else, remove it and return the original head
		node * curr = root;

		while (curr-> next != NULL && curr->next->thread->tid != tid)
		{
			curr = curr->next; 
		}

		// At this point, either curr is the end of the list, or curr-> next points to node of thread with tid
		if (curr->next != NULL)
		{
			assert(curr->next->thread->tid == tid);
			node * remove_node = curr->next;
			curr->next = curr->next->next;
			remove_node->next=NULL;
			*thread = remove_node;
		}
	}
	return root;
}

// Pop a node from the start of the list, makes thread point to removed node and returns new root
node * pop_node(node * root, node ** thread)
{
	if (root == NULL)
		return root;

	// remove the first element of the list, which is the root, and return new root
	node * new_root = root->next;
	root->next = NULL;
	*thread = root;
	return new_root;
}

// Push the thread node to the end of the list
node * push_node(node * root, node * thread)
{
	//printf("push_node_start\n");
	if (root == NULL)
	{	
		root=thread;
		return root;
	}

	// Traverse to the end of the list and add thread there
	node * curr = root;
	while(curr->next != NULL)
		curr = curr->next;

	curr->next = thread;
	//printf("push_node_end\n");
	return root;
}

// Empty the entire list
void empty_list(node * root)
{
	if (root == NULL)
		return;
	
	node * curr = root;

	// Pop each node and free the node's and its thread's space
	while (curr != NULL)
	{
		node * node_ptr;
		curr = pop_node(curr, &node_ptr);

		Tid tid = node_ptr->thread->tid;
		void* stack = node_ptr->thread->stack;

		threads[tid] = NULL;

		// Free the data allocated for the thread, the node pointing to it and the stack (if its not the initial thread)
		if (tid != 0)
			free(stack);
		free(node_ptr->thread);
		free(node_ptr);	
	}
	return;
}

// Print the read/exit list for testing
void print_list(node * root)
{
	node * curr = root;

	while (curr != NULL)
	{
		printf("%d", curr->thread->tid);
		printf("->");
		curr = curr->next;
	}
	printf("\n End of List \n");
}

void set_exit_code(struct wait_queue * queue, int exit_code)
{
	if (queue != NULL)
	{
		node * curr = queue->node;

		while (curr != NULL)
		{
			curr->thread->exit_code = exit_code;
			curr = curr->next;
		}
	}
	return;
}

void
thread_init(void)
{
	/* Add necessary initialization for your threads library here. */
    /* Initialize the thread control block for the first thread */
	
	int enabled = interrupts_off();

	// Initialize all threads and wait lists to point to NULL
	for (int i =0;i<THREAD_MAX_THREADS;i++)
	{
		threads[i] = NULL;
		wait_lists[i] = NULL;
	}
	current_thread=malloc(sizeof(thread));
	
	// Set their first element
	threads[0] = current_thread;
	wait_lists[0] = wait_queue_create();
       
	// Initialize the first thread
    ucontext_t first_context;
	if (getcontext(&first_context) == -1)
   		fprintf(stderr, "get_context failed in thread_init\n");

   current_thread->context = first_context;
   current_thread->tid = 0;
   current_thread->state = RUNNING;

   thread_count = 1;

   interrupts_set(enabled);
   return;
    
}

Tid
thread_id()
{
	int enabled = interrupts_off();

	if (current_thread == NULL)
		return THREAD_INVALID;
	
	interrupts_set(enabled);
	return current_thread->tid;
	
}

/* New thread starts by calling thread_stub. The arguments to thread_stub are
 * the thread_main() function, and one argument to the thread_main() function. 
 */
void
thread_stub(void (*thread_main)(void *), void *arg)
{
	// Need to allow interrupts outside critical section
	interrupts_on();

	thread_main(arg); // call thread_main() function with arg
	thread_exit(0);
}

Tid
thread_create(void (*fn) (void *), void *parg)
{
	int enabled = interrupts_off();
	//print_list(ready_list);
	//printf("Current tid %d\n", current_thread->tid);
	// Exit all threads with exit status 
	empty_list(exit_list);
	exit_list = NULL;


	// Check for a valid Tid and return THREAD_NOMORE if all are taken
	Tid tid = THREAD_MAX_THREADS;
	for (int i = 0; i < THREAD_MAX_THREADS; i++)
	{
		if (threads[i] == NULL)
		{
			tid = i;
			break;
		}
	}

	// In case all Tid are taken
	if (tid == THREAD_MAX_THREADS)
	{
		//free(new_node);
		//free(new_thread);
		return THREAD_NOMORE;
	}

	// Check if theres memory for a new thread and a new node pointing to that that thread
	thread * new_thread = malloc(sizeof(thread));
	if (new_thread == NULL)
		return THREAD_NOMEMORY;

	node * new_node = create_node(new_thread);
	if (new_node == NULL)
		return THREAD_NOMEMORY;

	// Allocate stack for thread and if successful, initialize the context and set register values
	void * stack = malloc(THREAD_MIN_STACK);
	if (stack == NULL)
	{
		free(new_node);
		free(new_thread);
		return THREAD_NOMEMORY;
	}



	if (getcontext(&(new_thread->context)) == -1)
		fprintf(stderr, "getcontext failed in thread_create on creating a new thread with tid=%d failed\n", new_thread->tid);

	// Pass in the thread function and its argument, make the new thread run the stub function using program counter and align stack pointer to satisfy the 16 byte allignment
	(new_thread -> context).uc_mcontext.gregs[REG_RIP] = (long long int) &thread_stub;
	(new_thread -> context).uc_mcontext.gregs[REG_RDI] = (long long int) fn;
	(new_thread -> context).uc_mcontext.gregs[REG_RSI] = (long long int) parg;	
	(new_thread -> context).uc_mcontext.gregs[REG_RSP] = ((long long int) stack + THREAD_MIN_STACK) - (((long long int) stack + THREAD_MIN_STACK) % 16) - 8;
	(new_thread -> context).uc_mcontext.gregs[REG_RBP] = (new_thread -> context).uc_mcontext.gregs[REG_RSP];

	new_thread->tid=tid;
	new_thread->state=READY;
	new_thread->stack = stack;

	thread_count++;
	// Add the thread in the ready list
	threads[tid] = new_thread;
	// Initialize its wait list
	wait_lists[tid] = wait_queue_create();
	//printf("Thread_create tid:%d\n",tid);
	ready_list = push_node(ready_list, new_node);
	//printf("Ready_List tid:%d\n",ready_list->thread->tid);

	interrupts_set(enabled);

	return tid;	
}

Tid switch_context(Tid new_thread_id, node ** new_list, thread_state new_state)
{
	int enabled = interrupts_off();

	//printf("Before switch\n");
	//print_list(ready_list);
	// Keep track of old thread
	Tid old_tid;
	old_tid = current_thread->tid;

	// Put the current thread into ready_list
	current_thread->state = new_state;
	*new_list = push_node(*new_list, create_node(current_thread));
	//printf("CurrentThreadID BeforeContext:%d\n",current_thread->tid);
	//printf("NewThreadID:%d\n",new_thread_id);

	//printf("After switch\n");
	//print_list(ready_list);

	// Remove new thread from ready list and start running it (make it the current thread)
	node * new_thread;
	ready_list = remove_node(new_thread_id, ready_list, &new_thread);
	//printf("After removing\n");
	//print_list(ready_list);

	new_thread->thread->state = RUNNING;
	//new_thread->thread->tid=new_thread_id;

	//thread * old_thread=current_thread;
	
	// Swap context and keep track of set_context calls to know which "return" of get_context we are in using shared memory
	volatile int setcontext_calls = 0;

	if(getcontext(&(current_thread->context)) == -1)
		fprintf(stderr, "getcontext failed in switch_context for current thread tid = %d failed\n", current_thread->tid);

	// Return new thread's ID if it already ran. (Code after this block won't run if this is the 2nd "return" from get_context)
	if (setcontext_calls == 1)
	{
		// Check if this thread was killed while it yielded to thread with new_thread_id and stop it if it did
		if (threads[old_tid]->state == EXITING)
		{
			interrupts_set(enabled);
			thread_exit(SIGKILL);
		}
			

		//current_thread=old_thread;
		interrupts_set(enabled);
		return new_thread_id;
	}

	// Make the current_thread the new_thread we will be running
	current_thread = new_thread->thread;
	// WE FREE NEW_THREAD SINCE THAT NODE POINTER IS NO LONGE RNEEDED
	new_thread->thread=NULL;
	new_thread->next=NULL;
	free(new_thread);

	// Mark the running of the new thread
	setcontext_calls = 1;
	//printf("CurrentThreadID_AfterContext:%d\n",current_thread->tid);
	
	setcontext(&(current_thread->context));

	interrupts_set(enabled);

	return new_thread_id;
	
}

Tid
thread_yield(Tid want_tid)
{
	int enabled = interrupts_off();

	// printf("Thread_yield\n");
	// printf("Current tid:%d, Want_tid:%d\n", current_thread->tid, want_tid);
	// printf("Before switch:");
	// print_list(ready_list);
	// Free up memory for threads with exiting status
	empty_list(exit_list);
	exit_list = NULL;

	// Deal with out of range Tids
	if ((want_tid >= THREAD_MAX_THREADS) || (want_tid < THREAD_SELF))
	{
		interrupts_set(enabled);
		return THREAD_INVALID;
	}
		

	// Deal with thread yielding to itself, either using Tid or THREAD_SELF
	if (want_tid==current_thread->tid || want_tid==THREAD_SELF)
	{	
		// Might use set/get context later but not necessary
		interrupts_set(enabled);
	
		return current_thread->tid;
	}

	// Tid of the thread we would want to switch to if we find it
	Tid new_thread_tid;

	// Deal with case where we yeild to next thread (FIFO)
	if (want_tid == THREAD_ANY)
	{
		if (ready_list == NULL)
		{
			interrupts_set(enabled);
			return THREAD_NONE;
		}

		// switch to the next thread
		new_thread_tid = switch_context(ready_list->thread->tid, &ready_list, READY);
	}
	// Deal with the case where we have a valid Tid number but it might not be created or was already killed
	else
	{
		if (threads[want_tid] == NULL)
		{
			interrupts_set(enabled);
			return THREAD_INVALID;
		}

		//printf("current tid %d, new tid %d", current_thread->tid, want_tid);
		new_thread_tid = switch_context(want_tid, &ready_list, READY);
	}
	//printf("Yielded to tid:%d\n", new_thread_tid);
	// printf("After switch\n");
	// print_list(ready_list);

	interrupts_set(enabled);

	return new_thread_tid;
}

void
thread_exit(int exit_code)
{
	int enabled = interrupts_off();

	//printf("Thread exit: Going to wait_queue_destroy\n");

	// Wake all threads in wait list of victim, free memory and uninitializa it
	set_exit_code(wait_lists[current_thread->tid], exit_code);
	wait_queue_destroy(wait_lists[current_thread->tid]);
	wait_lists[current_thread->tid] = NULL;


	// Deal with the case where there are no ready threads
	// We should just deal with all exiting threads and then free memory of current thread before exiting with provided exit_code and not returning 
	if (ready_list == NULL)
	{
		empty_list(exit_list);

		void * stack = current_thread->stack;
		free(stack);
		free(current_thread);

		// I dont think we need to even enable them again
		interrupts_set(enabled);

		exit(exit_code);
		

	}
	// Move the current thread to exit_list and switch to the next ready thread instead of returning
	else
	{
		
		switch_context(ready_list->thread->tid, &exit_list, EXITING);
	}
	// No matter the case, number of threads decrements
	thread_count--;
	// interrupts_set(enabled);
	return;
}

Tid
thread_kill(Tid tid)
{
	// WHERE SHOULD INTERRUPTS BE TURNED OFF?
	int enabled = interrupts_off();

	//printf("Thread_Kill\n");
	//printf("Current thread tid:%d, Victim tid:%d\n", current_thread->tid, tid);
	//print_list(ready_list);


	// Similar to yield, deal with out of range Tids
	if (tid >= THREAD_MAX_THREADS || tid < 0)
	{
		interrupts_set(enabled);
		return THREAD_INVALID;

	}

	// Deal with the case of killing yourself
	if (tid == current_thread->tid)
	{
		interrupts_set(enabled);
		return THREAD_INVALID;
	}

	// Deal with a case of threads with tid not existing
	if (threads[tid] == NULL)
	{
		interrupts_set(enabled);
		return THREAD_INVALID;

	}
	
	// Deal with the case of threads that are exiting already
	if (threads[tid]->state == EXITING)
	{
			interrupts_set(enabled);
			return THREAD_INVALID;
	}
	// Deal with a legit thread with tid 
	// Remove thread from ready_list, move it to so that it call be dealth with when it runs 
	node * target_node;

	// printf("Before killing\n");
	// print_list(ready_list);

	// If thread is ready to run, move it to exiting list and clear its waiting list
	if (threads[tid]->state == READY)
	{
		ready_list = remove_node(tid, ready_list, &target_node);
		target_node->thread->state = EXITING;
		exit_list = push_node(exit_list, target_node);
		//printf("From ThreadKill:Waking up  queue destroy\n");
		
	}
	// Otherwise, it has to be waiting so we just change state and itll be moved to
	// exiting state when awoken
	else
	{
		threads[tid]->state = EXITING;
	}
	
	set_exit_code(wait_lists[tid], SIGKILL);
	// Wake all threads in wait list of victim, free memory and uninitializa it
	wait_queue_destroy(wait_lists[tid]);
	wait_lists[tid] = NULL;
	// printf("After killing\n");
	// print_list(ready_list);

	interrupts_set(enabled);

	return tid;
	
}

/**************************************************************************
 * Important: The rest of the code should be implemented in Assignment 2. *
 **************************************************************************/

/* make sure to fill the wait_queue structure defined above */
struct wait_queue *
wait_queue_create()
{
	int enabled = interrupts_off();

	struct wait_queue *wq;

	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	wq->node = NULL;

	interrupts_set(enabled);

	return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{
	int enabled = interrupts_off();

	// wake up all threads before freeing memory
	if (wq->node != NULL)
	{
		//printf("Waking up in queue destroy\n");
		thread_wakeup(wq, 1);
	}
	free(wq);

	interrupts_set(enabled);
	return;
}

Tid
thread_sleep(struct wait_queue *queue)
{
	int enabled = interrupts_off();

	// Deal with case where wait queue is invalid
	if (queue == NULL)
	{
		interrupts_set(enabled);
		return THREAD_INVALID;
	}

	// Deal with case where no other threads are ready to run
	if (ready_list == NULL)
	{
		interrupts_set(enabled);
		return THREAD_NONE;
	}

	// Deal with a valid wait queue and non-empty ready list
	// Put the current thread in wait list with new state and run the ready thread
	Tid new_thread_tid = switch_context(ready_list->thread->tid, &(queue->node), WAITING);

	interrupts_set(enabled);
	return new_thread_tid;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
	int enabled = interrupts_off();

	// Deal with case where wait queue is uninitialized or empty
	if (queue == NULL || queue->node == NULL)
	{
		interrupts_set(enabled);
		return 0;
	}
	
	int woken_threads = 0;

	while (queue->node != NULL)
	{
		node * new_thread;
		queue->node = pop_node(queue->node, &new_thread);

		// Either it is ready to run or wanting to exit
		if (new_thread->thread->state == EXITING)
			exit_list = push_node(exit_list, new_thread);
			
		else
			ready_list = push_node(ready_list, new_thread);

		woken_threads++;

		if (all == 0)
			break;		
	}

	interrupts_set(enabled);
	return woken_threads;
}

/* suspend current thread until Thread tid exits */
Tid
thread_wait(Tid tid, int *exit_code)
{
	int enabled = interrupts_off();
	//printf("Waiting on Thread %d",tid);
	// Similar to yield, deal with out of range Tids
	if (tid >= THREAD_MAX_THREADS || tid < 0)
	{
		interrupts_set(enabled);
		return THREAD_INVALID;

	}

	// Deal with case where tid is of the current thread
	if (tid == current_thread->tid)
	{
		interrupts_set(enabled);
		return THREAD_INVALID;
	}

	// Deal with the case where thread tid exists
	if (threads[tid] == NULL)
	{
		interrupts_set(enabled);
		return THREAD_INVALID;
	}

	// Deal with the case where target thread hasnt already called thread_exit
	if (threads[tid]->state == EXITING)
	{
		interrupts_set(enabled);
		return THREAD_INVALID;
	}

	// To avoid deadlock, maybe we can stop thread_wait if target thread is already waiting for something
	// Not the best solution
	// if (threads[tid]->state == WAITING)
	// {
	// 	interrupts_set(enabled);
	// 	return THREAD_INVALID;
	// }
	// Put current thread in the wait queue of target thread
	thread_sleep(wait_lists[tid]);

	if (exit_code != NULL)
		*exit_code = current_thread->exit_code;

	interrupts_set(enabled);
	return tid;
}

struct lock {
	Tid owner;
	struct wait_queue * queue;
};

struct lock *
lock_create()
{
	int enabled = interrupts_off();

	struct lock *lock;

	lock = malloc(sizeof(struct lock));
	assert(lock);

	// Initialize the wait queue and make the owner tid -1 to indicate its available
	lock->owner = -1;
	lock->queue = wait_queue_create();

	interrupts_set(enabled);

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	int enabled = interrupts_off();

	assert(lock != NULL);

	// Destory the lock only if its not acquired
	if (lock->owner == -1)
	{
		wait_queue_destroy(lock->queue);
		free(lock);
	}	
	
	
	interrupts_set(enabled);
	return;
}

void
lock_acquire(struct lock *lock)
{
	int enabled = interrupts_off();

	assert(lock != NULL);

	// Make every thread sleep as long as the lock is owned by someone
	while (lock->owner != -1)
		thread_sleep(lock->queue);

	// Otherwise, make current thread the owner
	lock->owner = thread_id();

	interrupts_set(enabled);
	return;
	

}

void
lock_release(struct lock *lock)
{
	int enabled = interrupts_off();
	assert(lock != NULL);

	// If the thread releasing the lock is the actual owner, we wake up all waiting threads
	// and let them try to acquire the lock
	if (lock->owner == thread_id())
	{
		lock->owner = -1;
		thread_wakeup(lock->queue, 1);
	}	


	interrupts_set(enabled);
	return;
}

struct cv {
	/* ... Fill this in ... */
	struct wait_queue * queue;

};

struct cv *
cv_create()
{
	int enabled = interrupts_off();

	struct cv *cv;

	cv = malloc(sizeof(struct cv));
	assert(cv);

	// Initialize wait queue for cv
	cv->queue = wait_queue_create();
	
	interrupts_set(enabled);

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	int enabled = interrupts_off();
	
	assert(cv != NULL);

	// Destory the cv if its wait queue is empty. If yes, free all memory and return
	if (cv->queue->node == NULL)
	{
		wait_queue_destroy(cv->queue);
		free(cv);
	}

	
	interrupts_set(enabled);
	return;
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	int enabled = interrupts_off();
	
	assert(cv != NULL);
	assert(lock != NULL);

	// If current thread is the owner of the lock, we want to release the lock, suspend the thread on 
	// condition variable cv and then acquire the lock back before returning. Otherwise, just try acquiring 
	// the lock
	if (lock->owner == thread_id())
	{
		lock_release(lock);
		thread_sleep(cv->queue);
	}

	lock_acquire(lock);

	interrupts_set(enabled);
	return;
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	int enabled = interrupts_off();
	
	assert(cv != NULL);
	assert(lock != NULL);

	// If current thread doesnt own the lock, make it acquire it
	if (lock->owner != thread_id())
	{
		lock_acquire(lock);
	}

	// Wake one thread from cv
	thread_wakeup(cv->queue, 0);
	
	interrupts_set(enabled);
	return;
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	int enabled = interrupts_off();
	
	assert(cv != NULL);
	assert(lock != NULL);

	// If current thread doesnt own the lock, make it acquire it
	if (lock->owner != thread_id())
	{
		lock_acquire(lock);
	}

	// Wake all threads from cv
	thread_wakeup(cv->queue, 1);
	
	interrupts_set(enabled);
	return;
}
