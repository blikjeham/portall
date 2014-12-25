#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "conf.h"
#include "channels.h"
#include "logging.h"
#include "timer.h"

struct conf_input *deq_input;
struct conf_output *deq_output;
struct conf_tunnel *tunnel;
extern struct channel *deque;
extern int loglevel;

#define DB(fmt, args...) debug(3, "[conf]: " fmt, ##args)
#define DBINFO(fmt, args...) debug(2, "[conf]: " fmt, ##args)
#define DBWARN(fmt, args...) debug(1, "[conf]: " fmt, ##args)
#define DBERR(fmt, args...) debug(0, "[conf]: " fmt, ##args)

static int help(void)
{
	printf("Help page\n");
	return -1;
}

static char *protocol_str(int protocol)
{
	return protocol == PROTO_TCP ? "TCP" : "UDP";
}

static int create_output(struct conf_output *output)
{
	DBINFO("Creating new %s output channel on %s:%u, tag=%s",
	       protocol_str(output->protocol), output->dst,
	       output->dport, output->tag);
	output->channel = new_connecter(deque, output->dst, output->dport,
					output->protocol);
	if (!output->channel)
		return -1;
	strncpy(output->channel->tag, output->tag, MAX_TAG);
	return 0;
}

static int create_input(struct conf_input *input)
{
	DBINFO("Creating new %s input channel on %s:%u, tag=%s",
	       protocol_str(input->protocol), input->ip,
	       input->port, input->tag);
	if (input->protocol == PROTO_TCP)
		input->channel = new_tcp_listener(deque, input->ip,
						  input->port);
	if (input->protocol == PROTO_UDP)
		input->channel = new_udp_listener(deque, input->ip,
						  input->port);
	if (!input->channel)
		return -1;
	strncpy(input->channel->tag, input->tag, MAX_TAG);
	return 0;
}

static int tunnel_close(struct channel *channel)
{
	struct conf_input *input;
	struct conf_output *output;

	/* block both input and output channels */
	for_each_input(deq_input, input) {
		input->channel->pf->events &= ~EV_INPUT;
	}

	for_each_output(deq_output, output) {
		output->channel->pf->events &= ~EV_INPUT;
	}
	return 0;
}

int create_tunnel(struct conf_tunnel *tunnel)
{
	if (tunnel->remote) {
		DBINFO("Connecting to tunnel %s:%u", tunnel->ip,
		       tunnel->port);
		tunnel->channel = new_connecter(deque, tunnel->ip,
						tunnel->port,
						PROTO_TCP);
	} else {
		DBINFO("Listening for tunnel %s:%u", tunnel->ip,
		       tunnel->port);
		tunnel->channel = new_tcp_listener(deque, tunnel->ip,
						   tunnel->port);
	}
	if (tunnel->channel == NULL) {
		DBERR("Tunnel failed");
		return -1;
	}
	tunnel->channel->flags |= CHAN_TAGGED;
	tunnel->channel->on_close = tunnel_close;
	timer_arm(tunnel->channel->timer, 5, keep_alive);
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

	if (!tunnel) {
		DBERR("No tunnel configured");
		return -1;
	}

	for_each_output(deq_output, optr) {
		ret = create_output(optr);
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

	holder = strsep(&line, ",");
	if (!(new->af = parse_ip_and_port(holder, new->ip, &new->port)))
		ret = 2;

	if (!strncpy(new->tag, line, MAX_TAG))
		ret = 3;

	if (ret) {
		DBERR("Invalid input: %d", ret);
		input_free(new);
		return 1;
	}

	list_append(&deq_input->list, &new->list);
	return 0;
}

static int parse_output(char *line)
{
	char *holder;
	int ret = 0;
	struct conf_output *new = malloc(sizeof(struct conf_output));
	list_init(&new->list);

	holder = strsep(&line, "=");
	if (!(new->protocol = parse_protocol(holder))) {
		ret = 1;
	}

	holder = strsep(&line, ",");
	if (!(new->af = parse_ip_and_port(holder, new->dst, &new->dport))) {
		ret = 2;
	}

	if (!strncpy(new->tag, line, MAX_TAG)) {
		ret = 3;
	}

	if (ret) {
		DBERR("Invalid output");
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
		DBERR("Unknown tunnel mode");
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
		DBERR("Could not open file %s", filename);
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

int parse_short(char *arg)
{
	char *s = arg + 1;

	while (*s != '\0') {
		switch (*s) {
		case 'v':
			loglevel++;
			break;
		default:
			return 1;
		}
		s++;
	}
	return 0;
}

int get_config_files(int argc, char **argv)
{
	int i;
	int ret = 0;
	struct stat *file_stats = malloc(sizeof(struct stat));

	if (argc < 2) {
		DBERR("Please supply one or more config files");
		return -1;
	}

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--help") || ! strcmp(argv[i], "-h"))
			return help();
		if (argv[i][0] == '-') {
			if (parse_short(argv[i]))
				return help();
			else
				continue;
		}
		if ((ret = stat(argv[i], file_stats)) < 0) {
			DBERR("File does not exist %s", argv[i]);
			continue;
		}

		if ((ret = parse_file(argv[i])) < 0) {
			DBERR("Could not parse file %s (ret = %d)", argv[i],
				ret);
			continue;
		}
	}
	return ret;
}

