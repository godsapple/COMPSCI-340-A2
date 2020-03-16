/* 
 * File:   dispatchQueue.c
 * Author: FU, QIANG
 *
 * Modified by: qfu638
 */
#include "dispatchQueue.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/sysinfo.h>
#include <unistd.h>

// implementation of a linked list enqueue function
void push_dispatch_queue(dispatch_queue_t *queue, task_t task) {
    sll_node *current_node = queue->nodeHead;
    sll_node *newNode = (sll_node*) malloc(sizeof(sll_node));
    newNode->task = task;
    newNode->next = NULL;

    while(current_node->next != NULL) {
        current_node = current_node->next;
    }
    current_node->next = newNode;
}

// implementation of a linked list dequeue function
sll_node *pop_dispatch_queue(dispatch_queue_t *queue) {
    sll_node *previousHead = queue->nodeHead;
    queue->nodeHead = previousHead->next;
    return previousHead;
}

// used to push a head onto the queue if it at any point is null
void push_head_on_queue(dispatch_queue_t *queue, task_t *task){
    sll_node *head = (sll_node*) malloc(sizeof(sll_node));
    head->next = NULL;
    head->task = *task;
    queue->nodeHead = head;
}

// method that releases the queue, semaphore and queue_mutex lock memory when called
void dispatch_queue_destroy(dispatch_queue_t *queue) {
    // destroy mutexs
    pthread_mutex_destroy(&queue->queue_mutex);
    pthread_mutex_destroy(&queue->busy_thread_mutex);

    // destory semaphores
    sem_destroy(&queue->all_done_semaphore);
    sem_destroy(&queue->queue_semaphore);
    free(queue);
}

// method that releases the memeory of a task when executed
void destroy_task(task_t *task) {
    free(task);
}

// method that each thread calls when they run concurrently
void *perform_tasks(void *param){
    // case to dispatch queue to prevent compiler warnings
    dispatch_queue_t *queue = (dispatch_queue_t*) param;

    // poll indefinatly
    while(1) {
        // wait until more tasks are push to queue
        sem_wait(&queue->queue_semaphore);

        //check if termination condition has been met; break if met
        if(queue->terminate_condition == 1){
            break;
        }

        // update the queue's active thread counter and aquire queue_mutex lock
        pthread_mutex_lock(&queue->queue_mutex);
        pthread_mutex_lock(&queue->busy_thread_mutex);      
        queue->busy_threads = queue->busy_threads + 1;
        pthread_mutex_unlock(&queue->busy_thread_mutex);
      
        // dequeuing a task from the queue
        sll_node *targetNode = pop_dispatch_queue(queue);

        switch(queue->queue_type) {
        case CONCURRENT:
            // In the case of a concurrent queue, release the lock before performing the task
            pthread_mutex_unlock(&queue->queue_mutex);
            targetNode->task.work(targetNode->task.params);
            // release memory
            destroy_task(&targetNode->task);
            // decrement active thread on the queue as this thread is finished
            pthread_mutex_lock(&queue->busy_thread_mutex);      
            queue->busy_threads = queue->busy_threads - 1;
            pthread_mutex_unlock(&queue->busy_thread_mutex);
            break;
  
        case SERIAL:
            // In the case of a serial queue, release the lock after performing the task
            targetNode->task.work(targetNode->task.params);
            // release memory
            destroy_task(&targetNode->task);
            // decrement active thread on the queue as this thread is finished & release lock
            pthread_mutex_lock(&queue->busy_thread_mutex);      
            queue->busy_threads = queue->busy_threads - 1;
            pthread_mutex_unlock(&queue->busy_thread_mutex);
            pthread_mutex_unlock(&queue->queue_mutex);
            break;
        }
      
        if(queue->nodeHead == NULL && queue->busy_threads == 0){
            sem_post(&queue->all_done_semaphore);
        }
    }
    //should any threads still exist after the parent thread is killed then this statement will kill it
    pthread_exit(EXIT_SUCCESS);
}

void generate_threads(dispatch_queue_t *queue, queue_type_t queue_type) {
    // generate threads and execute the tasks on the semaphore queue
    int i;
    for(i = 0; i < get_nprocs_conf(); i++){
        pthread_t *thread = (pthread_t *) malloc(sizeof(pthread_t));
        pthread_create(thread , NULL, perform_tasks, queue);

    }
}

dispatch_queue_t *dispatch_queue_create(queue_type_t queue_type) {
    // delearing variables to put in the structure
    sem_t queue_semaphore;
    sem_t all_done_semaphore;
    pthread_mutex_t queue_mutex, busy_thread_mutex;
    dispatch_queue_t *queue = (dispatch_queue_t *) malloc(sizeof(dispatch_queue_t));

    // set up dispatch queue
    queue->queue_type = queue_type;
    queue->queue_mutex = queue_mutex;
    queue->busy_thread_mutex = busy_thread_mutex;
    queue->terminate_condition = 0;

    // set up dispatch queue thread
    queue->queue_semaphore = queue_semaphore;
    queue->all_done_semaphore = all_done_semaphore;

    // create a semaphore & queue_mutex lock
    sem_init(&queue->queue_semaphore, 0, 0);
    sem_init(&queue->all_done_semaphore, 0, 0);
    pthread_mutex_init(&queue->queue_mutex, NULL);
    pthread_mutex_init(&queue->busy_thread_mutex, NULL);

    //generate threads
    generate_threads(queue, queue_type);
    return queue;
}

task_t *task_create(void (* work)(void *), void *param, char *name) {
    // allocate memeory for the tast and populate the structure
    task_t *task = (task_t *) malloc(sizeof(task_t));
    *task->name = *name;
    task->work = (void (*)(void *))work;
    task->params = param;

    // This is never used but for best practice a default of ASYNC is assigned
    task->type = ASYNC;
    return task;
}

//peform task on the thread that calls it
int dispatch_sync(dispatch_queue_t *queue, task_t *task){
    // perform the task then destroy it
    task->work(task->params);
    destroy_task(task);
    return 0;
}

// the header specifies a int as the return value so that is what is done here, even thought the integer return value is never used
int dispatch_async(dispatch_queue_t *queue, task_t *task) {
    // lock queue
    pthread_mutex_lock(&queue->queue_mutex);

    // check if the head of the linked list is assigned, if not assign the incoming task as the head of the queue
    if(queue->nodeHead == NULL) {
        push_head_on_queue(queue, task);
    } else {
        push_dispatch_queue(queue, *task);
    }
  
    // unlock queue
    pthread_mutex_unlock(&queue->queue_mutex);
  
    // notify semaphore that new tasks avalible
    sem_post(&queue->queue_semaphore);
    return 0;
}

int dispatch_queue_wait(dispatch_queue_t *queue) {
    // block until there are no more active theads on the queue and there are no more tasks on the queue
    sem_wait(&queue->all_done_semaphore);
    // flush all waiting works waiting on the queue semaphore
    queue->terminate_condition = 1;
    int index;
    for(index = 0; index < get_nprocs_conf(); index++) {
        sem_post(&queue->queue_semaphore);
    }
    return 0;
}

void dispatch_for(dispatch_queue_t *queue, long number, void (*work) (long)) {
    int id;
    char names[number][2]; // because these are going to be parameters to tasks they have to hang around
    for(id = 'A'; id < 'A' + number; id++){
        char *name = names[id - 'A'];
        name[0] = id;
        name[1] = '\0';
        long param_value = id - 'A';
        task_t *task = task_create((void *)work, (void *)param_value, name);
        dispatch_async(queue, task);
    }
    dispatch_queue_wait(queue);
    dispatch_queue_destroy(queue);
}