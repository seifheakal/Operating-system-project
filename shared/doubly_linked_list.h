#pragma once

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct doubly_linked_list_node {
	struct doubly_linked_list_node* next;
	struct doubly_linked_list_node* prev;

	void* value;
} doubly_linked_list_node;

// doubly linked list
typedef struct doubly_linked_list
{
	struct doubly_linked_list_node* head;
	struct doubly_linked_list_node* tail;

} doubly_linked_list;

void doubly_linked_list_init(doubly_linked_list* ll)
{
	if (!ll)
		return;

	ll->head = ll->tail = 0;
}

void doubly_linked_list_free(doubly_linked_list* ll)
{
	if (!ll)
		return;

	// free all nodes
	struct doubly_linked_list_node* n = ll->head;
	while (n)
	{
		struct doubly_linked_list_node* next = n->next;

		// delete
		free(n);

		n = next;
	}
}

void doubly_linked_list_add(doubly_linked_list* ll, void* value)
{
	if (!ll)
		return;

	// create node
	struct doubly_linked_list_node* node = (struct doubly_linked_list_node*)malloc(sizeof(doubly_linked_list_node));
	memset(node, 0, sizeof(doubly_linked_list_node));

	node->value = value;

	// insert
	if (!ll->head)
	{
		ll->head = node;
		ll->tail = node;
	}
	else {
		node->prev = ll->tail;
		ll->tail->next = node;

		ll->tail = node;
	}
}

int doubly_linked_list_delete_node(doubly_linked_list* ll, doubly_linked_list_node* node)
{
	if (!ll || !ll->head || !node)
		return 0;

	//update prev
	if (node->next) {
		node->next->prev = node->prev;
	}

	//update next
	if (node->prev) {
		node->prev->next = node->next;
	}

	//update tail incase of deletion
	if (node == ll->tail) {
		ll->tail = node->prev;

		if (ll->tail) {
			ll->tail->next = 0;
		}
	}

	//update head incase of deletion
	if (node == ll->head) {
		ll->head = node->next;
	}

	// free node
	free(node);

	return 1;
}

int doubly_linked_list_delete(doubly_linked_list* ll, void* value) {
	if (!ll || !ll->head)
		return 0;

	struct doubly_linked_list_node* n = ll->head;
	while (n) {
		if (n->value == value) {
			return doubly_linked_list_delete_node(ll, n);
		}

		n = n->next;
	}

	return 0;
}

void doubly_linked_list_iterate(doubly_linked_list* ll, void(*callback)(void*, void*), void* param) {
	if (!ll || !callback || !ll->head) return;

	doubly_linked_list_node* node = ll->head;
	while (node) {
		// invoke callback
		callback(node->value, param);

		// proceed
		node = node->next;
	}
}