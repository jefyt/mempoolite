#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "mempoolite.h"

typedef struct multithreaded_param {
	size_t index;
	mempoolite_t *pool;
	int req_size;
} multithreaded_param_t;

void *
multithreaded_main(void *args) {
	multithreaded_param_t *param;
	void *buffer;
	int round_size;

	param = (multithreaded_param_t *) args;

	round_size = mempoolite_roundup(param->pool, param->req_size);
	printf("index: %u requested size: %d round-up size: %d\n", param->index,
		param->req_size, round_size);
	while ((buffer = mempoolite_malloc(param->pool, param->req_size)) != NULL) {
		printf("index: %u address: %p size: %d\n", param->index, buffer,
			param->req_size);
		/* Sleep for 1 millisecond to give other threads to run */
		usleep(1000);
	}

	return NULL;
}

int
main() {
	size_t alloc_counter;
	size_t buffer_size;
	size_t min_alloc;
	size_t num_threads;
	char *large_buffer;
	char *alloc_ret;
	mempoolite_t mempool;
	pthread_t *threads;
	multithreaded_param_t *threads_param;
	mempoolite_lock_t pool_lock;
	pthread_mutex_t mutex;
	int test_again;
	int scanf_ret;

	pthread_mutex_init(&mutex, NULL);

	/* Get the buffer and size */
	do {
		printf("Memory pool testing using mempoolite API\n");
		printf("Enter the total memory block size: ");
		scanf_ret = scanf("%u", &buffer_size);
		printf("Enter the minimum memory allocation size: ");
		scanf_ret = scanf("%u", &min_alloc);
		printf("Enter the number of threads to run for multi-threaded test: ");
		scanf_ret = scanf("%u", &num_threads);
		large_buffer = malloc(buffer_size);

		printf("buffer = %p size = %u minimum alloc = %u\n", large_buffer, buffer_size,
				min_alloc);

		mempoolite_init(&mempool, large_buffer, buffer_size, min_alloc, NULL);
		printf("Single-threaded test...\n");
		alloc_counter = 1;
		while ((alloc_ret = mempoolite_malloc(&mempool, min_alloc)) != NULL) {
			printf("malloc = %p counter = %u\n", alloc_ret, alloc_counter);
			alloc_counter++;
		}
		mempoolite_print_stats(&mempool, puts);

		printf("Multi-threaded test...\n");
		pool_lock.arg = (void *) &mutex;
		pool_lock.acquire = (int (*)(void *))pthread_mutex_lock;
		pool_lock.release = (int (*)(void *))pthread_mutex_unlock;
		mempoolite_init(&mempool, large_buffer, buffer_size, min_alloc, &pool_lock);
		threads = (pthread_t *) malloc(sizeof (*threads) * num_threads);
		threads_param = (multithreaded_param_t *) malloc(sizeof (*threads_param) * num_threads);
		/* Run all the threads */
		for (alloc_counter = 0; alloc_counter < num_threads; alloc_counter++) {
			threads_param[alloc_counter].index = alloc_counter;
			threads_param[alloc_counter].req_size = min_alloc;
			threads_param[alloc_counter].pool = &mempool;
			pthread_create(&threads[alloc_counter], NULL, multithreaded_main, &threads_param[alloc_counter]);
		}
		/* Wait for all the threads to finish */
		for (alloc_counter = 0; alloc_counter < num_threads; alloc_counter++) {
			pthread_join(threads[alloc_counter], NULL);
		}
		mempoolite_print_stats(&mempool, puts);
		free(threads_param);
		free(threads);

		free(large_buffer);

		printf("Test again? <0:false, non-zero:true>: ");
		scanf_ret = scanf("%d", &test_again);
	}	while (test_again);

	pthread_mutex_destroy(&mutex);

	return 0;
}
