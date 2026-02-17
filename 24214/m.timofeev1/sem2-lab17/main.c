#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>

#define MAX_CHUNK_LEN 80

typedef struct Node
{
	char *text;
	struct Node *next;
} Node;

static Node *head = NULL;
static pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;
static _Atomic bool running = true;

static void push_front_locked(const char *text)
{
	Node *node = (Node *)malloc(sizeof(Node));
	if (node == NULL)
	{
		perror("malloc");
		return;
	}

	node->text = strdup(text);
	if (node->text == NULL)
	{
		perror("strdup");
		free(node);
		return;
	}

	node->next = head;
	head = node;
}

static void print_list_locked(void)
{
	const Node *current = head;

	puts("--- current list ---");
	if (current == NULL)
	{
		puts("(empty)");
	}

	while (current != NULL)
	{
		puts(current->text);
		current = current->next;
	}
	puts("--------------------");
}

static void bubble_sort_locked(void)
{
	if (head == NULL || head->next == NULL)
	{
		return;
	}

	bool swapped;
	Node *end = NULL;

	do
	{
		swapped = false;
		Node *current = head;

		while (current->next != end)
		{
			if (strcmp(current->text, current->next->text) > 0)
			{
				char *tmp = current->text;
				current->text = current->next->text;
				current->next->text = tmp;
				swapped = true;
			}
			current = current->next;
		}

		end = current;
	} while (swapped);
}

static void free_list_locked(void)
{
	Node *current = head;
	head = NULL;

	while (current != NULL)
	{
		Node *next = current->next;
		free(current->text);
		free(current);
		current = next;
	}
}

static void add_line_chunks_to_front(const char *line)
{
	size_t len = strlen(line);

	if (len == 0)
	{
		return;
	}

	size_t chunks = (len + MAX_CHUNK_LEN - 1) / MAX_CHUNK_LEN;

	pthread_mutex_lock(&list_mutex);
	for (size_t chunk_index = chunks; chunk_index > 0; --chunk_index)
	{
		size_t start = (chunk_index - 1) * MAX_CHUNK_LEN;
		size_t piece_len = len - start;
		if (piece_len > MAX_CHUNK_LEN)
		{
			piece_len = MAX_CHUNK_LEN;
		}

		char piece[MAX_CHUNK_LEN + 1];
		memcpy(piece, line + start, piece_len);
		piece[piece_len] = '\0';

		push_front_locked(piece);
	}
	pthread_mutex_unlock(&list_mutex);
}

static void *sort_thread(void *arg)
{
	(void)arg;

	while (atomic_load(&running))
	{
		sleep(5);

		pthread_mutex_lock(&list_mutex);
		bubble_sort_locked();
		pthread_mutex_unlock(&list_mutex);
	}

	return NULL;
}

int main(void)
{
	pthread_t worker;
	if (pthread_create(&worker, NULL, sort_thread, NULL) != 0)
	{
		perror("pthread_create");
		return 1;
	}

	char *line = NULL;
	size_t capacity = 0;

	while (true)
	{
		ssize_t read = getline(&line, &capacity, stdin);
		if (read == -1)
		{
			break;
		}

		if (read > 0 && line[read - 1] == '\n')
		{
			line[read - 1] = '\0';
		}

		if (line[0] == '\0')
		{
			pthread_mutex_lock(&list_mutex);
			print_list_locked();
			pthread_mutex_unlock(&list_mutex);
			continue;
		}

		add_line_chunks_to_front(line);
	}

	free(line);

	atomic_store(&running, false);
	pthread_join(worker, NULL);

	pthread_mutex_lock(&list_mutex);
	free_list_locked();
	pthread_mutex_unlock(&list_mutex);

	pthread_mutex_destroy(&list_mutex);
	return 0;
}
