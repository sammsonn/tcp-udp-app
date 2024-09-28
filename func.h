#ifndef __FUNC_H__
#define __FUNC_H__

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#define DIE(assertion, call_description)                                                                               \
	do {                                                                                                               \
		if (assertion) {                                                                                               \
			fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);                                                         \
			perror(call_description);                                                                                  \
			exit(EXIT_FAILURE);                                                                                        \
		}                                                                                                              \
	} while (0)

typedef struct {
	char sub_id[11];
	char command[12], topic[51];
} tcp_message;

typedef struct {
	char ip[20];
	uint16_t port;
	char topic[51];
	unsigned int data_type;
	char payload[1501];
} udp_message;

typedef struct {
	int sockfd;
	char id[11];
} subscriber;

typedef struct {
	char name[51];
	subscriber *subscribers;
	int subscribers_count;
} topic;

int topic_matches_subscription(char *topic, char *subscription);
int send_all(int sockfd, void *buff, size_t len);
int recv_all(int sockfd, void *buff, size_t len);

#endif
