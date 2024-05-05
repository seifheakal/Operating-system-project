#pragma once

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct pri_queue_node
{
	struct pri_queue_node* next;
	struct pri_queue_node* prev;

	int priority;
	void* value;
} pri_queue_node;

// priority queue
typedef struct pri_queue
{
	struct pri_queue_node* head;
} pri_queue;

void pri_queue_init(pri_queue* q)
{
	if (!q)
		return;

	q->head = 0;
}

void pri_queue_free(pri_queue* q)
{
	if (!q)
		return;

	// free all nodes
	struct pri_queue_node* n = q->head;
	while (n)
	{
		struct pri_queue_node* next = n->next;

		// delete
		free(n);

		n = next;
	}
}

void pri_queue_enqueue(pri_queue* q, int priority, void* value)
{
	if (!q)
		return;

	// create node
	struct pri_queue_node* node = (struct pri_queue_node*)malloc(sizeof(pri_queue_node));
	memset(node, 0, sizeof(pri_queue_node));

	node->priority = priority;
	node->value = value;

	// insert
	if (!q->head)
	{
		q->head = node;
		return;
	}

	struct pri_queue_node* current = q->head;
	while (current)
	{
		if (current->priority > priority)
		{
			// insert here
			node->next = current;
			node->prev = current->prev;

			if (current->prev)
			{
				current->prev->next = node;
			}
			current->prev = node;

			if (current == q->head)
			{
				q->head = node;
			}

			return;
		}

		// not inserted yet, and next is null
		if (!current->next)
		{
			node->prev = current;
			current->next = node;

			return;
		}

		current = current->next;
	}
}

int pri_queue_dequeue(pri_queue* q, void** value)
{
	if (!q || !q->head)
		return 0;

	struct pri_queue_node* n = q->head;

	// update head
	q->head = n->next;

	// update prev
	if (q->head)
	{
		q->head->prev = 0;
	}

	if (value)
	{
		*value = n->value;
	}

	// free node
	free(n);

	return 1;
}

void print_pri_queue(pri_queue* q)
{
	printf("Queue: [ ");
	struct pri_queue_node* n = q->head;
	while (n)
	{
		printf("%d ", n->priority);
		//*(int *)n->value
		n = n->next;
	}

	printf("]\n");
}

int pri_queue_peek(pri_queue* q, void** result)
{
	if (!q || !q->head || !result) return 0;

	*result = q->head->value;
	return 1;
}

void pri_queue_iterate(pri_queue* q, void(*callback)(void*, void*), void* param) {
	if (!q || !callback || !q->head) return;

	pri_queue_node* node = q->head;
	while (node) {
		// invoke callback
		callback(node->value, param);

		// proceed
		node = node->next;
	}
}