#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "conf.h"
#include "channels.h"

struct conf_input *deq_input;
struct conf_output *deq_output;
struct conf_tunnel *tunnel;
extern struct channel *deque;

static int help(void)
{
	printf("Help page\n");
	return -1;
}

static int create_output(struct conf_output *output)
{
	output->channel = new_connecter(deque, output->dst, output->dport,
					output->protocol);
	return 0;
}

static int create_input(struct conf_input *input)
{
	if (input->protocol == PROTO_TCP)
		input->channel = new_tcp_listener(deque, input->ip,
						  input->port);
	if (input->protocol == PROTO_UDP)
		input->channel = new_udp_listener(deque, input->ip,
						  input->port);
	if (!input->channel)
		return -1;
	return 0;
}

int create_tunnel(struct conf_tunnel *tunnel)
{
	if (tunnel->remote) {
		tunnel->channel = new_connecter(deque, tunnel->ip,
						tunnel->port,
						PROTO_TCP);
	} else {
		tunnel->channel = new_tcp_listener(deque, tunnel->ip,
						   tunnel->port);
	}
	if (tunnel->channel == NULL) {
		printf("Tunnel failed\n");
		return -1;
	}
	return 0;
}

int create_sockets(void)
{
	/* create output first
	 * then tunnel,
	 * then inputs.
	 */
	int ret;
	struct conf_output *optr;
	struct conf_input *iptr;

	for_each_output(deq_output, optr) {
		ret = create_output(optr);
	}

	if (!tunnel) {
		printf("No tunnel configured\n");
		return -1;
	}
	if ((ret = create_tunnel(tunnel)) < 0)
		return -1;

	for_each_input(deq_input, iptr) {
		ret = create_input(iptr);
	}

	return ret;
}

static struct conf_input *input_init(void)
{
	struct conf_input *tmp = malloc(sizeof(struct conf_input));
	list_init(&tmp->list);
	return tmp;
}

static void input_free(struct conf_input *input)
{
	list_unlink(&input->list);
	free(input);
}

static struct conf_output *output_init(void)
{
	struct conf_output *tmp = malloc(sizeof(struct conf_output));
	list_init(&tmp->list);
	return tmp;
}

static void output_free(struct conf_output *output)
{
	list_unlink(&output->list);
	free(output);
}

static int parse_ip_and_port(char *line, char *ip, uint16_t *port)
{
	char *needle;
	char *iptmp = NULL;
	int af = AF_INET;

	/* parse IPv6 */
	if (strstr(line, "[") && (strstr(line, "]"))) {
		af = AF_INET6;
		needle = strsep(&line, "[");
		needle = strsep(&line, "]");
		iptmp = needle;
	}

	needle = strsep(&line, ":");
	*port = atoi(line);
	if (af == AF_INET) {
		iptmp = needle;
	}

	strncpy(ip, iptmp, INET6_ADDRSTRLEN);
	return af;
}

static int parse_protocol(char *line)
{
	if(!strcmp(line, "tcp"))
		return PROTO_TCP;
	if (!strcmp(line, "udp"))
		return PROTO_UDP;
	return 0;
}

static int parse_input(char *line)
{
	char *holder;
	int ret = 0;
	struct conf_input *new = malloc(sizeof(struct conf_input));
	list_init(&new->list);

	holder = strsep(&line, "=");
	if (!(new->protocol = parse_protocol(holder)))
		ret = 1;

	if (!(new->af = parse_ip_and_port(line, new->ip, &new->port)))
		ret = 2;

	if (ret) {
		printf("Invalid input: %d\n", ret);
		input_free(new);
		return 1;
	}

	printf("ip: %s, port %u, proto %d\n", new->ip, new->port,
	       new->protocol);
	list_append(&deq_input->list, &new->list);
	return 0;
}

static int parse_output(char *line)
{
	char *holder;
	int ret;
	struct conf_output *new = malloc(sizeof(struct conf_output));
	list_init(&new->list);

	holder = strsep(&line, "=");
	if (!(new->protocol = parse_protocol(holder)))
		ret = -1;

	if (!(new->af = parse_ip_and_port(line, new->dst, &new->dport)))
		ret = -1;

	if (ret) {
		output_free(new);
		return ret;
	}

	list_append(&deq_output->list, &new->list);
	return 0;
}

static int parse_tunnel(char *line)
{
	char *holder;
	int remote = -1;
	/* We can have only one tunnel */
	if (tunnel)
		return 1;

	holder = strsep(&line, "=");
	if (!strcmp(holder, "remote")) {
		remote = 1;
	} else if (!strcmp(holder, "local")) {
		remote = 0;
	} else {
		printf("Unknown tunnel mode\n");
		return -2;
	}
	tunnel = malloc(sizeof(struct conf_tunnel));
	tunnel->af = parse_ip_and_port(line, tunnel->ip, &tunnel->port);
	tunnel->remote = remote;

	return 0;
}

static int parse_line(char *line, int section)
{
	switch (section) {
	case CONF_INPUT:
		return parse_input(line);
	case CONF_OUTPUT:
		return parse_output(line);
	case CONF_TUNNEL:
		return parse_tunnel(line);
	default:
		return -1;
	}
}

static int parse_file(char *filename)
{
	FILE *f;
	int section = -1;
	char *line;
	line = malloc(MAX_LINE);

	if (!(f = fopen(filename, "r"))) {
		printf("Could not open file %s\n", filename);
		return -1;
	}

	deq_input = input_init();
	deq_output = output_init();

	while ((line = fgets(line, MAX_LINE, f))) {
		/* strip newline and comments */
		line = strsep(&line, "\n#");

		if (!strlen(line))
			continue;

		if (!strcmp(line, "[inputs]")) {
			section = CONF_INPUT;
			continue;
		}
		if (!strcmp(line, "[outputs]")) {
			section = CONF_OUTPUT;
			continue;
		}
		if (!strcmp(line, "[tunnels]")) {
			section = CONF_TUNNEL;
			continue;
		}

		if (parse_line(line, section) < 0)
			return -1;
	}
	return section;
}

int get_config_files(int argc, char **argv)
{
	int i;
	int ret = 0;
	struct stat *file_stats = malloc(sizeof(struct stat));

	if (argc < 2) {
		printf("Please supply one or more config files\n");
		return -1;
	}

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--help") || ! strcmp(argv[i], "-h"))
			return help();
		if ((ret = stat(argv[i], file_stats)) < 0) {
			printf("File does not exist %s\n", argv[i]);
			continue;
		}

		if ((ret = parse_file(argv[i])) < 0) {
			printf("Could not parse file %s\n", argv[i]);
			continue;
		}
	}
	return ret;
}

