/* 
 * File:   dispatchQueue.h
 * Author: robert
 *
 * Modified on August 7, 2018
 */


#ifndef DISPATCHQUEUE_H
#define	DISPATCHQUEUE_H

#ifdef	__cplusplus
extern "C" {
#endif


#include <pthread.h>
#include <semaphore.h>
    
#define error_exit(MESSAGE)     perror(MESSAGE), exit(EXIT_FAILURE)

    typedef enum { // whether dispatching a task synchronously or asynchronously
        ASYNC, SYNC
    } task_dispatch_type_t;
    
    typedef enum { // The type of dispatch queue.
        CONCURRENT, SERIAL
    } queue_type_t;
    
    typedef enum { // whether a queue is running, being waited on, or shutting down
        RUN, WAITED_ON, SHUTTING_DOWN
    } queue_state_t;

    typedef struct task {
        char name[64];          // to identify it when debugging
        void (*work)(void *);   // the function to perform
        void *params;           // parameters to pass to the function
        task_dispatch_type_t type; // asynchronous or synchronous
        sem_t task_semaphore;   // to signal when done, only used if dispatched synchronously
        struct task *next;      // to link to the next task
    } task_t;
    
    typedef struct dispatch_queue_t dispatch_queue_t; // the dispatch queue type

    struct dispatch_queue_t {
        queue_type_t queue_type;        // the type of queue - serial or concurrent
        task_t *head;                	// to keep the linked queue code simple
        task_t *tail;
        pthread_mutex_t queue_lock;     // lock when manipulating the task queue
        sem_t queue_task_semaphore;     // keeps track of available tasks
        pthread_t *threads; 			// one thread per core
        queue_state_t state;            // whether the queue is running normally or being waited on
        int num_threads;
    };
    
    task_t *task_create(void (*)(void *), void *, char*);
    
    void task_destroy(task_t *);

    dispatch_queue_t *dispatch_queue_create(queue_type_t);
    
    void dispatch_queue_destroy(dispatch_queue_t *);
    
    int dispatch_async(dispatch_queue_t *, task_t *);
    
    int dispatch_sync(dispatch_queue_t *, task_t *);
    
    void dispatch_for(dispatch_queue_t *, long, void (*)(long));
    
    int dispatch_queue_wait(dispatch_queue_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* DISPATCHQUEUE_H */
