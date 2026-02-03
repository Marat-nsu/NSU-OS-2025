#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

void *child_thread(void *arg)
{
	int counter = 0;
	while (1)
	{
		printf("Child thread: message %d\n", counter++);
		fflush(stdout);
		usleep(100000); // 100 ms.
	}

	return NULL;
}

int main()
{
	pthread_t thread;
	int result;

	result = pthread_create(&thread, NULL, child_thread, NULL);
	if (result != 0)
	{
		perror("Thread creation failed");
		exit(1);
	}

	printf("Parent thread: child thread created\n");

	sleep(2);

	printf("Parent thread: canceling child thread\n");
	result = pthread_cancel(thread);
	if (result != 0)
	{
		perror("Thread cancel failed");
		exit(1);
	}

	result = pthread_join(thread, NULL);
	if (result != 0)
	{
		perror("pthread_join failed");
		exit(1);
	}

	printf("Parent thread: child thread finished\n");

	return 0;
}
