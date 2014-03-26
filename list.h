#ifndef LIST_H
#define LIST_H

struct list {
	struct list *prev;
	struct list *next;
};

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define containerof(ptr, type, member) ({				\
			const typeof(((type *)0)->member) *__mptr = (ptr); \
			(type *)((char *)__mptr - offsetof(type, member)); \
		})

static inline void list_link(struct list *prev, struct list *next)
{
	prev->next = next;
	next->prev = prev;
}

static inline void list_append(struct list *prev, struct list *next)
{
	list_link(next, prev->next);
	list_link(prev, next);
}

static inline void list_unlink(struct list *node)
{
	node->prev->next = node->next;
	node->next->prev = node->prev;
}

static inline void list_init(struct list *node)
{
	list_link(node, node);
}

static inline void list_free(struct list *node)
{
	list_unlink(node);
}
#endif /* LIST_H */
