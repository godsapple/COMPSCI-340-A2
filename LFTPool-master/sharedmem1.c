#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>

void main() {
	void *shared;
	int *a_number;
	int b_number;

	shared = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
	printf("Program 1 - shared:%p\n", shared);

	a_number = (int *)shared;
	printf("a_number: %d, b_number: %d\n", *a_number, b_number);

	if (fork() != 0) {
		*a_number = 12345;
		b_number = 54321;
	}
	printf("a_number: %d, b_number: %d\n", *a_number, b_number);
}