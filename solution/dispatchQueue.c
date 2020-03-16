/*
 *      A simple dispatch queue for A2.
 */

#include "dispatchQueue.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// task_t *last_task = NULL;

/*
 * Returns the number of active cores in this machine.
 */
long num_cores() {
    long num = sysconf(_SC_NPROCESSORS_ONLN);
    if (num == -1) {
        error_exit("ERROR getting number of cores");
    }
    return num;
}

/*
 * Creates a task wrapper around a function to call with some parameters.
 */
task_t *task_create(void (*work)(void *), void *params, char* name /* = "name" */) {
    task_t *task = (task_t *) malloc(sizeof (task_t));
    if (task == NULL)
        error_exit("malloc creating task");
    strcpy(task->name, name);
    task->work = work;
    task->params = params;
    task->type = ASYNC; // by default
    return task;
}

/*
 * Destroys the task.
 * Ensuring that its memory is not lost.
 */
void task_destroy(task_t *task) {
    free(task);
}

void *thread_worker(void *q) {
    dispatch_queue_t *queue = (dispatch_queue_t*)q;
    task_t *task;
    for (;;) {
        // wait until there is some work to do
        if (sem_wait(&queue->queue_task_semaphore)) {
            error_exit("ERROR in thread_worker waiting on semaphore");
        }
        // lock the queue while removing the next task
        if (pthread_mutex_lock(&queue->queue_lock))
            error_exit("ERROR in thread_worker getting the queue lock 1");
        // If nothing in the queue, must be finished - time to go
        if (queue->head == NULL) {
            if (pthread_mutex_unlock(&queue->queue_lock))
                error_exit("ERROR in thread_worker no task in queue");
            return NULL;
        }
        task = queue->head;
        // unlink the new task
        queue->head = task->next;
        if (queue->head == NULL)
        	queue->tail = NULL;
        // unlock the queue
        if (pthread_mutex_unlock(&queue->queue_lock))
            error_exit("ERROR in thread_worker releasing the queue lock 1");
        // The following line does the actual task work.
        task->work(task->params);
        // if dispatched synchronously notify the calling thread
        if (task->type == SYNC && sem_post(&task->task_semaphore)) {
            error_exit("ERROR in thread_worker signalling synchronous caller");
        }
        if (task->type == SYNC && sem_destroy(&task->task_semaphore)) {
            error_exit("ERROR in thread_worker destroying task semaphore");
        }
        task_destroy(task);
    }
}

/*      Creates a dispatch queue. 
 *      At the moment this is not deallocated.
 *      returns a pointer to the dispatch queue or NULL if unsuccessful.
 */
dispatch_queue_t *dispatch_queue_create(queue_type_t queue_type) {
    dispatch_queue_t *queue = (dispatch_queue_t *) malloc(sizeof (dispatch_queue_t));
    // set up the queue
    if (queue) {
        queue->queue_type = queue_type;
        queue->head = NULL;
        queue->tail = NULL;

        if (pthread_mutex_init(&queue->queue_lock, NULL))
            error_exit("ERROR in dispatch_queue_create pthread_mutex_init");

        if (sem_init(&queue->queue_task_semaphore, 0, 0))
            error_exit("ERROR in dispatch_queue_create sem_init task semaphore");

        if (queue_type == CONCURRENT)
            queue->num_threads = num_cores();
        else
            queue->num_threads = 1;

        pthread_t *threads = (pthread_t *) calloc(queue->num_threads, sizeof (pthread_t));
        if (threads == NULL) {
            error_exit("ERROR in dispatch_queue_create can't allocate memory for threads");
        }
        queue->threads = threads;
        for (int i = 0; i < queue->num_threads; i++) {
            if (pthread_create(&threads[i], NULL, thread_worker, (void *)queue))
                error_exit("queue worker thread init");
        }
        queue->state = RUN;

    }
    return queue;
}

/*
 * Dismantles a dispatch queue.
 */
void dispatch_queue_destroy(dispatch_queue_t *queue) {
    queue->state = SHUTTING_DOWN;
    if (sem_destroy(&queue->queue_task_semaphore)) {
        error_exit("ERROR in dispatch_queue_destroy destroying task semaphore");
    }
    free(queue->threads);
    free(queue);
}

/*      Adds a task to a dispatch queue for asynchronous execution.
 *      This means it returns without blocking.
 *      At some later stage the task will be dispatched in a separate thread.
 */
int dispatch_async(dispatch_queue_t *queue, task_t *task) {
    // add the task to the queue
    // lock the queue
    if (pthread_mutex_lock(&queue->queue_lock))
        error_exit("ERROR in dispatch_async pthread_mutex_lock");

    if (queue->state == RUN) {
    	if (queue->head) {
    		queue->tail->next = task;
    		queue->tail = task; 
    	} else {
    		queue->head = task;
    		queue->tail = task;
    	}
	}

    // now release the queue lock
    if (pthread_mutex_unlock(&queue->queue_lock))
        error_exit("ERROR in dispatch_async pthread_mutex_unlock");
    // and signal the queue dispatch thread
    if (sem_post(&queue->queue_task_semaphore))
        error_exit("ERROR in dispatch_async signalling dispatch thread");
}

/*      Adds a task to a dispatch queue for synchronous execution.
 *      Doesn't return until the task has completed.
 */
int dispatch_sync(dispatch_queue_t *queue, task_t *task) {
    task->type = SYNC;
    // create a task semaphore so we can be notified when it is finished
    if (sem_init(&task->task_semaphore, 0, 0))
        error_exit("ERROR in dispatch_sync initialising semaphore");
    // first just add the task to the queue
    dispatch_async(queue, task);
    // then wait for it to complete
    if (sem_wait(&task->task_semaphore))
        error_exit("ERROR in dispatch_sync waiting for task semaphore");
    if (sem_destroy(&task->task_semaphore))
		error_exit("ERROR in dispatch_sync destroying task semaphore");
}

/*      Used to dispatch iterations as in a for loop.
 *      Each iteration is independent of the others.
 */
void dispatch_for(dispatch_queue_t *queue, long count, void (*work)(long)) {
    for (long i = 0; i < count; i++) {
        // construct a task out of the work?
        task_t *task;
        task = task_create((void (*)(void *))work, (void *)i, "parallel for loop");
        dispatch_async(queue, task);
    }
    // wait until all tasks have completed
    dispatch_queue_wait(queue);
}

/*      Waits until all tasks submitted to this queue have finished.
 *      This blocks the calling thread.
 */
int dispatch_queue_wait(dispatch_queue_t *queue) {
    // lock the queue before setting in case being concurrently accesssed
    if (pthread_mutex_lock(&queue->queue_lock))
        error_exit("ERROR in dispatch_queue_wait getting the queue lock");

    queue->state = WAITED_ON;

    if (pthread_mutex_unlock(&queue->queue_lock))
        error_exit("ERROR in dispatch_queue_wait releasing the queue lock");
    // signal all threads so that they run and find nothing to do
    for (long i = 0; i < queue->num_threads; i++)
        if (sem_post(&queue->queue_task_semaphore))
	        error_exit("ERROR in dispatch_async signalling dispatch thread");

	pthread_t *threads = queue->threads;
	for (int i = 0; i < queue->num_threads; i++)
        if (pthread_join(threads[i], NULL))
            error_exit("dispatch_queue_wait thread join");
    return 0;
}
