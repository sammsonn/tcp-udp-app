#include <stdio.h>

#include "func.h"

// functia principala pentru rularea serverului
void run_server(int tcp_sockfd, int udp_sockfd)
{
	// alocam memorie pentru 3 structuri de tipul pollfd
	struct pollfd *fds = malloc(3 * sizeof(struct pollfd));
	// setam numarul de descriptori de fisiere pe care ii vom monitoriza
	int nfds = 3;
	// initializam variabilele pentru numarul de octeti de trimis si de primit
	uint32_t bytes_to_send, bytes_to_receive;

	// ascultam pe socketul tcp
	int rc = listen(tcp_sockfd, 999);
	DIE(rc < 0, "listen");

	// setam descriptorii de fisiere si evenimentele pentru poll
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;
	fds[1].fd = tcp_sockfd;
	fds[1].events = POLLIN;
	fds[2].fd = udp_sockfd;
	fds[2].events = POLLIN;

	// alocam memorie pentru primul abonat
	subscriber *subscribers = malloc(sizeof(subscriber));
	int subscribers_count = 0;

	// alocam memorie pentru primul topic
	topic *topics = malloc(sizeof(topic));
	int topics_count = 0;

	// intram in bucla infinita a serverului
	while (1) {
		// apelam poll pentru a astepta evenimente pe descriptorii de fisiere
		rc = poll(fds, nfds, -1);
		DIE(rc < 0, "poll");

		// parcurgem descriptorii de fisiere pentru a vedea pe care s-a produs un eveniment
		for (int i = 0; i < nfds; i++) {
			if (fds[i].revents & POLLIN) {
				// daca s-a produs un eveniment pe socketul tcp, acceptam noua conexiune
				if (fds[i].fd == tcp_sockfd) {
					struct sockaddr_in cli_addr;
					socklen_t cli_len = sizeof(cli_addr);
					int newsockfd = accept(tcp_sockfd, (struct sockaddr *)&cli_addr, &cli_len);
					DIE(newsockfd < 0, "accept");

					// setam optiunile pentru noul socket
					const int enable = 1;
					rc = setsockopt(newsockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
					DIE(rc < 0, "tcp reuseaddr setsockopt");
					rc = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
					DIE(rc < 0, "tcp nodelay setsockopt");

					// realocam memorie pentru structura fds pentru a include noul socket
					fds = realloc(fds, (nfds + 1) * sizeof(struct pollfd));
					fds[nfds].fd = newsockfd;
					fds[nfds].events = POLLIN;
					nfds++;

					// primim id-ul abonatului de la client
					char subscriber_id[11];
					rc = recv_all(newsockfd, subscriber_id, sizeof(subscriber_id));

					int connected = 0;
					// verificam daca abonatul este deja conectat
					for (int j = 0; j < subscribers_count; j++) {
						if (strcmp(subscribers[j].id, subscriber_id) == 0 && subscribers[j].sockfd != -1) {
							printf("Client %s already connected.\n", subscriber_id);
							bytes_to_send = htonl(10);
							rc = send_all(newsockfd, &bytes_to_send, sizeof(uint32_t));
							rc = send_all(newsockfd, "TERMINATE", 10);
							// inchidem socketul si il eliminam din structura fds
							close(newsockfd);
							fds[nfds - 1].fd = -1;
							nfds--;
							fds = realloc(fds, nfds * sizeof(struct pollfd));

							connected = 1;
							break;
						}
					}

					// daca abonatul nu este deja conectat, il conectam
					if (!connected) {
						printf("New client %s connected from %s:%hu.\n", subscriber_id, inet_ntoa(cli_addr.sin_addr),
							   ntohs(cli_addr.sin_port));

						// verificam daca un abonat cu id-ul curent exista deja
						int found = 0;
						for (int j = 0; j < subscribers_count; j++) {
							if (strcmp(subscribers[j].id, subscriber_id) == 0) {
								subscribers[j].sockfd = newsockfd;
								// actualizam descriptorul de socket pentru abonat in toate topicurile la care este
								// abonat
								for (int j = 0; j < topics_count; j++) {
									for (int k = 0; k < topics[j].subscribers_count; k++) {
										if (strcmp(topics[j].subscribers[k].id, subscriber_id) == 0) {
											topics[j].subscribers[k].sockfd = newsockfd;
											break;
										}
									}
								}

								found = 1;
								break;
							}
						}

						// daca abonatul nu a fost gasit, il adaugam
						if (!found) {
							subscribers = realloc(subscribers, (subscribers_count + 1) * sizeof(subscriber));
							strcpy(subscribers[subscribers_count].id, subscriber_id);
							subscribers[subscribers_count].sockfd = newsockfd;
							subscribers_count++;
						}
					}
				} else if (fds[i].fd == udp_sockfd) {
					// date de la udp
					char buf[1551];
					memset(buf, 0, sizeof(buf));

					struct sockaddr_in cli_addr;
					socklen_t cli_len = sizeof(cli_addr);
					rc = recvfrom(udp_sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&cli_addr, &cli_len);
					DIE(rc < 0, "recvfrom");

					// alocam memorie pentru mesajul udp si il umplem cu datele primite
					udp_message *message = malloc(sizeof(udp_message));
					memcpy(message->ip, inet_ntoa(cli_addr.sin_addr), sizeof(message->ip));
					message->port = ntohs(cli_addr.sin_port);
					memcpy(message->topic, buf, 50);
					message->data_type = buf[50];
					memcpy(message->payload, buf + 51, sizeof(message->payload));

					// extragem topicul din mesaj
					char topic[51];
					memcpy(topic, buf, 50);
					topic[50] = '\0';

					// parcurgem toate topicurile pentru a vedea daca mesajul se potriveste cu vreunul
					for (int i = 0; i < topics_count; i++) {
						int sent = 0;
						char *topic_copy = strdup(topic);
						char *subscription_copy = strdup(topics[i].name);
						// daca mesajul se potriveste cu un topic, il trimitem tuturor abonati la acel topic
						if (strcmp(topics[i].name, topic) == 0 ||
							topic_matches_subscription(topic_copy, subscription_copy)) {
							for (int j = 0; j < topics[i].subscribers_count; j++) {
								// daca abonatul este conectat, trimitem mesajul in functie de tipul de date
								if (topics[i].subscribers[j].sockfd != -1) {
									if (message->data_type == 0) {
										bytes_to_send =
											htonl(sizeof(udp_message) - 1501 + sizeof(u_int8_t) + sizeof(u_int32_t));
										rc =
											send_all(topics[i].subscribers[j].sockfd, &bytes_to_send, sizeof(uint32_t));
										rc = send_all(topics[i].subscribers[j].sockfd, message, ntohl(bytes_to_send));
									} else if (message->data_type == 1) {
										bytes_to_send = htonl(sizeof(udp_message) - 1501 + sizeof(u_int16_t));
										rc =
											send_all(topics[i].subscribers[j].sockfd, &bytes_to_send, sizeof(uint32_t));
										rc = send_all(topics[i].subscribers[j].sockfd, message, ntohl(bytes_to_send));
									} else if (message->data_type == 2) {
										bytes_to_send = htonl(sizeof(udp_message) - 1501 + sizeof(u_int8_t) +
															  sizeof(u_int32_t) + sizeof(uint8_t));
										rc =
											send_all(topics[i].subscribers[j].sockfd, &bytes_to_send, sizeof(uint32_t));
										rc = send_all(topics[i].subscribers[j].sockfd, message, ntohl(bytes_to_send));
									} else if (message->data_type == 3) {
										bytes_to_send =
											htonl(sizeof(udp_message) - 1501 + strlen(message->payload) + 1);
										rc =
											send_all(topics[i].subscribers[j].sockfd, &bytes_to_send, sizeof(uint32_t));
										rc = send_all(topics[i].subscribers[j].sockfd, message, ntohl(bytes_to_send));
									}
									sent = 1;
								}
							}
							if (sent) {
								break;
							}
						}
					}
				} else if (fds[i].fd == STDIN_FILENO) {
					// daca s-a produs un eveniment pe descriptorul de fisiere pentru intrarea standard, citim datele
					char buf[5];
					memset(buf, 0, sizeof(buf));

					rc = read(STDIN_FILENO, buf, 5);
					DIE(rc < 0, "read");

					// daca am primit comanda "exit", trimitem un mesaj de terminare la toti clientii inainte de a
					// inchide conexiunea
					if (strncmp(buf, "exit", 4) == 0) {
						for (int j = 3; j < nfds; j++) {
							if (fds[j].fd != -1) {
								bytes_to_send = htonl(10);
								rc = send_all(fds[j].fd, &bytes_to_send, sizeof(uint32_t));
								rc = send_all(fds[j].fd, "TERMINATE", 10);
								close(fds[j].fd);
							}
						}
						return;
					}
				} else {
					// daca am primit un mesaj de la un client tcp
					tcp_message *message = malloc(sizeof(tcp_message));
					rc = recv_all(fds[i].fd, &bytes_to_receive, sizeof(uint32_t));
					bytes_to_receive = ntohl(bytes_to_receive);
					rc = recv_all(fds[i].fd, message, bytes_to_receive);

					// daca am primit un mesaj de deconectare de la un client
					if (strncmp(message->sub_id, "DISCONNECT", 10) == 0) {
						for (int j = 0; j < subscribers_count; j++) {
							if (fds[i].fd == subscribers[j].sockfd) {
								printf("Client %s disconnected.\n", subscribers[j].id);
								close(fds[i].fd);
								fds[i].fd = -1;
								subscribers[j].sockfd = -1;

								// actualizam descriptorul de socket pentru abonat in toate topicurile la care este
								// abonat
								for (int k = 0; k < topics_count; k++) {
									for (int l = 0; l < topics[k].subscribers_count; l++) {
										if (strcmp(topics[k].subscribers[l].id, subscribers[j].id) == 0) {
											topics[k].subscribers[l].sockfd = -1;
											break;
										}
									}
								}
								break;
							}
						}
					} else {
						// daca comanda primita este "subscribe"
						if (strcmp(message->command, "subscribe") == 0) {
							int found = 0;
							// verificam daca topicul exista deja
							for (int j = 0; j < topics_count; j++) {
								if (strcmp(topics[j].name, message->topic) == 0) {
									found = 1;
									break;
								}
							}

							if (found) {
								int subscribed = 0;
								// daca topicul exista, verificam daca clientul este deja abonat la acesta
								for (int j = 0; j < topics_count; j++) {
									if (strcmp(topics[j].name, message->topic) == 0) {
										for (int k = 0; k < topics[j].subscribers_count; k++) {
											if (strcmp(topics[j].subscribers[k].id, message->sub_id) == 0) {
												// daca clientul este deja abonat, actualizam descriptorul de socket
												// daca acesta este -1
												if (topics[j].subscribers[k].sockfd == -1) {
													topics[j].subscribers[k].sockfd = fds[i].fd;
												}
												subscribed = 1;
												break;
											}
										}

										// daca clientul nu este abonat, il adaugam la lista de abonati
										if (!subscribed) {
											topics[j].subscribers =
												realloc(topics[j].subscribers,
														(topics[j].subscribers_count + 1) * sizeof(subscriber));
											strcpy(topics[j].subscribers[topics[j].subscribers_count].id,
												   message->sub_id);
											topics[j].subscribers[topics[j].subscribers_count].sockfd = fds[i].fd;
											topics[j].subscribers_count++;
										}
										break;
									}
								}
							} else {
								// daca topicul nu exista, il cream si adaugam clientul ca abonat
								topics = realloc(topics, (topics_count + 1) * sizeof(topic));
								topics[topics_count].subscribers = malloc(sizeof(subscriber));
								topics[topics_count].subscribers_count = 0;
								strcpy(topics[topics_count].name, message->topic);
								strcpy(topics[topics_count].subscribers[0].id, message->sub_id);
								topics[topics_count].subscribers[0].sockfd = fds[i].fd;
								topics[topics_count].subscribers_count++;
								topics_count++;
							}
							// daca comanda primita este "unsubscribe"
						} else if (strcmp(message->command, "unsubscribe") == 0) {
							// parcurgem toate topicurile
							for (int j = 0; j < topics_count; j++) {
								// daca am gasit topicul la care clientul doreste sa se dezaboneze
								if (strcmp(topics[j].name, message->topic) == 0) {
									// parcurgem toti abonatii topicului
									for (int k = 0; k < topics[j].subscribers_count; k++) {
										// daca am gasit clientul in lista de abonati, il dezabonam setand descriptorul
										// de socket la -1
										if (strcmp(topics[j].subscribers[k].id, message->sub_id) == 0) {
											topics[j].subscribers[k].sockfd = -1;
											break;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	// verificam daca numarul de argumente este corect
	DIE(argc != 2, "Usage: ./server <PORT_DORIT>");

	uint16_t port;
	// citim portul din argumentele programului
	int rc = sscanf(argv[1], "%hu", &port);
	// verificam daca portul este valid
	DIE(rc != 1, "Given port is invalid");

	// initializam structura pentru adresa serverului
	struct sockaddr_in serv_addr;
	socklen_t serv_len = sizeof(serv_addr);
	memset(&serv_addr, 0, serv_len);

	// setam familia de adrese la ipv4
	serv_addr.sin_family = AF_INET;
	// convertim portul la formatul network byte order
	serv_addr.sin_port = htons(port);
	// acceptam conexiuni de la orice adresa
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	// cream socketul tcp
	int tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	// verificam daca crearea socketului a reusit
	DIE(tcp_sockfd < 0, "tcp socket");

	const int enable = 1;
	// setam optiunea so_reuseaddr pentru a putea reutiliza adresa si portul rapid
	rc = setsockopt(tcp_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
	DIE(rc < 0, "tcp reuseaddr setsockopt");
	// setam optiunea tcp_nodelay pentru a dezactiva algoritmul nagle
	rc = setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
	DIE(rc < 0, "tcp nodelay setsockopt");

	// asociem socketul cu adresa serverului
	rc = bind(tcp_sockfd, (struct sockaddr *)&serv_addr, serv_len);
	DIE(rc < 0, "tcp bind");

	// cream socketul udp
	int udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(udp_sockfd < 0, "udp socket");

	// setam optiunea so_reuseaddr pentru a putea reutiliza adresa si portul rapid
	rc = setsockopt(udp_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
	DIE(rc < 0, "udp setsockopt");

	// asociem socketul cu adresa serverului
	rc = bind(udp_sockfd, (struct sockaddr *)&serv_addr, serv_len);
	DIE(rc < 0, "udp bind");

	// rulam serverul
	run_server(tcp_sockfd, udp_sockfd);

	// inchidem socketurile la final
	close(tcp_sockfd);
	close(udp_sockfd);

	return 0;
}
