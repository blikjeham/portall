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

void list_append(struct list *, struct list *);
void list_unlink(struct list *);
void list_init(struct list *);
void list_free(struct list *);

#endif /* LIST_H */
