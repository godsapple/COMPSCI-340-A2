/* 
 * File:   test6.c
 * Author: robert
 */

#include "dispatchQueue.h"
#include <stdio.h>
#include <stdlib.h>

volatile long counter = 0;

void increment(void *identifier) {
    printf("task \"%s\"\n", (char *) identifier);
    long i;
    for (i = 0; i < 1000000000; i++)
        counter++;
}

void decrement(void *identifier) {
    printf("task \"%s\"\n", (char *) identifier);
    long i;
    for (i = 0; i < 1000000000; i++)
        counter--;
}

/*
 * Checks to see if tasks are serial.
 */
int main(int argc, char** argv) {
    // create a concurrent dispatch queue
    dispatch_queue_t * serial_dispatch_queue;
    task_t *task;
    serial_dispatch_queue = dispatch_queue_create(SERIAL);
    char id;
    char names[10][64]; // because these are going to be parameters to tasks they have to hang around
    for (id = 'A'; id <= 'J'; id++) {
        char *name = names[id - 'A'];
        sprintf(name, "Serial %c", id);
        task = task_create(increment, (void *) name, name);
        dispatch_async(serial_dispatch_queue, task);
    }
    printf("Serial tasks safely dispatched. counter: %ld\n", counter);
    
    dispatch_queue_t *concurrent_dispatch_queue;
    concurrent_dispatch_queue = dispatch_queue_create(CONCURRENT);
    char more_names[2][64];
    for (id = 'A'; id <= 'B'; id++) {
        char *name = more_names[id - 'A'];
        sprintf(name, "Concurrent %c", id);
        task = task_create(decrement, (void *)name, name);
        dispatch_sync(concurrent_dispatch_queue, task);
    }
    printf("Concurrent task safely dispatched and completed. counter: %ld\n", counter);
    
    
    dispatch_queue_wait(serial_dispatch_queue);
    printf("The final counter is %ld\n", counter);
    dispatch_queue_destroy(serial_dispatch_queue);
    dispatch_queue_destroy(concurrent_dispatch_queue);
    return EXIT_SUCCESS;
}
