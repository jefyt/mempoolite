#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "mempoolite.h"

typedef struct multithreaded_param
{
	size_t index;
	mempoolite_t *pool;
	size_t min_alloc;
} multithreaded_param_t;

void *multithreaded_main(void *args)
{
	multithreaded_param_t *param;
	void *buffer;

	param = (multithreaded_param_t *)args;

	while((buffer = mempoolite_malloc(param->pool, param->min_alloc)) != NULL) {
		printf("index: %u malloc = %p\n", param->index, buffer);
	}

	return NULL;
}

int main()
{
	size_t counter;
	size_t buffer_size;
	size_t min_alloc;
	size_t num_threads;
	char *buffer;
	char *temp;
	mempoolite_t pool;
	pthread_t *threads;
	multithreaded_param_t *threads_param;
	mempoolite_lock_t pool_lock;
	pthread_mutex_t mutex;
	int test_again;

	pthread_mutex_init(&mutex, NULL);

	/* Get the buffer and size */
	do {
		printf("Memory pool testing using mempoolite API\n");
		printf("Enter the total memory block size: ");
		scanf("%u", &buffer_size);
		printf("Enter the minimum memory allocation size: ");
		scanf("%u", &min_alloc);
		printf("Enter the number of threads to run for multi-threaded test: ");
		scanf("%u", &num_threads);
		buffer = malloc(buffer_size);

		printf("buffer = %p size = %u minimum alloc = %u\n", buffer, buffer_size,
			   min_alloc);

		mempoolite_init(&pool, buffer, buffer_size, min_alloc, NULL);
		printf("Single-threaded test...\n");
		counter = 1;
		while((temp = mempoolite_malloc(&pool, min_alloc)) != NULL) {
			printf("malloc = %p counter = %u\n", temp, counter);
			counter++;
		}

		printf("Multi-threaded test...\n");
		pool_lock.arg = (void *)&mutex;
		pool_lock.acquire = (int (*)(void *arg))pthread_mutex_lock;
		pool_lock.release = (int (*)(void *arg))pthread_mutex_unlock;
		mempoolite_init(&pool, buffer, buffer_size, min_alloc, &pool_lock);
		threads = (pthread_t *)malloc(sizeof(*threads) * num_threads);
		threads_param = (multithreaded_param_t *)malloc(sizeof(*threads_param) * num_threads);
		/* Run all the threads */
		for(counter = 0; counter < num_threads; counter++) {
			threads_param[counter].index = counter;
			threads_param[counter].min_alloc = min_alloc;
			threads_param[counter].pool = &pool;
			pthread_create(&threads[counter], NULL, multithreaded_main, &threads_param[counter]);
		}
		/* Wait for all the threads to finish */
		for(counter = 0; counter < num_threads; counter++) {
			pthread_join(threads[counter], NULL);
		}
		free(threads_param);
		free(threads);

		free(buffer);

		printf("Test again? <0:false, non-zero:true>: ");
		scanf("%d", &test_again);
	} while(test_again);

	pthread_mutex_destroy(&mutex);

	return 0;
}
