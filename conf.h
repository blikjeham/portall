#ifndef CONF_H
#define CONF_H

#include <stdio.h>
#include <netinet/in.h>
#include "list.h"

#define MAX_LINE 512
#define MAX_TAG 16

#define CONF_INPUT 1
#define CONF_OUTPUT 2
#define CONF_TUNNEL 3

struct conf_input {
	char ip[INET6_ADDRSTRLEN];
	uint16_t port;
	int protocol;
	int af;
	char tag[MAX_TAG];
	struct list list;
	struct channel *channel;
};

struct conf_output {
	char src[INET6_ADDRSTRLEN];
	char dst[INET6_ADDRSTRLEN];
	uint16_t sport;
	uint16_t dport;
	int protocol;
	int af;
	char tag[MAX_TAG];
	struct list list;
	struct channel *channel;
};

struct conf_tunnel {
	char ip[INET6_ADDRSTRLEN];
	uint16_t port;
	int af;
	int remote;
	struct channel *channel;
};

#define input_of(ptr) containerof(ptr, struct conf_input, list)

#define for_each_input(deque, ptr) for(ptr = input_of(deque->list.next); \
				       ptr != deque;			\
				       ptr = input_of(ptr->list.next))

#define output_of(ptr) containerof(ptr, struct conf_output, list)

#define for_each_output(deque, ptr) for(ptr = output_of(deque->list.next); \
					ptr != deque;			\
					ptr = output_of(ptr->list.next))

int create_sockets(void);
int get_config_files(int , char **);
#endif /* CONF_H */
