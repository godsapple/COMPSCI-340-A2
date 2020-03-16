#include "dispatchQueue.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

void test0() {
    sleep(1);
    printf("test0 running\n");
}

/* In the submitted version a function like this would be in 
   your dispatchQueue.c file. */
task_t *task_create(void (*work)(void *), void *params, char *name) {
    task_t *task = (task_t *)malloc(sizeof (task_t));
    task->work = work; // (*task).work = work;
    task->params = params;
    strcpy(task->name, name);
    return task;
}

void *dummy_runner(void *run_task) {
    task_t *my_task = (task_t *)run_task;
    printf("starting dummy_runner\n");
    my_task->work(NULL); /* possibly some parameters later */
    printf("finished dummy_runner and %s\n", my_task->name);
}

int main(int argc, char** argv) {
    task_t *task;
    pthread_t test_thread;
    task = task_create(test0, NULL, "test0");
    printf("Before\n");
    pthread_create(&test_thread, NULL, (void *)dummy_runner, (void *)task);
    printf("After\n");
    pthread_join(test_thread, NULL);
    return EXIT_SUCCESS;
}