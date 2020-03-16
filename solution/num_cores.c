#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Returns the number of active cores in this machine.
 */
long num_cores() {
    long num = sysconf(_SC_NPROCESSORS_ONLN);
    if (num == -1) {
        perror("ERROR getting number of cores");
    }
    return num;
}

int main(int argc, char** argv) {
    printf("This machine has %ld cores.\n", num_cores());
    return EXIT_SUCCESS;
}
