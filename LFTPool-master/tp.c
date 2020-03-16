#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

/*---------------------------------------------------------------
 * STRUCTS
 *--------------------------------------------------------------*/

typedef struct threadpool_t threadpool_t;
typedef struct threadpool_task_t threadpool_task_t;

struct threadpool_t{
	/* threadpool_t contains all pieces of the thread pool.
	 * we will use a linked queue representation for the tasks 
	 * and an array for the threads */
	int size;
	sem_t mutex, full;
	pthread_t *threads;
	threadpool_task_t *head;
	threadpool_task_t *tail;
};

struct threadpool_task_t{
	/* represents a single task in a linked list structure */
	void (*function)(void *);
	void * args;
	threadpool_task_t * next;
};


/*-------------------------------------------------------------
 * THREADPOOL PROCEDURES
 *-----------------------------------------------------------*/

void threadpool_shutdown( threadpool_t* pool ){
	/* frees all dynamically allocated resources of pool. joins all of the pthreads in the pool */
	threadpool_task_t * temp;
	int i;	

	/* deallocate task queue */
	while( pool->head ){
		temp = pool->head;
		pool->head=pool->head->next;
		free(temp);	
	}

	/* join threads */
	for(i = 0; i < pool->size; i++){
		if( pthread_join(pool->threads[i], NULL) != 0 ){
			fprintf(stderr, "Could not join thread #%d.\n", i);
		}
	}

	free(pool->threads);
	free(pool);
}

int threadpool_add_task( threadpool_t * pool, void (*function)(void *), void *args ){
	/* creates a new threadpool_task_t variable with the given function and arguments and 
 	 * enqueues it in the task queue. */
	threadpool_task_t * task;

	task = (threadpool_task_t *)malloc( sizeof(threadpool_task_t) );
	if( task == NULL ){
		fprintf(stderr, "Memory allocation failed while allocating memory for a task.\n");
		return -1;
	}
	task->function = function;
	task->args = args;
	task->next = NULL;

	sem_wait(&pool->mutex);
	if(pool->head == NULL && pool->tail == NULL){
		pool->head = task;
		pool->tail = task;
	}
	else{
		pool->tail->next = task;
		pool->tail = pool->tail->next;
	}
	sem_post(&pool->mutex);
	sem_post(&pool->full);
	return 0;
}

void * threadpool_wait( threadpool_t * pool ){
	/* this is the main function for threads in the pool. threads will use semaphores to request access to
	   the task queue, and perform the task at the head of the queue */ 
	threadpool_task_t * task;

	do{
		sem_wait(&pool->full);
		sem_wait(&pool->mutex);
		
		task = pool->head;
		/* dequeue task at head, run it, and deallocate memory */
		if( pool->head == pool->tail ){
			pool->head = pool->tail = NULL;
		}else{
			pool->head = pool->head->next;
		}
		sem_post(&pool->mutex);
		task->function(task->args);
		free(task);
	}while( 1 );

	return NULL;
}

void * threadpool_monitor( threadpool_t * pool ){
	/* this procedure is meant to be ran by a thread whose job is to monitor the thread pool.
 	 * it checks the threads in the pool every 5 seconds and restarts threads that have been stopped. */
	int i;
	do{
		sleep(5);
		//printf("checking\n");
		for(i = 0; i < pool->size; i++){
			if(pthread_kill(pool->threads[i], 0) != 0){
				/* thread has died */
				if(pthread_create( &pool->threads[i], NULL, (void *)threadpool_wait, pool) != 0){
					fprintf(stderr, "Error creating thread #%d.\n", i);
					exit(EXIT_FAILURE);
				}
				printf("Thread #%d stopped. It was restarted.\n", i);		
			}
		}
	}while( 1 );	
	return NULL;
}

int threadpool_init( threadpool_t * pool, int N ){
	/* creates a threadpool of size N, initializes a job queue, and starts monitoring thread */
	/* Returns 0 if success, 1 if error */

	int i, rc;
	sigset_t old;
	
	/* check if N (# of threads) is valid. print error and return -1 if invalid */
	if( N < 1 ){
		fprintf(stderr, "Thread pool size must be greater than or equal to 1\n");
		return -1;
	}
	pool->size = N;

	/* initialize semaphores - if there is an error creating, then print error and return -1. */
	if( sem_init(&pool->mutex, 0, 1) == -1 ){
		fprintf(stderr, "Error creating mutex semaphore.\n");
		return -1;
	}
	if( sem_init(&pool->full, 0, 0) == -1){
		fprintf(stderr, "Error creating full semaphore.\n");
		return -1;
	}

	/* initialize task queue */
	pool->head = NULL;
	pool->tail = NULL;

	/* allocate memory for threads - N threads for pool + 1 signal handling thread */
	pool->threads = (pthread_t *) malloc( sizeof(pthread_t) * (N+1) );
	if( pool->threads == NULL){
		fprintf(stderr, "Memory allocation failed when creating pthreads.\n");
		return -1;
	}
 
	/* initialize threads 0 to N-1 and dispatch to wait for tasks */
	for(i = 0; i < N; i++){
		rc = pthread_create( &pool->threads[i], NULL, (void *)threadpool_wait, pool);
		if( rc != 0 ){
			fprintf(stderr, "Error creating thread #%d.\n", i);
			return -1;	
		}
	}		

	/* initialize Nth thread to handle signals */
	rc = pthread_create( &pool->threads[N], NULL, (void *)threadpool_monitor, pool );
	if( rc!= 0 ){
		fprintf(stderr, "Error creating thread #%d.\n", N);
		return -1;
	}
	
	return 0;
}
