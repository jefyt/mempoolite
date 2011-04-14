#include <stdio.h>
#include <stdlib.h>

#include "mempoolite.h"

int main()
{
	size_t counter;
	size_t buffer_size;
	size_t min_alloc;
	char *buffer;
	char *temp;
	mempoolite_t pool;
	int test_again;

	/* Get the buffer and size */
	do {
		printf("Enter the total memory block size: ");
		scanf("%u", &buffer_size);
		printf("Enter the minimum memory allocation size: ");
		scanf("%u", &min_alloc);
		buffer = malloc(buffer_size);

		printf("buffer = %p size = %u minimum alloc = %u\n", buffer, buffer_size,
			   min_alloc);

		mempoolite_construct(&pool, buffer, buffer_size, min_alloc, NULL);
		printf("Allocation in loop\n");
		counter = 1;
		while((temp = mempoolite_malloc(&pool, min_alloc)) != NULL) {
			printf("malloc = %p counter = %u\n", temp, counter);
			counter++;
		}

		mempoolite_destruct(&pool);

		free(buffer);

		printf("Test again? <0:false, non-zero:true>: ");
		scanf("%d", &test_again);
	} while(test_again);
	return 0;
}
