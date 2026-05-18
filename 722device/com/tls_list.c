#include <stdio.h>
#include "tls_list.h"

void tls_list_initialize(tls_list_node_t* head)
{
	head->pre = head->next = head;
}

int	tls_list_empty(tls_list_node_t* head)
{
	if (NULL == head || NULL == head->pre || NULL == head->next)
		return 1;
	return (head->next == head);
}
	
tls_list_node_t* tls_list_insert_tail(tls_list_node_t* head, tls_list_node_t* node)
{
	node->next			= head;
	head->pre->next		= node;
	node->pre			= head->pre;
	head->pre			= node;

	return node;
}

tls_list_node_t* tls_list_remove_head(tls_list_node_t* head)
{
	tls_list_node_t* node		= head->next;
	node->next->pre				= head;
	head->next					= node->next;

	return node;
}

tls_list_node_t* tls_list_remove_node(tls_list_node_t* head, tls_list_node_t* node)
{
	tls_list_node_t*			list_node = NULL;

	if (NULL == node)
		return NULL;

	for (list_node = head->next; list_node != head; list_node = list_node->next)
	{
		if (list_node == node)
			break;
	}
	if (list_node == head)
		return NULL;

	list_node->pre->next = list_node->next;
	list_node->next->pre = list_node->pre;
	list_node->pre = list_node->next = list_node;

	return list_node;
}

tls_list_node_t* tls_list_insert_tail_lock(tls_list_node_t* head, tls_list_node_t* node, void* lock)
{
	return NULL;
}

tls_list_node_t* tls_list_remove_head_lock(tls_list_node_t* head, void* lock)
{
	return NULL;
}

tls_list_node_t* tls_list_remove_node_lock(tls_list_node_t* head, tls_list_node_t* node)
{
	return NULL;
}
