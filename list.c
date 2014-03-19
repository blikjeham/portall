#include "list.h"

static void list_link(struct list *prev, struct list *next)
{
	prev->next = next;
	next->prev = prev;
}

void list_append(struct list *prev, struct list *next)
{
	list_link(next, prev->next);
	list_link(prev, next);
}

void list_unlink(struct list *node)
{
	node->prev->next = node->next;
	node->next->prev = node->prev;
}

void list_init(struct list *node)
{
	list_link(node, node);
}

void list_free(struct list *node)
{
	list_unlink(node);
}
