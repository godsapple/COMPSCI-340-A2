/* 
 * File:   dispatchQueue.h
 * Author: FU, QIANG
 *
 * Modified by: qfu638
 */

#ifndef DISPATCHQUEUE_H
#define   DISPATCHQUEUE_H

#include <pthread.h>
#include <semaphore.h>
  
#define error_exit(MESSAGE)     perror(MESSAGE), exit(EXIT_FAILURE)

    typedef enum { // whether dispatching a task synchronously or asynchronously
        ASYNC, SYNC
    } task_dispatch_type_t;
  
    typedef enum { // The type of dispatch queue.
        CONCURRENT, SERIAL
    } queue_type_t;

    typedef struct task {
        char name[64];              // to identify it when debugging
        void (*work)(void *);       // the function to perform
        void *params;               // parameters to pass to the function
        task_dispatch_type_t type; // asynchronous or synchronous
    } task_t;
  
    typedef struct dispatch_queue_t dispatch_queue_t; // the dispatch queue type
    typedef struct dispatch_queue_thread_t dispatch_queue_thread_t; // the dispatch queue thread type
    typedef struct sll_node sll_node; // the singly linked list data structure type

    struct sll_node {
        task_t task;                // the task to be executed
        struct sll_node *next;      // link to the next item in the linked list
    };                              // singly linked list

    struct dispatch_queue_thread_t {
        dispatch_queue_t *queue;    // the queue this thread is associated with
        pthread_t thread;           // the thread which runs the task
        sem_t thread_semaphore;     // the semaphore the thread waits on until a task is allocated
        task_t *task;               // the current task for this tread
    };

    struct dispatch_queue_t {
        queue_type_t queue_type;            // the type of queue - serial or concurrent
        struct sll_node *nodeHead;          // first item in the linked list
        pthread_mutex_t queue_mutex;        // mutex associated with this queue
        pthread_mutex_t busy_thread_mutex; // mutex associated with this queue
        sem_t queue_semaphore;              // the semaphore on the queue to notify threads when a task is ready
        sem_t all_done_semaphore;           // semaphore used to notify the main thread that all tasks are completed
        int terminate_condition;            // condition used to determine whether or not the queue workers should terminate and shutdown
        int busy_threads;                   // number of activly threads working on a task from this queue
    };
  
    task_t *task_create(void (*)(void *), void *, char*);
  
    void task_destroy(task_t *);

    dispatch_queue_t *dispatch_queue_create(queue_type_t);
  
    void dispatch_queue_destroy(dispatch_queue_t *);
  
    int dispatch_async(dispatch_queue_t *, task_t *);
  
    int dispatch_sync(dispatch_queue_t *, task_t *);
  
    void dispatch_for(dispatch_queue_t *, long, void (*)(long));
  
    int dispatch_queue_wait(dispatch_queue_t *);

    void push_dispatch_queue(dispatch_queue_t *queue, task_t task);

    sll_node *pop_dispatch_queue(dispatch_queue_t *queue);

#endif   /* DISPATCHQUEUE_H */